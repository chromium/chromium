// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/apps/apps_section.h"

#include "ash/components/arc/app/arc_app_constants.h"
#include "ash/components/arc/arc_prefs.h"
#include "ash/components/arc/arc_util.h"
#include "ash/constants/ash_features.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/child_accounts/on_device_controls/app_controls_service_factory.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_features.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_pref_names.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_util.h"
#include "chrome/browser/ui/webui/ash/settings/pages/apps/android_apps_handler.h"
#include "chrome/browser/ui/webui/ash/settings/pages/apps/plugin_vm_handler.h"
#include "chrome/browser/ui/webui/ash/settings/pages/crostini/guest_os_handler.h"
#include "chrome/browser/ui/webui/ash/settings/pages/system_preferences/startup_section.h"
#include "chrome/browser/ui/webui/ash/settings/search/search_tag_registry.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_features.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/os_settings_resources.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/isolated_web_apps_policy.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/content_features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/chromeos/devicetype_utils.h"

namespace ash::settings {

namespace mojom {
using ::chromeos::settings::mojom::kAppDetailsSubpagePath;
using ::chromeos::settings::mojom::kAppManagementSubpagePath;
using ::chromeos::settings::mojom::kAppNotificationsManagerSubpagePath;
using ::chromeos::settings::mojom::kAppNotificationsSubpagePath;
using ::chromeos::settings::mojom::kAppParentalControlsSubpagePath;
using ::chromeos::settings::mojom::kAppsSectionPath;
using ::chromeos::settings::mojom::kArcVmUsbPreferencesSubpagePath;
using ::chromeos::settings::mojom::kGooglePlayStoreSubpagePath;
using ::chromeos::settings::mojom::kManageIsolatedWebAppsSubpagePath;
using ::chromeos::settings::mojom::kPluginVmSharedPathsSubpagePath;
using ::chromeos::settings::mojom::kPluginVmUsbPreferencesSubpagePath;
using ::chromeos::settings::mojom::Section;
using ::chromeos::settings::mojom::Setting;
using ::chromeos::settings::mojom::Subpage;
}  // namespace mojom

namespace {

const std::vector<SearchConcept>& GetAppsSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_APPS,
       mojom::kAppsSectionPath,
       mojom::SearchResultIcon::kAppsGrid,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSection,
       {.section = mojom::Section::kApps}},
      {IDS_OS_SETTINGS_TAG_APPS_MANAGEMENT,
       mojom::kAppManagementSubpagePath,
       mojom::SearchResultIcon::kAppsGrid,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kAppManagement},
       {IDS_OS_SETTINGS_TAG_APPS_MANAGEMENT_ALT1, SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetAppNotificationsSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_APP_NOTIFICATIONS,
       mojom::kAppNotificationsSubpagePath,
       ash::features::IsOsSettingsRevampWayfindingEnabled()
           ? mojom::SearchResultIcon::kNotifications
           : mojom::SearchResultIcon::kAppsGrid,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kAppNotifications}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetAppNotificationsManagerSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_APP_NOTIFICATIONS_MANAGER,
       mojom::kAppNotificationsManagerSubpagePath,
       ash::features::IsOsSettingsRevampWayfindingEnabled()
           ? mojom::SearchResultIcon::kNotifications
           : mojom::SearchResultIcon::kAppsGrid,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kAppNotificationsManager}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetAppBadgingSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags(
      {{IDS_OS_SETTINGS_TAG_APP_BADGING,
        mojom::kAppNotificationsSubpagePath,
        ash::features::IsOsSettingsRevampWayfindingEnabled()
            ? mojom::SearchResultIcon::kNotifications
            : mojom::SearchResultIcon::kAppsGrid,
        mojom::SearchResultDefaultRank::kMedium,
        mojom::SearchResultType::kSetting,
        {.setting = mojom::Setting::kAppBadgingOnOff}}});
  return *tags;
}

const std::vector<SearchConcept>& GetTurnOffAppNotificationSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags(
      {{IDS_OS_SETTINGS_TAG_DO_NOT_DISTURB_TURN_OFF,
        mojom::kAppNotificationsSubpagePath,
        ash::features::IsOsSettingsRevampWayfindingEnabled()
            ? mojom::SearchResultIcon::kNotifications
            : mojom::SearchResultIcon::kAppsGrid,
        mojom::SearchResultDefaultRank::kMedium,
        mojom::SearchResultType::kSetting,
        {.setting = mojom::Setting::kDoNotDisturbOnOff},
        {IDS_OS_SETTINGS_TAG_DO_NOT_DISTURB_TURN_OFF_ALT1,
         SearchConcept::kAltTagEnd}}});
  return *tags;
}

const std::vector<SearchConcept>& GetTurnOnAppNotificationSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags(
      {{IDS_OS_SETTINGS_TAG_DO_NOT_DISTURB_TURN_ON,
        mojom::kAppNotificationsSubpagePath,
        ash::features::IsOsSettingsRevampWayfindingEnabled()
            ? mojom::SearchResultIcon::kNotifications
            : mojom::SearchResultIcon::kAppsGrid,
        mojom::SearchResultDefaultRank::kMedium,
        mojom::SearchResultType::kSetting,
        {.setting = mojom::Setting::kDoNotDisturbOnOff},
        {IDS_OS_SETTINGS_TAG_DO_NOT_DISTURB_TURN_ON_ALT1,
         SearchConcept::kAltTagEnd}}});
  return *tags;
}

const std::vector<SearchConcept>& GetAndroidPlayStoreSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_PLAY_STORE,
       mojom::kGooglePlayStoreSubpagePath,
       mojom::SearchResultIcon::kGooglePlay,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kGooglePlayStore}},
      {IDS_OS_SETTINGS_TAG_REMOVE_PLAY_STORE,
       mojom::kGooglePlayStoreSubpagePath,
       mojom::SearchResultIcon::kGooglePlay,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kRemovePlayStore},
       {IDS_OS_SETTINGS_TAG_REMOVE_PLAY_STORE_ALT1, SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetAndroidSettingsSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_ANDROID_SETTINGS_WITH_PLAY_STORE,
       mojom::kGooglePlayStoreSubpagePath,
       mojom::SearchResultIcon::kGooglePlay,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kManageAndroidPreferences},
       {IDS_OS_SETTINGS_TAG_ANDROID_SETTINGS_WITH_PLAY_STORE_ALT1,
        SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetAndroidNoPlayStoreSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_ANDROID_SETTINGS,
       mojom::kAppsSectionPath,
       mojom::SearchResultIcon::kAndroid,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kManageAndroidPreferences},
       {IDS_OS_SETTINGS_TAG_ANDROID_SETTINGS_ALT1, SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetAndroidPlayStoreDisabledSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_ANDROID_TURN_ON_PLAY_STORE,
       mojom::kAppsSectionPath,
       mojom::SearchResultIcon::kAndroid,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kTurnOnPlayStore},
       {IDS_OS_SETTINGS_TAG_ANDROID_TURN_ON_PLAY_STORE_ALT1,
        SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetManageIsolatedWebAppsSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags(
      {{IDS_OS_SETTINGS_TAG_MANAGE_ISOLATED_WEB_APPS,
        mojom::kManageIsolatedWebAppsSubpagePath,
        ash::features::IsOsSettingsRevampWayfindingEnabled()
            ? mojom::SearchResultIcon::kNotifications
            : mojom::SearchResultIcon::kAppsGrid,
        mojom::SearchResultDefaultRank::kMedium,
        mojom::SearchResultType::kSubpage,
        {.subpage = mojom::Subpage::kManageIsolatedWebApps}}});
  return *tags;
}

