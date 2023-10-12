// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ntp/app_launcher_handler.h"

#include <stddef.h>

#include <string>
#include <utility>

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/rtl.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_source.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/browser_app_launcher.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_ui_util.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/apps/app_info_dialog.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/extensions/app_launch_params.h"
#include "chrome/browser/ui/extensions/extension_enable_flow.h"
#include "chrome/browser/ui/tab_dialogs.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/extensions/extension_basic_info.h"
#include "chrome/browser/ui/webui/extensions/extension_icon_source.h"
#include "chrome/browser/ui/webui/ntp/new_tab_ui.h"
#include "chrome/browser/web_applications/extension_status_utils.h"
#include "chrome/browser/web_applications/extensions/bookmark_app_util.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_install_params.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_metrics.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/favicon_base/favicon_types.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/browser/uninstall_result_code.h"
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
#include "extensions/common/extension_id.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "net/base/url_util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
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

const char kRunOnOsLoginModeNotRun[] = "run_on_os_login_mode_not_run";
const char kRunOnOsLoginModeWindowed[] = "run_on_os_login_mode_windowed";

// The Youtube app is incorrectly harded to be a 'bookmark app'. However, it is
// a platform app.
// TODO(crbug.com/1065748): Remove this hack once the youtube app is fixed.
bool IsYoutubeExtension(const std::string& extension_id) {
  return extension_id == extension_misc::kYoutubeAppId;
}

void GetWebAppBasicInfo(const webapps::AppId& app_id,
                        const web_app::WebAppRegistrar& app_registrar,
                        base::Value::Dict* info) {
  info->Set(kInfoIdKey, app_id);
  info->Set(kInfoNameKey, app_registrar.GetAppShortName(app_id));
  info->Set(kEnabledKey, true);
  info->Set(kKioskEnabledKey, false);
  info->Set(kKioskOnlyKey, false);
  info->Set(kOfflineEnabledKey, true);
  info->Set(kDescriptionKey, app_registrar.GetAppDescription(app_id));
  info->Set(kPackagedAppKey, false);
}

bool HasMatchingOrGreaterThanIcon(
    const web_app::SortedSizesPx& downloaded_icon_sizes,
    int pixels) {
  if (downloaded_icon_sizes.empty())
    return false;
  web_app::SquareSizePx largest = *downloaded_icon_sizes.rbegin();
  return largest >= pixels;
}

// Query string for showing the deprecation dialog with deletion options.
const char kDeprecationDialogQueryString[] = "showDeletionDialog";
// Query string for showing the force installed apps deprecation dialog.
// Should match with kChromeUIAppsWithForceInstalledDeprecationDialogURL.
const char kForceInstallDialogQueryString[] = "showForceInstallDialog";

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
  Profile* webui_profile = Profile::FromWebUI(web_ui());
  ExtensionRegistry::Get(webui_profile)->RemoveObserver(this);
  // Destroy `extension_uninstall_dialog_` now, since `this` is an
  // `ExtensionUninstallDialog::Delegate` and the dialog may call back into
  // `this` when destroyed.
  extension_uninstall_dialog_.reset();
}

base::Value::Dict AppLauncherHandler::CreateWebAppInfo(
    const webapps::AppId& app_id) {
  base::Value::Dict dict;

  // Communicate the kiosk flag so the apps page can disable showing the
  // context menu in kiosk mode.
  dict.Set("kioskMode", base::CommandLine::ForCurrentProcess()->HasSwitch(
                            switches::kKioskMode));

  auto& registrar = web_app_provider_->registrar_unsafe();

  std::u16string name = base::UTF8ToUTF16(registrar.GetAppShortName(app_id));
  NewTabUI::SetUrlTitleAndDirection(&dict, name,
                                    registrar.GetAppStartUrl(app_id));
  NewTabUI::SetFullNameAndDirection(name, &dict);

  GetWebAppBasicInfo(app_id, registrar, &dict);

  dict.Set("mayDisable", registrar.CanUserUninstallWebApp(app_id));
  bool is_locally_installed = registrar.IsLocallyInstalled(app_id);
  dict.Set("mayChangeLaunchType", is_locally_installed);

  // Any locally installed app can have shortcuts created.
  dict.Set("mayCreateShortcuts", is_locally_installed);
  dict.Set("isLocallyInstalled", is_locally_installed);

  const bool hide_display_mode = registrar.IsIsolated(app_id);
  dict.Set("hideDisplayMode", hide_display_mode);

  absl::optional<std::string> icon_big;
  absl::optional<std::string> icon_small;

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

  dict.Set("icon_big_exists", icon_big.has_value());
  dict.Set("icon_big", icon_big.value_or(GURL().spec()));
  dict.Set("icon_small_exists", icon_small.has_value());
  dict.Set("icon_small", icon_small.value_or(GURL().spec()));

  extensions::LaunchContainerAndType result =
      extensions::GetLaunchContainerAndTypeFromDisplayMode(
          registrar.GetAppEffectiveDisplayMode(app_id));
  dict.Set("launch_container", static_cast<int>(result.launch_container));
  dict.Set("launch_type", result.launch_type);
  dict.Set("is_component", false);
  dict.Set("is_webstore", false);

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
  dict.Set("page_index", sorting->PageStringOrdinalAsInteger(page_ordinal));

  syncer::StringOrdinal app_launch_ordinal =
      sorting->GetAppLaunchOrdinal(app_id);
  if (!app_launch_ordinal.IsValid()) {
    // Make sure every app has a launch ordinal (some predate the launch
    // ordinal).
    app_launch_ordinal = sorting->CreateNextAppLaunchOrdinal(page_ordinal);
    sorting->SetAppLaunchOrdinal(app_id, app_launch_ordinal);
  }
  dict.Set("app_launch_ordinal", app_launch_ordinal.ToInternalValue());

  // Only show the Run on OS Login menu item for locally installed web apps
  dict.Set("mayShowRunOnOsLoginMode",
           base::FeatureList::IsEnabled(features::kDesktopPWAsRunOnOsLogin) &&
               is_locally_installed);

  const auto login_mode = registrar.GetAppRunOnOsLoginMode(app_id);
  dict.Set("mayToggleRunOnOsLoginMode", login_mode.user_controllable);
  dict.Set("runOnOsLoginMode",
           (login_mode.value == web_app::RunOnOsLoginMode::kNotRun)
               ? kRunOnOsLoginModeNotRun
               : kRunOnOsLoginModeWindowed);

  // Show settings instead of App info for locally installed web apps.
  if (is_locally_installed) {
    dict.Set("settingsMenuItemOverrideText",
             l10n_util::GetStringUTF16(IDS_WEB_APP_SETTINGS_LINK));
  }
  return dict;
}

