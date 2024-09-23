// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/login/encryption_migration_screen_handler.h"

#include "base/system/sys_info.h"
#include "chrome/browser/ash/login/screens/encryption_migration_screen.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"
#include "ui/base/text/bytes_formatting.h"
#include "ui/chromeos/devicetype_utils.h"

namespace ash {

EncryptionMigrationScreenHandler::EncryptionMigrationScreenHandler()
    : BaseScreenHandler(kScreenId) {}

EncryptionMigrationScreenHandler::~EncryptionMigrationScreenHandler() = default;

void EncryptionMigrationScreenHandler::Show() {
  ShowInWebUI();
}

void EncryptionMigrationScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("migrationReadyTitle", IDS_ENCRYPTION_MIGRATION_READY_TITLE);
  builder->Add("migrationReadyDescription",
               ui::SubstituteChromeOSDeviceType(
                   IDS_ENCRYPTION_MIGRATION_READY_DESCRIPTION));
  builder->Add("migrationMigratingTitle",
               IDS_ENCRYPTION_MIGRATION_MIGRATING_TITLE);
  builder->Add("migrationMigratingDescription",
               ui::SubstituteChromeOSDeviceType(
                   IDS_ENCRYPTION_MIGRATION_MIGRATING_DESCRIPTION));
  builder->Add("migrationProgressLabel",
               IDS_ENCRYPTION_MIGRATION_PROGRESS_LABEL);
  builder->Add("migrationBatteryWarningLabel",
               IDS_ENCRYPTION_MIGRATION_BATTERY_WARNING_LABEL);
  builder->Add("migrationAskChargeMessage",
               ui::SubstituteChromeOSDeviceType(
                   IDS_ENCRYPTION_MIGRATION_ASK_CHARGE_MESSAGE));
  builder->Add("migrationNecessaryBatteryLevelLabel",
               IDS_ENCRYPTION_MIGRATION_NECESSARY_BATTERY_LEVEL_MESSAGE);
  builder->Add("migrationChargingLabel",
               IDS_ENCRYPTION_MIGRATION_CHARGING_LABEL);
  builder->Add("migrationFailedTitle", IDS_ENCRYPTION_MIGRATION_FAILED_TITLE);
  builder->Add("migrationFailedSubtitle",
               IDS_ENCRYPTION_MIGRATION_FAILED_SUBTITLE);
  builder->Add("migrationFailedMessage",
               ui::SubstituteChromeOSDeviceType(
                   IDS_ENCRYPTION_MIGRATION_FAILED_MESSAGE));
  builder->Add("migrationNospaceWarningLabel",
               IDS_ENCRYPTION_MIGRATION_NOSPACE_WARNING_LABEL);
  builder->Add("migrationAskFreeSpaceMessage",
               IDS_ENCRYPTION_MIGRATION_ASK_FREE_SPACE_MESSAGE);
  builder->Add("migrationAvailableSpaceLabel",
               IDS_ENCRYPTION_MIGRATION_AVAILABLE_SPACE_LABEL);
  builder->Add("migrationNecessarySpaceLabel",
               IDS_ENCRYPTION_MIGRATION_NECESSARY_SPACE_LABEL);
  builder->Add("migrationButtonUpdate", IDS_ENCRYPTION_MIGRATION_BUTTON_UPDATE);
  builder->Add("migrationButtonSkip", IDS_ENCRYPTION_MIGRATION_BUTTON_SKIP);
  builder->Add("migrationButtonRestart",
               IDS_ENCRYPTION_MIGRATION_BUTTON_RESTART);
  builder->Add("migrationButtonContinue",
               IDS_ENCRYPTION_MIGRATION_BUTTON_CONTINUE);
  builder->Add("migrationButtonSignIn", IDS_ENCRYPTION_MIGRATION_BUTTON_SIGNIN);
  builder->Add("migrationButtonReportAnIssue", IDS_REPORT_AN_ISSUE);
  builder->Add("migrationBoardName", base::SysInfo::GetLsbReleaseBoard());
  builder->Add("gaiaLoading", IDS_LOGIN_GAIA_LOADING_MESSAGE);
}

base::WeakPtr<EncryptionMigrationScreenView>
EncryptionMigrationScreenHandler::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace ash
