// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/web_app_launch_utils.h"

#include <atomic>
#include <memory>
#include <optional>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/json/values_util.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/not_fatal_until.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/one_shot_event.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/strings/to_string.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/types/optional_util.h"
#include "base/values.h"
#include "build/buildflag.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/apps/link_capturing/enable_link_capturing_infobar_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/sessions/app_session_service.h"
#include "chrome/browser/sessions/app_session_service_factory.h"
#include "chrome/browser/sessions/session_service_base.h"
#include "chrome/browser/sessions/session_service_lookup.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/intent_picker_tab_helper.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/web_app_browser_controller.h"
#include "chrome/browser/ui/web_applications/web_app_launch_process.h"
#include "chrome/browser/ui/web_applications/web_app_launch_utils.h"
#include "chrome/browser/ui/web_applications/web_app_tabbed_utils.h"
#include "chrome/browser/web_applications/link_capturing_features.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom-shared.h"
#include "chrome/browser/web_applications/navigation_capturing_log.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "components/webapps/browser/launch_queue/launch_params.h"
#include "components/webapps/browser/launch_queue/launch_queue.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "extensions/buildflags/buildflags.h"
#include "third_party/blink/public/common/renderer_preferences/renderer_preferences.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom-shared.h"
#include "third_party/blink/public/mojom/manifest/manifest_launch_handler.mojom-shared.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/ui_base_types.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/ui/extensions/hosted_app_browser_controller.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_browser_controller_ash.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/web_applications/chromeos_web_app_experiments.h"
#include "chromeos/ash/experiences/system_web_apps/types/system_web_app_delegate.h"
#include "chromeos/components/kiosk/kiosk_utils.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_WIN)
#include "base/strings/utf_string_conversions.h"
#endif

namespace web_app {
namespace {

Browser* ReparentWebContentsIntoAppBrowser(content::WebContents* contents,
                                           Browser* target_browser,
                                           const webapps::AppId& app_id,
                                           bool insert_as_pinned_home_tab) {
  DCHECK(target_browser->is_type_app());
  Browser* source_browser = chrome::FindBrowserWithTab(contents);
  CHECK(contents);

  if (!insert_as_pinned_home_tab) {
    MaybeAddPinnedHomeTab(target_browser, app_id);
  }

  // Avoid causing an existing non-app browser window to close if this is the
  // last tab remaining.
  if (source_browser->tab_strip_model()->count() == 1) {
    chrome::NewTab(source_browser);
  }

  ReparentWebContentsIntoBrowserImpl(
      source_browser, contents, target_browser,
      /*insert_as_pinned_first_tab=*/insert_as_pinned_home_tab);
  return target_browser;
}

#if BUILDFLAG(IS_CHROMEOS)
const ash::SystemWebAppDelegate* GetSystemWebAppDelegate(
    Browser* browser,
    const webapps::AppId& app_id) {
  auto system_app_type =
      ash::GetSystemWebAppTypeForAppId(browser->profile(), app_id);
  if (system_app_type) {
    return ash::SystemWebAppManager::Get(browser->profile())
        ->GetSystemApp(*system_app_type);
  }
  return nullptr;
}
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS)
std::unique_ptr<AppBrowserController> CreateWebKioskBrowserController(
    Browser* browser,
    WebAppProvider* provider,
    const webapps::AppId& app_id) {
  return std::make_unique<ash::WebKioskBrowserControllerAsh>(*provider, browser,
                                                             app_id);
}
#endif  // BUILDFLAG(IS_CHROMEOS)

std::unique_ptr<AppBrowserController> CreateWebAppBrowserController(
    Browser* browser,
    WebAppProvider* provider,
    const webapps::AppId& app_id) {
  bool should_have_tab_strip_for_swa = false;
#if BUILDFLAG(IS_CHROMEOS)
  const ash::SystemWebAppDelegate* system_app =
      GetSystemWebAppDelegate(browser, app_id);
  should_have_tab_strip_for_swa =
      system_app && system_app->ShouldHaveTabStrip();
#endif  // BUILDFLAG(IS_CHROMEOS)
  const bool has_tab_strip =
      !browser->is_type_app_popup() &&
      (should_have_tab_strip_for_swa ||
       provider->registrar_unsafe().IsTabbedWindowModeEnabled(app_id));
  return std::make_unique<WebAppBrowserController>(*provider, browser, app_id,
#if BUILDFLAG(IS_CHROMEOS)
                                                   system_app,
#endif  // BUILDFLAG(IS_CHROMEOS)
                                                   has_tab_strip);
}

std::unique_ptr<AppBrowserController> MaybeCreateHostedAppBrowserController(
    Browser* browser,
    const webapps::AppId& app_id) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(browser->profile())
          ->GetExtensionById(app_id, extensions::ExtensionRegistry::EVERYTHING);
  if (extension && extension->is_hosted_app()) {
    return std::make_unique<extensions::HostedAppBrowserController>(browser);
  }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
  return nullptr;
}

