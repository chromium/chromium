// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/lacros_data_backward_migration_screen_handler.h"

#include "chrome/browser/ash/login/screens/lacros_data_backward_migration_screen.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"

namespace ash {

LacrosDataBackwardMigrationScreenHandler::
    LacrosDataBackwardMigrationScreenHandler()
    : BaseScreenHandler(kScreenId) {}

LacrosDataBackwardMigrationScreenHandler::
    ~LacrosDataBackwardMigrationScreenHandler() = default;

void LacrosDataBackwardMigrationScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  // TODO(b/254435635): Update with backward migration specific strings.
  builder->Add("lacrosDataBackwardMigrationTitle",
               IDS_LACROS_DATA_MIGRATION_SCREEN_TITLE);
  builder->Add("lacrosDataBackwardMigrationSubtitle",
               IDS_LACROS_DATA_MIGRATION_SCREEN_SUBTITLE);
  builder->Add("lacrosDataBackwardMigrationErrorTitle",
               IDS_LACROS_DATA_MIGRATION_SCREEN_ERROR_TITLE);
  builder->Add("lacrosDataBackwardMigrationErrorSubtitle",
               IDS_LACROS_DATA_BACKWARD_MIGRATION_SCREEN_ERROR_SUBTITLE);
  builder->Add("lacrosDataBackwardMigrationErrorCancelButton",
               IDS_LACROS_DATA_MIGRATION_SCREEN_ERROR_CANCEL_BUTTON);
}

void LacrosDataBackwardMigrationScreenHandler::Show() {
  ShowInWebUI();
}

void LacrosDataBackwardMigrationScreenHandler::SetProgressValue(int progress) {
  CallExternalAPI("setProgressValue", progress);
}

void LacrosDataBackwardMigrationScreenHandler::SetFailureStatus() {
  CallExternalAPI("setFailureStatus");
}

}  // namespace ash