base::Value::Dict AppLauncherHandler::CreateExtensionInfo(
    const Extension* extension) {
  base::Value::Dict dict;

  // Communicate the kiosk flag so the apps page can disable showing the
  // context menu in kiosk mode.
  dict.Set("kioskMode", base::CommandLine::ForCurrentProcess()->HasSwitch(
                            switches::kKioskMode));

  bool is_deprecated_app = false;
  auto* context = extension_service_->GetBrowserContext();
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_FUCHSIA)
  is_deprecated_app =
      extensions::IsExtensionUnsupportedDeprecatedApp(context, extension->id());
#endif
  dict.Set("is_deprecated_app", is_deprecated_app);

  // The Extension class 'helpfully' wraps bidi control characters that
  // impede our ability to determine directionality.
  std::u16string short_name = base::UTF8ToUTF16(extension->short_name());
  base::i18n::UnadjustStringForLocaleDirection(&short_name);
  NewTabUI::SetUrlTitleAndDirection(
      &dict, short_name,
      extensions::AppLaunchInfo::GetFullLaunchURL(extension));

  std::u16string name = base::UTF8ToUTF16(extension->name());
  base::i18n::UnadjustStringForLocaleDirection(&name);
  if (is_deprecated_app && !extensions::IsExtensionForceInstalled(
                               context, extension->id(), nullptr)) {
    name = l10n_util::GetStringFUTF16(IDS_APPS_PAGE_DEPRECATED_APP_TITLE, name);
    deprecated_app_ids_.insert(extension->id());
  }
  NewTabUI::SetFullNameAndDirection(name, &dict);

  bool enabled = extension_service_->IsExtensionEnabled(extension->id()) &&
                 !extensions::ExtensionRegistry::Get(
                      extension_service_->GetBrowserContext())
                      ->terminated_extensions()
                      .GetByID(extension->id());
  extensions::GetExtensionBasicInfo(extension, enabled, &dict);

  dict.Set("mayDisable",
           extensions::ExtensionSystem::Get(extension_service_->profile())
               ->management_policy()
               ->UserMayModifySettings(extension, nullptr));

  bool is_locally_installed =
      !extension->is_hosted_app() ||
      BookmarkAppIsLocallyInstalled(extension_service_->profile(), extension);
  dict.Set("mayChangeLaunchType",
           !extension->is_platform_app() && is_locally_installed);

  // Any locally installed app can have shortcuts created.
  dict.Set(
      "mayCreateShortcuts",
      is_locally_installed && extension->id() != extensions::kWebStoreAppId);
  dict.Set("isLocallyInstalled", is_locally_installed);

  auto icon_size = extension_misc::EXTENSION_ICON_LARGE;
  auto match_type = ExtensionIconSet::MATCH_BIGGER;
  bool has_non_default_large_icon =
      !extensions::IconsInfo::GetIconURL(extension, icon_size, match_type)
           .is_empty();
  GURL large_icon = extensions::ExtensionIconSource::GetIconURL(
      extension, icon_size, match_type, false);
  dict.Set("icon_big", large_icon.spec());
  dict.Set("icon_big_exists", has_non_default_large_icon);

  icon_size = extension_misc::EXTENSION_ICON_BITTY;
  bool has_non_default_small_icon =
      !extensions::IconsInfo::GetIconURL(extension, icon_size, match_type)
           .is_empty();
  GURL small_icon = extensions::ExtensionIconSource::GetIconURL(
      extension, icon_size, match_type, false);
  dict.Set("icon_small", small_icon.spec());
  dict.Set("icon_small_exists", has_non_default_small_icon);

  dict.Set("launch_container",
           static_cast<int>(
               extensions::AppLaunchInfo::GetLaunchContainer(extension)));
  ExtensionPrefs* prefs = ExtensionPrefs::Get(extension_service_->profile());
  dict.Set("launch_type", extensions::GetLaunchType(prefs, extension));
  dict.Set("is_component", extension->location() ==
                               extensions::mojom::ManifestLocation::kComponent);
  dict.Set("is_webstore", extension->id() == extensions::kWebStoreAppId);

  AppSorting* sorting =
      ExtensionSystem::Get(extension_service_->profile())->app_sorting();
  syncer::StringOrdinal page_ordinal = sorting->GetPageOrdinal(extension->id());
  if (!page_ordinal.IsValid()) {
    // Make sure every app has a page ordinal (some predate the page ordinal).
    // The webstore app should be on the first page.
    page_ordinal = extension->id() == extensions::kWebStoreAppId
                       ? sorting->CreateFirstAppPageOrdinal()
                       : sorting->GetNaturalAppPageOrdinal();
    sorting->SetPageOrdinal(extension->id(), page_ordinal);
  }
  dict.Set("page_index", sorting->PageStringOrdinalAsInteger(page_ordinal));

  syncer::StringOrdinal app_launch_ordinal =
      sorting->GetAppLaunchOrdinal(extension->id());
  if (!app_launch_ordinal.IsValid()) {
    // Make sure every app has a launch ordinal (some predate the launch
    // ordinal). The webstore's app launch ordinal is always set to the first
    // position.
    app_launch_ordinal =
        extension->id() == extensions::kWebStoreAppId
            ? sorting->CreateFirstAppLaunchOrdinal(page_ordinal)
            : sorting->CreateNextAppLaunchOrdinal(page_ordinal);
    sorting->SetAppLaunchOrdinal(extension->id(), app_launch_ordinal);
  }
  dict.Set("app_launch_ordinal", app_launch_ordinal.ToInternalValue());

  // Run on OS Login is not implemented for extension/bookmark apps.
  dict.Set("mayShowRunOnOsLoginMode", false);
  dict.Set("mayToggleRunOnOsLoginMode", false);

  dict.Set("hideDisplayMode", false);
  return dict;
}

