// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/top_controls_slide_controller_chromeos.h"

#include <vector>

#include "base/auto_reset.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "cc/input/browser_controls_offset_tags_info.h"
#include "cc/input/browser_controls_state.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"
#include "chrome/common/url_constants.h"
#include "components/permissions/permission_request_manager.h"
#include "components/security_state/content/security_state_tab_helper.h"
#include "content/public/browser/focused_node_details.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/common/constants.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/display/tablet_state.h"
#include "ui/views/controls/native/native_view_host.h"

namespace {

bool IsSpokenFeedbackEnabled() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  auto* accessibility_manager = ash::AccessibilityManager::Get();
  return accessibility_manager &&
         accessibility_manager->IsSpokenFeedbackEnabled();
#else
  // TODO(crbug.com/40741702): Enable accessibility (a11y) support for
  // Lacros.
  NOTIMPLEMENTED() << "Enable accessibility support for Lacros.";
  return false;
#endif
}

// Based on the current status of |contents|, returns the browser top controls
// shown state constraints, which specifies if the top controls are allowed to
// be only shown, or either shown or hidden.
// This function is mostly similar to its corresponding Android one in Java code
// (See TabStateBrowserControlsVisibilityDelegate#canAutoHideBrowserControls()
// in TabStateBrowserControlsVisibilityDelegate.java).
cc::BrowserControlsState GetBrowserControlsStateConstraints(
    content::WebContents* contents) {
  DCHECK(contents);

  if (!display::Screen::GetScreen()->InTabletMode() ||
      contents->IsFullscreen() || contents->IsFocusedElementEditable() ||
      contents->IsBeingDestroyed() || contents->IsCrashed() ||
      IsSpokenFeedbackEnabled()) {
    return cc::BrowserControlsState::kShown;
  }

  content::NavigationEntry* entry = contents->GetController().GetVisibleEntry();
  if (!entry || entry->GetPageType() != content::PAGE_TYPE_NORMAL)
    return cc::BrowserControlsState::kShown;

  const GURL& url = entry->GetURL();
  if (url.SchemeIs(content::kChromeUIScheme) ||
      url.SchemeIs(chrome::kChromeNativeScheme) ||
      url.SchemeIs(extensions::kExtensionScheme)) {
    return cc::BrowserControlsState::kShown;
  }

  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());
  if (profile && search::IsNTPOrRelatedURL(url, profile))
    return cc::BrowserControlsState::kShown;

  auto* helper = SecurityStateTabHelper::FromWebContents(contents);
  switch (helper->GetSecurityLevel()) {
    case security_state::WARNING:
    case security_state::DANGEROUS:
      return cc::BrowserControlsState::kShown;

    // Force compiler failure if new security level types were added without
    // this being updated.
    case security_state::NONE:
    case security_state::SECURE:
    case security_state::SECURE_WITH_POLICY_INSTALLED_CERT:
    case security_state::SECURITY_LEVEL_COUNT:
      break;
  }

  // Keep top-chrome visible while a permission bubble is visible.
  auto* permission_manager =
      permissions::PermissionRequestManager::FromWebContents(contents);
  if (permission_manager && permission_manager->IsRequestInProgress())
    return cc::BrowserControlsState::kShown;

  return cc::BrowserControlsState::kBoth;
}