const std::vector<SearchConcept>& GetParentalControlsSearchConcepts() {
  // Redirect search queries to the parental controls row in the Apps section
  // because the app parental controls page should only be accessed after the
  // user has entered their PIN, which is triggered from the settings row.
  static const base::NoDestructor<std::vector<SearchConcept>> tags(
      {{IDS_OS_SETTINGS_TAG_APPS_PARENTAL_CONTROLS,
        mojom::kAppsSectionPath,
        mojom::SearchResultIcon::kAppsParentalControls,
        mojom::SearchResultDefaultRank::kMedium,
        mojom::SearchResultType::kSetting,
        {.setting = mojom::Setting::kAppParentalControls},
        {IDS_OS_SETTINGS_TAG_APPS_PARENTAL_CONTROLS_ALT1,
         SearchConcept::kAltTagEnd}}});
  return *tags;
}

const std::vector<SearchConcept>& GetTurnOnIsolatedWebAppsSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags(
      {{IDS_OS_SETTINGS_TAG_TURN_ON_ISOLATED_WEB_APPS,
        mojom::kManageIsolatedWebAppsSubpagePath,
        ash::features::IsOsSettingsRevampWayfindingEnabled()
            ? mojom::SearchResultIcon::kNotifications
            : mojom::SearchResultIcon::kAppsGrid,
        mojom::SearchResultDefaultRank::kMedium,
        mojom::SearchResultType::kSetting,
        {.setting = mojom::Setting::kEnableIsolatedWebAppsOnOff},
        {IDS_OS_SETTINGS_TAG_TURN_ON_ISOLATED_WEB_APPS_ALT1,
         SearchConcept::kAltTagEnd}}});
  return *tags;
}

