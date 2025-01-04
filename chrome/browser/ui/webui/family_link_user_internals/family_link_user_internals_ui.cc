// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/family_link_user_internals/family_link_user_internals_ui.h"

#include <memory>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/family_link_user_internals/family_link_user_internals_message_handler.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/family_link_user_internals_resources.h"
#include "chrome/grit/family_link_user_internals_resources_map.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/webui/webui_util.h"

namespace {

void CreateAndAddFamilyLinkUserInternalsHTMLSource(Profile* profile) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUIFamilyLinkUserInternalsHost);
  webui::SetupWebUIDataSource(
      source,
      base::span<const webui::ResourcePath>(kFamilyLinkUserInternalsResources),
      IDR_FAMILY_LINK_USER_INTERNALS_FAMILY_LINK_USER_INTERNALS_HTML);

  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome://resources 'self';");
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::TrustedTypes,
      "trusted-types lit-html-desktop;");
}

}  // namespace

FamilyLinkUserInternalsUI::FamilyLinkUserInternalsUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  CreateAndAddFamilyLinkUserInternalsHTMLSource(Profile::FromWebUI(web_ui));

  web_ui->AddMessageHandler(
      std::make_unique<FamilyLinkUserInternalsMessageHandler>());
}

FamilyLinkUserInternalsUI::~FamilyLinkUserInternalsUI() = default;
