// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/testapi/oobe_test_api_handler.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/ash_interfaces.h"
#include "ash/public/cpp/tablet_mode.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/values.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/ash/login/existing_user_controller.h"
#include "chrome/browser/ash/login/helper.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_utils.h"
#include "chrome/browser/ash/login/screens/hid_detection_screen.h"
#include "chrome/browser/ash/login/screens/network_screen.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_requisition_manager.h"
#include "chrome/browser/ui/ash/login_screen_client_impl.h"
#include "chrome/browser/ui/webui/ash/login/hid_detection_screen_handler.h"
#include "chromeos/ash/components/assistant/buildflags.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/account_id/account_id.h"
#include "components/login/localized_values_builder.h"
#include "services/device/public/mojom/input_service.mojom.h"

namespace ash {

OobeTestAPIHandler::OobeTestAPIHandler() = default;
OobeTestAPIHandler::~OobeTestAPIHandler() = default;

void OobeTestAPIHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("testapi_browseAsGuest", IDS_ASH_BROWSE_AS_GUEST_BUTTON);
}

void OobeTestAPIHandler::DeclareJSCallbacks() {
  AddCallback("OobeTestApi.loginWithPin", &OobeTestAPIHandler::LoginWithPin);
  AddCallback("OobeTestApi.advanceToScreen",
              &OobeTestAPIHandler::AdvanceToScreen);
  AddCallback("OobeTestApi.skipToLoginForTesting",
              &OobeTestAPIHandler::SkipToLoginForTesting);
  AddCallback("OobeTestApi.skipPostLoginScreens",
              &OobeTestAPIHandler::SkipPostLoginScreens);
  AddCallback("OobeTestApi.loginAsGuest", &OobeTestAPIHandler::LoginAsGuest);
  AddCallback("OobeTestApi.showGaiaDialog",
              &OobeTestAPIHandler::ShowGaiaDialog);

  // Keeping the code in case the test using this will be ported to tast. The
  // function used to be called getPrimaryDisplayNameForTesting. In order to use
  // this one you need to add a function into login/test_api/test_api.js.
  AddCallback("OobeTestApi.getPrimaryDisplayName",
              &OobeTestAPIHandler::HandleGetPrimaryDisplayName);
  AddCallback("OobeTestApi.emulateDevicesForTesting",
              &OobeTestAPIHandler::EmulateDevicesConnectedForTesting);
}

void OobeTestAPIHandler::GetAdditionalParameters(base::Value::Dict* dict) {
  login::NetworkStateHelper helper_;
  dict->Set("testapi_shouldSkipNetworkFirstShow",
                !switches::IsOOBENetworkScreenSkippingDisabledForTesting() &&
                helper_.IsConnectedToEthernet());

  dict->Set(
      "testapi_shouldSkipGuestTos",
      StartupUtils::IsEulaAccepted() || !BUILDFLAG(GOOGLE_CHROME_BRANDING));

  dict->Set("testapi_isFingerprintSupported",
            quick_unlock::IsFingerprintSupported());

  dict->Set("testapi_shouldSkipAssistant",
            features::IsOobeSkipAssistantEnabled() ||
                !BUILDFLAG(ENABLE_CROS_LIBASSISTANT));

  dict->Set("testapi_isBrandedBuild",
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
            true
#else
            false
#endif
  );

  dict->Set("testapi_isOobeInTabletMode",
            TabletMode::Get()->InTabletMode() ||
                switches::ShouldOobeUseTabletModeFirstRun());
  dict->Set("testapi_shouldSkipConsolidatedConsent",
            !BUILDFLAG(GOOGLE_CHROME_BRANDING));
  dict->Set("testapi_isHPSEnabled", ash::features::IsQuickDimEnabled());
}

void OobeTestAPIHandler::LoginWithPin(const std::string& username,
                                      const std::string& pin) {
  VLOG(1) << "LoginWithPin";
  LoginScreenClientImpl::Get()->AuthenticateUserWithPasswordOrPin(
      AccountId::FromUserEmail(username), pin, /*authenticated_by_pin=*/true,
      base::BindOnce([](bool success) {
        LOG_IF(ERROR, !success) << "Failed to authenticate with pin";
      }));
}

