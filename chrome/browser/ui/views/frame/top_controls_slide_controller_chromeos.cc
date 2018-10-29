// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/top_controls_slide_controller_chromeos.h"

#include "chrome/browser/permissions/permission_request_manager.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "chrome/browser/ui/ash/tablet_mode_client.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/focused_node_details.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/browser_controls_state.h"
#include "extensions/common/constants.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "ui/aura/window.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/views/controls/native/native_view_host.h"

namespace {

bool IsTabletModeEnabled() {
  return TabletModeClient::Get() &&
         TabletModeClient::Get()->tablet_mode_enabled();
}

bool IsSpokenFeedbackEnabled() {
  chromeos::AccessibilityManager* accessibility_manager =
      chromeos::AccessibilityManager::Get();
  return accessibility_manager &&
         accessibility_manager->IsSpokenFeedbackEnabled();
}

// Based on the current status of |contents|, returns the browser top controls
// shown state constraints, which specifies if the top controls are allowed to
// be only shown, or either shown or hidden.
// This function is mostly similar to its corresponding Android one in Java code
// (See TabStateBrowserControlsVisibilityDelegate#canAutoHideBrowserControls()
// in TabStateBrowserControlsVisibilityDelegate.java).
content::BrowserControlsState GetBrowserControlsStateConstraints(
    content::WebContents* contents) {
  DCHECK(contents);

  if (!IsTabletModeEnabled() || contents->IsFullscreen() ||
      contents->IsFocusedElementEditable() ||
      contents->ShowingInterstitialPage() || contents->IsBeingDestroyed() ||
      contents->IsCrashed() || IsSpokenFeedbackEnabled()) {
    return content::BROWSER_CONTROLS_STATE_SHOWN;
  }

  content::NavigationEntry* entry = contents->GetController().GetVisibleEntry();
  if (!entry || entry->GetPageType() != content::PAGE_TYPE_NORMAL)
    return content::BROWSER_CONTROLS_STATE_SHOWN;

  const GURL& url = entry->GetURL();
  if (url.SchemeIs(content::kChromeUIScheme) ||
      url.SchemeIs(chrome::kChromeNativeScheme) ||
      url.SchemeIs(extensions::kExtensionScheme)) {
    return content::BROWSER_CONTROLS_STATE_SHOWN;
  }

  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());
  if (profile && search::IsNTPURL(url, profile))
    return content::BROWSER_CONTROLS_STATE_SHOWN;

  auto* helper = SecurityStateTabHelper::FromWebContents(contents);
  security_state::SecurityInfo security_info;
  helper->GetSecurityInfo(&security_info);

  switch (security_info.security_level) {
    case security_state::HTTP_SHOW_WARNING:
    case security_state::DANGEROUS:
      return content::BROWSER_CONTROLS_STATE_SHOWN;

    // Force compiler failure if new security level types were added without
    // this being updated.
    case security_state::NONE:
    case security_state::EV_SECURE:
    case security_state::SECURE:
    case security_state::SECURE_WITH_POLICY_INSTALLED_CERT:
    case security_state::SECURITY_LEVEL_COUNT:
      break;
  }

  // Keep top-chrome visible while a permission bubble is visible.
  auto* permission_manager =
      PermissionRequestManager::FromWebContents(contents);
  if (permission_manager && permission_manager->IsBubbleVisible())
    return content::BROWSER_CONTROLS_STATE_SHOWN;

  return content::BROWSER_CONTROLS_STATE_BOTH;
}

// Instructs the renderer of |web_contents| to show the top controls, and also
// updates its shown state constraints based on the current status of
// |web_contents| (see GetBrowserControlsStateConstraints() above).
void UpdateBrowserControlsStateShown(content::WebContents* web_contents,
                                     bool animate) {
  DCHECK(web_contents);

  content::RenderFrameHost* main_frame = web_contents->GetMainFrame();
  if (!main_frame)
    return;

  chrome::mojom::ChromeRenderFrameAssociatedPtr renderer;
  main_frame->GetRemoteAssociatedInterfaces()->GetInterface(&renderer);

  if (!renderer)
    return;

  const content::BrowserControlsState constraints_state =
      GetBrowserControlsStateConstraints(web_contents);

  const content::BrowserControlsState current_state =
      content::BROWSER_CONTROLS_STATE_SHOWN;
  renderer->UpdateBrowserControlsState(constraints_state, current_state,
                                       animate);
}

