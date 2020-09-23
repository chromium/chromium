// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/application_launch.h"

#include <memory>
#include <string>
#include <utility>

#include "apps/launcher.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/apps/platform_apps/platform_app_launch.h"
#include "chrome/browser/banners/app_banner_settings_helper.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/extension_enable_flow.h"
#include "chrome/browser/ui/extensions/extension_enable_flow_delegate.h"
#include "chrome/browser/ui/web_applications/system_web_app_ui_utils.h"
#include "chrome/browser/ui/web_applications/web_app_launch_manager.h"
#include "chrome/browser/ui/web_applications/web_app_launch_utils.h"
#include "chrome/browser/web_applications/components/os_integration_manager.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/components/web_app_provider_base.h"
#include "chrome/browser/web_applications/components/web_app_tab_helper_base.h"
#include "chrome/browser/web_applications/system_web_app_manager.h"
#include "chrome/browser/web_launch/web_launch_files_helper.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/features/feature_provider.h"
#include "extensions/common/manifest_handlers/options_page_info.h"
#include "third_party/blink/public/common/features.h"
#include "ui/base/window_open_disposition.h"
#include "ui/display/scoped_display_for_new_windows.h"
#include "ui/gfx/geometry/rect.h"

#if defined(OS_MAC)
#include "chrome/browser/ui/browser_commands_mac.h"
#endif

using content::WebContents;
using extensions::Extension;
using extensions::ExtensionPrefs;
using extensions::ExtensionRegistry;
using extensions::ExtensionService;

namespace {

// Attempts to launch an app, prompting the user to enable it if necessary.
// This class manages its own lifetime.
class EnableViaDialogFlow : public ExtensionEnableFlowDelegate {
 public:
  EnableViaDialogFlow(ExtensionService* service,
                      ExtensionRegistry* registry,
                      Profile* profile,
                      const std::string& extension_id,
                      const base::Closure& callback)
      : service_(service),
        registry_(registry),
        profile_(profile),
        extension_id_(extension_id),
        callback_(callback) {}

  ~EnableViaDialogFlow() override {}

  void Run() {
    DCHECK(!service_->IsExtensionEnabled(extension_id_));
    flow_ =
        std::make_unique<ExtensionEnableFlow>(profile_, extension_id_, this);
    flow_->Start();
  }

 private:
  // ExtensionEnableFlowDelegate overrides.
  void ExtensionEnableFlowFinished() override {
    const Extension* extension =
        registry_->GetExtensionById(extension_id_, ExtensionRegistry::ENABLED);
    if (!extension)
      return;
    callback_.Run();
    delete this;
  }

  void ExtensionEnableFlowAborted(bool user_initiated) override { delete this; }

  ExtensionService* service_;
  ExtensionRegistry* registry_;
  Profile* profile_;
  std::string extension_id_;
  base::Closure callback_;
  std::unique_ptr<ExtensionEnableFlow> flow_;

