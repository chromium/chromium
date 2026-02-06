// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/overlay_base_controller.h"

#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/task_manager/web_contents_tags.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/lens/lens_preselection_bubble.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/child_process_termination_info.h"
#include "content/public/browser/render_process_host.h"
#include "net/base/network_change_notifier.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/webview/web_contents_set_background_color.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/view_class_properties.h"

namespace {

// Given a BGR bitmap, converts into a RGB bitmap instead. Returns empty bitmap
// if creation fails.
SkBitmap CreateRgbBitmap(const SkBitmap& bgr_bitmap) {
  // Convert bitmap from color type `kBGRA_8888_SkColorType` into a new Bitmap
  // with color type `kRGBA_8888_SkColorType` which will allow the bitmap to
  // render properly in the WebUI.
  sk_sp<SkColorSpace> srgb_color_space =
      bgr_bitmap.colorSpace()->makeSRGBGamma();
  SkImageInfo rgb_info = bgr_bitmap.info()
                             .makeColorType(kRGBA_8888_SkColorType)
                             .makeColorSpace(SkColorSpace::MakeSRGB());
  SkBitmap rgb_bitmap;
  rgb_bitmap.setInfo(rgb_info);
  rgb_bitmap.allocPixels(rgb_info);
  if (rgb_bitmap.writePixels(bgr_bitmap.pixmap())) {
    return rgb_bitmap;
  }

  // Bitmap creation failed.
  return SkBitmap();
}

}  // namespace

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(OverlayBaseController, kOverlayId);

OverlayBaseController::OverlayBaseController(tabs::TabInterface* tab,
                                             PrefService* pref_service)
    : tab_(tab), pref_service_(pref_service) {}

OverlayBaseController::~OverlayBaseController() {
  state_ = State::kOff;
}

bool OverlayBaseController::IsOverlayShowing() const {
  return state_ == State::kStartingWebUI || state_ == State::kOverlay;
}

bool OverlayBaseController::IsOverlayActive() const {
  return IsOverlayShowing() || state_ == State::kHidden ||
         state_ == State::kHiding || state_ == State::kIsReshowing;
}

bool OverlayBaseController::IsOverlayInitializing() {
  return state_ == State::kStartingWebUI || state_ == State::kScreenshot ||
         state_ == State::kClosingOpenedSidePanel;
}

bool OverlayBaseController::IsOverlayClosing() {
  return state_ == State::kClosing;
}

tabs::TabInterface* OverlayBaseController::GetTabInterface() {
  return tab_;
}

void OverlayBaseController::OnViewBoundsChanged(views::View* observed_view) {
  CHECK(observed_view == overlay_view_);

  // Set our view to the same bounds as the contents web view so it always
  // covers the tab contents.
  if (lens_overlay_blur_layer_delegate_) {
    // Set the blur to have the same bounds as our view, but since it is in our
    // views local coordinate system, the blur should be positioned at (0,0).
    lens_overlay_blur_layer_delegate_->layer()->SetBounds(
        overlay_view_->GetLocalBounds());
  }
}

#if BUILDFLAG(IS_MAC)
void OverlayBaseController::OnWidgetActivationChanged(views::Widget* widget,
                                                      bool active) {
  if (active && preselection_widget_) {
    // On Mac, traversing out of the preselection widget into the browser causes
    // the browser to restore its focus to the wrong place. Thus, when entering
    // the preselection widget, make sure to clear out the browser's native
    // focus. This causes the preselection widget to lose activation, so
    // reactivate it manually.
    BrowserElementsViews::From(tab_->GetBrowserWindowInterface())
        ->GetPrimaryWindowWidget()
        ->GetFocusManager()
        ->ClearNativeFocus();
    preselection_widget_->Activate();
  }
}
#endif

void OverlayBaseController::OnWidgetDestroying(views::Widget* widget) {
  preselection_widget_ = nullptr;
  preselection_widget_observer_.Reset();
}

