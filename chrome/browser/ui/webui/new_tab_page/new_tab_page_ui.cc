// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_page/new_tab_page_ui.h"

#include <memory>
#include <utility>

#include "chrome/browser/buildflags.h"
#include "chrome/browser/cart/cart_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/drive/drive_handler.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/search/instant_service_factory.h"
#include "chrome/browser/search/task_module/task_module_handler.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/search/omnibox_mojo_utils.h"
#include "chrome/browser/ui/webui/customize_themes/chrome_customize_themes_handler.h"
#include "chrome/browser/ui/webui/favicon_source.h"
#include "chrome/browser/ui/webui/new_tab_page/new_tab_page_handler.h"
#include "chrome/browser/ui/webui/new_tab_page/promo_browser_command/promo_browser_command_handler.h"
#include "chrome/browser/ui/webui/new_tab_page/untrusted_source.h"
#include "chrome/browser/ui/webui/realbox/realbox_handler.h"
#include "chrome/browser/ui/webui/sanitized_image_source.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/new_tab_page_resources.h"
#include "chrome/grit/new_tab_page_resources_map.h"
#include "components/favicon_base/favicon_url_parser.h"
#include "components/google/core/common/google_util.h"
#include "components/prefs/pref_service.h"
#include "components/search/ntp_features.h"
#include "components/search_engines/template_url_service.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_ui_data_source.h"
#include "media/base/media_switches.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/resources/grit/webui_generated_resources.h"
#include "url/url_util.h"

#if !defined(OFFICIAL_BUILD)
#include "chrome/browser/ui/webui/new_tab_page/foo/foo_handler.h"
#endif

using content::BrowserContext;
using content::WebContents;

namespace {

content::WebUIDataSource* CreateNewTabPageUiHtmlSource(
    Profile* profile,
    const base::Time& navigation_start_time) {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUINewTabPageHost);

  ui::Accelerator undo_accelerator(ui::VKEY_Z, ui::EF_PLATFORM_ACCELERATOR);
  source->AddString("undoDescription", l10n_util::GetStringFUTF16(
                                           IDS_UNDO_DESCRIPTION,
                                           undo_accelerator.GetShortcutText()));
  source->AddString("googleBaseUrl",
                    GURL(TemplateURLServiceFactory::GetForProfile(profile)
                             ->search_terms_data()
                             .GoogleBaseURLValue())
                        .spec());
  source->AddDouble("navigationStartTime", navigation_start_time.ToJsTime());

  // Realbox.
  source->AddBoolean(
      "realboxMatchOmniboxTheme",
      base::FeatureList::IsEnabled(ntp_features::kRealboxMatchOmniboxTheme));
  source->AddString(
      "realboxDefaultIcon",
      base::FeatureList::IsEnabled(ntp_features::kRealboxUseGoogleGIcon)
          ? omnibox::kGoogleGIconResourceName
          : omnibox::kSearchIconResourceName);
  source->AddString("realboxHint", l10n_util::GetStringUTF8(
                                       IDS_GOOGLE_SEARCH_BOX_EMPTY_HINT_MD));

  source->AddBoolean(
      "handleMostVisitedNavigationExplicitly",
      base::FeatureList::IsEnabled(
          ntp_features::kNtpHandleMostVisitedNavigationExplicitly));

  source->AddBoolean(
      "iframeOneGoogleBarEnabled",
      base::FeatureList::IsEnabled(ntp_features::kIframeOneGoogleBar));
  source->AddBoolean(
      "oneGoogleBarModalOverlaysEnabled",
      base::FeatureList::IsEnabled(ntp_features::kOneGoogleBarModalOverlays));

  source->AddBoolean("shortcutsEnabled",
                     base::FeatureList::IsEnabled(ntp_features::kNtpShortcuts));
  source->AddBoolean("logoEnabled",
                     base::FeatureList::IsEnabled(ntp_features::kNtpLogo));
  source->AddBoolean(
      "middleSlotPromoEnabled",
      base::FeatureList::IsEnabled(ntp_features::kNtpMiddleSlotPromo));
  source->AddBoolean("modulesEnabled",
                     base::FeatureList::IsEnabled(ntp_features::kModules));
  source->AddBoolean("modulesLoadEnabled", base::FeatureList::IsEnabled(
                                               ntp_features::kNtpModulesLoad));
  source->AddInteger("modulesLoadTimeout",
                     ntp_features::GetModulesLoadTimeout().InMilliseconds());

