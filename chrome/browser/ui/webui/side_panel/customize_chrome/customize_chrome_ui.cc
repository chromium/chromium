// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_chrome_ui.h"

#include <string>
#include <utility>

#include "chrome/browser/cart/cart_handler.h"
#include "chrome/browser/new_tab_page/modules/new_tab_page_modules.h"
#include "chrome/browser/new_tab_page/new_tab_page_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/background/ntp_custom_background_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/webui/cr_components/customize_color_scheme_mode/customize_color_scheme_mode_handler.h"
#include "chrome/browser/ui/webui/cr_components/theme_color_picker/theme_color_picker_handler.h"
#include "chrome/browser/ui/webui/new_tab_page/new_tab_page_ui.h"
#include "chrome/browser/ui/webui/sanitized_image_source.h"
#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_chrome_page_handler.h"
#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_chrome_section.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/side_panel_customize_chrome_resources.h"
#include "chrome/grit/side_panel_customize_chrome_resources_map.h"
#include "chrome/grit/side_panel_shared_resources.h"
#include "chrome/grit/side_panel_shared_resources_map.h"
#include "components/manta/features.h"
#include "components/search/ntp_features.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/webui/color_change_listener/color_change_handler.h"

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(CustomizeChromeUI,
                                      kChangeChromeThemeButtonElementId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(CustomizeChromeUI,
                                      kChangeChromeThemeClassicElementId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(CustomizeChromeUI,
                                      kChromeThemeBackElementId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(CustomizeChromeUI,
                                      kChromeThemeCollectionElementId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(CustomizeChromeUI, kChromeThemeElementId);

CustomizeChromeUI::CustomizeChromeUI(content::WebUI* web_ui)
    : ui::MojoBubbleWebUIController(web_ui),
      profile_(Profile::FromWebUI(web_ui)),
      web_contents_(web_ui->GetWebContents()),
      module_id_names_(ntp::MakeModuleIdNames(
          NewTabPageUI::IsDriveModuleEnabledForProfile(profile_))),
      page_factory_receiver_(this) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile_, chrome::kChromeUICustomizeChromeSidePanelHost);

  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      // Side panel strings.
      {"backButton", IDS_ACCNAME_BACK},
      {"title", IDS_SIDE_PANEL_CUSTOMIZE_CHROME_TITLE},
      // Header strings.
      {"appearanceHeader", IDS_NTP_CUSTOMIZE_APPEARANCE_LABEL},
      {"cardsHeader", IDS_NTP_CUSTOMIZE_MENU_MODULES_LABEL},
      {"categoriesHeader", IDS_NTP_CUSTOMIZE_THEMES_HEADER},
      {"shortcutsHeader", IDS_NTP_CUSTOMIZE_MENU_SHORTCUTS_LABEL},
      // Appearance strings.
      {"changeTheme", IDS_NTP_CUSTOMIZE_CHROME_CHANGE_THEME_LABEL},
      {"chromeColors", IDS_NTP_CUSTOMIZE_CHROME_COLORS_LABEL},
      {"chromeWebStore", IDS_EXTENSION_WEB_STORE_TITLE},
      {"classicChrome", IDS_NTP_CUSTOMIZE_NO_BACKGROUND_LABEL},
      {"colorsContainerLabel", IDS_NTP_THEMES_CONTAINER_LABEL},
      {"colorPickerLabel", IDS_NTP_CUSTOMIZE_COLOR_PICKER_LABEL},
      {"currentTheme", IDS_NTP_CUSTOMIZE_CHROME_CURRENT_THEME_LABEL},
      {"defaultColorName", IDS_NTP_CUSTOMIZE_DEFAULT_LABEL},
      {"greyDefaultColorName", IDS_NTP_CUSTOMIZE_GREY_DEFAULT_LABEL},
      {"hueSliderTitle", IDS_NTP_CUSTOMIZE_COLOR_HUE_SLIDER_TITLE},
      {"mainColorName", IDS_NTP_CUSTOMIZE_MAIN_COLOR_LABEL},
      {"managedColorsTitle", IDS_NTP_THEME_MANAGED_DIALOG_TITLE},
      {"managedColorsBody", IDS_NTP_THEME_MANAGED_DIALOG_BODY},
      {"uploadImage", IDS_NTP_CUSTOM_BG_UPLOAD_AN_IMAGE},
      {"uploadedImage", IDS_NTP_CUSTOMIZE_UPLOADED_IMAGE_LABEL},
      {"yourUploadedImage", IDS_NTP_CUSTOMIZE_YOUR_UPLOADED_IMAGE_LABEL},
      {"resetToClassicChrome",
       IDS_NTP_CUSTOMIZE_CHROME_RESET_TO_CLASSIC_CHROME_LABEL},
      {"followThemeToggle", IDS_NTP_CUSTOMIZE_CHROME_FOLLOW_THEME_LABEL},
      {"refreshDaily", IDS_NTP_CUSTOM_BG_DAILY_REFRESH},
      // Shortcut strings.
      {"mostVisited", IDS_NTP_CUSTOMIZE_MOST_VISITED_LABEL},
      {"myShortcuts", IDS_NTP_CUSTOMIZE_MY_SHORTCUTS_LABEL},
      {"shortcutsCurated", IDS_NTP_CUSTOMIZE_MY_SHORTCUTS_DESC},
      {"shortcutsSuggested", IDS_NTP_CUSTOMIZE_MOST_VISITED_DESC},
      {"showShortcutsToggle", IDS_NTP_CUSTOMIZE_SHOW_SHORTCUTS_LABEL},
      // Card strings.
      {"showCardsToggleTitle", IDS_NTP_CUSTOMIZE_SHOW_CARDS_LABEL},
      {"modulesCartDiscountConsentAccept",
       IDS_NTP_MODULES_CART_DISCOUNT_CONSENT_ACCEPT},
      {"modulesCartSentence", IDS_NTP_MODULES_CART_SENTENCE},
      // Required by <managed-dialog>.
      {"controlledSettingPolicy", IDS_CONTROLLED_SETTING_POLICY},
      {"close", IDS_NEW_TAB_VOICE_CLOSE_TOOLTIP},
      {"ok", IDS_OK},
      // CustomizeColorSchemeMode strings.
      {"colorSchemeModeLabel",
       IDS_NTP_CUSTOMIZE_CHROME_COLOR_SCHEME_MODE_GROUP_LABEL},
      {"lightMode", IDS_NTP_CUSTOMIZE_CHROME_COLOR_SCHEME_MODE_LIGHT_LABEL},
      {"darkMode", IDS_NTP_CUSTOMIZE_CHROME_COLOR_SCHEME_MODE_DARK_LABEL},
      {"systemMode", IDS_NTP_CUSTOMIZE_CHROME_COLOR_SCHEME_MODE_SYSTEM_LABEL},
  };
  source->AddLocalizedStrings(kLocalizedStrings);

  source->AddBoolean(
      "modulesEnabled",
      ntp::HasModulesEnabled(module_id_names_,
                             IdentityManagerFactory::GetForProfile(profile_)));
  source->AddBoolean(
      "showCartInQuestModuleSetting",
      IsCartModuleEnabled() &&
          base::FeatureList::IsEnabled(
              ntp_features::kNtpChromeCartInHistoryClusterModule));

  source->AddBoolean("showDeviceThemeToggle",
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN)
                     features::IsChromeWebuiRefresh2023());
#else
                     false);
