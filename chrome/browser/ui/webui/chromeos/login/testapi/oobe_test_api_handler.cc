// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/testapi/oobe_test_api_handler.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/ash_interfaces.h"
#include "ash/public/mojom/cros_display_config.mojom.h"
#include "base/bind.h"
#include "base/check.h"
#include "base/logging.h"
#include "base/values.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/ash/login/existing_user_controller.h"
#include "chrome/browser/ash/login/helper.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/screens/network_screen.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_requisition_manager.h"
#include "chrome/browser/ui/ash/login_screen_client_impl.h"
#include "components/account_id/account_id.h"

namespace chromeos {

OobeTestAPIHandler::OobeTestAPIHandler() = default;
OobeTestAPIHandler::~OobeTestAPIHandler() = default;

void OobeTestAPIHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {}

void OobeTestAPIHandler::DeclareJSCallbacks() {
  AddCallback("OobeTestApi.loginWithPin", &OobeTestAPIHandler::LoginWithPin);
  AddCallback("OobeTestApi.advanceToScreen",
              &OobeTestAPIHandler::AdvanceToScreen);
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
}

void OobeTestAPIHandler::InitializeDeprecated() {}

void OobeTestAPIHandler::GetAdditionalParameters(base::Value::Dict* dict) {
  login::NetworkStateHelper helper_;
  dict->Set(
      "testapi_shouldSkipNetworkFirstShow",
      features::IsOobeNetworkScreenSkipEnabled() &&
          !ash::switches::IsOOBENetworkScreenSkippingDisabledForTesting() &&
          helper_.IsConnectedToEthernet());
  dict->Set("testapi_shouldSkipEula",
            policy::EnrollmentRequisitionManager::IsRemoraRequisition() ||
                StartupUtils::IsEulaAccepted() ||
                features::IsOobeConsolidatedConsentEnabled() ||
                !BUILDFLAG(GOOGLE_CHROME_BRANDING));

  dict->Set("testapi_shouldSkipGuestTos",
            StartupUtils::IsEulaAccepted() ||
                !features::IsOobeConsolidatedConsentEnabled() ||
                !BUILDFLAG(GOOGLE_CHROME_BRANDING));
}

void OobeTestAPIHandler::LoginWithPin(const std::string& username,
                                      const std::string& pin) {
  LoginScreenClientImpl::Get()->AuthenticateUserWithPasswordOrPin(
      AccountId::FromUserEmail(username), pin, /*authenticated_by_pin=*/true,
      base::BindOnce([](bool success) {
        LOG_IF(ERROR, !success) << "Failed to authenticate with pin";
      }));
}

void OobeTestAPIHandler::AdvanceToScreen(const std::string& screen) {
  ash::LoginDisplayHost::default_host()->StartWizard(ash::OobeScreenId(screen));
}

void OobeTestAPIHandler::SkipPostLoginScreens() {
  ash::WizardController::SkipPostLoginScreensForTesting();
}

void OobeTestAPIHandler::LoginAsGuest() {
  ash::WizardController::default_controller()
      ->SkipToLoginForTesting();  // IN-TEST
  CHECK(ash::ExistingUserController::current_controller());
  UserContext context(user_manager::USER_TYPE_GUEST, EmptyAccountId());
  ash::ExistingUserController::current_controller()->Login(context,
                                                           SigninSpecifics());
}

void OobeTestAPIHandler::ShowGaiaDialog() {
  LoginDisplayHost::default_host()->ShowGaiaDialog(EmptyAccountId());
}
void OobeTestAPIHandler::HandleGetPrimaryDisplayName(
    const std::string& callback_id) {
  mojo::Remote<ash::mojom::CrosDisplayConfigController> cros_display_config;
  ash::BindCrosDisplayConfigController(
      cros_display_config.BindNewPipeAndPassReceiver());

  cros_display_config->GetDisplayUnitInfoList(
      false /* single_unified */,
      base::BindOnce(&OobeTestAPIHandler::OnGetDisplayUnitInfoList,
                     base::Unretained(this), callback_id));
}

void OobeTestAPIHandler::OnGetDisplayUnitInfoList(
    const std::string& callback_id,
    std::vector<ash::mojom::DisplayUnitInfoPtr> info_list) {
  std::string display_name;
  for (const ash::mojom::DisplayUnitInfoPtr& info : info_list) {
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

}  // namespace chromeos