  static constexpr webui::LocalizedString kStrings[] = {
      {"doneButton", IDS_DONE},
      {"title", IDS_NEW_TAB_TITLE},
      {"undo", IDS_NEW_TAB_UNDO_THUMBNAIL_REMOVE},
      {"controlledSettingPolicy", IDS_CONTROLLED_SETTING_POLICY},

      // Custom Links.
      {"addLinkTitle", IDS_NTP_CUSTOM_LINKS_ADD_SHORTCUT_TITLE},
      {"editLinkTitle", IDS_NTP_CUSTOM_LINKS_EDIT_SHORTCUT},
      {"invalidUrl", IDS_NTP_CUSTOM_LINKS_INVALID_URL},
      {"linkAddedMsg", IDS_NTP_CONFIRM_MSG_SHORTCUT_ADDED},
      {"linkCancel", IDS_NTP_CUSTOM_LINKS_CANCEL},
      {"linkCantCreate", IDS_NTP_CUSTOM_LINKS_CANT_CREATE},
      {"linkCantEdit", IDS_NTP_CUSTOM_LINKS_CANT_EDIT},
      {"linkDone", IDS_NTP_CUSTOM_LINKS_DONE},
      {"linkEditedMsg", IDS_NTP_CONFIRM_MSG_SHORTCUT_EDITED},
      {"linkRemove", IDS_NTP_CUSTOM_LINKS_REMOVE},
      {"linkRemovedMsg", IDS_NTP_CONFIRM_MSG_SHORTCUT_REMOVED},
      {"moreActions", IDS_SETTINGS_MORE_ACTIONS},
      {"nameField", IDS_NTP_CUSTOM_LINKS_NAME},
      {"restoreDefaultLinks", IDS_NTP_CONFIRM_MSG_RESTORE_DEFAULTS},
      {"restoreThumbnailsShort", IDS_NEW_TAB_RESTORE_THUMBNAILS_SHORT_LINK},
      {"shortcutAlreadyExists", IDS_NTP_CUSTOM_LINKS_ALREADY_EXISTS},
      {"urlField", IDS_NTP_CUSTOM_LINKS_URL},

      // Customize button and dialog.
      {"backButton", IDS_ACCNAME_BACK},
      {"backgroundsMenuItem", IDS_NTP_CUSTOMIZE_MENU_BACKGROUND_LABEL},
      {"cancelButton", IDS_CANCEL},
      {"colorPickerLabel", IDS_NTP_CUSTOMIZE_COLOR_PICKER_LABEL},
      {"customBackgroundDisabled",
       IDS_NTP_CUSTOMIZE_MENU_BACKGROUND_DISABLED_LABEL},
      {"customizeButton", IDS_NTP_CUSTOMIZE_BUTTON_LABEL},
      {"customizeThisPage", IDS_NTP_CUSTOM_BG_CUSTOMIZE_NTP_LABEL},
      {"defaultThemeLabel", IDS_NTP_CUSTOMIZE_DEFAULT_LABEL},
      {"hideShortcuts", IDS_NTP_CUSTOMIZE_HIDE_SHORTCUTS_LABEL},
      {"hideShortcutsDesc", IDS_NTP_CUSTOMIZE_HIDE_SHORTCUTS_DESC},
      {"hideAllCards", IDS_NTP_CUSTOMIZE_HIDE_ALL_CARDS_LABEL},
      {"customizeCards", IDS_NTP_CUSTOMIZE_CUSTOMIZE_CARDS_LABEL},
      {"mostVisited", IDS_NTP_CUSTOMIZE_MOST_VISITED_LABEL},
      {"myShortcuts", IDS_NTP_CUSTOMIZE_MY_SHORTCUTS_LABEL},
      {"noBackground", IDS_NTP_CUSTOMIZE_NO_BACKGROUND_LABEL},
      {"refreshDaily", IDS_NTP_CUSTOM_BG_DAILY_REFRESH},
      {"shortcutsCurated", IDS_NTP_CUSTOMIZE_MY_SHORTCUTS_DESC},
      {"shortcutsMenuItem", IDS_NTP_CUSTOMIZE_MENU_SHORTCUTS_LABEL},
      {"modulesMenuItem", IDS_NTP_CUSTOMIZE_MENU_MODULES_LABEL},
      {"shortcutsOption", IDS_NTP_CUSTOMIZE_MENU_SHORTCUTS_LABEL},
      {"shortcutsSuggested", IDS_NTP_CUSTOMIZE_MOST_VISITED_DESC},
      {"themesMenuItem", IDS_NTP_CUSTOMIZE_MENU_COLOR_LABEL},
      {"thirdPartyThemeDescription", IDS_NTP_CUSTOMIZE_3PT_THEME_DESC},
      {"uninstallThirdPartyThemeButton", IDS_NTP_CUSTOMIZE_3PT_THEME_UNINSTALL},
      {"uploadFromDevice", IDS_NTP_CUSTOMIZE_UPLOAD_FROM_DEVICE_LABEL},

      // Voice search.
      {"audioError", IDS_NEW_TAB_VOICE_AUDIO_ERROR},
      {"close", IDS_NEW_TAB_VOICE_CLOSE_TOOLTIP},
      {"details", IDS_NEW_TAB_VOICE_DETAILS},
      {"languageError", IDS_NEW_TAB_VOICE_LANGUAGE_ERROR},
      {"learnMore", IDS_LEARN_MORE},
      {"listening", IDS_NEW_TAB_VOICE_LISTENING},
      {"networkError", IDS_NEW_TAB_VOICE_NETWORK_ERROR},
      {"noTranslation", IDS_NEW_TAB_VOICE_NO_TRANSLATION},
      {"noVoice", IDS_NEW_TAB_VOICE_NO_VOICE},
      {"otherError", IDS_NEW_TAB_VOICE_OTHER_ERROR},
      {"permissionError", IDS_NEW_TAB_VOICE_PERMISSION_ERROR},
      {"speak", IDS_NEW_TAB_VOICE_READY},
      {"tryAgain", IDS_NEW_TAB_VOICE_TRY_AGAIN},
      {"voiceSearchButtonLabel", IDS_TOOLTIP_MIC_SEARCH},
      {"waiting", IDS_NEW_TAB_VOICE_WAITING},

      // Realbox.
      {"searchBoxHint", IDS_GOOGLE_SEARCH_BOX_EMPTY_HINT_MD},
      {"realboxSeparator", IDS_AUTOCOMPLETE_MATCH_DESCRIPTION_SEPARATOR},
      {"removeSuggestion", IDS_OMNIBOX_REMOVE_SUGGESTION},
      {"removeSuggestionA11ySuffix", IDS_ACC_REMOVE_SUGGESTION_SUFFIX},
      {"removeSuggestionA11yPrefix", IDS_ACC_REMOVE_SUGGESTION_FOCUSED_PREFIX},
      {"hideSuggestions", IDS_TOOLTIP_HEADER_HIDE_SUGGESTIONS_BUTTON},
      {"showSuggestions", IDS_TOOLTIP_HEADER_SHOW_SUGGESTIONS_BUTTON},
      {"hideSection", IDS_ACC_HEADER_HIDE_SUGGESTIONS_BUTTON},
      {"showSection", IDS_ACC_HEADER_SHOW_SUGGESTIONS_BUTTON},

      // Logo/doodle.
      {"copyLink", IDS_NTP_DOODLE_SHARE_DIALOG_COPY_LABEL},
      {"doodleLink", IDS_NTP_DOODLE_SHARE_DIALOG_LINK_LABEL},
      {"email", IDS_NTP_DOODLE_SHARE_DIALOG_MAIL_LABEL},
      {"facebook", IDS_NTP_DOODLE_SHARE_DIALOG_FACEBOOK_LABEL},
      {"shareDoodle", IDS_NTP_DOODLE_SHARE_LABEL},
      {"twitter", IDS_NTP_DOODLE_SHARE_DIALOG_TWITTER_LABEL},

      // Theme.
      {"themeCreatedBy", IDS_NEW_TAB_ATTRIBUTION_INTRO},
      {"themeManagedDialogTitle", IDS_NTP_THEME_MANAGED_DIALOG_TITLE},
      {"themeManagedDialogBody", IDS_NTP_THEME_MANAGED_DIALOG_BODY},
      {"ok", IDS_OK},

      // Modules.
      {"dismissModuleToastMessage", IDS_NTP_MODULES_DISMISS_TOAST_MESSAGE},
      {"disableModuleToastMessage", IDS_NTP_MODULES_DISABLE_TOAST_MESSAGE},
      {"moduleInfoButtonTitle", IDS_NTP_MODULES_INFO_BUTTON_TITLE},
      {"modulesDismissButtonText", IDS_NTP_MODULES_DISMISS_BUTTON_TEXT},
      {"modulesDisableButtonText", IDS_NTP_MODULES_DISABLE_BUTTON_TEXT},
      {"modulesCustomizeButtonText", IDS_NTP_MODULES_CUSTOMIZE_BUTTON_TEXT},
      {"modulesShoppingTasksSentence", IDS_NTP_MODULES_SHOPPING_TASKS_SENTENCE},
      {"modulesShoppingTasksLower", IDS_NTP_MODULES_SHOPPING_TASKS_LOWER},
      {"modulesRecipeTasksSentence", IDS_NTP_MODULES_RECIPE_TASKS_SENTENCE},
      {"modulesRecipeTasksLower", IDS_NTP_MODULES_RECIPE_TASKS_LOWER},
      {"modulesRecipeTasksLowerThese",
       IDS_NTP_MODULES_RECIPE_TASKS_LOWER_THESE},
      {"modulesCartSentence", IDS_NTP_MODULES_CART_SENTENCE},
      {"modulesCartLower", IDS_NTP_MODULES_CART_LOWER},
      {"modulesCartLowerThese", IDS_NTP_MODULES_CART_LOWER_THESE},
      {"modulesCartLowerYour", IDS_NTP_MODULES_CART_LOWER_YOUR},
      {"modulesDriveSentence", IDS_NTP_MODULES_DRIVE_SENTENCE},
      {"modulesDummyLower", IDS_NTP_MODULES_DUMMY_LOWER},
      {"modulesDriveTitle", IDS_NTP_MODULES_DRIVE_TITLE},
      {"modulesDummyTitle", IDS_NTP_MODULES_DUMMY_TITLE},
      {"modulesDummy2Title", IDS_NTP_MODULES_DUMMY2_TITLE},
      {"modulesKaleidoscopeTitle", IDS_NTP_MODULES_KALEIDOSCOPE_TITLE},
      {"modulesTasksInfoTitle", IDS_NTP_MODULES_SHOPPING_TASKS_INFO_TITLE},
      {"modulesTasksInfoClose", IDS_NTP_MODULES_SHOPPING_TASKS_INFO_CLOSE},
      {"modulesCartHeaderNew", IDS_NTP_MODULES_CART_HEADER_CHIP_NEW},
      {"modulesCartWarmWelcome", IDS_NTP_MODULES_CART_WARM_WELCOME},
      {"modulesCartModuleMenuHideToastMessage",
       IDS_NTP_MODULES_CART_MODULE_MENU_HIDE_TOAST_MESSAGE},
      {"modulesCartCartMenuHideMerchant",
       IDS_NTP_MODULES_CART_CART_MENU_HIDE_MERCHANT},
      {"modulesCartCartMenuHideMerchantToastMessage",
       IDS_NTP_MODULES_CART_CART_MENU_HIDE_MERCHANT_TOAST_MESSAGE},
      {"modulesCartCartMenuRemoveMerchant",
       IDS_NTP_MODULES_CART_CART_MENU_REMOVE_MERCHANT},
      {"modulesCartCartMenuRemoveMerchantToastMessage",
       IDS_NTP_MODULES_CART_CART_MENU_REMOVE_MERCHANT_TOAST_MESSAGE},
      {"modulesCartDiscountChipAmount",
       IDS_NTP_MODULES_CART_DISCOUNT_CHIP_AMOUNT},
      {"modulesCartDiscountChipUpToAmount",
       IDS_NTP_MODULES_CART_DISCOUNT_CHIP_UP_TO_AMOUNT},
      {"modulesCartDiscountConsentContent",
       IDS_NTP_MODULES_CART_DISCOUNT_CONSENT_CONTENT},
      {"modulesCartDiscountConsentAccept",
       IDS_NTP_MODULES_CART_DISCOUNT_CONSENT_ACCEPT},
      {"modulesCartDiscountConsentAcceptConfirmation",
       IDS_NTP_MODULES_CART_DISCOUNT_CONSENT_ACCEPT_CONFIRMATION},
      {"modulesCartDiscountConsentReject",
       IDS_NTP_MODULES_CART_DISCOUNT_CONSENT_REJECT},
      {"modulesCartDiscountConsentRejectConfirmation",
       IDS_NTP_MODULES_CART_DISCOUNT_CONSENT_REJECT_CONFIRMATION},
      {"modulesCartDiscountConsentConfirmationDismiss",
       IDS_NTP_MODULES_CART_DISCOUNT_CONSENT_CONFIRMATION_DISMISS},
  };
  source->AddLocalizedStrings(kStrings);