// Triggers a visual properties synchrnoization event on |contents|' main
// frame's view's widget.
void SynchronizeVisualProperties(content::WebContents* contents) {
  DCHECK(contents);

  content::RenderFrameHost* main_frame = contents->GetMainFrame();
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
class TopControlsSlideTabObserver : public content::WebContentsObserver,
                                    public PermissionRequestManager::Observer {
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
        PermissionRequestManager::FromWebContents(web_contents);
    if (permission_manager)
      permission_manager->AddObserver(this);
  }

  ~TopControlsSlideTabObserver() override {
    auto* permission_manager =
        PermissionRequestManager::FromWebContents(web_contents());
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
    shrink_renderer_size_ = shown_ratio_ > 0;
  }

  // content::WebContentsObserver:
  void RenderProcessGone(base::TerminationStatus status) override {
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
    UpdateBrowserControlsStateShown(false /* animate */);
    owner_->SetShownRatio(web_contents(), 1.f);
  }

  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    if (navigation_handle->IsInMainFrame() && navigation_handle->HasCommitted())
      UpdateBrowserControlsStateShown(true /* animate */);
  }

  void DidFailLoad(content::RenderFrameHost* render_frame_host,
                   const GURL& validated_url,
                   int error_code,
                   const base::string16& error_description) override {
    if (render_frame_host->IsCurrent() &&
        (render_frame_host == web_contents()->GetMainFrame())) {
      UpdateBrowserControlsStateShown(true /* animate */);
    }
  }

  void DidChangeVisibleSecurityState() override {
    UpdateBrowserControlsStateShown(true /* animate */);
  }

  void DidAttachInterstitialPage() override {
    UpdateBrowserControlsStateShown(true /* animate */);
  }

  void DidDetachInterstitialPage() override {
    UpdateBrowserControlsStateShown(true /* animate */);
  }

  // PermissionRequestManager::Observer:
  void OnBubbleAdded() override {
    UpdateBrowserControlsStateShown(true /* animate */);
  }

  void OnBubbleRemoved() override {
    // This will update the shown constraints.
    UpdateBrowserControlsStateShown(false /* animate */);
  }

 private:
  void UpdateBrowserControlsStateShown(bool animate) {
    ::UpdateBrowserControlsStateShown(web_contents(), animate);
  }

  TopControlsSlideControllerChromeOS* const owner_;

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
  // height of the browser's top controls. This value should only be updated at
  // the end of sliding, and should never change while sliding or scrolling are
  // in progress. https://crbug.com/885223.
  bool shrink_renderer_size_ = true;

  DISALLOW_COPY_AND_ASSIGN(TopControlsSlideTabObserver);
};

////////////////////////////////////////////////////////////////////////////////
// TopControlsSlideControllerChromeOS:

TopControlsSlideControllerChromeOS::TopControlsSlideControllerChromeOS(
    BrowserView* browser_view)
    : browser_view_(browser_view) {
  DCHECK(browser_view);
  DCHECK(browser_view->frame());
  DCHECK(browser_view->browser());
  DCHECK(browser_view->IsBrowserTypeNormal());
  DCHECK(browser_view->browser()->tab_strip_model());

  registrar_.Add(this, content::NOTIFICATION_FOCUS_CHANGED_IN_PAGE,
                 content::NotificationService::AllSources());

  if (TabletModeClient::Get())
    TabletModeClient::Get()->AddObserver(this);

  browser_view_->browser()->tab_strip_model()->AddObserver(this);

  chromeos::AccessibilityManager* accessibility_manager =
      chromeos::AccessibilityManager::Get();
  if (accessibility_manager) {
    accessibility_status_subscription_ =
        accessibility_manager->RegisterCallback(base::BindRepeating(
            &TopControlsSlideControllerChromeOS::OnAccessibilityStatusChanged,
            base::Unretained(this)));
  }

  OnEnabledStateChanged(CanEnable(base::nullopt));
}

TopControlsSlideControllerChromeOS::~TopControlsSlideControllerChromeOS() {
  OnEnabledStateChanged(false);

  browser_view_->browser()->tab_strip_model()->RemoveObserver(this);

  if (TabletModeClient::Get())
    TabletModeClient::Get()->RemoveObserver(this);
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

  // Make sure the value tracked per tab is always updated even when sliding is
  // disabled, so that we're always synchronized with the renderer.
  DCHECK(observed_tabs_.count(contents));

  // Note that there are two small windows of intervals between:
  // 1- When |is_gesture_scrolling_in_progress_| is set to true (i.e. received
  //    ET_GESTURE_SCROLL_BEGIN) and when |is_sliding_in_progress_| is set to
  //    true (i.e. top-chrome actually starts moving), and
  // 2- When |is_gesture_scrolling_in_progress_| is set to false (i.e.
  //    ET_GESTURE_SCROLL_END was received) and when |is_sliding_in_progress_|
  //    is set to false (i.e. top-chrome stopped moving) which can happen as the
  //    renderer continues to animate top-chrome towards fully-shown or
  //    fully-hidden after the user had lifted their fingers while the
  //    shown_ratio is still a fractional value.
  // Even during those two small windows, the
  // `DoBrowserControlsShrinkRendererSize` bit should remain unchanged from its
  // current value until sliding reaches a steady state.
  // Make sure it doesn't get updated if sliding is about to start.
  const bool sliding_or_scrolling_in_progress =
      is_gesture_scrolling_in_progress_ || is_sliding_in_progress_ ||
      (IsEnabled() && ratio != 0.f && ratio != 1.f);
  observed_tabs_[contents]->SetShownRatio(ratio,
                                          sliding_or_scrolling_in_progress);

  if (!IsEnabled()) {
    // However, if sliding is disabled, we don't update |shown_ratio_|, which is
    // the current value for the entire browser, and it must always be 1.f (i.e.
    // the top controls are fully shown).
    DCHECK_EQ(shown_ratio_, 1.f);
    return;
  }

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
    OnEnabledStateChanged(CanEnable(base::nullopt));
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

  auto iter = observed_tabs_.find(contents);
  if (iter == observed_tabs_.end()) {
    // this may be called for a new tab that hasn't attached yet to the tabstrip
    return false;
  }

  return iter->second->shrink_renderer_size();
}

