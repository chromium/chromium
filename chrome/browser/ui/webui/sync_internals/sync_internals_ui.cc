// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/sync_internals/sync_internals_ui.h"

#include <memory>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/sync_internals/sync_internals_message_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "components/grit/sync_service_sync_internals_resources.h"
#include "components/grit/sync_service_sync_internals_resources_map.h"
#include "components/sync/service/sync_internals_util.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"

namespace {

void CreateAndAddSyncInternalsHTMLSource(Profile* profile) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUISyncInternalsHost);
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome://resources chrome://webui-test 'self' "
      "'unsafe-eval';");
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::TrustedTypes,
      "trusted-types jstemplate static-types;");

  source->UseStringsJs();
  source->AddResourcePaths(
      base::make_span(kSyncServiceSyncInternalsResources,
                      kSyncServiceSyncInternalsResourcesSize));

  source->SetDefaultResource(IDR_SYNC_SERVICE_SYNC_INTERNALS_INDEX_HTML);
}

}  // namespace

SyncInternalsUI::SyncInternalsUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  CreateAndAddSyncInternalsHTMLSource(Profile::FromWebUI(web_ui));

  web_ui->AddMessageHandler(std::make_unique<SyncInternalsMessageHandler>());
}

SyncInternalsUI::~SyncInternalsUI() = default;
