// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_page/new_tab_page_ui.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/buildflags.h"
#include "chrome/browser/cart/cart_handler.h"
#include "chrome/browser/new_tab_page/modules/drive/drive_handler.h"
#include "chrome/browser/new_tab_page/modules/photos/photos_handler.h"
#include "chrome/browser/new_tab_page/modules/task_module/task_module_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/background/ntp_custom_background_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/search_provider_logos/logo_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/webui/browser_command/browser_command_handler.h"
#include "chrome/browser/ui/webui/cr_components/most_visited/most_visited_handler.h"
#include "chrome/browser/ui/webui/customize_themes/chrome_customize_themes_handler.h"
#include "chrome/browser/ui/webui/favicon_source.h"
#include "chrome/browser/ui/webui/new_tab_page/new_tab_page_handler.h"
#include "chrome/browser/ui/webui/new_tab_page/ntp_pref_names.h"
#include "chrome/browser/ui/webui/new_tab_page/untrusted_source.h"
#include "chrome/browser/ui/webui/realbox/realbox_handler.h"
#include "chrome/browser/ui/webui/sanitized_image_source.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/search/instant_types.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/new_tab_page_resources.h"
#include "chrome/grit/new_tab_page_resources_map.h"
#include "chrome/grit/theme_resources.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/favicon_base/favicon_url_parser.h"
#include "components/google/core/common/google_util.h"
#include "components/grit/components_scaled_resources.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/search/ntp_features.h"
#include "components/search_engines/template_url_service.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/driver/sync_service.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_ui_data_source.h"
#include "google_apis/gaia/core_account_id.h"
#include "media/base/media_switches.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "skia/ext/skia_utils_base.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/native_theme/native_theme.h"
#include "ui/resources/grit/webui_generated_resources.h"
#include "url/url_util.h"

#if !defined(OFFICIAL_BUILD)
#include "chrome/browser/ui/webui/new_tab_page/foo/foo_handler.h"
#endif

using content::BrowserContext;
using content::WebContents;

