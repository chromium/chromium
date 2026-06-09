// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/picture_in_picture/document_pip_host.h"

#include <vector>

#include "base/functional/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/picture_in_picture/document_pip_contents_view.h"
#include "chrome/browser/ui/views/picture_in_picture/document_pip_widget_delegate.h"
#include "chrome/browser/ui/views/picture_in_picture/picture_in_picture_tucker.h"
#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"
#include "content/public/browser/keyboard_event_processing_result.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/views/widget/widget.h"

WEB_CONTENTS_USER_DATA_KEY_IMPL(DocumentPipHost);

DocumentPipHost::DocumentPipHost(content::WebContents* opener_web_contents)
    : content::WebContentsUserData<DocumentPipHost>(*opener_web_contents),
      content::WebContentsObserver(opener_web_contents) {}

DocumentPipHost::~DocumentPipHost() {
  ClosePipWindow();
}

void DocumentPipHost::CreatePipWidget(
    std::unique_ptr<content::WebContents> child_web_contents,
    blink::mojom::PictureInPictureWindowOptions pip_options) {
  // Avoid creating a second widget if one already exists.
  if (widget_) {
    return;
  }

  CHECK(child_web_contents);
  pip_options_ = std::move(pip_options);

  child_web_contents->SetDelegate(this);

  widget_delegate_ = std::make_unique<DocumentPipWidgetDelegate>(
      this, std::move(child_web_contents));

  views::Widget::InitParams params(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW);
  // The Widget stores `delegate` as a raw pointer. Ownership stays with
  // `widget_delegate_` because we use CLIENT_OWNS_WIDGET without
  // SetOwnedByWidget(); the Widget will not delete the delegate.
  params.delegate = widget_delegate_.get();
  params.z_order = ui::ZOrderLevel::kFloatingWindow;
  params.visible_on_all_workspaces = true;
  params.remove_standard_frame = true;
  params.bounds = gfx::Rect(pip_options_.width, pip_options_.height);

  widget_ = std::make_unique<views::Widget>();
  widget_->Init(std::move(params));
  // Intercept external close paths (OS close button, DialogDelegate, etc.) so
  // they route through our teardown logic.
  // Safety: `this` owns `widget_` via unique_ptr, so the widget (and its
  // close callback) cannot outlive this host.
  widget_->MakeCloseSynchronous(base::BindOnce(
      &DocumentPipHost::OnWidgetCloseRequested, base::Unretained(this)));
}

Profile* DocumentPipHost::GetProfile() {
  return Profile::FromBrowserContext(web_contents()->GetBrowserContext());
}

content::WebContents* DocumentPipHost::GetOpenerWebContents() {
  return web_contents();
}

content::WebContents* DocumentPipHost::GetChildWebContents() {
  // The child is owned by the DocumentPipContentsView (a views::WebView)
  // inside the widget.
  if (widget_delegate_) {
    if (auto* contents_view = widget_delegate_->GetDocumentPipContentsView()) {
      return contents_view->web_contents();
    }
  }
  return nullptr;
}

views::Widget* DocumentPipHost::GetWidget() {
  return widget_.get();
}

const blink::mojom::PictureInPictureWindowOptions&
DocumentPipHost::GetPipOptions() const {
  return pip_options_;
}

// =============================================================================
// WebContentsObserver (observing the opener)
// =============================================================================

void DocumentPipHost::PrimaryPageChanged(content::Page& page) {
  // The opener navigated to a new primary page; close the PiP window.
  ClosePipWindow();
}

// =============================================================================
// WebContentsDelegate - Navigation & State
// =============================================================================

blink::mojom::DisplayMode DocumentPipHost::GetDisplayMode(
    const content::WebContents* web_contents) {
  return blink::mojom::DisplayMode::kPictureInPicture;
}

void DocumentPipHost::CloseContents(content::WebContents* source) {
  // The child WebContents requested closure. Tear down the PiP window and
  // child, but keep DocumentPipHost alive on the opener.
  ClosePipWindow();
}

void DocumentPipHost::NavigationStateChanged(
    content::WebContents* source,
    content::InvalidateTypes changed_flags) {
  // Update the frame view's title when the page title changes.
  if (widget_ && (changed_flags & content::INVALIDATE_TYPE_TITLE)) {
    widget_->UpdateWindowTitle();
  }
}

void DocumentPipHost::LoadingStateChanged(content::WebContents* source,
                                          bool should_show_loading_ui) {
  // No loading indicator in PiP - intentional no-op.
}

void DocumentPipHost::VisibleSecurityStateChanged(
    content::WebContents* source) {
  // This fires for the child WebContents.
  // The frame view's origin chip displays the opener's security state, not
  // the child's, so no update is needed here. The existing Browser-based PiP
  // path likewise does not react to this callback.
}

// =============================================================================
// WebContentsDelegate - Window Activation & Bounds
// =============================================================================

void DocumentPipHost::ActivateContents(content::WebContents* contents) {
  if (widget_) {
    widget_->Activate();
  }
}