base::Value::Dict ToDebugDict(const apps::AppLaunchParams& params) {
  base::Value::Dict value;
  value.Set("app_id", params.app_id);
  value.Set("launch_id", params.launch_id);
  value.Set("container", static_cast<int>(params.container));
  value.Set("disposition", static_cast<int>(params.disposition));
  value.Set("override_url", params.override_url.spec());
  value.Set("override_bounds", params.override_bounds.ToString());
  value.Set("override_app_name", params.override_app_name);
  value.Set("restore_id", params.restore_id);
#if BUILDFLAG(IS_WIN)
  value.Set("command_line",
            base::WideToUTF8(params.command_line.GetCommandLineString()));
#else
  value.Set("command_line", params.command_line.GetCommandLineString());
#endif
  value.Set("current_directory",
            base::FilePathToValue(params.current_directory));
  value.Set("launch_source", static_cast<int>(params.launch_source));
  value.Set("display_id", base::saturated_cast<int>(params.display_id));
  base::Value::List files_list;
  for (const base::FilePath& file : params.launch_files) {
    files_list.Append(base::FilePathToValue(file));
  }
  value.Set("launch_files", std::move(files_list));
  value.Set("intent", params.intent ? "<set>" : "<not set>");
  value.Set("url_handler_launch_url",
            params.url_handler_launch_url.value_or(GURL()).spec());
  value.Set("protocol_handler_launch_url",
            params.protocol_handler_launch_url.value_or(GURL()).spec());
  value.Set("omit_from_session_restore", params.omit_from_session_restore);
  return value;
}

bool IsNavigationCapturingReimplExperimentEnabled(
    const std::optional<webapps::AppId>& current_browser_app_id,
    const GURL& url,
    const std::optional<webapps::AppId>& controlling_app_id,
    const std::optional<blink::mojom::DisplayMode>& display_mode) {
  if (display_mode &&
      !WebAppRegistrar::IsSupportedDisplayModeForNavigationCapture(
          *display_mode)) {
    return false;
  }
  // Enabling the generic flag turns it on for all navigations.
  if (apps::features::IsNavigationCapturingReimplEnabled()) {
    if (!features::kForcedOffCapturingAppsOnFirstNavigation.Get().empty() &&
        controlling_app_id.has_value()) {
      std::vector<std::string> forced_capturing_off_app_ids = base::SplitString(
          features::kForcedOffCapturingAppsOnFirstNavigation.Get(), ",",
          base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
      for (const std::string& forced_capturing_off_app_id :
           forced_capturing_off_app_ids) {
        if (controlling_app_id == forced_capturing_off_app_id) {
          return false;
        }
      }
    }
    return true;
  }

#if BUILDFLAG(IS_CHROMEOS)
  // Check application-specific flags.
  if (controlling_app_id.has_value() &&
      ::web_app::ChromeOsWebAppExperiments::
          IsNavigationCapturingReimplEnabledForTargetApp(*controlling_app_id)) {
    return true;
  }
  if (current_browser_app_id.has_value() &&
      ::web_app::ChromeOsWebAppExperiments::
          IsNavigationCapturingReimplEnabledForSourceApp(
              *current_browser_app_id, url)) {
    return true;
  }
#endif

  return false;
}

void RecordDiyOrCraftedAppLaunch(const WebApp& web_app) {
  base::UmaHistogramEnumeration(
      "Launch.WebApp.DiyOrCrafted",
      web_app.is_diy_app() ? LaunchedAppType::kDiy : LaunchedAppType::kCrafted);
}

}  // namespace

