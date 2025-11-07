// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/navigation_capturing_process.h"

#include "base/debug/crash_logging.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_split.h"
#include "base/time/clock.h"
#include "base/types/optional_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/navigation_handle_user_data_forwarder.h"
#include "chrome/browser/ui/web_applications/web_app_browser_controller.h"
#include "chrome/browser/ui/web_applications/web_app_launch_navigation_handle_user_data.h"
#include "chrome/browser/ui/web_applications/web_app_launch_utils.h"
#include "chrome/browser/ui/web_applications/web_app_tabbed_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/window_management/isolated_web_apps_opened_tabs_counter_service.h"
#include "chrome/browser/web_applications/isolated_web_apps/window_management/isolated_web_apps_opened_tabs_counter_service_factory.h"
#include "chrome/browser/web_applications/link_capturing_features.h"
#include "chrome/browser/web_applications/navigation_capturing_log.h"
#include "chrome/browser/web_applications/navigation_capturing_metrics.h"
#include "chrome/browser/web_applications/navigation_capturing_settings.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_filter.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/common/content_features.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom-shared.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/web_applications/chromeos_web_app_experiments.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace web_app {

namespace {

bool IsDispositionValidForNavigationCapturing(
    WindowOpenDisposition disposition) {
  switch (disposition) {
    case WindowOpenDisposition::NEW_FOREGROUND_TAB:
    case WindowOpenDisposition::NEW_BACKGROUND_TAB:
    case WindowOpenDisposition::NEW_WINDOW:
      return true;
    case WindowOpenDisposition::UNKNOWN:
      // Note: App popups are handled in browser_navigator.cc
    case WindowOpenDisposition::NEW_POPUP:
    case WindowOpenDisposition::CURRENT_TAB:
    case WindowOpenDisposition::SINGLETON_TAB:
    case WindowOpenDisposition::SAVE_TO_DISK:
    case WindowOpenDisposition::OFF_THE_RECORD:
    case WindowOpenDisposition::IGNORE_ACTION:
    case WindowOpenDisposition::SWITCH_TO_TAB:
    case WindowOpenDisposition::NEW_PICTURE_IN_PICTURE:
      return false;
  }
}

bool IsPageTransitionValidForNavigationCapturing(
    ui::PageTransition transition) {
  switch (ui::PageTransitionStripQualifier(transition)) {
    case ui::PAGE_TRANSITION_TYPED:
    case ui::PAGE_TRANSITION_AUTO_TOPLEVEL:
    case ui::PAGE_TRANSITION_AUTO_BOOKMARK:
    case ui::PAGE_TRANSITION_AUTO_SUBFRAME:
    case ui::PAGE_TRANSITION_MANUAL_SUBFRAME:
    case ui::PAGE_TRANSITION_GENERATED:
    case ui::PAGE_TRANSITION_RELOAD:
    case ui::PAGE_TRANSITION_KEYWORD:
    case ui::PAGE_TRANSITION_KEYWORD_GENERATED:
      return false;
    case ui::PAGE_TRANSITION_LINK:
    case ui::PAGE_TRANSITION_FORM_SUBMIT:
      break;
    default:
      NOTREACHED();
  }
  if (base::to_underlying(ui::PageTransitionGetQualifier(transition)) != 0) {
    // Qualifiers indicate that this navigation was the result of a click on a
    // forward/back button, typing in the URL bar, or client-side redirections.
    // Don't handle any of those types of navigations.
    return false;
  }
  return true;
}

// Returns true if an auxiliary browsing context is getting created, so
// navigation should be done in the same container that it was triggered in.
bool IsAuxiliaryBrowsingContext(const NavigateParams& nav_params) {
  if ((nav_params.contents_to_insert &&
       nav_params.contents_to_insert->HasOpener()) ||
      nav_params.opener) {
    return true;
  }
  return false;
}

Browser* CreateWebAppWindowFromNavigationParams(
    const webapps::AppId& app_id,
    const NavigateParams& navigate_params) {
  Browser::CreateParams app_browser_params = CreateParamsForApp(
      app_id, /*is_popup=*/false,
      /*trusted_source=*/true, navigate_params.window_features.bounds,
      navigate_params.initiating_profile, navigate_params.user_gesture);
  Browser* created_browser =
      CreateWebAppWindowMaybeWithHomeTab(app_id, app_browser_params);
  return created_browser;
}

// TODO(crbug.com/371237535): Move to TabInterface once there is support for
// getting the browser interface for web contents that are in an app window.
// For all use-cases where a reparenting to an app window happens, launch params
// need to be enqueued so as to mimic the pre redirection behavior. See
// https://bit.ly/pwa-navigation-capturing?tab=t.0#bookmark=id.60x2trlfg6iq for
// more information.
void ReparentToAppBrowser(content::WebContents* old_web_contents,
                          const webapps::AppId& app_id,
                          blink::mojom::DisplayMode target_display_mode,
                          const GURL& target_url) {
  Browser* main_browser = chrome::FindBrowserWithTab(old_web_contents);
  BrowserWindowInterface* target_browser = nullptr;
  if (target_display_mode == blink::mojom::DisplayMode::kTabbed) {
    target_browser =
        AppBrowserController::FindForWebApp(*main_browser->profile(), app_id);
    // If somehow we found a browser that doesn't have a tab strip (which
    // might be possible if the manifest updated while a window is open),
    // don't return it to use for new tabs.
    if (target_browser &&
        !AppBrowserController::From(target_browser)->has_tab_strip()) {
      target_browser = nullptr;
    }
  }
  if (!target_browser) {
    target_browser = CreateWebAppWindowMaybeWithHomeTab(
        app_id,
        CreateParamsForApp(app_id, /*is_popup=*/false, /*trusted_source=*/true,
                           gfx::Rect(), main_browser->profile(),
                           /*user_gesture=*/true));
  }
  CHECK(AppBrowserController::IsWebApp(target_browser));
  ReparentWebContentsIntoBrowserImpl(main_browser, old_web_contents,
                                     target_browser,
                                     AppBrowserController::From(target_browser)
                                         ->IsUrlInHomeTabScope(target_url));
  CHECK(old_web_contents);
}

// TODO(crbug.com/371237535): Move to TabInterface once there is support for
// getting the browser interface for web contents that are in an app window.
void ReparentWebContentsToTabbedBrowser(content::WebContents* old_web_contents,
                                        WindowOpenDisposition disposition,
                                        Browser* navigate_params_browser) {
  Browser* source_browser = chrome::FindBrowserWithTab(old_web_contents);
  Browser* existing_browser_window =
      navigate_params_browser &&
              !AppBrowserController::IsWebApp(navigate_params_browser)
          ? navigate_params_browser
          : chrome::FindTabbedBrowser(source_browser->profile(),
                                      /*match_original_profiles=*/false);

  // Create a new browser window if the navigation was triggered via a
  // shift-click, or if there are no open tabbed browser windows at the moment.
  Browser* target_browser_window =
      (disposition == WindowOpenDisposition::NEW_WINDOW ||
       !existing_browser_window)
          ? Browser::Create(Browser::CreateParams(source_browser->profile(),
                                                  /*user_gesture=*/true))
          : existing_browser_window;

  ReparentWebContentsIntoBrowserImpl(source_browser, old_web_contents,
                                     target_browser_window);
}

BrowserWindowInterface* FindNormalBrowser(const Profile& profile) {
  BrowserWindowInterface* normal_browser = nullptr;
  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [&](BrowserWindowInterface* browser) {
        if (browser->GetType() == BrowserWindowInterface::TYPE_NORMAL &&
            browser->GetProfile() == &profile) {
          normal_browser = browser;
          return false;  // stop iterating
        }
        return true;  // continue iterating
      });
  return normal_browser;
}

// Record the result of navigation capturing before redirection happens or a
// network request has been made.
void RecordInitialNavigationCapturingResult(
    NavigationCapturingInitialResult result) {
  base::UmaHistogramEnumeration("Webapp.NavigationCapturing.Result", result);
}

}  // namespace

NavigationCapturingOverride::~NavigationCapturingOverride() = default;

// static
NavigationCapturingOverride NavigationCapturingOverride::CreateForNavigateNew(
    base::PassKey<NavigationCapturingProcess>,
    Browser* browser) {
  CHECK(browser);
  return NavigationCapturingOverride(browser);
}

// static
NavigationCapturingOverride
NavigationCapturingOverride::CreateForCancelNavigation(
    base::PassKey<NavigationCapturingProcess>) {
  return NavigationCapturingOverride(/*browser=*/nullptr);
}

// static
NavigationCapturingOverride NavigationCapturingOverride::CreateForFocusExisting(
    base::PassKey<NavigationCapturingProcess>,
    content::WebContents* web_contents) {
  // TODO(crbug.com/428933391): Set result.focused_contents and propagate this
  // to NavigateParams.
  return NavigationCapturingOverride(/*browser=*/nullptr);
}

// static
NavigationCapturingOverride
NavigationCapturingOverride::CreateForNavigateExisting(
    base::PassKey<NavigationCapturingProcess>,
    Browser* browser,
    int tab_index) {
  CHECK(browser->tab_strip_model()->GetWebContentsAt(tab_index));
  return NavigationCapturingOverride(browser, tab_index);
}

NavigationCapturingOverride::NavigationCapturingOverride(
    Browser* browser,
    std::optional<int> tab_index)
    : browser_(browser), tab_index_(std::move(tab_index)) {}