// Triggers a visual properties synchrnoization event on |contents|' main
// frame's view's widget.
void SynchronizeVisualProperties(content::WebContents* contents) {
  DCHECK(contents);

  content::RenderFrameHost* main_frame = contents->GetPrimaryMainFrame();
  if (!main_frame)
    return;

  auto* rvh = main_frame->GetRenderViewHost();
  if (!rvh)
    return;

  auto* widget = rvh->GetWidget();
  if (!widget)
    return;

  widget->SynchronizeVisualProperties();
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// TopControlsSlideTabObserver:

// Pushes updates of the browser top controls state constraints to the renderer
// when certain events happen on the webcontents. It also keeps track of the
// current top controls shown ratio for this tab so that it stays in sync with
// the corresponding value that the tab's renderer has.
class TopControlsSlideTabObserver
    : public content::WebContentsObserver,
      public permissions::PermissionRequestManager::Observer {
 public:
  TopControlsSlideTabObserver(content::WebContents* web_contents,
                              TopControlsSlideControllerChromeOS* owner)
      : content::WebContentsObserver(web_contents), owner_(owner) {
    // This object is constructed when |web_contents| is attached to the
    // browser's tabstrip, meaning that Browser is now the delegate of
    // |web_contents|. Updating the visual properties will now sync the correct
    // top chrome height in the renderer.
    SynchronizeVisualProperties(web_contents);
    auto* permission_manager =
        permissions::PermissionRequestManager::FromWebContents(web_contents);
    if (permission_manager)
      permission_manager->AddObserver(this);
  }

  TopControlsSlideTabObserver(const TopControlsSlideTabObserver&) = delete;
  TopControlsSlideTabObserver& operator=(const TopControlsSlideTabObserver&) =
      delete;

  ~TopControlsSlideTabObserver() override {
    auto* permission_manager =
        permissions::PermissionRequestManager::FromWebContents(web_contents());
    if (permission_manager)
      permission_manager->RemoveObserver(this);
  }

  float shown_ratio() const { return shown_ratio_; }
  bool shrink_renderer_size() const { return shrink_renderer_size_; }

  void SetShownRatio(float ratio, bool sliding_or_scrolling_in_progress) {
    shown_ratio_ = ratio;
    if (!sliding_or_scrolling_in_progress)
      UpdateDoBrowserControlsShrinkRendererSize();
  }

  void UpdateDoBrowserControlsShrinkRendererSize() {
    shrink_renderer_size_ = shown_ratio_ == 1.f;
  }

  // content::WebContentsObserver:
  void PrimaryMainFrameRenderProcessGone(
      base::TerminationStatus status) override {
    // There is no renderer to communicate with, so just ensure top-chrome
    // is shown. Also the render may have crashed before resetting the gesture
    // in progress bit.
    owner_->SetTopControlsGestureScrollInProgress(false);
    owner_->SetShownRatio(web_contents(), 1.f);
  }

  void OnRendererUnresponsive(
      content::RenderProcessHost* render_process_host) override {
    // The render process might respond shortly, so instruct the renderer to
    // show top-chrome, and show it manually immediately.
    UpdateBrowserControlsStateShown(/*animate=*/false);
    owner_->SetShownRatio(web_contents(), 1.f);
  }

  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    if (navigation_handle->IsInPrimaryMainFrame() &&
        navigation_handle->HasCommitted()) {
      UpdateBrowserControlsStateShown(/*animate=*/true);
    }
  }

  void DidFailLoad(content::RenderFrameHost* render_frame_host,
                   const GURL& validated_url,
                   int error_code) override {
    if (render_frame_host->IsActive() &&
        render_frame_host->IsInPrimaryMainFrame()) {
      UpdateBrowserControlsStateShown(/*animate=*/true);
    }
  }

  void DidChangeVisibleSecurityState() override {
    UpdateBrowserControlsStateShown(/*animate=*/true);
  }

  void OnFocusChangedInPage(content::FocusedNodeDetails* details) override {
    // Even if a non-editable node gets focused, if top-chrome is fully shown,
    // we should also update the browser controls state constraints so that
    // top-chrome is able to be hidden again.
    if (details->is_editable_node || shown_ratio_ == 1.f)
      UpdateBrowserControlsStateShown(/*animate=*/true);
  }

  // PermissionRequestManager::Observer:
  void OnPromptAdded() override {
    UpdateBrowserControlsStateShown(/*animate=*/true);
  }

  void OnRequestsFinalized() override {
    // This will update the shown constraints.
    UpdateBrowserControlsStateShown(/*animate=*/false);
  }

 private:
  void UpdateBrowserControlsStateShown(bool animate) {
    owner_->UpdateBrowserControlsStateShown(web_contents(), animate);
  }

  const raw_ptr<TopControlsSlideControllerChromeOS> owner_;

  // Tracks the current shown ratio of this tab as synchronized with its
  // renderer. This is needed because when switching tabs, we must restore the
  // shown ratio of the newly-activated tab manually, not just ask the renderer
  // to animate it to shown. The renderer may never animate anything to fully
  // shown. Here's an example:
  //
  // Assume we have two tabs:
  //
  // +-------+-------+
  // | Tab 1 | Tab 2 |
  // +-------+-------+
  //
  // - User scrolls and hides top-chrome for tab 1.
  // - User presses Ctrl + Tab to switch to tab 2.
  // - We *just* ask the renderer to show top-chrome for tab 2.
  // - Tab 2's renderer thinks that shown ratio is already 1 and top-chrome is
  //   already shown.
  // - Renderer doesn't call us, and top-chrome remains hidden even though it
  //   should be shown.
  float shown_ratio_ = 1.f;

  // Indicates whether the renderer's viewport size should be shrunk by the
  // height of the browser's top controls. This value never changes while
  // sliding is in progress. It is updated only once right before sliding begins
  // and remains unchanged until sliding ends, at which point it is updated
  // right before the final layout of the BrowserView.
  // https://crbug.com/885223.
  bool shrink_renderer_size_ = true;
};

