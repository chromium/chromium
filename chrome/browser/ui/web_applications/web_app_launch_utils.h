// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_LAUNCH_UTILS_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_LAUNCH_UTILS_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback_helpers.h"
#include "base/memory/stack_allocated.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/navigation_capturing_navigation_handle_user_data.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/navigation_handle.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom-shared.h"
#include "ui/gfx/geometry/rect.h"

class Profile;
class Browser;
class GURL;
enum class WindowOpenDisposition;
struct NavigateParams;

namespace apps {
struct AppLaunchParams;
}

namespace content {
class WebContents;
}

namespace web_app {
// This function moves `contents` from the `source_browser` to the
// `target_browser`. In doing so, it attempts to ensure that any logic that
// needs to occur when transitioning between 'app' and 'browser' windows occurs,
// and the all session restore logic is correctly updated. `contents` is not
// required to be the active web contents in `source_browser`.
//
// Note: This will CHECK-fail if `contents` is not in `source_browser`.
void ReparentWebContentsIntoBrowserImpl(
    Browser* source_browser,
    content::WebContents* contents,
    Browser* target_browser,
    bool insert_as_pinned_first_tab = false);

class AppBrowserController;
class WithAppResources;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class LaunchedAppType {
  kDiy = 0,
  kCrafted = 1,
  kMaxValue = kCrafted,
};

// Returns information useful for the browser to show UI affordances if a web
// app handles the navigation.
class AppNavigationResult {
  STACK_ALLOCATED();

 public:
  // No navigation capturing will happen for this navigation.
  static AppNavigationResult CapturingDisabled();
  // The navigation itself will be cancelled.
  static AppNavigationResult CancelNavigation();

  static AppNavigationResult NoCapturingOverrideBrowser(Browser* browser);

  // TODO(crbug.com/370856876): Possibly remove `disposition`.
  static AppNavigationResult AuxiliaryContext(WindowOpenDisposition disposition,
                                              base::Value::Dict debug_data);

  // TODO(crbug.com/370856876): Possibly remove `source_browser_app_id` and
  // `disposition`.
  static AppNavigationResult AuxiliaryContextInAppWindow(
      const webapps::AppId& source_browser_app_id,
      std::optional<webapps::AppId> source_tab_app_id,
      WindowOpenDisposition disposition,
      Browser* app_browser,
      base::Value::Dict debug_data);

  // Populates redirection info in case future redirects apply to an
  // application.
  static AppNavigationResult NoInitialActionRedirectionHandlingEligible(
      std::optional<webapps::AppId> source_browser_app_id,
      std::optional<webapps::AppId> source_tab_app_id,
      WindowOpenDisposition disposition,
      base::Value::Dict debug_data);

  // Create AppNavigationResult for a navigation triggered by user modified link
  // clicks that creates a new app container.
  static AppNavigationResult ForcedNewAppContext(
      std::optional<webapps::AppId> source_browser_app_id,
      std::optional<webapps::AppId> source_tab_app_id,
      const webapps::AppId capturing_app_id,
      blink::mojom::DisplayMode new_client_display_mode,
      Browser* host_browser,
      WindowOpenDisposition disposition,
      base::Value::Dict debug_data);

  // Create AppNavigationResult for a navigation that is captured by non user
  // modified link clicks that launch a new app container (either window or
  // tab).
  static AppNavigationResult CapturedNewClient(
      std::optional<webapps::AppId> source_browser_app_id,
      std::optional<webapps::AppId> source_tab_app_id,
      const webapps::AppId capturing_app_id,
      blink::mojom::DisplayMode new_client_display_mode,
      Browser* host_browser,
      WindowOpenDisposition disposition,
      base::Value::Dict debug_data);

  // Create AppNavigationResult for a navigation that is captured by non user
  // modified link clicks that uses an existing app container (either window or
  // tab).
  static AppNavigationResult CapturedNavigateExisting(
      std::optional<webapps::AppId> source_browser_app_id,
      std::optional<webapps::AppId> source_tab_app_id,
      const webapps::AppId capturing_app_id,
      Browser* app_browser,
      int browser_tab,
      WindowOpenDisposition disposition,
      base::Value::Dict debug_data);

  AppNavigationResult(AppNavigationResult&&);
  AppNavigationResult& operator=(AppNavigationResult&&);

