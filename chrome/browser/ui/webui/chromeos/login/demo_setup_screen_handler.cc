// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/demo_setup_screen_handler.h"

#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/login/oobe_screen.h"
#include "chrome/browser/chromeos/login/screens/demo_setup_screen.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"

namespace chromeos {

constexpr StaticOobeScreenId DemoSetupScreenView::kScreenId;

DemoSetupScreenView::~DemoSetupScreenView() = default;

DemoSetupScreenHandler::DemoSetupScreenHandler(
    JSCallsContainer* js_calls_container)
    : BaseScreenHandler(kScreenId, js_calls_container) {
  set_user_acted_method_path("login.DemoSetupScreen.userActed");
}

DemoSetupScreenHandler::~DemoSetupScreenHandler() {
  if (screen_)
    screen_->OnViewDestroyed(this);
}

void DemoSetupScreenHandler::Show() {
  ShowScreen(kScreenId);
}

void DemoSetupScreenHandler::Hide() {}

void DemoSetupScreenHandler::Bind(DemoSetupScreen* screen) {
  screen_ = screen;
  BaseScreenHandler::SetBaseScreen(screen);
}

void DemoSetupScreenHandler::OnSetupFailed(
    const DemoSetupController::DemoSetupError& error) {
  // TODO(wzang): Consider customization for RecoveryMethod::kReboot as well.
  CallJS("login.DemoSetupScreen.onSetupFailed",
         base::JoinString({error.GetLocalizedErrorMessage(),
                           error.GetLocalizedRecoveryMessage()},
                          base::UTF8ToUTF16(" ")),
         error.recovery_method() ==
             DemoSetupController::DemoSetupError::RecoveryMethod::kPowerwash);
}

void DemoSetupScreenHandler::OnSetupSucceeded() {
  CallJS("login.DemoSetupScreen.onSetupSucceeded");
}

void DemoSetupScreenHandler::Initialize() {}

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
}

}  // namespace chromeos