namespace {

constexpr char kPrevNavigationTimePrefName[] = "NewTabPage.PrevNavigationTime";
constexpr char kSignedOutNtpModulesSwitch[] = "signed-out-ntp-modules";

void AddRawStringOrDefault(content::WebUIDataSource* source,
                           const char key[],
                           const std::string str,
                           int default_string_id) {
  if (str.empty()) {
    source->AddLocalizedString(key, default_string_id);
  } else {
    source->AddString(key, str);
  }
}

// The Discount Consent V2 is gated by Chrome Cart, and that is enabled for
// en-us local only. So using plain en strings here is fine.
void AddResourcesForCartDiscountConsentV2(content::WebUIDataSource* source) {
  AddRawStringOrDefault(
      source, "modulesCartDiscountConsentContent",
      commerce::kNtpChromeCartModuleDiscountConsentStringChangeContent.Get(),
      IDS_NTP_MODULES_CART_DISCOUNT_CONSENT_CONTENT_V2);

  source->AddBoolean(
      "modulesCartConsentStepTwoDifferentColor",
      commerce::kNtpChromeCartModuleDiscountConsentInlineStepTwoDifferentColor
          .Get());

  AddRawStringOrDefault(
      source, "modulesCartDiscountConentTitle",
      commerce::kNtpChromeCartModuleDiscountConsentNtpDialogContentTitle.Get(),
      IDS_NTP_MODULES_CART_DISCOUNT_CONSENT_TITLE);

  source->AddBoolean(
      "modulesCartStepOneUseStaticContent",
      commerce::kNtpChromeCartModuleDiscountConsentNtpStepOneUseStaticContent
          .Get());
  // This does not have a raw string resource.
  source->AddString(
      "modulesCartStepOneStaticContent",
      commerce::kNtpChromeCartModuleDiscountConsentNtpStepOneStaticContent
          .Get());

  AddRawStringOrDefault(
      source, "modulesCartConsentStepOneOneMerchantContent",
      commerce::kNtpChromeCartModuleDiscountConsentNtpStepOneContentOneCart
          .Get(),
      IDS_NTP_MODULES_CART_DISCOUNT_CONSENT_STEP_1_WITH_MERCHANT_NAME);
  AddRawStringOrDefault(
      source, "modulesCartConsentStepOneTwoMerchantsContent",
      commerce::kNtpChromeCartModuleDiscountConsentNtpStepOneContentTwoCarts
          .Get(),
      IDS_NTP_MODULES_CART_DISCOUNT_CONSENT_STEP_1_WITH_TWO_MERCHANT_NAMES);
  AddRawStringOrDefault(
      source, "modulesCartConsentStepOneThreeMerchantsContent",
      commerce::kNtpChromeCartModuleDiscountConsentNtpStepOneContentThreeCarts
          .Get(),
      IDS_NTP_MODULES_CART_DISCOUNT_CONSENT_STEP_1_WITH_THREE_MERCHANT_NAMES);
  AddRawStringOrDefault(
      source, "modulesCartConsentStepTwoContent",
      commerce::kNtpChromeCartModuleDiscountConsentNtpStepTwoContent.Get(),
      IDS_NTP_MODULES_CART_DISCOUNT_CONSENT_CONTENT_V3);

  source->AddLocalizedString(
      "modulesCartConsentStepOneButton",
      IDS_NTP_MODULES_CART_DISCOUNT_CONSENT_STEP_1_CONTINUE);

  source->AddBoolean(
      "modulesCartDiscountInlineCardShowCloseButton",
      commerce::kNtpChromeCartModuleDiscountConsentInlineShowCloseButton.Get());
}

content::WebUIDataSource* CreateNewTabPageUiHtmlSource(Profile* profile) {
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

  source->AddBoolean(
      "handleMostVisitedNavigationExplicitly",
      base::FeatureList::IsEnabled(
          ntp_features::kNtpHandleMostVisitedNavigationExplicitly));

  source->AddBoolean(
      "oneGoogleBarEnabled",
      base::FeatureList::IsEnabled(ntp_features::kNtpOneGoogleBar));
  source->AddBoolean("shortcutsEnabled",
                     base::FeatureList::IsEnabled(ntp_features::kNtpShortcuts));
  source->AddBoolean("logoEnabled",
                     base::FeatureList::IsEnabled(ntp_features::kNtpLogo));
  source->AddBoolean(
      "middleSlotPromoEnabled",
      base::FeatureList::IsEnabled(ntp_features::kNtpMiddleSlotPromo) &&
          profile->GetPrefs()->GetBoolean(prefs::kNtpPromoVisible));
  source->AddBoolean(
      "modulesDragAndDropEnabled",
      base::FeatureList::IsEnabled(ntp_features::kNtpModulesDragAndDrop));
  source->AddBoolean("modulesFirstRunExperienceEnabled",
                     base::FeatureList::IsEnabled(
                         ntp_features::kNtpModulesFirstRunExperience));
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
      {"modulesRecipeInfo", IDS_NTP_MODULES_RECIPE_INFO},
      {"modulesRecipeTasksSentence", IDS_NTP_MODULES_RECIPE_TASKS_SENTENCE},
      {"modulesRecipeTasksLower", IDS_NTP_MODULES_RECIPE_TASKS_LOWER},
      {"modulesRecipeTasksLowerThese",
       IDS_NTP_MODULES_RECIPE_TASKS_LOWER_THESE},
      {"modulesRecipeViewedTasksSentence",
       IDS_NTP_MODULES_RECIPE_VIEWED_TASKS_SENTENCE},
      {"modulesRecipeViewedTasksLower",
       IDS_NTP_MODULES_RECIPE_VIEWED_TASKS_LOWER},
      {"modulesRecipeViewedTasksLowerThese",
       IDS_NTP_MODULES_RECIPE_VIEWED_TASKS_LOWER_THESE},
      {"modulesTasksInfo", IDS_NTP_MODULES_TASKS_INFO},
      {"modulesCartInfo", IDS_NTP_MODULES_CART_INFO},
      {"modulesCartSentence", IDS_NTP_MODULES_CART_SENTENCE},
      {"modulesCartSentenceV2", IDS_NTP_MODULES_CART_SENTENCE_V2},
      {"modulesCartLower", IDS_NTP_MODULES_CART_LOWER},
      {"modulesCartLowerThese", IDS_NTP_MODULES_CART_LOWER_THESE},
      {"modulesCartLowerYour", IDS_NTP_MODULES_CART_LOWER_YOUR},
      {"modulesDriveSentence", IDS_NTP_MODULES_DRIVE_SENTENCE},
      {"modulesDriveSentence2", IDS_NTP_MODULES_DRIVE_SENTENCE2},
      {"modulesDriveFilesSentence", IDS_NTP_MODULES_DRIVE_FILES_SENTENCE},
      {"modulesDriveFilesLower", IDS_NTP_MODULES_DRIVE_FILES_LOWER},
      {"modulesDummyLower", IDS_NTP_MODULES_DUMMY_LOWER},
      {"modulesDriveTitle", IDS_NTP_MODULES_DRIVE_TITLE},
      {"modulesDriveInfo", IDS_NTP_MODULES_DRIVE_INFO},
      {"modulesDummyTitle", IDS_NTP_MODULES_DUMMY_TITLE},
      {"modulesDummy2Title", IDS_NTP_MODULES_DUMMY2_TITLE},
      {"modulesDummy3Title", IDS_NTP_MODULES_DUMMY2_TITLE},
      {"modulesDummy4Title", IDS_NTP_MODULES_DUMMY2_TITLE},
      {"modulesDummy5Title", IDS_NTP_MODULES_DUMMY2_TITLE},
      {"modulesDummy6Title", IDS_NTP_MODULES_DUMMY2_TITLE},
      {"modulesDummy7Title", IDS_NTP_MODULES_DUMMY2_TITLE},
      {"modulesDummy8Title", IDS_NTP_MODULES_DUMMY2_TITLE},
      {"modulesDummy9Title", IDS_NTP_MODULES_DUMMY2_TITLE},
      {"modulesDummy10Title", IDS_NTP_MODULES_DUMMY2_TITLE},
      {"modulesDummy11Title", IDS_NTP_MODULES_DUMMY2_TITLE},
      {"modulesDummy12Title", IDS_NTP_MODULES_DUMMY2_TITLE},
      {"modulesKaleidoscopeTitle", IDS_NTP_MODULES_KALEIDOSCOPE_TITLE},
      {"modulesPhotosInfo", IDS_NTP_MODULES_PHOTOS_INFO},
      {"modulesPhotosSentence", IDS_NTP_MODULES_PHOTOS_MEMORIES_TITLE},
      {"modulesPhotosTitle", IDS_NTP_MODULES_PHOTOS_TITLE},
      {"modulesPhotosMemoriesDisable", IDS_NTP_MODULES_PHOTOS_MEMORIES_DISABLE},
      {"modulesPhotosMemoriesDisabled",
       IDS_NTP_MODULES_PHOTOS_MEMORIES_DISABLED},
      {"modulesPhotosMemoriesHideToday",
       IDS_NTP_MODULES_PHOTOS_MEMORIES_HIDE_TODAY},
      {"modulesPhotosMemoriesHiddenToday",
       IDS_NTP_MODULES_PHOTOS_MEMORIES_HIDDEN_TODAY},
      {"modulesPhotosMemoriesExplore", IDS_NTP_MODULES_PHOTOS_MEMORIES_EXPLORE},
      {"modulesPhotosMemoriesWelcomeTitle",
       IDS_NTP_MODULES_PHOTOS_MEMORIES_WELCOME_TITLE},
      {"modulesPhotosMemoriesWelcomeText",
       IDS_NTP_MODULES_PHOTOS_MEMORIES_WELCOME_TEXT},
      {"modulesPhotosMemoriesWelcomeButtonOptIn",
       IDS_NTP_MODULES_PHOTOS_MEMORIES_WELCOME_BUTTON_OPT_IN},
      {"modulesPhotosMemoriesWelcomeButtonOptOut",
       IDS_NTP_MODULES_PHOTOS_MEMORIES_WELCOME_BUTTON_OPT_OUT},
      {"modulesPhotosMemoriesWelcomeButtonSoftOptOut",
       IDS_NTP_MODULES_PHOTOS_MEMORIES_WELCOME_BUTTON_SOFT_OPT_OUT},
      {"modulesPhotosMemoriesSoftOptOut", IDS_NTP_MODULES_PHOTOS_SOFT_OPT_OUT},
      {"modulesPhotosMemoriesWelcomeExample",
       IDS_NTP_MODULES_PHOTOS_MEMORIES_WELCOME_EXAMPLE},
      {"modulesPhotosNew", IDS_NTP_MODULES_PHOTOS_NEW},
      {"modulesTasksInfoTitle", IDS_NTP_MODULES_SHOPPING_TASKS_INFO_TITLE},
      {"modulesTasksInfoClose", IDS_NTP_MODULES_SHOPPING_TASKS_INFO_CLOSE},
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
      {"modulesCartItemCountSingular",
       IDS_NTP_MODULES_CART_ITEM_COUNT_SINGULAR},
      {"modulesCartItemCountMultiple",
       IDS_NTP_MODULES_CART_ITEM_COUNT_MULTIPLE},
      {"modulesCartDiscountConsentContentV3",
       IDS_NTP_MODULES_CART_DISCOUNT_CONSENT_CONTENT_V3},
      {"modulesCartDiscountConentTitle",
       IDS_NTP_MODULES_CART_DISCOUNT_CONSENT_TITLE},
      {"modulesNewTagLabel", IDS_NTP_MODULES_NEW_TAG_LABEL},
      {"modulesFirstRunExperienceTitle",
       IDS_NTP_MODULES_FIRST_RUN_EXPERIENCE_TITLE},
      {"modulesFirstRunExperienceBodyLine1",
       IDS_NTP_MODULES_FIRST_RUN_EXPERIENCE_BODY_LINE_1},
      {"modulesFirstRunExperienceBodyLine2",
       IDS_NTP_MODULES_FIRST_RUN_EXPERIENCE_BODY_LINE_2},
      {"modulesFirstRunExperienceOptIn",
       IDS_NTP_MODULES_FIRST_RUN_EXPERIENCE_OPT_IN},
      {"modulesFirstRunExperienceOptOut",
       IDS_NTP_MODULES_FIRST_RUN_EXPERIENCE_OPT_OUT},
      {"modulesFirstRunExperienceOptOutToast",
       IDS_NTP_MODULES_FIRST_RUN_EXPERIENCE_OPT_OUT_TOAST},
  };
  source->AddLocalizedStrings(kStrings);