////////////////////////////////////////////////////////////////////////////////
// TopControlsSlideControllerChromeOS:

TopControlsSlideControllerChromeOS::TopControlsSlideControllerChromeOS(
    BrowserView* browser_view)
    : browser_view_(browser_view) {
  DCHECK(browser_view);
  DCHECK(browser_view->frame());
  DCHECK(browser_view->browser());
  DCHECK(browser_view->GetIsNormalType());
  DCHECK(browser_view->browser()->tab_strip_model());
  DCHECK(browser_view->GetLocationBarView());
  DCHECK(browser_view->GetLocationBarView()->omnibox_view());

  observed_omni_box_ = browser_view->GetLocationBarView()->omnibox_view();
  observed_omni_box_->AddObserver(this);

  browser_view_->browser()->tab_strip_model()->AddObserver(this);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  auto* accessibility_manager = ash::AccessibilityManager::Get();
  if (accessibility_manager) {
    accessibility_status_subscription_ =
        accessibility_manager->RegisterCallback(base::BindRepeating(
            &TopControlsSlideControllerChromeOS::OnAccessibilityStatusChanged,
            base::Unretained(this)));
  }
#endif

  OnEnabledStateChanged(CanEnable(std::nullopt));
}

TopControlsSlideControllerChromeOS::~TopControlsSlideControllerChromeOS() {
  OnEnabledStateChanged(false);

  browser_view_->browser()->tab_strip_model()->RemoveObserver(this);

  if (observed_omni_box_)
    observed_omni_box_->RemoveObserver(this);
}

bool TopControlsSlideControllerChromeOS::IsEnabled() const {
  return is_enabled_;
}

float TopControlsSlideControllerChromeOS::GetShownRatio() const {
  return shown_ratio_;
}

void TopControlsSlideControllerChromeOS::SetShownRatio(
    content::WebContents* contents,
    float ratio) {
  DCHECK(contents);

  if (pause_updates_)
    return;

  // Make sure the value tracked per tab is always updated even when sliding is
  // disabled, so that we're always synchronized with the renderer.
  DCHECK(observed_tabs_.count(contents));

  // The only times the `DoBrowserControlsShrinkRendererSize` bit is allowed to
  // change are:
  // 1) Right before we begin sliding the controls, which happens immediately
  //    after we set a fractional shown ratio.
  // 2) As soon as both gesture scrolling has finished and controls reach a
  //    terminal value (1 or 0). Note that a scroll might finish but controls
  //    might still be animating. In this case,
  //    `DoBrowserControlsShrinkRendererSize` is changed when the animation
  //    finishes.
  const bool is_enabled = IsEnabled();
  const bool sliding_or_scrolling_in_progress =
      is_gesture_scrolling_in_progress_ || is_sliding_in_progress_ ||
      (is_enabled && ratio != 0.f && ratio != 1.f);
  observed_tabs_[contents]->SetShownRatio(ratio,
                                          sliding_or_scrolling_in_progress);

  if (!is_enabled) {
    // However, if sliding is disabled, we don't update |shown_ratio_|, which is
    // the current value for the entire browser, and it must always be 1.f (i.e.
    // the top controls are fully shown).
    DCHECK_EQ(shown_ratio_, 1.f);
    return;
  }

  // Skip |shown_ratio_| update if the changes are not from the active
  // WebContents.
  if (contents != browser_view_->GetActiveWebContents())
    return;

  if (shown_ratio_ == ratio)
    return;

  shown_ratio_ = ratio;

  Refresh();

  // When disabling is deferred, we're waiting for the render to fully show top-
  // chrome, so look for a value of 1.f. The renderer may be animating towards
  // that value.
  if (defer_disabling_ && shown_ratio_ == 1.f) {
    defer_disabling_ = false;

    // Don't just set |is_enabled_| to false. Make sure it's a correct value.
    OnEnabledStateChanged(CanEnable(std::nullopt));
  }
}

