// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/arc_vm_data_migration_screen_handler.h"

#include "chrome/browser/ash/login/screens/arc_vm_data_migration_screen.h"
#include "components/login/localized_values_builder.h"
#include "ui/base/text/bytes_formatting.h"

namespace ash {

ArcVmDataMigrationScreenHandler::ArcVmDataMigrationScreenHandler()
    : BaseScreenHandler(kScreenId) {}

ArcVmDataMigrationScreenHandler::~ArcVmDataMigrationScreenHandler() = default;

void ArcVmDataMigrationScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  // TODO(b/258278176): Replace strings with l10n ones.
  builder->Add("loadingDialogTitle", u"Loading...");
  builder->Add("welcomeScreenTitle", u"Update your Chromebook");
  builder->Add("welcomeScreenDescriptionHeader", u"What to expect");
  builder->Add("welcomeScreenDescriptionBody",
               u"This is a critical update. During the update you will not be "
               u"able to use your device for up to 10 minutes. Please keep "
               u"your device connected to a charger during the update.");
  builder->Add("notEnoughFreeDiskSpaceMessage",
               u"Free up more than $1 of space");
  builder->Add("skipButtonLabel", u"Remind me later");
  builder->Add("updateButtonLabel", u"Next");
}

void ArcVmDataMigrationScreenHandler::Show() {
  ShowInWebUI();
}

void ArcVmDataMigrationScreenHandler::SetUIState(UIState state) {
  CallExternalAPI("setUIState", static_cast<int>(state));
}

void ArcVmDataMigrationScreenHandler::SetRequiredFreeDiskSpace(
    int64_t required_free_disk_space) {
  CallExternalAPI("setRequiredFreeDiskSpace",
                  ui::FormatBytes(required_free_disk_space));
}

}  // namespace ash