// static
void AppLauncherHandler::RegisterLoadTimeData(
    Profile* profile,
    content::WebUIDataSource* source) {
  PrefService* prefs = profile->GetPrefs();
  int shown_page = prefs->GetInteger(prefs::kNtpShownPage);
  source->AddInteger("shown_page_index", shown_page & INDEX_MASK);
}

// static
void AppLauncherHandler::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterIntegerPref(prefs::kNtpShownPage, APPS_PAGE_ID);
}

void AppLauncherHandler::RegisterMessages() {
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
                          base::Unretained(this), base::DoNothing()));
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
  web_ui()->RegisterMessageCallback(
      "runOnOsLogin",
      base::BindRepeating(&AppLauncherHandler::HandleRunOnOsLogin,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "deprecatedDialogLinkClicked",
      base::BindRepeating(&AppLauncherHandler::HandleLaunchDeprecatedAppDialog,
                          base::Unretained(this)));
}

void AppLauncherHandler::OnAppsReordered(
    content::BrowserContext* context,
    const absl::optional<std::string>& extension_id) {
  if (ignore_changes_ || !has_loaded_apps_)
    return;

  if (extension_id) {
    base::Value::Dict app_info;
    if (web_app_provider_->registrar_unsafe().IsInstalled(*extension_id)) {
      app_info = CreateWebAppInfo(*extension_id);
    } else {
      const Extension* extension =
          ExtensionRegistry::Get(extension_service_->profile())
              ->GetInstalledExtension(*extension_id);
      if (!extension) {
        // Extension could still be downloading or installing.
        return;
      }

      app_info = CreateExtensionInfo(extension);
    }
    web_ui()->CallJavascriptFunctionUnsafe("ntp.appMoved", std::move(app_info));
  } else {
    HandleGetApps(base::Value::List());
  }
}

void AppLauncherHandler::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const Extension* extension) {
  if (!ShouldShow(extension))
    return;

  visible_apps_.insert(extension->id());
  web_ui()->CallJavascriptFunctionUnsafe("ntp.appAdded",
                                         GetExtensionInfo(extension),
                                         /*highlight=*/base::Value(false));
}

void AppLauncherHandler::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    extensions::UnloadedExtensionReason reason) {
  ExtensionRemoved(extension, /*is_uninstall=*/false);
}

void AppLauncherHandler::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    extensions::UninstallReason reason) {
  ExtensionRemoved(extension, /*is_uninstall=*/true);
}

void AppLauncherHandler::OnWebAppInstalled(const webapps::AppId& app_id) {
  if (attempting_web_app_install_page_ordinal_.has_value()) {
    AppSorting* sorting =
        ExtensionSystem::Get(Profile::FromWebUI(web_ui()))->app_sorting();
    sorting->SetPageOrdinal(app_id,
                            attempting_web_app_install_page_ordinal_.value());
  }

  visible_apps_.insert(app_id);
  base::Value highlight(attempting_web_app_install_page_ordinal_.has_value());
  attempting_web_app_install_page_ordinal_ = absl::nullopt;
  web_ui()->CallJavascriptFunctionUnsafe("ntp.appAdded",
                                         CreateWebAppInfo(app_id), highlight);
}

void AppLauncherHandler::OnWebAppFirstInstallTimeChanged(
    const webapps::AppId& app_id,
    const base::Time& time) {
  // Use the appAdded to update the app icon's color to no longer be
  // greyscale.
  web_ui()->CallJavascriptFunctionUnsafe("ntp.appAdded",
                                         CreateWebAppInfo(app_id));
}

void AppLauncherHandler::OnWebAppWillBeUninstalled(
    const webapps::AppId& app_id) {
  base::Value::Dict app_info;
  // Since `isUninstall` is true below, the only item needed in the app_info
  // dictionary is the id.
  app_info.Set(kInfoIdKey, app_id);
  web_ui()->CallJavascriptFunctionUnsafe(
      "ntp.appRemoved", std::move(app_info),
      /*isUninstall=*/base::Value(true),
      base::Value(!extension_id_prompting_.empty()));
}

