// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/webui/extensions/extensions_ui.h"

#include <memory>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/manifest_v2_experiment_manager.h"
#include "chrome/browser/extensions/mv2_experiment_stage.h"
#include "chrome/browser/extensions/permissions_url_constants.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/favicon_source.h"
#include "chrome/browser/ui/webui/managed_ui_handler.h"
#include "chrome/browser/ui/webui/metrics_handler.h"
#include "chrome/browser/ui/webui/page_not_available_for_guest/page_not_available_for_guest_ui.h"
#include "chrome/browser/ui/webui/plural_string_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/extensions_resources.h"
#include "chrome/grit/extensions_resources_map.h"
#include "chrome/grit/generated_resources.h"
#include "components/favicon_base/favicon_url_parser.h"
#include "components/google/core/common/google_util.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/extension_urls.h"
#include "extensions/grit/extensions_browser_resources.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/web_ui_util.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/crosapi/browser_util.h"
#endif

namespace extensions {

namespace {

constexpr char kInDevModeKey[] = "inDevMode";
constexpr char kShowActivityLogKey[] = "showActivityLog";
constexpr char kLoadTimeClassesKey[] = "loadTimeClasses";
constexpr char kEnableEnhancedSiteControls[] = "enableEnhancedSiteControls";

std::string GetLoadTimeClasses(bool in_dev_mode) {
  return in_dev_mode ? "in-dev-mode" : std::string();
}

content::WebUIDataSource* CreateAndAddExtensionsSource(Profile* profile,
                                                       bool in_dev_mode) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUIExtensionsHost);
  webui::SetupWebUIDataSource(
      source, base::make_span(kExtensionsResources, kExtensionsResourcesSize),
      IDR_EXTENSIONS_EXTENSIONS_HTML);

  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      // Add common strings.
      {"add", IDS_ADD},
      {"back", IDS_ACCNAME_BACK},
      {"cancel", IDS_CANCEL},
      {"close", IDS_CLOSE},
      {"clear", IDS_CLEAR},
      {"confirm", IDS_CONFIRM},
      {"controlledSettingChildRestriction",
       IDS_CONTROLLED_SETTING_CHILD_RESTRICTION},
      {"controlledSettingPolicy", IDS_CONTROLLED_SETTING_POLICY},
      {"done", IDS_DONE},
      {"learnMore", IDS_LEARN_MORE},
      {"menu", IDS_MENU},
      {"noSearchResults", IDS_SEARCH_NO_RESULTS},
      {"ok", IDS_OK},
      {"save", IDS_SAVE},
      {"searchResultsPlural", IDS_SEARCH_RESULTS_PLURAL},
      {"searchResultsSingular", IDS_SEARCH_RESULTS_SINGULAR},

      // Multi-use strings defined in extensions_strings.grdp.
      {"remove", IDS_EXTENSIONS_REMOVE},
      {"moreOptions", IDS_EXTENSIONS_MORE_OPTIONS},

