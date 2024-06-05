// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXCLUSIVE_ACCESS_FULLSCREEN_CONTROLLER_H_
#define CHROME_BROWSER_UI_EXCLUSIVE_ACCESS_FULLSCREEN_CONTROLLER_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/time/time.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_controller_base.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_observer.h"
#include "components/content_settings/core/common/content_settings.h"
#include "content/public/browser/fullscreen_types.h"
#include "ui/display/types/display_constants.h"

class GURL;
class PopunderPreventer;

namespace content {
class WebContents;
class RenderFrameHost;
}  // namespace content

// There are two different kinds of fullscreen mode - "tab fullscreen" and
// "browser fullscreen". "Tab fullscreen" refers to a renderer-initiated
// fullscreen mode (eg: from a Flash plugin or via the JS fullscreen API),
// whereas "browser fullscreen" refers to the user putting the browser itself
// into fullscreen mode from the UI. The difference is that tab fullscreen has
// implications for how the contents of the tab render (eg: a video element may
// grow to consume the whole tab), whereas browser fullscreen mode doesn't.
// Therefore if a user forces an exit from tab fullscreen, we need to notify the
// tab so it can stop rendering in its fullscreen mode.
//
// For Flash, FullscreenController will auto-accept all permission requests for
// fullscreen, since the assumption is that the plugin handles this for us.
//
// FullscreenWithinTab Note:
// All fullscreen widgets are displayed within the tab contents area, and
// FullscreenController will expand the browser window so that the tab contents
// area fills the entire screen.
// However, special behavior applies when a tab is screen-captured or the
// content fullscreen feature is active.
//
// Screen-captured:
// First, the browser window will not be fullscreened. This allows the user to
// retain control of their desktop to work in other browser tabs or applications
// while the fullscreen view is displayed on a remote screen. Second,
// FullscreenController will auto-resize fullscreen widgets to that of the
// capture video resolution when they are hidden (e.g., when a user has
// switched to another tab). This is both a performance and quality improvement
// since scaling and letterboxing steps can be skipped in the capture pipeline.
//
// This class implements fullscreen behaviour.
class FullscreenController : public ExclusiveAccessControllerBase {
 public:
  explicit FullscreenController(ExclusiveAccessManager* manager);

  FullscreenController(const FullscreenController&) = delete;
  FullscreenController& operator=(const FullscreenController&) = delete;

  ~FullscreenController() override;

  void AddObserver(FullscreenObserver* observer);
  void RemoveObserver(FullscreenObserver* observer);

  static int64_t GetDisplayId(const content::WebContents& web_contents);

  // Browser/User Fullscreen ///////////////////////////////////////////////////

  // Returns true if the window is currently fullscreen and was initially
  // transitioned to fullscreen by a browser (i.e., not tab-initiated) mode
  // transition.
  bool IsFullscreenForBrowser() const;

  void ToggleBrowserFullscreenMode();

  // Extension API implementation uses this method to toggle fullscreen mode.
  // The extension's name is displayed in the full screen bubble UI to attribute
  // the cause of the full screen state change.
  void ToggleBrowserFullscreenModeWithExtension(const GURL& extension_url);

  // Tab/HTML/Flash Fullscreen /////////////////////////////////////////////////

  // Returns true if the browser window has/will fullscreen because of
  // tab-initiated fullscreen. The window may still be transitioning, and
  // BrowserWindow::IsFullscreen() may still return false.
  bool IsWindowFullscreenForTabOrPending() const;

  // Returns true if the browser window is fullscreen because of extension
  // initiated fullscreen.
  bool IsExtensionFullscreenOrPending() const;

  // Returns true if controller has entered fullscreen mode.
  bool IsControllerInitiatedFullscreen() const;

  // Returns true if the site has entered fullscreen.
  bool IsTabFullscreen() const;

  // Returns fullscreen state information about the given `web_contents`.
  content::FullscreenState GetFullscreenState(
      const content::WebContents* web_contents) const;

  // Returns true if |web_contents| is in fullscreen mode as a screen-captured
  // tab. See 'FullscreenWithinTab Note'.
  bool IsFullscreenWithinTab(const content::WebContents* web_contents) const;

  // True if fullscreen was entered because of tab fullscreen (was not
  // previously in user-initiated fullscreen).
  bool IsFullscreenCausedByTab() const;

  // Returns whether entering fullscreen with |EnterFullscreenModeForTab()| is
  // allowed.
  bool CanEnterFullscreenModeForTab(content::RenderFrameHost* requesting_frame);

  // Enter tab-initiated fullscreen mode. FullscreenController decides whether
  // to also fullscreen the browser window. See 'FullscreenWithinTab Note'.
  // `requesting_frame` is the specific content frame requesting fullscreen.
  // Sites with the Window Management permission may request fullscreen on a
  // particular display. In that case, `display_id` is the display's id;
  // otherwise, display::kInvalidDisplayId indicates no display is specified.
  // `CanEnterFullscreenModeForTab()` must return true on entry.
  void EnterFullscreenModeForTab(
      content::RenderFrameHost* requesting_frame,
      const int64_t display_id = display::kInvalidDisplayId);

  // Leave a tab-initiated fullscreen mode.
  // |web_contents| represents the tab that requests to no longer be fullscreen.
  void ExitFullscreenModeForTab(content::WebContents* web_contents);

  base::WeakPtr<FullscreenController> GetWeakPtr() {
    return ptr_factory_.GetWeakPtr();
  }

  // Called when fullscreen tabs open popups, to track potential popunders.
  void FullscreenTabOpeningPopup(content::WebContents* opener,
                                 content::WebContents* popup);