  source->AddString(
      "modulesTasksInfo1",
      l10n_util::GetStringFUTF16(IDS_NTP_MODULES_SHOPPING_TASKS_INFO_1,
                                 u"https://myactivity.google.com/"));
  source->AddString(
      "modulesTasksInfo2",
      l10n_util::GetStringFUTF16(IDS_NTP_MODULES_SHOPPING_TASKS_INFO_2,
                                 u"https://policies.google.com/"));

  // Register images that are purposefully not inlined in the HTML and instead
  // are set in Javascript.
  static constexpr webui::ResourcePath kImages[] = {
      {omnibox::kGoogleGIconResourceName,
       IDR_WEBUI_IMAGES_200_LOGO_GOOGLEG_PNG},
      {omnibox::kCalculatorIconResourceName, IDR_LOCAL_NTP_ICONS_CALCULATOR},
      {omnibox::kClockIconResourceName, IDR_LOCAL_NTP_ICONS_CLOCK},
      {omnibox::kDriveDocsIconResourceName, IDR_LOCAL_NTP_ICONS_DRIVE_DOCS},
      {omnibox::kDriveFolderIconResourceName, IDR_LOCAL_NTP_ICONS_DRIVE_FOLDER},
      {omnibox::kDriveFormIconResourceName, IDR_LOCAL_NTP_ICONS_DRIVE_FORM},
      {omnibox::kDriveImageIconResourceName, IDR_LOCAL_NTP_ICONS_DRIVE_IMAGE},
      {omnibox::kDriveLogoIconResourceName, IDR_LOCAL_NTP_ICONS_DRIVE_LOGO},
      {omnibox::kDrivePdfIconResourceName, IDR_LOCAL_NTP_ICONS_DRIVE_PDF},
      {omnibox::kDriveSheetsIconResourceName, IDR_LOCAL_NTP_ICONS_DRIVE_SHEETS},
      {omnibox::kDriveSlidesIconResourceName, IDR_LOCAL_NTP_ICONS_DRIVE_SLIDES},
      {omnibox::kDriveVideoIconResourceName, IDR_LOCAL_NTP_ICONS_DRIVE_VIDEO},
      {omnibox::kExtensionAppIconResourceName,
       IDR_LOCAL_NTP_ICONS_EXTENSION_APP},
      {omnibox::kPageIconResourceName, IDR_LOCAL_NTP_ICONS_PAGE},
      {omnibox::kSearchIconResourceName, IDR_WEBUI_IMAGES_ICON_SEARCH_SVG},
      {omnibox::kTrendingUpIconResourceName, IDR_LOCAL_NTP_ICONS_TRENDING_UP}};
  source->AddResourcePaths(kImages);

