// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ntp/app_launcher_handler.h"

#include <stddef.h>

#include <string>
#include <utility>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/i18n/rtl.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/apps/app_service/app_icon_source.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/browser_app_launcher.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_ui_util.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/installable/installable_metrics.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/apps/app_info_dialog.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/extensions/app_launch_params.h"
#include "chrome/browser/ui/extensions/extension_enable_flow.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/web_app_dialog_manager.h"
#include "chrome/browser/ui/web_applications/web_app_ui_manager_impl.h"
#include "chrome/browser/ui/webui/extensions/extension_basic_info.h"
#include "chrome/browser/ui/webui/extensions/extension_icon_source.h"
#include "chrome/browser/ui/webui/ntp/new_tab_ui.h"
#include "chrome/browser/web_applications/components/app_registry_controller.h"
#include "chrome/browser/web_applications/components/install_finalizer.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_provider_base.h"
#include "chrome/browser/web_applications/extensions/bookmark_app_finalizer_utils.h"
#include "chrome/browser/web_applications/extensions/bookmark_app_util.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_metrics.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/web_application_info.h"
#include "chrome/grit/generated_resources.h"
#include "components/favicon_base/favicon_types.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_ui.h"
#include "extensions/browser/app_sorting.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/management_policy.h"
#include "extensions/browser/pref_names.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_icon_set.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "net/base/url_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"
#include "url/gurl.h"

using content::WebContents;
using extensions::AppSorting;
using extensions::CrxInstaller;
using extensions::Extension;
using extensions::ExtensionPrefs;
using extensions::ExtensionRegistry;
using extensions::ExtensionSet;
using extensions::ExtensionSystem;

namespace {

// The purpose of this enum is to track which page on the NTP is showing.
// The lower 10 bits of kNtpShownPage are used for the index within the page
// group, and the rest of the bits are used for the page group ID (defined
// here).
static const int kPageIdOffset = 10;
enum {
  INDEX_MASK = (1 << kPageIdOffset) - 1,
  APPS_PAGE_ID = 2 << kPageIdOffset,
};

// Keys in the dictionary returned by GetExtensionBasicInfo().
const char kDescriptionKey[] = "description";
const char kEnabledKey[] = "enabled";
const char kInfoIdKey[] = "id";
const char kInfoNameKey[] = "name";
const char kKioskEnabledKey[] = "kioskEnabled";
const char kKioskOnlyKey[] = "kioskOnly";
const char kOfflineEnabledKey[] = "offlineEnabled";
const char kPackagedAppKey[] = "packagedApp";

const int kWebAppIconLargeNonDefault = 128;
const int kWebAppIconSmallNonDefault = 16;

// The Youtube app is incorrectly harded to be a 'bookmark app'. However, it is
// a platform app. This helper method special cases that, and should be used
// instead of extension->from_bookmark().
// TODO(crbug.com/1065748): Remove this hack once the youtube app is fixed.
bool FromBookmark(const extensions::Extension* extension) {
  return extension->from_bookmark() &&
         extension->id() != extension_misc::kYoutubeAppId;
}

// The Youtube app is incorrectly harded to be a 'bookmark app'. However, it is
// a platform app.
// TODO(crbug.com/1065748): Remove this hack once the youtube app is fixed.
bool IsYoutubeExtension(const std::string& extension_id) {
  return extension_id == extension_misc::kYoutubeAppId;
}

void GetWebAppBasicInfo(const web_app::AppId& app_id,
                        const web_app::AppRegistrar& app_registrar,
                        base::DictionaryValue* info) {
  info->SetString(kInfoIdKey, app_id);
  info->SetString(kInfoNameKey, app_registrar.GetAppShortName(app_id));
  info->SetBoolean(kEnabledKey, true);
  info->SetBoolean(kKioskEnabledKey, false);
  info->SetBoolean(kKioskOnlyKey, false);
  info->SetBoolean(kOfflineEnabledKey, true);
  info->SetString(kDescriptionKey, app_registrar.GetAppDescription(app_id));
  info->SetBoolean(kPackagedAppKey, false);
}

bool DesktopPWAsWithoutExtensions() {
  return base::FeatureList::IsEnabled(features::kDesktopPWAsWithoutExtensions);
}

bool HasMatchingOrGreaterThanIcon(const SortedSizesPx& downloaded_icon_sizes,
                                  int pixels) {
  if (downloaded_icon_sizes.empty())
    return false;
  SquareSizePx largest = *downloaded_icon_sizes.rbegin();
  return largest >= pixels;
}

}  // namespace

AppLauncherHandler::AppInstallInfo::AppInstallInfo() {}
AppLauncherHandler::AppInstallInfo::~AppInstallInfo() {}

AppLauncherHandler::AppLauncherHandler(
    extensions::ExtensionService* extension_service,
    web_app::WebAppProvider* web_app_provider)
    : extension_service_(extension_service),
      web_app_provider_(web_app_provider),
      ignore_changes_(false),
      has_loaded_apps_(false) {}

AppLauncherHandler::~AppLauncherHandler() {
  ExtensionRegistry::Get(Profile::FromWebUI(web_ui()))->RemoveObserver(this);
}