#endif

  source->AddBoolean(
      "extensionsCardEnabled",
      base::FeatureList::IsEnabled(
          ntp_features::kCustomizeChromeSidePanelExtensionsCard) &&
          features::IsChromeWebuiRefresh2023());

  source->AddBoolean("wallpaperSearchEnabled",
                     base::FeatureList::IsEnabled(
                         ntp_features::kCustomizeChromeWallpaperSearch) &&
                         manta::features::IsMantaServiceEnabled());

  webui::SetupChromeRefresh2023(source);

  webui::SetupWebUIDataSource(
      source,
      base::make_span(kSidePanelCustomizeChromeResources,
                      kSidePanelCustomizeChromeResourcesSize),
      IDR_SIDE_PANEL_CUSTOMIZE_CHROME_CUSTOMIZE_CHROME_HTML);
  source->AddResourcePaths(base::make_span(kSidePanelSharedResources,
                                           kSidePanelSharedResourcesSize));

  content::URLDataSource::Add(profile_,
                              std::make_unique<SanitizedImageSource>(profile_));
}

CustomizeChromeUI::~CustomizeChromeUI() = default;

void CustomizeChromeUI::ScrollToSection(CustomizeChromeSection section) {
  if (customize_chrome_page_handler_) {
    customize_chrome_page_handler_->ScrollToSection(section);
  } else {
    section_ = section;
  }
}

base::WeakPtr<CustomizeChromeUI> CustomizeChromeUI::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

WEB_UI_CONTROLLER_TYPE_IMPL(CustomizeChromeUI)

