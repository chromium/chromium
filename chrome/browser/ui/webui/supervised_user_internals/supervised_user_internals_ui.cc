// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/supervised_user_internals/supervised_user_internals_ui.h"

#include <memory>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/supervised_user_internals/supervised_user_internals_message_handler.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/dev_ui_browser_resources.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"

namespace {

content::WebUIDataSource* CreateSupervisedUserInternalsHTMLSource() {
  content::WebUIDataSource* source = content::WebUIDataSource::Create(
      chrome::kChromeUISupervisedUserInternalsHost);
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome://resources 'self' 'unsafe-eval';");
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::TrustedTypes,
      "trusted-types jstemplate;");

  source->AddResourcePath("supervised_user_internals.js",
                          IDR_SUPERVISED_USER_INTERNALS_JS);
  source->AddResourcePath("supervised_user_internals.css",
                          IDR_SUPERVISED_USER_INTERNALS_CSS);
  source->SetDefaultResource(IDR_SUPERVISED_USER_INTERNALS_HTML);
  return source;
}

}  // namespace

SupervisedUserInternalsUI::SupervisedUserInternalsUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile,
                                CreateSupervisedUserInternalsHTMLSource());

  web_ui->AddMessageHandler(
      std::make_unique<SupervisedUserInternalsMessageHandler>());
}

SupervisedUserInternalsUI::~SupervisedUserInternalsUI() {}
