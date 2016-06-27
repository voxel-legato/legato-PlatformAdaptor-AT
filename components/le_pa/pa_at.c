/** @file pa.c
 *
 * AT implementation of c_pa API.
 *
 * Copyright (C) Sierra Wireless Inc. Use of this work is subject to license.
 */

#include "legato.h"
#include "interfaces.h"
#include "pa_utils_local.h"
#include "pa_at_local.h"

#include "pa_mrc.h"
#include "pa_mrc_local.h"
#include "pa_sms.h"
#include "pa_sms_local.h"
#include "pa_sim.h"
#include "pa_sim_local.h"
#include "pa_mdc.h"
#include "pa_mdc_local.h"
#include "pa_mcc_local.h"
#include "pa_ecall.h"
#include "pa_ips.h"
#include "pa_temp.h"
#include "pa_antenna.h"
#include "pa_adc_local.h"

//--------------------------------------------------------------------------------------------------
/**
 * Device reference used for sending AT commands
 */
//--------------------------------------------------------------------------------------------------
le_atClient_DeviceRef_t AtDeviceRef = NULL;

//--------------------------------------------------------------------------------------------------
/**
 * Device reference used for PPP session
 */
//--------------------------------------------------------------------------------------------------
le_atClient_DeviceRef_t PppDeviceRef = NULL;

//--------------------------------------------------------------------------------------------------
/**
 * Device path used for sending AT commands
 */
//--------------------------------------------------------------------------------------------------
const char AtPortPath[] = "/dev/ttyACM0";

//--------------------------------------------------------------------------------------------------
/**
 * Device path used for PPP session
 */
//--------------------------------------------------------------------------------------------------
const char PppPortPath[] = "/dev/ttyACM4";