void OobeTestAPIHandler::AdvanceToScreen(const std::string& screen) {
  VLOG(1) << "AdvanceToScreen(" << screen << ")";
  LoginDisplayHost::default_host()->StartWizard(OobeScreenId(screen));
}

void OobeTestAPIHandler::SkipToLoginForTesting() {
  VLOG(1) << "SkipToLoginForTesting";
  WizardController* controller = WizardController::default_controller();
  if (!controller || !controller->is_initialized()) {
    LOG(ERROR)
        << "SkipToLoginForTesting is called when WizardController is not yet "
           "initialized. Please report at https://crbug.com/1336940";
    return;
  }
  controller->SkipToLoginForTesting();  // IN-TEST
}

void OobeTestAPIHandler::EmulateDevicesConnectedForTesting() {
  HIDDetectionScreen* screen_ = static_cast<HIDDetectionScreen*>(
      WizardController::default_controller()->GetScreen(
          HIDDetectionView::kScreenId));
  VLOG(1) << "EmulateDevicesConnectedForTesting";
  auto touchscreen = device::mojom::InputDeviceInfo::New();
  touchscreen->id = "fake_touchscreen";
  touchscreen->subsystem = device::mojom::InputDeviceSubsystem::SUBSYSTEM_INPUT;
  touchscreen->type = device::mojom::InputDeviceType::TYPE_UNKNOWN;
  touchscreen->is_touchscreen = true;
  screen_->InputDeviceAddedForTesting(std::move(touchscreen));  // IN-TEST

  auto mouse = device::mojom::InputDeviceInfo::New();
  mouse->id = "fake_mouse";
  mouse->subsystem = device::mojom::InputDeviceSubsystem::SUBSYSTEM_INPUT;
  mouse->type = device::mojom::InputDeviceType::TYPE_USB;
  mouse->is_mouse = true;
  screen_->InputDeviceAddedForTesting(std::move(mouse));  // IN-TEST

  auto keyboard = device::mojom::InputDeviceInfo::New();
  keyboard->id = "fake_keyboard";
  keyboard->subsystem = device::mojom::InputDeviceSubsystem::SUBSYSTEM_INPUT;
  keyboard->type = device::mojom::InputDeviceType::TYPE_USB;
  keyboard->is_keyboard = true;
  screen_->InputDeviceAddedForTesting(std::move(keyboard));  // IN-TEST
}

void OobeTestAPIHandler::SkipPostLoginScreens() {
  VLOG(1) << "SkipPostLoginScreens";
  WizardController::default_controller()
      ->SkipPostLoginScreensForTesting();  // IN-TEST
}

void OobeTestAPIHandler::LoginAsGuest() {
  VLOG(1) << "LoginAsGuest";
  WizardController::default_controller()->SkipToLoginForTesting();  // IN-TEST
  CHECK(ExistingUserController::current_controller());
  UserContext context(user_manager::USER_TYPE_GUEST, EmptyAccountId());
  ExistingUserController::current_controller()->Login(context,
                                                      SigninSpecifics());
}

void OobeTestAPIHandler::ShowGaiaDialog() {
  VLOG(1) << "ShowGaiaDialog";
  LoginDisplayHost::default_host()->ShowGaiaDialog(EmptyAccountId());
}

void OobeTestAPIHandler::HandleGetPrimaryDisplayName(
    const std::string& callback_id) {
  mojo::Remote<crosapi::mojom::CrosDisplayConfigController> cros_display_config;
  BindCrosDisplayConfigController(
      cros_display_config.BindNewPipeAndPassReceiver());

  cros_display_config->GetDisplayUnitInfoList(
      false /* single_unified */,
      base::BindOnce(&OobeTestAPIHandler::OnGetDisplayUnitInfoList,
                     base::Unretained(this), callback_id));
}

void OobeTestAPIHandler::OnGetDisplayUnitInfoList(
    const std::string& callback_id,
    std::vector<crosapi::mojom::DisplayUnitInfoPtr> info_list) {
  std::string display_name;
  for (const crosapi::mojom::DisplayUnitInfoPtr& info : info_list) {
    if (info->is_primary) {
      display_name = info->name;
      break;
    }
  }
  if (display_name.empty()) {
    RejectJavascriptCallback(base::Value(callback_id),
                             base::Value(display_name));
    return;
  }
  ResolveJavascriptCallback(base::Value(callback_id),
                            base::Value(display_name));
}

}  // namespace ash
