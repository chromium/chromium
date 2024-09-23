// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_chrome_ui.h"

#include <optional>
#include <string>
#include <utility>

#include "base/rand_util.h"
#include "chrome/browser/bad_message.h"
#include "chrome/browser/image_fetcher/image_decoder_impl.h"
#include "chrome/browser/new_tab_page/modules/new_tab_page_modules.h"
#include "chrome/browser/new_tab_page/new_tab_page_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/background/ntp_custom_background_service_factory.h"
#include "chrome/browser/search/background/wallpaper_search/wallpaper_search_background_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/side_panel/customize_chrome/customize_chrome_utils.h"
#include "chrome/browser/ui/webui/cr_components/customize_color_scheme_mode/customize_color_scheme_mode_handler.h"
#include "chrome/browser/ui/webui/cr_components/theme_color_picker/theme_color_picker_handler.h"
#include "chrome/browser/ui/webui/new_tab_page/new_tab_page_ui.h"
#include "chrome/browser/ui/webui/sanitized_image_source.h"
#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_chrome_page_handler.h"
#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_chrome_section.h"
#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_toolbar/customize_toolbar_handler.h"
#include "chrome/browser/ui/webui/side_panel/customize_chrome/wallpaper_search/wallpaper_search_handler.h"
#include "chrome/browser/ui/webui/side_panel/customize_chrome/wallpaper_search/wallpaper_search_string_map.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/side_panel_customize_chrome_resources.h"
#include "chrome/grit/side_panel_customize_chrome_resources_map.h"
#include "chrome/grit/side_panel_shared_resources.h"
#include "chrome/grit/side_panel_shared_resources_map.h"
#include "components/search/ntp_features.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/webui/color_change_listener/color_change_handler.h"