void AppLauncherHandler::OnWebAppUninstalled(
    const webapps::AppId& app_id,
    webapps::WebappUninstallSource uninstall_source) {
  // This can be redundant in most cases, however it is not uncommon for the
  // chrome://apps page to be loaded, or reloaded, during the uninstallation of
  // an app. In this state, the app is still in the registry, but the
  // |OnWebAppWillBeUninstalled| event has already been sent. Thus we also
  // listen to this event, to ensure that the app is removed.
  base::Value::Dict app_info;
  // Since `isUninstall` is true below, the only item needed in the app_info
  // dictionary is the id.
  app_info.Set(kInfoIdKey, app_id);
  web_ui()->CallJavascriptFunctionUnsafe(
      "ntp.appRemoved", std::move(app_info),
      /*isUninstall=*/base::Value(true),
      base::Value(!extension_id_prompting_.empty()));
}

void AppLauncherHandler::OnWebAppRunOnOsLoginModeChanged(
    const webapps::AppId& app_id,
    web_app::RunOnOsLoginMode run_on_os_login_mode) {
  CallJavascriptFunction("ntp.appAdded", base::Value(CreateWebAppInfo(app_id)));
}

void AppLauncherHandler::OnWebAppSettingsPolicyChanged() {
  HandleGetApps(base::Value::List());
}

void AppLauncherHandler::OnWebAppInstallManagerDestroyed() {
  web_apps_observation_.Reset();
  install_manager_observation_.Reset();
}

void AppLauncherHandler::OnAppRegistrarDestroyed() {
  web_apps_observation_.Reset();
  install_manager_observation_.Reset();
}

void AppLauncherHandler::FillAppDictionary(base::Value::Dict* dictionary) {
  // CreateExtensionInfo and ClearOrdinals can change the extension prefs.
  base::AutoReset<bool> auto_reset(&ignore_changes_, true);

  base::Value::List installed_extensions;
  Profile* profile = Profile::FromWebUI(web_ui());
  PrefService* prefs = profile->GetPrefs();

  std::set<webapps::AppId> web_app_ids;
  web_app::WebAppRegistrar& registrar = web_app_provider_->registrar_unsafe();
  for (const webapps::AppId& web_app_id : registrar.GetAppIds()) {
    // The Youtube app is harded to be a 'bookmark app', however it is not, it
    // is a platform app.
    // TODO(crbug.com/1065748): Remove this hack once the youtube app is fixed.
    if (IsYoutubeExtension(web_app_id))
      continue;
    installed_extensions.Append(base::Value(CreateWebAppInfo(web_app_id)));
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
      installed_extensions.Append(base::Value(GetExtensionInfo(extension)));
    }
  }

  dictionary->Set("apps", std::move(installed_extensions));
  if (deprecated_app_ids_.size() > 0)
    dictionary->Set(
        "deprecatedAppsDialogLinkText",
        l10n_util::GetPluralStringFUTF16(IDS_DEPRECATED_APPS_DELETION_LINK,
                                         deprecated_app_ids_.size()));

  const base::Value::List& app_page_names =
      prefs->GetList(prefs::kNtpAppPageNames);
  if (!app_page_names.size()) {
    ScopedListPrefUpdate update(prefs, prefs::kNtpAppPageNames);
    base::Value::List& list = update.Get();
    list.Append(l10n_util::GetStringUTF16(IDS_APP_DEFAULT_PAGE_NAME));
    dictionary->Set("appPageNames", list.Clone());
  } else {
    dictionary->Set("appPageNames", app_page_names.Clone());
  }
}

base::Value::Dict AppLauncherHandler::GetExtensionInfo(
    const Extension* extension) {
  // CreateExtensionInfo can change the extension prefs.
  base::AutoReset<bool> auto_reset(&ignore_changes_, true);
  return CreateExtensionInfo(extension);
}

void AppLauncherHandler::HandleGetApps(const base::Value::List& args) {
  AllowJavascript();
  base::Value::Dict dictionary;

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
      visible_apps_.insert((*it)->id());
    }

    const ExtensionSet& disabled_set = registry->disabled_extensions();
    for (ExtensionSet::const_iterator it = disabled_set.begin();
         it != disabled_set.end(); ++it) {
      visible_apps_.insert((*it)->id());
    }

    const ExtensionSet& terminated_set = registry->terminated_extensions();
    for (ExtensionSet::const_iterator it = terminated_set.begin();
         it != terminated_set.end(); ++it) {
      visible_apps_.insert((*it)->id());
    }
  }

  FillAppDictionary(&dictionary);
  web_ui()->CallJavascriptFunctionUnsafe("ntp.getAppsCallback",
                                         std::move(dictionary));

  // First time we get here we set up the observer so that we can tell update
  // the apps as they change.
  if (!has_loaded_apps_) {
    base::RepeatingClosure callback =
        base::BindRepeating(&AppLauncherHandler::OnExtensionPreferenceChanged,
                            base::Unretained(this));
    extension_pref_change_registrar_.Init(
        ExtensionPrefs::Get(profile)->pref_service());
    extension_pref_change_registrar_.Add(extensions::pref_names::kExtensions,
                                         callback);
    extension_pref_change_registrar_.Add(prefs::kNtpAppPageNames, callback);

    ExtensionRegistry::Get(profile)->AddObserver(this);
    install_tracker_observation_.Observe(
        extensions::InstallTracker::Get(profile));
    web_apps_observation_.Observe(&web_app_provider_->registrar_unsafe());
    install_manager_observation_.Observe(&web_app_provider_->install_manager());

    WebContents* web_contents = web_ui()->GetWebContents();
    std::string app_id;
    if (net::GetValueForKeyInQuery(web_contents->GetLastCommittedURL(),
                                   kDeprecationDialogQueryString, &app_id)) {
      if (extensions::IsExtensionUnsupportedDeprecatedApp(profile, app_id) &&
          !deprecated_app_ids_.empty()) {
        TabDialogs::FromWebContents(web_contents)
            ->ShowDeprecatedAppsDialog(
                app_id, deprecated_app_ids_, web_contents,
                base::BindOnce(
                    &AppLauncherHandler::LaunchApp,
                    weak_ptr_factory_.GetWeakPtr(), app_id,
                    extension_misc::AppLaunchBucket::APP_LAUNCH_CMD_LINE_APP,
                    "", WindowOpenDisposition::CURRENT_TAB, true));
      }
    }
    if (net::GetValueForKeyInQuery(web_contents->GetLastCommittedURL(),
                                   kForceInstallDialogQueryString, &app_id)) {
      if (extensions::IsExtensionUnsupportedDeprecatedApp(profile, app_id) &&
          extensions::IsExtensionForceInstalled(profile, app_id, nullptr)) {
        if (extensions::IsPreinstalledAppId(app_id)) {
          TabDialogs::FromWebContents(web_contents)
              ->ShowForceInstalledPreinstalledDeprecatedAppDialog(app_id,
                                                                  web_contents);
        } else {
          TabDialogs::FromWebContents(web_contents)
              ->ShowForceInstalledDeprecatedAppsDialog(
                  app_id, web_contents,
                  base::BindOnce(
                      &AppLauncherHandler::LaunchApp,
                      weak_ptr_factory_.GetWeakPtr(), app_id,
                      extension_misc::AppLaunchBucket::APP_LAUNCH_CMD_LINE_APP,
                      "", WindowOpenDisposition::CURRENT_TAB, true));
        }
      }
    }
  }
  has_loaded_apps_ = true;
}