//--------------------------------------------------------------------------------------------------
/**
 * Enable CMEE
 *
 * @return LE_FAULT         The function failed.
 * @return LE_TIMEOUT       No response was received.
 * @return LE_OK            The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
static le_result_t  EnableCmee()
{
    le_atClient_CmdRef_t cmdRef = NULL;
    le_result_t          res    = LE_FAULT;

    res = le_atClient_SetCommandAndSend(&cmdRef,
                                        pa_at_GetAtDeviceRef(),
                                        "AT+CMEE=1",
                                        "\0",
                                        DEFAULT_AT_RESPONSE,
                                        DEFAULT_AT_CMD_TIMEOUT);

    if (res == LE_OK)
    {
        le_atClient_Delete(cmdRef);
    }
    return res;
}

//--------------------------------------------------------------------------------------------------
/**
 * Disable echo
 *
 * @return LE_FAULT         The function failed.
 * @return LE_TIMEOUT       No response was received.
 * @return LE_OK            The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
static le_result_t  DisableEcho()
{
    le_atClient_CmdRef_t cmdRef = NULL;
    le_result_t          res    = LE_FAULT;

    res = le_atClient_SetCommandAndSend(&cmdRef,
                                        pa_at_GetAtDeviceRef(),
                                        "ATE0",
                                        "\0",
                                        DEFAULT_AT_RESPONSE,
                                        DEFAULT_AT_CMD_TIMEOUT);

    if (res == LE_OK)
    {
        le_atClient_Delete(cmdRef);
    }
    return res;
}

//--------------------------------------------------------------------------------------------------
/**
 * Save settings
 *
 * @return LE_FAULT        The function failed.
 * @return LE_TIMEOUT       No response was received.
 * @return LE_OK            The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
static le_result_t  SaveSettings()
{
    le_atClient_CmdRef_t cmdRef = NULL;
    le_result_t          res    = LE_FAULT;

    res = le_atClient_SetCommandAndSend(&cmdRef,
                                        pa_at_GetAtDeviceRef(),
                                        "AT&W",
                                        "\0",
                                        DEFAULT_AT_RESPONSE,
                                        DEFAULT_AT_CMD_TIMEOUT);
    if (res == LE_OK)
    {
        le_atClient_Delete(cmdRef);
    }
    return res;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set new message indication
 *
 * @return LE_FAULT         The function failed.
 * @return LE_TIMEOUT       No response was received.
 * @return LE_OK            The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
static le_result_t  SetNewSmsIndication()
{
    pa_sms_NmiMode_t mode;
    pa_sms_NmiMt_t   mt;
    pa_sms_NmiBm_t   bm;
    pa_sms_NmiDs_t   ds;
    pa_sms_NmiBfr_t  bfr;

    // Get & Set the configuration to enable message reception
    LE_DEBUG("Get New SMS message indication");
    if (pa_sms_GetNewMsgIndic(&mode, &mt, &bm,  &ds, &bfr)  != LE_OK)
    {
        LE_WARN("Get New SMS message indication failed, set default configuration");
        if (pa_sms_SetNewMsgIndic(PA_SMS_NMI_MODE_0,
                                    PA_SMS_MT_1,
                                    PA_SMS_BM_0,
                                    PA_SMS_DS_0,
                                    PA_SMS_BFR_0)  != LE_OK)
        {
            LE_ERROR("Set New SMS message indication failed");
            return LE_FAULT;
        }
    }

    LE_DEBUG("Set New SMS message indication");
    if (pa_sms_SetNewMsgIndic(mode,
                                PA_SMS_MT_1,
                                bm,
                                ds,
                                bfr)  != LE_OK)
    {
        LE_ERROR("Set New SMS message indication failed");
        return LE_FAULT;
    }

    return LE_OK;
}



//--------------------------------------------------------------------------------------------------
/**
 * Set Default configuration
 *
 * @return LE_FAULT         The function failed.
 * @return LE_OK            The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
static le_result_t SetDefaultConfig()
{
    if (DisableEcho() != LE_OK)
    {
        LE_WARN("modem is not well configured");
        return LE_FAULT;
    }

    if (pa_sms_SetMsgFormat(LE_SMS_FORMAT_PDU) != LE_OK)
    {
        LE_WARN("modem failed to switch to PDU format");
        return LE_FAULT;
    }

    if (SetNewSmsIndication() != LE_OK)
    {
        LE_WARN("modem failed to set New SMS indication");
        return LE_FAULT;
    }

    if (EnableCmee() != LE_OK)
    {
        LE_WARN("Failed to enable CMEE error");
        return LE_FAULT;
    }

    if (SaveSettings() != LE_OK)
    {
        LE_WARN("Failed to Save Modem Settings");
        return LE_FAULT;
    }

    return LE_OK;
}


//--------------------------------------------------------------------------------------------------
/**
 * This is used to get the device reference of the AT port.
 *
 **/
//--------------------------------------------------------------------------------------------------
le_atClient_DeviceRef_t pa_at_GetAtDeviceRef
(
    void
)
{
    return AtDeviceRef;
}

//--------------------------------------------------------------------------------------------------
/**
 * This is used to get the device reference of the PPP port.
 *
 **/
//--------------------------------------------------------------------------------------------------
le_atClient_DeviceRef_t pa_at_GetPppDeviceRef
(
    void
)
{
    return PppDeviceRef;
}


//--------------------------------------------------------------------------------------------------
/**
 * Component initializer automatically called by the application framework when the process starts.
 *
 **/
//--------------------------------------------------------------------------------------------------
COMPONENT_INIT
{
    AtDeviceRef = le_atClient_Start(AtPortPath);

    if (AtDeviceRef == NULL)
    {
        LE_ERROR("Can't open %s", AtPortPath);
        return;
    }

    PppDeviceRef = le_atClient_Start(PppPortPath);

    if (PppDeviceRef == NULL)
    {
        LE_ERROR("Can't open %s", PppPortPath);
        return;
    }

    if (SetDefaultConfig() != LE_OK)
    {
        LE_ERROR("PA is not configured as expected");
    }
    else
    {
        pa_mrc_Init();
        pa_sms_Init();
        pa_sim_Init();
        pa_mdc_Init();
        pa_mcc_Init();
        pa_ips_Init();
        pa_temp_Init();
        pa_antenna_Init();
        pa_adc_Init();
    }
}
