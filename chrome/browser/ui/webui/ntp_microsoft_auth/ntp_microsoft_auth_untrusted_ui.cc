// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ntp_microsoft_auth/ntp_microsoft_auth_untrusted_ui.h"

#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/new_tab_page_untrusted_resources.h"
#include "chrome/grit/new_tab_page_untrusted_resources_map.h"
#include "components/search/ntp_features.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

NtpMicrosoftAuthUntrustedUIConfig::NtpMicrosoftAuthUntrustedUIConfig()
    : DefaultWebUIConfig(content::kChromeUIUntrustedScheme,
                         chrome::kChromeUIUntrustedNtpMicrosoftAuthHost) {}

bool NtpMicrosoftAuthUntrustedUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return base::FeatureList::IsEnabled(
             ntp_features::kNtpMicrosoftAuthenticationModule) &&
         (base::FeatureList::IsEnabled(
              ntp_features::kNtpOutlookCalendarModule) ||
          base::FeatureList::IsEnabled(ntp_features::kNtpSharepointModule));
}

NtpMicrosoftAuthUntrustedUI::NtpMicrosoftAuthUntrustedUI(content::WebUI* web_ui)
    : ui::UntrustedWebUIController(web_ui) {
  auto* browser_context = web_ui->GetWebContents()->GetBrowserContext();
  content::WebUIDataSource* untrusted_source =
      content::WebUIDataSource::CreateAndAdd(
          browser_context, chrome::kChromeUIUntrustedNtpMicrosoftAuthURL);
  untrusted_source->AddFrameAncestor(GURL(chrome::kChromeUINewTabPageURL));
  untrusted_source->AddResourcePath(
      "", IDR_NEW_TAB_PAGE_UNTRUSTED_MICROSOFT_AUTH_HTML);
  untrusted_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc, "script-src 'self';");
  untrusted_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::DefaultSrc,
      base::StringPrintf("default-src %s;", chrome::kChromeUINewTabPageURL));
}

NtpMicrosoftAuthUntrustedUI::~NtpMicrosoftAuthUntrustedUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(NtpMicrosoftAuthUntrustedUI)