void TopControlsSlideControllerChromeOS::SetTopControlsGestureScrollInProgress(
    bool in_progress) {
  is_gesture_scrolling_in_progress_ = in_progress;

  // Gesture scrolling may end before we reach a terminal value (1.f or 0.f) for
  // the |shown_ratio_|. In this case the render should continue by animating
  // the top controls towards one side. Therefore we wait for that to happen.
  if (is_gesture_scrolling_in_progress_ || !is_sliding_in_progress_)
    return;

  // Also, it may end when we are already at a terminal value of the
  // |shown_ratio_| (for example user scrolls top-chrome up until it's fully
  // hidden, keeps their finger down without movement for a bit, and then
  // releases finger). Calling refresh in this case will take care of ending the
  // sliding state (if we are in it).
  Refresh();
}

bool TopControlsSlideControllerChromeOS::IsTopControlsGestureScrollInProgress()
    const {
  return is_gesture_scrolling_in_progress_;
}

void TopControlsSlideControllerChromeOS::OnTabletModeToggled(
    bool tablet_mode_enabled) {
  OnEnabledStateChanged(CanEnable(base::nullopt));
}

void TopControlsSlideControllerChromeOS::TabInsertedAt(
    TabStripModel* tab_strip_model,
    content::WebContents* contents,
    int index,
    bool foreground) {
  DCHECK(contents);
  observed_tabs_.emplace(
      contents, std::make_unique<TopControlsSlideTabObserver>(contents, this));
}

void TopControlsSlideControllerChromeOS::TabDetachedAt(
    content::WebContents* contents,
    int previous_index,
    bool was_active) {
  DCHECK(contents);
  observed_tabs_.erase(contents);
}

void TopControlsSlideControllerChromeOS::ActiveTabChanged(
    content::WebContents* old_contents,
    content::WebContents* new_contents,
    int index,
    int reason) {
  DCHECK(new_contents);
  DCHECK(observed_tabs_.count(new_contents));
  DCHECK(!is_gesture_scrolling_in_progress_);

  // Restore the newly-activated tab's shown ratio. If this is a newly inserted
  // tab, its |shown_ratio_| is 1.0f.
  SetShownRatio(new_contents, observed_tabs_[new_contents]->shown_ratio());
  UpdateBrowserControlsStateShown(new_contents, true /* animate */);
}

void TopControlsSlideControllerChromeOS::TabReplacedAt(
    TabStripModel* tab_strip_model,
    content::WebContents* old_contents,
    content::WebContents* new_contents,
    int index) {
  DCHECK(old_contents);
  DCHECK(new_contents);
  observed_tabs_.erase(old_contents);

  DCHECK(!observed_tabs_.count(new_contents));

  observed_tabs_.emplace(
      new_contents,
      std::make_unique<TopControlsSlideTabObserver>(new_contents, this));
}

void TopControlsSlideControllerChromeOS::SetTabNeedsAttentionAt(
    int index,
    bool attention) {
  UpdateBrowserControlsStateShown(browser_view_->GetActiveWebContents(),
                                  true /* animate */);
}

void TopControlsSlideControllerChromeOS::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  // TODO(afakhry): It would be nice to add a WebContentsObserver method that
  // broadcasts this event.
  if (type != content::NOTIFICATION_FOCUS_CHANGED_IN_PAGE)
    return;

  // Make sure this notification is meant for us.
  content::WebContents* active_contents = browser_view_->GetActiveWebContents();
  content::RenderViewHost* render_view_host =
      content::Source<content::RenderViewHost>(source).ptr();
  if (!active_contents || content::WebContents::FromRenderViewHost(
                              render_view_host) != active_contents) {
    return;
  }

  content::FocusedNodeDetails* node_details =
      content::Details<content::FocusedNodeDetails>(details).ptr();
  // If a non-editable node gets focused and top-chrome is fully shown, we
  // should also update the browser controls state constraints so that
  // top-chrome is able to be hidden again.
  if (node_details->is_editable_node || shown_ratio_ == 1.f)
    UpdateBrowserControlsStateShown(active_contents, true /* animate */);
}