void AddAppManagementStrings(content::WebUIDataSource* html_source) {
  const bool kIsRevampEnabled =
      ash::features::IsOsSettingsRevampWayfindingEnabled();

  webui::LocalizedString kLocalizedStrings[] = {
      {"appManagementAppDetailsTitle", IDS_APP_MANAGEMENT_APP_DETAILS_TITLE},
      {"appManagementAppDetailsTooltipWebA11y",
       IDS_APP_MANAGEMENT_APP_DETAILS_TOOLTIP_WEB_A11Y},
      {"appManagementAppDetailsTypeAndroid",
       IDS_OS_SETTINGS_APP_DETAILS_TYPE_ANDROID},
      {"appManagementAppDetailsTypeChrome",
       IDS_OS_SETTINGS_APP_DETAILS_TYPE_CHROME},
      {"appManagementAppDetailsTypeWeb", IDS_OS_SETTINGS_APP_DETAILS_TYPE_WEB},
      {"appManagementAppDetailsTypeCrosSystem",
       IDS_OS_SETTINGS_APP_DETAILS_TYPE_CROS_SYSTEM},
      {"appManagementAppDetailsInstallSourceWebStore",
       IDS_APP_MANAGEMENT_APP_DETAILS_INSTALL_SOURCE_WEB_STORE},
      {"appManagementAppDetailsInstallSourcePlayStore",
       IDS_APP_MANAGEMENT_APP_DETAILS_INSTALL_SOURCE_PLAY_STORE},
      {"appManagementAppDetailsInstallSourceBrowser",
       IDS_APP_MANAGEMENT_APP_DETAILS_INSTALL_SOURCE_BROWSER},
      {"appManagementAppDetailsTypeAndSourcePreinstalledApp",
       IDS_APP_MANAGEMENT_APP_DETAILS_TYPE_AND_SOURCE_PREINSTALLED_APP},
      {"appManagementAppDetailsTypeAndSourceCombined",
       IDS_APP_MANAGEMENT_APP_DETAILS_TYPE_AND_SOURCE_COMBINED},
      {"appManagementAppDetailsTypeAndroidInstallReasonPolicy",
       IDS_OS_SETTINGS_APP_DETAILS_TYPE_ANDROID_INSTALL_REASON_POLICY},
      {"appManagementAppDetailsTypeChromeInstallReasonPolicy",
       IDS_OS_SETTINGS_APP_DETAILS_TYPE_CHROME_INSTALL_REASON_POLICY},
      {"appManagementAppDetailsTypeWebInstallReasonPolicy",
       IDS_OS_SETTINGS_APP_DETAILS_TYPE_WEB_INSTALL_REASON_POLICY},
      {"appManagementAppDetailsVersion",
       IDS_APP_MANAGEMENT_APP_DETAILS_VERSION},
      {"appManagementAppDetailsStorageTitle",
       IDS_APP_MANAGEMENT_APP_DETAILS_STORAGE_TITLE},
      {"appManagementAppDetailsAppSize",
       IDS_APP_MANAGEMENT_APP_DETAILS_APP_SIZE},
      {"appManagementAppDetailsDataSize",
       IDS_APP_MANAGEMENT_APP_DETAILS_DATA_SIZE},
      {"appManagementAppInstalledByPolicyLabel",
       IDS_APP_MANAGEMENT_POLICY_APP_POLICY_STRING},
      {"appManagementArcManagePermissionsLabel",
       IDS_APP_MANAGEMENT_ARC_MANAGE_PERMISSIONS},
      {"appManagementCameraPermissionLabel", IDS_APP_MANAGEMENT_CAMERA},
      {"appManagementContactsPermissionLabel", IDS_APP_MANAGEMENT_CONTACTS},
      {"appManagementFileHandlingHeader",
       IDS_APP_MANAGEMENT_FILE_HANDLING_HEADER},
      {"appManagementIntentOverlapChangeButton",
       IDS_APP_MANAGEMENT_INTENT_OVERLAP_CHANGE_BUTTON},
      {"appManagementIntentOverlapDialogText1App",
       IDS_APP_MANAGEMENT_INTENT_OVERLAP_DIALOG_TEXT_1_APP},
      {"appManagementIntentOverlapDialogText2Apps",
       IDS_APP_MANAGEMENT_INTENT_OVERLAP_DIALOG_TEXT_2_APPS},
      {"appManagementIntentOverlapDialogText3Apps",
       IDS_APP_MANAGEMENT_INTENT_OVERLAP_DIALOG_TEXT_3_APPS},
      {"appManagementIntentOverlapDialogText4Apps",
       IDS_APP_MANAGEMENT_INTENT_OVERLAP_DIALOG_TEXT_4_APPS},
      {"appManagementIntentOverlapDialogText5OrMoreApps",
       IDS_APP_MANAGEMENT_INTENT_OVERLAP_DIALOG_TEXT_5_OR_MORE_APPS},
      {"appManagementIntentOverlapDialogTitle",
       IDS_APP_MANAGEMENT_INTENT_OVERLAP_DIALOG_TITLE},
      {"appManagementIntentOverlapWarningText1App",
       IDS_APP_MANAGEMENT_INTENT_OVERLAP_WARNING_TEXT_1_APP},
      {"appManagementIntentOverlapWarningText2Apps",
       IDS_APP_MANAGEMENT_INTENT_OVERLAP_WARNING_TEXT_2_APPS},
      {"appManagementIntentOverlapWarningText3Apps",
       IDS_APP_MANAGEMENT_INTENT_OVERLAP_WARNING_TEXT_3_APPS},
      {"appManagementIntentOverlapWarningText4Apps",
       IDS_APP_MANAGEMENT_INTENT_OVERLAP_WARNING_TEXT_4_APPS},
      {"appManagementIntentOverlapWarningText5OrMoreApps",
       IDS_APP_MANAGEMENT_INTENT_OVERLAP_WARNING_TEXT_5_OR_MORE_APPS},
      {"appManagementIntentSettingsDialogTitle",
       IDS_APP_MANAGEMENT_INTENT_SETTINGS_DIALOG_TITLE},
      {"appManagementIntentSettingsTitle",
       IDS_APP_MANAGEMENT_INTENT_SETTINGS_TITLE},
      {"appManagementIntentSharingOpenAppLabel",
       kIsRevampEnabled ? IDS_OS_SETTINGS_REVAMP_OPEN_IN_APP_TITLE
                        : IDS_APP_MANAGEMENT_INTENT_SHARING_APP_OPEN},
      {"appManagementPermissionItemClickTogglePermission",
       IDS_OS_SETTINGS_PERMISSION_ITEM_CLICK_TOGGLE_PERMISSION},
      {"appManagementIntentSharingOpenBrowserLabel",
       IDS_APP_MANAGEMENT_INTENT_SHARING_BROWSER_OPEN},
      {"appManagementIntentSharingTabExplanation",
       IDS_APP_MANAGEMENT_INTENT_SHARING_TAB_EXPLANATION},
      {"appManagementLocationPermissionLabel", IDS_APP_MANAGEMENT_LOCATION},
      {"appManagementMicrophonePermissionLabel", IDS_APP_MANAGEMENT_MICROPHONE},
      {"appManagementMorePermissionsLabel", IDS_APP_MANAGEMENT_MORE_SETTINGS},
      {"appManagementMorePermissionsLabelAndroidApp",
       IDS_OS_SETTINGS_REVAMP_APP_PERMISSIONS_TITLE_ANDROID},
      {"appManagementMorePermissionsLabelWebApp",
       IDS_OS_SETTINGS_REVAMP_APP_PERMISSIONS_TITLE_WEB_APP},
      {"appManagementMorePermissionsLabelChromeApp",
       IDS_OS_SETTINGS_REVAMP_APP_PERMISSIONS_TITLE_CHROME_APP},
      {"appManagementNoAppsFound", IDS_APP_MANAGEMENT_NO_APPS_FOUND},
      {"appManagementNoPermissions",
       IDS_APPLICATION_INFO_APP_NO_PERMISSIONS_TEXT},
      {"appManagementNotificationsLabel",
       kIsRevampEnabled ? IDS_OS_SETTINGS_REVAMP_APP_NOTIFICATIONS_TITLE
                        : IDS_APP_MANAGEMENT_NOTIFICATIONS},
      {"appManagementParentAppPermissionExplanation",
       IDS_APP_MANAGEMENT_PARENT_APP_PERMISSION_EXPLANATION},
      {"appManagementPermissionAllowed", IDS_APP_MANAGEMENT_PERMISSION_ALLOWED},
      {"appManagementPermissionAllowedWithDetails",
       IDS_APP_MANAGEMENT_PERMISSION_ALLOWED_WITH_DETAILS},
      {"appManagementPermissionAsk", IDS_APP_MANAGEMENT_PERMISSION_ASK},
      {"appManagementPermissionDenied", IDS_APP_MANAGEMENT_PERMISSION_DENIED},
      {"appManagementPermissionsLabel", IDS_APP_MANAGEMENT_PERMISSIONS},
      {"appManagementPinToShelfLabel", IDS_APP_MANAGEMENT_PIN_TO_SHELF},
      {"appManagementPresetWindowSizesLabel",
       IDS_APP_MANAGEMENT_PRESET_WINDOW_SIZES},
      {"appManagementPresetWindowSizesText",
       IDS_APP_MANAGEMENT_PRESET_WINDOW_SIZES_TEXT},
      {"appManagementPrintingPermissionLabel", IDS_APP_MANAGEMENT_PRINTING},
      {"appManagementSearchPrompt", IDS_APP_MANAGEMENT_SEARCH_PROMPT},
      {"appManagementStoragePermissionLabel", IDS_APP_MANAGEMENT_STORAGE},
      {"appManagementSubAppsListHeading",
       IDS_APP_MANAGEMENT_SUB_APPS_LIST_HEADING},
      {"appManagementSubAppPermissionExplanation",
       IDS_APP_MANAGEMENT_SUB_APP_PERMISSION_EXPLANATION},
      {"appManagementUninstallLabel", IDS_APP_MANAGEMENT_UNINSTALL_APP},
      {"close", IDS_CLOSE},
      {"fileHandlingOverflowDialogTitle",
       IDS_APP_MANAGEMENT_FILE_HANDLING_OVERFLOW_DIALOG_TITLE},
      {"fileHandlingSetDefaults",
       IDS_APP_MANAGEMENT_FILE_HANDLING_SET_DEFAULTS_LINK},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);
}