  source->AddBoolean(
      "recipeTasksModuleEnabled",
      base::FeatureList::IsEnabled(ntp_features::kNtpRecipeTasksModule));
  source->AddBoolean(
      "shoppingTasksModuleEnabled",
      base::FeatureList::IsEnabled(ntp_features::kNtpShoppingTasksModule));
  source->AddBoolean(
      "chromeCartModuleEnabled",
      base::FeatureList::IsEnabled(ntp_features::kNtpChromeCartModule));
  source->AddBoolean("driveModuleEnabled", base::FeatureList::IsEnabled(
                                               ntp_features::kNtpDriveModule));
  source->AddBoolean(
      "ruleBasedDiscountEnabled",
      base::GetFieldTrialParamValueByFeature(
          ntp_features::kNtpChromeCartModule,
          ntp_features::kNtpChromeCartModuleAbandonedCartDiscountParam) ==
          "true");

  webui::SetupWebUIDataSource(
      source, base::make_span(kNewTabPageResources, kNewTabPageResourcesSize),
      IDR_NEW_TAB_PAGE_NEW_TAB_PAGE_HTML);

  // Allows creating <script> and inlining as well as network requests to
  // support inlining the OneGoogleBar.
  // TODO(crbug.com/1076506): remove when changing to iframed OneGoogleBar.
  // Needs to happen after |webui::SetupWebUIDataSource()| since also overrides
  // script-src.
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome://resources chrome://test "
      "'self' 'unsafe-inline' https:;");
  // Allow embedding of iframes from the One Google Bar and
  // chrome-untrusted://new-tab-page for other external content and resources.
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ChildSrc,
      base::StringPrintf("child-src https: %s %s;",
                         google_util::CommandLineGoogleBaseURL().spec().c_str(),
                         chrome::kChromeUIUntrustedNewTabPageUrl));

  return source;
}

}  // namespace