bool TopControlsSlideControllerChromeOS::CanEnable(
    base::Optional<bool> fullscreen_state) const {
  return IsTabletModeEnabled() &&
         !(fullscreen_state.value_or(browser_view_->IsFullscreen()));
}

void TopControlsSlideControllerChromeOS::OnAccessibilityStatusChanged(
    const chromeos::AccessibilityStatusEventDetails& event_details) {
  if (event_details.notification_type !=
      chromeos::ACCESSIBILITY_TOGGLE_SPOKEN_FEEDBACK) {
    return;
  }

  content::WebContents* active_contents = browser_view_->GetActiveWebContents();
  if (active_contents)
    UpdateBrowserControlsStateShown(active_contents, true /* animate */);
}

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
  UpdateBrowserControlsStateShown(active_contents, false /* animate */);
}

void TopControlsSlideControllerChromeOS::Refresh() {
  if (!is_gesture_scrolling_in_progress_ &&
      (shown_ratio_ == 1.f || shown_ratio_ == 0.f)) {
    // Reached a terminal value and gesture scrolling is not in progress.
    OnEndSliding();
    return;
  }

  if (!is_sliding_in_progress_)
    OnBeginSliding();

  // Using |shown_ratio_|, translate the browser top controls (using the root
  // view layer), as well as the layer of page contents native view's container
  // (which is the clipping window in the case of a NativeViewHostAura).
  // The translation is done in the Y-coordinate by an amount equal to the
  // height of the hidden part of the browser top controls.
  const int top_container_height = browser_view_->top_container()->height();
  const float y_translation = top_container_height * (shown_ratio_ - 1.f);
  gfx::Transform trans;
  trans.Translate(0, y_translation);

  // We need to transform webcontents native view's container rather than the
  // webcontents native view itself. That's because the container in the case
  // of aura is the clipping window. If we translate the webcontents native view
  // the page will appear to scroll, but clipping window will act as a static
  // view port that doesn't move with the top controls.
  DCHECK(browser_view_->contents_web_view()->holder()->GetNativeViewContainer())
      << "The web contents' native view didn't attach yet!";
  ui::Layer* contents_container_layer = browser_view_->contents_web_view()
                                            ->holder()
                                            ->GetNativeViewContainer()
                                            ->layer();
  ui::Layer* root_layer = browser_view_->frame()->GetRootView()->layer();
  std::vector<ui::Layer*> layers = {root_layer, contents_container_layer};

  for (auto* layer : layers) {
    ui::ScopedLayerAnimationSettings settings(layer->GetAnimator());
    settings.SetTransitionDuration(base::TimeDelta::FromMilliseconds(0));
    settings.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_SET_NEW_TARGET);
    layer->SetTransform(trans);
  }
}

void TopControlsSlideControllerChromeOS::OnBeginSliding() {
  DCHECK(IsEnabled());

  // It should never be called again.
  DCHECK(!is_sliding_in_progress_);

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

  // At the end of sliding, we reset the webcontents NativeViewHostAura's
  // clipping window's layer's transform to identity. From now on, the views
  // layout takes care of where everything is.
  DCHECK(browser_view_->contents_web_view()->holder()->GetNativeViewContainer())
      << "The web contents' native view didn't attach yet!";
  ui::Layer* contents_container_layer = browser_view_->contents_web_view()
                                            ->holder()
                                            ->GetNativeViewContainer()
                                            ->layer();
  gfx::Transform transform;
  contents_container_layer->SetTransform(transform);

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
    browser_view_->Layout();
  }

  // If the top controls are fully hidden, then the top container is laid out
  // such that its bounds are outside the window. The window should continue to
  // mask anything outside its bounds.
  widget_layer->SetMasksToBounds(shown_ratio_ < 1.f);
}

void TopControlsSlideControllerChromeOS::
    UpdateDoBrowserControlsShrinkRendererSize() {
  // It should never be called while gesture scrolling is still in progress.
  DCHECK(!is_gesture_scrolling_in_progress_);

  // Nor should it be called while sliding is in progress.
  DCHECK(!is_sliding_in_progress_);

  content::WebContents* active_contents = browser_view_->GetActiveWebContents();
  if (!active_contents)
    return;

  DCHECK(observed_tabs_.count(active_contents));

  observed_tabs_[active_contents]->UpdateDoBrowserControlsShrinkRendererSize();
}
