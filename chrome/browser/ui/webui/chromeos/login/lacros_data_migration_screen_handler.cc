// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/lacros_data_migration_screen_handler.h"

#include "chrome/browser/ash/login/screens/lacros_data_migration_screen.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"

namespace chromeos {

constexpr StaticOobeScreenId LacrosDataMigrationScreenView::kScreenId;

LacrosDataMigrationScreenHandler::LacrosDataMigrationScreenHandler(
    JSCallsContainer* js_calls_container)
    : BaseScreenHandler(kScreenId, js_calls_container) {
  set_user_acted_method_path("login.LacrosDataMigrationScreen.userActed");
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
}

void LacrosDataMigrationScreenHandler::Bind(LacrosDataMigrationScreen* screen) {
  BaseScreenHandler::SetBaseScreen(screen);
  screen_ = screen;
}

void LacrosDataMigrationScreenHandler::Unbind() {
  BaseScreenHandler::SetBaseScreen(nullptr);
  screen_ = nullptr;
}

void LacrosDataMigrationScreenHandler::Show() {
  if (!page_is_ready()) {
    show_on_init_ = true;
    return;
  }
  ShowScreen(kScreenId);
}

void LacrosDataMigrationScreenHandler::SetProgressValue(int progress) {
  CallJS("login.LacrosDataMigrationScreen.setProgressValue", progress);
}

void LacrosDataMigrationScreenHandler::ShowSkipButton() {
  CallJS("login.LacrosDataMigrationScreen.showSkipButton");
}

void LacrosDataMigrationScreenHandler::Initialize() {
  if (show_on_init_) {
    Show();
    show_on_init_ = false;
  }
}

}  // namespace chromeos
