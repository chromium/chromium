// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/webui/ash/skyvault/local_files_migration_ui.h"

#include "base/functional/bind.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ash/skyvault/local_files_migration_dialog.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/skyvault_resources.h"
#include "chrome/grit/skyvault_resources_map.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/web_dialogs/web_dialog_ui.h"

namespace policy::local_user_files {

bool LocalFilesMigrationUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return base::FeatureList::IsEnabled(features::kSkyVault) &&
         base::FeatureList::IsEnabled(features::kSkyVaultV2);
}

LocalFilesMigrationUI::LocalFilesMigrationUI(content::WebUI* web_ui)
    : ui::WebDialogUI(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      Profile::FromWebUI(web_ui), chrome::kChromeUILocalFilesMigrationHost);

  // TODO(b/342340599): Pass strings etc.
  webui::SetupWebUIDataSource(
      source, base::make_span(kSkyvaultResources, kSkyvaultResourcesSize),
      IDR_SKYVAULT_LOCAL_FILES_HTML);

  web_ui->RegisterMessageCallback(
      "startMigration",
      base::BindRepeating(&LocalFilesMigrationUI::HandleStartMigration,
                          base::Unretained(this)));

  webui::EnableTrustedTypesCSP(source);
}

LocalFilesMigrationUI::~LocalFilesMigrationUI() = default;

void LocalFilesMigrationUI::HandleStartMigration(
    const base::Value::List& args) {
  // Signal to the dialog to run the migration callback.
  base::Value::List values;
  values.Append(kStartMigration);
  CloseDialog(values);
}

WEB_UI_CONTROLLER_TYPE_IMPL(LocalFilesMigrationUI)

}  // namespace policy::local_user_files