  // Platform Fullscreen ///////////////////////////////////////////////////////

  // Override from ExclusiveAccessControllerBase.
  void OnTabDeactivated(content::WebContents* web_contents) override;
  void OnTabDetachedFromView(content::WebContents* web_contents) override;
  void OnTabClosing(content::WebContents* web_contents) override;
  bool HandleUserPressedEscape() override;
  void HandleUserHeldEscape() override;
  void HandleUserReleasedEscapeEarly() override;
  bool RequiresPressAndHoldEscToExit() const override;

  void ExitExclusiveAccessToPreviousState() override;
  GURL GetURLForExclusiveAccessBubble() const override;
  void ExitExclusiveAccessIfNecessary() override;
  // Callbacks /////////////////////////////////////////////////////////////////

  // Called by Browser::WindowFullscreenStateChanged. This is called immediately
  // as fullscreen mode is toggled.
  void WindowFullscreenStateChanged();

  // Called by BrowserView::FullscreenStateChanged. This is called after
  // fullscreen mode is toggled and after the transition animation completes.
  void FullscreenTransitionCompleted();

  // Runs the given closure unless a fullscreen transition is currently in
  // progress. If a transition is in progress, the execution of the closure is
  // deferred and run after the transition is complete.
  void RunOrDeferUntilTransitionIsComplete(base::OnceClosure callback);

  void set_is_tab_fullscreen_for_testing(bool is_tab_fullscreen) {
    is_tab_fullscreen_for_testing_ = is_tab_fullscreen;
  }

 private:
  friend class ExclusiveAccessTest;

  enum FullscreenInternalOption { BROWSER, TAB };

  // Posts a task to notify observers of the fullscreen state change.
  void PostFullscreenChangeNotification();
  void NotifyFullscreenChange();

  // Notifies the tab that it has been forced out of fullscreen mode if
  // necessary.
  void NotifyTabExclusiveAccessLost() override;

  void ToggleFullscreenModeInternal(FullscreenInternalOption option,
                                    content::RenderFrameHost* requesting_frame,
                                    const int64_t display_id);
  void EnterFullscreenModeInternal(FullscreenInternalOption option,
                                   content::RenderFrameHost* requesting_frame,
                                   int64_t display_id);
  void ExitFullscreenModeInternal();
  void SetFullscreenedTab(content::WebContents* tab, const GURL& origin);

  // Returns true if |web_contents| was toggled into/out of fullscreen mode as a
  // screen-captured tab or as a content-fullscreen tab.
  // See 'FullscreenWithinTab Note'.
  bool MaybeToggleFullscreenWithinTab(content::WebContents* web_contents,
                                      bool enter_fullscreen);

  // Helper methods that should be used in a TAB context.
  GURL GetRequestingOrigin() const;
  GURL GetEmbeddingOrigin() const;

  // This is recorded when the web page requests to go fullscreen, even if the
  // fullscreen state doesn't change.
  void RecordMetricsOnFullscreenApiRequested(
      content::RenderFrameHost* requesting_frame);
  // This is recorded after entering fullscreen.
  void RecordMetricsOnEnteringFullscreen();

  // The origin of the specific frame requesting fullscreen, which may not match
  // the exclusive_access_tab()'s origin, if an embedded frame made the request.
  GURL requesting_origin_;

  // The URL of the extension which trigerred "browser fullscreen" mode.
  GURL extension_caused_fullscreen_;

  enum PriorFullscreenState {
    STATE_INVALID,
    STATE_NORMAL,
    STATE_BROWSER_FULLSCREEN,
  };
  // The state before entering tab fullscreen mode via webkitRequestFullScreen.
  // When not in tab fullscreen, it is STATE_INVALID.
  PriorFullscreenState state_prior_to_tab_fullscreen_ = STATE_INVALID;
  // The display that the window was on before entering tab fullscreen mode.
  int64_t display_id_prior_to_tab_fullscreen_ = display::kInvalidDisplayId;
  // Stores the target display when tab fullscreen is being entered.
  int64_t tab_fullscreen_target_display_id_ = display::kInvalidDisplayId;
  // True if the site has entered into fullscreen.
  bool tab_fullscreen_ = false;

  // True if this controller has toggled into tab OR browser fullscreen.
  bool toggled_into_fullscreen_ = false;

  // True if the transition to / from fullscreen has started, but not completed.
  bool started_fullscreen_transition_ = false;

  // This closure will be called after the transition to / from fullscreen
  // is completed.
  base::OnceClosure fullscreen_transition_complete_callback_;

  // Set in OnTabDeactivated(). Used to see if we're in the middle of
  // deactivation of a tab.
  raw_ptr<content::WebContents> deactivated_contents_ = nullptr;

  // Used in testing to set the state to tab fullscreen.
  bool is_tab_fullscreen_for_testing_ = false;

  // Tracks related popups that lost activation or were shown without activation
  // during content fullscreen sessions. This also activates the popups when
  // fullscreen exits, to prevent sites from creating persistent popunders.
  std::unique_ptr<PopunderPreventer> popunder_preventer_;

  base::ObserverList<FullscreenObserver> observer_list_;

  // Recorded when the controller switches to fullscreen or when the fullscreen
  // window state changes, which ever comes first.
  std::optional<base::TimeTicks> fullscreen_start_time_;

  // This is used for accessing HistoryService.
  base::CancelableTaskTracker task_tracker_;

  base::WeakPtrFactory<FullscreenController> ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_EXCLUSIVE_ACCESS_FULLSCREEN_CONTROLLER_H_
