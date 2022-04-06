// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/enable_adb_sideloading_screen_handler.h"

#include <string>

#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/screens/enable_adb_sideloading_screen.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {

EnableAdbSideloadingScreenHandler::EnableAdbSideloadingScreenHandler()
    : BaseScreenHandler(kScreenId) {}

EnableAdbSideloadingScreenHandler::~EnableAdbSideloadingScreenHandler() =
    default;

void EnableAdbSideloadingScreenHandler::Show() {
  if (!IsJavascriptAllowed()) {
    show_on_init_ = true;
    return;
  }
  ShowInWebUI();
}

void EnableAdbSideloadingScreenHandler::Hide() {}

void EnableAdbSideloadingScreenHandler::Bind(
    EnableAdbSideloadingScreen* screen) {
  BaseScreenHandler::SetBaseScreenDeprecated(screen);
}

void EnableAdbSideloadingScreenHandler::Unbind() {
  BaseScreenHandler::SetBaseScreenDeprecated(nullptr);
}

void EnableAdbSideloadingScreenHandler::SetScreenState(UIState value) {
  CallExternalAPI("setScreenState", static_cast<int>(value));
}

void EnableAdbSideloadingScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("enableAdbSideloadingSetupTitle",
               IDS_ENABLE_ARC_ADB_SIDELOADING_SETUP_TITLE);
  builder->Add("enableAdbSideloadingSetupMessage",
               IDS_ENABLE_ARC_ADB_SIDELOADING_SETUP_MESSAGE);
  builder->Add("enableAdbSideloadingErrorTitle",
               IDS_ENABLE_ARC_ADB_SIDELOADING_ERROR_TITLE);
  builder->Add("enableAdbSideloadingErrorMessage",
               IDS_ENABLE_ARC_ADB_SIDELOADING_ERROR_MESSAGE);
  builder->Add("enableAdbSideloadingLearnMore",
               IDS_ENABLE_ARC_ADB_SIDELOADING_LEARN_MORE);
  builder->Add("enableAdbSideloadingConfirmButton",
               IDS_ENABLE_ARC_ADB_SIDELOADING_CONFIRM_BUTTON);
  builder->Add("enableAdbSideloadingCancelButton",
               IDS_ENABLE_ARC_ADB_SIDELOADING_CANCEL_BUTTON);
  builder->Add("enableAdbSideloadingOkButton",
               IDS_ENABLE_ARC_ADB_SIDELOADING_OK_BUTTON);
}

void EnableAdbSideloadingScreenHandler::InitializeDeprecated() {
  if (!IsJavascriptAllowed())
    return;

  if (show_on_init_) {
    Show();
    show_on_init_ = false;
  }
}

}  // namespace chromeos
