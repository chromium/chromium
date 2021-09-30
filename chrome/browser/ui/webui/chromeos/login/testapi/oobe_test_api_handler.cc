// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/testapi/oobe_test_api_handler.h"

#include "base/bind.h"
#include "base/logging.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/ash/login/startup_utils.h"
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
}

void OobeTestAPIHandler::Initialize() {}

void OobeTestAPIHandler::GetAdditionalParameters(base::DictionaryValue* dict) {
  dict->SetBoolean(
      "testapi_shouldSkipEula",
      policy::EnrollmentRequisitionManager::IsRemoraRequisition() ||
          StartupUtils::IsEulaAccepted() || !BUILDFLAG(GOOGLE_CHROME_BRANDING));
}

void OobeTestAPIHandler::LoginWithPin(const std::string& username,
                                      const std::string& pin) {
  LoginScreenClientImpl::Get()->AuthenticateUserWithPasswordOrPin(
      AccountId::FromUserEmail(username), pin, /*authenticated_by_pin=*/true,
      base::BindOnce([](bool success) {
        LOG_IF(ERROR, !success) << "Failed to authenticate with pin";
      }));
}

}  // namespace chromeos
