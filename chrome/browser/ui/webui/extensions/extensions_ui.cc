// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/extensions/extensions_ui.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/chrome_extension_browser_constants.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/localized_string.h"
#include "chrome/browser/ui/webui/managed_ui_handler.h"
#include "chrome/browser/ui/webui/metrics_handler.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/extensions_resources.h"
#include "chrome/grit/extensions_resources_map.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/google/core/common/google_util.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "extensions/common/extension_urls.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/resources/grit/webui_resources.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/ownership/owner_settings_service_chromeos_factory.h"
#include "chrome/browser/ui/webui/extensions/chromeos/kiosk_apps_handler.h"
#endif

namespace extensions {

namespace {

constexpr char kInDevModeKey[] = "inDevMode";
constexpr char kShowActivityLogKey[] = "showActivityLog";
constexpr char kLoadTimeClassesKey[] = "loadTimeClasses";

#if !BUILDFLAG(OPTIMIZE_WEBUI)
constexpr char kGeneratedPath[] =
    "@out_folder@/gen/chrome/browser/resources/extensions/";
#endif

std::string GetLoadTimeClasses(bool in_dev_mode) {
  return in_dev_mode ? "in-dev-mode" : std::string();
}

content::WebUIDataSource* CreateMdExtensionsSource(Profile* profile,
                                                   bool in_dev_mode) {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIExtensionsHost);
  source->OverrideContentSecurityPolicyScriptSrc(
      "script-src chrome://resources chrome://test 'self';");

  source->UseStringsJs();

  static constexpr LocalizedString kLocalizedStrings[] = {
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

    // Add extension-specific strings.
    {"title", IDS_MANAGE_EXTENSIONS_SETTING_WINDOWS_TITLE},
    {"toolbarTitle", IDS_EXTENSIONS_TOOLBAR_TITLE},
    {"mainMenu", IDS_EXTENSIONS_MENU_BUTTON_LABEL},
    {"search", IDS_EXTENSIONS_SEARCH},
    // TODO(dpapad): Use a single merged string resource for "Clear search".
    {"clearSearch", IDS_DOWNLOAD_CLEAR_SEARCH},
    {"sidebarExtensions", IDS_EXTENSIONS_SIDEBAR_EXTENSIONS},
    {"appsTitle", IDS_EXTENSIONS_APPS_TITLE},
    {"noExtensionsOrApps", IDS_EXTENSIONS_NO_INSTALLED_ITEMS},
    {"noDescription", IDS_EXTENSIONS_NO_DESCRIPTION},
    {"viewInStore", IDS_EXTENSIONS_ITEM_CHROME_WEB_STORE},
    {"extensionWebsite", IDS_EXTENSIONS_ITEM_EXTENSION_WEBSITE},
    {"dropToInstall", IDS_EXTENSIONS_INSTALL_DROP_TARGET},
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
    {"stackTrace", IDS_EXTENSIONS_ERROR_STACK_TRACE},
    // TODO(dpapad): Unify with Settings' IDS_SETTINGS_WEB_STORE.
    {"openChromeWebStore", IDS_EXTENSIONS_SIDEBAR_OPEN_CHROME_WEB_STORE},
    {"keyboardShortcuts", IDS_EXTENSIONS_SIDEBAR_KEYBOARD_SHORTCUTS},
    {"incognitoInfoWarning", IDS_EXTENSIONS_INCOGNITO_WARNING},
    {"hostPermissionsDescription", IDS_EXTENSIONS_HOST_PERMISSIONS_DESCRIPTION},
    {"hostPermissionsEdit", IDS_EXTENSIONS_HOST_PERMISSIONS_EDIT},
    {"hostPermissionsHeading", IDS_EXTENSIONS_ITEM_HOST_PERMISSIONS_HEADING},
    {"hostAccessOnClick", IDS_EXTENSIONS_HOST_ACCESS_ON_CLICK},
    {"hostAccessOnSpecificSites", IDS_EXTENSIONS_HOST_ACCESS_ON_SPECIFIC_SITES},
    {"hostAccessOnAllSites", IDS_EXTENSIONS_HOST_ACCESS_ON_ALL_SITES},
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
    {"itemRemoveExtension", IDS_EXTENSIONS_ITEM_REMOVE_EXTENSION},
    {"itemSiteAccess", IDS_EXTENSIONS_ITEM_SITE_ACCESS},
    {"itemSiteAccessAddHost", IDS_EXTENSIONS_ITEM_SITE_ACCESS_ADD_HOST},
    {"itemSiteAccessEmpty", IDS_EXTENSIONS_ITEM_SITE_ACCESS_EMPTY},
    {"itemSource", IDS_EXTENSIONS_ITEM_SOURCE},
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
    {"itemRepair", IDS_EXTENSIONS_REPAIR_CORRUPTED},
    {"itemReload", IDS_EXTENSIONS_RELOAD_TERMINATED},
    {"loadErrorCouldNotLoadManifest",
     IDS_EXTENSIONS_LOAD_ERROR_COULD_NOT_LOAD_MANIFEST},
    {"loadErrorHeading", IDS_EXTENSIONS_LOAD_ERROR_HEADING},
    {"loadErrorFileLabel", IDS_EXTENSIONS_LOAD_ERROR_FILE_LABEL},
    {"loadErrorErrorLabel", IDS_EXTENSIONS_LOAD_ERROR_ERROR_LABEL},
    {"loadErrorRetry", IDS_EXTENSIONS_LOAD_ERROR_RETRY},
    {"loadingActivities", IDS_EXTENSIONS_LOADING_ACTIVITIES},
    {"missingOrUninstalledExtension", IDS_MISSING_OR_UNINSTALLED_EXTENSION},
    {"noActivities", IDS_EXTENSIONS_NO_ACTIVITIES},
    {"noErrorsToShow", IDS_EXTENSIONS_ERROR_NO_ERRORS_CODE_MESSAGE},
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
    {"shortcutNotSet", IDS_EXTENSIONS_SHORTCUT_NOT_SET},
    {"shortcutScopeGlobal", IDS_EXTENSIONS_SHORTCUT_SCOPE_GLOBAL},
    {"shortcutScopeLabel", IDS_EXTENSIONS_SHORTCUT_SCOPE_LABEL},
    {"shortcutScopeInChrome", IDS_EXTENSIONS_SHORTCUT_SCOPE_IN_CHROME},
    {"shortcutTypeAShortcut", IDS_EXTENSIONS_TYPE_A_SHORTCUT},
    {"shortcutIncludeStartModifier", IDS_EXTENSIONS_INCLUDE_START_MODIFIER},
    {"shortcutTooManyModifiers", IDS_EXTENSIONS_TOO_MANY_MODIFIERS},
    {"shortcutNeedCharacter", IDS_EXTENSIONS_NEED_CHARACTER},
    {"toolbarDevMode", IDS_EXTENSIONS_DEVELOPER_MODE},
    {"toolbarLoadUnpacked", IDS_EXTENSIONS_TOOLBAR_LOAD_UNPACKED},
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

#if defined(OS_CHROMEOS)
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
  AddLocalizedStringsBulk(source, kLocalizedStrings,
                          base::size(kLocalizedStrings));

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
#if defined(OS_CHROMEOS)
  source->AddString(
      "kioskDisableBailoutWarningBody",
      l10n_util::GetStringFUTF16(
          IDS_EXTENSIONS_KIOSK_DISABLE_BAILOUT_SHORTCUT_WARNING_BODY,
          l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_OS_NAME)));
#endif
  source->AddString(
      "getMoreExtensionsUrl",
      base::ASCIIToUTF16(
          google_util::AppendGoogleLocaleParam(
              GURL(extension_urls::GetWebstoreExtensionsCategoryURL()),
              g_browser_process->GetApplicationLocale())
              .spec()));
  source->AddString("hostPermissionsLearnMoreLink",
                    chrome_extension_constants::kRuntimeHostPermissionsHelpURL);
  source->AddBoolean(kInDevModeKey, in_dev_mode);
  source->AddBoolean(kShowActivityLogKey,
                     base::CommandLine::ForCurrentProcess()->HasSwitch(
                         ::switches::kEnableExtensionActivityLogging));
  source->AddString(kLoadTimeClassesKey, GetLoadTimeClasses(in_dev_mode));

#if BUILDFLAG(OPTIMIZE_WEBUI)
  source->AddResourcePath("extensions.js", IDR_EXTENSIONS_CRISPER_JS);
  source->SetDefaultResource(IDR_EXTENSIONS_VULCANIZED_HTML);
#else
  // Add all MD Extensions resources.
  for (size_t i = 0; i < kExtensionsResourcesSize; ++i) {
    std::string path = kExtensionsResources[i].name;
    if (path.rfind(kGeneratedPath, 0) == 0) {
      path = path.substr(sizeof(kGeneratedPath) - 1);
    }

    source->AddResourcePath(path, kExtensionsResources[i].value);
  }
  source->SetDefaultResource(IDR_EXTENSIONS_EXTENSIONS_HTML);
#endif
  source->EnableReplaceI18nInJS();
  source->AddResourcePath("test_loader.js", IDR_WEBUI_JS_TEST_LOADER);
  source->AddResourcePath("test_loader.html", IDR_WEBUI_HTML_TEST_LOADER);