      // Add extension-specific strings.
      {"title", IDS_MANAGE_EXTENSIONS_SETTING_WINDOWS_TITLE},
      {"toolbarTitle", IDS_EXTENSIONS_TOOLBAR_TITLE},
      {"mainMenu", IDS_EXTENSIONS_MENU_BUTTON_LABEL},
      {"search", IDS_EXTENSIONS_SEARCH},
      {"clearSearch", IDS_CLEAR_SEARCH},
      {"sidebarExtensions", IDS_EXTENSIONS_SIDEBAR_EXTENSIONS},
      {"appsTitle", IDS_EXTENSIONS_APPS_TITLE},
      {"noExtensionsOrApps", IDS_EXTENSIONS_NO_INSTALLED_ITEMS},
      {"noDescription", IDS_EXTENSIONS_NO_DESCRIPTION},
      {"viewInStore", IDS_EXTENSIONS_ITEM_CHROME_WEB_STORE},
      {"extensionWebsite", IDS_EXTENSIONS_ITEM_EXTENSION_WEBSITE},
      {"dropToInstall", IDS_EXTENSIONS_INSTALL_DROP_TARGET},
      {"editSitePermissionsAllowAllExtensions",
       IDS_EXTENSIONS_EDIT_SITE_PERMISSIONS_ALLOW_ALL_EXTENSIONS},
      {"editSitePermissionsCustomizePerExtension",
       IDS_EXTENSIONS_EDIT_SITE_PERMISSIONS_CUSTOMIZE_PER_EXTENSION},
      {"editSitePermissionsRestrictExtensions",
       IDS_EXTENSIONS_EDIT_SITE_PERMISSIONS_RESTRICT_EXTENSIONS},
      {"enableToggleTooltipDisabled",
       IDS_EXTENSIONS_ENABLE_TOGGLE_TOOLTIP_DISABLED},
      {"enableToggleTooltipEnabled",
       IDS_EXTENSIONS_ENABLE_TOGGLE_TOOLTIP_ENABLED},
      {"enableToggleTooltipEnabledWithSiteAccess",
       IDS_EXTENSIONS_ENABLE_TOGGLE_TOOLTIP_ENABLED_WITH_SITE_ACCESS},
      {"errorsPageHeading", IDS_EXTENSIONS_ERROR_PAGE_HEADING},
      {"clearActivities", IDS_EXTENSIONS_CLEAR_ACTIVITIES},
      {"clearAll", IDS_EXTENSIONS_ERROR_CLEAR_ALL},
      {"clearEntry", IDS_EXTENSIONS_A11Y_CLEAR_ENTRY},
      {"logLevel", IDS_EXTENSIONS_LOG_LEVEL_INFO},
      {"warnLevel", IDS_EXTENSIONS_LOG_LEVEL_WARN},
      {"errorLevel", IDS_EXTENSIONS_LOG_LEVEL_ERROR},
      {"anonymousFunction", IDS_EXTENSIONS_ERROR_ANONYMOUS_FUNCTION},
      {"errorContext", IDS_EXTENSIONS_ERROR_CONTEXT},
      {"errorContextUnknown", IDS_EXTENSIONS_ERROR_CONTEXT_UNKNOWN},
      {"safetyCheckExtensionsDetailPagePrimaryLabel",
       IDS_EXTENSIONS_SAFETY_CHECK_PRIMARY_LABEL},
      {"safetyCheckExtensionsKeep", IDS_CONFIRM_DOWNLOAD},
      {"stackTrace", IDS_EXTENSIONS_ERROR_STACK_TRACE},
      // TODO(dpapad): Unify with Settings' IDS_SETTINGS_WEB_STORE.
      {"sidebarDiscoverMore", IDS_EXTENSIONS_SIDEBAR_DISCOVER_MORE},
      {"keyboardShortcuts", IDS_EXTENSIONS_SIDEBAR_KEYBOARD_SHORTCUTS},
      {"incognitoInfoWarning", IDS_EXTENSIONS_INCOGNITO_WARNING},
      {"hostPermissionsDescription",
       IDS_EXTENSIONS_HOST_PERMISSIONS_DESCRIPTION},
      {"permissionsLearnMoreLabel",
       IDS_EXTENSIONS_PERMISSIONS_LEARN_MORE_A11Y_LABEL},
      {"hostPermissionsEdit", IDS_EXTENSIONS_HOST_PERMISSIONS_EDIT},
      {"hostPermissionsHeading", IDS_EXTENSIONS_ITEM_HOST_PERMISSIONS_HEADING},
      {"newHostPermissionsHeading",
       IDS_EXTENSIONS_NEW_HOST_PERMISSIONS_HEADING},
      {"hostPermissionsSubHeading",
       IDS_EXTENSIONS_HOST_PERMISSIONS_SUB_HEADING},
      {"hostAccessAskOnEveryVisit",
       IDS_EXTENSIONS_HOST_ACCESS_ASK_ON_EVERY_VISIT},
      {"hostAccessOnClick", IDS_EXTENSIONS_HOST_ACCESS_ON_CLICK},
      {"hostAccessOnSpecificSites",
       IDS_EXTENSIONS_HOST_ACCESS_ON_SPECIFIC_SITES},
      {"hostAccessAllowOnSpecificSites",
       IDS_EXTENSIONS_HOST_ACCESS_ALLOW_ON_SPECIFIC_SITES},
      {"hostAccessOnAllSites", IDS_EXTENSIONS_HOST_ACCESS_ON_ALL_SITES},
      {"hostAccessAllowOnAllSites",
       IDS_EXTENSIONS_HOST_ACCESS_ALLOW_ON_ALL_SITES},
      {"hostAllowedHosts", IDS_EXTENSIONS_ITEM_ALLOWED_HOSTS},
      {"itemId", IDS_EXTENSIONS_ITEM_ID},
      {"itemInspectViews", IDS_EXTENSIONS_ITEM_INSPECT_VIEWS},
      // NOTE: This text reads "<n> more". It's possible that it should be using
      // a plural string instead. Unfortunately, this is non-trivial since we
      // don't expose that capability to JS yet. Since we don't know it's a
      // problem, use a simple placeholder for now.
      {"itemInspectViewsExtra", IDS_EXTENSIONS_ITEM_INSPECT_VIEWS_EXTRA},
      {"noActiveViews", IDS_EXTENSIONS_ITEM_NO_ACTIVE_VIEWS},
      {"itemAllowIncognito", IDS_EXTENSIONS_ITEM_ALLOW_INCOGNITO},
      {"itemDescriptionLabel", IDS_EXTENSIONS_ITEM_DESCRIPTION},
      {"itemDependencies", IDS_EXTENSIONS_ITEM_DEPENDENCIES},
      {"itemDependentEntry", IDS_EXTENSIONS_DEPENDENT_ENTRY},
      {"itemDetails", IDS_EXTENSIONS_ITEM_DETAILS},
      {"itemDetailsBackButtonAriaLabel",
       IDS_EXTENSIONS_DETAILS_BACK_BUTTON_ARIA_LABEL},
      {"itemDetailsBackButtonRoleDescription",
       IDS_EXTENSIONS_DETAILS_BACK_BUTTON_ARIA_ROLE_DESCRIPTION},
      {"itemErrors", IDS_EXTENSIONS_ITEM_ERRORS},
      {"accessibilityErrorLine", IDS_EXTENSIONS_ACCESSIBILITY_ERROR_LINE},
      {"accessibilityErrorMultiLine",
       IDS_EXTENSIONS_ACCESSIBILITY_ERROR_MULTI_LINE},
      {"activityLogPageHeading", IDS_EXTENSIONS_ACTIVITY_LOG_PAGE_HEADING},
      {"activityLogTypeColumn", IDS_EXTENSIONS_ACTIVITY_LOG_TYPE_COLUMN},
      {"activityLogNameColumn", IDS_EXTENSIONS_ACTIVITY_LOG_NAME_COLUMN},
      {"activityLogCountColumn", IDS_EXTENSIONS_ACTIVITY_LOG_COUNT_COLUMN},
      {"activityLogTimeColumn", IDS_EXTENSIONS_ACTIVITY_LOG_TIME_COLUMN},
      {"activityLogSearchLabel", IDS_EXTENSIONS_ACTIVITY_LOG_SEARCH_LABEL},
      {"activityLogHistoryTabHeading",
       IDS_EXTENSIONS_ACTIVITY_LOG_HISTORY_TAB_HEADING},
      {"activityLogStreamTabHeading",
       IDS_EXTENSIONS_ACTIVITY_LOG_STREAM_TAB_HEADING},
      {"startActivityStream", IDS_EXTENSIONS_START_ACTIVITY_STREAM},
      {"stopActivityStream", IDS_EXTENSIONS_STOP_ACTIVITY_STREAM},
      {"parentDisabledPermissions", IDS_EXTENSIONS_PERMISSIONS_OFF},
      {"emptyStreamStarted", IDS_EXTENSIONS_EMPTY_STREAM_STARTED},
      {"emptyStreamStopped", IDS_EXTENSIONS_EMPTY_STREAM_STOPPED},
      {"activityArgumentsHeading", IDS_EXTENSIONS_ACTIVITY_ARGUMENTS_HEADING},
      {"webRequestInfoHeading", IDS_EXTENSIONS_WEB_REQUEST_INFO_HEADING},
      {"activityLogMoreActionsLabel",
       IDS_EXTENSIONS_ACTIVITY_LOG_MORE_ACTIONS_LABEL},
      {"activityLogExpandAll", IDS_EXTENSIONS_ACTIVITY_LOG_EXPAND_ALL},
      {"activityLogCollapseAll", IDS_EXTENSIONS_ACTIVITY_LOG_COLLAPSE_ALL},
      {"activityLogExportHistory", IDS_EXTENSIONS_ACTIVITY_LOG_EXPORT_HISTORY},
      {"appIcon", IDS_EXTENSIONS_APP_ICON},
      {"extensionIcon", IDS_EXTENSIONS_EXTENSION_ICON},
      {"extensionA11yAssociation", IDS_EXTENSIONS_EXTENSION_A11Y_ASSOCIATION},
      {"extensionsSectionHeader", IDS_EXTENSIONS_SECTION_HEADER},
      {"itemIdHeading", IDS_EXTENSIONS_ITEM_ID_HEADING},
      {"extensionEnabled", IDS_EXTENSIONS_EXTENSION_ENABLED},
      {"appEnabled", IDS_EXTENSIONS_APP_ENABLED},
      {"installWarnings", IDS_EXTENSIONS_INSTALL_WARNINGS},
      {"itemExtensionPath", IDS_EXTENSIONS_PATH},
      {"itemOff", IDS_EXTENSIONS_ITEM_OFF},
      {"itemOn", IDS_EXTENSIONS_ITEM_ON},
      {"itemOptions", IDS_EXTENSIONS_ITEM_OPTIONS},
      {"itemPermissions", IDS_EXTENSIONS_ITEM_PERMISSIONS},
      {"itemPermissionsEmpty", IDS_EXTENSIONS_ITEM_PERMISSIONS_EMPTY},
      {"itemPermissionsAndSiteAccessEmpty",
       IDS_EXTENSIONS_ITEM_PERMISSIONS_AND_SITE_ACCESS_EMPTY},
      {"itemPinToToolbar", IDS_EXTENSIONS_ITEM_PIN_TO_TOOLBAR},
      {"itemRemoveExtension", IDS_EXTENSIONS_ITEM_REMOVE_EXTENSION},
      {"itemShowAccessRequestsInToolbar",
       IDS_EXTENSIONS_ITEM_SHOW_ACCESS_REQUESTS_IN_TOOLBAR},
      {"itemShowAccessRequestsLearnMore",
       IDS_EXTENSIONS_ACCESS_REQUESTS_LEARN_MORE},
      {"itemSiteAccess", IDS_EXTENSIONS_ITEM_SITE_ACCESS},
      {"itemSiteAccessAddHost", IDS_EXTENSIONS_ITEM_SITE_ACCESS_ADD_HOST},
      {"itemSiteAccessEmpty", IDS_EXTENSIONS_ITEM_SITE_ACCESS_EMPTY},
      {"itemSource", IDS_EXTENSIONS_ITEM_SOURCE},
      {"itemSourceInstalledByDefault",
       IDS_EXTENSIONS_ITEM_SOURCE_INSTALLED_BY_DEFAULT},
      {"itemSourcePolicy", IDS_EXTENSIONS_ITEM_SOURCE_POLICY},
      {"itemSourceSideloaded", IDS_EXTENSIONS_ITEM_SOURCE_SIDELOADED},
      {"itemSourceUnpacked", IDS_EXTENSIONS_ITEM_SOURCE_UNPACKED},
      {"itemSourceWebstore", IDS_EXTENSIONS_ITEM_SOURCE_WEBSTORE},
      {"itemVersion", IDS_EXTENSIONS_ITEM_VERSION},
      {"itemReloaded", IDS_EXTENSIONS_ITEM_RELOADED},
      {"itemReloading", IDS_EXTENSIONS_ITEM_RELOADING},
      // TODO(dpapad): Replace this with an Extensions specific string.
      {"itemSize", IDS_DIRECTORY_LISTING_SIZE},
      {"itemAllowOnFileUrls", IDS_EXTENSIONS_ALLOW_FILE_ACCESS},
      {"itemAllowOnAllSites", IDS_EXTENSIONS_ALLOW_ON_ALL_URLS},
      {"itemAllowOnFollowingSites", IDS_EXTENSIONS_ALLOW_ON_FOLLOWING_SITES},
      {"itemCollectErrors", IDS_EXTENSIONS_ENABLE_ERROR_COLLECTION},
      {"itemCorruptInstall", IDS_EXTENSIONS_CORRUPTED_EXTENSION},
      {"itemAllowlistWarning",
       IDS_EXTENSIONS_SAFE_BROWSING_CRX_ALLOWLIST_WARNING},
      {"itemAllowlistWarningLearnMoreLabel",
       IDS_EXTENSIONS_SAFE_BROWSING_CRX_ALLOWLIST_WARNING_LEARN_MORE},
      {"itemRepair", IDS_EXTENSIONS_REPAIR_CORRUPTED},
      {"itemReload", IDS_EXTENSIONS_RELOAD_TERMINATED},
      {"loadErrorCouldNotLoadManifest",
       IDS_EXTENSIONS_LOAD_ERROR_COULD_NOT_LOAD_MANIFEST},
      {"loadErrorHeading", IDS_EXTENSIONS_LOAD_ERROR_HEADING},
      {"loadErrorFileLabel", IDS_EXTENSIONS_LOAD_ERROR_FILE_LABEL},
      {"loadErrorErrorLabel", IDS_EXTENSIONS_LOAD_ERROR_ERROR_LABEL},
      {"loadErrorRetry", IDS_EXTENSIONS_LOAD_ERROR_RETRY},
      {"loadingActivities", IDS_EXTENSIONS_LOADING_ACTIVITIES},
      {"matchingRestrictedSitesAllow",
       IDS_EXTENSIONS_MATCHING_RESTRICTED_SITES_ALLOW},
      {"matchingRestrictedSitesTitle",
       IDS_EXTENSIONS_MATCHING_RESTRICTED_SITES_TITLE},
      {"matchingRestrictedSitesWarning",
       IDS_EXTENSIONS_MATCHING_RESTRICTED_SITES_WARNING},
      {"missingOrUninstalledExtension", IDS_MISSING_OR_UNINSTALLED_EXTENSION},
      {"noActivities", IDS_EXTENSIONS_NO_ACTIVITIES},
      {"noErrorsToShow", IDS_EXTENSIONS_ERROR_NO_ERRORS_CODE_MESSAGE},
      {"opensInNewTab", IDS_EXTENSIONS_OPENS_IN_NEW_TAB},
      {"removeSitesDialogTitle", IDS_EXTENSIONS_REMOVE_SITES_DIALOG_TITLE},
      {"runtimeHostsDialogInputError",
       IDS_EXTENSIONS_RUNTIME_HOSTS_DIALOG_INPUT_ERROR},
      {"runtimeHostsDialogInputLabel",
       IDS_EXTENSIONS_RUNTIME_HOSTS_DIALOG_INPUT_LABEL},
      {"runtimeHostsDialogTitle", IDS_EXTENSIONS_RUNTIME_HOSTS_DIALOG_TITLE},
      {"packDialogTitle", IDS_EXTENSIONS_PACK_DIALOG_TITLE},
      {"packDialogWarningTitle", IDS_EXTENSIONS_PACK_DIALOG_WARNING_TITLE},
      {"packDialogErrorTitle", IDS_EXTENSIONS_PACK_DIALOG_ERROR_TITLE},
      {"packDialogProceedAnyway", IDS_EXTENSIONS_PACK_DIALOG_PROCEED_ANYWAY},
      {"packDialogBrowse", IDS_EXTENSIONS_PACK_DIALOG_BROWSE_BUTTON},
      {"packDialogExtensionRoot",
       IDS_EXTENSIONS_PACK_DIALOG_EXTENSION_ROOT_LABEL},
      {"packDialogKeyFile", IDS_EXTENSIONS_PACK_DIALOG_KEY_FILE_LABEL},
      {"packDialogContent", IDS_EXTENSION_PACK_DIALOG_HEADING},
      {"packDialogConfirm", IDS_EXTENSIONS_PACK_DIALOG_CONFIRM_BUTTON},
      {"publishedInStoreRequiredByPolicy",
       IDS_EXTENSIONS_DISABLED_PUBLISHED_IN_STORE_REQUIRED_BY_POLICY},
      {"sitePermissions", IDS_EXTENSIONS_SITE_PERMISSIONS},
      {"sitePermissionsAllSitesPageTitle",
       IDS_EXTENSIONS_SITE_PERMISSIONS_ALL_SITES_PAGE_TITLE},
      {"sitePermissionsAllSitesExtensionCount",
       // TODO(crbug.com/40251278): Convert the below two strings to use plural
       // strings.
       IDS_EXTENSIONS_SITE_PERMISSIONS_ALL_SITES_EXTENSION_COUNT},
      {"sitePermissionsAllSitesOneExtension",
       IDS_EXTENSIONS_SITE_PERMISSIONS_ALL_SITES_ONE_EXTENSION},
      {"sitePermissionsAlwaysOnAllSites",
       IDS_EXTENSIONS_SITE_PERMISSIONS_ALWAYS_ON_ALL_SITES},
      {"sitePermissionsAlwaysOnThisSite",
       IDS_EXTENSIONS_SITE_PERMISSIONS_ALWAYS_ON_THIS_SITE},
      {"sitePermissionsAskOnEveryVisit",
       IDS_EXTENSIONS_SITE_PERMISSIONS_ASK_ON_EVERY_VISIT},
      {"sitePermissionsPageTitle", IDS_EXTENSIONS_SITE_PERMISSIONS_PAGE_TITLE},
      {"sitePermissionsAddSiteDialogTitle",
       IDS_EXTENSIONS_SITE_PERMISSIONS_ADD_SITE_DIALOG_TITLE},
      {"sitePermissionsEditSiteDialogTitle",
       IDS_EXTENSIONS_SITE_PERMISSIONS_EDIT_SITE_DIALOG_TITLE},
      {"sitePermissionsDialogInputError",
       IDS_EXTENSIONS_SITE_PERMISSIONS_DIALOG_INPUT_ERROR},
      {"sitePermissionsDialogInputLabel",
       IDS_EXTENSIONS_SITE_PERMISSIONS_DIALOG_INPUT_LABEL},
      {"sitePermissionsEditPermissions",
       IDS_EXTENSIONS_SITE_PERMISSIONS_EDIT_PERMISSIONS},
      {"sitePermissionsEditPermissionsDialogTitle",
       IDS_EXTENSIONS_SITE_PERMISSIONS_EDIT_PERMISSIONS_DIALOG_TITLE},
      {"sitePermissionsEditUrl", IDS_EXTENSIONS_SITE_PERMISSIONS_EDIT_URL},
      {"sitePermissionsIncludesSubdomains",
       IDS_EXTENSIONS_SITE_PERMISSIONS_INCLUDES_SUBDOMAINS},
      {"sitePermissionsViewAllSites",
       IDS_EXTENSIONS_SITE_PERMISSIONS_VIEW_ALL_SITES},
      {"siteSettings", IDS_EXTENSIONS_SITE_SETTINGS},
      {"permittedSites", IDS_EXTENSIONS_PERMITTED_SITES},
      {"restrictedSites", IDS_EXTENSIONS_RESTRICTED_SITES},
      {"noSitesAdded", IDS_EXTENSIONS_NO_SITES_ADDED},
      {"editShortcutInputLabel", IDS_EXTENSIONS_EDIT_SHORTCUT_INPUT_LABEL},
      {"editShortcutButtonLabel", IDS_EXTENSIONS_EDIT_SHORTCUT_BUTTON_LABEL},
      {"mv2DeprecationPanelTitle", IDS_EXTENSIONS_MV2_DEPRECATION_PANEL_TITLE},
      {"mv2DeprecationPanelDismissButton",
       IDS_EXTENSIONS_MV2_DEPRECATION_PANEL_DISMISS_BUTTON},
      {"mv2DeprecationPanelExtensionActionMenuLabel",
       IDS_EXTENSIONS_MV2_DEPRECATION_PANEL_ACTION_MENU_BUTTON_LABEL},
      {"mv2DeprecationPanelFindAlternativeButton",
       IDS_EXTENSIONS_MV2_DEPRECATION_PANEL_FIND_ALTERNATIVE_BUTTON},
      {"mv2DeprecationPanelFindAlternativeButtonAccLabel",
       IDS_EXTENSIONS_MV2_DEPRECATION_PANEL_FIND_ALTERNATIVE_BUTTON_ACC_LABEL},
      {"mv2DeprecationPanelRemoveButtonAccLabel",
       IDS_EXTENSIONS_MV2_DEPRECATION_PANEL_REMOVE_BUTTON_ACC_LABEL},
      {"mv2DeprecationPanelKeepForNowButton",
       IDS_EXTENSIONS_MV2_DEPRECATION_PANEL_KEEP_FOR_NOW_BUTTON},
      {"mv2DeprecationPanelRemoveExtensionButton", IDS_EXTENSIONS_UNINSTALL},
      {"mv2DeprecationMessageDisabledHeader",
       IDS_EXTENSIONS_MV2_DEPRECATION_MESSAGE_DISABLED_HEADER},
      {"mv2DeprecationMessageDisabledSubtitle",
       IDS_EXTENSIONS_MV2_DEPRECATION_MESSAGE_DISABLED_SUBTITLE},
      {"mv2DeprecationMessageRemoveButton",
       IDS_EXTENSIONS_MV2_DEPRECATION_MESSAGE_REMOVE_BUTTON},
      {"mv2DeprecationMessageWarningHeader",
       IDS_EXTENSIONS_MV2_DEPRECATION_MESSAGE_WARNING_HEADER},
      {"mv2DeprecationMessageWarningSubtitle",
       IDS_EXTENSIONS_MV2_DEPRECATION_MESSAGE_WARNING_SUBTITLE},
      {"mv2DeprecationUnsupportedExtensionOffText",
       IDS_EXTENSIONS_MV2_DEPRECATION_UNSUPPORTED_EXTENSION_OFF_TEXT},
      {"shortcutNotSet", IDS_EXTENSIONS_SHORTCUT_NOT_SET},
      {"shortcutScopeGlobal", IDS_EXTENSIONS_SHORTCUT_SCOPE_GLOBAL},
      {"shortcutScopeLabel", IDS_EXTENSIONS_SHORTCUT_SCOPE_LABEL},
      {"shortcutScopeInChrome", IDS_EXTENSIONS_SHORTCUT_SCOPE_IN_CHROME},
      {"shortcutSet", IDS_EXTENSIONS_SHORTCUT_SET},
      {"shortcutTypeAShortcut", IDS_EXTENSIONS_TYPE_A_SHORTCUT},
      {"shortcutIncludeStartModifier", IDS_EXTENSIONS_INCLUDE_START_MODIFIER},
      {"shortcutTooManyModifiers", IDS_EXTENSIONS_TOO_MANY_MODIFIERS},
      {"shortcutNeedCharacter", IDS_EXTENSIONS_NEED_CHARACTER},
      {"subpageArrowRoleDescription", IDS_EXTENSIONS_SUBPAGE_BUTTON},
      {"itemSuspiciousInstallLearnMore",
       IDS_EXTENSIONS_ADDED_WITHOUT_KNOWLEDGE_LEARN_MORE},
      {"toolbarDevMode", IDS_EXTENSIONS_DEVELOPER_MODE},
      {"toolbarLoadUnpacked", IDS_EXTENSIONS_TOOLBAR_LOAD_UNPACKED},
      {"toolbarLoadUnpackedDone", IDS_EXTENSIONS_TOOLBAR_LOAD_UNPACKED_DONE},
      {"toolbarPack", IDS_EXTENSIONS_TOOLBAR_PACK},
      {"toolbarUpdateNow", IDS_EXTENSIONS_TOOLBAR_UPDATE_NOW},
      {"toolbarUpdateNowTooltip", IDS_EXTENSIONS_TOOLBAR_UPDATE_NOW_TOOLTIP},
      {"toolbarUpdateDone", IDS_EXTENSIONS_TOOLBAR_UPDATE_DONE},
      {"toolbarUpdatingToast", IDS_EXTENSIONS_TOOLBAR_UPDATING_TOAST},
      {"updateRequiredByPolicy",
       IDS_EXTENSIONS_DISABLED_UPDATE_REQUIRED_BY_POLICY},
      {"viewActivityLog", IDS_EXTENSIONS_VIEW_ACTIVITY_LOG},
      {"viewBackgroundPage", IDS_EXTENSIONS_BACKGROUND_PAGE},
      {"viewIncognito", IDS_EXTENSIONS_VIEW_INCOGNITO},
      {"viewInactive", IDS_EXTENSIONS_VIEW_INACTIVE},
      {"viewIframe", IDS_EXTENSIONS_VIEW_IFRAME},
      {"viewServiceWorker", IDS_EXTENSIONS_SERVICE_WORKER_BACKGROUND},
      {"safetyCheckKeepExtension", IDS_EXTENSIONS_SC_KEEP_EXT},
      {"safetyCheckRemoveAll", IDS_EXTENSIONS_SC_REMOVE_ALL},
      {"safetyHubHeader", IDS_SETTINGS_SAFETY_HUB},
      {"safetyCheckRemoveButtonA11yLabel",
       IDS_EXTENSIONS_SC_REMOVE_BUTTON_A11Y_LABEL},
      {"safetyCheckOptionMenuA11yLabel",
       IDS_EXTENSIONS_SC_OPTION_MENU_A11Y_LABEL},
#if BUILDFLAG(IS_CHROMEOS_ASH)
      {"manageKioskApp", IDS_EXTENSIONS_MANAGE_KIOSK_APP},
      {"kioskAddApp", IDS_EXTENSIONS_KIOSK_ADD_APP},
      {"kioskAddAppHint", IDS_EXTENSIONS_KIOSK_ADD_APP_HINT},
      {"kioskEnableAutoLaunch", IDS_EXTENSIONS_KIOSK_ENABLE_AUTO_LAUNCH},
      {"kioskDisableAutoLaunch", IDS_EXTENSIONS_KIOSK_DISABLE_AUTO_LAUNCH},
      {"kioskAutoLaunch", IDS_EXTENSIONS_KIOSK_AUTO_LAUNCH},
      {"kioskInvalidApp", IDS_EXTENSIONS_KIOSK_INVALID_APP},
      {"kioskDisableBailout",
       IDS_EXTENSIONS_KIOSK_DISABLE_BAILOUT_SHORTCUT_LABEL},
      {"kioskDisableBailoutWarningTitle",
       IDS_EXTENSIONS_KIOSK_DISABLE_BAILOUT_SHORTCUT_WARNING_TITLE},
#endif
  };
  source->AddLocalizedStrings(kLocalizedStrings);

  // Add localized generic strings that need '&' to be removed from them.
  webui::AddLocalizedString(source, "edit", IDS_EDIT);

  source->AddString("errorLinesNotShownSingular",
                    l10n_util::GetPluralStringFUTF16(
                        IDS_EXTENSIONS_ERROR_LINES_NOT_SHOWN, 1));
  source->AddString("errorLinesNotShownPlural",
                    l10n_util::GetPluralStringFUTF16(
                        IDS_EXTENSIONS_ERROR_LINES_NOT_SHOWN, 2));
  source->AddString(
      "itemSuspiciousInstall",
      l10n_util::GetStringFUTF16(
          IDS_EXTENSIONS_ADDED_WITHOUT_KNOWLEDGE,
          l10n_util::GetStringUTF16(IDS_EXTENSION_WEB_STORE_TITLE)));
  source->AddString(
      "suspiciousInstallHelpUrl",
      base::ASCIIToUTF16(google_util::AppendGoogleLocaleParam(
                             GURL(chrome::kRemoveNonCWSExtensionURL),
                             g_browser_process->GetApplicationLocale())
                             .spec()));
  source->AddString("enhancedSafeBrowsingWarningHelpUrl",
                    chrome::kCwsEnhancedSafeBrowsingLearnMoreURL);
  source->AddString(
      "getMoreExtensionsUrl",
      base::ASCIIToUTF16(
          google_util::AppendGoogleLocaleParam(
              extension_urls::AppendUtmSource(
                  GURL(extension_urls::GetWebstoreExtensionsCategoryURL()),
                  extension_urls::kExtensionsSidebarUtmSource),
              g_browser_process->GetApplicationLocale())
              .spec()));
  source->AddString(
      "hostPermissionsLearnMoreLink",
      extension_permissions_constants::kRuntimeHostPermissionsHelpURL);
  source->AddBoolean(kInDevModeKey, in_dev_mode);
  source->AddBoolean(kShowActivityLogKey,
                     base::CommandLine::ForCurrentProcess()->HasSwitch(
                         ::switches::kEnableExtensionActivityLogging));

  source->AddString(kLoadTimeClassesKey, GetLoadTimeClasses(in_dev_mode));

  source->AddBoolean(
      "safetyCheckExtensionsReviewEnabled",
      base::FeatureList::IsEnabled(features::kSafetyCheckExtensions));

  source->AddBoolean(kEnableEnhancedSiteControls,
                     base::FeatureList::IsEnabled(
                         extensions_features::kExtensionsMenuAccessControl));
  source->AddString(
      "showAccessRequestsInToolbarLearnMoreLink",
      extension_permissions_constants::kShowAccessRequestsInToolbarHelpURL);
  source->AddBoolean(
      "enableUserPermittedSites",
      base::FeatureList::IsEnabled(
          extensions_features::kExtensionsMenuAccessControlWithPermittedSites));
  source->AddBoolean(
      "safetyCheckShowReviewPanel",
      base::FeatureList::IsEnabled(features::kSafetyCheckExtensions));
  source->AddBoolean("safetyHubShowReviewPanel",
                     base::FeatureList::IsEnabled(features::kSafetyHub));

  // MV2 deprecation.
  auto* mv2_experiment_manager = ManifestV2ExperimentManager::Get(profile);
  MV2ExperimentStage experiment_stage =
      mv2_experiment_manager->GetCurrentExperimentStage();
  source->AddInteger("MV2ExperimentStage", static_cast<int>(experiment_stage));
  source->AddBoolean(
      "MV2DeprecationNoticeDismissed",
      mv2_experiment_manager->DidUserAcknowledgeNoticeGlobally());

