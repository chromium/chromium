// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/arc_vm_data_migration_screen_handler.h"

#include "chrome/browser/ash/login/screens/arc_vm_data_migration_screen.h"
#include "components/login/localized_values_builder.h"
#include "components/strings/grit/components_strings.h"

namespace ash {

ArcVmDataMigrationScreenHandler::ArcVmDataMigrationScreenHandler()
    : BaseScreenHandler(kScreenId) {}

ArcVmDataMigrationScreenHandler::~ArcVmDataMigrationScreenHandler() = default;

void ArcVmDataMigrationScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("loadingDialogTitle",
               IDS_ARC_VM_DATA_MIGRATION_LOADING_SCREEN_MESSAGE);
  builder->Add("welcomeScreenTitle",
               IDS_ARC_VM_DATA_MIGRATION_WELCOME_SCREEN_TITLE);
  builder->Add("welcomeScreenDescriptionHeader",
               IDS_ARC_VM_DATA_MIGRATION_WHAT_TO_EXPECT_TITLE);
  builder->Add("welcomeScreenUpdateDescription",
               IDS_ARC_VM_DATA_MIGRATION_UPDATE_DESCRIPTION);
  builder->Add("welcomeScreenBlockingBehaviorDescription",
               IDS_ARC_VM_DATA_MIGRATION_BLOCKING_BEHAVIOR_DESCRIPTION);
  builder->Add("connectToChargerMessage",
               IDS_ARC_VM_DATA_MIGRATION_CONNECT_TO_CHARGER_MESSAGE);
  builder->Add("notEnoughFreeDiskSpaceMessage",
               IDS_ARC_VM_DATA_MIGRATION_NOT_ENOUGH_FREE_DISK_SPACE_MESSAGE);
  builder->Add("notEnoughBatteryMessage",
               IDS_ARC_VM_DATA_MIGRATION_NOT_ENOUGH_BATTERY_MESSAGE);
  builder->Add("skipButtonLabel",
               IDS_ARC_VM_DATA_MIGRATION_SCREEN_SKIP_BUTTON_LABEL);
  builder->Add("updateButtonLabel",
               IDS_ARC_VM_DATA_MIGRATION_SCREEN_UPDATE_BUTTON_LABEL);
  builder->Add("resumeScreenTitle",
               IDS_ARC_VM_DATA_MIGRATION_RESUME_SCREEN_TITLE);
  builder->Add("resumeScreenDescriptionHeader",
               IDS_ARC_VM_DATA_MIGRATION_WHAT_TO_EXPECT_TITLE);
  builder->Add("resumeScreenDescriptionBody",
               IDS_ARC_VM_DATA_MIGRATION_RESUME_DESCRIPTION);
  builder->Add("resumeButtonLabel",
               IDS_ARC_VM_DATA_MIGRATION_SCREEN_RESUME_BUTTON_LABEL);
  builder->Add("progressScreenTitle",
               IDS_ARC_VM_DATA_MIGRATION_PROGRESS_SCREEN_TITLE);
  builder->Add("progressScreenSubtitle",
               IDS_ARC_VM_DATA_MIGRATION_PROGRESS_DESCRIPTION);
  builder->Add("successScreenTitle",
               IDS_ARC_VM_DATA_MIGRATION_SUCCESS_SCREEN_TITLE);
  builder->Add("finishButtonLabel",
               IDS_ARC_VM_DATA_MIGRATION_SCREEN_FINISH_BUTTON_LABEL);
  builder->Add("failureScreenTitle",
               IDS_ARC_VM_DATA_MIGRATION_FAILURE_SCREEN_TITLE);
  builder->Add("failureScreenDescription",
               IDS_ARC_VM_DATA_MIGRATION_FAILURE_DESCRIPTION);
  builder->Add("failureScreenAskFeedbackReport",
               IDS_ARC_VM_DATA_MIGRATION_SEND_FEEDBACK_MESSAGE);
  builder->Add("reportButtonLabel",
               IDS_ARC_VM_DATA_MIGRATION_SCREEN_REPORT_BUTTON_LABEL);
}

void ArcVmDataMigrationScreenHandler::Show() {
  ShowInWebUI();
}

base::WeakPtr<ArcVmDataMigrationScreenView>
ArcVmDataMigrationScreenHandler::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace ash
