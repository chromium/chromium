// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/profile_customization_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/signin/profile_customization_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/resources/grit/webui_resources.h"

ProfileCustomizationUI::ProfileCustomizationUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::Create(
      chrome::kChromeUIProfileCustomizationHost);
  source->SetDefaultResource(IDR_PROFILE_CUSTOMIZATION_HTML);
  source->AddResourcePath("profile_customization_app.js",
                          IDR_PROFILE_CUSTOMIZATION_APP_JS);

  // Localized strings.
  source->UseStringsJs();
  source->EnableReplaceI18nInJS();
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"profileCustomizationDoneLabel",
       IDS_PROFILE_CUSTOMIZATION_DONE_BUTTON_LABEL},
      {"profileCustomizationPickThemeTitle",
       IDS_PROFILE_CUSTOMIZATION_PICK_THEME_TITLE},
  };
  webui::AddLocalizedStringsBulk(source, kLocalizedStrings);

  // Resources for testing.
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome://resources chrome://test 'self';");
  source->DisableTrustedTypesCSP();
  source->AddResourcePath("test_loader.js", IDR_WEBUI_JS_TEST_LOADER);
  source->AddResourcePath("test_loader.html", IDR_WEBUI_HTML_TEST_LOADER);

  content::WebUIDataSource::Add(Profile::FromWebUI(web_ui), source);

  web_ui->AddMessageHandler(std::make_unique<ProfileCustomizationHandler>());
}

ProfileCustomizationUI::~ProfileCustomizationUI() = default;