  DISALLOW_COPY_AND_ASSIGN(EnableViaDialogFlow);
};

const Extension* GetExtension(Profile* profile,
                              const apps::AppLaunchParams& params) {
  if (params.app_id.empty())
    return NULL;
  ExtensionRegistry* registry = ExtensionRegistry::Get(profile);
  return registry->GetExtensionById(
      params.app_id, ExtensionRegistry::ENABLED | ExtensionRegistry::DISABLED |
                         ExtensionRegistry::TERMINATED);
}

bool IsAllowedToOverrideURL(const extensions::Extension* extension,
                            const GURL& override_url) {
  if (extension->web_extent().MatchesURL(override_url))
    return true;

  if (override_url.GetOrigin() == extension->url())
    return true;

  if (extension->from_bookmark() &&
      extensions::AppLaunchInfo::GetFullLaunchURL(extension).GetOrigin() ==
          override_url.GetOrigin()) {
    return true;
  }

  return false;
}

// Get the launch URL for a given extension, with optional override/fallback.
// |override_url|, if non-empty, will be preferred over the extension's
// launch url.
GURL UrlForExtension(const extensions::Extension* extension,
                     Profile* profile,
                     const apps::AppLaunchParams& params) {
  if (!extension)
    return params.override_url;

  GURL url;
  if (!params.override_url.is_empty()) {
    DCHECK(IsAllowedToOverrideURL(extension, params.override_url));
    url = params.override_url;
  } else if (extension->from_bookmark()) {
    web_app::OsIntegrationManager& os_integration_manager =
        web_app::WebAppProviderBase::GetProviderBase(profile)
            ->os_integration_manager();
    url = os_integration_manager
              .GetMatchingFileHandlerURL(params.app_id, params.launch_files)
              .value_or(extensions::AppLaunchInfo::GetFullLaunchURL(extension));
  } else {
    url = extensions::AppLaunchInfo::GetFullLaunchURL(extension);
  }

  // For extensions lacking launch urls, determine a reasonable fallback.
  if (!url.is_valid()) {
    url = extensions::OptionsPageInfo::GetOptionsPage(extension);
    if (!url.is_valid())
      url = GURL(chrome::kChromeUIExtensionsURL);
  }

  return url;
}

ui::WindowShowState DetermineWindowShowState(
    Profile* profile,
    extensions::LaunchContainer container,
    const Extension* extension) {
  if (!extension ||
      container != extensions::LaunchContainer::kLaunchContainerWindow)
    return ui::SHOW_STATE_DEFAULT;

  if (chrome::IsRunningInForcedAppMode())
    return ui::SHOW_STATE_FULLSCREEN;

#if defined(OS_CHROMEOS)
  // In ash, LAUNCH_TYPE_FULLSCREEN launches in a maximized app window and
  // LAUNCH_TYPE_WINDOW launches in a default app window.
  extensions::LaunchType launch_type =
      extensions::GetLaunchType(ExtensionPrefs::Get(profile), extension);
  if (launch_type == extensions::LAUNCH_TYPE_FULLSCREEN)
    return ui::SHOW_STATE_MAXIMIZED;
  else if (launch_type == extensions::LAUNCH_TYPE_WINDOW)
    return ui::SHOW_STATE_DEFAULT;
#endif

  return ui::SHOW_STATE_DEFAULT;
}

WebContents* OpenApplicationTab(Profile* profile,
                                const apps::AppLaunchParams& launch_params,
                                const GURL& url) {
  const Extension* extension = GetExtension(profile, launch_params);
  CHECK(extension);
  WindowOpenDisposition disposition = launch_params.disposition;

  Browser* browser =
      chrome::FindTabbedBrowser(profile, false, launch_params.display_id);
  WebContents* contents = NULL;
  if (!browser) {
    // No browser for this profile, need to open a new one.
    //
    // TODO(erg): AppLaunchParams should pass user_gesture from the extension
    // system to here.
    browser =
        new Browser(Browser::CreateParams(Browser::TYPE_NORMAL, profile, true));
    browser->window()->Show();
    // There's no current tab in this browser window, so add a new one.
    disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  } else {
    // For existing browser, ensure its window is shown and activated.
    browser->window()->Show();
    browser->window()->Activate();
  }

  extensions::LaunchType launch_type =
      extensions::GetLaunchType(ExtensionPrefs::Get(profile), extension);
  UMA_HISTOGRAM_ENUMERATION("Extensions.AppTabLaunchType", launch_type, 100);

  int add_type = TabStripModel::ADD_ACTIVE;
  if (launch_type == extensions::LAUNCH_TYPE_PINNED)
    add_type |= TabStripModel::ADD_PINNED;

  ui::PageTransition transition = ui::PAGE_TRANSITION_AUTO_BOOKMARK;
  NavigateParams params(browser, url, transition);
  params.tabstrip_add_types = add_type;
  params.disposition = disposition;

  if (disposition == WindowOpenDisposition::CURRENT_TAB) {
    WebContents* existing_tab =
        browser->tab_strip_model()->GetActiveWebContents();
    TabStripModel* model = browser->tab_strip_model();
    int tab_index = model->GetIndexOfWebContents(existing_tab);

    existing_tab->OpenURL(content::OpenURLParams(
        url,
        content::Referrer::SanitizeForRequest(
            url, content::Referrer(existing_tab->GetURL(),
                                   network::mojom::ReferrerPolicy::kDefault)),
        disposition, transition, false));
    // Reset existing_tab as OpenURL() may have clobbered it.
    existing_tab = browser->tab_strip_model()->GetActiveWebContents();
    if (params.tabstrip_add_types & TabStripModel::ADD_PINNED) {
      model->SetTabPinned(tab_index, true);
      // Pinning may have moved the tab.
      tab_index = model->GetIndexOfWebContents(existing_tab);
    }
    if (params.tabstrip_add_types & TabStripModel::ADD_ACTIVE) {
      model->ActivateTabAt(tab_index, {TabStripModel::GestureType::kOther});
    }

    contents = existing_tab;
  } else {
    Navigate(&params);
    contents = params.navigated_or_inserted_contents;
  }

  if (extension->from_bookmark()) {
    web_app::WebAppTabHelperBase* tab_helper =
        web_app::WebAppTabHelperBase::FromWebContents(contents);
    DCHECK(tab_helper);
    tab_helper->SetAppId(extension->id());
  }

#if defined(OS_CHROMEOS)
  // In ash, LAUNCH_FULLSCREEN launches in the OpenApplicationWindow function
  // i.e. it should not reach here.
  DCHECK(launch_type != extensions::LAUNCH_TYPE_FULLSCREEN);
#else
  // TODO(skerner):  If we are already in full screen mode, and the user set the
  // app to open as a regular or pinned tab, what should happen? Today we open
  // the tab, but stay in full screen mode.  Should we leave full screen mode in
  // this case?
  if (launch_type == extensions::LAUNCH_TYPE_FULLSCREEN &&
      !browser->window()->IsFullscreen()) {
    chrome::ToggleFullscreenMode(browser);
  }
#endif  // OS_CHROMEOS
  return contents;
}

WebContents* OpenEnabledApplication(Profile* profile,
                                    const apps::AppLaunchParams& params) {
  const Extension* extension = GetExtension(profile, params);
  if (!extension)
    return NULL;

  WebContents* tab = NULL;
  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile);
  prefs->SetActiveBit(extension->id(), true);