void AddGuestOsStrings(content::WebUIDataSource* html_source) {
  // These strings are used for both Crostini and Plugin VM.
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"guestOsSharedPaths", IDS_SETTINGS_GUEST_OS_SHARED_PATHS},
      {"guestOsSharedPathsListHeading",
       IDS_SETTINGS_GUEST_OS_SHARED_PATHS_LIST_HEADING},
      {"guestOsSharedPathsInstructionsRemove",
       IDS_SETTINGS_GUEST_OS_SHARED_PATHS_INSTRUCTIONS_REMOVE},
      {"guestOsSharedPathsStopSharing",
       IDS_SETTINGS_GUEST_OS_SHARED_PATHS_STOP_SHARING},
      {"guestOsSharedPathsRemoveFailureDialogTitle",
       IDS_SETTINGS_GUEST_OS_SHARED_PATHS_REMOVE_FAILURE_DIALOG_TITLE},
      {"guestOsSharedPathsRemoveFailureTryAgain",
       IDS_SETTINGS_GUEST_OS_SHARED_PATHS_REMOVE_FAILURE_TRY_AGAIN},
      {"guestOsSharedPathsListEmptyMessage",
       IDS_SETTINGS_GUEST_OS_SHARED_PATHS_LIST_EMPTY_MESSAGE},
      {"guestOsSharedUsbDevicesLabel",
       IDS_SETTINGS_GUEST_OS_SHARED_USB_DEVICES_LABEL},
      {"guestOsSharedUsbDevicesDescription",
       IDS_SETTINGS_GUEST_OS_SHARED_USB_DEVICES_DESCRIPTION},
      {"guestOsSharedUsbDevicesExtraDescription",
       IDS_SETTINGS_GUEST_OS_SHARED_USB_DEVICES_EXTRA_DESCRIPTION},
      {"guestOsSharedUsbDevicesListEmptyMessage",
       IDS_SETTINGS_GUEST_OS_SHARED_USB_DEVICES_LIST_EMPTY_MESSAGE},
      {"guestOsSharedUsbDevicesInUse",
       IDS_SETTINGS_GUEST_OS_SHARED_USB_DEVICES_IN_USE},
      {"guestOsSharedUsbDevicesReassign",
       IDS_SETTINGS_GUEST_OS_SHARED_USB_DEVICES_REASSIGN},
      {"guestOsSharedUsbDevicesTableTitle",
       IDS_SETTINGS_GUEST_OS_SHARED_USB_DEVICES_TABLE_TITLE},
      {"guestOsSharedUsbDevicesAddTitle",
       IDS_SETTINGS_GUEST_OS_SHARED_USB_DEVICES_ADD_TITLE},
      {"guestOsSharedUsbDevicesNoneAttached",
       IDS_SETTINGS_GUEST_OS_SHARED_USB_DEVICES_NONE_ATTACHED},
      {"guestOsSharedUsbDevicesNotificationDialogTitleEnable",
       IDS_SETTINGS_GUEST_OS_SHARED_USB_DEVICES_NOTIFICATION_DIALOG_TITLE_ENABLE},
      {"guestOsSharedUsbDevicesNotificationDialogTitleDisable",
       IDS_SETTINGS_GUEST_OS_SHARED_USB_DEVICES_NOTIFICATION_DIALOG_TITLE_DISABLE},
      {"guestOsSharedUsbDevicesNotificationDialogAccept",
       IDS_SETTINGS_GUEST_OS_SHARED_USB_DEVICES_NOTIFICATION_DIALOG_ACCEPT},
      {"guestOsSharedUsbDevicesNotificationsLabel",
       IDS_SETTINGS_GUEST_OS_SHARED_USB_DEVICES_NOTIFICATION_LABEL},
      {"guestOsSharedUsbPersistentPassthroughLabel",
       IDS_SETTINGS_GUEST_OS_SHARED_USB_PERSISTENT_PASSTHROUGH_LABEL},
      {"guestOsSharedUsbPersistentPassthroughDialogTitleEnable",
       IDS_SETTINGS_GUEST_OS_SHARED_USB_PERSISTENT_PASSTHROUGH_DIALOG_TITLE_ENABLE},
      {"guestOsSharedUsbPersistentPassthroughDialogTitleDisable",
       IDS_SETTINGS_GUEST_OS_SHARED_USB_PERSISTENT_PASSTHROUGH_DIALOG_TITLE_DISABLE},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);
}

void AddBorealisStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"borealisMainPermissionText",
       IDS_SETTINGS_APPS_BOREALIS_MAIN_PERMISSION_TEXT},
      {"borealisAppPermissionText",
       IDS_SETTINGS_APPS_BOREALIS_APP_PERMISSION_TEXT},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);
}

void AddAppParentalControlsStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"appParentalControlsAccessDialogTitle",
       IDS_OS_SETTINGS_APP_PARENTAL_CONTROLS_ACCESS_DIALOG_TITLE},
      {"appParentalControlsBlockedAppsCountText",
       IDS_OS_SETTINGS_APP_PARENTAL_CONTROLS_BLOCKED_APPS_COUNT_TEXT},
      {"appParentalControlsChoosePinSubtitle",
       IDS_OS_SETTINGS_APP_PARENTAL_CONTROLS_CHOOSE_PIN_SUBTITLE},
      {"appParentalControlsChoosePinTitle",
       IDS_OS_SETTINGS_APP_PARENTAL_CONTROLS_CHOOSE_PIN_TITLE},
      {"appParentalControlsConfirmPinTitle",
       IDS_OS_SETTINGS_APP_PARENTAL_CONTROLS_CONFIRM_PIN_TITLE},
      {"appParentalControlsForgotPinLinkName",
       IDS_OS_SETTINGS_APP_PARENTAL_CONTROLS_FORGOT_PIN_LINK_NAME},
      {"appParentalControlsHeaderText",
       IDS_OS_SETTINGS_APP_PARENTAL_CONTROLS_APPS_TITLE_TEXT},
      {"appParentalControlsNoAppsText",
       IDS_OS_SETTINGS_APP_PARENTAL_CONTROLS_NO_APPS_FOUND_TEXT},
      {"appParentalControlsPinIncorrectErrorText",
       IDS_OS_SETTINGS_APP_PARENTAL_CONTROLS_PIN_INCORRECT_ERROR_TEXT},
      {"appParentalControlsPinMismatchErrorText",
       IDS_OS_SETTINGS_APP_PARENTAL_CONTROLS_PIN_MISMATCH_ERROR_TEXT},
      {"appParentalControlsSearchPrompt",
       IDS_OS_SETTINGS_APP_PARENTAL_CONTROLS_SEARCH_PROMPT},
      {"appParentalControlsTitle", IDS_OS_SETTINGS_APP_PARENTAL_CONTROLS_LABEL},
      {"appParentalControlsPinWrongLengthErrorText",
       IDS_OS_SETTINGS_APP_PARENTAL_CONTROLS_PIN_WRONG_LENGTH_ERROR_TEXT},
      {"appParentalControlsPinNumericErrorText",
       IDS_OS_SETTINGS_APP_PARENTAL_CONTROLS_PIN_NUMERIC_ERROR_TEXT},
      {"appParentalControlsTitle", IDS_OS_SETTINGS_APP_PARENTAL_CONTROLS_LABEL},
      {"appParentalControlsSetUpButton",
       IDS_OS_SETTINGS_APP_PARENTAL_CONTROLS_SET_UP_BUTTON},
      {"appParentalControlsTitle", IDS_OS_SETTINGS_APP_PARENTAL_CONTROLS_LABEL},
  };

  html_source->AddLocalizedStrings(kLocalizedStrings);

  html_source->AddString("appParentalControlsLearnMoreUrl",
                         chrome::kAppParentalControlsLearnMoreUrl);

  html_source->AddString(
      "appParentalControlsSubtitle",
      l10n_util::GetStringFUTF16(IDS_OS_SETTINGS_APP_PARENTAL_CONTROLS_SUBLABEL,
                                 ui::GetChromeOSDeviceName(),
                                 chrome::kAppParentalControlsLearnMoreUrl));
  html_source->AddString(
      "appParentalControlsSubtitleDescription",
      l10n_util::GetStringFUTF16(
          IDS_OS_SETTINGS_APP_PARENTAL_CONTROLS_SUBLABEL_DESCRIPTION,
          ui::GetChromeOSDeviceName()));
}

bool ShowPluginVm(const Profile* profile, const PrefService& pref_service) {
  // Even if not allowed, we still want to show Plugin VM if the VM image is on
  // disk, so that users are still able to delete the image at will.
  return plugin_vm::PluginVmFeatures::Get()->IsAllowed(profile) ||
         pref_service.GetBoolean(plugin_vm::prefs::kPluginVmImageExists);
}

}  // namespace

