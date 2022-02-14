// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/enterprise_profile_welcome_ui.h"

#include "base/callback_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/signin/enterprise_profile_welcome_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/signin_resources.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/resource_path.h"
#include "ui/resources/grit/webui_generated_resources.h"

EnterpriseProfileWelcomeUI::EnterpriseProfileWelcomeUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::Create(
      chrome::kChromeUIEnterpriseProfileWelcomeHost);
  webui::SetJSModuleDefaults(source);

  source->SetDefaultResource(
      IDR_SIGNIN_ENTERPRISE_PROFILE_WELCOME_ENTERPRISE_PROFILE_WELCOME_HTML);
  static constexpr webui::ResourcePath kResources[] = {
      {"enterprise_profile_welcome_app.js",
       IDR_SIGNIN_ENTERPRISE_PROFILE_WELCOME_ENTERPRISE_PROFILE_WELCOME_APP_JS},
      {"enterprise_profile_welcome_browser_proxy.js",
       IDR_SIGNIN_ENTERPRISE_PROFILE_WELCOME_ENTERPRISE_PROFILE_WELCOME_BROWSER_PROXY_JS},
      {"images/enterprise_profile_welcome_illustration.svg",
       IDR_SIGNIN_ENTERPRISE_PROFILE_WELCOME_IMAGES_ENTERPRISE_PROFILE_WELCOME_ILLUSTRATION_SVG},
      {"signin_shared_css.js", IDR_SIGNIN_SIGNIN_SHARED_CSS_JS},
      {"signin_vars_css.js", IDR_SIGNIN_SIGNIN_VARS_CSS_JS},
  };
  source->AddResourcePaths(kResources);

  source->AddLocalizedString("enterpriseProfileWelcomeTitle",
                             IDS_ENTERPRISE_PROFILE_WELCOME_TITLE);
  source->AddLocalizedString("cancelLabel", IDS_CANCEL);
  source->AddBoolean("isModalDialog", false);

  content::WebUIDataSource::Add(Profile::FromWebUI(web_ui), source);
}

EnterpriseProfileWelcomeUI::~EnterpriseProfileWelcomeUI() = default;

void EnterpriseProfileWelcomeUI::Initialize(
    Browser* browser,
    EnterpriseProfileWelcomeUI::ScreenType type,
    const AccountInfo& account_info,
    absl::optional<SkColor> profile_color,
    base::OnceCallback<void(bool)> proceed_callback) {
  auto handler = std::make_unique<EnterpriseProfileWelcomeHandler>(
      browser, type, account_info, profile_color, std::move(proceed_callback));
  handler_ = handler.get();

  if (type ==
      EnterpriseProfileWelcomeUI::ScreenType::kEnterpriseAccountCreation) {
    base::DictionaryValue update_data;
    update_data.SetBoolKey("isModalDialog", true);
    update_data.SetStringKey(
        "enterpriseProfileWelcomeTitle",
        l10n_util::GetStringUTF16(
            IDS_ENTERPRISE_WELCOME_PROFILE_REQUIRED_TITLE));
    content::WebUIDataSource::Update(
        Profile::FromWebUI(web_ui()),
        chrome::kChromeUIEnterpriseProfileWelcomeHost,
        update_data.CreateDeepCopy());
  }

  web_ui()->AddMessageHandler(std::move(handler));
}

EnterpriseProfileWelcomeHandler*
EnterpriseProfileWelcomeUI::GetHandlerForTesting() {
  return handler_;
}

WEB_UI_CONTROLLER_TYPE_IMPL(EnterpriseProfileWelcomeUI)