void AppLauncherHandler::CreateWebAppInfo(const web_app::AppId& app_id,
                                          base::DictionaryValue* value) {
  // The items which are to be written into |value| are also described in
  // chrome/browser/resources/ntp4/page_list_view.js in @typedef for AppInfo.
  // Please update it whenever you add or remove any keys here.
  value->Clear();

  // Communicate the kiosk flag so the apps page can disable showing the
  // context menu in kiosk mode.
  value->SetBoolean(
      "kioskMode",
      base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kKioskMode));

  auto& registrar = web_app_provider_->registrar();

  base::string16 name = base::UTF8ToUTF16(registrar.GetAppShortName(app_id));
  NewTabUI::SetUrlTitleAndDirection(value, name,
                                    registrar.GetAppStartUrl(app_id));
  NewTabUI::SetFullNameAndDirection(name, value);

  GetWebAppBasicInfo(app_id, registrar, value);

  value->SetBoolean("mayDisable", web_app_provider_->install_finalizer()
                                      .CanUserUninstallExternalApp(app_id));
  bool is_locally_installed = registrar.IsLocallyInstalled(app_id);
  value->SetBoolean("mayChangeLaunchType", is_locally_installed);

  // Any locally installed app can have shortcuts created.
  value->SetBoolean("mayCreateShortcuts", is_locally_installed);
  value->SetBoolean("isLocallyInstalled", is_locally_installed);

  base::Optional<std::string> icon_big;
  base::Optional<std::string> icon_small;

  if (HasMatchingOrGreaterThanIcon(
          registrar.GetAppDownloadedIconSizesAny(app_id),
          kWebAppIconLargeNonDefault)) {
    icon_big =
        apps::AppIconSource::GetIconURL(app_id, kWebAppIconLargeNonDefault)
            .spec();
  }

  if (HasMatchingOrGreaterThanIcon(
          registrar.GetAppDownloadedIconSizesAny(app_id),
          kWebAppIconSmallNonDefault)) {
    icon_small =
        apps::AppIconSource::GetIconURL(app_id, kWebAppIconSmallNonDefault)
            .spec();
  }

  value->SetBoolean("icon_big_exists", icon_big.has_value());
  value->SetString("icon_big", icon_big.value_or(GURL().spec()));
  value->SetBoolean("icon_small_exists", icon_small.has_value());
  value->SetString("icon_small", icon_small.value_or(GURL().spec()));

  extensions::LaunchContainerAndType result =
      extensions::GetLaunchContainerAndTypeFromDisplayMode(
          registrar.GetAppUserDisplayMode(app_id));
  value->SetInteger("launch_container",
                    static_cast<int>(result.launch_container));
  value->SetInteger("launch_type", result.launch_type);
  value->SetBoolean("is_component", false);
  value->SetBoolean("is_webstore", false);

  // TODO(https://crbug.com/1061586): Figure out a way to keep the AppSorting
  // system compatible with web apps.
  DCHECK_NE(app_id, extensions::kWebStoreAppId);
  AppSorting* sorting =
      ExtensionSystem::Get(Profile::FromWebUI(web_ui()))->app_sorting();
  syncer::StringOrdinal page_ordinal = sorting->GetPageOrdinal(app_id);
  if (!page_ordinal.IsValid()) {
    // Make sure every app has a page ordinal (some predate the page ordinal).
    page_ordinal = sorting->GetNaturalAppPageOrdinal();
    sorting->SetPageOrdinal(app_id, page_ordinal);
  }
  value->SetInteger("page_index",
                    sorting->PageStringOrdinalAsInteger(page_ordinal));

  syncer::StringOrdinal app_launch_ordinal =
      sorting->GetAppLaunchOrdinal(app_id);
  if (!app_launch_ordinal.IsValid()) {
    // Make sure every app has a launch ordinal (some predate the launch
    // ordinal).
    app_launch_ordinal = sorting->CreateNextAppLaunchOrdinal(page_ordinal);
    sorting->SetAppLaunchOrdinal(app_id, app_launch_ordinal);
  }
  value->SetString("app_launch_ordinal", app_launch_ordinal.ToInternalValue());
}

void AppLauncherHandler::CreateExtensionInfo(const Extension* extension,
                                             base::DictionaryValue* value) {
  DCHECK(!FromBookmark(extension));
  // The items which are to be written into |value| are also described in
  // chrome/browser/resources/ntp4/page_list_view.js in @typedef for AppInfo.
  // Please update it whenever you add or remove any keys here.
  value->Clear();

  // Communicate the kiosk flag so the apps page can disable showing the
  // context menu in kiosk mode.
  value->SetBoolean(
      "kioskMode",
      base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kKioskMode));

  // The Extension class 'helpfully' wraps bidi control characters that
  // impede our ability to determine directionality.
  base::string16 short_name = base::UTF8ToUTF16(extension->short_name());
  base::i18n::UnadjustStringForLocaleDirection(&short_name);
  NewTabUI::SetUrlTitleAndDirection(
      value,
      short_name,
      extensions::AppLaunchInfo::GetFullLaunchURL(extension));

  base::string16 name = base::UTF8ToUTF16(extension->name());
  base::i18n::UnadjustStringForLocaleDirection(&name);
  NewTabUI::SetFullNameAndDirection(name, value);

  bool enabled = extension_service_->IsExtensionEnabled(extension->id()) &&
                 !extensions::ExtensionRegistry::Get(
                      extension_service_->GetBrowserContext())
                      ->terminated_extensions()
                      .GetByID(extension->id());
  extensions::GetExtensionBasicInfo(extension, enabled, value);

  value->SetBoolean(
      "mayDisable",
      extensions::ExtensionSystem::Get(extension_service_->profile())
          ->management_policy()
          ->UserMayModifySettings(extension, nullptr));

  bool is_locally_installed =
      !extension->is_hosted_app() ||
      BookmarkAppIsLocallyInstalled(extension_service_->profile(), extension);
  value->SetBoolean("mayChangeLaunchType",
                    !extension->is_platform_app() && is_locally_installed);

  // Any locally installed app can have shortcuts created.
  value->SetBoolean("mayCreateShortcuts", is_locally_installed);
  value->SetBoolean("isLocallyInstalled", is_locally_installed);

  auto icon_size = extension_misc::EXTENSION_ICON_LARGE;
  auto match_type = ExtensionIconSet::MATCH_BIGGER;
  bool has_non_default_large_icon =
      !extensions::IconsInfo::GetIconURL(extension, icon_size, match_type)
           .is_empty();
  GURL large_icon = extensions::ExtensionIconSource::GetIconURL(
      extension, icon_size, match_type, false);
  value->SetString("icon_big", large_icon.spec());
  value->SetBoolean("icon_big_exists", has_non_default_large_icon);

  icon_size = extension_misc::EXTENSION_ICON_BITTY;
  bool has_non_default_small_icon =
      !extensions::IconsInfo::GetIconURL(extension, icon_size, match_type)
           .is_empty();
  GURL small_icon = extensions::ExtensionIconSource::GetIconURL(
      extension, icon_size, match_type, false);
  value->SetString("icon_small", small_icon.spec());
  value->SetBoolean("icon_small_exists", has_non_default_small_icon);

  value->SetInteger(
      "launch_container",
      static_cast<int>(
          extensions::AppLaunchInfo::GetLaunchContainer(extension)));
  ExtensionPrefs* prefs = ExtensionPrefs::Get(extension_service_->profile());
  value->SetInteger("launch_type", extensions::GetLaunchType(prefs, extension));
  value->SetBoolean("is_component",
                    extension->location() == extensions::Manifest::COMPONENT);
  value->SetBoolean("is_webstore",
      extension->id() == extensions::kWebStoreAppId);

  AppSorting* sorting =
      ExtensionSystem::Get(extension_service_->profile())->app_sorting();
  syncer::StringOrdinal page_ordinal = sorting->GetPageOrdinal(extension->id());
  if (!page_ordinal.IsValid()) {
    // Make sure every app has a page ordinal (some predate the page ordinal).
    // The webstore app should be on the first page.
    page_ordinal = extension->id() == extensions::kWebStoreAppId ?
        sorting->CreateFirstAppPageOrdinal() :
        sorting->GetNaturalAppPageOrdinal();
    sorting->SetPageOrdinal(extension->id(), page_ordinal);
  }
  value->SetInteger("page_index",
      sorting->PageStringOrdinalAsInteger(page_ordinal));

  syncer::StringOrdinal app_launch_ordinal =
      sorting->GetAppLaunchOrdinal(extension->id());
  if (!app_launch_ordinal.IsValid()) {
    // Make sure every app has a launch ordinal (some predate the launch
    // ordinal). The webstore's app launch ordinal is always set to the first
    // position.
    app_launch_ordinal = extension->id() == extensions::kWebStoreAppId ?
        sorting->CreateFirstAppLaunchOrdinal(page_ordinal) :
        sorting->CreateNextAppLaunchOrdinal(page_ordinal);
    sorting->SetAppLaunchOrdinal(extension->id(), app_launch_ordinal);
  }
  value->SetString("app_launch_ordinal", app_launch_ordinal.ToInternalValue());
}