void AppLauncherHandler::HandleLaunchApp(const base::Value::List& args) {
  const std::string& extension_id = args[0].GetString();
  double source = args[1].GetDouble();

  extension_misc::AppLaunchBucket launch_bucket =
      static_cast<extension_misc::AppLaunchBucket>(static_cast<int>(source));
  CHECK(launch_bucket >= 0 &&
        launch_bucket < extension_misc::APP_LAUNCH_BUCKET_BOUNDARY);

  WindowOpenDisposition disposition =
      args.size() > 3 ? webui::GetDispositionFromClick(args, 3)
                      : WindowOpenDisposition::CURRENT_TAB;
  std::string source_value;
  if (args.size() > 2) {
    source_value = args[2].GetString();
  }
  LaunchApp(extension_id, launch_bucket, source_value, disposition, false);
}

void AppLauncherHandler::LaunchApp(
    std::string extension_id,
    extension_misc::AppLaunchBucket launch_bucket,
    const std::string& source_value,
    WindowOpenDisposition disposition,
    bool force_launch_deprecated_apps) {
  Profile* profile = extension_service_->profile();

  if (!force_launch_deprecated_apps &&
      extensions::IsExtensionUnsupportedDeprecatedApp(profile, extension_id) &&
      base::FeatureList::IsEnabled(features::kChromeAppsDeprecation)) {
    if (!extensions::IsExtensionForceInstalled(profile, extension_id,
                                               nullptr)) {
      TabDialogs::FromWebContents(web_ui()->GetWebContents())
          ->ShowDeprecatedAppsDialog(
              extension_id, deprecated_app_ids_, web_ui()->GetWebContents(),
              base::BindOnce(&AppLauncherHandler::LaunchApp,
                             weak_ptr_factory_.GetWeakPtr(), extension_id,
                             launch_bucket, source_value, disposition, true));
      return;
    } else if (extensions::IsPreinstalledAppId(extension_id)) {
      TabDialogs::FromWebContents(web_ui()->GetWebContents())
          ->ShowForceInstalledPreinstalledDeprecatedAppDialog(
              extension_id, web_ui()->GetWebContents());
      return;
    } else {
      TabDialogs::FromWebContents(web_ui()->GetWebContents())
          ->ShowForceInstalledDeprecatedAppsDialog(
              extension_id, web_ui()->GetWebContents(),
              base::BindOnce(&AppLauncherHandler::LaunchApp,
                             weak_ptr_factory_.GetWeakPtr(), extension_id,
                             launch_bucket, source_value, disposition, true));
      return;
    }
  }

  extensions::Manifest::Type type;
  GURL full_launch_url;
  apps::LaunchContainer launch_container;

  web_app::WebAppRegistrar& registrar = web_app_provider_->registrar_unsafe();
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
    type = extension->GetType();
    full_launch_url = extensions::AppLaunchInfo::GetFullLaunchURL(extension);
    launch_container =
        extensions::GetLaunchContainer(ExtensionPrefs::Get(profile), extension);
  }

  GURL override_url;
  if (extension_id != extensions::kWebStoreAppId) {
    CHECK_NE(launch_bucket, extension_misc::APP_LAUNCH_BUCKET_INVALID);
    extensions::RecordAppLaunchType(launch_bucket, type);
  } else {
    extensions::RecordWebStoreLaunch();
    if (!source_value.empty()) {
      override_url = net::AppendQueryParameter(
          full_launch_url, extension_urls::kWebstoreSourceField, source_value);
    }
  }

  if (disposition == WindowOpenDisposition::NEW_FOREGROUND_TAB ||
      disposition == WindowOpenDisposition::NEW_BACKGROUND_TAB ||
      disposition == WindowOpenDisposition::NEW_WINDOW) {
    // TODO(jamescook): Proper support for background tabs.
    apps::AppLaunchParams params(
        extension_id,
        disposition == WindowOpenDisposition::NEW_WINDOW
            ? apps::LaunchContainer::kLaunchContainerWindow
            : apps::LaunchContainer::kLaunchContainerTab,
        disposition, apps::LaunchSource::kFromNewTabPage);
    params.override_url = override_url;
    apps::AppServiceProxyFactory::GetForProfile(profile)
        ->BrowserAppLauncher()
        ->LaunchAppWithParams(std::move(params), base::DoNothing());
  } else {
    // To give a more "launchy" experience when using the NTP launcher, we close
    // it automatically. However, if the chrome://apps page is the LAST page in
    // the browser window, then we don't close it.
    Browser* browser = chrome::FindBrowserWithTab(web_ui()->GetWebContents());
    base::WeakPtr<Browser> browser_ptr;
    WebContents* old_contents = nullptr;
    base::WeakPtr<WebContents> old_contents_ptr;
    if (browser) {
      browser_ptr = browser->AsWeakPtr();
      old_contents = browser->tab_strip_model()->GetActiveWebContents();
      old_contents_ptr = old_contents->GetWeakPtr();
    }

    apps::AppLaunchParams params(
        extension_id, launch_container,
        old_contents ? WindowOpenDisposition::CURRENT_TAB
                     : WindowOpenDisposition::NEW_FOREGROUND_TAB,
        apps::LaunchSource::kFromNewTabPage);
    params.override_url = override_url;
    apps::AppServiceProxyFactory::GetForProfile(profile)
        ->BrowserAppLauncher()
        ->LaunchAppWithParams(
            std::move(params),
            base::BindOnce(
                [](base::WeakPtr<Browser> apps_page_browser,
                   base::WeakPtr<WebContents> old_contents,
                   content::WebContents* new_web_contents) {
                  if (!apps_page_browser || !old_contents)
                    return;
                  if (new_web_contents != old_contents.get() &&
                      apps_page_browser->tab_strip_model()->count() > 1) {
                    // This will also destroy the handler, so do not perform
                    // any actions after.
                    chrome::CloseWebContents(apps_page_browser.get(),
                                             old_contents.get(),
                                             /*add_to_history=*/true);
                  }
                },
                browser_ptr, old_contents_ptr));
  }
}