  source->AddInteger(
      "modulesCartDiscountConsentVariation",
      commerce::kNtpChromeCartModuleDiscountConsentNtpVariation.Get());

  if (base::FeatureList::IsEnabled(commerce::kDiscountConsentV2)) {
    AddResourcesForCartDiscountConsentV2(source);
  } else {
    source->AddLocalizedString("modulesCartDiscountConsentContent",
                               IDS_NTP_MODULES_CART_DISCOUNT_CONSENT_CONTENT);
  }

#if !defined(OFFICIAL_BUILD)
  source->AddBoolean(
      "dummyModulesEnabled",
      base::FeatureList::IsEnabled(ntp_features::kNtpDummyModules));
#endif
  source->AddBoolean(
      "recipeTasksModuleEnabled",
      base::FeatureList::IsEnabled(ntp_features::kNtpRecipeTasksModule));
  source->AddBoolean(
      "chromeCartModuleEnabled",
      base::FeatureList::IsEnabled(ntp_features::kNtpChromeCartModule));
  source->AddBoolean(
      "photosModuleEnabled",
      base::FeatureList::IsEnabled(ntp_features::kNtpPhotosModule));
  source->AddString("photosModuleCustomArtWork",
                    base::GetFieldTrialParamValueByFeature(
                        ntp_features::kNtpPhotosModuleCustomizedOptInArtWork,
                        ntp_features::kNtpPhotosModuleOptInArtWorkParam));
  source->AddBoolean(
      "ruleBasedDiscountEnabled",
      base::GetFieldTrialParamValueByFeature(
          ntp_features::kNtpChromeCartModule,
          ntp_features::kNtpChromeCartModuleAbandonedCartDiscountParam) ==
          "true");
  source->AddBoolean(
      "modulesRedesignedEnabled",
      base::FeatureList::IsEnabled(ntp_features::kNtpModulesRedesigned));
  source->AddBoolean(
      "modulesRedesignedLayoutEnabled",
      base::FeatureList::IsEnabled(ntp_features::kNtpModulesRedesignedLayout));

