// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_TOP_CONTROLS_SLIDE_CONTROLLER_CHROMEOS_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_TOP_CONTROLS_SLIDE_CONTROLLER_CHROMEOS_H_

#include <memory>
#include <optional>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/views/frame/top_controls_slide_controller.h"
#include "ui/display/display_observer.h"
#include "ui/views/view_observer.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#endif

class BrowserView;
class TopControlsSlideTabObserver;

// Implements the Android-like slide behavior of the browser top controls for
// Chrome OS. This behavior is enabled only in tablet mode when the browser is
// not in immersive fullscreen mode. The browser top controls (a.k.a.
// top-chrome) shows and hides with page gesture scrolls.
// This controller tracks the current values of the top controls shown ratio for
// the entire browser, as well as for each tab. The values tracked per tabs
// mirror the values kept in each renderer of the corresponding tab.
//
// There are many conditions that should fully show the browser top controls if
// they're fully hidden. Examples are:
// - Switching, creating, or removing tabs.
// - Tab's renderer process crashing or hanging.
// - Focusing on an editable element within a web-page.
// - Exiting tablet mode.
// - Entering immersive fullscreen mode.
// - Page security level changes.
class TopControlsSlideControllerChromeOS : public TopControlsSlideController,
                                           public TabStripModelObserver,
                                           public display::DisplayObserver,
                                           public views::ViewObserver {
 public:
  explicit TopControlsSlideControllerChromeOS(BrowserView* browser_view);

  TopControlsSlideControllerChromeOS(
      const TopControlsSlideControllerChromeOS&) = delete;
  TopControlsSlideControllerChromeOS& operator=(
      const TopControlsSlideControllerChromeOS&) = delete;

  ~TopControlsSlideControllerChromeOS() override;

  // TopControlsSlideController:
  bool IsEnabled() const override;
  float GetShownRatio() const override;
  void SetShownRatio(content::WebContents* contents, float ratio) override;
  void OnBrowserFullscreenStateWillChange(bool new_fullscreen_state) override;
  bool DoBrowserControlsShrinkRendererSize(
      const content::WebContents* contents) const override;
  void SetTopControlsGestureScrollInProgress(bool in_progress) override;
  bool IsTopControlsGestureScrollInProgress() const override;
  bool IsTopControlsSlidingInProgress() const override;

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;
  void SetTabNeedsAttentionAt(int index, bool attention) override;

  // display::DisplayObserver:
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;
  void OnDisplayTabletStateChanged(display::TabletState state) override;

  // views::ViewObserver:
  void OnViewIsDeleting(views::View* observed_view) override;
  void OnViewFocused(views::View* observed_view) override;
  void OnViewBlurred(views::View* observed_view) override;

  // Instructs the renderer of |web_contents| to show the top controls, and also
  // updates its shown state constraints based on the current status of
  // |web_contents| (see GetBrowserControlsStateConstraints()), and the focused
  // state of the omnibox.
  // If |web_contents| is nullptr, then browser's active WebContents will be
  // updated.
  // If |animate| is true and the top controls are hidden, they will animate to
  // be fully shown.
  void UpdateBrowserControlsStateShown(content::WebContents* web_contents,
                                       bool animate);

 private:
  // Returns true if this feature can be turned on. If |fullscreen_state| is
  // supplied, it will be used in calculating the result, otherwise the current
  // fullscreen state will be queried from BrowserView. This is needed since
  // BrowserView informs us with fullscreen state changes before they happen
  // (See OnBrowserFullscreenStateWillChange()) so that we can disable the
  // sliding behavior *before* immersive mode is entered.
  bool CanEnable(std::optional<bool> fullscreen_state) const;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Called back from the ash::AccessibilityManager so that we're updated by the
  // status of Chromevox, which when enabled, sliding the top-controls should
  // be disabled. This is important for users who want to touch explore and need
  // this to be consistent.
  void OnAccessibilityStatusChanged(
      const ash::AccessibilityStatusEventDetails& event_details);
#endif

  void OnEnabledStateChanged(bool new_state);

  // Refreshes the status of the browser top controls.
  void Refresh();

  // Prepares for sliding the browser top controls by creating the necessary
  // layers, adjusting bounds and laying out the BrowserView one last time
  // before we enter the transient state, during which layer transforms are used
  // to slide the top controls.
  void OnBeginSliding();

  // Prepares for entering the steady state where the top controls reach their
  // final positions, and the |shown_ratio_| reaches either one of its terminal
  // values (1.f or 0.f). Layer transforms are reset to identity, and the
  // BrowserView is laid out into its final bounds.
  void OnEndSliding();

  // Updates whether the currently active tab has shrunk its renderer's viewport
  // size.
  void UpdateDoBrowserControlsShrinkRendererSize();

  // Returns the tab observer associated with the given |contents|, or nullptr
  // if |contents| is not observed yet.
  TopControlsSlideTabObserver* GetTabSlideObserverForWebContents(
      const content::WebContents* contents) const;

  raw_ptr<BrowserView> browser_view_;

  // The omnibox can be focused via a keyboard shortcut, in which case, we have
  // to show the top controls, and keep them shown until it's blurred.
  raw_ptr<views::View> observed_omni_box_ = nullptr;

  // Represents the per-browser (as opposed to per-tab) shown ratio of the top
  // controls that is currently applied.
  float shown_ratio_ = 1.f;

  // Indicates whether sliding the top controls with gesture scrolls is
  // currently enabled, which is true when tablet mode is enabled and the
  // browser window is not full-screened. This value is cached here since it
  // needs to be queried whenever we get an update from the renderer to adjust
  // the shown ratio. These updates result from touch gesture scrolls, so we
  // need to minimize the work we do to get these values, so sliding the browser
  // top controls feels smooth.
  bool is_enabled_ = false;

  // Whether we need to wait for the renderer to set the shown ratio to 1.f
  // before we toggle |is_enabled_| to false. It is used to postpone disabling
  // top-chrome sliding until the renderer responds so that we can make sure
  // both the renderer and the browser are both synchronized.
  bool defer_disabling_ = false;

  // Indicates whether a touch gesture scrolling is in progress. This value is
  // updated by the renderer when it receives a GestureEventAck of type either
  // kGestureScrollBegin or kGestureScrollEnd.
  bool is_gesture_scrolling_in_progress_ = false;

  // Indicates that the browser top controls are sliding up or down. This is
  // different from |is_gesture_scrolling_in_progress_| above. The top controls
  // may be sliding due an in-progress gesture scrolls or due to a renderer-
  // managed animation (such as in response to showing tabs or focusing on an
  // editable element within the page).
  // As long as this value is true, we are in a transient state, and layer
  // transforms are used to slide the top controls for efficiency. Once it turns
  // false, the layer transforms are reset to identity and the browser view is
  // re-laid out.
  bool is_sliding_in_progress_ = false;

  // Set to true when the state of this feature is temporarily disabled when
  // some display metrics change while sliding and scrolling are both in
  // progress, so that once the user lifts their finger to end the gesture
  // scrolling, the enabled state is updated to the correct value.
  bool update_state_after_gesture_scrolling_ends_ = false;

  // True when it's desired to ignore shown ratios sent by the renderer for this
  // tab. This happens when a display property changes while sliding and gesture
  // scrolling are in progress, in which case we force-set the top controls to
  // fully shown, and ignore any renderer-sent values.
  bool pause_updates_ = false;

  // We need to observe the tab's web contents to listen to events that affect
  // the browser top controls shown state for each tab.
  base::flat_map<content::WebContents*,
                 std::unique_ptr<TopControlsSlideTabObserver>>
      observed_tabs_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  base::CallbackListSubscription accessibility_status_subscription_;
#endif

  display::ScopedDisplayObserver display_observer_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_TOP_CONTROLS_SLIDE_CONTROLLER_CHROMEOS_H_