AppsSection::AppsSection(Profile* profile,
                         SearchTagRegistry* search_tag_registry,
                         PrefService* pref_service,
                         ArcAppListPrefs* arc_app_list_prefs,
                         apps::AppServiceProxy* app_service_proxy)
    : OsSettingsSection(profile, search_tag_registry),
      startup_subsection_(
          !ash::features::IsOsSettingsRevampWayfindingEnabled()
              ? std::make_optional<StartupSection>(profile, search_tag_registry)
              : std::nullopt),
      pref_service_(pref_service),
      arc_app_list_prefs_(arc_app_list_prefs),
      app_service_proxy_(app_service_proxy),
      is_arc_allowed_(arc::IsArcAllowedForProfile(profile)) {
  if (!ash::features::IsOsSettingsRevampWayfindingEnabled()) {
    CHECK(startup_subsection_);
  }

  SearchTagRegistry::ScopedTagUpdater updater = registry()->StartUpdate();
  updater.AddSearchTags(GetAppsSearchConcepts());

  // Note: The MessageCenterAsh check here is added for unit testing purposes
  // otherwise check statements are not needed in production.
  if (MessageCenterAsh::Get()) {
    MessageCenterAsh::Get()->AddObserver(this);
    OnQuietModeChanged(MessageCenterAsh::Get()->IsQuietMode());
  }

  if (is_arc_allowed_) {
    pref_change_registrar_.Init(pref_service_);
    pref_change_registrar_.Add(
        arc::prefs::kArcEnabled,
        base::BindRepeating(&AppsSection::UpdateAndroidSearchTags,
                            base::Unretained(this)));

    if (arc_app_list_prefs_) {
      arc_app_list_prefs_->AddObserver(this);
    }

    UpdateAndroidSearchTags();
  }

  if (web_app::IsIwaUnmanagedInstallEnabled(profile)) {
    updater.AddSearchTags(GetManageIsolatedWebAppsSearchConcepts());
    updater.AddSearchTags(GetTurnOnIsolatedWebAppsSearchConcepts());
  }

  if (on_device_controls::AppControlsServiceFactory::
          IsOnDeviceAppControlsAvailable(profile)) {
    updater.AddSearchTags(GetParentalControlsSearchConcepts());
  }
}

AppsSection::~AppsSection() {
  // TODO(crbug.com/1237465): observer is never removed because ash::Shell is
  // destroyed first.
  // Note: The MessageCenterAsh check is also added for unit testing purposes.
  if (MessageCenterAsh::Get()) {
    MessageCenterAsh::Get()->RemoveObserver(this);
  }

  if (arc::IsArcAllowedForProfile(profile())) {
    if (arc_app_list_prefs_) {
      arc_app_list_prefs_->RemoveObserver(this);
    }
  }
}

