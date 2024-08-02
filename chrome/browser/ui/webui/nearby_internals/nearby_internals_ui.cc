// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/webui/nearby_internals/nearby_internals_ui.h"

#include <memory>

#include "base/containers/span.h"
#include "base/feature_list.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/nearby_internals/nearby_internals_contact_handler.h"
#include "chrome/browser/ui/webui/nearby_internals/nearby_internals_http_handler.h"
#include "chrome/browser/ui/webui/nearby_internals/nearby_internals_logs_handler.h"
#include "chrome/browser/ui/webui/nearby_internals/nearby_internals_prefs_handler.h"
#include "chrome/browser/ui/webui/nearby_internals/nearby_internals_ui_presence_handler.h"
#include "chrome/browser/ui/webui/nearby_internals/nearby_internals_ui_trigger_handler.h"
#include "chrome/browser/ui/webui/nearby_internals/quick_pair/quick_pair_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/nearby_internals_resources.h"
#include "chrome/grit/nearby_internals_resources_map.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/webui/web_ui_util.h"

NearbyInternalsUI::NearbyInternalsUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui, /*enable_chrome_send=*/true) {
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::CreateAndAdd(
          Profile::FromWebUI(web_ui), chrome::kChromeUINearbyInternalsHost);

  webui::SetupWebUIDataSource(
      html_source,
      base::make_span(kNearbyInternalsResources, kNearbyInternalsResourcesSize),
      IDR_NEARBY_INTERNALS_INDEX_HTML);

  content::BrowserContext* context =
      web_ui->GetWebContents()->GetBrowserContext();

  web_ui->AddMessageHandler(std::make_unique<NearbyInternalsLogsHandler>());
  web_ui->AddMessageHandler(
      std::make_unique<NearbyInternalsContactHandler>(context));
  web_ui->AddMessageHandler(
      std::make_unique<NearbyInternalsHttpHandler>(context));
  web_ui->AddMessageHandler(
      std::make_unique<NearbyInternalsPrefsHandler>(context));
  web_ui->AddMessageHandler(
      std::make_unique<NearbyInternalsPresenceHandler>(context));
  web_ui->AddMessageHandler(
      std::make_unique<NearbyInternalsUiTriggerHandler>(context));
  web_ui->AddMessageHandler(std::make_unique<QuickPairHandler>());
}

NearbyInternalsUI::~NearbyInternalsUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(NearbyInternalsUI)