void TopControlsSlideControllerChromeOS::OnBrowserFullscreenStateWillChange(
    bool new_fullscreen_state) {
  OnEnabledStateChanged(CanEnable(new_fullscreen_state));
}

bool TopControlsSlideControllerChromeOS::DoBrowserControlsShrinkRendererSize(
    const content::WebContents* contents) const {
  if (!IsEnabled())
    return false;

  auto* tab_observer = GetTabSlideObserverForWebContents(contents);
  return tab_observer && tab_observer->shrink_renderer_size();
}

void TopControlsSlideControllerChromeOS::SetTopControlsGestureScrollInProgress(
    bool in_progress) {
  if (is_gesture_scrolling_in_progress_ == in_progress)
    return;

  is_gesture_scrolling_in_progress_ = in_progress;

  if (update_state_after_gesture_scrolling_ends_) {
    DCHECK(!is_gesture_scrolling_in_progress_);
    DCHECK(pause_updates_);
    OnEnabledStateChanged(CanEnable(std::nullopt));
    update_state_after_gesture_scrolling_ends_ = false;
    pause_updates_ = false;
  }

  if (!IsEnabled())
    return;

  if (is_gesture_scrolling_in_progress_) {
    // Once gesture scrolling starts, the renderer is expected to
    // SetShownRatio() or at least call back here to reset
    // |is_gesture_scrolling_in_progress_| back to false. Nothing needs to be
    // done here.
    return;
  }

  // Regardless of the value of |is_sliding_in_progress_|, which may be:
  // - True:
  //   * We haven't reached a terminal value (1.f or 0.f) for the
  //     |shown_ratio_|. In this case the render should continue by animating
  //     the top controls towards one side. Therefore we wait for that to
  //     happen.
  //   * We are already at a terminal value of the |shown_ratio_| but sliding
  //     hasn't ended, because gesture scrolling hasn't ended (for example user
  //     scrolls top-chrome up until it's fully hidden, keeps their finger down
  //     without movement for a bit, and then releases finger).
  //
  // - False:
  //   * In tests, where flings can be very fast that the renderer sets the
  //     shown ratio from one terminal value to the opposite terminal value
  //     directly (without fractional values). In this case no sliding happens,
  //     but we still want to commit the new value of the shown ratio, once
  //     gesture scrolling ends.
  //
  // Calling refresh will take care of the above cases.
  Refresh();
}

bool TopControlsSlideControllerChromeOS::IsTopControlsGestureScrollInProgress()
    const {
  return is_gesture_scrolling_in_progress_;
}

bool TopControlsSlideControllerChromeOS::IsTopControlsSlidingInProgress()
    const {
  return is_sliding_in_progress_;
}

void TopControlsSlideControllerChromeOS::OnDisplayTabletStateChanged(
    display::TabletState state) {
  switch (state) {
    case display::TabletState::kInTabletMode:
    case display::TabletState::kInClamshellMode:
      OnEnabledStateChanged(CanEnable(std::nullopt));
      return;
    case display::TabletState::kEnteringTabletMode:
    case display::TabletState::kExitingTabletMode:
      break;
  }
}

