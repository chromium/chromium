// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/profile_customization_ui.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/customize_themes/chrome_customize_themes_handler.h"
#include "chrome/browser/ui/webui/signin/profile_customization_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/base/webui/resource_path.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/resources/grit/webui_generated_resources.h"
#include "ui/resources/grit/webui_resources.h"

ProfileCustomizationUI::ProfileCustomizationUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui, /*enable_chrome_send=*/true),
      customize_themes_factory_receiver_(this) {
  content::WebUIDataSource* source = content::WebUIDataSource::Create(
      chrome::kChromeUIProfileCustomizationHost);
  webui::SetJSModuleDefaults(source);
  source->SetDefaultResource(IDR_PROFILE_CUSTOMIZATION_HTML);
  static constexpr webui::ResourcePath kResources[] = {
      {"profile_customization_app.js", IDR_PROFILE_CUSTOMIZATION_APP_JS},
      {"profile_customization_browser_proxy.js",
       IDR_PROFILE_CUSTOMIZATION_BROWSER_PROXY_JS},
      {"signin_shared_css.js", IDR_SIGNIN_SHARED_CSS_JS},
      {"signin_vars_css.js", IDR_SIGNIN_VARS_CSS_JS},
  };
  source->AddResourcePaths(kResources);

  // Localized strings.
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"profileCustomizationDoneLabel",
       IDS_PROFILE_CUSTOMIZATION_DONE_BUTTON_LABEL},
      {"profileCustomizationInputLabel", IDS_PROFILE_CUSTOMIZATION_INPUT_LABEL},
      {"profileCustomizationText", IDS_PROFILE_CUSTOMIZATION_TEXT},

      // Color picker strings:
      {"colorPickerLabel", IDS_NTP_CUSTOMIZE_COLOR_PICKER_LABEL},
      {"defaultThemeLabel", IDS_NTP_CUSTOMIZE_DEFAULT_LABEL},
      {"thirdPartyThemeDescription", IDS_NTP_CUSTOMIZE_3PT_THEME_DESC},
      {"uninstallThirdPartyThemeButton", IDS_NTP_CUSTOMIZE_3PT_THEME_UNINSTALL},
  };
  source->AddLocalizedStrings(kLocalizedStrings);

  // loadTimeData.
  Profile* profile = Profile::FromWebUI(web_ui);
  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile->GetPath());
  source->AddString("profileName",
                    base::UTF16ToUTF8(entry->GetLocalProfileName()));

  content::WebUIDataSource::Add(profile, source);
}

ProfileCustomizationUI::~ProfileCustomizationUI() = default;

void ProfileCustomizationUI::Initialize(base::OnceClosure done_closure) {
  web_ui()->AddMessageHandler(
      std::make_unique<ProfileCustomizationHandler>(std::move(done_closure)));
}

void ProfileCustomizationUI::BindInterface(
    mojo::PendingReceiver<
        customize_themes::mojom::CustomizeThemesHandlerFactory>
        pending_receiver) {
  if (customize_themes_factory_receiver_.is_bound())
    customize_themes_factory_receiver_.reset();
  customize_themes_factory_receiver_.Bind(std::move(pending_receiver));
}

void ProfileCustomizationUI::CreateCustomizeThemesHandler(
    mojo::PendingRemote<customize_themes::mojom::CustomizeThemesClient>
        pending_client,
    mojo::PendingReceiver<customize_themes::mojom::CustomizeThemesHandler>
        pending_handler) {
  customize_themes_handler_ = std::make_unique<ChromeCustomizeThemesHandler>(
      std::move(pending_client), std::move(pending_handler),
      web_ui()->GetWebContents(), Profile::FromWebUI(web_ui()));
}

WEB_UI_CONTROLLER_TYPE_IMPL(ProfileCustomizationUI)