void OverlayBaseController::OnImmersiveRevealStarted() {
  // The toolbar has began to reveal. If the overlay is showing, hide the
  // preselection bubble to ensure it doesn't cover with the toolbar UI.
  if (IsOverlayShowing()) {
    HidePreselectionBubble();
  }
}

void OverlayBaseController::OnImmersiveRevealEnded() {
  // The toolbar is no longer revealed. If the overlay is showing, reshow the
  // preselection bubble to ensure it doesn't cover with the toolbar UI.
  if (IsOverlayShowing()) {
    ShowPreselectionBubble();
  }
}

void OverlayBaseController::OnImmersiveFullscreenEntered() {
  // The browser entered immersive fullscreen. If the overlay is showing, call
  // close and reopen the preselection bubble to ensure it respositions
  // correctly.
  if (IsOverlayShowing()) {
    CloseAndReshowPreselectionBubble();
  }
}

void OverlayBaseController::OnImmersiveFullscreenExited() {
  // The browser exited immersive fullscreen. If the overlay is showing, call
  // close and reopen the preselection bubble to ensure it respositions
  // correctly.
  if (IsOverlayShowing()) {
    CloseAndReshowPreselectionBubble();
  }
}

void OverlayBaseController::RenderProcessExited(
    content::RenderProcessHost* host,
    const content::ChildProcessTerminationInfo& info) {
  // Exit early if the overlay is already closing.
  if (IsOverlayClosing()) {
    return;
  }

  // The overlay's primary main frame process has exited, either cleanly or
  // unexpectedly. Close the overlay so that the user does not get into a broken
  // state where the overlay cannot be dismissed. Note that RenderProcessExited
  // can be called during the destruction of a frame in the overlay, so it is
  // important to post a task to close the overlay to avoid double-freeing the
  // overlay's frames. See https://crbug.com/371643466.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &OverlayBaseController::RequestSyncClose, weak_factory_.GetWeakPtr(),
          info.status == base::TERMINATION_STATUS_NORMAL_TERMINATION
              ? DismissalSource::kOverlayRendererClosedNormally
              : DismissalSource::kOverlayRendererClosedUnexpectedly));
}

raw_ptr<views::View> OverlayBaseController::CreateViewForOverlay() {
  // Grab the host view for the overlay which is owned by the browser view.
  auto* const host_view =
      BrowserElementsViews::From(tab_->GetBrowserWindowInterface())
          ->GetView(GetViewContainerId());
  CHECK(host_view);

  // Setup a preselection anchor view. Usually bubbles are anchored to top
  // chrome, but top chrome is not always visible when our overlay is visible.
  // Instead of anchroing to top chrome, we anchor to this view because 1) it
  // always exists when the overlay exists and 2) it is before the WebView in
  // the view hierarchy and therefore will receive focus first when tabbing from
  // top chrome.
  std::unique_ptr<views::View> anchor_view = std::make_unique<views::View>();
  anchor_view->SetFocusBehavior(views::View::FocusBehavior::NEVER);
  preselection_widget_anchor_ = host_view->AddChildView(std::move(anchor_view));

  // Create the web view.
  std::unique_ptr<views::WebView> web_view = std::make_unique<views::WebView>(
      tab_->GetContents()->GetBrowserContext());
  content::WebContents* web_view_contents = web_view->GetWebContents();
  web_view->SetProperty(views::kElementIdentifierKey, kOverlayId);
  views::WebContentsSetBackgroundColor::CreateForWebContentsWithColor(
      web_view_contents, SK_ColorTRANSPARENT);

  // Set the label for the renderer process in Chrome Task Manager.
  task_manager::WebContentsTags::CreateForToolContents(web_view_contents,
                                                       GetToolResourceId());

  // As the embedder for the lens overlay WebUI content we must set the
  // appropriate tab interface here.
  webui::SetTabInterface(web_view_contents, GetTabInterface());

  // Set the web contents delegate to this controller so we can handle keyboard
  // events. Allow accelerators (e.g. hotkeys) to work on this web view.
  web_view->set_allow_accelerators(true);
  web_view->GetWebContents()->SetDelegate(this);

  // Load the untrusted WebUI into the web view.
  web_view->LoadInitialURL(GetInitialURL());

  overlay_web_view_ = host_view->AddChildView(std::move(web_view));
  return host_view;
}

