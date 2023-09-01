// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/invalidations/invalidations_ui.h"

#include <memory>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/invalidations/invalidations_message_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/invalidations_resources.h"
#include "chrome/grit/invalidations_resources_map.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"

void CreateAndAddInvalidationsHTMLSource(Profile* profile) {
  // This is done once per opening of the page
  // This method does not fire when refreshing the page
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUIInvalidationsHost);
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome://resources chrome://webui-test 'self' "
      "'unsafe-eval';");
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::TrustedTypes,
      "trusted-types jstemplate webui-test-script;");
  source->AddResourcePaths(
      base::make_span(kInvalidationsResources, kInvalidationsResourcesSize));
  source->SetDefaultResource(IDR_INVALIDATIONS_ABOUT_INVALIDATIONS_HTML);
}

InvalidationsUI::InvalidationsUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  if (profile) {
    CreateAndAddInvalidationsHTMLSource(profile);
    web_ui->AddMessageHandler(std::make_unique<InvalidationsMessageHandler>());
  }
}

InvalidationsUI::~InvalidationsUI() {}