  std::vector<std::string> splitExperimentGroup = base::SplitString(
      base::GetFieldTrialParamValueByFeature(
          ntp_features::kNtpRecipeTasksModule,
          ntp_features::kNtpRecipeTasksModuleExperimentGroupParam),
      "-", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  source->AddBoolean(
      "modulesRecipeHistoricalExperimentEnabled",
      !splitExperimentGroup.empty() && splitExperimentGroup[0] == "historical");

  RealboxHandler::SetupWebUIDataSource(source);

  webui::SetupWebUIDataSource(
      source, base::make_span(kNewTabPageResources, kNewTabPageResourcesSize),
      IDR_NEW_TAB_PAGE_NEW_TAB_PAGE_HTML);

  // Allow embedding of iframes for the doodle and
  // chrome-untrusted://new-tab-page for other external content and resources.
  // NOTE: Use caution when overriding content security policies as that cean
  // lead to subtle security bugs such as https://crbug.com/1251541.
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ChildSrc,
      base::StringPrintf("child-src https: %s %s;",
                         google_util::CommandLineGoogleBaseURL().spec().c_str(),
                         chrome::kChromeUIUntrustedNewTabPageUrl));

  return source;
}

bool HasCredentials(Profile* profile) {
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  return
      /* Can be null if Chrome signin is disabled. */ identity_manager &&
      identity_manager->GetAccountsInCookieJar().signed_in_accounts.size() > 0;
}

}  // namespace