void AppsSection::AddLoadTimeData(content::WebUIDataSource* html_source) {
  const bool kIsRevampEnabled =
      ash::features::IsOsSettingsRevampWayfindingEnabled();

  webui::LocalizedString kLocalizedStrings[] = {
      {"appsPageTitle", IDS_SETTINGS_APPS_TITLE},
      {"appsMenuItemDescription", IDS_OS_SETTINGS_APPS_MENU_ITEM_DESCRIPTION},
      {"appsmenuItemDescriptionArcUnavailable",
       IDS_OS_SETTINGS_APPS_MENU_ITEM_DESCRIPTION_ARC_UNAVAILABLE},
      {"appManagementTitle", IDS_SETTINGS_APPS_LINK_TEXT},
      {"appNotificationsTitle", IDS_SETTINGS_APP_NOTIFICATIONS_LINK_TEXT},
      {"doNotDisturbToggleTitle",
       IDS_SETTINGS_APP_NOTIFICATIONS_DO_NOT_DISTURB_TOGGLE_TITLE},
      {"manageIsolatedWebAppsLinkText",
       IDS_SETTINGS_MANAGE_ISOLATED_WEB_APPS_LINK_TEXT},
      {"manageIsolatedWebAppsTitle",
       IDS_SETTINGS_MANAGE_ISOLATED_WEB_APPS_SUBPAGE_TITLE},

      {"appNotificationsManagerLabel",
       IDS_OS_SETTINGS_REVAMP_NOTIFICATIONS_MANAGER_LABEL},
      {"appNotificationsManagerSublabel",
       IDS_OS_SETTINGS_REVAMP_NOTIFICATIONS_MANAGER_LINK_DESCRIPTION},
      {"doNotDisturbToggleDescription",
       kIsRevampEnabled
           ? IDS_OS_SETTINGS_REVAMP_APP_NOTIFICATIONS_DO_NOT_DISTURB_TOGGLE_DESCRIPTION
           : IDS_SETTINGS_APP_NOTIFICATIONS_DO_NOT_DISTURB_TOGGLE_DESCRIPTION},
      {"appNotificationsLinkToBrowserSettingsDescription",
       IDS_SETTINGS_APP_NOTIFICATIONS_LINK_TO_BROWSER_SETTINGS_DESCRIPTION},
      {"appNotificationsRowSublabel",
       IDS_OS_SETTINGS_REVAMP_APP_NOTIFICATIONS_LINK_DESCRIPTION},
      {"appNotificationsCountDescription",
       IDS_SETTINGS_APP_NOTIFICATIONS_SUBLABEL_TEXT},
      {"appNotificationsDoNotDisturbEnabledDescription",
       IDS_SETTINGS_APP_NOTIFICATIONS_DND_ENABLED_SUBLABEL_TEXT},
      {"appBadgingToggleLabel", IDS_SETTINGS_APP_BADGING_TOGGLE_LABEL},
      {"appBadgingToggleSublabel", IDS_SETTINGS_APP_BADGING_TOGGLE_SUBLABEL},
      {"enableIsolatedWebAppsToggleLabel",
       IDS_SETTINGS_ENABLE_ISOLATED_WEB_APPS_LABEL},
      {"appManagementAppLanguageLabel", IDS_APP_MANAGEMENT_APP_LANGUAGE_LABEL},
      {"permissionAllowedTextWithTurnOnCameraAccessButton",
       IDS_APP_MANAGEMENT_PERMISSION_ALLOWED_TEXT_WITH_TURN_ON_SYSTEM_CAMERA_ACCESS_BUTTON},
      {"permissionAllowedTextWithTurnOnMicrophoneAccessButton",
       IDS_APP_MANAGEMENT_PERMISSION_ALLOWED_TEXT_WITH_TURN_ON_SYSTEM_MICROPHONE_ACCESS_BUTTON},
      {"permissionAllowedTextWithTurnOnLocationAccessButton",
       IDS_APP_MANAGEMENT_PERMISSION_ALLOWED_TEXT_WITH_TURN_ON_SYSTEM_LOCATION_ACCESS_BUTTON},
      {"permissionAllowedTextWithDetailsAndTurnOnCameraAccessButton",
       IDS_APP_MANAGEMENT_PERMISSION_ALLOWED_TEXT_WITH_DETAILS_AND_TURN_ON_SYSTEM_CAMERA_ACCESS_BUTTON},
      {"permissionAllowedTextWithDetailsAndTurnOnMicrophoneAccessButton",
       IDS_APP_MANAGEMENT_PERMISSION_ALLOWED_TEXT_WITH_DETAILS_AND_TURN_ON_SYSTEM_MICROPHONE_ACCESS_BUTTON},
      {"permissionAllowedTextWithDetailsAndTurnOnLocationAccessButton",
       IDS_APP_MANAGEMENT_PERMISSION_ALLOWED_TEXT_WITH_DETAILS_AND_TURN_ON_SYSTEM_LOCATION_ACCESS_BUTTON},
      {"permissionAllowedButNoCameraConnectedText",
       IDS_APP_MANAGEMENT_PERMISSION_ALLOWED_BUT_NO_CAMERA_CONNECTED_TEXT},
      {"permissionAllowedButNoMicrophoneConnectedText",
       IDS_APP_MANAGEMENT_PERMISSION_ALLOWED_BUT_NO_MICROPHONE_CONNECTED_TEXT},
      {"permissionAllowedButNoCameraConnectedTextWithDetails",
       IDS_APP_MANAGEMENT_PERMISSION_ALLOWED_BUT_NO_CAMERA_CONNECTED_TEXT_WITH_DETAILS},
      {"permissionAllowedButNoMicrophoneConnectedTextWithDetails",
       IDS_APP_MANAGEMENT_PERMISSION_ALLOWED_BUT_NO_MICROPHONE_CONNECTED_TEXT_WITH_DETAILS},
      {"permissionAllowedButMicrophoneHwSwitchActiveText",
       IDS_APP_MANAGEMENT_PERMISSION_ALLOWED_BUT_MICROPHONE_HW_SWITCH_ACTIVE_TEXT},
      {"permissionAllowedButMicrophoneHwSwitchActiveTextWithDetails",
       IDS_APP_MANAGEMENT_PERMISSION_ALLOWED_BUT_MICROPHONE_HW_SWITCH_ACTIVE_TEXT_WITH_DETAILS},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);

  html_source->AddBoolean("appManagementArcReadOnlyPermissions",
                          arc::IsReadOnlyPermissionsEnabled());

  html_source->AddString("appManagementDeviceName",
                         ui::GetChromeOSDeviceName());

  html_source->AddString("appNotificationsBrowserSettingsURL",
                         chrome::kAppNotificationsBrowserSettingsURL);

  // We have 2 variants of Android apps settings. Default case, when the Play
  // Store app exists we show expandable section that allows as to
  // enable/disable the Play Store and link to Android settings which is
  // available once settings app is registered in the system.
  // For AOSP images we don't have the Play Store app. In last case we Android
  // apps settings consists only from root link to Android settings and only
  // visible once settings app is registered.
  html_source->AddBoolean("androidAppsVisible", is_arc_allowed_);
  html_source->AddBoolean("isPlayStoreAvailable", arc::IsPlayStoreAvailable());

  html_source->AddBoolean(
      "showOsSettingsAppNotificationsRow",
      base::FeatureList::IsEnabled(features::kOsSettingsAppNotificationsPage));
  html_source->AddBoolean("isArcVmEnabled", arc::IsArcVmEnabled());

  html_source->AddBoolean("showManageIsolatedWebAppsRow",
                          web_app::IsIwaUnmanagedInstallEnabled(profile()));
  html_source->AddString(
      "isolatedWebAppsDescription",
      l10n_util::GetStringFUTF16(IDS_SETTINGS_ISOLATED_WEB_APPS_DESCRIPTION,
                                 chrome::kIsolatedWebAppsLearnMoreUrl));

  html_source->AddBoolean("privacyHubAppPermissionsV2Enabled",
                          features::IsCrosPrivacyHubAppPermissionsV2Enabled());

  html_source->AddBoolean("privacyHubLocationAccessControlEnabled",
                          ash::features::IsCrosPrivacyHubLocationEnabled());

  html_source->AddBoolean("isAppParentalControlsFeatureAvailable",
                          on_device_controls::AppControlsServiceFactory::
                              IsOnDeviceAppControlsAvailable(profile()));

  AddAppManagementStrings(html_source);
  AddGuestOsStrings(html_source);
  AddAndroidAppStrings(html_source);
  AddPluginVmLoadTimeData(html_source);
  AddBorealisStrings(html_source);
  AddAppParentalControlsStrings(html_source);

  // Startup subsection exists only when OsSettingsRevampWayfinding is disabled.
  if (startup_subsection_) {
    startup_subsection_->AddLoadTimeData(html_source);
  }
}

void AppsSection::AddHandlers(content::WebUI* web_ui) {
  web_ui->AddMessageHandler(
      std::make_unique<AndroidAppsHandler>(profile(), app_service_proxy_));
  if (arc::IsArcVmEnabled()) {
    web_ui->AddMessageHandler(std::make_unique<GuestOsHandler>(profile()));
  }

  if (ShowPluginVm(profile(), *pref_service_)) {
    web_ui->AddMessageHandler(std::make_unique<GuestOsHandler>(profile()));
    web_ui->AddMessageHandler(std::make_unique<PluginVmHandler>(profile()));
  }
}

int AppsSection::GetSectionNameMessageId() const {
  return IDS_SETTINGS_APPS_TITLE;
}

mojom::Section AppsSection::GetSection() const {
  return mojom::Section::kApps;
}

mojom::SearchResultIcon AppsSection::GetSectionIcon() const {
  return mojom::SearchResultIcon::kAppsGrid;
}

const char* AppsSection::GetSectionPath() const {
  return mojom::kAppsSectionPath;
}

bool AppsSection::LogMetric(mojom::Setting setting, base::Value& value) const {
  if (setting == mojom::Setting::kDoNotDisturbOnOff) {
    base::UmaHistogramBoolean("ChromeOS.Settings.Apps.DoNotDisturbOnOff",
                              value.GetBool());
    return true;
  }
  if (setting == mojom::Setting::kAppNotificationOnOff) {
    base::UmaHistogramBoolean(
        "ChromeOS.Settings.NotificationPage.PermissionOnOff", value.GetBool());
    return true;
  }
  return false;
}