// static
void AppLauncherHandler::GetLocalizedValues(Profile* profile,
                                            base::DictionaryValue* values) {
  PrefService* prefs = profile->GetPrefs();
  int shown_page = prefs->GetInteger(prefs::kNtpShownPage);
  values->SetInteger("shown_page_index", shown_page & INDEX_MASK);
}

// static
void AppLauncherHandler::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterIntegerPref(prefs::kNtpShownPage, APPS_PAGE_ID);
}

void AppLauncherHandler::RegisterMessages() {
  registrar_.Add(this, chrome::NOTIFICATION_APP_INSTALLED_TO_NTP,
      content::Source<WebContents>(web_ui()->GetWebContents()));

  web_ui()->RegisterMessageCallback(
      "getApps", base::BindRepeating(&AppLauncherHandler::HandleGetApps,
                                     base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "launchApp", base::BindRepeating(&AppLauncherHandler::HandleLaunchApp,
                                       base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setLaunchType",
      base::BindRepeating(&AppLauncherHandler::HandleSetLaunchType,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "uninstallApp",
      base::BindRepeating(&AppLauncherHandler::HandleUninstallApp,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "createAppShortcut",
      base::BindRepeating(&AppLauncherHandler::HandleCreateAppShortcut,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "installAppLocally",
      base::BindRepeating(&AppLauncherHandler::HandleInstallAppLocally,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "showAppInfo", base::BindRepeating(&AppLauncherHandler::HandleShowAppInfo,
                                         base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "reorderApps", base::BindRepeating(&AppLauncherHandler::HandleReorderApps,
                                         base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setPageIndex",
      base::BindRepeating(&AppLauncherHandler::HandleSetPageIndex,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "saveAppPageName",
      base::BindRepeating(&AppLauncherHandler::HandleSaveAppPageName,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "generateAppForLink",
      base::BindRepeating(&AppLauncherHandler::HandleGenerateAppForLink,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "pageSelected",
      base::BindRepeating(&AppLauncherHandler::HandlePageSelected,
                          base::Unretained(this)));
}

void AppLauncherHandler::Observe(int type,
                                 const content::NotificationSource& source,
                                 const content::NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_APP_INSTALLED_TO_NTP) {
    highlight_app_id_ = *content::Details<const std::string>(details).ptr();
    if (has_loaded_apps_)
      SetAppToBeHighlighted();
    return;
  }

  if (ignore_changes_ || !has_loaded_apps_)
    return;

  switch (type) {
    case chrome::NOTIFICATION_APP_LAUNCHER_REORDERED: {
      const std::string* id =
          content::Details<const std::string>(details).ptr();
      if (id) {
        base::DictionaryValue app_info;
        if (web_app_provider_->registrar().IsInstalled(*id)) {
          CreateWebAppInfo(*id, &app_info);
        } else {
          const Extension* extension =
              ExtensionRegistry::Get(extension_service_->profile())
                  ->GetInstalledExtension(*id);
          if (!extension) {
            // Extension could still be downloading or installing.
            return;
          }

          CreateExtensionInfo(extension, &app_info);
        }
        web_ui()->CallJavascriptFunctionUnsafe("ntp.appMoved", app_info);
      } else {
        HandleGetApps(nullptr);
      }
      break;
    }
    default:
      NOTREACHED();
  }
}

void AppLauncherHandler::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const Extension* extension) {
  if (!ShouldShow(extension))
    return;
  if (FromBookmark(extension))
    return;

  std::unique_ptr<base::DictionaryValue> app_info(GetExtensionInfo(extension));
  if (!app_info.get())
    return;

  visible_apps_.insert(extension->id());
  web_ui()->CallJavascriptFunctionUnsafe("ntp.appAdded", *app_info,
                                         /*highlight=*/base::Value(false));
}

void AppLauncherHandler::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    extensions::UnloadedExtensionReason reason) {
  // Exclude events from bookmarks apps if BMO is turned on.
  if (extension->from_bookmark())
    return;
  ExtensionRemoved(extension, /*is_uninstall=*/false);
}

void AppLauncherHandler::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    extensions::UninstallReason reason) {
  // Exclude events from bookmarks apps if BMO is turned on.
  if (FromBookmark(extension))
    return;
  ExtensionRemoved(extension, /*is_uninstall=*/true);
}

void AppLauncherHandler::OnWebAppInstalled(const web_app::AppId& app_id) {
  std::unique_ptr<base::DictionaryValue> app_info(GetWebAppInfo(app_id));
  if (!app_info.get())
    return;

  if (attempting_web_app_install_page_ordinal_.has_value()) {
    AppSorting* sorting =
        ExtensionSystem::Get(Profile::FromWebUI(web_ui()))->app_sorting();
    sorting->SetPageOrdinal(app_id,
                            attempting_web_app_install_page_ordinal_.value());
  }

  visible_apps_.insert(app_id);
  base::Value highlight(attempting_web_app_install_page_ordinal_.has_value());
  attempting_web_app_install_page_ordinal_ = base::nullopt;
  web_ui()->CallJavascriptFunctionUnsafe("ntp.appAdded", *app_info, highlight);
}

void AppLauncherHandler::OnWebAppUninstalled(const web_app::AppId& app_id) {
  std::unique_ptr<base::DictionaryValue> app_info =
      std::make_unique<base::DictionaryValue>();
  app_info->SetString(kInfoIdKey, app_id);
  // Since |isUninstaLL| is true below, the only item needed in the app_info
  // dictionary is the id.
  web_ui()->CallJavascriptFunctionUnsafe(
      "ntp.appRemoved", *app_info, /*isUninstall=*/base::Value(true),
      base::Value(!extension_id_prompting_.empty()));
}

void AppLauncherHandler::OnAppRegistrarDestroyed() {
  web_apps_observer_.RemoveAll();
}

void AppLauncherHandler::FillAppDictionary(base::DictionaryValue* dictionary) {
  // CreateExtensionInfo and ClearOrdinals can change the extension prefs.
  base::AutoReset<bool> auto_reset(&ignore_changes_, true);

  auto installed_extensions = std::make_unique<base::ListValue>();
  Profile* profile = Profile::FromWebUI(web_ui());
  PrefService* prefs = profile->GetPrefs();

  std::set<web_app::AppId> web_app_ids;
  web_app::AppRegistrar& registrar = web_app_provider_->registrar();
  for (const web_app::AppId& web_app_id : registrar.GetAppIds()) {
    // The Youtube app is harded to be a 'bookmark app', however it is not, it
    // is a platform app.
    // TODO(crbug.com/1065748): Remove this hack once the youtube app is fixed.
    if (IsYoutubeExtension(web_app_id))
      continue;
    installed_extensions->Append(GetWebAppInfo(web_app_id));
    web_app_ids.insert(web_app_id);
  }

  ExtensionRegistry* registry =
      ExtensionRegistry::Get(extension_service_->profile());
  for (auto it = visible_apps_.begin(); it != visible_apps_.end(); ++it) {
    if (base::Contains(web_app_ids, *it))
      continue;
    const Extension* extension = registry->GetInstalledExtension(*it);
    if (extension &&
        extensions::ui_util::ShouldDisplayInNewTabPage(extension, profile)) {
      DCHECK(!FromBookmark(extension));
      installed_extensions->Append(GetExtensionInfo(extension));
    }
  }

  dictionary->Set("apps", std::move(installed_extensions));

  const base::ListValue* app_page_names =
      prefs->GetList(prefs::kNtpAppPageNames);
  if (!app_page_names || !app_page_names->GetSize()) {
    ListPrefUpdate update(prefs, prefs::kNtpAppPageNames);
    base::ListValue* list = update.Get();
    list->Set(0, std::make_unique<base::Value>(
                     l10n_util::GetStringUTF16(IDS_APP_DEFAULT_PAGE_NAME)));
    dictionary->SetKey("appPageNames", list->Clone());
  } else {
    dictionary->SetKey("appPageNames", app_page_names->Clone());
  }
}

std::unique_ptr<base::DictionaryValue> AppLauncherHandler::GetExtensionInfo(
    const Extension* extension) {
  std::unique_ptr<base::DictionaryValue> app_info(new base::DictionaryValue());
  // CreateExtensionInfo can change the extension prefs.
  base::AutoReset<bool> auto_reset(&ignore_changes_, true);
  CreateExtensionInfo(extension, app_info.get());
  return app_info;
}

std::unique_ptr<base::DictionaryValue> AppLauncherHandler::GetWebAppInfo(
    const web_app::AppId& app_id) {
  std::unique_ptr<base::DictionaryValue> app_info(new base::DictionaryValue());
  CreateWebAppInfo(app_id, app_info.get());
  return app_info;
}

void AppLauncherHandler::HandleGetApps(const base::ListValue* args) {
  base::DictionaryValue dictionary;

  // Tell the client whether to show the promo for this view. We don't do this
  // in the case of PREF_CHANGED because:
  //
  // a) At that point in time, depending on the pref that changed, it can look
  //    like the set of apps installed has changed, and we will mark the promo
  //    expired.
  // b) Conceptually, it doesn't really make sense to count a
  //    prefchange-triggered refresh as a promo 'view'.
  Profile* profile = Profile::FromWebUI(web_ui());

  // The first time we load the apps we must add all current app to the list
  // of apps visible on the NTP.
  if (!has_loaded_apps_) {
    ExtensionRegistry* registry = ExtensionRegistry::Get(profile);
    const ExtensionSet& enabled_set = registry->enabled_extensions();
    for (extensions::ExtensionSet::const_iterator it = enabled_set.begin();
         it != enabled_set.end(); ++it) {
      if (FromBookmark(it->get()))
        continue;
      visible_apps_.insert((*it)->id());
    }

    const ExtensionSet& disabled_set = registry->disabled_extensions();
    for (ExtensionSet::const_iterator it = disabled_set.begin();
         it != disabled_set.end(); ++it) {
      if (FromBookmark(it->get()))
        continue;
      visible_apps_.insert((*it)->id());
    }

    const ExtensionSet& terminated_set = registry->terminated_extensions();
    for (ExtensionSet::const_iterator it = terminated_set.begin();
         it != terminated_set.end(); ++it) {
      if (FromBookmark(it->get()))
        continue;
      visible_apps_.insert((*it)->id());
    }
  }

  SetAppToBeHighlighted();
  FillAppDictionary(&dictionary);
  web_ui()->CallJavascriptFunctionUnsafe("ntp.getAppsCallback", dictionary);

  // First time we get here we set up the observer so that we can tell update
  // the apps as they change.
  if (!has_loaded_apps_) {
    base::Closure callback = base::Bind(
        &AppLauncherHandler::OnExtensionPreferenceChanged,
        base::Unretained(this));
    extension_pref_change_registrar_.Init(
        ExtensionPrefs::Get(profile)->pref_service());
    extension_pref_change_registrar_.Add(
        extensions::pref_names::kExtensions, callback);
    extension_pref_change_registrar_.Add(prefs::kNtpAppPageNames, callback);

    ExtensionRegistry::Get(profile)->AddObserver(this);
    registrar_.Add(this, chrome::NOTIFICATION_APP_LAUNCHER_REORDERED,
                   content::Source<AppSorting>(
                       ExtensionSystem::Get(profile)->app_sorting()));
    web_apps_observer_.Add(&web_app_provider_->registrar());
  }

  has_loaded_apps_ = true;
}

void AppLauncherHandler::HandleLaunchApp(const base::ListValue* args) {
  std::string extension_id;
  CHECK(args->GetString(0, &extension_id));
  double source = -1.0;
  CHECK(args->GetDouble(1, &source));
  GURL override_url;

  extension_misc::AppLaunchBucket launch_bucket =
      static_cast<extension_misc::AppLaunchBucket>(
          static_cast<int>(source));
  CHECK(launch_bucket >= 0 &&
        launch_bucket < extension_misc::APP_LAUNCH_BUCKET_BOUNDARY);

  Profile* profile = extension_service_->profile();

  extensions::Manifest::Type type;
  GURL full_launch_url;
  apps::mojom::LaunchContainer launch_container;

  web_app::AppRegistrar& registrar = web_app_provider_->registrar();
  if (registrar.IsInstalled(extension_id) &&
      !IsYoutubeExtension(extension_id)) {
    type = extensions::Manifest::Type::TYPE_HOSTED_APP;
    full_launch_url = registrar.GetAppStartUrl(extension_id);
    launch_container = web_app::ConvertDisplayModeToAppLaunchContainer(
        registrar.GetAppEffectiveDisplayMode(extension_id));
  } else {
    const Extension* extension = extensions::ExtensionRegistry::Get(profile)
                                     ->enabled_extensions()
                                     .GetByID(extension_id);

    // Prompt the user to re-enable the application if disabled.
    if (!extension) {
      PromptToEnableApp(extension_id);
      return;
    }
    DCHECK(!FromBookmark(extension));
    type = extension->GetType();
    full_launch_url = extensions::AppLaunchInfo::GetFullLaunchURL(extension);
    launch_container =
        extensions::GetLaunchContainer(ExtensionPrefs::Get(profile), extension);
  }

  WindowOpenDisposition disposition =
      args->GetSize() > 3 ? webui::GetDispositionFromClick(args, 3)
                          : WindowOpenDisposition::CURRENT_TAB;
  if (extension_id != extensions::kWebStoreAppId) {
    CHECK_NE(launch_bucket, extension_misc::APP_LAUNCH_BUCKET_INVALID);
    extensions::RecordAppLaunchType(launch_bucket, type);
  } else {
    extensions::RecordWebStoreLaunch();

    if (args->GetSize() > 2) {
      std::string source_value;
      CHECK(args->GetString(2, &source_value));
      if (!source_value.empty()) {
        override_url = net::AppendQueryParameter(
            full_launch_url, extension_urls::kWebstoreSourceField,
            source_value);
      }
    }
  }

  if (disposition == WindowOpenDisposition::NEW_FOREGROUND_TAB ||
      disposition == WindowOpenDisposition::NEW_BACKGROUND_TAB ||
      disposition == WindowOpenDisposition::NEW_WINDOW) {
    // TODO(jamescook): Proper support for background tabs.
    apps::AppLaunchParams params(
        extension_id,
        disposition == WindowOpenDisposition::NEW_WINDOW
            ? apps::mojom::LaunchContainer::kLaunchContainerWindow
            : apps::mojom::LaunchContainer::kLaunchContainerTab,
        disposition, apps::mojom::AppLaunchSource::kSourceNewTabPage);
    params.override_url = override_url;
    apps::AppServiceProxyFactory::GetForProfile(profile)
        ->BrowserAppLauncher()
        ->LaunchAppWithParams(params);
  } else {
    // To give a more "launchy" experience when using the NTP launcher, we close
    // it automatically.
    Browser* browser = chrome::FindBrowserWithWebContents(
        web_ui()->GetWebContents());
    WebContents* old_contents = nullptr;
    if (browser)
      old_contents = browser->tab_strip_model()->GetActiveWebContents();

    apps::AppLaunchParams params(
        extension_id, launch_container,
        old_contents ? WindowOpenDisposition::CURRENT_TAB
                     : WindowOpenDisposition::NEW_FOREGROUND_TAB,
        extensions::AppLaunchSource::kSourceNewTabPage);
    params.override_url = override_url;
    WebContents* new_contents =
        apps::AppServiceProxyFactory::GetForProfile(profile)
            ->BrowserAppLauncher()
            ->LaunchAppWithParams(params);

    // This will also destroy the handler, so do not perform any actions after.
    if (new_contents != old_contents && browser &&
        browser->tab_strip_model()->count() > 1) {
      chrome::CloseWebContents(browser, old_contents, true);
    }
  }
}

void AppLauncherHandler::HandleSetLaunchType(const base::ListValue* args) {
  std::string app_id;
  double launch_type_double;
  CHECK(args->GetString(0, &app_id));
  CHECK(args->GetDouble(1, &launch_type_double));
  extensions::LaunchType launch_type =
      static_cast<extensions::LaunchType>(static_cast<int>(launch_type_double));

  if (web_app_provider_->registrar().IsInstalled(app_id)) {
    // Don't update the page; it already knows about the launch type change.
    base::AutoReset<bool> auto_reset(&ignore_changes_, true);
    web_app::DisplayMode display_mode = web_app::DisplayMode::kBrowser;
    switch (launch_type) {
      case extensions::LAUNCH_TYPE_FULLSCREEN:
        display_mode = web_app::DisplayMode::kFullscreen;
        break;
      case extensions::LAUNCH_TYPE_WINDOW:
        display_mode = web_app::DisplayMode::kStandalone;
        break;
      case extensions::LAUNCH_TYPE_PINNED:
      case extensions::LAUNCH_TYPE_REGULAR:
        display_mode = web_app::DisplayMode::kBrowser;
        break;
      case extensions::LAUNCH_TYPE_INVALID:
      case extensions::NUM_LAUNCH_TYPES:
        NOTREACHED();
        break;
    }

    web_app_provider_->registry_controller().SetAppUserDisplayMode(
        app_id, display_mode, /*is_user_action=*/true);
    return;
  }

  const Extension* extension =
      extensions::ExtensionRegistry::Get(extension_service_->profile())
          ->GetExtensionById(app_id,
                             extensions::ExtensionRegistry::ENABLED |
                                 extensions::ExtensionRegistry::DISABLED |
                                 extensions::ExtensionRegistry::TERMINATED);
  if (!extension)
    return;
  DCHECK(!FromBookmark(extension));

  // Don't update the page; it already knows about the launch type change.
  base::AutoReset<bool> auto_reset(&ignore_changes_, true);
  extensions::SetLaunchType(Profile::FromWebUI(web_ui()), app_id, launch_type);
}

void AppLauncherHandler::HandleUninstallApp(const base::ListValue* args) {
  std::string extension_id;
  CHECK(args->GetString(0, &extension_id));

  if (web_app_provider_->registrar().IsInstalled(extension_id) &&
      !IsYoutubeExtension(extension_id)) {
    if (!extension_id_prompting_.empty())
      return;  // Only one prompt at a time.
    if (!web_app_provider_->install_finalizer().CanUserUninstallExternalApp(
            extension_id)) {
      LOG(ERROR) << "Attempt to uninstall a webapp that is non-usermanagable "
                 << "was made. App id : " << extension_id;
      return;
    }

    auto uninstall_success_callback = base::BindOnce(
        [](base::WeakPtr<AppLauncherHandler> app_launcher_handler,
           bool success) {
          if (app_launcher_handler)
            app_launcher_handler->CleanupAfterUninstall();
        },
        weak_ptr_factory_.GetWeakPtr());

    extension_id_prompting_ = extension_id;
    bool dont_confirm = false;
    if (args->GetBoolean(1, &dont_confirm) && dont_confirm) {
      base::AutoReset<bool> auto_reset(&ignore_changes_, true);
      web_app_provider_->install_finalizer().UninstallExternalAppByUser(
          extension_id_prompting_, std::move(uninstall_success_callback));
    } else {
      Browser* browser =
          chrome::FindBrowserWithWebContents(web_ui()->GetWebContents());
      web_app::WebAppUiManagerImpl::Get(Profile::FromWebUI(web_ui()))
          ->dialog_manager()
          .UninstallWebApp(
              extension_id_prompting_,
              web_app::WebAppDialogManager::UninstallSource::kAppsPage,
              browser->window(), std::move(uninstall_success_callback));
    }
    return;
  }
  const Extension* extension =
      ExtensionRegistry::Get(extension_service_->profile())
          ->GetInstalledExtension(extension_id);
  if (!extension)
    return;
  DCHECK(!FromBookmark(extension));

  if (!extensions::ExtensionSystem::Get(extension_service_->profile())
           ->management_policy()
           ->UserMayModifySettings(extension, nullptr)) {
    LOG(ERROR) << "Attempt to uninstall an extension that is non-usermanagable "
               << "was made. Extension id : " << extension->id();
    return;
  }
  if (!extension_id_prompting_.empty())
    return;  // Only one prompt at a time.

  extension_id_prompting_ = extension_id;

  bool dont_confirm = false;
  if (args->GetBoolean(1, &dont_confirm) && dont_confirm) {
    base::AutoReset<bool> auto_reset(&ignore_changes_, true);
    // Do the uninstall work here.
    extension_service_->UninstallExtension(
        extension_id_prompting_, extensions::UNINSTALL_REASON_USER_INITIATED,
        nullptr);
    CleanupAfterUninstall();
  } else {
    CreateExtensionUninstallDialog()->ConfirmUninstall(
        extension, extensions::UNINSTALL_REASON_USER_INITIATED,
        extensions::UNINSTALL_SOURCE_CHROME_APPS_PAGE);
  }
}

void AppLauncherHandler::HandleCreateAppShortcut(const base::ListValue* args) {
  std::string app_id;
  CHECK(args->GetString(0, &app_id));

  if (web_app_provider_->registrar().IsInstalled(app_id) &&
      !IsYoutubeExtension(app_id)) {
    Browser* browser =
        chrome::FindBrowserWithWebContents(web_ui()->GetWebContents());
    chrome::ShowCreateChromeAppShortcutsDialog(
        browser->window()->GetNativeWindow(), browser->profile(), app_id,
        base::BindRepeating([](bool success) {
          base::UmaHistogramBoolean(
              "Apps.AppInfoDialog.CreateWebAppShortcutSuccess", success);
        }));
    return;
  }

  const Extension* extension =
      extensions::ExtensionRegistry::Get(extension_service_->profile())
          ->GetExtensionById(app_id,
                             extensions::ExtensionRegistry::ENABLED |
                                 extensions::ExtensionRegistry::DISABLED |
                                 extensions::ExtensionRegistry::TERMINATED);
  if (!extension)
    return;
  DCHECK(!FromBookmark(extension));

  Browser* browser = chrome::FindBrowserWithWebContents(
        web_ui()->GetWebContents());
  chrome::ShowCreateChromeAppShortcutsDialog(
      browser->window()->GetNativeWindow(), browser->profile(), extension,
      base::BindRepeating([](bool success) {
        base::UmaHistogramBoolean(
            "Apps.AppInfoDialog.CreateExtensionShortcutSuccess", success);
      }));
}

void AppLauncherHandler::HandleInstallAppLocally(const base::ListValue* args) {
  std::string app_id;
  CHECK(args->GetString(0, &app_id));

  if (!web_app_provider_->registrar().IsInstalled(app_id))
    return;

  web_app_provider_->registry_controller().SetAppIsLocallyInstalled(app_id,
                                                                    true);
  web_app_provider_->registry_controller().SetAppInstallTime(app_id,
                                                             base::Time::Now());
  web_app::InstallOsHooksOptions options;
  options.add_to_desktop = true;
  options.add_to_quick_launch_bar = false;
  options.os_hooks[web_app::OsHookType::kShortcuts] = true;
  options.os_hooks[web_app::OsHookType::kShortcutsMenu] = true;
  options.os_hooks[web_app::OsHookType::kFileHandlers] = true;
  options.os_hooks[web_app::OsHookType::kRunOnOsLogin] = false;

  web_app_provider_->os_integration_manager().InstallOsHooks(
      app_id,
      base::BindOnce(&AppLauncherHandler::OnOsHooksInstalled,
                     weak_ptr_factory_.GetWeakPtr(), app_id),
      /*web_application_info=*/nullptr, std::move(options));

  // Use the appAdded to update the app icon's color to no longer be
  // greyscale.
  std::unique_ptr<base::DictionaryValue> app_info = GetWebAppInfo(app_id);
  if (app_info)
    web_ui()->CallJavascriptFunctionUnsafe("ntp.appAdded", *app_info);
}

void AppLauncherHandler::HandleShowAppInfo(const base::ListValue* args) {
  std::string extension_id;
  CHECK(args->GetString(0, &extension_id));

  if (web_app_provider_->registrar().IsInstalled(extension_id) &&
      !IsYoutubeExtension(extension_id)) {
    chrome::ShowSiteSettings(
        chrome::FindBrowserWithWebContents(web_ui()->GetWebContents()),
        web_app_provider_->registrar().GetAppStartUrl(extension_id));
    return;
  }

  const Extension* extension =
      extensions::ExtensionRegistry::Get(extension_service_->profile())
          ->GetExtensionById(extension_id,
                             extensions::ExtensionRegistry::ENABLED |
                                 extensions::ExtensionRegistry::DISABLED |
                                 extensions::ExtensionRegistry::TERMINATED);
  if (!extension)
    return;
  DCHECK(!FromBookmark(extension));

  UMA_HISTOGRAM_ENUMERATION("Apps.AppInfoDialog.Launches",
                            AppInfoLaunchSource::FROM_APPS_PAGE,
                            AppInfoLaunchSource::NUM_LAUNCH_SOURCES);

  ShowAppInfoInNativeDialog(web_ui()->GetWebContents(),
                            Profile::FromWebUI(web_ui()), extension,
                            base::Closure());
}

void AppLauncherHandler::HandleReorderApps(const base::ListValue* args) {
  CHECK(args->GetSize() == 2);

  std::string dragged_app_id;
  const base::ListValue* app_order;
  CHECK(args->GetString(0, &dragged_app_id));
  CHECK(args->GetList(1, &app_order));

  std::string predecessor_to_moved_ext;
  std::string successor_to_moved_ext;
  for (size_t i = 0; i < app_order->GetSize(); ++i) {
    std::string value;
    if (app_order->GetString(i, &value) && value == dragged_app_id) {
      if (i > 0)
        CHECK(app_order->GetString(i - 1, &predecessor_to_moved_ext));
      if (i + 1 < app_order->GetSize())
        CHECK(app_order->GetString(i + 1, &successor_to_moved_ext));
      break;
    }
  }

  // Don't update the page; it already knows the apps have been reordered.
  base::AutoReset<bool> auto_reset(&ignore_changes_, true);
  ExtensionSystem::Get(extension_service_->GetBrowserContext())
      ->app_sorting()
      ->OnExtensionMoved(dragged_app_id, predecessor_to_moved_ext,
                         successor_to_moved_ext);
}

void AppLauncherHandler::HandleSetPageIndex(const base::ListValue* args) {
  AppSorting* app_sorting =
      ExtensionSystem::Get(extension_service_->profile())->app_sorting();
  std::string extension_id;
  double page_index;
  CHECK(args->GetString(0, &extension_id));
  CHECK(args->GetDouble(1, &page_index));
  const syncer::StringOrdinal& page_ordinal =
      app_sorting->PageIntegerAsStringOrdinal(static_cast<size_t>(page_index));

  // Don't update the page; it already knows the apps have been reordered.
  base::AutoReset<bool> auto_reset(&ignore_changes_, true);
  app_sorting->SetPageOrdinal(extension_id, page_ordinal);
}

void AppLauncherHandler::HandleSaveAppPageName(const base::ListValue* args) {
  std::string name;
  CHECK(args->GetString(0, &name));

  double page_index;
  CHECK(args->GetDouble(1, &page_index));

  base::AutoReset<bool> auto_reset(&ignore_changes_, true);
  PrefService* prefs = Profile::FromWebUI(web_ui())->GetPrefs();
  ListPrefUpdate update(prefs, prefs::kNtpAppPageNames);
  base::ListValue* list = update.Get();
  list->Set(static_cast<size_t>(page_index),
            std::make_unique<base::Value>(name));
}

void AppLauncherHandler::HandleGenerateAppForLink(const base::ListValue* args) {
  std::string url;
  CHECK(args->GetString(0, &url));
  GURL launch_url(url);

  // Can only install one app at a time.
  if (attempting_web_app_install_page_ordinal_.has_value())
    return;

  base::string16 title;
  CHECK(args->GetString(1, &title));

  double page_index;
  CHECK(args->GetDouble(2, &page_index));
  AppSorting* app_sorting =
      ExtensionSystem::Get(extension_service_->profile())->app_sorting();
  const syncer::StringOrdinal& page_ordinal =
      app_sorting->PageIntegerAsStringOrdinal(static_cast<size_t>(page_index));

  Profile* profile = Profile::FromWebUI(web_ui());
  favicon::FaviconService* favicon_service =
      FaviconServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::EXPLICIT_ACCESS);
  if (!favicon_service) {
    LOG(ERROR) << "No favicon service";
    return;
  }

  std::unique_ptr<AppInstallInfo> install_info(new AppInstallInfo());
  install_info->title = base::CollapseWhitespace(
      title, /* trim_sequences_with_line_breaks */ false);
  install_info->app_url = launch_url;
  install_info->page_ordinal = page_ordinal;

  favicon_service->GetFaviconImageForPageURL(
      launch_url,
      base::BindOnce(&AppLauncherHandler::OnFaviconForAppInstallFromLink,
                     base::Unretained(this), base::Passed(&install_info)),
      &cancelable_task_tracker_);
}

void AppLauncherHandler::HandlePageSelected(const base::ListValue* args) {
  double index_double;
  CHECK(args->GetDouble(0, &index_double));
  int index = static_cast<int>(index_double);

  PrefService* prefs = Profile::FromWebUI(web_ui())->GetPrefs();
  prefs->SetInteger(prefs::kNtpShownPage, APPS_PAGE_ID | index);
}

void AppLauncherHandler::OnFaviconForAppInstallFromLink(
    std::unique_ptr<AppInstallInfo> install_info,
    const favicon_base::FaviconImageResult& image_result) {
  auto web_app = std::make_unique<WebApplicationInfo>();
  web_app->title = install_info->title;
  web_app->start_url = install_info->app_url;

  if (!image_result.image.IsEmpty()) {
    web_app->icon_bitmaps_any[image_result.image.Width()] =
        image_result.image.AsBitmap();
  }

  attempting_web_app_install_page_ordinal_ = install_info->page_ordinal;

  web_app::InstallManager::OnceInstallCallback install_complete_callback =
      base::BindOnce(
          [](base::WeakPtr<AppLauncherHandler> app_launcher_handler,
             const web_app::AppId& app_id,
             web_app::InstallResultCode install_result) {
            // Note: this installation path only happens when the user drags a
            // link to chrome://apps, hence the specific metric name.
            base::UmaHistogramEnumeration(
                "Apps.Launcher.InstallAppFromLinkResult", install_result);
            if (!app_launcher_handler)
              return;
            if (install_result !=
                web_app::InstallResultCode::kSuccessNewInstall) {
              app_launcher_handler->attempting_web_app_install_page_ordinal_ =
                  base::nullopt;
            }
          },
          weak_ptr_factory_.GetWeakPtr());

  web_app_provider_->install_manager().InstallWebAppFromInfo(
      std::move(web_app), web_app::ForInstallableSite::kUnknown,
      WebappInstallSource::SYNC, std::move(install_complete_callback));
}

void AppLauncherHandler::SetAppToBeHighlighted() {
  if (highlight_app_id_.empty())
    return;

  base::Value app_id(highlight_app_id_);
  web_ui()->CallJavascriptFunctionUnsafe("ntp.setAppToBeHighlighted", app_id);
  highlight_app_id_.clear();
}

void AppLauncherHandler::OnExtensionPreferenceChanged() {
  base::DictionaryValue dictionary;
  FillAppDictionary(&dictionary);
  web_ui()->CallJavascriptFunctionUnsafe("ntp.appsPrefChangeCallback",
                                         dictionary);
}

void AppLauncherHandler::CleanupAfterUninstall() {
  extension_id_prompting_.clear();
}

void AppLauncherHandler::PromptToEnableApp(const std::string& extension_id) {
  if (!extension_id_prompting_.empty())
    return;  // Only one prompt at a time.

  if (DesktopPWAsWithoutExtensions() &&
      web_app_provider_->registrar().IsInstalled(extension_id_prompting_)) {
    NOTIMPLEMENTED();
    return;
  }

  extension_id_prompting_ = extension_id;
  extension_enable_flow_ = std::make_unique<ExtensionEnableFlow>(
      Profile::FromWebUI(web_ui()), extension_id, this);
  extension_enable_flow_->StartForWebContents(web_ui()->GetWebContents());
}

void AppLauncherHandler::OnOsHooksInstalled(
    const web_app::AppId& app_id,
    const web_app::OsHooksResults os_hooks_results) {
  // TODO(dmurph): Once installation takes the OSHookResults bitfield, then
  // use that to compare with the results, and record if they all were
  // successful, instead of just shortcuts.
  base::UmaHistogramBoolean("Apps.Launcher.InstallLocallyShortcutsCreated",
                            os_hooks_results[web_app::OsHookType::kShortcuts]);
}

void AppLauncherHandler::OnExtensionUninstallDialogClosed(
    bool did_start_uninstall,
    const base::string16& error) {
  CleanupAfterUninstall();
}

void AppLauncherHandler::ExtensionEnableFlowFinished() {
  DCHECK_EQ(extension_id_prompting_, extension_enable_flow_->extension_id());

  // We bounce this off the NTP so the browser can update the apps icon.
  // If we don't launch the app asynchronously, then the app's disabled
  // icon disappears but isn't replaced by the enabled icon, making a poor
  // visual experience.
  base::Value app_id(extension_id_prompting_);
  web_ui()->CallJavascriptFunctionUnsafe("ntp.launchAppAfterEnable", app_id);

  extension_enable_flow_.reset();
  extension_id_prompting_ = "";
}

void AppLauncherHandler::ExtensionEnableFlowAborted(bool user_initiated) {
  DCHECK_EQ(extension_id_prompting_, extension_enable_flow_->extension_id());

  if (DesktopPWAsWithoutExtensions() &&
      web_app_provider_->registrar().IsInstalled(extension_id_prompting_)) {
    NOTIMPLEMENTED();
    return;
  }

  // We record the histograms here because ExtensionUninstallCanceled is also
  // called when the extension uninstall dialog is canceled.
  const Extension* extension =
      extensions::ExtensionRegistry::Get(extension_service_->profile())
          ->GetExtensionById(extension_id_prompting_,
                             extensions::ExtensionRegistry::ENABLED |
                                 extensions::ExtensionRegistry::DISABLED |
                                 extensions::ExtensionRegistry::TERMINATED);
  std::string histogram_name = user_initiated ? "ReEnableCancel"
                                              : "ReEnableAbort";
  extensions::ExtensionService::RecordPermissionMessagesHistogram(
      extension, histogram_name.c_str());

  extension_enable_flow_.reset();
  CleanupAfterUninstall();
}

extensions::ExtensionUninstallDialog*
AppLauncherHandler::CreateExtensionUninstallDialog() {
  Browser* browser =
      chrome::FindBrowserWithWebContents(web_ui()->GetWebContents());
  extension_uninstall_dialog_ = extensions::ExtensionUninstallDialog::Create(
      extension_service_->profile(), browser->window()->GetNativeWindow(),
      this);
  return extension_uninstall_dialog_.get();
}

void AppLauncherHandler::ExtensionRemoved(const Extension* extension,
                                          bool is_uninstall) {
  DCHECK(!FromBookmark(extension));
  if (!ShouldShow(extension))
    return;

  std::unique_ptr<base::DictionaryValue> app_info(GetExtensionInfo(extension));
  if (!app_info.get())
    return;

  web_ui()->CallJavascriptFunctionUnsafe(
      "ntp.appRemoved", *app_info, base::Value(is_uninstall),
      base::Value(!extension_id_prompting_.empty()));
}

bool AppLauncherHandler::ShouldShow(const Extension* extension) const {
  if (ignore_changes_ || !has_loaded_apps_ || !extension->is_app())
    return false;

  Profile* profile = Profile::FromWebUI(web_ui());
  return extensions::ui_util::ShouldDisplayInNewTabPage(extension, profile);
}