NewTabPageUI::NewTabPageUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui, /*enable_chrome_send=*/false),
      content::WebContentsObserver(web_ui->GetWebContents()),
      page_factory_receiver_(this),
      customize_themes_factory_receiver_(this),
      most_visited_page_factory_receiver_(this),
      browser_command_factory_receiver_(this),
      profile_(Profile::FromWebUI(web_ui)),
      theme_service_(ThemeServiceFactory::GetForProfile(profile_)),
      ntp_custom_background_service_(
          NtpCustomBackgroundServiceFactory::GetForProfile(profile_)),
      web_contents_(web_ui->GetWebContents()),
      // We initialize navigation_start_time_ to a reasonable value to account
      // for the unlikely case where the NewTabPageHandler is created before we
      // received the DidStartNavigation event.
      navigation_start_time_(base::Time::Now()) {
  auto* source = CreateNewTabPageUiHtmlSource(profile_);
  source->AddBoolean(
      "customBackgroundDisabledByPolicy",
      ntp_custom_background_service_->IsCustomBackgroundDisabledByPolicy());
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

  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(
      ntp_prefs::kNtpUseMostVisitedTiles,
      base::BindRepeating(&NewTabPageUI::OnCustomLinksEnabledPrefChanged,
                          weak_ptr_factory_.GetWeakPtr()));
  pref_change_registrar_.Add(
      ntp_prefs::kNtpShortcutsVisible,
      base::BindRepeating(&NewTabPageUI::OnTilesVisibilityPrefChanged,
                          weak_ptr_factory_.GetWeakPtr()));

  // Store basic theme info in load time data to make the background color and
  // background image available as soon as the page loads to prevent a potential
  // white flicker.

  // Load time data is cached across page reloads. Listen for theme changes so
  // that theme info is up-to-date when reloading.
  native_theme_observation_.Observe(ui::NativeTheme::GetInstanceForNativeUi());
  theme_service_observation_.Observe(theme_service_.get());
  ntp_custom_background_service_observation_.Observe(
      ntp_custom_background_service_.get());

  // Populates the load time data with basic info.
  OnThemeChanged();
  OnCustomBackgroundImageUpdated();
  OnLoad();
}

WEB_UI_CONTROLLER_TYPE_IMPL(NewTabPageUI)

NewTabPageUI::~NewTabPageUI() = default;

// static
bool NewTabPageUI::IsNewTabPageOrigin(const GURL& url) {
  return url.DeprecatedGetOriginAsURL() ==
         GURL(chrome::kChromeUINewTabPageURL).DeprecatedGetOriginAsURL();
}

// static
void NewTabPageUI::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterTimePref(kPrevNavigationTimePrefName, base::Time());
  registry->RegisterBooleanPref(ntp_prefs::kNtpUseMostVisitedTiles, false);
  registry->RegisterBooleanPref(ntp_prefs::kNtpShortcutsVisible, true);
  registry->RegisterBooleanPref(prefs::kNtpPromoVisible, true);
}