  if (CanLaunchViaEvent(extension)) {
    // When launching an app with a command line, there might be a file path to
    // work with that command line, so
    // LaunchPlatformAppWithCommandLineAndLaunchId should be called to handle
    // the command line. If |launch_files| is set without |command_line|, that
    // means launching the app with files, so call
    // LaunchPlatformAppWithFilePaths to forward |launch_files| to the app.
    if (params.command_line.GetArgs().empty() && !params.launch_files.empty()) {
      apps::LaunchPlatformAppWithFilePaths(profile, extension,
                                           params.launch_files);
      return nullptr;
    }

    apps::LaunchPlatformAppWithCommandLineAndLaunchId(
        profile, extension, params.launch_id, params.command_line,
        params.current_directory, params.source);
    return NULL;
  }

  UMA_HISTOGRAM_ENUMERATION("Extensions.HostedAppLaunchContainer",
                            params.container);

  GURL url = UrlForExtension(extension, profile, params);

  // System Web Apps go through their own launch path.
  base::Optional<web_app::SystemAppType> system_app_type =
      web_app::GetSystemWebAppTypeForAppId(profile, extension->id());
  if (system_app_type) {
    Browser* browser =
        web_app::LaunchSystemWebApp(profile, *system_app_type, url, params);
    return browser->tab_strip_model()->GetActiveWebContents();
  }

  // Record v1 app launch. Platform app launch is recorded when dispatching
  // the onLaunched event.
  prefs->SetLastLaunchTime(extension->id(), base::Time::Now());

  switch (params.container) {
    case extensions::LaunchContainer::kLaunchContainerNone: {
      NOTREACHED();
      break;
    }
    // Panels are deprecated. Launch a normal window instead.
    case extensions::LaunchContainer::kLaunchContainerPanelDeprecated:
    case extensions::LaunchContainer::kLaunchContainerWindow:
      tab = OpenApplicationWindow(profile, params, url);
      break;
    case extensions::LaunchContainer::kLaunchContainerTab: {
      tab = OpenApplicationTab(profile, params, url);
      break;
    }
    default:
      NOTREACHED();
      break;
  }