void AppsSection::RegisterHierarchy(HierarchyGenerator* generator) const {
  generator->RegisterTopLevelSetting(mojom::Setting::kTurnOnPlayStore);
  generator->RegisterTopLevelSetting(mojom::Setting::kAppParentalControls);

  // Manage apps.
  generator->RegisterTopLevelSubpage(IDS_SETTINGS_APPS_LINK_TEXT,
                                     mojom::Subpage::kAppManagement,
                                     mojom::SearchResultIcon::kAppsGrid,
                                     mojom::SearchResultDefaultRank::kMedium,
                                     mojom::kAppManagementSubpagePath);

  // App Notifications
  generator->RegisterTopLevelSubpage(IDS_SETTINGS_APP_NOTIFICATIONS_LINK_TEXT,
                                     mojom::Subpage::kAppNotifications,
                                     mojom::SearchResultIcon::kAppsGrid,
                                     mojom::SearchResultDefaultRank::kMedium,
                                     mojom::kAppNotificationsSubpagePath);
  generator->RegisterNestedSubpage(
      IDS_OS_SETTINGS_TAG_APP_NOTIFICATIONS_MANAGER,
      mojom::Subpage::kAppNotificationsManager,
      mojom::Subpage::kAppNotifications, mojom::SearchResultIcon::kAppsGrid,
      mojom::SearchResultDefaultRank::kMedium,
      mojom::kAppNotificationsManagerSubpagePath);
  generator->RegisterNestedSetting(mojom::Setting::kDoNotDisturbOnOff,
                                   mojom::Subpage::kAppNotifications);
  generator->RegisterNestedSetting(mojom::Setting::kAppBadgingOnOff,
                                   mojom::Subpage::kAppNotifications);
  generator->RegisterNestedSetting(mojom::Setting::kAppNotificationOnOff,
                                   mojom::Subpage::kAppNotifications);

  // Manage Isolated Web Apps
  generator->RegisterTopLevelSubpage(IDS_SETTINGS_APPS_LINK_TEXT,
                                     mojom::Subpage::kManageIsolatedWebApps,
                                     mojom::SearchResultIcon::kAppsGrid,
                                     mojom::SearchResultDefaultRank::kMedium,
                                     mojom::kManageIsolatedWebAppsSubpagePath);
  generator->RegisterNestedSetting(mojom::Setting::kEnableIsolatedWebAppsOnOff,
                                   mojom::Subpage::kManageIsolatedWebApps);

  // Note: The subpage name in the UI is updated dynamically based on the app
  // being shown, but we use a generic "App details" string here.
  generator->RegisterNestedSubpage(
      IDS_SETTINGS_APP_DETAILS_TITLE, mojom::Subpage::kAppDetails,
      mojom::Subpage::kAppManagement, mojom::SearchResultIcon::kAppsGrid,
      mojom::SearchResultDefaultRank::kMedium, mojom::kAppDetailsSubpagePath);
  generator->RegisterNestedSetting(mojom::Setting::kAppPinToShelfOnOff,
                                   mojom::Subpage::kAppDetails);
  generator->RegisterNestedSetting(mojom::Setting::kAppResizeLockOnOff,
                                   mojom::Subpage::kAppDetails);

  generator->RegisterNestedSubpage(
      IDS_SETTINGS_GUEST_OS_SHARED_PATHS, mojom::Subpage::kPluginVmSharedPaths,
      mojom::Subpage::kAppManagement, mojom::SearchResultIcon::kAppsGrid,
      mojom::SearchResultDefaultRank::kMedium,
      mojom::kPluginVmSharedPathsSubpagePath);
  generator->RegisterNestedSubpage(
      IDS_SETTINGS_GUEST_OS_SHARED_USB_DEVICES_LABEL,
      mojom::Subpage::kPluginVmUsbPreferences, mojom::Subpage::kAppManagement,
      mojom::SearchResultIcon::kAppsGrid,
      mojom::SearchResultDefaultRank::kMedium,
      mojom::kPluginVmUsbPreferencesSubpagePath);

  // Google Play Store.
  generator->RegisterTopLevelSubpage(IDS_SETTINGS_ANDROID_APPS_LABEL,
                                     mojom::Subpage::kGooglePlayStore,
                                     mojom::SearchResultIcon::kGooglePlay,
                                     mojom::SearchResultDefaultRank::kMedium,
                                     mojom::kGooglePlayStoreSubpagePath);
  static constexpr mojom::Setting kGooglePlayStoreSettings[] = {
      mojom::Setting::kManageAndroidPreferences,
      mojom::Setting::kRemovePlayStore,
  };
  RegisterNestedSettingBulk(mojom::Subpage::kGooglePlayStore,
                            kGooglePlayStoreSettings, generator);
  generator->RegisterTopLevelAltSetting(
      mojom::Setting::kManageAndroidPreferences);

  generator->RegisterNestedSubpage(
      IDS_SETTINGS_GUEST_OS_SHARED_USB_DEVICES_LABEL,
      mojom::Subpage::kArcVmUsbPreferences, mojom::Subpage::kGooglePlayStore,
      mojom::SearchResultIcon::kGooglePlay,
      mojom::SearchResultDefaultRank::kMedium,
      mojom::kArcVmUsbPreferencesSubpagePath);

  // On-device parental controls for apps
  generator->RegisterTopLevelSubpage(
      IDS_OS_SETTINGS_APP_PARENTAL_CONTROLS_LABEL,
      mojom::Subpage::kAppParentalControls,
      mojom::SearchResultIcon::kAppsParentalControls,
      mojom::SearchResultDefaultRank::kMedium,
      mojom::kAppParentalControlsSubpagePath);

  // Startup subsection exists only when OsSettingsRevampWayfinding is disabled.
  if (startup_subsection_) {
    startup_subsection_->RegisterHierarchy(generator);
  }
}

void AppsSection::OnAppRegistered(const std::string& app_id,
                                  const ArcAppListPrefs::AppInfo& app_info) {
  UpdateAndroidSearchTags();
}

