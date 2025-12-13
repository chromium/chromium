// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/skyvault/local_files_migration_ui.h"

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ash/policy/skyvault/histogram_helper.h"
#include "chrome/browser/ash/policy/skyvault/policy_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ash/skyvault/local_files_migration_dialog.h"
#include "chrome/browser/ui/webui/ash/skyvault/local_files_migration_page_handler.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/skyvault_resources.h"
#include "chrome/grit/skyvault_resources_map.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/web_dialogs/web_dialog_ui.h"
#include "ui/webui/webui_util.h"

namespace policy::local_user_files {

bool LocalFilesMigrationUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return base::FeatureList::IsEnabled(features::kSkyVault) &&
         base::FeatureList::IsEnabled(features::kSkyVaultV2);
}

LocalFilesMigrationUI::LocalFilesMigrationUI(content::WebUI* web_ui)
    : ui::MojoWebDialogUI(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      Profile::FromWebUI(web_ui), chrome::kChromeUILocalFilesMigrationHost);
  static constexpr webui::LocalizedString kStrings[] = {
      // Upload case:
      // Cloud providers
      {"googleDrive", IDS_POLICY_SKYVAULT_CLOUD_PROVIDER_GOOGLE_DRIVE},
      {"oneDrive", IDS_POLICY_SKYVAULT_CLOUD_PROVIDER_ONEDRIVE},
      // Title
      {"uploadTitleHour", IDS_POLICY_SKYVAULT_MIGRATION_DIALOG_TITLE_HOUR},
      {"uploadTitleHours", IDS_POLICY_SKYVAULT_MIGRATION_DIALOG_TITLE_HOURS},
      {"uploadTitleMinute", IDS_POLICY_SKYVAULT_MIGRATION_DIALOG_TITLE_MINUTE},
      {"uploadTitleMinutes",
       IDS_POLICY_SKYVAULT_MIGRATION_DIALOG_TITLE_MINUTES},
      // Body
      {"uploadStartMessage",
       IDS_POLICY_SKYVAULT_MIGRATION_DIALOG_UPLOAD_START_ON_MESSAGE},
      {"uploadDoneMessage",
       IDS_POLICY_SKYVAULT_MIGRATION_DIALOG_UPLOAD_DONE_MESSAGE},
      // Buttons
      {"uploadNow", IDS_POLICY_SKYVAULT_MIGRATION_DIALOG_UPLOAD_NOW_BUTTON},
      {"uploadInHours",
       IDS_POLICY_SKYVAULT_MIGRATION_DIALOG_UPLOAD_IN_HOURS_BUTTON},
      {"uploadInMinutes",
       IDS_POLICY_SKYVAULT_MIGRATION_DIALOG_UPLOAD_IN_MINUTES_BUTTON},
      // Delete case:
      // Title
      {"deleteTitleHours", IDS_POLICY_SKYVAULT_DELETION_DIALOG_TITLE_HOURS},
      {"deleteTitleHour", IDS_POLICY_SKYVAULT_DELETION_DIALOG_TITLE_HOUR},
      {"deleteTitleMinutes", IDS_POLICY_SKYVAULT_DELETION_DIALOG_TITLE_MINUTES},
      {"deleteTitleMinute", IDS_POLICY_SKYVAULT_DELETION_DIALOG_TITLE_MINUTE},
      // Body
      {"deleteStartMessage", IDS_POLICY_SKYVAULT_DELETION_DIALOG_START_MESSAGE},
      {"deleteStoreMessage", IDS_POLICY_SKYVAULT_DELETION_DIALOG_STORE_MESSAGE},
      // Buttons
      {"deleteNow", IDS_POLICY_SKYVAULT_DELETION_DIALOG_DELETE_NOW_BUTTON},
      {"deleteInHours",
       IDS_POLICY_SKYVAULT_DELETION_DIALOG_DELETE_IN_HOURS_BUTTON},
      {"deleteInMinutes",
       IDS_POLICY_SKYVAULT_DELETION_DIALOG_DELETE_IN_MINUTES_BUTTON},
  };
  source->AddLocalizedStrings(kStrings);

  webui::SetupWebUIDataSource(source, kSkyvaultResources,
                              IDR_SKYVAULT_LOCAL_FILES_HTML);

  webui::EnableTrustedTypesCSP(source);
}

LocalFilesMigrationUI::~LocalFilesMigrationUI() = default;

void LocalFilesMigrationUI::BindInterface(
    mojo::PendingReceiver<mojom::PageHandlerFactory> receiver) {
  factory_receiver_.reset();
  factory_receiver_.Bind(std::move(receiver));
}

void LocalFilesMigrationUI::CreatePageHandler(
    mojo::PendingRemote<mojom::Page> page,
    mojo::PendingReceiver<mojom::PageHandler> receiver) {
  CHECK(page.is_valid());
  CHECK(!handler_);

  handler_ = std::make_unique<LocalFilesMigrationPageHandler>(
      web_ui(), Profile::FromWebUI(web_ui()), destination_,
      migration_start_time_,
      base::BindOnce(&LocalFilesMigrationUI::ProcessResponseAndCloseDialog,
                     base::Unretained(this)),
      std::move(page), std::move(receiver));
}

void LocalFilesMigrationUI::SetInitialDialogInfo(
    MigrationDestination destination,
    base::Time migration_start_time) {
  destination_ = destination;
  migration_start_time_ = migration_start_time;
}

void LocalFilesMigrationUI::ProcessResponseAndCloseDialog(DialogAction action) {
  base::Value::List values;
  if (action == DialogAction::kUploadNow) {
    // Signal to the dialog to run the migration callback.
    values.Append(kStartMigration);
  }
  CloseDialog(values);
  SkyVaultMigrationDialogActionHistogram(destination_, action);
}

WEB_UI_CONTROLLER_TYPE_IMPL(LocalFilesMigrationUI)

}  // namespace policy::local_user_files
