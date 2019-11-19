// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/enable_adb_sideloading_screen_handler.h"

#include <string>

#include "chrome/browser/chromeos/login/oobe_screen.h"
#include "chrome/browser/chromeos/login/screens/enable_adb_sideloading_screen.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {

constexpr StaticOobeScreenId EnableAdbSideloadingScreenView::kScreenId;

EnableAdbSideloadingScreenHandler::EnableAdbSideloadingScreenHandler(
    JSCallsContainer* js_calls_container)
    : BaseScreenHandler(kScreenId, js_calls_container) {
  set_user_acted_method_path("login.EnableAdbSideloadingScreen.userActed");
}

EnableAdbSideloadingScreenHandler::~EnableAdbSideloadingScreenHandler() {
  if (screen_)
    screen_->OnViewDestroyed(this);
}

void EnableAdbSideloadingScreenHandler::Show() {
  if (!page_is_ready()) {
    show_on_init_ = true;
    return;
  }
  ShowScreen(kScreenId);
}

void EnableAdbSideloadingScreenHandler::Hide() {}

void EnableAdbSideloadingScreenHandler::Bind(
    EnableAdbSideloadingScreen* screen) {
  screen_ = screen;
  BaseScreenHandler::SetBaseScreen(screen_);
}

void EnableAdbSideloadingScreenHandler::Unbind() {
  screen_ = nullptr;
  BaseScreenHandler::SetBaseScreen(nullptr);
}

void EnableAdbSideloadingScreenHandler::SetScreenState(UIState value) {
  CallJS("login.EnableAdbSideloadingScreen.setScreenState",
         static_cast<int>(value));
}

void EnableAdbSideloadingScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("enableAdbSideloadingSetupTitle",
               IDS_ENABLE_ARC_ADB_SIDELOADING_SETUP_TITLE);
  builder->Add("enableAdbSideloadingSetupMessage",
               IDS_ENABLE_ARC_ADB_SIDELOADING_SETUP_MESSAGE);
  builder->Add("enableAdbSideloadingIllustrationTitle",
               IDS_ENABLE_ARC_ADB_SIDELOADING_SETUP_ILLUSTRATION_TITLE);
  builder->Add("enableAdbSideloadingErrorTitle",
               IDS_ENABLE_ARC_ADB_SIDELOADING_ERROR_TITLE);
  builder->Add("enableAdbSideloadingErrorMessage",
               IDS_ENABLE_ARC_ADB_SIDELOADING_ERROR_MESSAGE);
  builder->Add("enableAdbSideloadingErrorIllustrationTitle",
               IDS_ENABLE_ARC_ADB_SIDELOADING_ERROR_ILLUSTRATION_TITLE);
  builder->Add("enableAdbSideloadingLearnMore",
               IDS_ENABLE_ARC_ADB_SIDELOADING_LEARN_MORE);
  builder->Add("enableAdbSideloadingConfirmButton",
               IDS_ENABLE_ARC_ADB_SIDELOADING_CONFIRM_BUTTON);
  builder->Add("enableAdbSideloadingCancelButton",
               IDS_ENABLE_ARC_ADB_SIDELOADING_CANCEL_BUTTON);
  builder->Add("enableAdbSideloadingOkButton",
               IDS_ENABLE_ARC_ADB_SIDELOADING_OK_BUTTON);
}

void EnableAdbSideloadingScreenHandler::Initialize() {
  if (!page_is_ready())
    return;

  if (show_on_init_) {
    Show();
    show_on_init_ = false;
  }
}

}  // namespace chromeos
