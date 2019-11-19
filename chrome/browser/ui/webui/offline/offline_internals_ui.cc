// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/offline/offline_internals_ui.h"

#include <memory>

#include "base/macros.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/offline/offline_internals_ui_message_handler.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"

OfflineInternalsUI::OfflineInternalsUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  // chrome://offline-internals source.
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::Create(chrome::kChromeUIOfflineInternalsHost);

  // Required resources.
  html_source->UseStringsJs();
  html_source->AddResourcePath("offline_internals.css",
                               IDR_OFFLINE_INTERNALS_CSS);
  html_source->AddResourcePath("offline_internals.js",
                               IDR_OFFLINE_INTERNALS_JS);
  html_source->AddResourcePath("offline_internals_browser_proxy.js",
                               IDR_OFFLINE_INTERNALS_BROWSER_PROXY_JS);
  html_source->SetDefaultResource(IDR_OFFLINE_INTERNALS_HTML);

  Profile* profile = Profile::FromWebUI(web_ui);
  html_source->AddBoolean("isIncognito", profile->IsOffTheRecord());

  content::WebUIDataSource::Add(profile, html_source);

  web_ui->AddMessageHandler(
      std::make_unique<offline_internals::OfflineInternalsUIMessageHandler>());
}

OfflineInternalsUI::~OfflineInternalsUI() {}
