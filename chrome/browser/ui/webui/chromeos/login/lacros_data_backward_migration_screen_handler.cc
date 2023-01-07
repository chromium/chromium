// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/lacros_data_backward_migration_screen_handler.h"

#include "chrome/browser/ash/login/screens/lacros_data_backward_migration_screen.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"

namespace chromeos {

LacrosDataBackwardMigrationScreenHandler::
    LacrosDataBackwardMigrationScreenHandler()
    : BaseScreenHandler(kScreenId) {}

LacrosDataBackwardMigrationScreenHandler::
    ~LacrosDataBackwardMigrationScreenHandler() = default;

void LacrosDataBackwardMigrationScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("lacrosDataBackwardMigrationTitle",
               IDS_LACROS_DATA_MIGRATION_SCREEN_TITLE);
  builder->Add("lacrosDataBackwardMigrationSubtitle",
               IDS_LACROS_DATA_MIGRATION_SCREEN_SUBTITLE);
}

void LacrosDataBackwardMigrationScreenHandler::Show() {
  ShowInWebUI();
}

}  // namespace chromeos
