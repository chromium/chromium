// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_NAVIGATION_CAPTURING_PROCESS_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_NAVIGATION_CAPTURING_PROCESS_H_

#include <memory>
#include <optional>
#include <ostream>
#include <tuple>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/web_app_launch_navigation_handle_user_data.h"
#include "chrome/browser/web_applications/navigation_capturing_metrics.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "components/webapps/common/web_app_id.h"
#include "ui/base/window_open_disposition.h"

class Browser;
struct NavigateParams;

namespace web_app {

class NavigationCapturingSettings;

// This class encompasses all of the logic for navigations in the browser to be
// captured into installed web app launches.
//
// Instances are created by `::Navigate()` in `browser_navigator.cc`, which
// also calls into this class to possibly capture the initial navigation. The
// instance is then eventually attached to the `NavigationHandle` for the
// navigation that was started. Later when all redirects for the navigation
// has been resolved, `NavigationCapturingRedirectThrottle()` again calls into
// this class to handle the result of the redirects, possible changing what app
// captures the navigation (or not capturing a previously captured navigation
// any longer).
//
// This class is also responsible for populating
// WebAppLaunchNavigationHandleUserData, which is used to gather launch metrics,
// populate the launch queue, and possibly trigger IPH.
class NavigationCapturingProcess
    : public content::NavigationHandleUserData<NavigationCapturingProcess> {
 public:
  // Returns null if navigation capturing is disabled for this navigation. This
  // returning non-null however does not mean that navigation capturing will
  // definitely happen for this navigation; the logic in
  // `GetInitialBrowserAndTabOverrideForNavigation()` can still decide that no
  // capturing should apply to this navigation.
  static std::unique_ptr<NavigationCapturingProcess> MaybeHandleAppNavigation(
      const NavigateParams& navigate_params);

  ~NavigationCapturingProcess() override;

  // The first step of the navigation capturing process. This is called by
  // `browser_navigator.cc` to check if the navigation capturing process wants
  // to override the browser and or tab to use. A return value of `nullopt`
  // means that no overriding should happen (but the navigation could still be
  // captured on later redirects). A null `Browser*` means that the navigation
  // should be aborted.
  using BrowserAndTabOverride = std::optional<std::tuple<Browser*, int>>;
  BrowserAndTabOverride GetInitialBrowserAndTabOverrideForNavigation(
      const NavigateParams& params);

  // Called by `browesr_navigator.cc` after the WebContents and optionally
  // NavigationHandle are known for the navigation it handled. Will CHECK-fail
  // if called before `GetInitialBrowserAndTabOverrideForNavigation()`.
  static void AfterWebContentsCreation(
      std::unique_ptr<NavigationCapturingProcess> process,
      content::WebContents& web_contents,
      content::NavigationHandle* navigation_handle);

  // Attaches this NavigationCapturing process to a specific navigation. Will
  // CHECK-fail if called before
  // `GetInitialBrowserAndTabOverrideForNavigation()` is called. If that method
  // decided no navigation capturing should apply to this navigation, this will
  // destroy `user_data` rather than attach it.
  static void AttachToNavigationHandle(
      content::NavigationHandle& navigation_handle,
      std::unique_ptr<NavigationCapturingProcess> user_data);
  // Similar to `AttachToNavigationHandle()`, but instead attaches to the next
  // navigation to start in the given `web_contents`.
  static void AttachToNextNavigationInWebContents(
      content::WebContents& web_contents,
      std::unique_ptr<NavigationCapturingProcess> user_data);

  // The second step of the navigation capturing process. Called by
  // `NavigationCapturingRedirectionThrottle` when the final URL is known after
  // any possible redirects have happened. Will only be called after this class
  // has been attached to a NavigationHandle, and will CHECK-fail otherwise.
  using ThrottleCheckResult = content::NavigationThrottle::ThrottleCheckResult;
  ThrottleCheckResult HandleRedirect();

  content::NavigationHandle* navigation_handle() const {
    return navigation_handle_;
  }

 private:
  friend NavigationHandleUserData;

  // To ensure the (public) methods of this class get called in the correct
  // order, this enum is used to verify what state of the pipeline we're
  // currently in.
  enum class PipelineState {
    kCreated,
    kInitialOverrideCalculated,
    kAttachedToWebContents,
    kAttachedToNavigationHandle,
    kFinished
  };

  explicit NavigationCapturingProcess(const NavigateParams& params);

  // Returns true if based on the NavigateParams this instance was created with
  // the navigation capturing reimpl experiment is enabled.
  bool IsNavigationCapturingReimplExperimentEnabled();

  // Called when this process is attached to a NavigationHandle.
  void OnAttachedToNavigationHandle();

  // Returns the effective client mode for the given app, taking into account
  // the app's effective display mode as well as what windows and tabs are
  // currently open.
  //
  // If the effective client mode is `kNavigateNew`, only the `browser` will be
  // populated, indicating the window in which the new tab should be opened. If
  // a new window should be opened `browser` will be null.
  //
  // For an effective client mode of `kNavigateExisting` or `kFocusExisting`,
  // both `browser` and `tab_index` will be populated, indicating the tab that
  // should be navigated or focused.
  //
  // If the target app uses standalone 'app' tabbed display mode, the
  // `target_url` is used to decide if a pinned home tab or other tab should be
  // used.
  struct ClientModeAndBrowser {
    LaunchHandler::ClientMode effective_client_mode =
        LaunchHandler::ClientMode::kNavigateNew;
    raw_ptr<Browser> browser = nullptr;
    std::optional<int> tab_index;
  };
  ClientModeAndBrowser GetEffectiveClientModeAndBrowser(
      const webapps::AppId& app_id,
      const GURL& target_url);

  // Helper methods for `GetInitialBrowserAndTabOverrideForNavigation()` that
  // return the correct return value and update internal state of this class
  // with the corresponding outcome.
  BrowserAndTabOverride CapturingDisabled();
  BrowserAndTabOverride CancelInitialNavigation();
  BrowserAndTabOverride NoCapturingOverrideBrowser(Browser* browser);
  BrowserAndTabOverride AuxiliaryContext();
  BrowserAndTabOverride AuxiliaryContextInAppWindow(Browser* app_browser);
  BrowserAndTabOverride NoInitialActionRedirectionHandlingEligible();
  BrowserAndTabOverride ForcedNewAppContext(
      blink::mojom::DisplayMode app_display_mode,
      Browser* host_browser);
  BrowserAndTabOverride CapturedNewClient(
      blink::mojom::DisplayMode app_display_mode,
      Browser* host_browser);
  BrowserAndTabOverride CapturedNavigateExisting(Browser* app_browser,
                                                 int browser_tab);

  // Updates the `launched_app_id_` field, and if this process as already been
  // attached to a `NavigationHandle`, also creates or updates the
  // `WebAppLaunchNavigationHandleUserData` for that handle. An empty `app_id`
  // will cause the launch data to be cleared.
  void SetLaunchedAppId(const webapps::AppId& app_id,
                        bool force_iph_off = false);

  // Returns true if `initial_nav_handling_result_` is one of the values where
  // the navigation was captured by an app.
  bool InitialResultWasCaptured() const;

  // Returns true if `initial_nav_handling_result_` is one of the values where
  // the NavigationCapturingProcess performs navigation capturing and handles a
  // navigation, regardless of whether it opens a new app window or not.
  bool IsHandledByNavigationCapturing() const;

  bool is_user_modified_click() const {
    return disposition_ == WindowOpenDisposition::NEW_WINDOW ||
           disposition_ == WindowOpenDisposition::NEW_BACKGROUND_TAB;
  }

  base::Value::Dict& PopulateAndGetDebugData();

  PipelineState state_ = PipelineState::kCreated;

  std::unique_ptr<NavigationCapturingSettings> navigation_capturing_settings_;

  // These fields are copied or derived from the NavigateParams of the original
  // navigation.
  const raw_ref<Profile> profile_;
  const std::optional<webapps::AppId> source_browser_app_id_;
  const std::optional<webapps::AppId> source_tab_app_id_;
  const GURL navigation_params_url_;
  const WindowOpenDisposition disposition_;
  const raw_ptr<Browser> navigation_params_browser_;
  std::optional<webapps::AppId> first_navigation_app_id_;
  std::optional<blink::mojom::DisplayMode> first_navigation_app_display_mode_;

  bool navigation_capturing_enabled_ = false;

  // This field records the outcome of handling the initial navigation, before
  // any redirects might have happened. This is written to histograms on the
  // destruction of the NavigationCapturingProcess.
  NavigationCapturingInitialResult initial_nav_handling_result_ =
      NavigationCapturingInitialResult::kNotHandled;

  // This field records the outcome of the navigation post redirection, if it
  // happens. This is written to histograms on the destruction of the
  // NavigationCapturingProcess.
  std::optional<NavigationCapturingRedirectionResult> redirection_result_;

  // The app that ended up being launched as a result of the navigation being
  // captured. This is initially set by
  // `GetInitialBrowserAndTabOverrideForNavigation()` and can be cleared or
  // reset by `HandleRedirect()` if the redirect results in a different app
  // handling the launch.
  bool force_iph_off_ = false;
  std::optional<webapps::AppId> launched_app_id_;

  // Set to the NavigationHandle this process is owned by, once it is attached
  // to one.
  raw_ptr<content::NavigationHandle> navigation_handle_ = nullptr;

  // Debug information persisted to chrome://web-app-internals on destruction of
  // this class.
  base::Value::Dict debug_data_;
  std::optional<int64_t> navigation_handle_id_ = std::nullopt;

  // Stores the exact time when the navigation capturing process starts
  // "handling" the current navigation when asked from Navigate().
  base::TimeTicks time_navigation_started_{base::TimeTicks::Now()};

  NAVIGATION_HANDLE_USER_DATA_KEY_DECL();
};
}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_NAVIGATION_CAPTURING_PROCESS_H_