void ReparentWebContentsIntoBrowserImpl(Browser* source_browser,
                                        content::WebContents* web_contents,
                                        Browser* target_browser,
                                        bool insert_as_pinned_home_tab) {
  CHECK(source_browser);
  CHECK(web_contents);
  CHECK(target_browser);
  CHECK(AreWebAppsEnabled(target_browser->profile()));
  CHECK(AreWebAppsEnabled(source_browser->profile()));
  CHECK_EQ(source_browser->profile(), target_browser->profile());

  // In a reparent, the owning session service needs to be told it's tab
  // has been removed, otherwise it will reopen the tab on restoration.
  SessionServiceBase* service =
      GetAppropriateSessionServiceForProfile(source_browser);
  service->TabClosing(web_contents);

  // Check-fail if the web contents given is not part of the source browser.
  std::optional<int> found_tab_index;
  for (int i = 0; i < source_browser->tab_strip_model()->count(); ++i) {
    if (source_browser->tab_strip_model()->GetWebContentsAt(i) ==
        web_contents) {
      found_tab_index = i;
      break;
    }
  }
  CHECK(found_tab_index);

  TabStripModel* const source_tabstrip = source_browser->tab_strip_model();
  const std::optional<webapps::AppId> source_app_id =
      AppBrowserController::IsWebApp(source_browser)
          ? source_browser->app_controller()->app_id()
          : std::optional<webapps::AppId>(std::nullopt);
  const std::optional<webapps::AppId> target_app_id =
      AppBrowserController::IsWebApp(target_browser)
          ? target_browser->app_controller()->app_id()
          : std::optional<webapps::AppId>(std::nullopt);

  // Always reset the window controls overlay titlebar area when going to a
  // browser window or the app ids are different. The code will no-op if the old
  // rect matches the new rect.
  if (!target_app_id || target_app_id != source_app_id) {
    web_contents->UpdateWindowControlsOverlay(gfx::Rect());
  }

  std::unique_ptr<content::WebContents> contents_move =
      source_tabstrip->DetachWebContentsAtForInsertion(found_tab_index.value());
  int location = target_browser->tab_strip_model()->count();
  int add_types = (AddTabTypes::ADD_INHERIT_OPENER | AddTabTypes::ADD_ACTIVE);
  if (insert_as_pinned_home_tab) {
    location = 0;
    add_types |= AddTabTypes::ADD_PINNED;
  }
  const bool target_has_pinned_home_tab =
      HasPinnedHomeTab(target_browser->tab_strip_model());
  // This method moves a WebContents from a non-normal browser window to a
  // normal browser window. We cannot move the Tab over directly since TabModel
  // enforces the requirement that it cannot move between window types.
  // https://crbug.com/334281979): Non-normal browser windows should not have a
  // tab to begin with.
  target_browser->tab_strip_model()->InsertWebContentsAt(
      location, std::move(contents_move), add_types);
  CHECK_EQ(web_contents,
           target_browser->tab_strip_model()->GetActiveWebContents());

  if (insert_as_pinned_home_tab) {
    if (target_has_pinned_home_tab) {
      target_browser->tab_strip_model()->DetachAndDeleteWebContentsAt(1);
    }
    SetWebContentsIsPinnedHomeTab(
        target_browser->tab_strip_model()->GetWebContentsAt(0));
  }

  if (!target_app_id) {
    IntentPickerTabHelper* helper =
        IntentPickerTabHelper::FromWebContents(web_contents);
    CHECK(helper);
    helper->MaybeShowIntentPickerIcon();
  }
#if !BUILDFLAG(IS_CHROMEOS)
  if (source_app_id && source_app_id != target_app_id) {
    apps::EnableLinkCapturingInfoBarDelegate::RemoveInfoBar(web_contents);
  }
#endif
  target_browser->window()->Show();

  // The window will be registered correctly, however the tab will not be
  // correctly tracked. We need to do a reset to get the tab correctly tracked
  // by either the app service or the regular service
  SessionServiceBase* target_service =
      GetAppropriateSessionServiceForProfile(target_browser);
  target_service->ResetFromCurrentBrowsers();
}

std::optional<webapps::AppId> GetWebAppForActiveTab(const Browser* browser) {
  const content::WebContents* const web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  if (!web_contents) {
    return std::nullopt;
  }
  const WebAppTabHelper* tab_helper =
      WebAppTabHelper::FromWebContents(web_contents);
  if (!tab_helper) {
    return std::nullopt;
  }
  return tab_helper->app_id();
}

void PrunePreScopeNavigationHistory(const GURL& scope,
                                    content::WebContents* contents) {
  content::NavigationController& navigation_controller =
      contents->GetController();
  if (!navigation_controller.CanPruneAllButLastCommitted()) {
    return;
  }

  int index = navigation_controller.GetEntryCount() - 1;
  while (index >= 0 &&
         IsInScope(navigation_controller.GetEntryAtIndex(index)->GetURL(),
                   scope)) {
    --index;
  }

  while (index >= 0) {
    navigation_controller.RemoveEntryAtIndex(index);
    --index;
  }
}

