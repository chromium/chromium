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

OsTrialScreenHandler::OsTrialScreenHandler() : BaseScreenHandler(kScreenId) {
  set_user_acted_method_path_deprecated("login.OsTrialScreen.userActed");
}

OsTrialScreenHandler::~OsTrialScreenHandler() {
  if (screen_)
    screen_->OnViewDestroyed(this);
}

void OsTrialScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->AddF("osTrialTitle", IDS_OS_TRIAL_TITLE,
                IDS_INSTALLED_PRODUCT_OS_NAME);
  builder->AddF("osTrialSubtitle", IDS_OS_TRIAL_SUBTITLE,
                IDS_INSTALLED_PRODUCT_OS_NAME);
  builder->AddF("osTrialInstallTitle", IDS_OS_TRIAL_INSTALL_TITLE,
                IDS_INSTALLED_PRODUCT_OS_NAME);
  builder->AddF("osTrialInstallSubtitle", IDS_OS_TRIAL_INSTALL_SUBTITLE,
                IDS_INSTALLED_PRODUCT_OS_NAME);
  builder->Add("osTrialTryTitle", IDS_OS_TRIAL_TRY_TITLE);
  builder->AddF("osTrialTrySubtitle", IDS_OS_TRIAL_TRY_SUBTITLE,
                IDS_INSTALLED_PRODUCT_OS_NAME);
  builder->Add("osTrialNextButton", IDS_OS_TRIAL_NEXT_BUTTON);
}

void OsTrialScreenHandler::InitializeDeprecated() {}

void OsTrialScreenHandler::Show() {
  ShowInWebUI();
}

void OsTrialScreenHandler::Bind(ash::OsTrialScreen* screen) {
  screen_ = screen;
  BaseScreenHandler::SetBaseScreenDeprecated(screen_);
}

void OsTrialScreenHandler::Unbind() {
  screen_ = nullptr;
  BaseScreenHandler::SetBaseScreenDeprecated(nullptr);
}

}  // namespace chromeos