#if BUILDFLAG(IS_CHROMEOS_ASH)
  source->AddString(
      "kioskDisableBailoutWarningBody",
      l10n_util::GetStringFUTF16(
          IDS_EXTENSIONS_KIOSK_DISABLE_BAILOUT_SHORTCUT_WARNING_BODY,
          l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_OS_NAME)));

  source->AddBoolean("isLacrosEnabled",
                     crosapi::browser_util::IsLacrosEnabled());
#endif

  return source;
}

}  // namespace

ExtensionsUIConfig::ExtensionsUIConfig()
    : WebUIConfig(content::kChromeUIScheme, chrome::kChromeUIExtensionsHost) {}

ExtensionsUIConfig::~ExtensionsUIConfig() = default;

std::unique_ptr<content::WebUIController>
ExtensionsUIConfig::CreateWebUIController(content::WebUI* web_ui,
                                          const GURL& url) {
  Profile* profile = Profile::FromWebUI(web_ui);
  if (profile->IsGuestSession()) {
    return std::make_unique<PageNotAvailableForGuestUI>(
        web_ui, chrome::kChromeUIExtensionsHost);
  }
  return std::make_unique<ExtensionsUI>(web_ui);
}

ExtensionsUI::ExtensionsUI(content::WebUI* web_ui)
    : WebUIController(web_ui),
      webui_load_timer_(web_ui->GetWebContents(),
                        "Extensions.WebUi.DocumentLoadedInMainFrameTime",
                        "Extensions.WebUi.LoadCompletedInMainFrame") {
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource* source = nullptr;

  in_dev_mode_.Init(prefs::kExtensionsUIDeveloperMode, profile->GetPrefs(),
                    base::BindRepeating(&ExtensionsUI::OnDevModeChanged,
                                        base::Unretained(this)));

  source = CreateAndAddExtensionsSource(profile, *in_dev_mode_);
  ManagedUIHandler::Initialize(web_ui, source);

  // Need to allow <object> elements so that the <extensionoptions> browser
  // plugin can be loaded within chrome://extensions.
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ObjectSrc, "object-src 'self';");

  content::URLDataSource::Add(
      profile, std::make_unique<FaviconSource>(
                   profile, chrome::FaviconUrlFormat::kFavicon2));

  // Add a handler to provide pluralized strings.
  auto plural_string_handler = std::make_unique<PluralStringHandler>();
  plural_string_handler->AddLocalizedString("safetyCheckTitle",
                                            IDS_EXTENSIONS_SC_TITLE);
  plural_string_handler->AddLocalizedString("safetyCheckDescription",
                                            IDS_EXTENSIONS_SC_DESCRIPTION);
  plural_string_handler->AddLocalizedString("safetyCheckAllDoneForNow",
                                            IDS_EXTENSIONS_SC_ALL_DONE_FOR_NOW);
  plural_string_handler->AddLocalizedString(
      "mv2DeprecationPanelWarningHeader",
      IDS_EXTENSIONS_MV2_DEPRECATION_PANEL_WARNING_HEADER);
  plural_string_handler->AddLocalizedString(
      "mv2DeprecationPanelWarningSubtitle",
      IDS_EXTENSIONS_MV2_DEPRECATION_PANEL_WARNING_SUBTITLE);
  plural_string_handler->AddLocalizedString(
      "mv2DeprecationPanelDisabledHeader",
      IDS_EXTENSIONS_MV2_DEPRECATION_PANEL_DISABLED_HEADER);
  plural_string_handler->AddLocalizedString(
      "mv2DeprecationPanelDisabledSubtitle",
      IDS_EXTENSIONS_MV2_DEPRECATION_PANEL_DISABLED_SUBTITLE);
  web_ui->AddMessageHandler(std::move(plural_string_handler));
}

ExtensionsUI::~ExtensionsUI() = default;

// static
base::RefCountedMemory* ExtensionsUI::GetFaviconResourceBytes(
    ui::ResourceScaleFactor scale_factor) {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  return rb.LoadDataResourceBytesForScale(IDR_EXTENSIONS_FAVICON, scale_factor);
}

void ExtensionsUI::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(
      prefs::kExtensionsUIDeveloperMode, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
}

// Normally volatile data does not belong in loadTimeData, but in this case
// prevents flickering on a very prominent surface (top of the landing page).
void ExtensionsUI::OnDevModeChanged() {
  base::Value::Dict update;
  update.Set(kInDevModeKey, *in_dev_mode_);
  update.Set(kLoadTimeClassesKey, GetLoadTimeClasses(*in_dev_mode_));
  content::WebUIDataSource::Update(Profile::FromWebUI(web_ui()),
                                   chrome::kChromeUIExtensionsHost,
                                   std::move(update));
}

}  // namespace extensions