  if (extension->from_bookmark()) {
    if (web_app::WebAppProviderBase::GetProviderBase(profile)
            ->os_integration_manager()
            .IsFileHandlingAPIAvailable(extension->id())) {
      web_launch::WebLaunchFilesHelper::SetLaunchPaths(tab, url,
                                                       params.launch_files);
    }

    UMA_HISTOGRAM_ENUMERATION("Extensions.BookmarkAppLaunchSource",
                              params.source);
    UMA_HISTOGRAM_ENUMERATION("Extensions.BookmarkAppLaunchContainer",
                              params.container);

    // Record the launch time in the site engagement service. A recent bookmark
    // app launch will provide an engagement boost to the origin.
    SiteEngagementService* service = SiteEngagementService::Get(profile);
    service->SetLastShortcutLaunchTime(tab, url);

    // Refresh the app banner added to homescreen event. The user may have
    // cleared their browsing data since installing the app, which removes the
    // event and will potentially permit a banner to be shown for the site.
    AppBannerSettingsHelper::RecordBannerEvent(
        tab, url, url.spec(),
        AppBannerSettingsHelper::APP_BANNER_EVENT_DID_ADD_TO_HOMESCREEN,
        base::Time::Now());
  }
  return tab;
}

}  // namespace

WebContents* OpenApplication(Profile* profile,
                             const apps::AppLaunchParams& params) {
  return OpenEnabledApplication(profile, params);
}

Browser* CreateApplicationWindow(Profile* profile,
                                 const apps::AppLaunchParams& params,
                                 const GURL& url,
                                 bool can_resize) {
  const Extension* const extension = GetExtension(profile, params);

  std::string app_name;
  if (!params.override_app_name.empty())
    app_name = params.override_app_name;
  else if (extension)
    app_name = web_app::GenerateApplicationNameFromAppId(extension->id());
  else
    app_name = web_app::GenerateApplicationNameFromURL(url);

  gfx::Rect initial_bounds;
  if (!params.override_bounds.IsEmpty()) {
    initial_bounds = params.override_bounds;
  } else if (extension) {
    initial_bounds.set_width(
        extensions::AppLaunchInfo::GetLaunchWidth(extension));
    initial_bounds.set_height(
        extensions::AppLaunchInfo::GetLaunchHeight(extension));
  }

  // Place new windows on the specified display.
  display::ScopedDisplayForNewWindows scoped_display(params.display_id);

  // TODO(erg): AppLaunchParams should pass through the user_gesture from the
  // extension system here.
  Browser::CreateParams browser_params(
      params.disposition == WindowOpenDisposition::NEW_POPUP
          ? Browser::CreateParams::CreateForAppPopup(app_name,
                                                     /*trusted_source=*/true,
                                                     initial_bounds, profile,
                                                     /*user_gesture=*/true)
          : Browser::CreateParams::CreateForApp(app_name,
                                                /*trusted_source=*/true,
                                                initial_bounds, profile,
                                                /*user_gesture=*/true));

  browser_params.initial_show_state =
      DetermineWindowShowState(profile, params.container, extension);
  browser_params.can_resize = can_resize;

  return new Browser(browser_params);
}