bool DocumentPipHost::IsContentsActive(content::WebContents* contents) {
  // PiP has a single WebContents - it is always "active".
  return true;
}

void DocumentPipHost::SetContentsBounds(content::WebContents* source,
                                        const gfx::Rect& bounds) {
  // Record feature usage for window.moveTo()/resizeTo() calls, aligned with
  // Browser::SetContentsBounds which records the same metrics for all
  // non-normal browser types including TYPE_PICTURE_IN_PICTURE.
  std::vector<blink::mojom::WebFeature> features = {
      blink::mojom::WebFeature::kMovedOrResizedPopup};
  if (creation_timer_.Elapsed() > base::Seconds(2)) {
    features.push_back(
        blink::mojom::WebFeature::kMovedOrResizedPopup2sAfterCreation);
  }
  page_load_metrics::MetricsWebContentsObserver::RecordFeatureUsage(
      source->GetPrimaryMainFrame(), std::move(features));

  if (widget_) {
    widget_->SetBounds(bounds);
  }
}

// =============================================================================
// WebContentsDelegate - UI Events & Input
// =============================================================================

void DocumentPipHost::UpdateTargetURL(content::WebContents* source,
                                      const GURL& url) {
  // No status bar in PiP - intentional no-op.
}

void DocumentPipHost::ContentsMouseEvent(content::WebContents* source,
                                         const ui::Event& event) {
  // No status bar in PiP - intentional no-op.
}

content::KeyboardEventProcessingResult DocumentPipHost::PreHandleKeyboardEvent(
    content::WebContents* source,
    const input::NativeWebKeyboardEvent& event) {
  // Standalone PiP has no ExclusiveAccessManager (which in Browser intercepts
  // Esc to exit fullscreen, pointer lock, and keyboard lock). Fullscreen is
  // blocked for PiP windows; pointer/keyboard lock work via the renderer but
  // the browser-side "Press Esc to exit" bubble is missing.
  // Let the widget's NativeWidget handle OS accelerators.
  return content::KeyboardEventProcessingResult::NOT_HANDLED;
}

bool DocumentPipHost::HandleKeyboardEvent(
    content::WebContents* source,
    const input::NativeWebKeyboardEvent& event) {
  // No browser chrome accelerators in PiP. Unhandled keyboard events from
  // the renderer are dropped.
  return false;
}

bool DocumentPipHost::TakeFocus(content::WebContents* source, bool reverse) {
  // PiP has a single content area - nothing else to focus.
  return false;
}

// =============================================================================
// WebContentsDelegate - New Windows & Popups
// =============================================================================

content::WebContents* DocumentPipHost::AddNewContents(
    content::WebContents* source,
    std::unique_ptr<content::WebContents> new_contents,
    const GURL& target_url,
    WindowOpenDisposition disposition,
    const blink::mojom::WindowFeatures& window_features,
    bool user_gesture,
    bool* was_blocked) {
  // Forward popups to the opener's delegate so they open in the opener
  // browser, matching existing Browser-backed PiP behavior.
  content::WebContents* opener = GetOpenerWebContents();
  if (opener && opener->GetDelegate()) {
    return opener->GetDelegate()->AddNewContents(
        opener, std::move(new_contents), target_url, disposition,
        window_features, user_gesture, was_blocked);
  }
  // Opener is gone - block the popup.
  if (was_blocked) {
    *was_blocked = true;
  }
  return nullptr;
}

content::WebContents* DocumentPipHost::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params,
    base::OnceCallback<void(content::NavigationHandle&)>
        navigation_handle_callback) {
  if (params.disposition == WindowOpenDisposition::CURRENT_TAB) {
    ClosePipWindow();
    return nullptr;
  }

  // Redirect cross-window navigations to the opener browser.
  content::WebContents* opener = GetOpenerWebContents();
  if (opener && opener->GetDelegate()) {
    return opener->GetDelegate()->OpenURLFromTab(
        opener, params, std::move(navigation_handle_callback));
  }
  return nullptr;
}

bool DocumentPipHost::IsWebContentsCreationOverridden(
    content::RenderFrameHost* opener,
    content::SiteInstance* source_site_instance,
    content::mojom::WindowContainerType window_container_type,
    const GURL& opener_url,
    const std::string& frame_name,
    const GURL& target_url) {
  // Allow WebContents creation - popups are forwarded to the opener
  // in AddNewContents().
  return false;
}

void DocumentPipHost::WebContentsCreated(content::WebContents* source_contents,
                                         int opener_render_process_id,
                                         int opener_render_frame_id,
                                         const std::string& frame_name,
                                         const GURL& target_url,
                                         content::WebContents* new_contents) {
  // No-op - popup tracking is handled by AddNewContents.
}

// =============================================================================
// WebContentsDelegate - Dialogs & Logging
// =============================================================================

content::JavaScriptDialogManager* DocumentPipHost::GetJavaScriptDialogManager(
    content::WebContents* source) {
  // No dialog manager is wired up yet, so dialogs are auto-dismissed. A
  // DocumentPipDialogManagerDelegate will be added in a follow-up.
  return nullptr;
}

