// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/download_internals/download_internals_ui.h"

#include <utility>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/download_internals/download_internals_ui_message_handler.h"
#include "chrome/common/url_constants.h"
#include "components/grit/download_internals_resources.h"
#include "components/grit/download_internals_resources_map.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"

DownloadInternalsUI::DownloadInternalsUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  // chrome://download-internals source.
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::Create(chrome::kChromeUIDownloadInternalsHost);
  html_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome://resources 'self' 'unsafe-eval';");
  html_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::TrustedTypes,
      "trusted-types jstemplate;");

  // Required resources.
  html_source->UseStringsJs();
  html_source->AddResourcePaths(base::make_span(
      kDownloadInternalsResources, kDownloadInternalsResourcesSize));
  html_source->AddResourcePath("",
                               IDR_DOWNLOAD_INTERNALS_DOWNLOAD_INTERNALS_HTML);

  Profile* profile = Profile::FromWebUI(web_ui);

  content::WebUIDataSource::Add(profile, html_source);

  web_ui->AddMessageHandler(
      std::make_unique<
          download_internals::DownloadInternalsUIMessageHandler>());
}

DownloadInternalsUI::~DownloadInternalsUI() = default;