namespace {

int64_t RandInt64() {
  int64_t number;
  base::RandBytes(base::byte_span_from_ref(number));
  return number;
}

}  // namespace

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
    : TopChromeWebUIController(web_ui),
      image_decoder_(std::make_unique<ImageDecoderImpl>()),
      profile_(Profile::FromWebUI(web_ui)),
      web_contents_(web_ui->GetWebContents()),
      module_id_names_(
          ntp::MakeModuleIdNames(NewTabPageUI::IsManagedProfile(profile_),
                                 profile_)),
      page_factory_receiver_(this),
      id_(RandInt64()) {
  const bool wallpaper_search_enabled =
      customize_chrome::IsWallpaperSearchEnabledForProfile(profile_);
  if (wallpaper_search_enabled) {
    wallpaper_search_background_manager_ =
        std::make_unique<WallpaperSearchBackgroundManager>(profile_);
    wallpaper_search_string_map_ = WallpaperSearchStringMap::Create();
  }
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
      {"toolbarHeader", IDS_NTP_CUSTOMIZE_MENU_TOOLBAR_LABEL},
      // Appearance strings.
      {"changeTheme", IDS_NTP_CUSTOMIZE_CHROME_CHANGE_THEME_LABEL},
      {"chromeWebStore", IDS_EXTENSION_WEB_STORE_TITLE},
      {"classicChrome", IDS_NTP_CUSTOMIZE_NO_BACKGROUND_LABEL},
      {"colorsContainerLabel", IDS_NTP_THEMES_CONTAINER_LABEL},
      {"colorPickerLabel", IDS_NTP_CUSTOMIZE_COLOR_PICKER_LABEL},
      {"currentTheme", IDS_NTP_CUSTOMIZE_CHROME_CURRENT_THEME_LABEL},
      {"defaultColorName", IDS_NTP_CUSTOMIZE_DEFAULT_LABEL},
      {"greyDefaultColorName", IDS_NTP_CUSTOMIZE_GREY_DEFAULT_LABEL},
      {"hueSliderTitle", IDS_NTP_CUSTOMIZE_COLOR_HUE_SLIDER_TITLE},
      {"hueSliderAriaLabel", IDS_NTP_CUSTOMIZE_COLOR_HUE_SLIDER_ARIA_LABEL},
      {"hueSliderDeleteTitle", IDS_NTP_CUSTOMIZE_COLOR_HUE_SLIDER_DELETE_TITLE},
      {"hueSliderDeleteA11yLabel",
       IDS_NTP_CUSTOMIZE_COLOR_HUE_SLIDER_DELETE_A11Y_LABEL},
      {"managedColorsTitle", IDS_NTP_THEME_MANAGED_DIALOG_TITLE},
      {"managedColorsBody", IDS_NTP_THEME_MANAGED_DIALOG_BODY},
      {"uploadImage", IDS_NTP_CUSTOM_BG_UPLOAD_AN_IMAGE},
      {"uploadedImage", IDS_NTP_CUSTOMIZE_UPLOADED_IMAGE_LABEL},
      {"yourUploadedImage", IDS_NTP_CUSTOMIZE_YOUR_UPLOADED_IMAGE_LABEL},
      {"yourSearchedImage", IDS_NTP_CUSTOMIZE_YOUR_SEARCHED_IMAGE_LABEL},
      {"resetToClassicChrome",
       IDS_NTP_CUSTOMIZE_CHROME_RESET_TO_CLASSIC_CHROME_LABEL},
      {"updatedToClassicChrome",
       IDS_NTP_CUSTOMIZE_CHROME_RESET_TO_CLASSIC_CHROME_COMPLETE},
      {"updatedToUploadedImage",
       IDS_NTP_CUSTOMIZE_CHROME_RESET_TO_UPLOADED_IMAGE_COMPLETE},
      {"followThemeToggle", IDS_NTP_CUSTOMIZE_CHROME_FOLLOW_THEME_LABEL},
      {"refreshDaily", IDS_NTP_CUSTOM_BG_DAILY_REFRESH},
      {"newTabPageManagedBy", IDS_NTP_CUSTOMIZE_CHROME_MANAGED_NEW_TAB_PAGE},
      {"newTabPageManagedByA11yLabel",
       IDS_NTP_CUSTOMIZE_CHROME_MANAGED_NEW_TAB_PAGE_ACCESSIBILITY},
      // Shortcut strings.
      {"mostVisited", IDS_NTP_CUSTOMIZE_MOST_VISITED_LABEL},
      {"myShortcuts", IDS_NTP_CUSTOMIZE_MY_SHORTCUTS_LABEL},
      {"shortcutsCurated", IDS_NTP_CUSTOMIZE_MY_SHORTCUTS_DESC},
      {"shortcutsSuggested", IDS_NTP_CUSTOMIZE_MOST_VISITED_DESC},
      {"showShortcutsToggle", IDS_NTP_CUSTOMIZE_SHOW_SHORTCUTS_LABEL},
      // Card strings.
      {"showCardsToggleTitle", IDS_NTP_CUSTOMIZE_SHOW_CARDS_LABEL},
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
      // Wallpaper search strings.
      {"colorRed", IDS_NTP_WALLPAPER_SEARCH_COLOR_RED_LABEL},
      {"colorBlue", IDS_NTP_WALLPAPER_SEARCH_COLOR_BLUE_LABEL},
      {"colorYellow", IDS_NTP_WALLPAPER_SEARCH_COLOR_YELLOW_LABEL},
      {"colorGreen", IDS_NTP_WALLPAPER_SEARCH_COLOR_GREEN_LABEL},
      {"colorBlack", IDS_NTP_WALLPAPER_SEARCH_COLOR_BLACK_LABEL},
      {"separator", IDS_NTP_WALLPAPER_SEARCH_SEPARATOR},
      {"genericErrorDescription",
       IDS_NTP_WALLPAPER_SEARCH_GENERIC_ERROR_DESCRIPTION},
      {"genericErrorDescriptionWithHistory",
       IDS_NTP_WALLPAPER_SEARCH_GENERIC_ERROR_DESCRIPTION_WITH_HISTORY},
      {"genericErrorTitle", IDS_NTP_WALLPAPER_SEARCH_GENERIC_ERROR_TITLE},
      {"offlineDescription", IDS_NTP_WALLPAPER_SEARCH_OFFLINE_DESCRIPTION},
      {"offlineDescriptionWithHistory",
       IDS_NTP_WALLPAPER_SEARCH_OFFLINE_DESCRIPTION_WITH_HISTORY},
      {"offlineTitle", IDS_NTP_WALLPAPER_SEARCH_OFFLINE_TITLE},
      {"optionalDetailsLabel", IDS_NTP_WALLPAPER_SEARCH_OPTIONAL_DETAILS_LABEL},
      {"requestThrottledDescription",
       IDS_NTP_WALLPAPER_SEARCH_REQUEST_THROTTLED_DESCRIPTION},
      {"requestThrottledTitle",
       IDS_NTP_WALLPAPER_SEARCH_REQUEST_THROTTLED_TITLE},
      {"tryAgain", IDS_NTP_WALLPAPER_SEARCH_TRY_AGAIN_CTA},
      {"wallpaperSearchHistoryHeader", IDS_NTP_WALLPAPER_SEARCH_HISTORY_HEADER},
      {"wallpaperSearchMoodLabel", IDS_NTP_WALLPAPER_SEARCH_MOOD_LABEL},
      {"wallpaperSearchMoodDefaultOptionLabel",
       IDS_NTP_WALLPAPER_SEARCH_MOOD_DEFAULT_OPTION_LABEL},
      {"wallpaperSearchStyleLabel", IDS_NTP_WALLPAPER_SEARCH_STYLE_LABEL},
      {"wallpaperSearchStyleDefaultOptionLabel",
       IDS_NTP_WALLPAPER_SEARCH_STYLE_DEFAULT_OPTION_LABEL},
      {"wallpaperSearchSubjectLabel", IDS_NTP_WALLPAPER_SEARCH_SUBJECT_LABEL},
      {"wallpaperSearchSubjectDefaultOptionLabel",
       IDS_NTP_WALLPAPER_SEARCH_SUBJECT_DEFAULT_OPTION_LABEL},
      {"wallpaperSearchSubmitBtn", IDS_NTP_WALLPAPER_SEARCH_SUBMIT_BTN_TEXT},
      {"wallpaperSearchResultLabel", IDS_NTP_WALLPAPER_SEARCH_RESULT_LABEL},
      {"wallpaperSearchResultLabelB",
       IDS_NTP_WALLPAPER_SEARCH_RESULT_LABEL_WITH_DESCRIPTOR_B},
      {"wallpaperSearchResultLabelC",
       IDS_NTP_WALLPAPER_SEARCH_RESULT_LABEL_WITH_DESCRIPTOR_C},
      {"wallpaperSearchResultLabelBC",
       IDS_NTP_WALLPAPER_SEARCH_RESULT_LABEL_WITH_DESCRIPTORS_B_AND_C},
      {"experimentalFeatureDisclaimer", IDS_NTP_WALLPAPER_SEARCH_DISCLAIMER},
      {"learnMore", IDS_LEARN_MORE},
      {"learnMoreAboutFeatureA11yLabel",
       IDS_LEARN_MORE_ABOUT_FEATURE_A11Y_LABEL},
      {"thumbsDown", IDS_THUMBS_DOWN_OPENS_FEEDBACK_FORM_A11Y_LABEL},
      {"thumbsUp", IDS_THUMBS_UP_RESULTS_A11Y_LABEL},
      {"wallpaperSearchPageHeader", IDS_NTP_WALLPAPER_SEARCH_PAGE_HEADER},
      {"wallpaperSearchTileLabel", IDS_NTP_WALLPAPER_SEARCH_TILE_LABEL},
      {"wallpaperSearchInspirationHeader",
       IDS_NTP_WALLPAPER_SEARCH_INSPIRATION_HEADER},
      {"wallpaperSearchLoadingA11yMessage",
       IDS_NTP_WALLPAPER_SEARCH_LOADING_A11Y_MESSAGE},
      {"wallpaperSearchSuccessA11yMessage",
       IDS_NTP_WALLPAPER_SEARCH_SUCCESS_A11Y_MESSAGE},
      {"wallpaperSearchHistoryResultLabelNoDescriptor",
       IDS_NTP_WALLPAPER_SEARCH_HISTORY_RESULT_LABEL_NO_DESCRIPTOR},
      {"wallpaperSearchHistoryResultLabel",
       IDS_NTP_WALLPAPER_SEARCH_HISTORY_RESULT_LABEL},
      {"wallpaperSearchHistoryResultLabelB",
       IDS_NTP_WALLPAPER_SEARCH_HISTORY_RESULT_LABEL_WITH_DESCRIPTOR_B},
      {"wallpaperSearchHistoryResultLabelC",
       IDS_NTP_WALLPAPER_SEARCH_HISTORY_RESULT_LABEL_WITH_DESCRIPTOR_C},
      {"wallpaperSearchHistoryResultLabelBC",
       IDS_NTP_WALLPAPER_SEARCH_HISTORY_RESULT_LABEL_WITH_DESCRIPTORS_B_AND_C},
      {"showInspirationCardToggle",
       IDS_NTP_WALLPAPER_SEARCH_INSPIRATION_CARD_TOGGLE_TITLE},
      {"genericErrorDescriptionWithInspiration",
       IDS_NTP_WALLPAPER_SEARCH_GENERIC_ERROR_DESCRIPTION_WITH_INSPIRATION},
      {"genericErrorDescriptionWithHistoryAndInspiration",
       IDS_NTP_WALLPAPER_SEARCH_GENERIC_ERROR_DESCRIPTION_WITH_HISTORY_AND_INSPIRATION},
      {"wallpaperSearchDescriptorsChangedA11yMessage",
       IDS_NTP_WALLPAPER_SEARCH_DESCRIPTORS_CHANGED_A11Y_MESSAGE},
      {"signedOutDescription", IDS_NTP_WALLPAPER_SEARCH_SIGNED_OUT_DESCRIPTION},
      {"signedOutTitle", IDS_NTP_WALLPAPER_SEARCH_SIGNED_OUT_TITLE},
      // Side Panel Extension Card.
      {"customizeWithChromeWebstoreLabel",
       IDS_NTP_CUSTOMIZE_CHROME_WEBSTORE_LABEL},
      {"webstoreShoppingCategoryLabel",
       IDS_NTP_WEBSTORE_SHOPPING_CATEOGRY_LABEL},
      {"webstoreWritingHelpCollectionLabel",
       IDS_NTP_WEBSTORE_WRITTING_HELP_COLLECTION_LABEL},
      {"webstoreProductivityCategoryLabel",
       IDS_NTP_WEBSTORE_PRODUCTIVITY_CATEOGRY_LABEL},
      // Customize Toolbar strings.
      {"toolbarButtonA11yLabel", IDS_NTP_CUSTOMIZE_TOOLBAR_BUTTON_A11Y_LABEL},
      {"chooseToolbarIconsLabel", IDS_NTP_CUSTOMIZE_TOOLBAR_CHOOSE_ICONS_LABEL},
      {"resetToDefaultButtonLabel",
       IDS_NTP_CUSTOMIZE_TOOLBAR_RESET_TO_DEFAULT_BUTTON_LABEL},
      {"resetToDefaultButtonAnnouncement",
       IDS_NTP_CUSTOMIZE_TOOLBAR_RESET_TO_DEFAULT_ANNOUNCEMENT},
      {"reorderTipLabel", IDS_NTP_CUSTOMIZE_TOOLBAR_REORDER_TIP_LABEL},
      {"newBadgeLabel", IDS_NEW_BADGE},
  };
  source->AddLocalizedStrings(kLocalizedStrings);

  source->AddBoolean(
      "modulesEnabled",
      ntp::HasModulesEnabled(module_id_names_,
                             IdentityManagerFactory::GetForProfile(profile_)));

  source->AddBoolean("showDeviceThemeToggle",
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN)
                     true);