void CustomizeChromeUI::BindInterface(
    mojo::PendingReceiver<side_panel::mojom::CustomizeChromePageHandlerFactory>
        receiver) {
  if (page_factory_receiver_.is_bound()) {
    page_factory_receiver_.reset();
  }
  page_factory_receiver_.Bind(std::move(receiver));
}

void CustomizeChromeUI::BindInterface(
    mojo::PendingReceiver<chrome_cart::mojom::CartHandler>
        pending_page_handler) {
  cart_handler_ = std::make_unique<CartHandler>(std::move(pending_page_handler),
                                                profile_, web_contents_);
}

void CustomizeChromeUI::BindInterface(
    mojo::PendingReceiver<help_bubble::mojom::HelpBubbleHandlerFactory>
        pending_receiver) {
  if (help_bubble_handler_factory_receiver_.is_bound()) {
    help_bubble_handler_factory_receiver_.reset();
  }
  help_bubble_handler_factory_receiver_.Bind(std::move(pending_receiver));
}

void CustomizeChromeUI::BindInterface(
    mojo::PendingReceiver<customize_color_scheme_mode::mojom::
                              CustomizeColorSchemeModeHandlerFactory>
        pending_receiver) {
  if (customize_color_scheme_mode_handler_factory_receiver_.is_bound()) {
    customize_color_scheme_mode_handler_factory_receiver_.reset();
  }
  customize_color_scheme_mode_handler_factory_receiver_.Bind(
      std::move(pending_receiver));
}

void CustomizeChromeUI::BindInterface(
    mojo::PendingReceiver<
        theme_color_picker::mojom::ThemeColorPickerHandlerFactory>
        pending_receiver) {
  if (theme_color_picker_handler_factory_receiver_.is_bound()) {
    theme_color_picker_handler_factory_receiver_.reset();
  }
  theme_color_picker_handler_factory_receiver_.Bind(
      std::move(pending_receiver));
}

void CustomizeChromeUI::BindInterface(
    mojo::PendingReceiver<color_change_listener::mojom::PageHandler>
        pending_receiver) {
  color_provider_handler_ = std::make_unique<ui::ColorChangeHandler>(
      web_ui()->GetWebContents(), std::move(pending_receiver));
}

void CustomizeChromeUI::CreatePageHandler(
    mojo::PendingRemote<side_panel::mojom::CustomizeChromePage> pending_page,
    mojo::PendingReceiver<side_panel::mojom::CustomizeChromePageHandler>
        pending_page_handler) {
  DCHECK(pending_page.is_valid());
  customize_chrome_page_handler_ = std::make_unique<CustomizeChromePageHandler>(
      std::move(pending_page_handler), std::move(pending_page),
      NtpCustomBackgroundServiceFactory::GetForProfile(profile_), web_contents_,
      module_id_names_);
  if (section_.has_value()) {
    customize_chrome_page_handler_->ScrollToSection(*section_);
    section_.reset();
  }
}

void CustomizeChromeUI::CreateHelpBubbleHandler(
    mojo::PendingRemote<help_bubble::mojom::HelpBubbleClient> client,
    mojo::PendingReceiver<help_bubble::mojom::HelpBubbleHandler> handler) {
  help_bubble_handler_ = std::make_unique<user_education::HelpBubbleHandler>(
      std::move(handler), std::move(client), this,
      std::vector<ui::ElementIdentifier>{
          CustomizeChromeUI::kChangeChromeThemeButtonElementId,
          CustomizeChromeUI::kChangeChromeThemeClassicElementId,
          CustomizeChromeUI::kChromeThemeCollectionElementId,
          CustomizeChromeUI::kChromeThemeElementId,
          CustomizeChromeUI::kChromeThemeBackElementId});
}

void CustomizeChromeUI::CreateCustomizeColorSchemeModeHandler(
    mojo::PendingRemote<
        customize_color_scheme_mode::mojom::CustomizeColorSchemeModeClient>
        client,
    mojo::PendingReceiver<
        customize_color_scheme_mode::mojom::CustomizeColorSchemeModeHandler>
        handler) {
  customize_color_scheme_mode_handler_ =
      std::make_unique<CustomizeColorSchemeModeHandler>(
          std::move(client), std::move(handler), profile_);
}

void CustomizeChromeUI::CreateThemeColorPickerHandler(
    mojo::PendingReceiver<theme_color_picker::mojom::ThemeColorPickerHandler>
        handler,
    mojo::PendingRemote<theme_color_picker::mojom::ThemeColorPickerClient>
        client) {
  theme_color_picker_handler_ = std::make_unique<ThemeColorPickerHandler>(
      std::move(handler), std::move(client),
      NtpCustomBackgroundServiceFactory::GetForProfile(profile_),
      web_contents_);
}
