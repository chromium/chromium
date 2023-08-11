// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/profile_customization_ui.h"

#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/search/background/ntp_custom_background_service_factory.h"
#include "chrome/browser/signin/signin_features.h"
#include "chrome/browser/ui/webui/cr_components/theme_color_picker/theme_color_picker_handler.h"
#include "chrome/browser/ui/webui/customize_themes/chrome_customize_themes_handler.h"
#include "chrome/browser/ui/webui/signin/profile_customization_handler.h"
#include "chrome/browser/ui/webui/signin/signin_url_utils.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/signin_resources.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/webui/resource_path.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/resources/grit/webui_resources.h"

ProfileCustomizationUI::ProfileCustomizationUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui, /*enable_chrome_send=*/true),
      customize_themes_factory_receiver_(this) {
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUIProfileCustomizationHost);

  static constexpr webui::ResourcePath kResources[] = {
      {"profile_customization_app.js",
       IDR_SIGNIN_PROFILE_CUSTOMIZATION_PROFILE_CUSTOMIZATION_APP_JS},
      {"profile_customization_app.html.js",
       IDR_SIGNIN_PROFILE_CUSTOMIZATION_PROFILE_CUSTOMIZATION_APP_HTML_JS},
      {"profile_customization_browser_proxy.js",
       IDR_SIGNIN_PROFILE_CUSTOMIZATION_PROFILE_CUSTOMIZATION_BROWSER_PROXY_JS},
      {"images/profile_customization_illustration.svg",
       IDR_SIGNIN_PROFILE_CUSTOMIZATION_IMAGES_PROFILE_CUSTOMIZATION_ILLUSTRATION_SVG},
      {"images/profile_customization_illustration_dark.svg",
       IDR_SIGNIN_PROFILE_CUSTOMIZATION_IMAGES_PROFILE_CUSTOMIZATION_ILLUSTRATION_DARK_SVG},
      {"signin_shared.css.js", IDR_SIGNIN_SIGNIN_SHARED_CSS_JS},
      {"signin_vars.css.js", IDR_SIGNIN_SIGNIN_VARS_CSS_JS},
  };

  webui::SetupWebUIDataSource(
      source, base::make_span(kResources),
      IDR_SIGNIN_PROFILE_CUSTOMIZATION_PROFILE_CUSTOMIZATION_HTML);

  // Localized strings.
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"profileCustomizationDoneLabel",
       IDS_PROFILE_CUSTOMIZATION_DONE_BUTTON_LABEL},
      {"profileCustomizationSkipLabel",
       IDS_PROFILE_CUSTOMIZATION_SKIP_BUTTON_LABEL},
      {"profileCustomizationInputLabel", IDS_PROFILE_CUSTOMIZATION_INPUT_LABEL},
      {"profileCustomizationInputPlaceholder",
       IDS_PROFILE_CUSTOMIZATION_INPUT_PLACEHOLDER},
      {"profileCustomizationInputErrorMessage",
       IDS_PROFILE_CUSTOMIZATION_INPUT_ERROR_MESSAGE},
      {"profileCustomizationText", IDS_PROFILE_CUSTOMIZATION_TEXT},
      {"profileCustomizationTitle", IDS_PROFILE_CUSTOMIZATION_TITLE_V2},
      {"localProfileCreationTitle",
       IDS_PROFILE_CUSTOMIZATION_LOCAL_PROFILE_CREATION_TITLE},
      {"profileCustomizationDeleteProfileLabel",
       IDS_PROFILE_CUSTOMIZATION_DELETE_PROFILE_BUTTON_LABEL},
      {"profileCustomizationCustomizeAvatarLabel",
       IDS_PROFILE_PICKER_PROFILE_CREATION_FLOW_LOCAL_PROFILE_CREATION_CUSTOMIZE_AVATAR_BUTTON_LABEL},
      {"profileCustomizationAvatarSelectionTitle",
       IDS_PROFILE_PICKER_PROFILE_CREATION_FLOW_LOCAL_PROFILE_CREATION_AVATAR_TEXT},
      {"profileCustomizationAvatarSelectionBackButtonLabel",
       IDS_PROFILE_CUSTOMIZATION_AVATAR_SELECTION_BACK_BUTTON_LABEL},

      // Color picker strings:
      {"colorPickerLabel", IDS_NTP_CUSTOMIZE_COLOR_PICKER_LABEL},
      {"colorsContainerLabel", IDS_NTP_THEMES_CONTAINER_LABEL},
      {"currentTheme", IDS_NTP_CUSTOMIZE_CHROME_CURRENT_THEME_LABEL},
      {"defaultColorName", IDS_NTP_CUSTOMIZE_DEFAULT_LABEL},
      {"defaultThemeLabel", IDS_NTP_CUSTOMIZE_DEFAULT_LABEL},
      {"greyDefaultColorName", IDS_NTP_CUSTOMIZE_GREY_DEFAULT_LABEL},
      {"mainColorName", IDS_NTP_CUSTOMIZE_MAIN_COLOR_LABEL},
      {"managedColorsBody", IDS_NTP_THEME_MANAGED_DIALOG_BODY},
      {"managedColorsTitle", IDS_NTP_THEME_MANAGED_DIALOG_TITLE},
      {"themesContainerLabel",
       IDS_PROFILE_CUSTOMIZATION_THEMES_CONTAINER_LABEL},
      {"thirdPartyThemeDescription", IDS_NTP_CUSTOMIZE_3PT_THEME_DESC},
      {"uninstallThirdPartyThemeButton", IDS_NTP_CUSTOMIZE_3PT_THEME_UNINSTALL},
  };
  source->AddLocalizedStrings(kLocalizedStrings);

  // loadTimeData.
  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile->GetPath());
  source->AddString("profileName",
                    base::UTF16ToUTF8(entry->GetLocalProfileName()));
  const GURL& url = web_ui->GetWebContents()->GetVisibleURL();
  source->AddBoolean("isLocalProfileCreation",
                     GetProfileCustomizationStyle(url) ==
                         ProfileCustomizationStyle::kLocalProfileCreation);
  webui::SetupChromeRefresh2023(source);

  if (url.query() == "debug") {
    // Not intended to be hooked to anything. The bubble will not initialize it
    // so we force it here.
    Initialize(base::DoNothing());
  }
}