void TopControlsSlideControllerChromeOS::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (change.type() == TabStripModelChange::kInserted) {
    for (const auto& contents : change.GetInsert()->contents) {
      observed_tabs_.emplace(contents.contents,
                             std::make_unique<TopControlsSlideTabObserver>(
                                 contents.contents, this));
    }
  } else if (change.type() == TabStripModelChange::kRemoved) {
    for (const auto& contents : change.GetRemove()->contents)
      observed_tabs_.erase(contents.contents);
  } else if (change.type() == TabStripModelChange::kReplaced) {
    auto* replace = change.GetReplace();
    observed_tabs_.erase(replace->old_contents);
    DCHECK(!observed_tabs_.count(replace->new_contents));
    observed_tabs_.emplace(replace->new_contents,
                           std::make_unique<TopControlsSlideTabObserver>(
                               replace->new_contents, this));
  }

  if (tab_strip_model->empty() || !selection.active_tab_changed())
    return;

  content::WebContents* new_active_contents = selection.new_contents;
  DCHECK(observed_tabs_.count(new_active_contents));

  // Restore the newly-activated tab's shown ratio. If this is a newly inserted
  // tab, its |shown_ratio_| is 1.0f.
  SetShownRatio(new_active_contents,
                observed_tabs_[new_active_contents]->shown_ratio());
  UpdateBrowserControlsStateShown(new_active_contents, /*animate=*/true);
}

void TopControlsSlideControllerChromeOS::SetTabNeedsAttentionAt(
    int index,
    bool attention) {
  UpdateBrowserControlsStateShown(/*web_contents=*/nullptr, /*animate=*/true);
}

void TopControlsSlideControllerChromeOS::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t changed_metrics) {
  if (!IsEnabled())
    return;

  if (!is_sliding_in_progress_ || !is_gesture_scrolling_in_progress_)
    return;

  // If any of the below display metrics changes while both sliding and gesture
  // scrolling are in progress, we force-set the top controls to be fully shown,
  // and temporarily disables the state of the top controls sliding feature
  // until the user lifts their finger to end gesture scrolling, at which point
  // we set it back to its correct value.
  // This is necessary, since this way the browser view will layout properly,
  // avoiding having a broken page or a broken browser view if one of the below
  // changes happen while the top controls are not in a steady state.
  constexpr int kCheckedMetrics =
      display::DisplayObserver::DISPLAY_METRIC_BOUNDS |
      display::DisplayObserver::DISPLAY_METRIC_WORK_AREA |
      display::DisplayObserver::DISPLAY_METRIC_DEVICE_SCALE_FACTOR |
      display::DisplayObserver::DISPLAY_METRIC_ROTATION |
      display::DisplayObserver::DISPLAY_METRIC_PRIMARY |
      display::DisplayObserver::DISPLAY_METRIC_MIRROR_STATE;

  if ((changed_metrics & kCheckedMetrics) == 0)
    return;

  if (browser_view_->GetNativeWindow()->GetHost()->GetDisplayId() !=
      display.id()) {
    return;
  }

  content::WebContents* active_contents = browser_view_->GetActiveWebContents();
  if (!active_contents)
    return;

  update_state_after_gesture_scrolling_ends_ = true;
  {
    // Setting |is_gesture_scrolling_in_progress_| to false temporarily will end
    // the sliding when we set the shown ratio to a terminal value of 1.f.
    base::AutoReset<bool> resetter{&is_gesture_scrolling_in_progress_, false};
    SetShownRatio(active_contents, 1.f);
  }
  pause_updates_ = true;
  OnEnabledStateChanged(false);
}

void TopControlsSlideControllerChromeOS::OnViewIsDeleting(
    views::View* observed_view) {
  DCHECK_EQ(observed_view, observed_omni_box_);
  observed_omni_box_ = nullptr;
  UpdateBrowserControlsStateShown(/*web_contents=*/nullptr, /*animate=*/true);
}

void TopControlsSlideControllerChromeOS::OnViewFocused(
    views::View* observed_view) {
  DCHECK_EQ(observed_view, observed_omni_box_);
  UpdateBrowserControlsStateShown(/*web_contents=*/nullptr, /*animate=*/true);
}