// static
void NewTabPageUI::ResetProfilePrefs(PrefService* prefs) {
  ntp_tiles::MostVisitedSites::ResetProfilePrefs(prefs);
  prefs->SetBoolean(ntp_prefs::kNtpUseMostVisitedTiles, false);
  prefs->SetBoolean(ntp_prefs::kNtpShortcutsVisible, true);
}

// static
bool NewTabPageUI::IsDriveModuleEnabled(Profile* profile) {
  // TODO(crbug.com/1321896): Explore not requiring sync for the drive
  // module to be enabled.
  auto* sync_service = SyncServiceFactory::GetForProfile(profile);
  if (!base::FeatureList::IsEnabled(ntp_features::kNtpDriveModule) ||
      !sync_service || !sync_service->IsSyncFeatureEnabled()) {
    return false;
  }
  if (base::GetFieldTrialParamValueByFeature(
          ntp_features::kNtpDriveModule,
          ntp_features::kNtpDriveModuleManagedUsersOnlyParam) != "true") {
    return true;
  }
  // TODO(crbug.com/1213351): Stop calling the private method
  // FindExtendedPrimaryAccountInfo().
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  return /* Can be null if Chrome signin is disabled. */ identity_manager &&
         identity_manager
             ->FindExtendedPrimaryAccountInfo(signin::ConsentLevel::kSync)
             .IsManaged();
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
    mojo::PendingReceiver<browser_command::mojom::CommandHandlerFactory>
        pending_receiver) {
  if (browser_command_factory_receiver_.is_bound())
    browser_command_factory_receiver_.reset();
  browser_command_factory_receiver_.Bind(std::move(pending_receiver));
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
    mojo::PendingReceiver<most_visited::mojom::MostVisitedPageHandlerFactory>
        pending_receiver) {
  if (most_visited_page_factory_receiver_.is_bound()) {
    most_visited_page_factory_receiver_.reset();
  }
  most_visited_page_factory_receiver_.Bind(std::move(pending_receiver));
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

void NewTabPageUI::BindInterface(
    mojo::PendingReceiver<photos::mojom::PhotosHandler> pending_receiver) {
  photos_handler_ = std::make_unique<PhotosHandler>(std::move(pending_receiver),
                                                    profile_, web_contents_);
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
      ntp_custom_background_service_, theme_service_,
      LogoServiceFactory::GetForProfile(profile_), web_contents_,
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

void NewTabPageUI::CreateBrowserCommandHandler(
    mojo::PendingReceiver<browser_command::mojom::CommandHandler>
        pending_handler) {
  using browser_command::mojom::Command;
  std::vector<Command> supported_commands = {
      Command::kOpenSafetyCheck,
      Command::kOpenSafeBrowsingEnhancedProtectionSettings,
  };
  promo_browser_command_handler_ = std::make_unique<BrowserCommandHandler>(
      std::move(pending_handler), profile_, supported_commands);
}

void NewTabPageUI::CreatePageHandler(
    mojo::PendingRemote<most_visited::mojom::MostVisitedPage> pending_page,
    mojo::PendingReceiver<most_visited::mojom::MostVisitedPageHandler>
        pending_page_handler) {
  DCHECK(pending_page.is_valid());
  most_visited_page_handler_ = std::make_unique<MostVisitedHandler>(
      std::move(pending_page_handler), std::move(pending_page), profile_,
      web_contents_, GURL(chrome::kChromeUINewTabPageURL),
      navigation_start_time_);
  most_visited_page_handler_->EnableCustomLinks(IsCustomLinksEnabled());
  most_visited_page_handler_->SetShortcutsVisible(IsShortcutsVisible());
}

void NewTabPageUI::OnNativeThemeUpdated(ui::NativeTheme* observed_theme) {
  OnThemeChanged();
}

void NewTabPageUI::OnThemeChanged() {
  base::Value::Dict update;

  const ui::ThemeProvider* theme_provider =
      webui::GetThemeProvider(web_contents_);
  // TODO(crbug.com/1299925): Always mock theme provider in tests so that
  // `theme_provider` is never nullptr.
  if (theme_provider) {
    auto background_color =
        theme_provider->GetColor(ThemeProperties::COLOR_NTP_BACKGROUND);
    update.Set("backgroundColor", skia::SkColorToHexString(background_color));
  } else {
    update.Set("backgroundColor", "");
  }
  content::WebUIDataSource::Update(profile_, chrome::kChromeUINewTabPageHost,
                                   std::move(update));
}

void NewTabPageUI::OnCustomBackgroundImageUpdated() {
  base::Value::Dict update;
  url::RawCanonOutputT<char> encoded_url;
  auto custom_background_url =
      (ntp_custom_background_service_
           ? ntp_custom_background_service_->GetCustomBackground()
           : absl::optional<CustomBackground>())
          .value_or(CustomBackground())
          .custom_background_url;
  url::EncodeURIComponent(custom_background_url.spec().c_str(),
                          custom_background_url.spec().size(), &encoded_url);
  update.Set(
      "backgroundImageUrl",
      encoded_url.length() > 0
          ? base::StrCat(
                {"chrome-untrusted://new-tab-page/custom_background_image?url=",
                 std::string(encoded_url.data(), encoded_url.length())})
          : "");
  content::WebUIDataSource::Update(profile_, chrome::kChromeUINewTabPageHost,
                                   std::move(update));
}

void NewTabPageUI::OnNtpCustomBackgroundServiceShuttingDown() {
  ntp_custom_background_service_observation_.Reset();
  ntp_custom_background_service_ = nullptr;
}

void NewTabPageUI::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->IsInPrimaryMainFrame() &&
      navigation_handle->GetURL() == GURL(chrome::kChromeUINewTabPageURL)) {
    navigation_start_time_ = base::Time::Now();
    OnLoad();
    auto prev_navigation_time =
        profile_->GetPrefs()->GetTime(kPrevNavigationTimePrefName);
    if (!prev_navigation_time.is_null()) {
      base::UmaHistogramCustomTimes(
          "NewTabPage.TimeSinceLastNTP",
          navigation_start_time_ - prev_navigation_time, base::Seconds(1),
          base::Days(1), 100);
    }
    profile_->GetPrefs()->SetTime(kPrevNavigationTimePrefName,
                                  navigation_start_time_);
    base::UmaHistogramBoolean("NewTabPage.HasCredentials",
                              HasCredentials(profile_));
  }
}