  // If false, then `OnWebAppNavigationAfterWebContentsCreation() exits early`.
  bool capturing_feature_enabled() const { return capturing_feature_enabled_; }

  // The browser instance to perform navigation in, and the tab inside the
  // browser if overridden by the web app system. If std::nullopt, performs the
  // default navigation behavior in browser_navigator.cc.
  const std::optional<std::tuple<Browser*, int>>& browser_tab_override() const {
    return browser_tab_override_;
  }

  // True if the `MaybeHandleAppNavigation` considered this navigation to
  // be capturable, and the resulting navigation should be considered a launch
  // for the given app (and do things like enqueue launch params and show IPH).
  bool perform_app_handling_tasks_in_web_contents() const {
    return perform_app_handling_tasks_in_web_contents_;
  }

  // Information necessary for handling redirection after a response is received
  // as part of a navigation.
  const NavigationCapturingRedirectionInfo& redirection_info() const {
    return redirection_info_;
  }

  base::Value::Dict TakeDebugData();

 private:
  AppNavigationResult(
      bool capturing_feature_enabled,
      std::optional<std::tuple<Browser*, int>> browser_tab_override,
      bool perform_app_handling_tasks_in_web_contents,
      const NavigationCapturingRedirectionInfo& redirection_info,
      base::Value::Dict debug_value);

  bool capturing_feature_enabled_ = false;

  std::optional<std::tuple<Browser*, int>> browser_tab_override_;
  bool perform_app_handling_tasks_in_web_contents_ = false;

  NavigationCapturingRedirectionInfo redirection_info_;