bool DocumentPipHost::DidAddMessageToConsole(
    content::WebContents* source,
    blink::mojom::ConsoleMessageLevel log_level,
    const std::u16string& message,
    int32_t line_no,
    const std::u16string& source_id) {
  // Don't consume the message - let the default logging mechanism handle it.
  return false;
}

// =============================================================================
// WebContentsDelegate - Window Properties & Fullscreen
// =============================================================================

bool DocumentPipHost::GetCanResize() {
  return true;
}

ui::mojom::WindowShowState DocumentPipHost::GetWindowShowState() const {
  if (!widget_) {
    return ui::mojom::WindowShowState::kDefault;
  }
  if (widget_->IsMinimized()) {
    return ui::mojom::WindowShowState::kMinimized;
  }
  if (widget_->IsMaximized()) {
    return ui::mojom::WindowShowState::kMaximized;
  }
  if (widget_->IsFullscreen()) {
    return ui::mojom::WindowShowState::kFullscreen;
  }
  return ui::mojom::WindowShowState::kNormal;
}

content::FullscreenState DocumentPipHost::GetFullscreenState(
    const content::WebContents* web_contents) const {
  // PiP windows are never fullscreen.
  return content::FullscreenState();
}

bool DocumentPipHost::IsFullscreenForTabOrPending(
    const content::WebContents* web_contents) {
  return false;
}

bool DocumentPipHost::CanEnterFullscreenModeForTab(
    content::RenderFrameHost* requesting_frame) {
  // PiP windows cannot enter fullscreen.
  return false;
}

// =============================================================================
// WebContentsDelegate - Feature Capabilities
// =============================================================================

bool DocumentPipHost::CanOverscrollContent() {
  return false;
}

bool DocumentPipHost::IsBackForwardCacheSupported(
    content::WebContents& web_contents) {
  return true;
}

bool DocumentPipHost::ShouldFocusLocationBarByDefault(
    content::WebContents* source) {
  return false;
}

bool DocumentPipHost::ShouldUseInstancedSystemMediaControls() const {
  return false;
}

content::WebContents* DocumentPipHost::GetResponsibleWebContents(
    content::WebContents* web_contents) {
  return web_contents;
}

std::string DocumentPipHost::GetTitleForMediaControls(
    content::WebContents* web_contents) {
  return std::string();
}

void DocumentPipHost::UpdatePreferredSize(content::WebContents* web_contents,
                                          const gfx::Size& pref_size) {
  // TODO(nicostap): Animate to preferred size once
  // DocumentPipBoundsController lands.
}

std::optional<gfx::Rect> DocumentPipHost::GetWindowBoundsInScreen() {
  if (widget_) {
    return widget_->GetWindowBoundsInScreen();
  }
  return std::nullopt;
}

void DocumentPipHost::BeforeUnloadFired(content::WebContents* tab,
                                        bool proceed,
                                        bool* proceed_to_fire_unload) {
  // PiP windows always proceed - no "are you sure?" interstitial.
  if (proceed_to_fire_unload) {
    *proceed_to_fire_unload = true;
  }
}

// =============================================================================
// Private helpers
// =============================================================================

void DocumentPipHost::ClosePipWindow() {
  // Clear the child's delegate before tearing down, since the host set itself
  // as delegate in CreatePipWidget().
  content::WebContents* child = GetChildWebContents();
  if (child) {
    child->SetDelegate(nullptr);
  }

  // Destroy the tucker before the widget, since it references the widget.
  tucker_.reset();
  is_tucking_forced_ = false;

  // CLIENT_OWNS_WIDGET: synchronously destroy the widget. This tears down the
  // view tree -> DocumentPipContentsView (the WebView) -> child WebContents.
  // The widget references `widget_delegate_` by raw pointer, so destroy the
  // widget first, then the delegate.
  widget_.reset();
  widget_delegate_.reset();
}

void DocumentPipHost::OnWidgetCloseRequested(
    views::Widget::ClosedReason reason) {
  ClosePipWindow();
}

// =============================================================================
// PictureInPictureWindow
// =============================================================================

void DocumentPipHost::SetForcedTucking(bool tuck) {
  if (!tucker_ && widget_) {
    tucker_ = std::make_unique<PictureInPictureTucker>(*widget_);
  }
  is_tucking_forced_ = tuck;

  // Attempting to tuck our Widget before it's been shown causes issues since
  // it may be still adjusting its bounds. Once visible, tucking will be
  // enforced.
  if (widget_ && widget_->IsVisible()) {
    if (is_tucking_forced_) {
      tucker_->Tuck();
    } else {
      tucker_->Untuck();
    }
  }
}

#if BUILDFLAG(IS_MAC)
void DocumentPipHost::OnAnyBrowserEnteredFullscreen() {
  if (widget_) {
    widget_->MoveToActiveFullscreenSpace();
  }
}
#endif
