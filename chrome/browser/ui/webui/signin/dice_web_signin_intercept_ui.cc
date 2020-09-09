// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/dice_web_signin_intercept_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/ui/webui/signin/dice_web_signin_intercept_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/resources/grit/webui_resources.h"

DiceWebSigninInterceptUI::DiceWebSigninInterceptUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::Create(
      chrome::kChromeUIDiceWebSigninInterceptHost);
  source->SetDefaultResource(IDR_SIGNIN_DICE_WEB_INTERCEPT_HTML);
  source->AddResourcePath("dice_web_signin_intercept_app.js",
                          IDR_SIGNIN_DICE_WEB_INTERCEPT_APP_JS);
  source->AddResourcePath("dice_web_signin_intercept_browser_proxy.js",
                          IDR_SIGNIN_DICE_WEB_INTERCEPT_BROWSER_PROXY_JS);
  source->AddResourcePath("signin_icons.js", IDR_SIGNIN_ICONS_JS);
  source->AddResourcePath("signin_shared_css.js", IDR_SIGNIN_SHARED_CSS_JS);
  source->AddResourcePath("signin_vars_css.js", IDR_SIGNIN_VARS_CSS_JS);

  // Localized strings.
  source->UseStringsJs();
  source->EnableReplaceI18nInJS();
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"diceWebSigninInterceptAcceptLabel",
       IDS_SIGNIN_DICE_WEB_INTERCEPT_BUBBLE_NEW_PROFILE_BUTTON_LABEL},
      {"diceWebSigninInterceptCancelLabel",
       IDS_SIGNIN_DICE_WEB_INTERCEPT_BUBBLE_CANCEL_BUTTON_LABEL},
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
}

DiceWebSigninInterceptUI::~DiceWebSigninInterceptUI() = default;

void DiceWebSigninInterceptUI::Initialize(
    const DiceWebSigninInterceptor::Delegate::BubbleParameters&
        bubble_parameters,
    base::OnceCallback<void(bool)> callback) {
  web_ui()->AddMessageHandler(std::make_unique<DiceWebSigninInterceptHandler>(
      bubble_parameters, std::move(callback)));
}

WEB_UI_CONTROLLER_TYPE_IMPL(DiceWebSigninInterceptUI)
