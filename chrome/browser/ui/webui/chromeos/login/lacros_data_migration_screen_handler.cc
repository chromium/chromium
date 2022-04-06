// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/lacros_data_migration_screen_handler.h"

#include "chrome/browser/ash/login/screens/lacros_data_migration_screen.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"
#include "ui/base/text/bytes_formatting.h"

namespace chromeos {

constexpr StaticOobeScreenId LacrosDataMigrationScreenView::kScreenId;

LacrosDataMigrationScreenHandler::LacrosDataMigrationScreenHandler()
    : BaseScreenHandler(kScreenId) {
  set_user_acted_method_path_deprecated(
      "login.LacrosDataMigrationScreen.userActed");
}

LacrosDataMigrationScreenHandler::~LacrosDataMigrationScreenHandler() {
  if (screen_)
    screen_->OnViewDestroyed(this);
}

void LacrosDataMigrationScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("lacrosDataMigrationTitle",
               IDS_LACROS_DATA_MIGRATION_SCREEN_TITLE);
  builder->Add("lacrosDataMigrationSubtitle",
               IDS_LACROS_DATA_MIGRATION_SCREEN_SUBTITLE);
  builder->Add("lacrosDataMigrationSkipButton",
               IDS_LACROS_DATA_MIGRATION_SCREEN_SKIP_BUTTON);
  builder->Add("lacrosDataMigrationSkipSuggestion",
               IDS_LACROS_DATA_MIGRATION_SCREEN_SKIP_SUGGESTION);
  builder->Add("batteryWarningTitle", IDS_UPDATE_BATTERY_WARNING_TITLE);
  builder->Add("batteryWarningText", IDS_UPDATE_BATTERY_WARNING_TEXT);
  builder->Add("lacrosDataMigrationErrorTitle",
               IDS_LACROS_DATA_MIGRATION_SCREEN_ERROR_TITLE);
  builder->Add("lacrosDataMigrationErrorLowDiskSpace",
               IDS_LACROS_DATA_MIGRATION_SCREEN_ERROR_LOW_DISK_SPACE);
  builder->Add("lacrosDataMigrationErrorSubtitle",
               IDS_LACROS_DATA_MIGRATION_SCREEN_ERROR_SUBTITLE);
  builder->Add("lacrosDataMigrationErrorCancelButton",
               IDS_LACROS_DATA_MIGRATION_SCREEN_ERROR_CANCEL_BUTTON);
  builder->Add("lacrosDataMigrationErrorGotoFilesButton",
               IDS_LACROS_DATA_MIGRATION_SCREEN_ERROR_GOTO_FILES_BUTTON);
}

void LacrosDataMigrationScreenHandler::Bind(LacrosDataMigrationScreen* screen) {
  BaseScreenHandler::SetBaseScreenDeprecated(screen);
  screen_ = screen;
}

void LacrosDataMigrationScreenHandler::Unbind() {
  BaseScreenHandler::SetBaseScreenDeprecated(nullptr);
  screen_ = nullptr;
}

void LacrosDataMigrationScreenHandler::Show() {
  if (!IsJavascriptAllowed()) {
    show_on_init_ = true;
    return;
  }
  observation_.Observe(GetOobeUI());
  ShowInWebUI();
}

void LacrosDataMigrationScreenHandler::SetProgressValue(int progress) {
  CallJS("login.LacrosDataMigrationScreen.setProgressValue", progress);
}

void LacrosDataMigrationScreenHandler::ShowSkipButton() {
  CallJS("login.LacrosDataMigrationScreen.showSkipButton");
}

void LacrosDataMigrationScreenHandler::SetLowBatteryStatus(bool low_battery) {
  CallJS("login.LacrosDataMigrationScreen.setLowBatteryStatus", low_battery);
}

void LacrosDataMigrationScreenHandler::SetFailureStatus(
    const absl::optional<uint64_t>& required_size,
    bool show_goto_files) {
  CallJS("login.LacrosDataMigrationScreen.setFailureStatus",
         required_size.has_value()
             ? ui::FormatBytes(static_cast<int64_t>(required_size.value()))
             : std::u16string(),
         show_goto_files);
}

void LacrosDataMigrationScreenHandler::OnCurrentScreenChanged(
    OobeScreenId current_screen,
    OobeScreenId new_screen) {
  if (new_screen == kScreenId && screen_)
    screen_->OnViewVisible();
  observation_.Reset();
}

void LacrosDataMigrationScreenHandler::OnDestroyingOobeUI() {
  observation_.Reset();
}

void LacrosDataMigrationScreenHandler::InitializeDeprecated() {
  if (show_on_init_) {
    Show();
    show_on_init_ = false;
  }
}

}  // namespace chromeos