NewTabPageUI::NewTabPageUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui, /*enable_chrome_send=*/false),
      content::WebContentsObserver(web_ui->GetWebContents()),
      page_factory_receiver_(this),
      customize_themes_factory_receiver_(this),
      profile_(Profile::FromWebUI(web_ui)),
      instant_service_(InstantServiceFactory::GetForProfile(profile_)),
      web_contents_(web_ui->GetWebContents()),
      // We initialize navigation_start_time_ to a reasonable value to account
      // for the unlikely case where the NewTabPageHandler is created before we
      // received the DidStartNavigation event.
      navigation_start_time_(base::Time::Now()) {
  auto* source = CreateNewTabPageUiHtmlSource(profile_, navigation_start_time_);
  source->AddBoolean("customBackgroundDisabledByPolicy",
                     instant_service_->IsCustomBackgroundDisabledByPolicy());
  source->AddBoolean(
      "modulesVisibleManagedByPolicy",
      profile_->GetPrefs()->IsManagedPreference(prefs::kNtpModulesVisible));
  content::WebUIDataSource::Add(profile_, source);

  content::URLDataSource::Add(profile_,
                              std::make_unique<SanitizedImageSource>(profile_));
  content::URLDataSource::Add(
      profile_, std::make_unique<FaviconSource>(
                    profile_, chrome::FaviconUrlFormat::kFavicon2));
  content::URLDataSource::Add(profile_,
                              std::make_unique<UntrustedSource>(profile_));
  content::URLDataSource::Add(
      profile_,
      std::make_unique<ThemeSource>(profile_, /*serve_untrusted=*/true));

  web_ui->AddRequestableScheme(content::kChromeUIUntrustedScheme);

  instant_service_->AddObserver(this);
  instant_service_->UpdateNtpTheme();
}

