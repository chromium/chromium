// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/testapi/oobe_test_api_handler.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "base/bind.h"
#include "base/check.h"
#include "base/logging.h"
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

OobeTestAPIHandler::OobeTestAPIHandler(JSCallsContainer* js_calls_container)
    : BaseWebUIHandler(js_calls_container) {
  DCHECK(js_calls_container);
}

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
}

void OobeTestAPIHandler::Initialize() {}

void OobeTestAPIHandler::GetAdditionalParameters(base::DictionaryValue* dict) {
  login::NetworkStateHelper helper_;
  dict->SetBoolKey(
      "testapi_shouldSkipNetworkFirstShow",
      features::IsOobeNetworkScreenSkipEnabled() &&
          !ash::switches::IsOOBENetworkScreenSkippingDisabledForTesting() &&
          helper_.IsConnectedToEthernet());
  dict->SetBoolKey(
      "testapi_shouldSkipEula",
      policy::EnrollmentRequisitionManager::IsRemoraRequisition() ||
          StartupUtils::IsEulaAccepted() ||
          features::IsOobeConsolidatedConsentEnabled() ||
          !BUILDFLAG(GOOGLE_CHROME_BRANDING));

  dict->SetBoolKey("testapi_shouldSkipGuestTos",
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

}  // namespace chromeos
