// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/web_ui/safe_browsing_ui.h"

#include "components/grit/safe_browsing_resources.h"
#include "components/grit/safe_browsing_resources_map.h"
#include "components/safe_browsing/content/browser/web_ui/safe_browsing_ui_handler.h"
#include "components/safe_browsing/core/common/web_ui_constants.h"
#include "components/sync/protocol/user_event_specifics.pb.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

namespace safe_browsing {

SafeBrowsingUI::SafeBrowsingUI(
    content::WebUI* web_ui,
    std::unique_ptr<SafeBrowsingLocalStateDelegate> delegate,
    os_crypt_async::OSCryptAsync* os_crypt_async)
    : content::WebUIController(web_ui) {
  content::BrowserContext* browser_context =
      web_ui->GetWebContents()->GetBrowserContext();
  // Set up the chrome://safe-browsing source.
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::CreateAndAdd(
          browser_context, safe_browsing::kChromeUISafeBrowsingHost);

  // Register callback handler.
  // Handles messages from JavaScript to C++ via chrome.send().
  web_ui->AddMessageHandler(std::make_unique<SafeBrowsingUIHandler>(
      browser_context, std::move(delegate), os_crypt_async));

  // Add required resources.
  html_source->AddResourcePaths(kSafeBrowsingResources);
  html_source->AddResourcePath("", IDR_SAFE_BROWSING_SAFE_BROWSING_HTML);

  // Static types
  html_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::TrustedTypes,
      "trusted-types static-types;");
}

SafeBrowsingUI::~SafeBrowsingUI() = default;

CrSBLogMessage::CrSBLogMessage() = default;

CrSBLogMessage::~CrSBLogMessage() {
  WebUIContentInfoSingleton::GetInstance()->LogMessage(stream_.str());
  DLOG(WARNING) << stream_.str();
}

}  // namespace safe_browsing