// static
std::unique_ptr<NavigationCapturingProcess>
NavigationCapturingProcess::MaybeHandleAppNavigation(
    const NavigateParams& params) {
  Profile* profile = params.initiating_profile;

#if BUILDFLAG(IS_CHROMEOS)
  // System Web Apps should not be going through the navigation capturing
  // process.
  const std::optional<ash::SystemWebAppType> capturing_system_app_type =
      ash::GetCapturingSystemAppForURL(profile, params.url);
  if (capturing_system_app_type.has_value()) {
    if (params.browser && ash::IsBrowserForSystemWebApp(
                              params.browser->GetBrowserForMigrationOnly(),
                              capturing_system_app_type.value())) {
      RecordInitialNavigationCapturingResult(
          NavigationCapturingInitialResult::kNotHandled);
      return nullptr;
    }
    // This process should never be called for URLS captured by system web apps
    // from a non-system-web-app browser.
    NOTREACHED();
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  if (!AreWebAppsUserInstallable(profile) ||
      Browser::GetCreationStatusForProfile(profile) !=
          Browser::CreationStatus::kOk ||
      !params.url.is_valid()) {
    RecordInitialNavigationCapturingResult(
        NavigationCapturingInitialResult::kNotHandled);
    return nullptr;
  }

  web_app::WebAppProvider* provider =
      web_app::WebAppProvider::GetForWebApps(profile);
  if (!provider) {
    RecordInitialNavigationCapturingResult(
        NavigationCapturingInitialResult::kNotHandled);
    return nullptr;
  }

  auto result = base::WrapUnique(new NavigationCapturingProcess(params));
  const bool should_handle_navigations_in_app =
      result->IsNavigationCapturingReimplExperimentEnabled() ||
      params.is_service_worker_open_window ||
      content::SiteIsolationPolicy::ShouldUrlUseApplicationIsolationLevel(
          profile, params.url);

  if (!should_handle_navigations_in_app) {
    // Don't record debug information for ALL navigations unless expensive
    // DCHECKs are enabled.
#if !EXPENSIVE_DCHECKS_ARE_ON()
    result->debug_data_.clear();
#endif
    return nullptr;
  }
  return result;
}

NavigationCapturingProcess::NavigationCapturingProcess(
    const NavigateParams& params)
    : profile_(raw_ref<Profile>::from_ptr(params.initiating_profile)),
      source_browser_app_id_(
          params.browser &&
                  web_app::AppBrowserController::IsWebApp(params.browser)
              ? std::optional(params.browser->GetBrowserForMigrationOnly()
                                  ->app_controller()
                                  ->app_id())
              : std::nullopt),
      source_tab_app_id_(params.source_contents
                             ? base::OptionalFromPtr(WebAppTabHelper::GetAppId(
                                   params.source_contents))
                             : std::nullopt),
      navigation_params_url_(params.url),
      disposition_(params.disposition),
      navigation_params_browser_(
          params.browser ? params.browser->GetBrowserForMigrationOnly()
                         : nullptr) {
  CHECK(AreWebAppsUserInstallable(&*profile_));
  CHECK(params.url.is_valid());

  WebAppProvider* provider = WebAppProvider::GetForWebApps(&*profile_);
  CHECK(provider);
  web_app::WebAppRegistrar& registrar = provider->registrar_unsafe();
  navigation_capturing_settings_ =
      NavigationCapturingSettings::Create(*profile_);

  debug_data_.Set("referrer.url", params.referrer.url.possibly_invalid_spec());
  debug_data_.Set("source_contents_url",
                  params.source_contents
                      ? params.source_contents->GetLastCommittedURL()
                            .possibly_invalid_spec()
                      : "<nullptr>");
  debug_data_.Set("navigation_params_opener", params.opener != nullptr);
  debug_data_.Set("navigation_params_contents_to_insert",
                  base::ToString(params.contents_to_insert.get()));
  debug_data_.Set("params.transition",
                  ui::PageTransitionGetCoreTransitionString(
                      ui::PageTransitionStripQualifier(params.transition)));
  debug_data_.Set(
      "params.transition.qualifiers",
      static_cast<int>(ui::PageTransitionGetQualifier(params.transition)));

  first_navigation_app_id_ =
      navigation_capturing_settings_->GetCapturingWebAppForUrl(params.url);

  if (first_navigation_app_id_) {
    CHECK(registrar.GetAppById(*first_navigation_app_id_));
    first_navigation_app_display_mode_ =
        registrar.GetAppEffectiveDisplayMode(*first_navigation_app_id_);
  }

  isolated_web_app_navigation_ =
      content::SiteIsolationPolicy::ShouldUrlUseApplicationIsolationLevel(
          &profile_.get(), params.url);
}

NavigationCapturingProcess::~NavigationCapturingProcess() {
  bool record = navigation_capturing_enabled_;
#if EXPENSIVE_DCHECKS_ARE_ON()
  record = true;
#endif

  RecordInitialNavigationCapturingResult(initial_nav_handling_result_);

  if (redirection_result_.has_value()) {
    base::UmaHistogramEnumeration(
        "Webapp.NavigationCapturing.Redirection.FinalResult",
        redirection_result_.value());
  }

  if (!debug_data_.empty() && record) {
    WebAppProvider* provider = WebAppProvider::GetForWebApps(&*profile_);
    provider->navigation_capturing_log().LogData(
        "NavigationCapturingProcess",
        base::Value(std::move(PopulateAndGetDebugData())),
        navigation_handle_id_);
  }
}

// static
void NavigationCapturingProcess::AttachToNavigationHandle(
    content::NavigationHandle& navigation_handle,
    std::unique_ptr<NavigationCapturingProcess> user_data) {
  if (!user_data->IsHandledByNavigationCapturing()) {
    return;
  }
  CHECK(!user_data->navigation_handle_);
  user_data->navigation_handle_ = &navigation_handle;
  CHECK(user_data->state_ == PipelineState::kInitialOverrideCalculated ||
        user_data->state_ == PipelineState::kAttachedToWebContents);
  user_data->state_ = PipelineState::kAttachedToNavigationHandle;
  user_data->OnAttachedToNavigationHandle();
  navigation_handle.SetUserData(UserDataKey(), std::move(user_data));
}

// static
void NavigationCapturingProcess::AttachToNextNavigationInWebContents(
    content::WebContents& web_contents,
    std::unique_ptr<NavigationCapturingProcess> user_data) {
  if (!user_data->IsHandledByNavigationCapturing()) {
    return;
  }
  CHECK_EQ(user_data->state_, PipelineState::kInitialOverrideCalculated);
  user_data->state_ = PipelineState::kAttachedToWebContents;
  // If there already is a user data with the same user data key attached to the
  // web contents, we want to overwrite that user data to make sure the newest
  // process gets attached to the next navigation. As such we don't check for
  // existing user data.
  GURL target_url = user_data->navigation_params_url_;
  web_contents.SetUserData(
      UserDataKey(),
      std::make_unique<
          NavigationHandleUserDataForwarder<NavigationCapturingProcess>>(
          web_contents, std::move(user_data), target_url));
}

std::optional<NavigationCapturingOverride>
NavigationCapturingProcess::GetInitialNavigationParamsOverride(
    const NavigateParams& params) {
  CHECK(AreWebAppsUserInstallable(&*profile_));
  CHECK(params.url.is_valid());
  CHECK_EQ(state_, PipelineState::kCreated);

  WebAppProvider* provider = WebAppProvider::GetForWebApps(&*profile_);
  CHECK(provider);
  web_app::WebAppRegistrar& registrar = provider->registrar_unsafe();

  // Only proceed as below if the navigation capturing is enabled. The flag in
  // the redirection info has to store the result of this check, so that the
  // logic in `OnWebAppNavigationAfterWebContentsCreation()` is skipped when not
  // needed.
  navigation_capturing_enabled_ =
      IsNavigationCapturingReimplExperimentEnabled();
  debug_data_.Set("is_service_worker_clients_open_window",
                  params.is_service_worker_open_window);

  if (isolated_web_app_navigation_) {
    return HandleIsolatedWebAppNavigation(params);
  }

  CHECK(!isolated_web_app_navigation_);
  // Handle service worker related navigations if any here.
  if (params.is_service_worker_open_window && !navigation_capturing_enabled_) {
    // See service_worker_client_utils::OpenWindow() for more details.
    CHECK(!params.browser);
    CHECK_EQ(disposition_, WindowOpenDisposition::NEW_FOREGROUND_TAB);
    CHECK(ui::PageTransitionCoreTypeIs(params.transition,
                                       ui::PAGE_TRANSITION_AUTO_TOPLEVEL));

    if (std::optional<webapps::AppId> app_id =
            web_app::FindInstalledAppWithUrlInScope(&profile_.get(), params.url,
                                                    /*window_only=*/true)) {
      Browser* host_window =
          CreateWebAppWindowFromNavigationParams(*app_id, params);
      return NoCapturingOverrideBrowser(host_window);
    }

    return CapturingDisabled();
  }

  // Below here handles the states outlined in
  // https://bit.ly/pwa-navigation-capturing
  if (!navigation_capturing_enabled_) {
    return CapturingDisabled();
  }
  if (params.started_from_context_menu ||
      params.pwa_navigation_capturing_force_off ||
      params.tabstrip_index != -1) {
    return CapturingDisabled();
  }
  if (!IsDispositionValidForNavigationCapturing(disposition_)) {
    return CapturingDisabled();
  }

  // The service worker clients API currently uses
  // PAGE_TRANSITION_AUTO_TOPLEVEL, which is normally considered invalid for
  // navigation capturing. Explicitly allow that.
  if (!params.is_service_worker_open_window &&
      !IsPageTransitionValidForNavigationCapturing(params.transition)) {
    return CapturingDisabled();
  }
  bool is_for_new_browser =
      params.browser &&
      params.browser->GetBrowserForMigrationOnly()
              ->tab_strip_model()
              ->count() == 0 &&
      (disposition_ == WindowOpenDisposition::NEW_FOREGROUND_TAB ||
       disposition_ == WindowOpenDisposition::NEW_BACKGROUND_TAB);
  if (is_for_new_browser) {
    // Some calls to `Navigate` populate a newly created browser in
    // `params.browser`, with no tabs. Callers can assume that browser is used,
    // so enabling capturing cause the user to see a browser with no tabs. We
    // cannot simply close it, as sometimes callers then use/reference that
    // browser. While that is likely a bug (callers should use the
    // `params.browser` to be compatible with other logic that changes the
    // browser), disable capturing in this case for now.
    return CapturingDisabled();
  }

  // Ensure that we proceed for a `first_navigation_app_display_mode_` to be
  // navigation captured only for the use-cases that are supported.
  if (first_navigation_app_display_mode_) {
    CHECK(WebAppRegistrar::IsSupportedDisplayModeForNavigationCapture(
        *first_navigation_app_display_mode_));
  }

  std::optional<ClientModeAndBrowser> client_mode_and_browser;
  if (first_navigation_app_id_) {
    client_mode_and_browser =
        GetEffectiveClientModeAndBrowser(*first_navigation_app_id_, params.url);
    debug_data_.Set(
        "first_navigation_app_effective_client_mode",
        base::ToString(client_mode_and_browser->effective_client_mode));
    CHECK(client_mode_and_browser->effective_client_mode !=
          LaunchHandler::ClientMode::kAuto);
    debug_data_.Set("first_navigation_app_host_browser",
                    client_mode_and_browser->browser
                        ? base::ToString(client_mode_and_browser->browser)
                        : "<none>");
    debug_data_.Set("first_navigation_app_host_tab",
                    client_mode_and_browser->tab_index.has_value()
                        ? base::ToString(*client_mode_and_browser->tab_index)
                        : "<none>");
  }

  // Case: Any click (user modified or non-modified) with auxiliary browsing
  // context. Only needs to be handled if it is triggered in the context of an
  // app browser.
  if (IsAuxiliaryBrowsingContext(params)) {
    debug_data_.Set("is_auxiliary_browsing_context", true);
    if (!navigation_capturing_settings_
             ->ShouldAuxiliaryContextsKeepSameContainer(source_browser_app_id_,
                                                        params.url)) {
      return CapturingDisabled();
    }
    if (source_browser_app_id_.has_value()) {
      Browser* app_window = CreateWebAppWindowFromNavigationParams(
          *source_browser_app_id_, params);

      return AuxiliaryContextInAppWindow(app_window);
    }
    return AuxiliaryContext();
  }
  debug_data_.Set("is_auxiliary_browsing_context", false);

  // If no app controls the the target url, then there cannot be any further
  // capturing logic unless a redirect occurs.
  if (!first_navigation_app_id_) {
    return NoInitialActionRedirectionHandlingEligible();
  }

  CHECK(first_navigation_app_display_mode_);
  CHECK(client_mode_and_browser);
  const webapps::AppId& app_id = *first_navigation_app_id_;
  blink::mojom::DisplayMode app_display_mode =
      *first_navigation_app_display_mode_;
  LaunchHandler::ClientMode client_mode =
      client_mode_and_browser->effective_client_mode;

  // Case: User-modified clicks.
  if (is_user_modified_click()) {
    // The default behavior is only modified if the source is an app browser or
    // the controlling app's display mode is 'browser'.
    if (!source_browser_app_id_.has_value() &&
        app_display_mode != DisplayMode::kBrowser) {
      return NoInitialActionRedirectionHandlingEligible();
    }
    // Case: Shift-clicks with a new top level browsing context.
    if (disposition_ == WindowOpenDisposition::NEW_WINDOW) {
      Browser* app_host_window;
      if (app_display_mode == DisplayMode::kBrowser) {
        app_host_window = Browser::Create(
            Browser::CreateParams(&*profile_, params.user_gesture));
      } else {
        app_host_window =
            CreateWebAppWindowFromNavigationParams(app_id, params);
      }
      return ForcedNewAppContext(app_display_mode, app_host_window);
    }

    bool is_in_source_app_with_url_in_scope =
        source_browser_app_id_
            ? registrar.IsUrlInAppScope(params.url, *source_browser_app_id_)
            : false;

    // Case: Middle clicks with a new top level browsing context.
    if (disposition_ == WindowOpenDisposition::NEW_BACKGROUND_TAB &&
        (app_display_mode == DisplayMode::kBrowser ||
         (app_id == source_browser_app_id_ &&
          is_in_source_app_with_url_in_scope))) {
      if (source_browser_app_id_.has_value() &&
          !params.browser->GetBrowserForMigrationOnly()
               ->app_controller()
               ->ShouldHideNewTabButton()) {
        // Apps that support tabbed mode can open a new tab in the current app
        // browser itself.
        return ForcedNewAppContext(
            app_display_mode, params.browser->GetBrowserForMigrationOnly());
      }
      Browser* app_host_window;
      if (app_display_mode == DisplayMode::kBrowser) {
        // For a 'new tab' with the 'browser' requested display mode, prefer
        // using an existing browser window.
        app_host_window = client_mode_and_browser->browser
                              ? client_mode_and_browser->browser.get()
                              : Browser::Create(Browser::CreateParams(
                                    &*profile_, params.user_gesture));
      } else {
        app_host_window =
            CreateWebAppWindowFromNavigationParams(app_id, params);
      }
      return ForcedNewAppContext(app_display_mode, app_host_window);
    }
    return NoInitialActionRedirectionHandlingEligible();
  }

  if (disposition_ != WindowOpenDisposition::NEW_FOREGROUND_TAB) {
    return CapturingDisabled();
  }

  // Case: Left click, non-user-modified. Capturable.

  // Opening in non-browser-tab requires OS integration. Since os integration
  // cannot be triggered synchronously, treat this as opening in browser.
  if (registrar.GetInstallState(app_id) ==
      proto::INSTALLED_WITHOUT_OS_INTEGRATION) {
    app_display_mode = blink::mojom::DisplayMode::kBrowser;
  }

  // Prevent-close requires only focusing the existing tab, and never
  // navigating.
  if (registrar.IsPreventCloseEnabled(app_id) &&
      !registrar.IsTabbedWindowModeEnabled(app_id)) {
    client_mode = LaunchHandler::ClientMode::kFocusExisting;
  }
  debug_data_.Set("client_mode", base::ToString(client_mode));

  // Focus existing.
  if (client_mode == LaunchHandler::ClientMode::kFocusExisting) {
    CHECK(client_mode_and_browser->browser);
    CHECK(client_mode_and_browser->tab_index.has_value());
    return CapturedFocusExisting(client_mode_and_browser->browser,
                                 *client_mode_and_browser->tab_index,
                                 params.url);
  }

  // Navigate existing.
  if (client_mode == LaunchHandler::ClientMode::kNavigateExisting) {
    CHECK(client_mode_and_browser->browser);
    CHECK(client_mode_and_browser->tab_index.has_value());
    return CapturedNavigateExisting(client_mode_and_browser->browser,
                                    *client_mode_and_browser->tab_index);
  }

  // Navigate new.
  CHECK(client_mode == LaunchHandler::ClientMode::kNavigateNew);

  Browser* host_window = nullptr;
  switch (app_display_mode) {
    case blink::mojom::DisplayMode::kBrowser:
      if (client_mode_and_browser->browser) {
        host_window = client_mode_and_browser->browser;
      } else {
        host_window = Browser::Create(
            Browser::CreateParams(&*profile_, params.user_gesture));
      }
      break;
    case blink::mojom::DisplayMode::kMinimalUi:
    case blink::mojom::DisplayMode::kStandalone:
    case blink::mojom::DisplayMode::kWindowControlsOverlay:
    case blink::mojom::DisplayMode::kBorderless:
      host_window = CreateWebAppWindowFromNavigationParams(app_id, params);
      break;
    case blink::mojom::DisplayMode::kTabbed:
      if (client_mode_and_browser->browser) {
        host_window = client_mode_and_browser->browser;
      } else {
        host_window = CreateWebAppWindowFromNavigationParams(app_id, params);
      }
      CHECK(host_window->app_controller()->has_tab_strip());
      if (host_window->app_controller()->IsUrlInHomeTabScope(params.url)) {
        return CapturedNavigateExisting(host_window, 0);
      }
      break;
    case blink::mojom::DisplayMode::kUndefined:
    case blink::mojom::DisplayMode::kPictureInPicture:
    case blink::mojom::DisplayMode::kFullscreen:
      NOTREACHED();
  }

  return CapturedNewClient(app_display_mode, host_window);
}

std::optional<NavigationCapturingOverride>
NavigationCapturingProcess::HandleIsolatedWebAppNavigation(
    const NavigateParams& params) {
  CHECK(isolated_web_app_navigation_);
  if (!first_navigation_app_id_) {
    return CancelInitialNavigation(
        NavigationCapturingInitialResult::kNavigationCanceled);
  }

  const webapps::AppId& iwa_id = *first_navigation_app_id_;
  const DisplayMode& app_display_mode = *first_navigation_app_display_mode_;

  CHECK(app_display_mode == DisplayMode::kStandalone ||
        app_display_mode == DisplayMode::kBorderless);

  // Prefer `params.browser` if it's a compatible IWA browser.
  bool iwa_browser =
      params.browser &&
      web_app::AppBrowserController::IsForWebApp(params.browser, iwa_id);

  bool capturing_disabled = [&]() {
    switch (disposition_) {
      case WindowOpenDisposition::NEW_POPUP:
      case WindowOpenDisposition::NEW_PICTURE_IN_PICTURE:
        // App popups and picture-in-picture are handled in the switch statement
        // in `GetBrowserAndTabForDisposition()`.
        return true;
      case WindowOpenDisposition::NEW_FOREGROUND_TAB:
      case WindowOpenDisposition::NEW_BACKGROUND_TAB:
        // If the browser window does not yet have any tabs, and we are
        // attempting to add the first tab to it, allow for it to be reused.
        return iwa_browser && params.browser->GetBrowserForMigrationOnly()
                                  ->tab_strip_model()
                                  ->empty();
      case WindowOpenDisposition::CURRENT_TAB:
        return iwa_browser;
      case WindowOpenDisposition::NEW_WINDOW:
        return false;
      case WindowOpenDisposition::UNKNOWN:
      case WindowOpenDisposition::SINGLETON_TAB:
      case WindowOpenDisposition::SAVE_TO_DISK:
      case WindowOpenDisposition::OFF_THE_RECORD:
      case WindowOpenDisposition::IGNORE_ACTION:
      case WindowOpenDisposition::SWITCH_TO_TAB:
        // These are not supposed to be reachable with IWA URLs.
        return true;
    }
  }();

  if (capturing_disabled) {
    return CapturingDisabled();
  }

  if (ui::PageTransitionCoreTypeIs(params.transition,
                                   ui::PAGE_TRANSITION_LINK)) {
    // Any links: same-IWA or cross-IWA window.open(), same-IWA or cross-IWA
    // anchor link, cross-IWA meta tag redirect.
    if (source_browser_app_id_ != iwa_id) {
      // TODO(crbug.com/424422466): Support cross-IWA navigations to start_url.
      return CancelInitialNavigation(
          NavigationCapturingInitialResult::kNavigationCanceled);
    }

    if (IsAuxiliaryBrowsingContext(params)) {
      debug_data_.Set("is_auxiliary_browsing_context", true);
      Browser* aux_window =
          CreateWebAppWindowFromNavigationParams(iwa_id, params);
      return AuxiliaryContextInAppWindow(aux_window);
    }
  }

  // Auxiliary browsing contexts should only be openable via link transitions.
  CHECK(!IsAuxiliaryBrowsingContext(params));
  debug_data_.Set("is_auxiliary_browsing_context", false);

  // As per https://bit.ly/pwa-navigation-capturing, user modified clicks (AKA
  // the intention to open the link in a new browsing context) should be
  // respected over the launch_handler configuration.
  if (is_user_modified_click()) {
    return ForcedNewAppContext(
        app_display_mode,
        CreateWebAppWindowFromNavigationParams(iwa_id, params));
  }

  ClientModeAndBrowser client_mode_and_browser =
      GetEffectiveClientModeAndBrowser(iwa_id, params.url);
  LaunchHandler::ClientMode client_mode =
      client_mode_and_browser.effective_client_mode;

  switch (client_mode) {
    case LaunchHandler::ClientMode::kAuto:
      NOTREACHED();
    case LaunchHandler::ClientMode::kFocusExisting:
    case LaunchHandler::ClientMode::kNavigateExisting: {
      // TODO(crbug.com/424173119): support navigate-existing for link
      // transitions / service worker windows / omnibox in IWAs.
      debug_data_.Set(
          "client_mode",
          base::ToString(LaunchHandler::ClientMode::kFocusExisting));
      return CapturedFocusExisting(client_mode_and_browser.browser,
                                   *client_mode_and_browser.tab_index,
                                   params.url);
    }
    case LaunchHandler::ClientMode::kNavigateNew: {
      debug_data_.Set("client_mode",
                      base::ToString(LaunchHandler::ClientMode::kNavigateNew));
      Browser* host_window =
          CreateWebAppWindowFromNavigationParams(iwa_id, params);
      return CapturedNewClient(app_display_mode, host_window);
    }
  }
}

void NavigationCapturingProcess::MaybeNotifyIwaTabCounterService(
    content::WebContents& web_contents,
    content::NavigationHandle* navigation_handle) {
  Profile* const profile =
      Profile::FromBrowserContext(web_contents.GetBrowserContext());
  if (!profile) {
    return;
  }

  WebAppProvider* const provider = WebAppProvider::GetForWebApps(profile);
  if (!provider) {
    return;
  }

  std::optional<webapps::AppId> iwa_opener_app_id;

  // Try to find the App ID from the opener chain.
  content::WebContents* opener =
      web_contents.GetFirstWebContentsInLiveOriginalOpenerChain();
  const webapps::AppId* app_id =
      opener ? WebAppTabHelper::GetAppId(opener) : nullptr;

  if (app_id && provider->registrar_unsafe().AppMatches(
                    *app_id, WebAppFilter::IsIsolatedApp())) {
    iwa_opener_app_id = *app_id;
  }

  // Fallback to the initiator origin.
  if (!iwa_opener_app_id) {
    if (!navigation_handle) {
      return;
    }

    const auto& initiator_origin = navigation_handle->GetInitiatorOrigin();
    if (!initiator_origin) {
      return;
    }

    iwa_opener_app_id = provider->registrar_unsafe().FindBestAppWithUrlInScope(
        initiator_origin->GetURL(), WebAppFilter::IsIsolatedApp());
  }

  // If the "iwa_opener_app_id" is still not found, then there is a chance that
  // the popup was not opened by the IWA. Exist early if that is the case.
  if (!iwa_opener_app_id) {
    return;
  }

  auto* counter_service =
      IsolatedWebAppsOpenedTabsCounterServiceFactory::GetForProfile(profile);
  if (!counter_service) {
    return;
  }

  counter_service->OnWebContentsCreated(*iwa_opener_app_id, &web_contents,
                                        provider->clock().Now());
}

// static
void NavigationCapturingProcess::AfterWebContentsCreation(
    std::unique_ptr<NavigationCapturingProcess> process,
    content::WebContents& web_contents,
    content::NavigationHandle* navigation_handle) {
  process->MaybeNotifyIwaTabCounterService(web_contents, navigation_handle);

  if (navigation_handle) {
    AttachToNavigationHandle(*navigation_handle, std::move(process));
  } else {
    AttachToNextNavigationInWebContents(web_contents, std::move(process));
  }
}

NavigationCapturingProcess::ThrottleCheckResult
NavigationCapturingProcess::HandleRedirect() {
  CHECK(navigation_handle());
  CHECK_EQ(state_, PipelineState::kAttachedToNavigationHandle);
  navigation_handle_id_ = navigation_handle()->GetNavigationId();
  state_ = PipelineState::kFinished;

  // See https://bit.ly/pwa-navigation-capturing and
  // https://bit.ly/pwa-navigation-handling-dd for more context.
  // Exit early if:
  // 1. If there were no redirects, then the only url in the redirect chain
  // should be the last url to go to.
  // 2. This is not a server side redirect.
  // 3. The navigation was started from the context menu.
  if (navigation_handle()->GetRedirectChain().size() == 1 ||
      !navigation_handle()->WasServerRedirect() ||
      (navigation_handle()->WasStartedFromContextMenu())) {
    debug_data_.Set("!redirection_result", "ineligible");
    redirection_result_ = NavigationCapturingRedirectionResult::kNotHandled;
    return content::NavigationThrottle::PROCEED;
  }
  CHECK(!isolated_web_app_navigation_)
      << "Isolated Web Apps do not support redirects.";
  const GURL& final_url = navigation_handle()->GetURL();
  debug_data_.Set("!redirection_final_url", final_url.possibly_invalid_spec());
  if (!final_url.is_valid()) {
    redirection_result_ = NavigationCapturingRedirectionResult::kNotHandled;
    return content::NavigationThrottle::PROCEED;
  }

  // Do not handle redirections for navigations that create an auxiliary
  // browsing context, or if the app window that opened is not a part of the
  // navigation handling flow.
  if (initial_nav_handling_result_ ==
          NavigationCapturingInitialResult::kNotHandled ||
      initial_nav_handling_result_ ==
          NavigationCapturingInitialResult::kAuxiliaryContextAppBrowserTab ||
      initial_nav_handling_result_ ==
          NavigationCapturingInitialResult::kAuxiliaryContextAppWindow) {
    redirection_result_ = NavigationCapturingRedirectionResult::kNotHandled;
    return content::NavigationThrottle::PROCEED;
  }

  content::WebContents* const web_contents_for_navigation =
      navigation_handle()->GetWebContents();

  WebAppProvider* provider =
      WebAppProvider::GetForWebContents(web_contents_for_navigation);
  WebAppRegistrar& registrar = provider->registrar_unsafe();
  std::optional<webapps::AppId> target_app_id =
      navigation_capturing_settings_->GetCapturingWebAppForUrl(final_url);

  // "Same first navigation state" case:
  // First, we can exit early if the first navigation app id matches the target
  // app id (which includes if they are both std::nullopt), as this means we
  // already did the 'correct' navigation capturing behavior on the first
  // navigation.
  if (first_navigation_app_id_ == target_app_id) {
    debug_data_.Set("!redirection_result", "Same app.");
    redirection_result_ = NavigationCapturingRedirectionResult::kSameContext;
    return content::NavigationThrottle::PROCEED;
  }

  // Clear out the "launch app id" field. This way we ensure that in any branch
  // where the redirect does not result in an app being launched we don't
  // accidentally (try to) treat it as a launch. Any branch where an app launch
  // does happen will re-set the field to the correct value.
  SetLaunchedAppId(std::nullopt);

  // After this point:
  // - The browsing context is a top-level browsing context.
  // - The initial navigation capturing app_id does not match the final
  //   target_app_id (and either can be std::nullopt, but not both).
  // - Navigation is only triggered as part of left, middle or shift clicks.

  bool is_source_app_matching_final_target =
      target_app_id == source_browser_app_id_;

  // First, handle cases where the final url is not in scope of any app. These
  // can mostly proceed as is, except for two cases where the initial navigation
  // ended up in an app window but should now be in a browser tab.
  if (!target_app_id.has_value()) {
    if (initial_nav_handling_result_ ==
            NavigationCapturingInitialResult::kForcedContextAppWindow ||
        initial_nav_handling_result_ ==
            NavigationCapturingInitialResult::kNewAppWindow) {
      debug_data_.Set("!redirection_result", "Reparent, btab");
      ReparentWebContentsToTabbedBrowser(web_contents_for_navigation,
                                         disposition_,
                                         navigation_params_browser_);
      redirection_result_ =
          NavigationCapturingRedirectionResult::kReparentBrowserTabToBrowserTab;
    } else {
      debug_data_.Set("!redirection_result", "Noop1");
      redirection_result_ = NavigationCapturingRedirectionResult::kSameContext;
    }
    return content::NavigationThrottle::PROCEED;
  }

  CHECK(registrar.GetAppById(*target_app_id));
  blink::mojom::DisplayMode target_display_mode =
      registrar.GetAppEffectiveDisplayMode(*target_app_id);
  CHECK(WebAppRegistrar::IsSupportedDisplayModeForNavigationCapture(
      target_display_mode));

  // For the remaining cases we know that the navigation ended up in scope of a
  // target application.

  // First, handle the case where a new app container (app or browser) was
  // force-created for an app for user modified clicks. This refers to the
  // use-cases here:
  // https://bit.ly/pwa-navigation-capturing?tab=t.0#bookmark=id.ugh0e993wsl8,
  // where a new app container is made.
  if (initial_nav_handling_result_ ==
      NavigationCapturingInitialResult::kForcedContextAppWindow) {
    CHECK(source_browser_app_id_.has_value());
    CHECK(first_navigation_app_id_.has_value());
    // standalone-app -> browser-tab-app.
    if (target_display_mode == blink::mojom::DisplayMode::kBrowser) {
      debug_data_.Set("!redirection_result", "app to btab");
      SetLaunchedAppId(*target_app_id, /*force_iph_off=*/true);
      ReparentWebContentsToTabbedBrowser(web_contents_for_navigation,
                                         disposition_,
                                         navigation_params_browser_);
      redirection_result_ =
          NavigationCapturingRedirectionResult::kReparentAppToBrowserTab;
      return content::NavigationThrottle::PROCEED;
    }
    debug_data_.Set("!redirection_result", "app to app");
    // standalone-app -> standalone-app.
    SetLaunchedAppId(*target_app_id);
    CHECK(target_display_mode != blink::mojom::DisplayMode::kBrowser);
    ReparentToAppBrowser(web_contents_for_navigation, *target_app_id,
                         target_display_mode, final_url);
    redirection_result_ =
        NavigationCapturingRedirectionResult::kReparentAppToApp;
    return content::NavigationThrottle::PROCEED;
  }
  if (initial_nav_handling_result_ ==
      NavigationCapturingInitialResult::kForcedContextAppBrowserTab) {
    // browser-tab-app -> browser-tab-app.
    if (target_display_mode == blink::mojom::DisplayMode::kBrowser) {
      debug_data_.Set("!redirection_result", "N/A, btab");
      redirection_result_ = NavigationCapturingRedirectionResult::kSameContext;
      return content::NavigationThrottle::PROCEED;
    }
    // browser-tab-app -> standalone-app. This must have a source app id to
    // ensure that we cannot have a user-modified click go from a regular
    // browser tab to an app window.
    CHECK(target_display_mode != blink::mojom::DisplayMode::kBrowser);
    if (source_browser_app_id_.has_value()) {
      SetLaunchedAppId(*target_app_id);
      debug_data_.Set("!redirection_result", "btab to app");
      ReparentToAppBrowser(web_contents_for_navigation, *target_app_id,
                           target_display_mode, final_url);
      redirection_result_ =
          NavigationCapturingRedirectionResult::kReparentBrowserTabToApp;
      return content::NavigationThrottle::PROCEED;
    }
  }

  // Handle the last user-modified correction, where a user-modified click from
  // an app went to the browser, but needs to be reparented back into an app.
  // See
  // https://bit.ly/pwa-navigation-capturing?tab=t.0#bookmark=id.ugh0e993wsl8
  // for more information.
  if (initial_nav_handling_result_ ==
          NavigationCapturingInitialResult::kNewTabRedirectionEligible &&
      is_user_modified_click() && source_browser_app_id_.has_value()) {
    // As per the UX direction in the doc, NEW_BACKGROUND_TAB only creates a
    // new app window for an app when coming from the same app window.
    // Otherwise, only NEW_WINDOW can create a new app window when coming from
    // an app.
    if ((disposition_ == WindowOpenDisposition::NEW_BACKGROUND_TAB &&
         is_source_app_matching_final_target) ||
        (disposition_ == WindowOpenDisposition::NEW_WINDOW)) {
      // browser-tab -> browser-tab-app.
      if (target_display_mode == blink::mojom::DisplayMode::kBrowser) {
        SetLaunchedAppId(*target_app_id, /*force_iph_off=*/true);
        debug_data_.Set("!redirection_result", "N/A, btab");
        redirection_result_ =
            NavigationCapturingRedirectionResult::kSameContext;
        return content::NavigationThrottle::PROCEED;
      }
      // browser-tab -> standalone app
      SetLaunchedAppId(*target_app_id);
      debug_data_.Set("!redirection_result", "btab to app");
      ReparentToAppBrowser(web_contents_for_navigation, *target_app_id,
                           target_display_mode, final_url);
      redirection_result_ =
          NavigationCapturingRedirectionResult::kReparentBrowserTabToApp;
      return content::NavigationThrottle::PROCEED;
    }
  }

  // All other user-modified cases should do the default thing and navigate the
  // existing container.
  if (is_user_modified_click()) {
    debug_data_.Set("!redirection_result", "N/A");
    redirection_result_ = NavigationCapturingRedirectionResult::kSameContext;
    return content::NavigationThrottle::PROCEED;
  }

  ClientModeAndBrowser client_mode_and_browser =
      GetEffectiveClientModeAndBrowser(*target_app_id, final_url);

  // After this point:
  // - The navigation is non-user-modified.
  // - This is a top-level browsing context.
  // - The first navigation app_id doesn't match the target app_id (as per "Same
  //   first navigation state" case above).

  // Handle all cases where the initial navigation was captured, and that now
  // needs to be corrected. See the table at
  // bit.ly/pwa-navigation-handling-dd?tab=t.0#bookmark=id.hnvzj4iwiviz

  // First, address the 'navigate-new' or 'browser' initial capture, where the
  // final state is also an effective 'navigate-new'.
  if (client_mode_and_browser.effective_client_mode ==
          LaunchHandler::ClientMode::kNavigateNew &&
      (initial_nav_handling_result_ ==
           NavigationCapturingInitialResult::kNewTabRedirectionEligible ||
       initial_nav_handling_result_ ==
           NavigationCapturingInitialResult::kNewAppWindow ||
       initial_nav_handling_result_ ==
           NavigationCapturingInitialResult::kNewAppBrowserTab)) {
    // Handle all cases that result in a standalone app.
    // (browser tab, browser-tab-app, or standalone-app -> standalone-app)
    if (target_display_mode != blink::mojom::DisplayMode::kBrowser) {
      SetLaunchedAppId(*target_app_id);
      debug_data_.Set("!redirection_result", "app");
      ReparentToAppBrowser(web_contents_for_navigation, *target_app_id,
                           target_display_mode, final_url);
      redirection_result_ =
          NavigationCapturingRedirectionResult::kAppWindowOpened;
      return content::NavigationThrottle::PROCEED;
    }
    // Handle all cases that result in a browser-tab-app.
    // (browser tab, browser-tab-app, or standalone-app -> browser-tab-app)
    CHECK(target_display_mode == blink::mojom::DisplayMode::kBrowser);
    SetLaunchedAppId(*target_app_id, /*force_iph_off=*/true);
    if (initial_nav_handling_result_ ==
        NavigationCapturingInitialResult::kNewAppWindow) {
      debug_data_.Set("!redirection_result", "btab");
      ReparentWebContentsToTabbedBrowser(web_contents_for_navigation,
                                         disposition_,
                                         navigation_params_browser_);
      redirection_result_ =
          NavigationCapturingRedirectionResult::kAppBrowserTabOpened;
    }
    return content::NavigationThrottle::PROCEED;
  }

  // Only proceed from now on if the final app can be capturable depending on
  // the result of the initial navigation handling. This involves only 2
  // use-cases, where the intermediary result is either a browser tab, or an app
  // window that opened as a result of a capturable navigation.
  bool final_navigation_can_be_capturable =
      InitialResultWasCaptured() ||
      initial_nav_handling_result_ ==
          NavigationCapturingInitialResult::kNewTabRedirectionEligible;
  if (!final_navigation_can_be_capturable) {
    debug_data_.Set("!redirection_result", "N/A, not capturable");
    redirection_result_ = NavigationCapturingRedirectionResult::kNotCapturable;
    return content::NavigationThrottle::PROCEED;
  }

  // Handle the use-case where the target_app_id has a launch handling mode of
  // kFocusExisting or kNavigateExisting. In both cases this navigation gets
  // aborted, and in some cases the web contents being navigated gets closed.
  if (client_mode_and_browser.effective_client_mode ==
          LaunchHandler::ClientMode::kFocusExisting ||
      client_mode_and_browser.effective_client_mode ==
          LaunchHandler::ClientMode::kNavigateExisting) {
    CHECK(client_mode_and_browser.browser);
    CHECK(client_mode_and_browser.tab_index.has_value());
    CHECK_NE(*client_mode_and_browser.tab_index, -1);

    FocusAppContainer(client_mode_and_browser.browser,
                      *client_mode_and_browser.tab_index);

    content::WebContents* pre_existing_contents =
        client_mode_and_browser.browser->tab_strip_model()->GetWebContentsAt(
            *client_mode_and_browser.tab_index);
    CHECK(pre_existing_contents);
    CHECK_NE(pre_existing_contents, web_contents_for_navigation);

    bool is_web_app_browser =
        WebAppBrowserController::IsWebApp(client_mode_and_browser.browser);

    if (client_mode_and_browser.effective_client_mode ==
        LaunchHandler::ClientMode::kNavigateExisting) {
      content::OpenURLParams params =
          content::OpenURLParams::FromNavigationHandle(navigation_handle());

      // Reset the frame_tree_node_id to make sure we're navigating the main
      // frame in the target web contents.
      params.frame_tree_node_id = {};

      pre_existing_contents->OpenURL(
          params,
          base::BindOnce(
              [](const webapps::AppId& target_app_id,
                 base::TimeTicks time_navigation_started,
                 content::NavigationHandle& navigation_handle) {
                WebAppLaunchNavigationHandleUserData::CreateForNavigationHandle(
                    navigation_handle, target_app_id, /*force_iph_off=*/false,
                    time_navigation_started);
              },
              *target_app_id, time_navigation_started_));
      debug_data_.Set("!redirection_result", "cancel, navigate-existing");
      redirection_result_ =
          is_web_app_browser
              ? NavigationCapturingRedirectionResult::kNavigateExistingAppWindow
              : NavigationCapturingRedirectionResult::
                    kNavigateExistingAppBrowserTab;
    } else {
      // Perform post navigation operations, like recording app launch metrics,
      // or showing the navigation capturing IPH.
      CHECK(!time_navigation_started_.is_null());
      EnqueueLaunchParams(pre_existing_contents, *target_app_id, final_url,
                          /*wait_for_navigation_to_complete=*/false,
                          time_navigation_started_);
      MaybeShowNavigationCaptureIph(*target_app_id, &*profile_,
                                    client_mode_and_browser.browser);
      RecordLaunchMetrics(*target_app_id,
                          apps::LaunchContainer::kLaunchContainerWindow,
                          apps::LaunchSource::kFromNavigationCapturing,
                          final_url, pre_existing_contents);
      RecordNavigationCapturingDisplayModeMetrics(
          *target_app_id, pre_existing_contents, !is_web_app_browser);
      debug_data_.Set("!redirection_result", "cancel, focus-existing");
      redirection_result_ =
          is_web_app_browser
              ? NavigationCapturingRedirectionResult::kFocusExistingAppWindow
              : NavigationCapturingRedirectionResult::
                    kFocusExistingAppBrowserTab;
    }

    // Close the old tab or app window, if it was created as part of the current
    // navigation to mimic the behavior where the redirected url matches an
    // outcome without redirection. Any residual app windows or tabs that were
    // there before the current navigation started shouldn't be closed.
    if (initial_nav_handling_result_ ==
            NavigationCapturingInitialResult::kNewAppWindow ||
        initial_nav_handling_result_ ==
            NavigationCapturingInitialResult::kNewAppBrowserTab ||
        initial_nav_handling_result_ ==
            NavigationCapturingInitialResult::kNewTabRedirectionEligible) {
      debug_data_.Set("redirection_closed_page", true);
      web_contents_for_navigation->ClosePage();
    }
    return content::NavigationThrottle::CANCEL;
  }

  debug_data_.Set("!redirection_result", "Noop2");
  redirection_result_ = NavigationCapturingRedirectionResult::kSameContext;
  return content::NavigationThrottle::PROCEED;
}

void NavigationCapturingProcess::OnAttachedToNavigationHandle() {
  CHECK(navigation_handle());
  CHECK(IsHandledByNavigationCapturing());
  if (!launched_app_id_) {
    return;
  }

  web_app::WebAppLaunchNavigationHandleUserData::CreateForNavigationHandle(
      *navigation_handle(), *launched_app_id_,
      /*force_iph_off=*/force_iph_off_ || isolated_web_app_navigation_,
      time_navigation_started_);
}

bool NavigationCapturingProcess::
    IsNavigationCapturingReimplExperimentEnabled() {
  if (first_navigation_app_display_mode_ &&
      !WebAppRegistrar::IsSupportedDisplayModeForNavigationCapture(
          *first_navigation_app_display_mode_)) {
    return false;
  }
  // Enabling the generic flag turns it on for all navigations.
  if (apps::features::IsNavigationCapturingReimplEnabled()) {
    if (!features::kForcedOffCapturingAppsOnFirstNavigation.Get().empty() &&
        first_navigation_app_id_.has_value()) {
      std::vector<std::string> forced_capturing_off_app_ids = base::SplitString(
          features::kForcedOffCapturingAppsOnFirstNavigation.Get(), ",",
          base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
      for (const std::string& forced_capturing_off_app_id :
           forced_capturing_off_app_ids) {
        if (first_navigation_app_id_ == forced_capturing_off_app_id) {
          return false;
        }
      }
    }
    return true;
  }

#if BUILDFLAG(IS_CHROMEOS)
  // Check application-specific flags.
  if (first_navigation_app_id_.has_value() &&
      ::web_app::ChromeOsWebAppExperiments::
          IsNavigationCapturingReimplEnabledForTargetApp(
              *first_navigation_app_id_)) {
    return true;
  }
  if (source_browser_app_id_.has_value() &&
      ::web_app::ChromeOsWebAppExperiments::
          IsNavigationCapturingReimplEnabledForSourceApp(
              *source_browser_app_id_, navigation_params_url_)) {
    return true;
  }
#endif

  return false;
}

NavigationCapturingProcess::ClientModeAndBrowser
NavigationCapturingProcess::GetEffectiveClientModeAndBrowser(
    const webapps::AppId& app_id,
    const GURL& target_url) {
  WebAppProvider* provider = WebAppProvider::GetForWebApps(&*profile_);
  CHECK(provider);
  web_app::WebAppRegistrar& registrar = provider->registrar_unsafe();

  ClientModeAndBrowser result;
  result.effective_client_mode = registrar.GetAppById(app_id)
                                     ->launch_handler()
                                     .value_or(LaunchHandler())
                                     .parsed_client_mode();
  if (result.effective_client_mode == LaunchHandler::ClientMode::kAuto) {
    result.effective_client_mode = LaunchHandler::ClientMode::kNavigateNew;
  }

  blink::mojom::DisplayMode requested_display_mode =
      registrar.GetAppEffectiveDisplayMode(app_id);

  // Opening in non-browser-tab requires OS integration. Since os integration
  // cannot be triggered synchronously, treat this as opening in browser.
  if (registrar.GetInstallState(app_id) ==
      proto::INSTALLED_WITHOUT_OS_INTEGRATION) {
    requested_display_mode = blink::mojom::DisplayMode::kBrowser;
  }

  // If the developer has an in-scope link that opens a new top level browsing
  // in their own page, then force the client mode to be navigate-new. To
  // detect this, we consider both the app_id that controls the referrer url
  // as well as the app id of the tab that initiated the navigation.
  if (source_tab_app_id_ == app_id) {
    result.effective_client_mode = LaunchHandler::ClientMode::kNavigateNew;
  }

  if (result.effective_client_mode ==
          LaunchHandler::ClientMode::kNavigateExisting ||
      result.effective_client_mode ==
          LaunchHandler::ClientMode::kFocusExisting) {
    bool for_focus_existing = result.effective_client_mode ==
                              LaunchHandler::ClientMode::kFocusExisting;
    // For navigate and focus existing find an existing tab for this app,
    // depending on the display mode requested.
    std::optional<AppBrowserController::BrowserAndTabIndex> existing_app_host;
    switch (requested_display_mode) {
      case blink::mojom::DisplayMode::kUndefined:
      case blink::mojom::DisplayMode::kBrowser:
        // If a populated browser exists with an applicable tab for
        // focus-existing or navigate-existing, use that instead of a another
        // existing browser. Note: This could mean that the provided browser is
        // not a 'normal' and could be an app browser. It seems fine to
        // respect this.
        if (navigation_params_browser_) {
          std::optional<int> tab_index =
              AppBrowserController::FindTabIndexForApp(
                  navigation_params_browser_, app_id, for_focus_existing);
          if (tab_index.has_value()) {
            existing_app_host = {.browser = navigation_params_browser_,
                                 .tab_index = *tab_index};
            break;
          }
        }
        existing_app_host =
            AppBrowserController::FindTopLevelBrowsingContextForWebApp(
                *profile_, app_id, /*for_app_browser=*/false,
                for_focus_existing);
        break;
      case blink::mojom::DisplayMode::kMinimalUi:
      case blink::mojom::DisplayMode::kStandalone:
      case blink::mojom::DisplayMode::kWindowControlsOverlay:
      case blink::mojom::DisplayMode::kBorderless:
      case blink::mojom::DisplayMode::kTabbed: {
        // First try to choose an existing app host based on whether the
        // params.browser is populated and belongs to the same `app_id`.
        // If that is not found, start looking into all active app browsers.
        if (navigation_params_browser_ &&
            WebAppBrowserController::IsForWebApp(navigation_params_browser_,
                                                 app_id)) {
          std::optional<int> tab_index =
              AppBrowserController::FindTabIndexForApp(
                  navigation_params_browser_, app_id, for_focus_existing);
          if (tab_index.has_value()) {
            existing_app_host = {.browser = navigation_params_browser_,
                                 .tab_index = *tab_index};
            break;
          }
        }

        using HomeTabScope = AppBrowserController::HomeTabScope;
        HomeTabScope home_tab_scope = HomeTabScope::kDontCare;
        if (requested_display_mode == blink::mojom::DisplayMode::kTabbed &&
            result.effective_client_mode ==
                LaunchHandler::ClientMode::kNavigateExisting) {
          home_tab_scope = registrar.IsUrlInHomeTabScope(target_url, app_id)
                               ? HomeTabScope::kInScope
                               : HomeTabScope::kOutOfScope;
        }
        existing_app_host =
            AppBrowserController::FindTopLevelBrowsingContextForWebApp(
                *profile_, app_id, /*for_app_browser=*/true, for_focus_existing,
                home_tab_scope);
        // If no app tab was found, fall back to looking for a regular browser
        // tab.
        if (!existing_app_host) {
          existing_app_host =
              AppBrowserController::FindTopLevelBrowsingContextForWebApp(
                  *profile_, app_id, /*for_app_browser=*/false,
                  for_focus_existing);
        }
        break;
      }
      case blink::mojom::DisplayMode::kFullscreen:
      case blink::mojom::DisplayMode::kPictureInPicture:
        NOTREACHED();
    }

    if (existing_app_host.has_value()) {
      CHECK(existing_app_host->browser);
      CHECK_NE(existing_app_host->tab_index, -1);
      result.browser =
          existing_app_host->browser
              ? existing_app_host->browser->GetBrowserForMigrationOnly()
              : nullptr;
      result.tab_index = existing_app_host->tab_index;
      return result;
    }
    // If no tab was found to focus or navigate, we'll need to open and
    // navigate a new tab instead.
    result.effective_client_mode = LaunchHandler::ClientMode::kNavigateNew;
  }

  CHECK_EQ(result.effective_client_mode,
           LaunchHandler::ClientMode::kNavigateNew);
  result.tab_index = std::nullopt;
  switch (requested_display_mode) {
    case blink::mojom::DisplayMode::kUndefined:
    case blink::mojom::DisplayMode::kBrowser:
      // For kBrowser apps, an explicitly specific browser to navigate in
      // should override what browser we might otherwise use for the profile.
      if (navigation_params_browser_ &&
          navigation_params_browser_->is_type_normal()) {
        result.browser = navigation_params_browser_;
      } else {
        BrowserWindowInterface* browser = FindNormalBrowser(*profile_);
        result.browser =
            browser ? browser->GetBrowserForMigrationOnly() : nullptr;
      }
      break;
    case blink::mojom::DisplayMode::kMinimalUi:
    case blink::mojom::DisplayMode::kStandalone:
    case blink::mojom::DisplayMode::kWindowControlsOverlay:
    case blink::mojom::DisplayMode::kBorderless:
      // Non-tabbed standalone modes do not support opening a new tab in an
      // existing browser. So never return a browser in this case.
      break;
    case blink::mojom::DisplayMode::kTabbed: {
      // TODO(crbug.com/403587716): Add tests for this case. Test should mimic
      // opening two app windows and prioritizing the one that gets passed in
      // NavigateParams.
      if (navigation_params_browser_ &&
          WebAppBrowserController::IsForWebApp(navigation_params_browser_,
                                               app_id) &&
          navigation_params_browser_->app_controller()->has_tab_strip()) {
        result.browser = navigation_params_browser_;
        break;
      }
      BrowserWindowInterface* browser_for_web_app =
          AppBrowserController::FindForWebApp(*profile_, app_id);
      result.browser = browser_for_web_app
                           ? browser_for_web_app->GetBrowserForMigrationOnly()
                           : nullptr;
      // If somehow we found a browser that doesn't have a tab strip (which
      // might be possible if the manifest updated while a window is open),
      // don't return it to use for new tabs.
      if (result.browser &&
          !result.browser->app_controller()->has_tab_strip()) {
        result.browser = nullptr;
      }
      break;
    }
    case blink::mojom::DisplayMode::kFullscreen:
    case blink::mojom::DisplayMode::kPictureInPicture:
      NOTREACHED();
  }

  return result;
}

NavigationCapturingProcess::MaybeNavigationCapturingOverride
NavigationCapturingProcess::CapturingDisabled() {
  // Don't record debug information for ALL navigations unless expensive DCHECKs
  // are enabled.
#if EXPENSIVE_DCHECKS_ARE_ON()
  debug_data_.Set("!result", "capturing disabled");
#else
  debug_data_.clear();
#endif
  CHECK_EQ(state_, PipelineState::kCreated);
  state_ = PipelineState::kInitialOverrideCalculated;
  initial_nav_handling_result_ = NavigationCapturingInitialResult::kNotHandled;
  return std::nullopt;
}

NavigationCapturingProcess::MaybeNavigationCapturingOverride
NavigationCapturingProcess::CancelInitialNavigation(
    NavigationCapturingInitialResult result) {
  debug_data_.Set("!result", "cancel navigation");
  CHECK_EQ(state_, PipelineState::kCreated);
  state_ = PipelineState::kInitialOverrideCalculated;
  initial_nav_handling_result_ = result;
  return NavigationCapturingOverride::CreateForCancelNavigation(
      base::PassKey<NavigationCapturingProcess>());
}

NavigationCapturingProcess::MaybeNavigationCapturingOverride
NavigationCapturingProcess::NoCapturingOverrideBrowser(Browser* browser) {
  debug_data_.Set("!result", "no capturing, override browser");
  CHECK_EQ(state_, PipelineState::kCreated);
  state_ = PipelineState::kInitialOverrideCalculated;
  initial_nav_handling_result_ =
      NavigationCapturingInitialResult::kOverrideBrowser;
  return NavigationCapturingOverride::CreateForNavigateNew(
      base::PassKey<NavigationCapturingProcess>(), browser);
}

NavigationCapturingProcess::MaybeNavigationCapturingOverride
NavigationCapturingProcess::AuxiliaryContext() {
  initial_nav_handling_result_ =
      NavigationCapturingInitialResult::kAuxiliaryContextAppBrowserTab;
  // Don't record debug information for ALL navigations unless expensive DCHECKs
  // are enabled.
#if EXPENSIVE_DCHECKS_ARE_ON()
  debug_data_.Set("!result", "auxiliary context");
#else
  debug_data_.clear();
#endif
  CHECK_EQ(state_, PipelineState::kCreated);
  state_ = PipelineState::kInitialOverrideCalculated;
  return std::nullopt;
}

NavigationCapturingProcess::MaybeNavigationCapturingOverride
NavigationCapturingProcess::AuxiliaryContextInAppWindow(Browser* app_browser) {
  CHECK(app_browser->app_controller());
  initial_nav_handling_result_ =
      NavigationCapturingInitialResult::kAuxiliaryContextAppWindow;
  if (first_navigation_app_id_.has_value()) {
    SetLaunchedAppId(*first_navigation_app_id_);
  }
  debug_data_.Set("!result", "auxiliary context in app window");
  CHECK_EQ(state_, PipelineState::kCreated);
  state_ = PipelineState::kInitialOverrideCalculated;
  return NavigationCapturingOverride::CreateForNavigateNew(
      base::PassKey<NavigationCapturingProcess>(), app_browser);
}

NavigationCapturingProcess::MaybeNavigationCapturingOverride
NavigationCapturingProcess::NoInitialActionRedirectionHandlingEligible() {
  initial_nav_handling_result_ =
      NavigationCapturingInitialResult::kNewTabRedirectionEligible;
  // Don't record debug information for ALL navigations unless expensive DCHECKs
  // are enabled.
  // TODO(https://crbug.com/351775835): Consider not erasing debug data until we
  // know the redirect wasn't navigation captured either.
#if EXPENSIVE_DCHECKS_ARE_ON()
  debug_data_.Set("!result",
                  "no initial action, redirection handling eligible");
#else
  debug_data_.clear();
#endif
  CHECK_EQ(state_, PipelineState::kCreated);
  state_ = PipelineState::kInitialOverrideCalculated;
  return std::nullopt;
}

NavigationCapturingProcess::MaybeNavigationCapturingOverride
NavigationCapturingProcess::ForcedNewAppContext(
    blink::mojom::DisplayMode app_display_mode,
    Browser* host_browser) {
  CHECK(first_navigation_app_id_.has_value());
  CHECK(WebAppRegistrar::IsSupportedDisplayModeForNavigationCapture(
      app_display_mode));
  CHECK((app_display_mode != blink::mojom::DisplayMode::kBrowser) ==
        (!!host_browser->app_controller()));
  CHECK(disposition_ == WindowOpenDisposition::NEW_BACKGROUND_TAB ||
        disposition_ == WindowOpenDisposition::NEW_WINDOW);
  CHECK(is_user_modified_click());
  initial_nav_handling_result_ =
      app_display_mode == DisplayMode::kBrowser
          ? NavigationCapturingInitialResult::kForcedContextAppBrowserTab
          : NavigationCapturingInitialResult::kForcedContextAppWindow;
  // Do not show iph when opening browser-tab-apps in a new browser tab, as
  // this matches what is 'normal' - clicking on a link opens a new browser
  // tab.
  SetLaunchedAppId(*first_navigation_app_id_,
                   /*force_iph_off=*/app_display_mode == DisplayMode::kBrowser);
  debug_data_.Set("!result", "forced new app context");
  CHECK_EQ(state_, PipelineState::kCreated);
  state_ = PipelineState::kInitialOverrideCalculated;
  return NavigationCapturingOverride::CreateForNavigateNew(
      base::PassKey<NavigationCapturingProcess>(), host_browser);
}

NavigationCapturingProcess::MaybeNavigationCapturingOverride
NavigationCapturingProcess::CapturedNewClient(
    blink::mojom::DisplayMode app_display_mode,
    Browser* host_browser) {
  SCOPED_CRASH_KEY_STRING1024("crbug396028223", "app_display_mode",
                              base::ToString((app_display_mode)));
  SCOPED_CRASH_KEY_STRING1024(
      "crbug396028223", "contains_app_controller",
      base::ToString((!!host_browser->app_controller())));

  debug_data_.Set("!result", "captured new client");
  SCOPED_CRASH_KEY_STRING1024("crbug396028223", "capturing_debug_info",
                              debug_data_.DebugString());

  CHECK(first_navigation_app_id_.has_value());
  CHECK(WebAppRegistrar::IsSupportedDisplayModeForNavigationCapture(
      app_display_mode));
  CHECK((app_display_mode != blink::mojom::DisplayMode::kBrowser) ==
        (!!host_browser->app_controller()));

  if (isolated_web_app_navigation_) {
    CHECK(disposition_ == WindowOpenDisposition::NEW_FOREGROUND_TAB ||
          disposition_ == WindowOpenDisposition::CURRENT_TAB);
    initial_nav_handling_result_ =
        NavigationCapturingInitialResult::kNewAppWindow;
  } else {
    CHECK_EQ(disposition_, WindowOpenDisposition::NEW_FOREGROUND_TAB);
    initial_nav_handling_result_ =
        app_display_mode == DisplayMode::kBrowser
            ? NavigationCapturingInitialResult::kNewAppBrowserTab
            : NavigationCapturingInitialResult::kNewAppWindow;
  }
  // Do not show iph when opening browser-tab-apps in a new browser tab, as
  // this matches what is 'normal' - clicking on a link opens a new browser
  // tab.
  SetLaunchedAppId(*first_navigation_app_id_,
                   /*force_iph_off=*/app_display_mode == DisplayMode::kBrowser);
  CHECK_EQ(state_, PipelineState::kCreated);
  state_ = PipelineState::kInitialOverrideCalculated;
  return NavigationCapturingOverride::CreateForNavigateNew(
      base::PassKey<NavigationCapturingProcess>(), host_browser);
}

NavigationCapturingProcess::MaybeNavigationCapturingOverride
NavigationCapturingProcess::CapturedNavigateExisting(Browser* app_browser,
                                                     int browser_tab) {
  CHECK(first_navigation_app_id_.has_value());

  CHECK(browser_tab != -1);
  if (isolated_web_app_navigation_) {
    CHECK(disposition_ == WindowOpenDisposition::NEW_FOREGROUND_TAB ||
          disposition_ == WindowOpenDisposition::CURRENT_TAB);

    CHECK(WebAppBrowserController::IsWebApp(app_browser));
    initial_nav_handling_result_ =
        NavigationCapturingInitialResult::kNavigateExistingAppWindow;
  } else {
    CHECK_EQ(disposition_, WindowOpenDisposition::NEW_FOREGROUND_TAB);
    initial_nav_handling_result_ =
        WebAppBrowserController::IsWebApp(app_browser)
            ? NavigationCapturingInitialResult::kNavigateExistingAppWindow
            : NavigationCapturingInitialResult::kNavigateExistingAppBrowserTab;
  }
  SetLaunchedAppId(*first_navigation_app_id_);
  debug_data_.Set("!result", "captured navigate existing");
  CHECK_EQ(state_, PipelineState::kCreated);
  state_ = PipelineState::kInitialOverrideCalculated;
  return NavigationCapturingOverride::CreateForNavigateExisting(
      base::PassKey<NavigationCapturingProcess>(), app_browser, browser_tab);
}

NavigationCapturingProcess::MaybeNavigationCapturingOverride
NavigationCapturingProcess::CapturedFocusExisting(Browser* browser,
                                                  int browser_tab,
                                                  const GURL& url) {
  CHECK(first_navigation_app_id_.has_value());
  const auto& app_id = *first_navigation_app_id_;

  content::WebContents* contents =
      browser->tab_strip_model()->GetWebContentsAt(browser_tab);
  CHECK(contents);
  FocusAppContainer(browser, browser_tab);

  CHECK(!time_navigation_started_.is_null());
  bool is_current_container_window = WebAppBrowserController::IsWebApp(browser);

  // Abort the navigation by returning a `nullptr`. Because this means
  // `OnWebAppNavigationAfterWebContentsCreation` won't be called, enqueue
  // the launch params instantly and record the debug data.
  EnqueueLaunchParams(contents, app_id, url,
                      /*wait_for_navigation_to_complete=*/false,
                      time_navigation_started_);

  MaybeShowNavigationCaptureIph(app_id, &*profile_, browser);

  // TODO(crbug.com/336371044): Update RecordLaunchMetrics() to also work
  // with apps that open in a new browser tab.
  RecordLaunchMetrics(app_id, apps::LaunchContainer::kLaunchContainerWindow,
                      apps::LaunchSource::kFromNavigationCapturing, url,
                      contents);

  RecordNavigationCapturingDisplayModeMetrics(app_id, contents,
                                              !is_current_container_window);

  debug_data_.Set("!result", "cancel navigation (focus-existing)");
  CHECK_EQ(state_, PipelineState::kCreated);
  state_ = PipelineState::kInitialOverrideCalculated;
  initial_nav_handling_result_ =
      is_current_container_window
          ? NavigationCapturingInitialResult::kFocusExistingAppWindow
          : NavigationCapturingInitialResult::kFocusExistingAppBrowserTab;
  return NavigationCapturingOverride::CreateForFocusExisting(
      base::PassKey<NavigationCapturingProcess>(), contents);
}

void NavigationCapturingProcess::SetLaunchedAppId(
    std::optional<webapps::AppId> app_id,
    bool force_iph_off) {
  CHECK(IsHandledByNavigationCapturing());
  launched_app_id_ = app_id;
  force_iph_off_ = force_iph_off;
  debug_data_.Set("!result.launched_app_id", app_id.value_or("<none>"));
  debug_data_.Set("!result.force_iph_off", force_iph_off);
  if (!navigation_handle()) {
    return;
  }

  // Always delete the existing user data before optionally recreating new user
  // data.
  if (WebAppLaunchNavigationHandleUserData::GetForNavigationHandle(
          *navigation_handle())) {
    WebAppLaunchNavigationHandleUserData::DeleteForNavigationHandle(
        *navigation_handle());
  }
  if (launched_app_id_.has_value()) {
    OnAttachedToNavigationHandle();
  }
}

bool NavigationCapturingProcess::InitialResultWasCaptured() const {
  switch (initial_nav_handling_result_) {
    case NavigationCapturingInitialResult::kNewTabRedirectionEligible:
    case NavigationCapturingInitialResult::kForcedContextAppWindow:
    case NavigationCapturingInitialResult::kForcedContextAppBrowserTab:
    case NavigationCapturingInitialResult::kNotHandled:
    case NavigationCapturingInitialResult::kAuxiliaryContextAppWindow:
    case NavigationCapturingInitialResult::kAuxiliaryContextAppBrowserTab:
    case NavigationCapturingInitialResult::kOverrideBrowser:
    case NavigationCapturingInitialResult::kNavigationCanceled:
      return false;
    case NavigationCapturingInitialResult::kNewAppWindow:
    case NavigationCapturingInitialResult::kNewAppBrowserTab:
    case NavigationCapturingInitialResult::kNavigateExistingAppBrowserTab:
    case NavigationCapturingInitialResult::kNavigateExistingAppWindow:
    case NavigationCapturingInitialResult::kFocusExistingAppBrowserTab:
    case NavigationCapturingInitialResult::kFocusExistingAppWindow:
      return true;
  }
}

bool NavigationCapturingProcess::IsHandledByNavigationCapturing() const {
  switch (initial_nav_handling_result_) {
    case NavigationCapturingInitialResult::kNewTabRedirectionEligible:
    case NavigationCapturingInitialResult::kForcedContextAppWindow:
    case NavigationCapturingInitialResult::kForcedContextAppBrowserTab:
    case NavigationCapturingInitialResult::kAuxiliaryContextAppWindow:
    case NavigationCapturingInitialResult::kAuxiliaryContextAppBrowserTab:
    case NavigationCapturingInitialResult::kNewAppWindow:
    case NavigationCapturingInitialResult::kNewAppBrowserTab:
    case NavigationCapturingInitialResult::kNavigateExistingAppBrowserTab:
    case NavigationCapturingInitialResult::kNavigateExistingAppWindow:
    case NavigationCapturingInitialResult::kFocusExistingAppBrowserTab:
    case NavigationCapturingInitialResult::kFocusExistingAppWindow:
    case NavigationCapturingInitialResult::kNavigationCanceled:
      return true;
    case NavigationCapturingInitialResult::kNotHandled:
    case NavigationCapturingInitialResult::kOverrideBrowser:
      return false;
  }
}

base::Value::Dict& NavigationCapturingProcess::PopulateAndGetDebugData() {
  debug_data_.Set("!navigation_params_url",
                  navigation_params_url_.possibly_invalid_spec());
  debug_data_.Set("navigation_params_browser",
                  base::ToString(navigation_params_browser_.get()));
  debug_data_.Set("isolated_web_app_navigation", isolated_web_app_navigation_);
  debug_data_.Set("source_tab_app_id", source_tab_app_id_.value_or("<none>"));
  debug_data_.Set("disposition", base::ToString(disposition_));
  debug_data_.Set("state", base::ToString(state_));
  debug_data_.Set("initiating_profile", profile_->GetDebugName());
  debug_data_.Set("source_browser_app_id",
                  source_browser_app_id_.value_or("<none>"));
  debug_data_.Set("is_user_modified_click", is_user_modified_click());
  debug_data_.Set("first_navigation_app_id",
                  first_navigation_app_id_.value_or("<none>"));
  debug_data_.Set("first_navigation_registrar_effective_display_mode_",
                  base::ToString(first_navigation_app_display_mode_.value_or(
                      blink::mojom::DisplayMode::kUndefined)));
  return debug_data_;
}

NAVIGATION_HANDLE_USER_DATA_KEY_IMPL(NavigationCapturingProcess);

}  // namespace web_app
