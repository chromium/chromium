// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/os_trial_screen_handler.h"

#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/screens/os_trial_screen.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"

namespace chromeos {

// static
constexpr StaticOobeScreenId OsTrialScreenView::kScreenId;

OsTrialScreenHandler::OsTrialScreenHandler(JSCallsContainer* js_calls_container)
    : BaseScreenHandler(kScreenId, js_calls_container) {
  set_user_acted_method_path("login.OsTrialScreen.userActed");
}

OsTrialScreenHandler::~OsTrialScreenHandler() {
  if (screen_)
    screen_->OnViewDestroyed(this);
}

void OsTrialScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("osTrialTitle", IDS_OS_TRIAL_TITLE);
  builder->Add("osTrialSubtitle", IDS_OS_TRIAL_SUBTITLE);
  builder->Add("osTrialInstallTitle", IDS_OS_TRIAL_INSTALL_TITLE);
  builder->Add("osTrialInstallSubtitle", IDS_OS_TRIAL_INSTALL_SUBTITLE);
  builder->Add("osTrialTryTitle", IDS_OS_TRIAL_TRY_TITLE);
  builder->Add("osTrialTrySubtitle", IDS_OS_TRIAL_TRY_SUBTITLE);
  builder->Add("osTrialNextButton", IDS_OS_TRIAL_NEXT_BUTTON);

  // OS names
  builder->Add("osInstallChromiumOS", IDS_CHROMIUM_OS_NAME);
  builder->Add("osInstallCloudReadyOS", IDS_CLOUD_READY_OS_NAME);
}

void OsTrialScreenHandler::Initialize() {}

void OsTrialScreenHandler::Show() {
  ShowScreen(kScreenId);
}

void OsTrialScreenHandler::Bind(ash::OsTrialScreen* screen) {
  screen_ = screen;
  BaseScreenHandler::SetBaseScreen(screen_);
}

void OsTrialScreenHandler::Unbind() {
  screen_ = nullptr;
  BaseScreenHandler::SetBaseScreen(nullptr);
}

void OsTrialScreenHandler::SetIsBrandedBuild(bool is_branded) {
  CallJS("login.OsTrialScreen.setIsBrandedBuild", is_branded);
}

}  // namespace chromeos