void TopControlsSlideControllerChromeOS::OnViewBlurred(
    views::View* observed_view) {
  DCHECK_EQ(observed_view, observed_omni_box_);
  UpdateBrowserControlsStateShown(/*web_contents=*/nullptr, /*animate=*/true);
}

void TopControlsSlideControllerChromeOS::UpdateBrowserControlsStateShown(
    content::WebContents* web_contents,
    bool animate) {
  web_contents =
      web_contents ? web_contents : browser_view_->GetActiveWebContents();
  if (!web_contents)
    return;

  // If the omnibox is focused, then the top controls should be constrained to
  // remain fully shown until the omnibox is blurred.
  const cc::BrowserControlsState constraints_state =
      observed_omni_box_ && observed_omni_box_->HasFocus()
          ? cc::BrowserControlsState::kShown
          : GetBrowserControlsStateConstraints(web_contents);

  const cc::BrowserControlsState current_state =
      cc::BrowserControlsState::kShown;
  web_contents->UpdateBrowserControlsState(constraints_state, current_state,
                                           animate, std::nullopt);
}

bool TopControlsSlideControllerChromeOS::CanEnable(
    std::optional<bool> fullscreen_state) const {
  return display::Screen::GetScreen()->InTabletMode() &&
         !(fullscreen_state.value_or(browser_view_->IsFullscreen()));
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void TopControlsSlideControllerChromeOS::OnAccessibilityStatusChanged(
    const ash::AccessibilityStatusEventDetails& event_details) {
  if (event_details.notification_type !=
      ash::AccessibilityNotificationType::kToggleSpokenFeedback) {
    return;
  }

  UpdateBrowserControlsStateShown(/*web_contents=*/nullptr, /*animate=*/true);
}
#endif

void TopControlsSlideControllerChromeOS::OnEnabledStateChanged(bool new_state) {
  if (new_state == is_enabled_)
    return;

  is_enabled_ = new_state;

  content::WebContents* active_contents = browser_view_->GetActiveWebContents();
  if (!active_contents)
    return;

  if (!new_state && shown_ratio_ < 1.f) {
    // We should never set the shown ratio immediately here, rather ask the
    // renderer to show top-chrome without animation. Since this will happen
    // later asynchronously, we need to defer the enabled status update until
    // we get called by the renderer to set the shown ratio to 1.f. Otherwise
    // we will layout the page to a smaller height before the renderer gets
    // to know that it needs to update the shown ratio to 1.f.
    // https://crbug.com/884453.
    is_enabled_ = true;
    defer_disabling_ = true;
  } else {
    defer_disabling_ = false;

    // Now that the state of this feature is changed, force the renderer to get
    // the new top controls height by triggering a visual properties
    // synchrnoization event.
    SynchronizeVisualProperties(active_contents);
  }

  // This will also update the browser controls state constraints in the render
  // now that the state changed.
  UpdateBrowserControlsStateShown(/*web_contents=*/nullptr, /*animate=*/false);
}

void TopControlsSlideControllerChromeOS::Refresh() {
  const bool got_a_terminal_shown_ratio =
      (shown_ratio_ == 1.f || shown_ratio_ == 0.f);
  if (!is_gesture_scrolling_in_progress_ && got_a_terminal_shown_ratio) {
    // Reached a terminal value and gesture scrolling is not in progress.
    OnEndSliding();
    return;
  }

  if (!is_sliding_in_progress_) {
    if (got_a_terminal_shown_ratio) {
      // Don't start sliding until we receive a fractional shown ratio.
      return;
    }

    OnBeginSliding();
  }

  // Using |shown_ratio_|, translate the browser top controls (using the root
  // view layer), as well as the layer of page contents native view's container
  // (which is the clipping window in the case of a NativeViewHostAura).
  // The translation is done in the Y-coordinate by an amount equal to the
  // height of the hidden part of the browser top controls.
  const int top_container_height = browser_view_->top_container()->height();
  const float y_translation = top_container_height * (shown_ratio_ - 1.f);
  gfx::Transform trans;
  trans.Translate(0, y_translation);

  ui::Layer* root_layer = browser_view_->frame()->GetRootView()->layer();
  std::vector<ui::Layer*> layers = {root_layer};
  // We need to transform all the native views' containers of all the attached
  // NativeViewHosts to this BrowserView, rather than the NativeViewHosts
  // themselves. The attached NativeViewHosts can be active tab's WebContents,
  // and the webui tabstrip (if enabled). This is because for example in the
  // case of the tab's WebContents, the container in the case of aura is the
  // clipping window. If we translate the WebContents native view the page will
  // appear to scroll, but clipping window will act as a static/ view port that
  // doesn't move with the top controls.
  for (auto* native_view_host :
       browser_view_->GetNativeViewHostsForTopControlsSlide()) {
    DCHECK(native_view_host->GetNativeViewContainer())
        << "The native view didn't attach yet to the NativeViewHost!";
    layers.push_back(native_view_host->GetNativeViewContainer()->layer());
  }

  for (auto* layer : layers) {
    ui::ScopedLayerAnimationSettings settings(layer->GetAnimator());
    settings.SetTransitionDuration(base::Milliseconds(0));
    settings.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_SET_NEW_TARGET);
    layer->SetTransform(trans);
  }
}

