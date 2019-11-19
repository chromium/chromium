// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/download_internals/download_internals_ui.h"

#include <utility>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/download_internals/download_internals_ui_message_handler.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"

DownloadInternalsUI::DownloadInternalsUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  // chrome://download-internals source.
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::Create(chrome::kChromeUIDownloadInternalsHost);
  html_source->OverrideContentSecurityPolicyScriptSrc(
      "script-src chrome://resources 'self' 'unsafe-eval';");

  // Required resources.
  html_source->UseStringsJs();
  html_source->AddResourcePath("download_internals.css",
                               IDR_DOWNLOAD_INTERNALS_CSS);
  html_source->AddResourcePath("download_internals.js",
                               IDR_DOWNLOAD_INTERNALS_JS);
  html_source->AddResourcePath("download_internals_browser_proxy.js",
                               IDR_DOWNLOAD_INTERNALS_BROWSER_PROXY_JS);
  html_source->AddResourcePath("download_internals_visuals.js",
                               IDR_DOWNLOAD_INTERNALS_VISUALS_JS);
  html_source->SetDefaultResource(IDR_DOWNLOAD_INTERNALS_HTML);

  Profile* profile = Profile::FromWebUI(web_ui);

  content::WebUIDataSource::Add(profile, html_source);

  web_ui->AddMessageHandler(
      std::make_unique<
          download_internals::DownloadInternalsUIMessageHandler>());
}

DownloadInternalsUI::~DownloadInternalsUI() = default;
