// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/app_home/app_home_page_handler.h"

#include "base/check_deref.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ref.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/apps/app_service/app_icon_source.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/browser_app_launcher.h"
#include "chrome/browser/extensions/bookmark_app_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_ui_util.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/apps/app_info_dialog.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/extensions/extension_enable_flow.h"
#include "chrome/browser/ui/tab_dialogs.h"
#include "chrome/browser/ui/webui/app_home/app_home.mojom-shared.h"
#include "chrome/browser/ui/webui/extensions/extension_icon_source.h"
#include "chrome/browser/web_applications/extension_status_utils.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/proto/web_app_install_state.pb.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_metrics.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/grit/generated_resources.h"
#include "components/webapps/browser/uninstall_result_code.h"
#include "content/public/browser/web_ui.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "net/base/url_util.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom-shared.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/window_open_disposition_utils.h"
#include "url/gurl.h"

static_assert(BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX));

using content::WebUI;
using extensions::Extension;
using extensions::ExtensionRegistry;
using extensions::ExtensionSet;

namespace webapps {

namespace {

const int kWebAppIconSize = 64;

// Query string for showing the deprecation dialog with deletion options.
const char kDeprecationDialogQueryString[] = "showDeletionDialog";
// Query string for showing the force installed apps deprecation dialog.
// Should match with kChromeUIAppsWithForceInstalledDeprecationDialogURL.
const char kForceInstallDialogQueryString[] = "showForceInstallDialog";

// The Youtube app is incorrectly hardcoded to be a 'bookmark app'. However, it
// is a platform app.
// TODO(crbug.com/40124309): Remove this hack once the youtube app is fixed.
bool IsYoutubeExtension(const std::string& extension_id) {
  return extension_id == extension_misc::kYoutubeAppId;
}

}  // namespace

AppHomePageHandler::AppHomePageHandler(
    content::WebUI* web_ui,
    Profile* profile,
    mojo::PendingReceiver<app_home::mojom::PageHandler> receiver,
    mojo::PendingRemote<app_home::mojom::Page> page)
    : web_ui_(web_ui),
      profile_(profile),
      receiver_(this, std::move(receiver)),
      page_(std::move(page)),
      web_app_provider_(web_app::WebAppProvider::GetForWebApps(profile)),
      extension_system_(
          CHECK_DEREF(extensions::ExtensionSystem::Get(profile))) {
  web_app_registrar_observation_.Observe(
      &web_app_provider_->registrar_unsafe());
  install_manager_observation_.Observe(&web_app_provider_->install_manager());
  ExtensionRegistry::Get(profile)->AddObserver(this);
}

AppHomePageHandler::~AppHomePageHandler() {
  ExtensionRegistry::Get(profile_)->RemoveObserver(this);
  // Destroy `extension_uninstall_dialog_` now, since `this` is an
  // `ExtensionUninstallDialog::Delegate` and the dialog may call back into
  // `this` when destroyed.
  extension_uninstall_dialog_.reset();
}

Browser* AppHomePageHandler::GetCurrentBrowser() {
  return chrome::FindBrowserWithTab(web_ui_->GetWebContents());
}

void AppHomePageHandler::LoadDeprecatedAppsDialogIfRequired() {
  content::WebContents* web_contents = web_ui_->GetWebContents();
  std::string app_id;
  auto event_ptr = app_home::mojom::ClickEvent::New();
  event_ptr->button = 0.0;
  event_ptr->alt_key = false;
  event_ptr->ctrl_key = false;
  event_ptr->meta_key = false;
  event_ptr->shift_key = false;
  if (net::GetValueForKeyInQuery(web_contents->GetLastCommittedURL(),
                                 kDeprecationDialogQueryString, &app_id)) {
    if (extensions::IsExtensionUnsupportedDeprecatedApp(profile_, app_id) &&
        !deprecated_app_ids_.empty()) {
      TabDialogs::FromWebContents(web_contents)
          ->ShowDeprecatedAppsDialog(app_id, deprecated_app_ids_, web_contents);
    }
  } else if (net::GetValueForKeyInQuery(web_contents->GetLastCommittedURL(),
                                        kForceInstallDialogQueryString,
                                        &app_id)) {
    if (extensions::IsExtensionUnsupportedDeprecatedApp(profile_, app_id) &&
        extensions::IsExtensionForceInstalled(profile_, app_id, nullptr)) {
      if (extensions::IsPreinstalledAppId(app_id)) {
        TabDialogs::FromWebContents(web_contents)
            ->ShowForceInstalledPreinstalledDeprecatedAppDialog(app_id,
                                                                web_contents);
      } else {
        TabDialogs::FromWebContents(web_contents)
            ->ShowForceInstalledDeprecatedAppsDialog(app_id, web_contents);
      }
    }
  }
  has_maybe_loaded_deprecated_apps_dialog_ = true;
}

void AppHomePageHandler::LaunchAppInternal(
    const std::string& app_id,
    extension_misc::AppLaunchBucket launch_bucket,
    app_home::mojom::ClickEventPtr click_event) {
  if (extensions::IsExtensionUnsupportedDeprecatedApp(profile_, app_id) &&
      base::FeatureList::IsEnabled(features::kChromeAppsDeprecation)) {
    if (!extensions::IsExtensionForceInstalled(profile_, app_id, nullptr)) {
      TabDialogs::FromWebContents(web_ui_->GetWebContents())
          ->ShowDeprecatedAppsDialog(app_id, deprecated_app_ids_,
                                     web_ui_->GetWebContents());
      return;
    } else {
      if (extensions::IsPreinstalledAppId(app_id)) {
        TabDialogs::FromWebContents(web_ui_->GetWebContents())
            ->ShowForceInstalledPreinstalledDeprecatedAppDialog(
                app_id, web_ui_->GetWebContents());
      } else {
        TabDialogs::FromWebContents(web_ui_->GetWebContents())
            ->ShowForceInstalledDeprecatedAppsDialog(app_id,
                                                     web_ui_->GetWebContents());
      }
      return;
    }
  }

  extensions::Manifest::Type type;
  GURL full_launch_url;
  apps::LaunchContainer launch_container;

  web_app::WebAppRegistrar& registrar = web_app_provider_->registrar_unsafe();
  if (registrar.IsInstalled(app_id) && !IsYoutubeExtension(app_id)) {
    type = extensions::Manifest::Type::TYPE_HOSTED_APP;
    full_launch_url = registrar.GetAppStartUrl(app_id);
    launch_container = web_app::ConvertDisplayModeToAppLaunchContainer(
        registrar.GetAppEffectiveDisplayMode(app_id));
  } else {
    const Extension* extension = extensions::ExtensionRegistry::Get(profile_)
                                     ->enabled_extensions()
                                     .GetByID(app_id);

    // Prompt the user to re-enable the application if disabled.
    if (!extension) {
      PromptToEnableExtensionApp(app_id);
      return;
    }
    type = extension->GetType();
    full_launch_url = extensions::AppLaunchInfo::GetFullLaunchURL(extension);
    launch_container = extensions::GetLaunchContainer(
        extensions::ExtensionPrefs::Get(profile_), extension);
  }

  WindowOpenDisposition disposition =
      click_event ? ui::DispositionFromClick(
                        click_event->button == 1.0, click_event->alt_key,
                        click_event->ctrl_key, click_event->meta_key,
                        click_event->shift_key)
                  : WindowOpenDisposition::CURRENT_TAB;
  GURL override_url;

  CHECK_NE(launch_bucket, extension_misc::APP_LAUNCH_BUCKET_INVALID);
  extensions::RecordAppLaunchType(launch_bucket, type);

  if (disposition == WindowOpenDisposition::NEW_FOREGROUND_TAB ||
      disposition == WindowOpenDisposition::NEW_BACKGROUND_TAB ||
      disposition == WindowOpenDisposition::NEW_WINDOW) {
    // TODO(jamescook): Proper support for background tabs.
    apps::AppLaunchParams params(
        app_id,
        disposition == WindowOpenDisposition::NEW_WINDOW
            ? apps::LaunchContainer::kLaunchContainerWindow
            : apps::LaunchContainer::kLaunchContainerTab,
        disposition, apps::LaunchSource::kFromAppHomePage);
    params.override_url = override_url;
    apps::AppServiceProxyFactory::GetForProfile(profile_)
        ->BrowserAppLauncher()
        ->LaunchAppWithParams(std::move(params), base::DoNothing());
  } else {
    // To give a more "launchy" experience when using the NTP launcher, we close
    // it automatically. However, if the chrome://apps page is the LAST page in
    // the browser window, then we don't close it.
    Browser* browser = GetCurrentBrowser();
    base::WeakPtr<Browser> browser_ptr;
    content::WebContents* old_contents = nullptr;
    base::WeakPtr<content::WebContents> old_contents_ptr;
    if (browser) {
      browser_ptr = browser->AsWeakPtr();
      old_contents = browser->tab_strip_model()->GetActiveWebContents();
      old_contents_ptr = old_contents->GetWeakPtr();
    }

    apps::AppLaunchParams params(
        app_id, launch_container,
        old_contents ? WindowOpenDisposition::CURRENT_TAB
                     : WindowOpenDisposition::NEW_FOREGROUND_TAB,
        apps::LaunchSource::kFromAppHomePage);
    params.override_url = override_url;
    apps::AppServiceProxyFactory::GetForProfile(profile_)
        ->BrowserAppLauncher()
        ->LaunchAppWithParams(
            std::move(params),
            base::BindOnce(
                [](base::WeakPtr<Browser> apps_page_browser,
                   base::WeakPtr<content::WebContents> old_contents,
                   content::WebContents* new_web_contents) {
                  if (!apps_page_browser || !old_contents) {
                    return;
                  }
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

void AppHomePageHandler::SetUserDisplayMode(
    const std::string& app_id,
    web_app::mojom::UserDisplayMode user_display_mode) {
  web_app_provider_->scheduler().SetUserDisplayMode(app_id, user_display_mode,
                                                    base::DoNothing());
}

app_home::mojom::AppInfoPtr AppHomePageHandler::GetApp(
    const webapps::AppId& app_id) {
  std::vector<app_home::mojom::AppInfoPtr> all_apps;
  FillWebAppInfoList(&all_apps);
  FillExtensionInfoList(&all_apps);
  app_home::mojom::AppInfoPtr found_app;
  for (const auto& app : all_apps) {
    if (app->id == app_id) {
      found_app = app.Clone();
      break;
    }
  }
  return found_app;
}

void AppHomePageHandler::ShowWebAppSettings(const std::string& app_id) {
  chrome::ShowWebAppSettings(
      GetCurrentBrowser(), app_id,
      web_app::AppSettingsPageEntryPoint::kChromeAppsPage);
}

void AppHomePageHandler::ShowExtensionAppSettings(
    const extensions::Extension* extension) {
  ShowAppInfoInNativeDialog(web_ui_->GetWebContents(), profile_, extension,
                            base::DoNothing());
}

void AppHomePageHandler::CreateWebAppShortcut(const std::string& app_id,
                                              base::OnceClosure done) {
  Browser* browser = GetCurrentBrowser();
  chrome::ShowCreateChromeAppShortcutsDialog(
      browser->window()->GetNativeWindow(), browser->profile(), app_id,
      base::BindOnce(
          [](base::OnceClosure done, bool success) {
            base::UmaHistogramBoolean(
                "Apps.AppInfoDialog.CreateWebAppShortcutSuccess", success);
            std::move(done).Run();
          },
          std::move(done)));
}

void AppHomePageHandler::CreateExtensionAppShortcut(
    const extensions::Extension* extension,
    base::OnceClosure done) {
  Browser* browser = GetCurrentBrowser();
  chrome::ShowCreateChromeAppShortcutsDialog(
      browser->window()->GetNativeWindow(), browser->profile(), extension,
      base::IgnoreArgs<bool>(std::move(done)));
}

app_home::mojom::AppInfoPtr AppHomePageHandler::CreateAppInfoPtrFromWebApp(
    const webapps::AppId& app_id) {
  auto& registrar = web_app_provider_->registrar_unsafe();

  auto app_info = app_home::mojom::AppInfo::New();

  app_info->id = app_id;

  GURL start_url = registrar.GetAppStartUrl(app_id);
  app_info->start_url = start_url;

  std::string name = registrar.GetAppShortName(app_id);
  app_info->name = name;

  app_info->icon_url = apps::AppIconSource::GetIconURL(app_id, kWebAppIconSize);

  bool is_locally_installed = registrar.IsInstallState(
      app_id, {web_app::proto::INSTALLED_WITHOUT_OS_INTEGRATION,
               web_app::proto::INSTALLED_WITH_OS_INTEGRATION});

  const auto login_mode = registrar.GetAppRunOnOsLoginMode(app_id);
  // Only show the Run on OS Login menu item for locally installed web apps
  app_info->may_show_run_on_os_login_mode =
      base::FeatureList::IsEnabled(features::kDesktopPWAsRunOnOsLogin) &&
      is_locally_installed;
  app_info->may_toggle_run_on_os_login_mode = login_mode.user_controllable;
  app_info->run_on_os_login_mode = login_mode.value;

  app_info->is_locally_installed = is_locally_installed;
  // Treat all other types of display mode as "open as window".
  app_info->open_in_window = registrar.GetAppEffectiveDisplayMode(app_id) !=
                             blink::mojom::DisplayMode::kBrowser;

  app_info->store_page_url = std::nullopt;
  app_info->may_uninstall =
      web_app_provider_->registrar_unsafe().CanUserUninstallWebApp(app_id);
  app_info->is_deprecated_app = false;
  return app_info;
}

app_home::mojom::AppInfoPtr AppHomePageHandler::CreateAppInfoPtrFromExtension(
    const Extension* extension) {
  auto app_info = app_home::mojom::AppInfo::New();

  app_info->id = extension->id();

  GURL start_url = extensions::AppLaunchInfo::GetFullLaunchURL(extension);
  app_info->start_url = start_url;

  bool deprecated_app = false;
  auto* context = extension_system_->extension_service()->GetBrowserContext();
  deprecated_app =
      extensions::IsExtensionUnsupportedDeprecatedApp(context, extension->id());

  if (deprecated_app) {
    app_info->name =
        l10n_util::GetStringFUTF8(IDS_APPS_PAGE_DEPRECATED_APP_TITLE,
                                  base::UTF8ToUTF16(extension->name()));
  } else {
    app_info->name = extension->name();
  }

  app_info->icon_url = extensions::ExtensionIconSource::GetIconURL(
      extension, extension_misc::EXTENSION_ICON_LARGE,
      ExtensionIconSet::Match::kBigger, false /*grayscale*/);

  app_info->may_show_run_on_os_login_mode = false;
  app_info->may_toggle_run_on_os_login_mode = false;

  app_info->is_locally_installed =
      !extension->is_hosted_app() ||
      extensions::BookmarkAppIsLocallyInstalled(profile_, extension);
  app_info->store_page_url = std::nullopt;
  if (extension->from_webstore()) {
    GURL store_url = GURL(base::StrCat(
        {"https://chrome.google.com/webstore/detail/", extension->id()}));
    DCHECK(store_url.is_valid());
    app_info->store_page_url = store_url;
  }
  app_info->is_deprecated_app = deprecated_app;
  app_info->may_uninstall =
      extension_system_->management_policy()->UserMayModifySettings(extension,
                                                                    nullptr);
  return app_info;
}

void AppHomePageHandler::FillWebAppInfoList(
    std::vector<app_home::mojom::AppInfoPtr>* result) {
  web_app::WebAppRegistrar& registrar = web_app_provider_->registrar_unsafe();

  for (const webapps::AppId& web_app_id : registrar.GetAppIds()) {
    if (IsYoutubeExtension(web_app_id)) {
      continue;
    }
    result->emplace_back(CreateAppInfoPtrFromWebApp(web_app_id));
  }
}

void AppHomePageHandler::FillExtensionInfoList(
    std::vector<app_home::mojom::AppInfoPtr>* result) {
  ExtensionRegistry* registry = ExtensionRegistry::Get(profile_);
  const ExtensionSet extension_apps = registry->GenerateInstalledExtensionsSet(
      ExtensionRegistry::ENABLED | ExtensionRegistry::DISABLED |
      ExtensionRegistry::TERMINATED);
  for (const auto& extension : extension_apps) {
    if (!extensions::ui_util::ShouldDisplayInNewTabPage(extension.get(),
                                                        profile_) ||
        extension->id() == extensions::kWebStoreAppId) {
      continue;
    }

    auto* context = extension_system_->extension_service()->GetBrowserContext();
    const bool is_deprecated_app =
        extensions::IsExtensionUnsupportedDeprecatedApp(context,
                                                        extension->id());
    if (is_deprecated_app && !extensions::IsExtensionForceInstalled(
                                 context, extension->id(), nullptr)) {
      deprecated_app_ids_.insert(extension->id());
    }
    result->emplace_back(CreateAppInfoPtrFromExtension(extension.get()));
  }
}

void AppHomePageHandler::OnExtensionUninstallDialogClosed(
    bool did_start_uninstall,
    const std::u16string& error) {
  ResetExtensionDialogState();
}

void AppHomePageHandler::ResetExtensionDialogState() {
  extension_dialog_prompting_ = false;
}

void AppHomePageHandler::UninstallWebApp(const std::string& web_app_id) {
  if (!web_app_provider_->registrar_unsafe().CanUserUninstallWebApp(
          web_app_id)) {
    LOG(ERROR) << "Attempt to uninstall a webapp that is non-usermanagable "
                  "was made. App id : "
               << web_app_id;
    return;
  }

  extension_dialog_prompting_ = true;

  auto uninstall_success_callback = base::BindOnce(
      [](base::WeakPtr<AppHomePageHandler> app_home_page_handler,
         webapps::UninstallResultCode code) {
        if (app_home_page_handler) {
          app_home_page_handler->ResetExtensionDialogState();
        }
      },
      weak_ptr_factory_.GetWeakPtr());

  Browser* browser = GetCurrentBrowser();
  CHECK(browser);
  web_app_provider_->ui_manager().PresentUserUninstallDialog(
      web_app_id, webapps::WebappUninstallSource::kAppsPage, browser->window(),
      std::move(uninstall_success_callback));
  return;
}

extensions::ExtensionUninstallDialog*
AppHomePageHandler::CreateExtensionUninstallDialog() {
  Browser* browser = GetCurrentBrowser();
  extension_uninstall_dialog_ = extensions::ExtensionUninstallDialog::Create(
      profile_, browser->window()->GetNativeWindow(), this);
  return extension_uninstall_dialog_.get();
}

void AppHomePageHandler::UninstallExtensionApp(const Extension* extension) {
  if (!extension_system_->management_policy()->UserMayModifySettings(extension,
                                                                     nullptr)) {
    LOG(ERROR) << "Attempt to uninstall an extension that is non-usermanagable "
                  "was made. Extension id : "
               << extension->id();
    return;
  }

  extension_dialog_prompting_ = true;

  Browser* browser = GetCurrentBrowser();
  extension_uninstall_dialog_ = extensions::ExtensionUninstallDialog::Create(
      profile_, browser->window()->GetNativeWindow(), this);

  extension_uninstall_dialog_->ConfirmUninstall(
      extension, extensions::UNINSTALL_REASON_USER_INITIATED,
      extensions::UNINSTALL_SOURCE_CHROME_APPS_PAGE);
}

void AppHomePageHandler::ExtensionRemoved(const Extension* extension) {
  if (deprecated_app_ids_.find(extension->id()) != deprecated_app_ids_.end()) {
    deprecated_app_ids_.erase(extension->id());
  }

  if (!extension->is_app() ||
      !extensions::ui_util::ShouldDisplayInNewTabPage(extension, profile_)) {
    return;
  }

  auto app_info = app_home::mojom::AppInfo::New();
  app_info->id = extension->id();
  page_->RemoveApp(std::move(app_info));
}

void AppHomePageHandler::OnWebAppWillBeUninstalled(
    const webapps::AppId& app_id) {
  auto app_info = app_home::mojom::AppInfo::New();
  app_info->id = app_id;
  page_->RemoveApp(std::move(app_info));
}

void AppHomePageHandler::OnWebAppInstalled(const webapps::AppId& app_id) {
  page_->AddApp(CreateAppInfoPtrFromWebApp(app_id));
}

void AppHomePageHandler::OnWebAppInstallManagerDestroyed() {
  install_manager_observation_.Reset();
}

void AppHomePageHandler::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension) {
  if (extensions::ui_util::ShouldDisplayInNewTabPage(extension, profile_)) {
    page_->AddApp(CreateAppInfoPtrFromExtension(extension));
  }
}

void AppHomePageHandler::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    extensions::UnloadedExtensionReason reason) {
  ExtensionRemoved(extension);
}

void AppHomePageHandler::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    extensions::UninstallReason reason) {
  ExtensionRemoved(extension);
}

void AppHomePageHandler::PromptToEnableExtensionApp(
    const std::string& extension_app_id) {
  if (extension_dialog_prompting_) {
    return;  // Only one prompt at a time.
  }

  extension_dialog_prompting_ = true;
  extension_enable_flow_ =
      std::make_unique<ExtensionEnableFlow>(profile_, extension_app_id, this);
  extension_enable_flow_->StartForWebContents(web_ui_->GetWebContents());
}

void AppHomePageHandler::ExtensionEnableFlowFinished() {
  // Reload the page so the browser can update the apps icon.
  // If we don't launch the app asynchronously, then the app's disabled
  // icon disappears but isn't replaced by the enabled icon, making a poor
  // visual experience.
  web_ui_->GetWebContents()->ReloadFocusedFrame();

  extension_enable_flow_.reset();
  ResetExtensionDialogState();
}

void AppHomePageHandler::ExtensionEnableFlowAborted(bool user_initiated) {
  extension_enable_flow_.reset();
  ResetExtensionDialogState();
}

void AppHomePageHandler::GetApps(GetAppsCallback callback) {
  std::vector<app_home::mojom::AppInfoPtr> result;
  FillWebAppInfoList(&result);
  FillExtensionInfoList(&result);
  sort(result.begin(), result.end(),
       [](const app_home::mojom::AppInfoPtr& lhs,
          const app_home::mojom::AppInfoPtr& rhs) {
         return lhs->name < rhs->name;
       });

  if (!has_maybe_loaded_deprecated_apps_dialog_) {
    LoadDeprecatedAppsDialogIfRequired();
  }
  std::move(callback).Run(std::move(result));
}

void AppHomePageHandler::GetDeprecationLinkString(
    GetDeprecationLinkStringCallback callback) {
  std::string message;
  if (deprecated_app_ids_.size() == 0) {
    message = "";
  } else {
    message = l10n_util::GetPluralStringFUTF8(IDS_DEPRECATED_APPS_DELETION_LINK,
                                              deprecated_app_ids_.size());
  }
  std::move(callback).Run(message);
}

void AppHomePageHandler::OnWebAppRunOnOsLoginModeChanged(
    const webapps::AppId& app_id,
    web_app::RunOnOsLoginMode run_on_os_login_mode) {
  page_->AddApp(CreateAppInfoPtrFromWebApp(app_id));
}

void AppHomePageHandler::OnWebAppUserDisplayModeChanged(
    const webapps::AppId& app_id,
    web_app::mojom::UserDisplayMode user_display_mode) {
  page_->AddApp(CreateAppInfoPtrFromWebApp(app_id));
}

void AppHomePageHandler::OnWebAppInstalledWithOsHooks(
    const webapps::AppId& app_id) {
  page_->AddApp(CreateAppInfoPtrFromWebApp(app_id));
}

void AppHomePageHandler::OnAppRegistrarDestroyed() {
  web_app_registrar_observation_.Reset();
}

void AppHomePageHandler::UninstallApp(const std::string& app_id) {
  if (extension_dialog_prompting_) {
    return;
  }

  if (web_app_provider_->registrar_unsafe().IsInstalled(app_id) &&
      !IsYoutubeExtension(app_id)) {
    UninstallWebApp(app_id);
    return;
  }

  const Extension* extension =
      ExtensionRegistry::Get(profile_)->GetInstalledExtension(app_id);
  if (extension) {
    UninstallExtensionApp(extension);
  }
}

void AppHomePageHandler::ShowAppSettings(const std::string& app_id) {
  if (web_app_provider_->registrar_unsafe().IsInstalled(app_id) &&
      !IsYoutubeExtension(app_id)) {
    ShowWebAppSettings(app_id);
    return;
  }

  const Extension* extension =
      extensions::ExtensionRegistry::Get(
          extension_system_->extension_service()->GetBrowserContext())
          ->GetExtensionById(app_id,
                             extensions::ExtensionRegistry::ENABLED |
                                 extensions::ExtensionRegistry::DISABLED |
                                 extensions::ExtensionRegistry::TERMINATED);
  if (extension) {
    ShowExtensionAppSettings(extension);
  }
}

void AppHomePageHandler::CreateAppShortcut(const std::string& app_id,
                                           CreateAppShortcutCallback callback) {
  if (web_app_provider_->registrar_unsafe().IsInstalled(app_id) &&
      !IsYoutubeExtension(app_id)) {
    CreateWebAppShortcut(app_id, std::move(callback));
    return;
  }

  const Extension* extension =
      extensions::ExtensionRegistry::Get(profile_)->GetExtensionById(
          app_id, extensions::ExtensionRegistry::ENABLED |
                      extensions::ExtensionRegistry::DISABLED |
                      extensions::ExtensionRegistry::TERMINATED);
  if (extension) {
    CreateExtensionAppShortcut(extension, std::move(callback));
  }
}

void AppHomePageHandler::LaunchApp(const std::string& app_id,
                                   app_home::mojom::ClickEventPtr click_event) {
  LaunchAppInternal(app_id, extension_misc::APP_LAUNCH_NTP_APPS_MAXIMIZED,
                    std::move(click_event));
}

void AppHomePageHandler::SetRunOnOsLoginMode(
    const std::string& app_id,
    web_app::RunOnOsLoginMode run_on_os_login_mode) {
  if (!base::FeatureList::IsEnabled(features::kDesktopPWAsRunOnOsLogin)) {
    return;
  }

  if (run_on_os_login_mode != web_app::RunOnOsLoginMode::kNotRun &&
      run_on_os_login_mode != web_app::RunOnOsLoginMode::kWindowed) {
    return;  // Other login mode is not supported;
  }

  web_app_provider_->scheduler().SetRunOnOsLoginMode(
      app_id, run_on_os_login_mode, base::DoNothing());
}

void AppHomePageHandler::LaunchDeprecatedAppDialog() {
  TabDialogs::FromWebContents(web_ui_->GetWebContents())
      ->ShowDeprecatedAppsDialog(extensions::ExtensionId(), deprecated_app_ids_,
                                 web_ui_->GetWebContents());
}

void AppHomePageHandler::InstallAppLocally(const std::string& app_id) {
  web_app_provider_->scheduler().InstallAppLocally(app_id, base::DoNothing());
}

}  // namespace webapps