ProfileCustomizationUI::~ProfileCustomizationUI() = default;

void ProfileCustomizationUI::Initialize(
    base::OnceCallback<void(ProfileCustomizationHandler::CustomizationResult)>
        completion_callback) {
  std::unique_ptr<ProfileCustomizationHandler> handler =
      std::make_unique<ProfileCustomizationHandler>(
          Profile::FromWebUI(web_ui()), std::move(completion_callback));
  profile_customization_handler_ = handler.get();
  web_ui()->AddMessageHandler(std::move(handler));
}

void ProfileCustomizationUI::BindInterface(
    mojo::PendingReceiver<
        customize_themes::mojom::CustomizeThemesHandlerFactory>
        pending_receiver) {
  if (customize_themes_factory_receiver_.is_bound())
    customize_themes_factory_receiver_.reset();
  customize_themes_factory_receiver_.Bind(std::move(pending_receiver));
}

void ProfileCustomizationUI::BindInterface(
    mojo::PendingReceiver<
        theme_color_picker::mojom::ThemeColorPickerHandlerFactory>
        pending_receiver) {
  if (theme_color_picker_handler_factory_receiver_.is_bound()) {
    theme_color_picker_handler_factory_receiver_.reset();
  }
  theme_color_picker_handler_factory_receiver_.Bind(
      std::move(pending_receiver));
}

ProfileCustomizationHandler*
ProfileCustomizationUI::GetProfileCustomizationHandlerForTesting() {
  return profile_customization_handler_;
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

void ProfileCustomizationUI::CreateThemeColorPickerHandler(
    mojo::PendingReceiver<theme_color_picker::mojom::ThemeColorPickerHandler>
        handler,
    mojo::PendingRemote<theme_color_picker::mojom::ThemeColorPickerClient>
        client) {
  theme_color_picker_handler_ = std::make_unique<ThemeColorPickerHandler>(
      std::move(handler), std::move(client),
      NtpCustomBackgroundServiceFactory::GetForProfile(
          Profile::FromWebUI(web_ui())),
      web_ui()->GetWebContents());
}

WEB_UI_CONTROLLER_TYPE_IMPL(ProfileCustomizationUI)