void AppsSection::AddAndroidAppStrings(content::WebUIDataSource* html_source) {
  const bool kIsRevampEnabled =
      ash::features::IsOsSettingsRevampWayfindingEnabled();

  webui::LocalizedString kLocalizedStrings[] = {
      {"androidAppsPageLabel", kIsRevampEnabled
                                   ? IDS_OS_SETTINGS_REVAMP_ANDROID_APPS_LABEL
                                   : IDS_SETTINGS_ANDROID_APPS_LABEL},
      {"androidAppsEnable", IDS_SETTINGS_TURN_ON},
      {"androidAppsManageApps",
       kIsRevampEnabled ? IDS_OS_SETTINGS_REVAMP_ANDROID_APPS_MANAGE_APPS
                        : IDS_SETTINGS_ANDROID_APPS_MANAGE_APPS},
      {"androidAppsRemove", kIsRevampEnabled
                                ? IDS_OS_SETTINGS_REVAMP_ANDROID_APPS_REMOVE
                                : IDS_SETTINGS_ANDROID_APPS_REMOVE},
      {"androidAppsRemoveButton", IDS_SETTINGS_ANDROID_APPS_REMOVE_BUTTON},
      {"androidAppsDisableDialogTitle",
       kIsRevampEnabled
           ? IDS_OS_SETTINGS_REVAMP_ANDROID_APPS_DISABLE_DIALOG_TITLE
           : IDS_SETTINGS_ANDROID_APPS_DISABLE_DIALOG_TITLE},
      {"androidAppsDisableDialogMessage",
       kIsRevampEnabled
           ? IDS_OS_SETTINGS_REVAMP_ANDROID_APPS_DISABLE_DIALOG_MESSAGE
           : IDS_SETTINGS_ANDROID_APPS_DISABLE_DIALOG_MESSAGE},
      {"androidAppsDisableDialogRemove",
       kIsRevampEnabled ? IDS_SETTINGS_ANDROID_APPS_REMOVE_BUTTON
                        : IDS_SETTINGS_ANDROID_APPS_DISABLE_DIALOG_REMOVE},
      {"arcvmSharedUsbDevicesDescription",
       IDS_SETTINGS_APPS_ARC_VM_SHARED_USB_DEVICES_DESCRIPTION},
      {"androidAppsEnableButtonRole",
       IDS_SETTINGS_ANDROID_APPS_ENABLE_BUTTON_ROLE},
      {"androidOpenGooglePlay", IDS_OS_SETTINGS_REVAMP_OPEN_GOOGLE_PLAY},
      {"appLanguageDeviceLanguageLabel",
       IDS_OS_SETTINGS_APP_LANGUAGE_DEVICE_LANGUAGE_LABEL},
      {"appLanguageDialogTitle", IDS_OS_SETTINGS_APP_LANGUAGE_DIALOG_TITLE},
      {"appLanguageDialogSearchPlaceholderText",
       IDS_OS_SETTINGS_APP_LANGUAGE_DIALOG_SEARCH_PLACEHOLDER_TEXT},
      {"appLanguageDialogSuggestedLabel",
       IDS_OS_SETTINGS_APP_LANGUAGE_DIALOG_SUGGESTED_LABEL},
      {"appLanguageDialogAllLanguagesLabel",
       IDS_OS_SETTINGS_APP_LANGUAGE_DIALOG_ALL_LANGUAGES_LABEL},
      {"appLanguageDialogUpdateButtonText",
       IDS_OS_SETTINGS_APP_LANGUAGE_DIALOG_UPDATE_BUTTON_TEXT},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);
  html_source->AddLocalizedString("androidAppsPageTitle",
                                  arc::IsPlayStoreAvailable()
                                      ? IDS_SETTINGS_ANDROID_APPS_TITLE
                                      : IDS_SETTINGS_ANDROID_SETTINGS_TITLE);
  html_source->AddString(
      "androidAppsSubtext",
      kIsRevampEnabled
          ? l10n_util::GetStringFUTF16(
                IDS_OS_SETTINGS_REVAMP_ANDROID_APPS_SUBTEXT,
                GetHelpUrlWithBoard(chrome::kAndroidAppsLearnMoreURL))
          : l10n_util::GetStringFUTF16(
                IDS_SETTINGS_ANDROID_APPS_SUBTEXT, ui::GetChromeOSDeviceName(),
                GetHelpUrlWithBoard(chrome::kAndroidAppsLearnMoreURL)));
  if (kIsRevampEnabled) {
    html_source->AddLocalizedString(
        "androidAppsSubtextDescription",
        IDS_OS_SETTINGS_REVAMP_ANDROID_APPS_SUBTEXT_DESCRIPTION);
  } else {
    html_source->AddString("androidAppsSubtextDescription",
                           l10n_util::GetStringFUTF16(
                               IDS_SETTINGS_ANDROID_APPS_SUBTEXT_DESCRIPTION,
                               ui::GetChromeOSDeviceName()));
  }
}

void AppsSection::AddPluginVmLoadTimeData(
    content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"pluginVmSharedPathsInstructionsAdd",
       IDS_SETTINGS_APPS_PLUGIN_VM_SHARED_PATHS_INSTRUCTIONS_ADD},
      {"pluginVmSharedPathsRemoveFailureDialogMessage",
       IDS_SETTINGS_APPS_PLUGIN_VM_SHARED_PATHS_REMOVE_FAILURE_DIALOG_MESSAGE},
      {"pluginVmSharedUsbDevicesDescription",
       IDS_SETTINGS_APPS_PLUGIN_VM_SHARED_USB_DEVICES_DESCRIPTION},
      {"pluginVmPermissionDialogCameraLabel",
       IDS_SETTINGS_APPS_PLUGIN_VM_PERMISSION_DIALOG_CAMERA_LABEL},
      {"pluginVmPermissionDialogMicrophoneLabel",
       IDS_SETTINGS_APPS_PLUGIN_VM_PERMISSION_DIALOG_MICROPHONE_LABEL},
      {"pluginVmPermissionDialogRelaunchButton",
       IDS_SETTINGS_APPS_PLUGIN_VM_PERMISSION_DIALOG_RELAUNCH_BUTTON},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);

  html_source->AddBoolean("isPluginVmAvailable",
                          ShowPluginVm(profile(), *pref_service_));
  html_source->AddString(
      "pluginVmSharedPathsInstructionsLocate",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_APPS_PLUGIN_VM_SHARED_PATHS_INSTRUCTIONS_LOCATE,
          base::UTF8ToUTF16(plugin_vm::kChromeOSBaseDirectoryDisplayText)));
}

void AppsSection::UpdateAndroidSearchTags() {
  SearchTagRegistry::ScopedTagUpdater updater = registry()->StartUpdate();

  updater.RemoveSearchTags(GetAndroidNoPlayStoreSearchConcepts());
  updater.RemoveSearchTags(GetAndroidPlayStoreDisabledSearchConcepts());
  updater.RemoveSearchTags(GetAndroidPlayStoreSearchConcepts());
  updater.RemoveSearchTags(GetAndroidSettingsSearchConcepts());

  if (!arc::IsPlayStoreAvailable()) {
    updater.AddSearchTags(GetAndroidNoPlayStoreSearchConcepts());
    return;
  }

  if (!arc::IsArcPlayStoreEnabledForProfile(profile())) {
    updater.AddSearchTags(GetAndroidPlayStoreDisabledSearchConcepts());
    return;
  }

  updater.AddSearchTags(GetAndroidPlayStoreSearchConcepts());

  if (arc_app_list_prefs_ &&
      arc_app_list_prefs_->IsRegistered(arc::kSettingsAppId)) {
    updater.AddSearchTags(GetAndroidSettingsSearchConcepts());
  }
}

void AppsSection::OnQuietModeChanged(bool in_quiet_mode) {
  if (!features::IsAppNotificationsPageEnabled()) {
    return;
  }

  const bool kIsRevampEnabled =
      ash::features::IsOsSettingsRevampWayfindingEnabled();
  SearchTagRegistry::ScopedTagUpdater updater = registry()->StartUpdate();

  updater.RemoveSearchTags(GetTurnOnAppNotificationSearchConcepts());
  updater.RemoveSearchTags(GetTurnOffAppNotificationSearchConcepts());
  updater.RemoveSearchTags(GetAppNotificationsSearchConcepts());
  if (kIsRevampEnabled) {
    updater.RemoveSearchTags(GetAppNotificationsManagerSearchConcepts());
  }
  updater.RemoveSearchTags(GetAppBadgingSearchConcepts());

  updater.AddSearchTags(GetAppNotificationsSearchConcepts());
  if (kIsRevampEnabled) {
    updater.AddSearchTags(GetAppNotificationsManagerSearchConcepts());
  }
  updater.AddSearchTags(GetAppBadgingSearchConcepts());

  if (!MessageCenterAsh::Get()->IsQuietMode()) {
    updater.AddSearchTags(GetTurnOnAppNotificationSearchConcepts());
    return;
  }

  updater.AddSearchTags(GetTurnOffAppNotificationSearchConcepts());
}

}  // namespace ash::settings