  return source;
}

}  // namespace

ExtensionsUI::ExtensionsUI(content::WebUI* web_ui)
    : WebUIController(web_ui),
      webui_load_timer_(web_ui->GetWebContents(),
                        "Extensions.WebUi.DocumentLoadedInMainFrameTime.MD",
                        "Extensions.WebUi.LoadCompletedInMainFrame.MD") {
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource* source = nullptr;

  in_dev_mode_.Init(
      prefs::kExtensionsUIDeveloperMode, profile->GetPrefs(),
      base::Bind(&ExtensionsUI::OnDevModeChanged, base::Unretained(this)));

  source = CreateMdExtensionsSource(profile, *in_dev_mode_);
  ManagedUIHandler::Initialize(web_ui, source);

#if defined(OS_CHROMEOS)
  auto kiosk_app_handler = std::make_unique<chromeos::KioskAppsHandler>(
      chromeos::OwnerSettingsServiceChromeOSFactory::GetForBrowserContext(
          profile));
  web_ui->AddMessageHandler(std::move(kiosk_app_handler));
#endif

  // Need to allow <object> elements so that the <extensionoptions> browser
  // plugin can be loaded within chrome://extensions.
  source->OverrideContentSecurityPolicyObjectSrc("object-src 'self';");

  content::WebUIDataSource::Add(profile, source);
}

ExtensionsUI::~ExtensionsUI() {}

// static
base::RefCountedMemory* ExtensionsUI::GetFaviconResourceBytes(
    ui::ScaleFactor scale_factor) {
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
  auto update = std::make_unique<base::DictionaryValue>();
  update->SetBoolean(kInDevModeKey, *in_dev_mode_);
  update->SetString(kLoadTimeClassesKey, GetLoadTimeClasses(*in_dev_mode_));
  content::WebUIDataSource::Update(Profile::FromWebUI(web_ui()),
                                   chrome::kChromeUIExtensionsHost,
                                   std::move(update));
}

}  // namespace extensions
