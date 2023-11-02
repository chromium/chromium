// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/family_link_user_internals/family_link_user_internals_ui.h"

#include <memory>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/family_link_user_internals/family_link_user_internals_message_handler.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/dev_ui_browser_resources.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"

namespace {

content::WebUIDataSource* CreateFamilyLinkUserInternalsHTMLSource() {
  content::WebUIDataSource* source = content::WebUIDataSource::Create(
      chrome::kChromeUIFamilyLinkUserInternalsHost);
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome://resources 'self' 'unsafe-eval';");
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::TrustedTypes,
      "trusted-types jstemplate;");

  source->AddResourcePath("family_link_user_internals.js",
                          IDR_FAMILY_LINK_USER_INTERNALS_JS);
  source->AddResourcePath("family_link_user_internals.css",
                          IDR_FAMILY_LINK_USER_INTERNALS_CSS);
  source->SetDefaultResource(IDR_FAMILY_LINK_USER_INTERNALS_HTML);
  return source;
}

}  // namespace

FamilyLinkUserInternalsUI::FamilyLinkUserInternalsUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile,
                                CreateFamilyLinkUserInternalsHTMLSource());

  web_ui->AddMessageHandler(
      std::make_unique<FamilyLinkUserInternalsMessageHandler>());
}

FamilyLinkUserInternalsUI::~FamilyLinkUserInternalsUI() {}