WEB_UI_CONTROLLER_TYPE_IMPL(NewTabPageUI)

NewTabPageUI::~NewTabPageUI() {
  instant_service_->RemoveObserver(this);
}

// static
bool NewTabPageUI::IsNewTabPageOrigin(const GURL& url) {
  return url.GetOrigin() == GURL(chrome::kChromeUINewTabPageURL).GetOrigin();
}

void NewTabPageUI::BindInterface(
    mojo::PendingReceiver<new_tab_page::mojom::PageHandlerFactory>
        pending_receiver) {
  if (page_factory_receiver_.is_bound()) {
    page_factory_receiver_.reset();
  }

  page_factory_receiver_.Bind(std::move(pending_receiver));
}

void NewTabPageUI::BindInterface(
    mojo::PendingReceiver<realbox::mojom::PageHandler> pending_page_handler) {
  realbox_handler_ = std::make_unique<RealboxHandler>(
      std::move(pending_page_handler), profile_, web_contents_);
}

void NewTabPageUI::BindInterface(
    mojo::PendingReceiver<promo_browser_command::mojom::CommandHandler>
        pending_page_handler) {
  promo_browser_command_handler_ = std::make_unique<PromoBrowserCommandHandler>(
      std::move(pending_page_handler), profile_);
}

void NewTabPageUI::BindInterface(
    mojo::PendingReceiver<
        customize_themes::mojom::CustomizeThemesHandlerFactory>
        pending_receiver) {
  if (customize_themes_factory_receiver_.is_bound()) {
    customize_themes_factory_receiver_.reset();
  }
  customize_themes_factory_receiver_.Bind(std::move(pending_receiver));
}

