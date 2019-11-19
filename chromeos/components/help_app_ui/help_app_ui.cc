// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/help_app_ui/help_app_ui.h"
#include "chromeos/components/help_app_ui/help_app_guest_ui.h"

#include "chromeos/components/help_app_ui/url_constants.h"
#include "chromeos/grit/chromeos_help_app_resources.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

namespace chromeos {

namespace {
content::WebUIDataSource* CreateHostDataSource() {
  auto* source = content::WebUIDataSource::Create(kChromeUIHelpAppHost);

  // TODO(crbug.com/1012578): This is a placeholder only, update with the
  // actual app content.
  source->SetDefaultResource(IDR_HELP_APP_INDEX_HTML);
  source->AddResourcePath("pwa.html", IDR_HELP_APP_PWA_HTML);
  source->AddResourcePath("manifest.json", IDR_HELP_APP_MANIFEST);
  source->AddResourcePath("app_icon_192.png", IDR_HELP_APP_ICON_192);
  return source;
}
}  // namespace

HelpAppUI::HelpAppUI(content::WebUI* web_ui) : MojoWebUIController(web_ui) {
  content::BrowserContext* browser_context =
      web_ui->GetWebContents()->GetBrowserContext();
  content::WebUIDataSource* host_source = CreateHostDataSource();
  content::WebUIDataSource::Add(browser_context, host_source);
  // We need a CSP override to use the guest origin in the host.
  std::string csp = std::string("frame-src ") + kChromeUIHelpAppGuestURL + ";";
  host_source->OverrideContentSecurityPolicyChildSrc(csp);

  content::WebUIDataSource* guest_source = CreateHelpAppGuestDataSource();
  content::WebUIDataSource::Add(browser_context, guest_source);
}

HelpAppUI::~HelpAppUI() = default;

}  // namespace chromeos
