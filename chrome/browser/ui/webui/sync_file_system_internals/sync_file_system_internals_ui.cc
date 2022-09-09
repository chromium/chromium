// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/sync_file_system_internals/sync_file_system_internals_ui.h"

#include <memory>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/sync_file_system_internals/dump_database_handler.h"
#include "chrome/browser/ui/webui/sync_file_system_internals/extension_statuses_handler.h"
#include "chrome/browser/ui/webui/sync_file_system_internals/file_metadata_handler.h"
#include "chrome/browser/ui/webui/sync_file_system_internals/sync_file_system_internals_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/sync_file_system_internals_resources.h"
#include "chrome/grit/sync_file_system_internals_resources_map.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"

namespace {

content::WebUIDataSource* CreateSyncFileSystemInternalsHTMLSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(
          chrome::kChromeUISyncFileSystemInternalsHost);
  source->UseStringsJs();
  source->AddResourcePaths(
      base::make_span(kSyncFileSystemInternalsResources,
                      kSyncFileSystemInternalsResourcesSize));
  source->SetDefaultResource(IDR_SYNC_FILE_SYSTEM_INTERNALS_MAIN_HTML);
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::TrustedTypes,
      "trusted-types static-types;");
  return source;
}

}  // namespace

SyncFileSystemInternalsUI::SyncFileSystemInternalsUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  web_ui->AddMessageHandler(
      std::make_unique<syncfs_internals::SyncFileSystemInternalsHandler>(
          profile));
  web_ui->AddMessageHandler(
      std::make_unique<syncfs_internals::ExtensionStatusesHandler>(profile));
  web_ui->AddMessageHandler(
      std::make_unique<syncfs_internals::FileMetadataHandler>(profile));
  web_ui->AddMessageHandler(
      std::make_unique<syncfs_internals::DumpDatabaseHandler>(profile));
  content::WebUIDataSource::Add(profile,
                                CreateSyncFileSystemInternalsHTMLSource());
}

SyncFileSystemInternalsUI::~SyncFileSystemInternalsUI() {}
