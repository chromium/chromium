// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/dice_web_signin_intercept_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/signin/dice_web_signin_intercept_handler.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/webui/resource_path.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/resources/grit/webui_generated_resources.h"
#include "ui/resources/grit/webui_resources.h"

DiceWebSigninInterceptUI::DiceWebSigninInterceptUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::Create(
      chrome::kChromeUIDiceWebSigninInterceptHost);
  source->SetDefaultResource(IDR_SIGNIN_DICE_WEB_INTERCEPT_HTML);

  static constexpr webui::ResourcePath kResources[] = {
      {"dice_web_signin_intercept_app.js",
       IDR_SIGNIN_DICE_WEB_INTERCEPT_APP_JS},
      {"dice_web_signin_intercept_browser_proxy.js",
       IDR_SIGNIN_DICE_WEB_INTERCEPT_BROWSER_PROXY_JS},
      {"signin_shared_css.js", IDR_SIGNIN_SHARED_CSS_JS},
      {"signin_vars_css.js", IDR_SIGNIN_VARS_CSS_JS},
      // Resources for testing.
      {"test_loader.js", IDR_WEBUI_JS_TEST_LOADER_JS},
      {"test_loader_util.js", IDR_WEBUI_JS_TEST_LOADER_UTIL_JS},
      {"test_loader.html", IDR_WEBUI_HTML_TEST_LOADER_HTML},
  };
  source->AddResourcePaths(kResources);

  source->AddLocalizedString("guestLink",
                             IDS_SIGNIN_DICE_WEB_INTERCEPT_BUBBLE_GUEST_LINK);
  source->UseStringsJs();

  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome://resources chrome://test 'self';");
  source->DisableTrustedTypesCSP();

  content::WebUIDataSource::Add(Profile::FromWebUI(web_ui), source);
}

DiceWebSigninInterceptUI::~DiceWebSigninInterceptUI() = default;

void DiceWebSigninInterceptUI::Initialize(
    const DiceWebSigninInterceptor::Delegate::BubbleParameters&
        bubble_parameters,
    base::OnceCallback<void(SigninInterceptionUserChoice)> callback) {
  web_ui()->AddMessageHandler(std::make_unique<DiceWebSigninInterceptHandler>(
      bubble_parameters, std::move(callback)));
}

WEB_UI_CONTROLLER_TYPE_IMPL(DiceWebSigninInterceptUI)