#else
                     false);
#endif

  source->AddBoolean(
      "extensionsCardEnabled",
      base::FeatureList::IsEnabled(
          ntp_features::kCustomizeChromeSidePanelExtensionsCard));

  source->AddBoolean("wallpaperSearchEnabled", wallpaper_search_enabled);
  source->AddBoolean(
      "wallpaperSearchInspirationCardEnabled",
      wallpaper_search_enabled &&
          base::FeatureList::IsEnabled(
              ntp_features::kCustomizeChromeWallpaperSearchInspirationCard));
  source->AddBoolean(
      "wallpaperSearchButtonEnabled",
      wallpaper_search_enabled &&
          base::FeatureList::IsEnabled(
              ntp_features::kCustomizeChromeWallpaperSearchButton));
  source->AddBoolean("toolbarCustomizationEnabled",
                     base::FeatureList::IsEnabled(features::kToolbarPinning));
  source->AddBoolean("imageErrorDetectionEnabled",
                     base::FeatureList::IsEnabled(
                         ntp_features::kNtpBackgroundImageErrorDetection));

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

void CustomizeChromeUI::AttachedTabStateUpdated(
    bool is_source_tab_first_party_ntp) {
  if (customize_chrome_page_handler_) {
    customize_chrome_page_handler_->AttachedTabStateUpdated(
        is_source_tab_first_party_ntp);
  } else {
    is_source_tab_first_party_ntp_ = is_source_tab_first_party_ntp;
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
    mojo::PendingReceiver<
        side_panel::customize_chrome::mojom::CustomizeToolbarHandlerFactory>
        receiver) {
  if (customize_toolbar_handler_factory_receiver_.is_bound()) {
    customize_toolbar_handler_factory_receiver_.reset();
  }
  customize_toolbar_handler_factory_receiver_.Bind(std::move(receiver));
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

void CustomizeChromeUI::BindInterface(
    mojo::PendingReceiver<
        side_panel::customize_chrome::mojom::WallpaperSearchHandlerFactory>
        pending_receiver) {
  if (wallpaper_search_handler_factory_receiver_.is_bound()) {
    wallpaper_search_handler_factory_receiver_.reset();
  }
  wallpaper_search_handler_factory_receiver_.Bind(std::move(pending_receiver));
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
  if (is_source_tab_first_party_ntp_.has_value()) {
    customize_chrome_page_handler_->AttachedTabStateUpdated(
        is_source_tab_first_party_ntp_.value());
    is_source_tab_first_party_ntp_.reset();
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

void CustomizeChromeUI::CreateWallpaperSearchHandler(
    mojo::PendingRemote<
        side_panel::customize_chrome::mojom::WallpaperSearchClient> client,
    mojo::PendingReceiver<
        side_panel::customize_chrome::mojom::WallpaperSearchHandler> handler) {
  if (wallpaper_search_handler_) {
    mojo::ReportBadMessage("Only allowed to create one Mojo pipe per WebUI.");
    return;
  }
  CHECK(wallpaper_search_background_manager_);
  CHECK(wallpaper_search_string_map_);
  wallpaper_search_handler_ = std::make_unique<WallpaperSearchHandler>(
      std::move(handler), std::move(client), profile_, image_decoder_.get(),
      wallpaper_search_background_manager_.get(), id_,
      wallpaper_search_string_map_.get());
}

void CustomizeChromeUI::CreateCustomizeToolbarHandler(
    mojo::PendingRemote<
        side_panel::customize_chrome::mojom::CustomizeToolbarClient> client,
    mojo::PendingReceiver<
        side_panel::customize_chrome::mojom::CustomizeToolbarHandler> handler) {
  if (customize_toolbar_handler_) {
    mojo::ReportBadMessage("Only allowed to create one Mojo pipe per WebUI.");
    return;
  }

  const raw_ptr<Browser> browser =
      chrome::FindBrowserWithWindow(web_contents_->GetTopLevelNativeWindow());
  CHECK(browser);

  customize_toolbar_handler_ = std::make_unique<CustomizeToolbarHandler>(
      std::move(handler), std::move(client), browser);
}