void OverlayBaseController::InitializeScreenshot(
    const SkBitmap& bitmap,
    base::OnceCallback<void(SkBitmap)> callback) {
  // While capturing a screenshot the overlay was cancelled. Do nothing.
  if (state_ == State::kOff || IsOverlayClosing()) {
    return;
  }

  // The documentation for CopyFromSurface claims that the copy can fail, but
  // without providing information about how this can happen.
  // Supposedly IsSurfaceAvailableForCopy() should guard against this case, but
  // this is a multi-process, multi-threaded environment so there may be a
  // TOCTTOU race condition.
  if (bitmap.drawsNothing()) {
    RequestSyncClose(DismissalSource::kErrorScreenshotCreationFailed);
    return;
  }

  // The following two methods happen async to parallelize the two bottlenecks
  // in our invocation flow.
  // Create the new RGB bitmap async to prevent the main thread from blocking on
  // the encoding.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::USER_BLOCKING},
      base::BindOnce(&CreateRgbBitmap, bitmap), std::move(callback));
  ShowOverlay();

  state_ = State::kStartingWebUI;
}

void OverlayBaseController::SetLiveBlurImpl(bool enabled) {
  if (!lens_overlay_blur_layer_delegate_) {
    return;
  }

  if (enabled) {
    lens_overlay_blur_layer_delegate_->StartBackgroundImageCapture();
    return;
  }

  lens_overlay_blur_layer_delegate_->StopBackgroundImageCapture();
}

void OverlayBaseController::ShowOverlay() {
  auto* contents_web_view =
      BrowserElementsViews::From(tab_->GetBrowserWindowInterface())
          ->RetrieveView(kActiveContentsWebViewRetrievalId);
  CHECK(contents_web_view);

  NotifyIsOverlayShowing(true);
  // If the view already exists, we just need to reshow it.
  if (overlay_view_) {
    // Restore the state to show the overlay.
    overlay_view_->SetVisible(true);
    preselection_widget_anchor_->SetVisible(true);
    overlay_web_view_->SetVisible(true);
    SetOverlayRoundedCorner();

    // Restart the live blur since the view is visible again.
    SetLiveBlurImpl(should_enable_live_blur_on_show_);

    // The overlay needs to be focused on show to immediately begin
    // receiving key events.
    overlay_web_view_->RequestFocus();

    // Disable mouse and keyboard inputs to the tab contents web view. Do this
    // after the overlay takes focus. If it is done before, focus will move from
    // the contents web view to another Chrome UI element before the overlay can
    // take focus.
    contents_web_view->SetEnabled(false);
    return;
  }

  // Create the views that will house our UI.
  overlay_view_ = CreateViewForOverlay();
  overlay_view_->SetVisible(true);
  SetOverlayRoundedCorner();

  // Sanity check that the overlay view is above the contents web view.
  auto* parent_view = overlay_view_->parent();
  views::View* child_contents_view = contents_web_view;
  // TODO(crbug.com/443102583): Remove this block if overlay_view_ ends up
  // getting reparented such that it always shares a parent with
  // contents_web_view.
  // The hierarchy to access the contents web view is:
  // BrowserView->MultiContentsView->ContentsContainerView->ContentsWebView
  // Since the overlay view is parented by BrowserView, to properly pass the
  // check below, we should only compare direct children of BrowserView.
  child_contents_view = child_contents_view->parent()->parent();
  CHECK(parent_view->GetIndexOf(overlay_view_) >
        parent_view->GetIndexOf(child_contents_view));

  // Observe the overlay view to handle resizing the background blur layer.
  tab_contents_view_observer_.Observe(overlay_view_);

  // The overlay needs to be focused on show to immediately begin
  // receiving key events.
  CHECK(overlay_web_view_);
  overlay_web_view_->RequestFocus();

  // Disable mouse and keyboard inputs to the tab contents web view. Do this
  // after the overlay takes focus. If it is done before, focus will move from
  // the contents web view to another Chrome UI element before the overlay can
  // take focus.
  contents_web_view->SetEnabled(false);

  // Listen to the render process housing out overlay.
  overlay_web_view_->GetWebContents()
      ->GetPrimaryMainFrame()
      ->GetProcess()
      ->AddObserver(this);
}

