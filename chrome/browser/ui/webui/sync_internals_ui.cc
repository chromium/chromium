// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/sync_internals_ui.h"

#include <memory>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/sync_internals_message_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "components/grit/sync_driver_resources.h"
#include "components/sync/driver/sync_internals_util.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"

namespace {

content::WebUIDataSource* CreateSyncInternalsHTMLSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUISyncInternalsHost);
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome://resources 'self' 'unsafe-eval';");
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::TrustedTypes,
      "trusted-types jstemplate cr-ui-tree-js-static;");

  source->UseStringsJs();

  static constexpr webui::ResourcePath kResources[] = {
      {syncer::sync_ui_util::kSyncIndexJS,
       IDR_SYNC_DRIVER_SYNC_INTERNALS_INDEX_JS},
      {syncer::sync_ui_util::kChromeSyncJS,
       IDR_SYNC_DRIVER_SYNC_INTERNALS_CHROME_SYNC_JS},
      {syncer::sync_ui_util::kSyncLogJS,
       IDR_SYNC_DRIVER_SYNC_INTERNALS_SYNC_LOG_JS},
      {syncer::sync_ui_util::kSyncNodeBrowserJS,
       IDR_SYNC_DRIVER_SYNC_INTERNALS_SYNC_NODE_BROWSER_JS},
      {syncer::sync_ui_util::kSyncSearchJS,
       IDR_SYNC_DRIVER_SYNC_INTERNALS_SYNC_SEARCH_JS},
      {syncer::sync_ui_util::kAboutJS, IDR_SYNC_DRIVER_SYNC_INTERNALS_ABOUT_JS},
      {syncer::sync_ui_util::kDataJS, IDR_SYNC_DRIVER_SYNC_INTERNALS_DATA_JS},
      {syncer::sync_ui_util::kEventsJS,
       IDR_SYNC_DRIVER_SYNC_INTERNALS_EVENTS_JS},
      {syncer::sync_ui_util::kSearchJS,
       IDR_SYNC_DRIVER_SYNC_INTERNALS_SEARCH_JS},
      {syncer::sync_ui_util::kUserEventsJS,
       IDR_SYNC_DRIVER_SYNC_INTERNALS_USER_EVENTS_JS},
      {syncer::sync_ui_util::kTrafficLogJS,
       IDR_SYNC_DRIVER_SYNC_INTERNALS_TRAFFIC_LOG_JS},
      {syncer::sync_ui_util::kInvalidationsJS,
       IDR_SYNC_DRIVER_SYNC_INTERNALS_INVALIDATIONS_JS},
  };
  webui::AddResourcePathsBulk(source, kResources);

  source->SetDefaultResource(IDR_SYNC_DRIVER_SYNC_INTERNALS_INDEX_HTML);
  return source;
}

}  // namespace

SyncInternalsUI::SyncInternalsUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, CreateSyncInternalsHTMLSource());

  web_ui->AddMessageHandler(std::make_unique<SyncInternalsMessageHandler>());
}

SyncInternalsUI::~SyncInternalsUI() {}
