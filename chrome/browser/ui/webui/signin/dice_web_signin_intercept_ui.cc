// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/dice_web_signin_intercept_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/ui/webui/signin/dice_web_signin_intercept_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/signin_resources.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/webui/resource_path.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/resources/grit/webui_resources.h"
#include "url/gurl.h"

namespace {

// Helper to create parameters used for testing, when loading the intercept
// bubble directly with the `debug` query param set.
WebSigninInterceptor::Delegate::BubbleParameters
CreateSampleBubbleParameters() {
  // Looks like the transparent checkerboard.
  std::string small_png =
      "data:image/"
      "png;base64,iVBORw0KGgoAAAANSUhEUgAAAAgAAAAIAQMAAAD+wSzIAAAABlBMVEX///"
      "+/v7+jQ3Y5AAAADklEQVQI12P4AIX8EAgALgAD/aNpbtEAAAAASUVORK5CYII";

  AccountInfo intercepted_account;
  intercepted_account.account_id = CoreAccountId::FromGaiaId("intercepted_ID");
  intercepted_account.given_name = "Sam";
  intercepted_account.full_name = "Sam Sample";
  intercepted_account.email = "sam.sample@intercepted.com";
  intercepted_account.picture_url = small_png;
  intercepted_account.hosted_domain = kNoHostedDomainFound;

  AccountInfo primary_account;
  primary_account.account_id = CoreAccountId::FromGaiaId("primary_ID");
  primary_account.given_name = "Tessa";
  primary_account.full_name = "Tessa Tester";
  primary_account.email = "tessa.tester@primary.com";
  primary_account.picture_url = small_png;
  primary_account.hosted_domain = kNoHostedDomainFound;

  return WebSigninInterceptor::Delegate::BubbleParameters(
      WebSigninInterceptor::SigninInterceptionType::kMultiUser,
      intercepted_account, primary_account, SK_ColorMAGENTA);
}

}  // namespace

DiceWebSigninInterceptUI::DiceWebSigninInterceptUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      Profile::FromWebUI(web_ui), chrome::kChromeUIDiceWebSigninInterceptHost);
  source->SetDefaultResource(
      IDR_SIGNIN_DICE_WEB_SIGNIN_INTERCEPT_DICE_WEB_SIGNIN_INTERCEPT_HTML);

  static constexpr webui::ResourcePath kResources[] = {
      {"dice_web_signin_intercept_app.js",
       IDR_SIGNIN_DICE_WEB_SIGNIN_INTERCEPT_DICE_WEB_SIGNIN_INTERCEPT_APP_JS},
      {"dice_web_signin_intercept_app.css.js",
       IDR_SIGNIN_DICE_WEB_SIGNIN_INTERCEPT_DICE_WEB_SIGNIN_INTERCEPT_APP_CSS_JS},
      {"dice_web_signin_intercept_app.html.js",
       IDR_SIGNIN_DICE_WEB_SIGNIN_INTERCEPT_DICE_WEB_SIGNIN_INTERCEPT_APP_HTML_JS},
      {"dice_web_signin_intercept_browser_proxy.js",
       IDR_SIGNIN_DICE_WEB_SIGNIN_INTERCEPT_DICE_WEB_SIGNIN_INTERCEPT_BROWSER_PROXY_JS},
      {"signin_shared.css.js", IDR_SIGNIN_SIGNIN_SHARED_CSS_JS},
      {"signin_vars.css.js", IDR_SIGNIN_SIGNIN_VARS_CSS_JS},
      {"images/split_header.svg",
       IDR_SIGNIN_DICE_WEB_SIGNIN_INTERCEPT_IMAGES_SPLIT_HEADER_SVG},
      // Resources for testing.
      {"test_loader.js", IDR_WEBUI_JS_TEST_LOADER_JS},
      {"test_loader_util.js", IDR_WEBUI_JS_TEST_LOADER_UTIL_JS},
      {"test_loader.html", IDR_WEBUI_TEST_LOADER_HTML},
      // Resources for the Chrome signin sub page: /chrome_signin.
      {chrome::kChromeUIDiceWebSigninInterceptChromeSigninSubPage,
       IDR_SIGNIN_DICE_WEB_SIGNIN_INTERCEPT_CHROME_SIGNIN_CHROME_SIGNIN_HTML},
      {"chrome_signin/chrome_signin_app.js",
       IDR_SIGNIN_DICE_WEB_SIGNIN_INTERCEPT_CHROME_SIGNIN_CHROME_SIGNIN_APP_JS},
      {"chrome_signin/chrome_signin_app.css.js",
       IDR_SIGNIN_DICE_WEB_SIGNIN_INTERCEPT_CHROME_SIGNIN_CHROME_SIGNIN_APP_CSS_JS},
      {"chrome_signin/chrome_signin_app.html.js",
       IDR_SIGNIN_DICE_WEB_SIGNIN_INTERCEPT_CHROME_SIGNIN_CHROME_SIGNIN_APP_HTML_JS},
  };
  source->AddResourcePaths(kResources);

  // Adding localized strings for the Chrome Signin sub page: /chrome_signin.
  source->AddLocalizedString(
      "chromeSigninAcceptText",
      IDS_SIGNIN_DICE_WEB_INTERCEPT_BUBBLE_CHROME_SIGNIN_ACCEPT_TEXT);
  source->AddLocalizedString(
      "chromeSigninDeclineText",
      IDS_SIGNIN_DICE_WEB_INTERCEPT_BUBBLE_CHROME_SIGNIN_DECLINE_TEXT);
  source->AddLocalizedString("acceptButtonAriaLabel",
                             IDS_SIGNIN_CONTINUE_AS_BUTTON_ACCESSIBILITY_LABEL);

  source->UseStringsJs();
  source->EnableReplaceI18nInJS();

  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome://resources chrome://webui-test 'self';");
  webui::EnableTrustedTypesCSP(source);

  if (web_ui->GetWebContents()->GetVisibleURL().query() == "debug") {
    // Not intended to be hooked to anything. The bubble will not initialize it
    // so we force it here.
    Initialize(CreateSampleBubbleParameters(), base::DoNothing(),
               base::DoNothing());
  }
}

DiceWebSigninInterceptUI::~DiceWebSigninInterceptUI() = default;

void DiceWebSigninInterceptUI::Initialize(
    const WebSigninInterceptor::Delegate::BubbleParameters& bubble_parameters,
    base::OnceCallback<void(int)> show_widget_with_height_callback,
    base::OnceCallback<void(SigninInterceptionUserChoice)>
        completion_callback) {
  web_ui()->AddMessageHandler(std::make_unique<DiceWebSigninInterceptHandler>(
      bubble_parameters, std::move(show_widget_with_height_callback),
      std::move(completion_callback)));
}

WEB_UI_CONTROLLER_TYPE_IMPL(DiceWebSigninInterceptUI)