bool MaybeHandleIntentPickerFocusExistingOrNavigateExisting(
    Profile* profile,
    const GURL& launch_url,
    content::WebContents* contents,
    const webapps::AppId& app_id,
    base::TimeTicks time_reparent_started,
    WebAppRegistrar& registrar) {
  LaunchHandler::ClientMode client_mode = registrar.GetAppById(app_id)
                                              ->launch_handler()
                                              .value_or(LaunchHandler())
                                              .parsed_client_mode();
  if (client_mode != LaunchHandler::ClientMode::kFocusExisting &&
      client_mode != LaunchHandler::ClientMode::kNavigateExisting) {
    return false;
  }
  std::optional<AppBrowserController::BrowserAndTabIndex> existing_app_host =
      AppBrowserController::FindTopLevelBrowsingContextForWebApp(
          *profile, app_id, Browser::TYPE_APP,
          /*for_focus_existing=*/client_mode ==
              LaunchHandler::ClientMode::kFocusExisting);
  if (!existing_app_host.has_value()) {
    return false;
  }
  CHECK(existing_app_host->browser);
  CHECK(WebAppBrowserController::IsWebApp(existing_app_host->browser));
  CHECK_NE(existing_app_host->tab_index, -1);

  content::WebContents* preexisting_web_contents =
      existing_app_host->browser->tab_strip_model()->GetWebContentsAt(
          existing_app_host->tab_index);
  CHECK(preexisting_web_contents != contents);

  // We've found a browser in the background. We need to focus it and enqueue
  // launch params. But first we ensure that the contents (for which the Intent
  // Picker was clicked) goes away without its containing browser closing.
  Browser* foreground_browser = chrome::FindBrowserWithTab(contents);
  if (foreground_browser->tab_strip_model()->count() == 1) {
    chrome::NewTab(foreground_browser);
  }

  contents->Close();

  FocusAppContainer(existing_app_host->browser, existing_app_host->tab_index);

  if (client_mode == LaunchHandler::ClientMode::kNavigateExisting) {
    NavigateParams nav_params(existing_app_host->browser, launch_url,
                              ui::PageTransition::PAGE_TRANSITION_LINK);
    Navigate(&nav_params);
  }

  RecordLaunchMetrics(app_id, apps::LaunchContainer::kLaunchContainerWindow,
                      apps::LaunchSource::kFromOmnibox, launch_url,
                      preexisting_web_contents);
  EnqueueLaunchParams(preexisting_web_contents, app_id, launch_url,
                      /*wait_for_navigation_to_complete=*/client_mode ==
                          LaunchHandler::ClientMode::kNavigateExisting,
                      time_reparent_started);
  return true;
}

Browser* ReparentWebAppForActiveTab(Browser* browser) {
  std::optional<webapps::AppId> app_id = GetWebAppForActiveTab(browser);
  if (!app_id) {
    return nullptr;
  }
  return ReparentWebContentsIntoAppBrowser(
      browser->tab_strip_model()->GetActiveWebContents(), *app_id);
}

Browser* ReparentWebContentsIntoAppBrowser(
    content::WebContents* contents,
    const webapps::AppId& app_id,
    base::OnceCallback<void(content::WebContents*)> completion_callback) {
  base::TimeTicks time_reparent_started = base::TimeTicks::Now();
  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());
  // Incognito tabs reparent correctly, but remain incognito without any
  // indication to the user, so disallow it.
  DCHECK(!profile->IsOffTheRecord());

  // Clear navigation history that occurred before the user most recently
  // entered the app's scope. The minimal-ui Back button will be initially
  // disabled if the previous page was outside scope. Packaged apps are not
  // affected.
  WebAppProvider* provider = WebAppProvider::GetForWebApps(profile);
  CHECK(provider);
  WebAppRegistrar& registrar = provider->registrar_unsafe();
  const WebApp* web_app = registrar.GetAppById(app_id);
  if (!web_app) {
    std::move(completion_callback).Run(contents);
    return nullptr;
  }

  if (registrar.IsInstallState(
          app_id, {proto::InstallState::SUGGESTED_FROM_ANOTHER_DEVICE,
                   proto::InstallState::INSTALLED_WITHOUT_OS_INTEGRATION,
                   proto::InstallState::INSTALLED_WITH_OS_INTEGRATION})) {
    std::optional<GURL> app_scope = registrar.GetAppScope(app_id);
    if (!app_scope) {
      app_scope = registrar.GetAppStartUrl(app_id).GetWithoutFilename();
    }

    PrunePreScopeNavigationHistory(*app_scope, contents);
  }
  WebAppTabHelper* tab_helper = WebAppTabHelper::FromWebContents(contents);
  // This function assumes `contents` is from a browser tab.
  CHECK(!tab_helper->is_in_app_window())
      << tab_helper->window_app_id().value_or("<none>");

  auto launch_url = contents->GetLastCommittedURL();
  UpdateLaunchStats(contents, app_id, launch_url);
  RecordLaunchMetrics(app_id, apps::LaunchContainer::kLaunchContainerWindow,
                      apps::LaunchSource::kFromReparenting, launch_url,
                      contents);
  blink::mojom::DisplayMode display_mode =
      registrar.GetAppEffectiveDisplayMode(app_id);

  // The current browser, in this situation, is a browser tab, so std::nullopt
  // is appropriate for `current_browser_app_id`.
  if (IsNavigationCapturingReimplExperimentEnabled(
          /*current_browser_app_id=*/std::nullopt, launch_url, app_id,
          display_mode)) {
    // The Intent Picker needs to respect kFocusExisting and kNavigateExisting,
    // by focusing such apps in the background instead of re-parenting the
    // current contents.
    if (MaybeHandleIntentPickerFocusExistingOrNavigateExisting(
            profile, launch_url, contents, app_id, time_reparent_started,
            registrar)) {
      return nullptr;
    }
  }

  if (web_app->launch_handler()
          .value_or(LaunchHandler{})
          .TargetsExistingClients() ||
      registrar.IsPreventCloseEnabled(web_app->app_id())) {
    if (AppBrowserController::FindForWebApp(*profile, app_id)) {
      // TODO(crbug.com/40246677): Use apps::AppServiceProxy::LaunchAppWithUrl()
      // instead to ensure all the usual wrapping code around web app launches
      // gets executed.
      apps::AppLaunchParams params(
          app_id, apps::LaunchContainer::kLaunchContainerWindow,
          WindowOpenDisposition::CURRENT_TAB, apps::LaunchSource::kFromOmnibox);
      params.override_url = launch_url;
      content::WebContents* new_web_contents =
          WebAppLaunchProcess::CreateAndRun(
              *profile, registrar, provider->os_integration_manager(), params);
      contents->Close();
      std::move(completion_callback).Run(new_web_contents);
      return chrome::FindBrowserWithTab(new_web_contents);
    }
  }

  Browser* browser = nullptr;

  if (registrar.IsTabbedWindowModeEnabled(app_id)) {
    browser = AppBrowserController::FindForWebApp(*profile, app_id);
  }

  if (!browser) {
    browser = Browser::Create(Browser::CreateParams::CreateForApp(
        GenerateApplicationNameFromAppId(app_id), true /* trusted_source */,
        gfx::Rect(), profile, true /* user_gesture */));

    // If the current url isn't in scope, then set the initial url on the
    // AppBrowserController so that the 'x' button still shows up.
    CHECK(browser->app_controller());
    browser->app_controller()->MaybeSetInitialUrlOnReparentTab();
  }

  bool as_pinned_home_tab =
      browser->app_controller()->IsUrlInHomeTabScope(launch_url);

  Browser* reparented_browser = ReparentWebContentsIntoAppBrowser(
      contents, browser, app_id, as_pinned_home_tab);
  std::move(completion_callback).Run(contents);
  return reparented_browser;
}