WebContents* NavigateApplicationWindow(Browser* browser,
                                       const apps::AppLaunchParams& params,
                                       const GURL& url,
                                       WindowOpenDisposition disposition) {
  const Extension* const extension = GetExtension(browser->profile(), params);
  ui::PageTransition transition =
      (extension ? ui::PAGE_TRANSITION_AUTO_BOOKMARK
                 : ui::PAGE_TRANSITION_AUTO_TOPLEVEL);

  NavigateParams nav_params(browser, url, transition);
  nav_params.disposition = disposition;
  Navigate(&nav_params);

  WebContents* const web_contents = nav_params.navigated_or_inserted_contents;

  if (extension && !extension->from_bookmark()) {
    DCHECK(extension->is_app());
    extensions::TabHelper::FromWebContents(web_contents)
        ->SetExtensionApp(extension);
  }
  web_app::SetAppPrefsForWebContents(web_contents);

  // TODO(https://crbug.com/1032443):
  // Eventually move this to browser_navigator.cc: CreateTargetContents().
  if (extension && extension->from_bookmark()) {
    web_app::WebAppTabHelperBase* tab_helper =
        web_app::WebAppTabHelperBase::FromWebContents(web_contents);
    DCHECK(tab_helper);
    tab_helper->SetAppId(extension->id());
  }

  return web_contents;
}

WebContents* OpenApplicationWindow(Profile* profile,
                                   const apps::AppLaunchParams& params,
                                   const GURL& url) {
  Browser* browser = CreateApplicationWindow(profile, params, url);
  WebContents* web_contents = NavigateApplicationWindow(
      browser, params, url, WindowOpenDisposition::NEW_FOREGROUND_TAB);

  browser->window()->Show();
  return web_contents;
}

void OpenApplicationWithReenablePrompt(Profile* profile,
                                       const apps::AppLaunchParams& params) {
  const Extension* extension = GetExtension(profile, params);
  if (!extension)
    return;

  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  ExtensionRegistry* registry = ExtensionRegistry::Get(profile);
  if (!service->IsExtensionEnabled(extension->id()) ||
      registry->GetExtensionById(extension->id(),
                                 ExtensionRegistry::TERMINATED)) {
    base::Callback<gfx::NativeWindow(void)> dialog_parent_window_getter;
    // TODO(pkotwicz): Figure out which window should be used as the parent for
    // the "enable application" dialog in Athena.
    (new EnableViaDialogFlow(
         service, registry, profile, extension->id(),
         base::Bind(base::IgnoreResult(OpenEnabledApplication), profile,
                    params)))
        ->Run();
    return;
  }

  OpenEnabledApplication(profile, params);
}

WebContents* OpenAppShortcutWindow(Profile* profile, const GURL& url) {
  apps::AppLaunchParams launch_params(
      std::string(),  // this is a URL app. No app id.
      extensions::LaunchContainer::kLaunchContainerWindow,
      WindowOpenDisposition::NEW_WINDOW,
      extensions::AppLaunchSource::kSourceCommandLine);
  launch_params.override_url = url;

  WebContents* tab = OpenApplicationWindow(profile, launch_params, url);

  if (!tab)
    return NULL;

  return tab;
}

bool CanLaunchViaEvent(const extensions::Extension* extension) {
  const extensions::Feature* feature =
      extensions::FeatureProvider::GetAPIFeature("app.runtime");
  return feature && feature->IsAvailableToExtension(extension).is_available();
}

void LaunchAppWithCallback(
    Profile* profile,
    const std::string& app_id,
    const base::CommandLine& command_line,
    const base::FilePath& current_directory,
    base::OnceCallback<void(Browser* browser,
                            apps::mojom::LaunchContainer container)> callback) {
  apps::mojom::LaunchContainer container;
  if (apps::OpenExtensionApplicationWindow(profile, app_id, command_line,
                                           current_directory)) {
    const extensions::Extension* extension =
        extensions::ExtensionRegistry::Get(profile)->GetInstalledExtension(
            app_id);
    // TODO(crbug.com/1061843): Remove this when BMO launches.
    if (extension && extension->from_bookmark())
      web_app::RecordAppWindowLaunch(profile, app_id);

    container = apps::mojom::LaunchContainer::kLaunchContainerWindow;
  } else if (apps::OpenExtensionApplicationTab(profile, app_id)) {
    container = apps::mojom::LaunchContainer::kLaunchContainerTab;
  } else {
    // Open an empty browser window as the app_id is invalid.
    apps::CreateBrowserWithNewTabPage(profile);
    container = apps::mojom::LaunchContainer::kLaunchContainerNone;
  }
  std::move(callback).Run(BrowserList::GetInstance()->GetLastActive(),
                          container);
}
