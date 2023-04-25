// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/enterprise_profile_welcome_ui.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_features.h"
#include "chrome/browser/ui/webui/signin/enterprise_profile_welcome_handler.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/signin_resources.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/webui/resource_path.h"
#include "ui/resources/grit/webui_resources.h"

EnterpriseProfileWelcomeUI::EnterpriseProfileWelcomeUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      Profile::FromWebUI(web_ui),
      chrome::kChromeUIEnterpriseProfileWelcomeHost);

  static constexpr webui::ResourcePath kResources[] = {
      {"enterprise_profile_welcome_app.js",
       IDR_SIGNIN_ENTERPRISE_PROFILE_WELCOME_ENTERPRISE_PROFILE_WELCOME_APP_JS},
      {"enterprise_profile_welcome_app.html.js",
       IDR_SIGNIN_ENTERPRISE_PROFILE_WELCOME_ENTERPRISE_PROFILE_WELCOME_APP_HTML_JS},
      {"enterprise_profile_welcome_browser_proxy.js",
       IDR_SIGNIN_ENTERPRISE_PROFILE_WELCOME_ENTERPRISE_PROFILE_WELCOME_BROWSER_PROXY_JS},
      {"signin_shared.css.js", IDR_SIGNIN_SIGNIN_SHARED_CSS_JS},
      {"signin_vars.css.js", IDR_SIGNIN_SIGNIN_VARS_CSS_JS},
      {"tangible_sync_style_shared.css.js",
       IDR_SIGNIN_TANGIBLE_SYNC_STYLE_SHARED_CSS_JS},
  };

  webui::SetupWebUIDataSource(
      source, base::make_span(kResources),
      IDR_SIGNIN_ENTERPRISE_PROFILE_WELCOME_ENTERPRISE_PROFILE_WELCOME_HTML);

  source->AddResourcePath(
      "images/enterprise_profile_welcome_illustration.svg",
      IDR_SIGNIN_ENTERPRISE_PROFILE_WELCOME_IMAGES_ENTERPRISE_PROFILE_WELCOME_ILLUSTRATION_SVG);
  source->AddResourcePath("images/left-banner.svg",
                          IDR_SIGNIN_IMAGES_SHARED_LEFT_BANNER_SVG);
  source->AddResourcePath("images/left-banner-dark.svg",
                          IDR_SIGNIN_IMAGES_SHARED_LEFT_BANNER_DARK_SVG);
  source->AddResourcePath("images/right-banner.svg",
                          IDR_SIGNIN_IMAGES_SHARED_RIGHT_BANNER_SVG);
  source->AddResourcePath("images/right-banner-dark.svg",
                          IDR_SIGNIN_IMAGES_SHARED_RIGHT_BANNER_DARK_SVG);
  source->AddResourcePath("images/tangible_sync_style_dialog_illustration.svg",
                          IDR_SIGNIN_IMAGES_SHARED_DIALOG_ILLUSTRATION_SVG);
  source->AddResourcePath(
      "images/tangible_sync_style_dialog_illustration_dark.svg",
      IDR_SIGNIN_IMAGES_SHARED_DIALOG_ILLUSTRATION_DARK_SVG);
  source->AddLocalizedString("enterpriseProfileWelcomeTitle",
                             IDS_ENTERPRISE_PROFILE_WELCOME_TITLE);
  source->AddLocalizedString("cancelLabel", IDS_CANCEL);
  source->AddLocalizedString("proceedAlternateLabel",
                             IDS_WELCOME_SIGNIN_VIEW_SIGNIN);
  source->AddLocalizedString("linkDataText",
                             IDS_ENTERPRISE_PROFILE_WELCOME_LINK_DATA_CHECKBOX);
  source->AddBoolean("showLinkDataCheckbox", false);
  source->AddBoolean("isModalDialog", false);
  source->AddBoolean(
      "isTangibleSyncStyleEnabled",
      base::FeatureList::IsEnabled(kEnterpriseWelcomeTangibleSyncStyle) &&
          base::FeatureList::IsEnabled(switches::kTangibleSync));
  webui::SetupChromeRefresh2023(source);
}

EnterpriseProfileWelcomeUI::~EnterpriseProfileWelcomeUI() = default;

void EnterpriseProfileWelcomeUI::Initialize(
    Browser* browser,
    EnterpriseProfileWelcomeUI::ScreenType type,
    const AccountInfo& account_info,
    bool profile_creation_required_by_policy,
    bool show_link_data_option,
    absl::optional<SkColor> profile_color,
    signin::SigninChoiceCallback proceed_callback) {
  auto handler = std::make_unique<EnterpriseProfileWelcomeHandler>(
      browser, type, profile_creation_required_by_policy, show_link_data_option,
      account_info, profile_color, std::move(proceed_callback));
  handler_ = handler.get();

  if (type ==
      EnterpriseProfileWelcomeUI::ScreenType::kEnterpriseAccountCreation) {
    base::Value::Dict update_data;
    update_data.Set("isModalDialog", true);

    int title_id = profile_creation_required_by_policy
                       ? IDS_ENTERPRISE_WELCOME_PROFILE_REQUIRED_TITLE
                       : IDS_ENTERPRISE_WELCOME_PROFILE_WILL_BE_MANAGED_TITLE;
    update_data.Set("enterpriseProfileWelcomeTitle",
                    l10n_util::GetStringUTF16(title_id));

    update_data.Set("showLinkDataCheckbox", show_link_data_option);

    content::WebUIDataSource::Update(
        Profile::FromWebUI(web_ui()),
        chrome::kChromeUIEnterpriseProfileWelcomeHost, std::move(update_data));
  }

  web_ui()->AddMessageHandler(std::move(handler));
}

EnterpriseProfileWelcomeHandler*
EnterpriseProfileWelcomeUI::GetHandlerForTesting() {
  return handler_;
}

WEB_UI_CONTROLLER_TYPE_IMPL(EnterpriseProfileWelcomeUI)
