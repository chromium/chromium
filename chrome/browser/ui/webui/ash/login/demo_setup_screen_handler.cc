// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/demo_setup_screen_handler.h"

#include <string>

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"

namespace ash {

DemoSetupScreenView::~DemoSetupScreenView() = default;

DemoSetupScreenHandler::DemoSetupScreenHandler()
    : BaseScreenHandler(kScreenId) {}

DemoSetupScreenHandler::~DemoSetupScreenHandler() = default;

void DemoSetupScreenHandler::Show() {
  ShowInWebUI();
}

void DemoSetupScreenHandler::OnSetupFailed(
    const DemoSetupController::DemoSetupError& error) {
  // TODO(wzang): Consider customization for RecoveryMethod::kReboot as well.
  CallExternalAPI(
      "onSetupFailed",
      base::JoinString({error.GetLocalizedErrorMessage(),
                        error.GetLocalizedRecoveryMessage()},
                       u" "),
      error.recovery_method() ==
          DemoSetupController::DemoSetupError::RecoveryMethod::kPowerwash);
}

void DemoSetupScreenHandler::SetCurrentSetupStep(
    DemoSetupController::DemoSetupStep current_step) {
  CallExternalAPI("setCurrentSetupStep",
                  DemoSetupController::GetDemoSetupStepString(current_step));
}

void DemoSetupScreenHandler::OnSetupSucceeded() {
  CallExternalAPI("onSetupSucceeded");
}

base::WeakPtr<DemoSetupScreenView> DemoSetupScreenHandler::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void DemoSetupScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("demoSetupProgressScreenTitle",
               IDS_OOBE_DEMO_SETUP_PROGRESS_SCREEN_TITLE);
  builder->Add("demoSetupErrorScreenTitle",
               IDS_OOBE_DEMO_SETUP_ERROR_SCREEN_TITLE);
  builder->Add("demoSetupErrorScreenRetryButtonLabel",
               IDS_OOBE_DEMO_SETUP_ERROR_SCREEN_RETRY_BUTTON_LABEL);
  builder->Add("demoSetupErrorScreenPowerwashButtonLabel",
               IDS_LOCAL_STATE_ERROR_POWERWASH_BUTTON);

  builder->Add("demoSetupProgressStepDownload",
               IDS_OOBE_DEMO_SETUP_PROGRESS_STEP_DOWNLOAD);
  builder->Add("demoSetupProgressStepEnroll",
               IDS_OOBE_DEMO_SETUP_PROGRESS_STEP_ENROLL);
}

void DemoSetupScreenHandler::GetAdditionalParameters(
    base::Value::Dict* parameters) {
  parameters->Set("demoSetupSteps", DemoSetupController::GetDemoSetupSteps());
}

}  // namespace ash