void SetWebContentsIsPinnedHomeTab(content::WebContents* contents) {
  auto* helper = WebAppTabHelper::FromWebContents(contents);
  helper->set_is_pinned_home_tab(true);
}

std::unique_ptr<AppBrowserController> MaybeCreateAppBrowserController(
    Browser* browser) {
  std::unique_ptr<AppBrowserController> controller;
  const webapps::AppId app_id =
      GetAppIdFromApplicationName(browser->app_name());
  auto* const provider =
      WebAppProvider::GetForLocalAppsUnchecked(browser->profile());
  if (provider &&
      provider->registrar_unsafe().IsInstallState(
          app_id, {proto::InstallState::SUGGESTED_FROM_ANOTHER_DEVICE,
                   proto::InstallState::INSTALLED_WITHOUT_OS_INTEGRATION,
                   proto::InstallState::INSTALLED_WITH_OS_INTEGRATION})) {
#if BUILDFLAG(IS_CHROMEOS)
    if (chromeos::IsKioskSession()) {
      controller = CreateWebKioskBrowserController(browser, provider, app_id);
    } else {
      controller = CreateWebAppBrowserController(browser, provider, app_id);
    }
#else
    controller = CreateWebAppBrowserController(browser, provider, app_id);
#endif  // BUILDFLAG(IS_CHROMEOS)
  } else {
    controller = MaybeCreateHostedAppBrowserController(browser, app_id);
  }
  if (controller) {
    controller->Init();
  }
  return controller;
}

void MaybeAddPinnedHomeTab(Browser* browser, const std::string& app_id) {
  WebAppRegistrar& registrar =
      WebAppProvider::GetForLocalAppsUnchecked(browser->profile())
          ->registrar_unsafe();
  std::optional<GURL> pinned_home_tab_url =
      registrar.GetAppPinnedHomeTabUrl(app_id);

  if (registrar.IsTabbedWindowModeEnabled(app_id) &&
      !HasPinnedHomeTab(browser->tab_strip_model()) &&
      pinned_home_tab_url.has_value()) {
    NavigateParams home_tab_nav_params(browser, pinned_home_tab_url.value(),
                                       ui::PAGE_TRANSITION_AUTO_BOOKMARK);
    home_tab_nav_params.disposition = WindowOpenDisposition::NEW_BACKGROUND_TAB;
    home_tab_nav_params.tabstrip_add_types |= AddTabTypes::ADD_PINNED;
    Navigate(&home_tab_nav_params);

    content::WebContents* const web_contents =
        home_tab_nav_params.navigated_or_inserted_contents;

    if (web_contents) {
      SetWebContentsIsPinnedHomeTab(web_contents);
    }
  }
}

