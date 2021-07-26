// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/testapi/oobe_test_api_handler.h"

#include "build/branding_buildflags.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_requisition_manager.h"

namespace chromeos {

OobeTestAPIHandler::OobeTestAPIHandler(JSCallsContainer* js_calls_container)
    : BaseWebUIHandler(js_calls_container) {
  DCHECK(js_calls_container);
}

OobeTestAPIHandler::~OobeTestAPIHandler() = default;

void OobeTestAPIHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {}

void OobeTestAPIHandler::Initialize() {}

void OobeTestAPIHandler::GetAdditionalParameters(base::DictionaryValue* dict) {
  dict->SetBoolean(
      "testapi_shouldSkipEula",
      policy::EnrollmentRequisitionManager::IsRemoraRequisition() ||
          !BUILDFLAG(GOOGLE_CHROME_BRANDING));
}

}  // namespace chromeos