void OverlayBaseController::HideOverlay() {
  // Re-enable mouse and keyboard events to the tab contents web view, and take
  // focus before the overlay view is hidden. If it is done after, focus will
  // move from the overlay view to another Chrome UI element before the contents
  // web view can take focus.
  auto* contents_web_view =
      BrowserElementsViews::From(tab_->GetBrowserWindowInterface())
          ->RetrieveView(kActiveContentsWebViewRetrievalId);
  CHECK(contents_web_view);
  contents_web_view->SetEnabled(true);
  contents_web_view->RequestFocus();

  // Hide the overlay view, but keep the web view attached to the overlay view
  // so that the overlay can be re-shown without creating a new web view.
  if (preselection_widget_anchor_) {
    preselection_widget_anchor_->SetVisible(false);
  }
  if (overlay_web_view_) {
    overlay_web_view_->SetVisible(false);
  }
  MaybeHideSharedOverlayView();

  // Save the current value of whether live blur is enabled so that it can be
  // restored when the overlay is shown again.
  if (lens_overlay_blur_layer_delegate_) {
    should_enable_live_blur_on_show_ =
        lens_overlay_blur_layer_delegate_->IsLiveBlurActive();
  }
  SetLiveBlurImpl(false);
  HidePreselectionBubble();

  NotifyIsOverlayShowing(false);
}

void OverlayBaseController::MaybeHideSharedOverlayView() {
  if (!overlay_view_) {
    return;
  }
  for (views::View* child : overlay_view_->children()) {
    if (child->GetVisible()) {
      // If any child is visible, it is being used by another tab so do not hide
      // the overlay view.
      return;
    }
  }
  overlay_view_->SetVisible(false);
}

void OverlayBaseController::HideOverlayAndSetHiddenState() {
  if (state_ != State::kHiding) {
    return;
  }
  HideOverlay();
  state_ = State::kHidden;
}

void OverlayBaseController::SetOverlayRoundedCorner() {
  CHECK(overlay_view_ && overlay_web_view_);

  const bool should_round_corner = IsResultsSidePanelShowing();
  const float radius =
      should_round_corner
          ? overlay_web_view_->GetLayoutProvider()->GetCornerRadiusMetric(
                views::ShapeContextTokens::kContentSeparatorRadius)
          : 0;
  const bool right_aligned =
      pref_service_->GetBoolean(prefs::kSidePanelHorizontalAlignment);
  const gfx::RoundedCornersF radii = gfx::RoundedCornersF{
      right_aligned ? 0 : radius, right_aligned ? radius : 0, 0, 0};

  overlay_web_view_->holder()->SetCornerRadii(radii);

  // If we show the overlay with overlay_view_ being painted to a layer,
  // there is a visual bug where the background is momentarily transparent,
  // causing flickering. When we don't want the corner to be rounded,
  // instead of setting the corner radii to 0, destroy the layer instead.
  // See crbug.com/437355402.
  if (!should_round_corner) {
    overlay_view_->DestroyLayer();
    return;
  }

  overlay_view_->SetPaintToLayer();
  overlay_view_->layer()->SetIsFastRoundedCorner(true);
  overlay_view_->layer()->SetRoundedCornerRadius(radii);
}