void TopControlsSlideControllerChromeOS::OnBeginSliding() {
  DCHECK(IsEnabled());

  // It should never be called again.
  DCHECK(!is_sliding_in_progress_);

  // Explicitly update the `DoBrowserControlsShrinkRendererSize` bit here before
  // we begin sliding, and before we resize the browser view below, which will
  // result in changing the bounds of the `BrowserView::contents_web_view_`,
  // causing the RednerWidgetHost to request the new value of the
  // `DoBrowserControlsShrinkRendererSize` bit, which should be false from now
  // on, during and after sliding, until only sliding ends and the top controls
  // are fully shown.
  UpdateDoBrowserControlsShrinkRendererSize();

  is_sliding_in_progress_ = true;

  BrowserFrame* browser_frame = browser_view_->frame();
  views::View* root_view = browser_frame->GetRootView();
  // We paint to layer to be able to efficiently translate the browser
  // top-controls without having to adjust the bounds of the views which trigger
  // re-layouts and re-paints, which makes scrolling feel laggy.
  root_view->SetPaintToLayer();
  // We need to make the layer non-opaque as the tabstrip has transparent areas
  // (where there are no tabs) which shows the frame header from underneath it.
  // Making the root view paint to a layer will always produce garbage and
  // artifacts while the layer is being scrolled if it's left to be opaque.
  // Making it non-opaque fixes this issue.
  root_view->layer()->SetFillsBoundsOpaquely(false);

  // We need to fix the order of the layers after making the root view paint to
  // layer. Otherwise, the root view's layer will show on top of the contents'
  // native view's layer and cover it.
  browser_frame->ReorderNativeViews();

  ui::Layer* widget_layer = browser_frame->GetLayer();

  // OnBeginSliding() means we are in a transient state (i.e. the top controls
  // didn't reach its final state of either fully shown or fully hidden). During
  // this state, we resize the widget's root view to be bigger in height so the
  // contents can take up more space, and slidding top-chrome doesn't result in
  // showing clipped web contents.
  // This resize will trigger a relayout on the BrowserView which will take care
  // of positioning everything correctly (See BrowserViewLayout).
  // Note: It's ok to trigger a layout at the beginning and ending of the slide
  // but not in-between. Layers transforms handles the in-between.
  gfx::Rect root_bounds = root_view->bounds();
  const int top_container_height = browser_view_->top_container()->height();
  const int new_height = widget_layer->bounds().height() + top_container_height;
  root_bounds.set_height(new_height);
  root_view->SetBoundsRect(root_bounds);
  // Changing the bounds will have triggered an InvalidateLayout() on
  // NativeViewHost. InvalidateLayout() results in layout being performed later,
  // after transforms are set. NativeViewHostAura calculates the bounds of the
  // window using transforms. By calling LayoutRootViewIfNecessary() we force
  // the layout now, before any transforms are installed. To do otherwise
  // results in NativeViewHost positioning the WebContents at the wrong
  // location.
  // TODO(crbug.com/40622302): this is rather fragile, and the code should
  // deal with layout being performed during the slide.
  root_view->GetWidget()->LayoutRootViewIfNecessary();

  // We don't want anything to show outside the browser window's bounds.
  widget_layer->SetMasksToBounds(true);
}