void AppLauncherHandler::HandleSetLaunchType(const base::Value::List& args) {
  const std::string& app_id = args[0].GetString();
  double launch_type_double = args[1].GetDouble();
  extensions::LaunchType launch_type =
      static_cast<extensions::LaunchType>(static_cast<int>(launch_type_double));

  if (web_app_provider_->registrar_unsafe().IsInstalled(app_id)) {
    // Don't update the page; it already knows about the launch type change.
    base::AutoReset<bool> auto_reset(&ignore_changes_, true);
    web_app::mojom::UserDisplayMode user_display_mode =
        web_app::mojom::UserDisplayMode::kBrowser;
    switch (launch_type) {
      case extensions::LAUNCH_TYPE_FULLSCREEN:
      case extensions::LAUNCH_TYPE_WINDOW:
        user_display_mode = web_app::mojom::UserDisplayMode::kStandalone;
        break;
      case extensions::LAUNCH_TYPE_PINNED:
      case extensions::LAUNCH_TYPE_REGULAR:
        user_display_mode = web_app::mojom::UserDisplayMode::kBrowser;
        break;
      case extensions::LAUNCH_TYPE_INVALID:
      case extensions::NUM_LAUNCH_TYPES:
        NOTREACHED();
        break;
    }

    web_app_provider_->sync_bridge_unsafe().SetAppUserDisplayMode(
        app_id, user_display_mode, /*is_user_action=*/true);
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

  // Don't update the page; it already knows about the launch type change.
  base::AutoReset<bool> auto_reset(&ignore_changes_, true);
  extensions::SetLaunchType(Profile::FromWebUI(web_ui()), app_id, launch_type);
}

void AppLauncherHandler::HandleUninstallApp(const base::Value::List& args) {
  const std::string& extension_id = args[0].GetString();

  if (web_app_provider_->registrar_unsafe().IsInstalled(extension_id) &&
      !IsYoutubeExtension(extension_id)) {
    if (!extension_id_prompting_.empty())
      return;  // Only one prompt at a time.
    if (!web_app_provider_->registrar_unsafe().CanUserUninstallWebApp(
            extension_id)) {
      LOG(ERROR) << "Attempt to uninstall a webapp that is non-usermanagable "
                 << "was made. App id : " << extension_id;
      return;
    }

    extension_id_prompting_ = extension_id;
    const bool dont_confirm =
        args.size() >= 2 && args[1].is_bool() && args[1].GetBool();

    if (dont_confirm) {
      auto uninstall_success_callback = base::BindOnce(
          [](base::WeakPtr<AppLauncherHandler> app_launcher_handler,
             webapps::UninstallResultCode code) {
            if (app_launcher_handler) {
              app_launcher_handler->CleanupAfterUninstall();
            }
          },
          weak_ptr_factory_.GetWeakPtr());

      base::AutoReset<bool> auto_reset(&ignore_changes_, true);
      web_app_provider_->scheduler().UninstallWebApp(
          extension_id_prompting_, webapps::WebappUninstallSource::kAppsPage,
          std::move(uninstall_success_callback));
    } else {
      auto uninstall_success_callback = base::BindOnce(
          [](base::WeakPtr<AppLauncherHandler> app_launcher_handler,
             webapps::UninstallResultCode code) {
            if (app_launcher_handler) {
              app_launcher_handler->CleanupAfterUninstall();
            }
          },
          weak_ptr_factory_.GetWeakPtr());

      Browser* browser = chrome::FindBrowserWithTab(web_ui()->GetWebContents());
      web_app_provider_->ui_manager().PresentUserUninstallDialog(
          extension_id_prompting_, webapps::WebappUninstallSource::kAppsPage,
          browser->window(), std::move(uninstall_success_callback));
    }
    return;
  }
  const Extension* extension =
      ExtensionRegistry::Get(extension_service_->profile())
          ->GetInstalledExtension(extension_id);
  if (!extension)
    return;

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

  const bool dont_confirm =
      args.size() >= 2 && args[1].is_bool() && args[1].GetBool();
  if (dont_confirm) {
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

void AppLauncherHandler::HandleCreateAppShortcut(
    base::OnceClosure done,
    const base::Value::List& args) {
  const std::string& app_id = args[0].GetString();
  if (web_app_provider_->registrar_unsafe().IsInstalled(app_id) &&
      !IsYoutubeExtension(app_id)) {
    Browser* browser = chrome::FindBrowserWithTab(web_ui()->GetWebContents());
    chrome::ShowCreateChromeAppShortcutsDialog(
        browser->window()->GetNativeWindow(), browser->profile(), app_id,
        base::BindOnce(
            [](base::OnceClosure done, bool success) {
              base::UmaHistogramBoolean(
                  "Apps.AppInfoDialog.CreateWebAppShortcutSuccess", success);
              std::move(done).Run();
            },
            std::move(done)));
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

  Browser* browser = chrome::FindBrowserWithTab(web_ui()->GetWebContents());
  chrome::ShowCreateChromeAppShortcutsDialog(
      browser->window()->GetNativeWindow(), browser->profile(), extension,
      base::IgnoreArgs<bool>(std::move(done)));
}

void AppLauncherHandler::HandleInstallAppLocally(
    const base::Value::List& args) {
  const std::string& app_id = args[0].GetString();

  web_app_provider_->scheduler().InstallAppLocally(app_id, base::DoNothing());
}

void AppLauncherHandler::HandleShowAppInfo(const base::Value::List& args) {
  const std::string& extension_id = args[0].GetString();

  if (web_app_provider_->registrar_unsafe().IsInstalled(extension_id) &&
      !IsYoutubeExtension(extension_id)) {
    // This assumes the AppLauncherHandler is only used by chrome://apps page.
    // It needs to be updated if it's also used by other surfaces.
    chrome::ShowWebAppSettings(
        chrome::FindBrowserWithTab(web_ui()->GetWebContents()), extension_id,
        web_app::AppSettingsPageEntryPoint::kChromeAppsPage);
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

  ShowAppInfoInNativeDialog(web_ui()->GetWebContents(),
                            Profile::FromWebUI(web_ui()), extension,
                            base::DoNothing());
}

void AppLauncherHandler::HandleReorderApps(const base::Value::List& args_list) {
  CHECK_EQ(args_list.size(), 2u);

  const std::string& dragged_app_id = args_list[0].GetString();
  const base::Value::List& app_order = args_list[1].GetList();

  std::string predecessor_to_moved_ext;
  std::string successor_to_moved_ext;
  for (size_t i = 0; i < app_order.size(); ++i) {
    const std::string* value = app_order[i].GetIfString();
    if (value && *value == dragged_app_id) {
      if (i > 0)
        predecessor_to_moved_ext = app_order[i - 1].GetString();
      if (i + 1 < app_order.size())
        successor_to_moved_ext = app_order[i + 1].GetString();
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

void AppLauncherHandler::HandleSetPageIndex(const base::Value::List& args) {
  AppSorting* app_sorting =
      ExtensionSystem::Get(extension_service_->profile())->app_sorting();
  const std::string& extension_id = args[0].GetString();
  double page_index = args[1].GetDouble();
  const syncer::StringOrdinal& page_ordinal =
      app_sorting->PageIntegerAsStringOrdinal(static_cast<size_t>(page_index));

  // Don't update the page; it already knows the apps have been reordered.
  base::AutoReset<bool> auto_reset(&ignore_changes_, true);
  app_sorting->SetPageOrdinal(extension_id, page_ordinal);
}

void AppLauncherHandler::HandleSaveAppPageName(const base::Value::List& args) {
  const std::string& name = args[0].GetString();
  size_t page_index = static_cast<size_t>(args[1].GetDouble());

  base::AutoReset<bool> auto_reset(&ignore_changes_, true);
  PrefService* prefs = Profile::FromWebUI(web_ui())->GetPrefs();
  ScopedListPrefUpdate update(prefs, prefs::kNtpAppPageNames);
  base::Value::List& list = update.Get();
  if (page_index < list.size()) {
    list[page_index] = base::Value(name);
  } else {
    list.Append(name);
  }
}

void AppLauncherHandler::HandleGenerateAppForLink(
    const base::Value::List& list) {
  GURL launch_url(list[0].GetString());
  // Do not install app for invalid url.
  if (!launch_url.SchemeIsHTTPOrHTTPS())
    return;
  // Can only install one app at a time.
  if (attempting_web_app_install_page_ordinal_.has_value())
    return;

  std::u16string title = base::UTF8ToUTF16(list[1].GetString());

  double page_index = list[2].GetDouble();
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
                     base::Unretained(this), std::move(install_info)),
      &cancelable_task_tracker_);
}

void AppLauncherHandler::HandlePageSelected(const base::Value::List& args) {
  double index_double = args[0].GetDouble();
  int index = static_cast<int>(index_double);

  PrefService* prefs = Profile::FromWebUI(web_ui())->GetPrefs();
  prefs->SetInteger(prefs::kNtpShownPage, APPS_PAGE_ID | index);
}

void AppLauncherHandler::HandleRunOnOsLogin(const base::Value::List& args) {
  if (!base::FeatureList::IsEnabled(features::kDesktopPWAsRunOnOsLogin))
    return;

  const std::string& app_id = args[0].GetString();
  const std::string& mode_string = args[1].GetString();
  web_app::RunOnOsLoginMode mode;

  if (mode_string == kRunOnOsLoginModeNotRun) {
    mode = web_app::RunOnOsLoginMode::kNotRun;
  } else if (mode_string == kRunOnOsLoginModeWindowed) {
    mode = web_app::RunOnOsLoginMode::kWindowed;
  } else {
    // Specified mode is not supported.
    return;
  }

  web_app_provider_->scheduler().SetRunOnOsLoginMode(app_id, mode,
                                                     base::DoNothing());
}

void AppLauncherHandler::HandleLaunchDeprecatedAppDialog(
    const base::Value::List& args) {
  TabDialogs::FromWebContents(web_ui()->GetWebContents())
      ->ShowDeprecatedAppsDialog(extensions::ExtensionId(), deprecated_app_ids_,
                                 web_ui()->GetWebContents(), base::DoNothing());
}

void AppLauncherHandler::OnFaviconForAppInstallFromLink(
    std::unique_ptr<AppInstallInfo> install_info,
    const favicon_base::FaviconImageResult& image_result) {
  auto web_app = std::make_unique<web_app::WebAppInstallInfo>();
  web_app->title = install_info->title;
  web_app->start_url = install_info->app_url;

  if (!image_result.image.IsEmpty()) {
    web_app->icon_bitmaps.any[image_result.image.Width()] =
        image_result.image.AsBitmap();
  }

  attempting_web_app_install_page_ordinal_ = install_info->page_ordinal;

  web_app::OnceInstallCallback install_complete_callback = base::BindOnce(
      [](base::WeakPtr<AppLauncherHandler> app_launcher_handler,
         const webapps::AppId& app_id,
         webapps::InstallResultCode install_result) {
        if (!app_launcher_handler)
          return;
        if (install_result != webapps::InstallResultCode::kSuccessNewInstall) {
          app_launcher_handler->attempting_web_app_install_page_ordinal_ =
              absl::nullopt;
        }
      },
      weak_ptr_factory_.GetWeakPtr());

  web_app::WebAppInstallParams install_params;
  install_params.add_to_desktop = true;
  install_params.add_to_quick_launch_bar = false;
  install_params.add_to_applications_menu = true;

  web_app_provider_->scheduler().InstallFromInfoWithParams(
      std::move(web_app),
      /*overwrite_existing_manifest_fields=*/false,
      webapps::WebappInstallSource::SYNC, std::move(install_complete_callback),
      install_params);
}

void AppLauncherHandler::OnExtensionPreferenceChanged() {
  base::Value::Dict dictionary;
  FillAppDictionary(&dictionary);
  web_ui()->CallJavascriptFunctionUnsafe("ntp.appsPrefChangeCallback",
                                         std::move(dictionary));
}

void AppLauncherHandler::CleanupAfterUninstall() {
  extension_id_prompting_.clear();
}

void AppLauncherHandler::PromptToEnableApp(const std::string& extension_id) {
  if (!extension_id_prompting_.empty())
    return;  // Only one prompt at a time.

  if (web_app_provider_->registrar_unsafe().IsInstalled(
          extension_id_prompting_)) {
    NOTIMPLEMENTED();
    return;
  }

  extension_id_prompting_ = extension_id;
  extension_enable_flow_ = std::make_unique<ExtensionEnableFlow>(
      Profile::FromWebUI(web_ui()), extension_id, this);
  extension_enable_flow_->StartForWebContents(web_ui()->GetWebContents());
}

void AppLauncherHandler::OnExtensionUninstallDialogClosed(
    bool did_start_uninstall,
    const std::u16string& error) {
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

  if (web_app_provider_->registrar_unsafe().IsInstalled(
          extension_id_prompting_)) {
    NOTIMPLEMENTED();
    return;
  }

  extension_enable_flow_.reset();
  CleanupAfterUninstall();
}

extensions::ExtensionUninstallDialog*
AppLauncherHandler::CreateExtensionUninstallDialog() {
  Browser* browser = chrome::FindBrowserWithTab(web_ui()->GetWebContents());
  extension_uninstall_dialog_ = extensions::ExtensionUninstallDialog::Create(
      extension_service_->profile(), browser->window()->GetNativeWindow(),
      this);
  return extension_uninstall_dialog_.get();
}

void AppLauncherHandler::ExtensionRemoved(const Extension* extension,
                                          bool is_uninstall) {
  // If deprecated extension is deleted, remove from set for dialog.
  if (deprecated_app_ids_.find(extension->id()) != deprecated_app_ids_.end()) {
    deprecated_app_ids_.erase(extension->id());
  }

  if (!ShouldShow(extension))
    return;

  base::Value::Dict app_info(GetExtensionInfo(extension));

  web_ui()->CallJavascriptFunctionUnsafe(
      "ntp.appRemoved", std::move(app_info), base::Value(is_uninstall),
      base::Value(!extension_id_prompting_.empty()));
}

bool AppLauncherHandler::ShouldShow(const Extension* extension) {
  if (ignore_changes_ || !has_loaded_apps_ || !extension->is_app())
    return false;

  Profile* profile = Profile::FromWebUI(web_ui());
  return extensions::ui_util::ShouldDisplayInNewTabPage(extension, profile);
}