void OverlayBaseController::ShowPreselectionBubble() {
  // Don't show the preselection bubble if the overlay is not being shown.
  if (IsResultsSidePanelShowing()) {
    return;
  }

#if BUILDFLAG(IS_MAC)
  // On Mac, the kShowFullscreenToolbar pref is used to determine whether the
  // toolbar is always shown. This causes the toolbar to never unreveal, meaning
  // the preselection bubble will never be shown. Check for this case and show
  // the preselection bubble if needed.
  const bool always_show_toolbar =
      pref_service_->GetBoolean(prefs::kShowFullscreenToolbar);
#else
  const bool always_show_toolbar = false;
#endif  // BUILDFLAG(IS_MAC)

  if (!always_show_toolbar &&
      ImmersiveModeController::From(tab_->GetBrowserWindowInterface())
          ->IsRevealed()) {
    // If the immersive mode controller is revealing top chrome, do not show
    // the preselection bubble. The bubble will be shown once the reveal
    // finishes.
    return;
  }

  if (!preselection_widget_) {
    CHECK(preselection_widget_anchor_);
    // Setup the preselection widget.
    preselection_widget_ = views::BubbleDialogDelegateView::CreateBubble(
        std::make_unique<lens::LensPreselectionBubble>(
            tab_->GetHandle(), preselection_widget_anchor_,
            net::NetworkChangeNotifier::IsOffline(),
            /*exit_clicked_callback=*/
            base::BindRepeating(&OverlayBaseController::RequestSyncClose,
                                weak_factory_.GetWeakPtr(),
                                DismissalSource::kPreselectionToastExitButton),
            /*on_cancel_callback=*/
            base::BindOnce(&OverlayBaseController::RequestSyncClose,
                           weak_factory_.GetWeakPtr(),
                           DismissalSource::kPreselectionToastEscapeKeyPress)));
    preselection_widget_observer_.Observe(preselection_widget_);
    // Setting the parent allows focus traversal out of the preselection widget.
    preselection_widget_->SetFocusTraversableParent(
        preselection_widget_anchor_->GetWidget()->GetFocusTraversable());
    preselection_widget_->SetFocusTraversableParentView(
        preselection_widget_anchor_);
  }

  // When in fullscreen, top Chrome may cover this widget on Mac. Set the
  // z-order to floating UI element to ensure the widget is above the top
  // Chrome. Only do this if immersive mode is enabled to avoid issues with
  // the preselection widget covering other windows.
  if (ImmersiveModeController::From(tab_->GetBrowserWindowInterface())
          ->IsEnabled()) {
    preselection_widget_->SetZOrderLevel(ui::ZOrderLevel::kFloatingUIElement);
  } else {
    preselection_widget_->SetZOrderLevel(ui::ZOrderLevel::kNormal);
  }

  auto* bubble_view = static_cast<lens::LensPreselectionBubble*>(
      preselection_widget_->widget_delegate());
  bubble_view->SetCanActivate(true);

  // The bubble position is dependent on if top chrome is showing. Resize the
  // bubble to ensure the correct position is used.
  bubble_view->SizeToContents();
  // Show inactive so that the overlay remains active.
  preselection_widget_->ShowInactive();
}

void OverlayBaseController::CloseAndReshowPreselectionBubble() {
  // If the preselection bubble is already closed, do not reshow it.
  if (!preselection_widget_) {
    return;
  }
  ClosePreselectionBubbleImpl();
  ShowPreselectionBubble();
}

void OverlayBaseController::HidePreselectionBubble() {
  if (preselection_widget_) {
    // The preselection bubble remains in the browser's focus order even when it
    // is hidden, for example, when another browser tab is active. This means it
    // remains possible for the bubble to be activated by keyboard input i.e.
    // tabbing into the bubble, which unhides the bubble even on a browser tab
    // where the overlay is not being shown. Prevent this by setting the bubble
    // to non-activatable while it is hidden.
    auto* bubble_view = static_cast<lens::LensPreselectionBubble*>(
        preselection_widget_->widget_delegate());
    bubble_view->SetCanActivate(false);

    preselection_widget_->Hide();
  }
}

void OverlayBaseController::ClosePreselectionBubbleImpl() {
  if (preselection_widget_) {
    preselection_widget_->Close();
    preselection_widget_ = nullptr;
    preselection_widget_observer_.Reset();
  }
}