void MaybeShowNavigationCaptureIph(webapps::AppId app_id,
                                   Profile* profile,
                                   Browser* browser) {
  // Prevent ChromeOS from reaching this function in tests.
#if !BUILDFLAG(IS_CHROMEOS)
  web_app::WebAppProvider* provider =
      web_app::WebAppProvider::GetForWebApps(profile);
  CHECK(provider);
  provider->ui_manager().MaybeShowIPHPromoForAppsLaunchedViaLinkCapturing(
      browser, profile, app_id);
#endif
}

Browser::CreateParams CreateParamsForApp(const webapps::AppId& app_id,
                                         bool is_popup,
                                         bool trusted_source,
                                         const gfx::Rect& window_bounds,
                                         Profile* profile,
                                         bool user_gesture) {
  std::string app_name = GenerateApplicationNameFromAppId(app_id);
  Browser::CreateParams params =
      is_popup
          ? Browser::CreateParams::CreateForAppPopup(
                app_name, trusted_source, window_bounds, profile, user_gesture)
          : Browser::CreateParams::CreateForApp(
                app_name, trusted_source, window_bounds, profile, user_gesture);
  params.initial_show_state = IsRunningInForcedAppMode()
                                  ? ui::mojom::WindowShowState::kFullscreen
                                  : ui::mojom::WindowShowState::kDefault;
  return params;
}

Browser* CreateWebAppWindowMaybeWithHomeTab(
    const webapps::AppId& app_id,
    const Browser::CreateParams& params) {
  CHECK(params.type == Browser::Type::TYPE_APP_POPUP ||
        params.type == Browser::Type::TYPE_APP);
  Browser* browser = Browser::Create(params);
  CHECK(GenerateApplicationNameFromAppId(app_id) == browser->app_name());
  if (params.type != Browser::Type::TYPE_APP_POPUP) {
    MaybeAddPinnedHomeTab(browser, app_id);
  }
  return browser;
}

Browser* CreateWebAppWindowFromNavigationParams(
    const webapps::AppId& app_id,
    const NavigateParams& navigate_params,
    bool should_create_app_popup = false) {
  Browser::CreateParams app_browser_params = CreateParamsForApp(
      app_id, should_create_app_popup, navigate_params.trusted_source,
      navigate_params.window_features.bounds,
      navigate_params.initiating_profile, navigate_params.user_gesture);
  Browser* created_browser =
      CreateWebAppWindowMaybeWithHomeTab(app_id, app_browser_params);
  return created_browser;
}