bool NewTabPageUI::IsCustomLinksEnabled() const {
  return !profile_->GetPrefs()->GetBoolean(ntp_prefs::kNtpUseMostVisitedTiles);
}

bool NewTabPageUI::IsShortcutsVisible() const {
  return profile_->GetPrefs()->GetBoolean(ntp_prefs::kNtpShortcutsVisible);
}

void NewTabPageUI::OnCustomLinksEnabledPrefChanged() {
  if (most_visited_page_handler_) {
    most_visited_page_handler_->EnableCustomLinks(IsCustomLinksEnabled());
  }
}

void NewTabPageUI::OnTilesVisibilityPrefChanged() {
  if (most_visited_page_handler_) {
    most_visited_page_handler_->SetShortcutsVisible(IsShortcutsVisible());
  }
}

void NewTabPageUI::OnLoad() {
  base::Value::Dict update;
  update.Set("navigationStartTime", navigation_start_time_.ToJsTime());
  // Only enable modules if account credentials are available as most modules
  // won't have data to render otherwise. We can override this behavior with the
  // "--signed-out-ntp-modules" command line switch, e.g. to allow modules in
  // perf tests, which do not support sign-in.
  update.Set("modulesEnabled",
             base::FeatureList::IsEnabled(ntp_features::kModules) &&
                 (base::CommandLine::ForCurrentProcess()->HasSwitch(
                      kSignedOutNtpModulesSwitch) ||
                  HasCredentials(profile_)));
  update.Set("driveModuleEnabled",
             NewTabPageUI::IsDriveModuleEnabled(profile_));
  content::WebUIDataSource::Update(profile_, chrome::kChromeUINewTabPageHost,
                                   std::move(update));
}

// static
base::RefCountedMemory* NewTabPageUI::GetFaviconResourceBytes(
    ui::ResourceScaleFactor scale_factor) {
  return static_cast<base::RefCountedMemory*>(
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytesForScale(
          IDR_NTP_FAVICON, scale_factor));
}