  // Debug information persisted to chrome://web-app-internals.
  base::Value::Dict debug_value_;
};

std::optional<webapps::AppId> GetWebAppForActiveTab(const Browser* browser);

// Clears navigation history prior to user entering app scope.
void PrunePreScopeNavigationHistory(const GURL& scope,
                                    content::WebContents* contents);

// Invokes ReparentWebContentsIntoAppBrowser() for the active tab for the
// web app that has the tab's URL in its scope. Does nothing if there is no web
// app in scope.
Browser* ReparentWebAppForActiveTab(Browser* browser);

// Reparents `contents` into a standalone web app window for `app_id`.
// - If the web app has a launch_handler set to reuse existing windows and there
// are existing web app windows around this will launch the web app into the
// existing window and close `contents`.
// - If the web app is in experimental tabbed mode and has and existing web app
// window, `contents` will be reparented into the existing window.
// - Otherwise a new browser window is created for `contents` to be reparented
// into.
// Returns the browser instance where the reparenting has happened, nullptr
// otherwise. Runs `completion_callback` with the existing `contents`, if it was
// reparented, or with the new `web_contents` that was created if the behavior
// deemed it necessary (like for focus existing and navigate-existing
// use-cases).
Browser* ReparentWebContentsIntoAppBrowser(
    content::WebContents* contents,
    const webapps::AppId& app_id,
    base::OnceCallback<void(content::WebContents*)> completion_callback =
        base::DoNothingAs<void(content::WebContents*)>());

// Marks the web contents as being the pinned home tab of a tabbed web app.
void SetWebContentsIsPinnedHomeTab(content::WebContents* contents);

std::unique_ptr<AppBrowserController> MaybeCreateAppBrowserController(
    Browser* browser);

void MaybeAddPinnedHomeTab(Browser* browser, const std::string& app_id);

// Shows the navigation capturing IPH if the situation warrants it (e.g. the
// WebAppProvider is available, guardrail metrics are not suppressing it and
// the IPH is permitted to show).
void MaybeShowNavigationCaptureIph(webapps::AppId app_id,
                                   Profile* profile,
                                   Browser* browser);

// This creates appropriate CreateParams for creating a PWA window or PWA popup
// window.
Browser::CreateParams CreateParamsForApp(const webapps::AppId& app_id,
                                         bool is_popup,
                                         bool trusted_source,
                                         const gfx::Rect& window_bounds,
                                         Profile* profile,
                                         bool user_gesture);

Browser* CreateWebAppWindowMaybeWithHomeTab(
    const webapps::AppId& app_id,
    const Browser::CreateParams& params);

content::WebContents* NavigateWebAppUsingParams(const std::string& app_id,
                                                NavigateParams& nav_params);

// RecordLaunchMetrics methods report UMA metrics. It shouldn't have other
// side-effects (e.g. updating app launch time).
void RecordLaunchMetrics(const webapps::AppId& app_id,
                         apps::LaunchContainer container,
                         apps::LaunchSource launch_source,
                         const GURL& launch_url,
                         content::WebContents* web_contents);

// Updates statistics about web app launch. For example, app's last launch time
// (populates recently launched app list) and site engagement stats.
void UpdateLaunchStats(content::WebContents* web_contents,
                       const webapps::AppId& app_id,
                       const GURL& launch_url);

// Locks that lock apps all have the WithAppResources mixin, allowing any
// app-locking lock to call this method.
void LaunchWebApp(apps::AppLaunchParams params,
                  LaunchWebAppWindowSetting launch_setting,
                  Profile& profile,
                  WithAppResources& app_resources,
                  LaunchWebAppDebugValueCallback callback);

// Returns the effective client mode for the given app, taking into account the
// app's effective display mode as well as what windows and tabs are currently
// open.
//
// If an applicable browser and tab for the given app was found, `browser` and
// `tab_index` will be populated even if the effective client mode is
// `kNavigateNew`. On the other hand, the returned client mode will never be
// `kFocusExisting` or `kNavigateExisting` if no existing tab was found.
//
// Searches all browsers and tabs to find an applicable browser for the given
// `app_id` and its effective display mode, specifically for use with navigation
// capturing. The tabs in each browser are searched for one that matches the
// given `app_id`. This is the priority order of returned items:
// - If a tab is found for `app_id` in a browser that matches the
//   display mode, then that is returned.
// - If the display mode is for a standalone PWA:
//   - Fall back to look for the first normal browser with a tab matching
//     `app_id`, unless `ignore_browser_tabs_for_standalone_apps` is set.
//   - Otherwise set `browser` to `nullptr`.
// - If the display mode is `kBrowser`:
//   - Fall back to returning `navigate_params_requested_browser` or the first
//     normal browser window, and `nullopt` for the tab.
//   - Otherwise set `browser` to `nullptr`.
// - Set `browser` to `nullptr` for all other cases.
struct ClientModeAndBrowser {
  LaunchHandler::ClientMode effective_client_mode =
      LaunchHandler::ClientMode::kNavigateNew;
  raw_ptr<Browser> browser = nullptr;
  std::optional<int> tab_index;
};
ClientModeAndBrowser GetEffectiveClientModeAndBrowserForCapturing(
    Profile& profile,
    const webapps::AppId& app_id,
    const std::optional<webapps::AppId> source_tab_app_id_from_navigation,
    bool ignore_browser_tabs_for_standalone_apps,
    Browser* navigate_params_requested_browser);

// Returns an AppNavigationResult with pertinent details on how to handle a
// navigation if the web app system can do so. If not, the
// `browser_tab_override` is set to be std::nullopt so that ::Navigate() inside
// the browser_navigator code can pick this up. This function may create a
// browser instance, an app window or a new tab as needed.
AppNavigationResult MaybeHandleAppNavigation(
    const NavigateParams& navigate_params);

// Will enqueue the given url in the launch params for this web contents. Does
// not check if the url is within scope of the app.
void EnqueueLaunchParams(content::WebContents* contents,
                         const webapps::AppId& app_id,
                         const GURL& url,
                         bool wait_for_navigation_to_complete);

// Handle navigation-related tasks for the app, like enqueuing launch params,
// showing a navigation capturing IPH bubble and storing information necessary
// for handling redirections in the current `WebContents` or `NavigationHandle`,
// after the appropriate app-scoped `WebContents` has been identified and
// prepared for navigation.
void OnWebAppNavigationAfterWebContentsCreation(
    web_app::AppNavigationResult app_navigation_result,
    const NavigateParams& params,
    base::WeakPtr<content::NavigationHandle> navigation_handle);

// Focus the app container depending on whether the `browser` is an app window
// or if it is a normal tabbed browser. `browser` shouldn't be a nullptr, and
// the `tab_index` should be a valid index for a tab inside `browser`.
void FocusAppContainer(Browser* browser, int tab_index);

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_LAUNCH_UTILS_H_
