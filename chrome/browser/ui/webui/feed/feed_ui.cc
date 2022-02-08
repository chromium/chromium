// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/feed/feed_ui.h"
#include "base/containers/span.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/grit/feed_resources.h"
#include "chrome/grit/feed_resources_map.h"
#include "components/feed/feed_feature_list.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/resource_path.h"

namespace feed {

FeedUI::FeedUI(content::WebUI* web_ui) : ui::UntrustedWebUIController(web_ui) {
  web_ui->AddRequestableScheme("https");
  // TODO(crbug.com/1292623): We should disable http requests before launching.
  web_ui->AddRequestableScheme("http");

  // Create a URLDataSource and add resources.
  auto* source = content::WebUIDataSource::Create("chrome-untrusted://feed/");
  webui::SetupWebUIDataSource(
      source, base::make_span(kFeedResources, kFeedResourcesSize),
      IDR_FEED_FEED_HTML);

  // TODO(crbug.com/1292623): CSP is weak during development and will be
  // tightened once the final architecture is decided.
  //  - Unsafe-eval/unsafe-inline is used by wasm code and is likely that we can
  //    avoid this for the final production version.
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome-untrusted://resources 'unsafe-eval' 'unsafe-inline' "
      "https://*.google.com https://google.com http://localhost:8000 'self';");

  // We want to be able to load frames from Google.
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::FrameSrc,
      "frame-src https://google.com https://*.google.com 'self';");
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::DefaultSrc, "default-src 'self';");

  // Configurable javascript for prototyping purposes.
  source->AddString("scriptUrl", feed::kWebUiScriptFetchUrl.Get());

  // Register the URLDataSource
  auto* browser_context = web_ui->GetWebContents()->GetBrowserContext();
  content::WebUIDataSource::Add(browser_context, source);
}

}  // namespace feed
