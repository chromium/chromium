// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ntp_microsoft_auth/ntp_microsoft_auth_untrusted_ui.h"

#include "base/strings/stringprintf.h"
#include "chrome/browser/new_tab_page/new_tab_page_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/new_tab_page/new_tab_page_ui.h"
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
  return IsMicrosoftModuleEnabledForProfile(
      Profile::FromBrowserContext(browser_context));
}

NtpMicrosoftAuthUntrustedUI::NtpMicrosoftAuthUntrustedUI(content::WebUI* web_ui)
    : ui::UntrustedWebUIController(web_ui),
      profile_(Profile::FromWebUI(web_ui)) {
  auto* browser_context = web_ui->GetWebContents()->GetBrowserContext();
  content::WebUIDataSource* untrusted_source =
      content::WebUIDataSource::CreateAndAdd(
          browser_context, chrome::kChromeUIUntrustedNtpMicrosoftAuthURL);
  untrusted_source->AddFrameAncestor(GURL(chrome::kChromeUINewTabPageURL));
  untrusted_source->AddResourcePath(
      "", IDR_NEW_TAB_PAGE_UNTRUSTED_MICROSOFT_AUTH_HTML);
  untrusted_source->AddResourcePath("msal_browser.js",
                                    IDR_NEW_TAB_PAGE_UNTRUSTED_MSAL_BROWSER_JS);
  untrusted_source->AddResourcePath(
      "microsoft_auth.js", IDR_NEW_TAB_PAGE_UNTRUSTED_MICROSOFT_AUTH_JS);
  untrusted_source->AddResourcePath(
      "microsoft_auth_proxy.js",
      IDR_NEW_TAB_PAGE_UNTRUSTED_MICROSOFT_AUTH_PROXY_JS);
  untrusted_source->AddResourcePath(
      "ntp_microsoft_auth_untrusted_ui.mojom-webui.js",
      IDR_NEW_TAB_PAGE_UNTRUSTED_NTP_MICROSOFT_AUTH_UNTRUSTED_UI_MOJOM_WEBUI_JS);
  untrusted_source->AddResourcePath(
      "ntp_microsoft_auth_shared_ui.mojom-webui.js",
      IDR_NEW_TAB_PAGE_UNTRUSTED_NTP_MICROSOFT_AUTH_SHARED_UI_MOJOM_WEBUI_JS);
  untrusted_source->AddResourcePath("msal_browser/msal-browser.min.js",
                                    IDR_MSAL_BROWSER_MSAL_BROWSER_MIN_JS);
  untrusted_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src 'self' "
      "chrome-untrusted://resources/mojo/mojo/public/js/bindings.js "
      "chrome-untrusted://resources/mojo/mojo/public/mojom/base/"
      "time.mojom-webui.js "
      "chrome-untrusted://resources/mojo/mojo/public/mojom/base/"
      "time_converters.js "
      "chrome-untrusted://resources/mojo/mojo/public/mojom/base/"
      "time.mojom-converters.js;");
  untrusted_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::DefaultSrc,
      base::StringPrintf("default-src 'self' %s;",
                         chrome::kChromeUINewTabPageURL));
  untrusted_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ConnectSrc,
      "connect-src https://login.microsoftonline.com;");
  untrusted_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ChildSrc,
      "child-src https://login.microsoftonline.com "
      "https://chromeenterprise.google;");
}

NtpMicrosoftAuthUntrustedUI::~NtpMicrosoftAuthUntrustedUI() = default;

void NtpMicrosoftAuthUntrustedUI::BindInterface(
    mojo::PendingReceiver<
        new_tab_page::mojom::MicrosoftAuthUntrustedDocumentInterfacesFactory>
        factory) {
  if (untrusted_page_factory_.is_bound()) {
    untrusted_page_factory_.reset();
  }

  untrusted_page_factory_.Bind(std::move(factory));
}

void NtpMicrosoftAuthUntrustedUI::CreatePageHandler(
    mojo::PendingReceiver<
        new_tab_page::mojom::MicrosoftAuthUntrustedPageHandler>
        pending_page_handler,
    mojo::PendingRemote<new_tab_page::mojom::MicrosoftAuthUntrustedDocument>
        pending_document) {
  page_handler_ = std::make_unique<MicrosoftAuthUntrustedPageHandler>(
      std::move(pending_page_handler), std::move(pending_document), profile_);
}

void NtpMicrosoftAuthUntrustedUI::ConnectToParentDocument(
    mojo::PendingRemote<new_tab_page::mojom::MicrosoftAuthUntrustedDocument>
        child_untrusted_document_remote) {
  // Find the parent frame's controller.
  auto* chrome_frame = web_ui()->GetWebContents()->GetPrimaryMainFrame();
  if (!chrome_frame) {
    return;
  }

  CHECK(chrome_frame->GetWebUI());

  auto* new_tab_page_ui_controller =
      chrome_frame->GetWebUI()->GetController()->GetAs<NewTabPageUI>();
  CHECK(new_tab_page_ui_controller);

  new_tab_page_ui_controller->ConnectToParentDocument(
      std::move(child_untrusted_document_remote));
}

WEB_UI_CONTROLLER_TYPE_IMPL(NtpMicrosoftAuthUntrustedUI)