void TopControlsSlideControllerChromeOS::OnEndSliding() {
  DCHECK(IsEnabled());

  // This should only be called at terminal values of the |shown_ratio_|.
  DCHECK(shown_ratio_ == 1.f || shown_ratio_ == 0.f);

  // It should never be called while gesture scrolling is still in progress.
  DCHECK(!is_gesture_scrolling_in_progress_);

  // If disabling is deferred, sliding should end only when top-chrome is fully
  // shown.
  DCHECK(!defer_disabling_ || (shown_ratio_ == 1.f));

  // It can, however, be called when sliding is not in progress as a result of
  // Setting the value directly (for example due to renderer crash), or a direct
  // call from the renderer to set the shown ratio to a terminal value.
  is_sliding_in_progress_ = false;

  // At the end of sliding, we reset the transforms of all the attached
  // NativeViewHostAuras' clipping windows' layers to identity. From now on, the
  // views layout takes care of where everything is.
  const gfx::Transform identity_transform;
  for (auto* native_view_host :
       browser_view_->GetNativeViewHostsForTopControlsSlide()) {
    DCHECK(native_view_host->GetNativeViewContainer())
        << "The native view didn't attach yet to the NativeViewHost!";
    native_view_host->GetNativeViewContainer()->layer()->SetTransform(
        identity_transform);
  }

  BrowserFrame* browser_frame = browser_view_->frame();
  views::View* root_view = browser_frame->GetRootView();
  root_view->DestroyLayer();

  ui::Layer* widget_layer = browser_frame->GetLayer();

  // Note the difference between the below root view resize, and the
  // corresponding one in OnBeginSliding() above. Here we have reached a steady
  // terminal (|shown_ratio_| is either 1.f or 0.f) state, which means the
  // height of the root view should be restored to the height of the widget.
  // Note: It's ok to trigger a layout at the beginning and ending of the slide
  // but not in-between. Layers transforms handles the in-between.
  auto root_bounds = root_view->bounds();
  const int original_height = root_bounds.height();
  const int new_height = widget_layer->bounds().height();

  // This must be updated here **before** the browser is laid out, since the
  // renderer (as a result of the layout) may query this value, and hence it
  // should be correct.
  UpdateDoBrowserControlsShrinkRendererSize();

  // We need to guarantee a browser view re-layout, but want to avoid doing that
  // twice.
  if (new_height != original_height) {
    root_bounds.set_height(new_height);
    root_view->SetBoundsRect(root_bounds);
  } else {
    // This can happen when setting the shown ratio directly from one terminal
    // value to the opposite. The height of the root view doesn't change, but
    // the browser view must be re-laid out.
    browser_view_->DeprecatedLayoutImmediately();
  }

  // If the top controls are fully hidden, then the top container is laid out
  // such that its bounds are outside the window. The window should continue to
  // mask anything outside its bounds.
  widget_layer->SetMasksToBounds(shown_ratio_ < 1.f);
}

void TopControlsSlideControllerChromeOS::
    UpdateDoBrowserControlsShrinkRendererSize() {
  // It should never be called while sliding is in progress.
  DCHECK(!is_sliding_in_progress_);

  content::WebContents* active_contents = browser_view_->GetActiveWebContents();
  if (!active_contents)
    return;

  auto* tab_observer = GetTabSlideObserverForWebContents(active_contents);
  if (tab_observer)
    tab_observer->UpdateDoBrowserControlsShrinkRendererSize();
}

TopControlsSlideTabObserver*
TopControlsSlideControllerChromeOS::GetTabSlideObserverForWebContents(
    const content::WebContents* contents) const {
  auto iter = observed_tabs_.find(contents);
  if (iter == observed_tabs_.end()) {
    // this may be called for a new tab that hasn't attached yet to the
    // tabstrip.
    return nullptr;
  }
  return iter->second.get();
}
