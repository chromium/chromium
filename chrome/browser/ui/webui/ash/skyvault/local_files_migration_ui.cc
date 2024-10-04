// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/webui/ash/skyvault/local_files_migration_ui.h"

#include "base/functional/bind.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ash/skyvault/local_files_migration_dialog.h"
#include "chrome/browser/ui/webui/ash/skyvault/local_files_migration_page_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/skyvault_resources.h"
#include "chrome/grit/skyvault_resources_map.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/web_dialogs/web_dialog_ui.h"
#include "ui/webui/color_change_listener/color_change_handler.h"

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
      // Cloud providers
      {"googleDrive", IDS_POLICY_SKYVAULT_CLOUD_PROVIDER_GOOGLE_DRIVE},
      {"oneDrive", IDS_POLICY_SKYVAULT_CLOUD_PROVIDER_ONEDRIVE},
      // Title
      {"titleHour", IDS_POLICY_SKYVAULT_MIGRATION_DIALOG_TITLE_HOUR},
      {"titleHours", IDS_POLICY_SKYVAULT_MIGRATION_DIALOG_TITLE_HOURS},
      {"titleMinute", IDS_POLICY_SKYVAULT_MIGRATION_DIALOG_TITLE_MINUTE},
      {"titleMinutes", IDS_POLICY_SKYVAULT_MIGRATION_DIALOG_TITLE_MINUTES},
      // Body
      {"uploadStartMessage",
       IDS_POLICY_SKYVAULT_MIGRATION_DIALOG_UPLOAD_START_MESSAGE},
      {"uploadDoneMessage",
       IDS_POLICY_SKYVAULT_MIGRATION_DIALOG_UPLOAD_DONE_MESSAGE},
      // Buttons
      {"uploadNow", IDS_POLICY_SKYVAULT_MIGRATION_DIALOG_UPLOAD_NOW_BUTTON},
      {"uploadInHours",
       IDS_POLICY_SKYVAULT_MIGRATION_DIALOG_UPLOAD_IN_HOURS_BUTTON},
      {"uploadInMinutes",
       IDS_POLICY_SKYVAULT_MIGRATION_DIALOG_UPLOAD_IN_MINUTES_BUTTON},
  };
  source->AddLocalizedStrings(kStrings);

  webui::SetupWebUIDataSource(
      source, base::make_span(kSkyvaultResources, kSkyvaultResourcesSize),
      IDR_SKYVAULT_LOCAL_FILES_HTML);

  webui::EnableTrustedTypesCSP(source);
}

LocalFilesMigrationUI::~LocalFilesMigrationUI() = default;

void LocalFilesMigrationUI::BindInterface(
    mojo::PendingReceiver<mojom::PageHandlerFactory> receiver) {
  factory_receiver_.reset();
  factory_receiver_.Bind(std::move(receiver));
}

void LocalFilesMigrationUI::BindInterface(
    mojo::PendingReceiver<color_change_listener::mojom::PageHandler> receiver) {
  color_provider_handler_ = std::make_unique<ui::ColorChangeHandler>(
      web_ui()->GetWebContents(), std::move(receiver));
}

void LocalFilesMigrationUI::CreatePageHandler(
    mojo::PendingRemote<mojom::Page> page,
    mojo::PendingReceiver<mojom::PageHandler> receiver) {
  CHECK(page.is_valid());
  CHECK(!handler_);

  handler_ = std::make_unique<LocalFilesMigrationPageHandler>(
      web_ui(), Profile::FromWebUI(web_ui()), cloud_provider_,
      migration_start_time_,
      base::BindOnce(&LocalFilesMigrationUI::ProcessResponseAndCloseDialog,
                     base::Unretained(this)),
      std::move(page), std::move(receiver));
}

void LocalFilesMigrationUI::SetInitialDialogInfo(
    CloudProvider cloud_provider,
    base::Time migration_start_time) {
  cloud_provider_ = cloud_provider;
  migration_start_time_ = migration_start_time;
}

void LocalFilesMigrationUI::ProcessResponseAndCloseDialog(UserAction action) {
  base::Value::List values;
  if (action == UserAction::kUploadNow) {
    // Signal to the dialog to run the migration callback.
    values.Append(kStartMigration);
  }
  CloseDialog(values);
}

WEB_UI_CONTROLLER_TYPE_IMPL(LocalFilesMigrationUI)

}  // namespace policy::local_user_files