content::WebContents* NavigateWebAppUsingParams(NavigateParams& nav_params) {
  nav_params.pwa_navigation_capturing_force_off = true;
  if (nav_params.browser->app_controller() &&
      nav_params.browser->app_controller()->IsUrlInHomeTabScope(
          nav_params.url)) {
    // Navigations to the home tab URL in tabbed apps should happen in the home
    // tab.
    nav_params.browser->tab_strip_model()->ActivateTabAt(0);
    content::WebContents* home_tab_web_contents =
        nav_params.browser->tab_strip_model()->GetWebContentsAt(0);
    GURL previous_home_tab_url = home_tab_web_contents->GetLastCommittedURL();
    if (previous_home_tab_url == nav_params.url) {
      // URL is identical so no need for the navigation.
      return home_tab_web_contents;
    }
    nav_params.disposition = WindowOpenDisposition::CURRENT_TAB;
  }

#if BUILDFLAG(IS_CHROMEOS)
  Browser* browser = nav_params.browser;
  const std::optional<ash::SystemWebAppType> capturing_system_app_type =
      ash::GetCapturingSystemAppForURL(browser->profile(), nav_params.url);
  if (capturing_system_app_type &&
      (!browser ||
       !IsBrowserForSystemWebApp(browser, capturing_system_app_type.value()))) {
    // Web app launch process should receive the correct `NavigateParams`
    // argument from system web app launches, so that Navigate() call below
    // succeeds (i.e. don't trigger system web app link capture).
    //
    // This block safe guards against misuse of APIs (that can cause
    // GetCapturingSystemAppForURL returning the wrong value).
    //
    // TODO(crbug.com/40253765): Remove this block when we find a better
    // way to prevent API misuse (e.g. by ensuring test coverage for new
    // features that could trigger this code) or this code path is no longer
    // possible.
    base::debug::DumpWithoutCrashing();
    return nullptr;
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  Navigate(&nav_params);

  content::WebContents* const web_contents =
      nav_params.navigated_or_inserted_contents;

  return web_contents;
}

void RecordAppWindowLaunchMetric(Profile* profile,
                                 const std::string& app_id,
                                 apps::LaunchSource launch_source) {
  WebAppProvider* provider = WebAppProvider::GetForLocalAppsUnchecked(profile);
  if (!provider) {
    return;
  }

  const WebApp* web_app = provider->registrar_unsafe().GetAppById(app_id);
  if (!web_app) {
    return;
  }

  DisplayMode display =
      provider->registrar_unsafe().GetAppEffectiveDisplayMode(app_id);
  if (display != DisplayMode::kUndefined) {
    DCHECK_LT(DisplayMode::kUndefined, display);
    DCHECK_LE(display, DisplayMode::kMaxValue);
    base::UmaHistogramEnumeration("Launch.WebAppDisplayMode", display);
    if (web_app->is_diy_app()) {
      base::UmaHistogramEnumeration("Launch.Window.DiyApp.WebAppDisplayMode",
                                    display);
    }
  }

  // Reparenting launches don't respect the launch_handler setting.
  if (launch_source != apps::LaunchSource::kFromReparenting) {
    base::UmaHistogramEnumeration("Launch.WebAppLaunchHandlerClientMode",
                                  web_app->launch_handler()
                                      .value_or(LaunchHandler())
                                      .parsed_client_mode());
  }

  RecordDiyOrCraftedAppLaunch(*web_app);
}

void RecordAppTabLaunchMetric(Profile* profile,
                              const std::string& app_id,
                              apps::LaunchSource launch_source) {
  WebAppProvider* provider = WebAppProvider::GetForLocalAppsUnchecked(profile);
  if (!provider) {
    return;
  }

  const WebApp* web_app = provider->registrar_unsafe().GetAppById(app_id);
  if (!web_app) {
    return;
  }

  // Measure the display mode that was specified in the manifest if this app was
  // set to open in a standalone window.
  DisplayMode display =
      provider->registrar_unsafe().GetEffectiveDisplayModeFromManifest(app_id);
  if (display != DisplayMode::kUndefined) {
    DCHECK_LT(DisplayMode::kUndefined, display);
    DCHECK_LE(display, DisplayMode::kMaxValue);
    base::UmaHistogramEnumeration("Launch.BrowserTab.WebAppDisplayMode",
                                  display);
    if (web_app->is_diy_app()) {
      base::UmaHistogramEnumeration(
          "Launch.BrowserTab.DiyApp.WebAppDisplayMode", display);
    }
  }

  // Reparenting launches don't respect the launch_handler setting.
  if (launch_source != apps::LaunchSource::kFromReparenting) {
    base::UmaHistogramEnumeration(
        "Launch.BrowserTab.WebAppLaunchHandlerClientMode",
        web_app->launch_handler()
            .value_or(LaunchHandler())
            .parsed_client_mode());
  }

  RecordDiyOrCraftedAppLaunch(*web_app);
}

void RecordLaunchMetrics(const webapps::AppId& app_id,
                         apps::LaunchContainer container,
                         apps::LaunchSource launch_source,
                         const GURL& launch_url,
                         content::WebContents* web_contents) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());

#if BUILDFLAG(IS_CHROMEOS)
  // System web apps have different launch paths compared with web apps, and
  // those paths aren't configurable. So their launch metrics shouldn't be
  // reported to avoid skewing web app metrics.
  DCHECK(!ash::GetSystemWebAppTypeForAppId(profile, app_id))
      << "System web apps shouldn't be included in web app launch metrics";
#endif  // BUILDFLAG(IS_CHROMEOS)

  if (container == apps::LaunchContainer::kLaunchContainerWindow) {
    RecordAppWindowLaunchMetric(profile, app_id, launch_source);
  }
  if (container == apps::LaunchContainer::kLaunchContainerTab) {
    RecordAppTabLaunchMetric(profile, app_id, launch_source);
  }

  base::UmaHistogramEnumeration("WebApp.LaunchSource", launch_source);
  base::UmaHistogramEnumeration("WebApp.LaunchContainer", container);
}

void UpdateLaunchStats(content::WebContents* web_contents,
                       const webapps::AppId& app_id,
                       const GURL& launch_url) {
  CHECK(web_contents != nullptr);
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());

  WebAppProvider::GetForLocalAppsUnchecked(profile)
      ->sync_bridge_unsafe()
      .SetAppLastLaunchTime(app_id, base::Time::Now());

#if BUILDFLAG(IS_CHROMEOS)
  if (ash::GetSystemWebAppTypeForAppId(profile, app_id)) {
    // System web apps doesn't use the rest of the stats.
    return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  // Update the launch time in the site engagement service. A recent web
  // app launch will provide an engagement boost to the origin.
  site_engagement::SiteEngagementService::Get(profile)
      ->SetLastShortcutLaunchTime(web_contents, app_id, launch_url);
}