void NewTabPageUI::BindInterface(
    mojo::PendingReceiver<task_module::mojom::TaskModuleHandler>
        pending_receiver) {
  task_module_handler_ = std::make_unique<TaskModuleHandler>(
      std::move(pending_receiver), profile_);
}

void NewTabPageUI::BindInterface(
    mojo::PendingReceiver<drive::mojom::DriveHandler> pending_receiver) {
  drive_handler_ =
      std::make_unique<DriveHandler>(std::move(pending_receiver), profile_);
}

#if !defined(OFFICIAL_BUILD)
void NewTabPageUI::BindInterface(
    mojo::PendingReceiver<foo::mojom::FooHandler> pending_page_handler) {
  foo_handler_ = std::make_unique<FooHandler>(std::move(pending_page_handler));
}
#endif

void NewTabPageUI::BindInterface(
    mojo::PendingReceiver<chrome_cart::mojom::CartHandler>
        pending_page_handler) {
  cart_handler_ =
      std::make_unique<CartHandler>(std::move(pending_page_handler), profile_);
}

void NewTabPageUI::CreatePageHandler(
    mojo::PendingRemote<new_tab_page::mojom::Page> pending_page,
    mojo::PendingReceiver<new_tab_page::mojom::PageHandler>
        pending_page_handler) {
  DCHECK(pending_page.is_valid());
  page_handler_ = std::make_unique<NewTabPageHandler>(
      std::move(pending_page_handler), std::move(pending_page), profile_,
      instant_service_, web_contents_,
      navigation_start_time_);
}

void NewTabPageUI::CreateCustomizeThemesHandler(
    mojo::PendingRemote<customize_themes::mojom::CustomizeThemesClient>
        pending_client,
    mojo::PendingReceiver<customize_themes::mojom::CustomizeThemesHandler>
        pending_handler) {
  customize_themes_handler_ = std::make_unique<ChromeCustomizeThemesHandler>(
      std::move(pending_client), std::move(pending_handler), web_contents_,
      profile_);
}

void NewTabPageUI::NtpThemeChanged(const NtpTheme& theme) {
  // Load time data is cached across page reloads. Update the background color
  // here to prevent a white flicker on page reload.
  UpdateBackgroundColor(theme);
}

void NewTabPageUI::MostVisitedInfoChanged(const InstantMostVisitedInfo& info) {}

void NewTabPageUI::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->IsInMainFrame()) {
    navigation_start_time_ = base::Time::Now();
    std::unique_ptr<base::DictionaryValue> update(new base::DictionaryValue);
    update->SetDouble("navigationStartTime", navigation_start_time_.ToJsTime());
    content::WebUIDataSource::Update(profile_, chrome::kChromeUINewTabPageHost,
                                     std::move(update));
  }
}

void NewTabPageUI::UpdateBackgroundColor(const NtpTheme& theme) {
  std::unique_ptr<base::DictionaryValue> update(new base::DictionaryValue);
  auto background_color = theme.background_color;
  update->SetString(
      "backgroundColor",
      base::StringPrintf("#%02X%02X%02X", SkColorGetR(background_color),
                         SkColorGetG(background_color),
                         SkColorGetB(background_color)));
  url::RawCanonOutputT<char> encoded_url;
  url::EncodeURIComponent(theme.custom_background_url.spec().c_str(),
                          theme.custom_background_url.spec().size(),
                          &encoded_url);
  update->SetString("backgroundImageUrl",
                    std::string(encoded_url.data(), encoded_url.length()));
  content::WebUIDataSource::Update(profile_, chrome::kChromeUINewTabPageHost,
                                   std::move(update));
}