void LaunchWebApp(apps::AppLaunchParams params,
                  LaunchWebAppWindowSetting launch_setting,
                  Profile& profile,
                  WithAppResources& lock,
                  LaunchWebAppDebugValueCallback callback) {
  base::Value::Dict debug_value;
  debug_value.Set("launch_params", ToDebugDict(params));
  debug_value.Set("launch_window_setting", static_cast<int>(launch_setting));

  if (launch_setting == LaunchWebAppWindowSetting::kOverrideWithWebAppConfig) {
    DisplayMode display_mode =
        lock.registrar().GetAppEffectiveDisplayMode(params.app_id);
    switch (display_mode) {
      case DisplayMode::kUndefined:
      case DisplayMode::kFullscreen:
      case DisplayMode::kBrowser:
        params.container = apps::LaunchContainer::kLaunchContainerTab;
        break;
      case DisplayMode::kMinimalUi:
      case DisplayMode::kWindowControlsOverlay:
      case DisplayMode::kTabbed:
      case DisplayMode::kBorderless:
      case DisplayMode::kPictureInPicture:
      case DisplayMode::kStandalone:
        params.container = apps::LaunchContainer::kLaunchContainerWindow;
        break;
    }
  }

  DCHECK_NE(params.container, apps::LaunchContainer::kLaunchContainerNone);

  apps::LaunchContainer container;
  Browser* browser = nullptr;
  content::WebContents* web_contents = nullptr;
  // Do not launch anything if the profile is being deleted.
  if (Browser::GetCreationStatusForProfile(&profile) ==
      Browser::CreationStatus::kOk) {
    if (lock.registrar().IsInstallState(
            params.app_id,
            {proto::InstallState::SUGGESTED_FROM_ANOTHER_DEVICE,
             proto::InstallState::INSTALLED_WITHOUT_OS_INTEGRATION,
             proto::InstallState::INSTALLED_WITH_OS_INTEGRATION})) {
      container = params.container;
      if (WebAppLaunchProcess::GetOpenApplicationCallbackForTesting()) {
        WebAppLaunchProcess::GetOpenApplicationCallbackForTesting().Run(
            std::move(params));
      } else {
        web_contents = WebAppLaunchProcess::CreateAndRun(
            profile, lock.registrar(), lock.os_integration_manager(), params);
      }
      if (web_contents) {
        browser = chrome::FindBrowserWithTab(web_contents);
      }
    } else {
      debug_value.Set("error", "Unknown app id.");
      // Open an empty browser window as the app_id is invalid.
      DVLOG(1) << "Cannot launch app with unknown id: " << params.app_id;
      container = apps::LaunchContainer::kLaunchContainerNone;
      browser = apps::CreateBrowserWithNewTabPage(&profile);
    }
  } else {
    std::string error_str = base::StringPrintf(
        "Cannot launch app %s without profile creation: %d",
        params.app_id.c_str(),
        static_cast<int>(Browser::GetCreationStatusForProfile(&profile)));
    debug_value.Set("error", error_str);
    DVLOG(1) << error_str;
  }

  debug_value.Set("web_contents", base::ToString(web_contents));
  debug_value.Set("browser", base::ToString(browser));

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback),
                     browser ? browser->AsWeakPtr() : nullptr,
                     web_contents ? web_contents->GetWeakPtr() : nullptr,
                     container, base::Value(std::move(debug_value))));
}

void EnqueueLaunchParams(content::WebContents* contents,
                         const webapps::AppId& app_id,
                         const GURL& url,
                         bool wait_for_navigation_to_complete,
                         base::TimeTicks time_navigation_started) {
  CHECK(contents);
  webapps::LaunchParams launch_params;
  launch_params.started_new_navigation = wait_for_navigation_to_complete;
  launch_params.app_id = app_id;
  launch_params.target_url = url;
  if (!time_navigation_started.is_null()) {
    launch_params.time_navigation_started_for_enqueue = time_navigation_started;
  }
  WebAppTabHelper::FromWebContents(contents)->EnsureLaunchQueue().Enqueue(
      std::move(launch_params));
}

void FocusAppContainer(Browser* browser, int tab_index) {
  CHECK(browser);
  content::WebContents* const web_contents =
      browser->tab_strip_model()->GetWebContentsAt(tab_index);
  CHECK(web_contents);
  web_contents->Focus();
  // ActivateTabAt() does not work for PWA windows.
  if (!WebAppBrowserController::IsWebApp(browser)) {
    // Note: This will CHECK-fail if tab_index is invalid.
    browser->tab_strip_model()->ActivateTabAt(tab_index);
  }
  // This call will un-minimize the window.
  browser->GetBrowserView().Activate();
}

}  // namespace web_app
