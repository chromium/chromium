// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/zoom_bubble_coordinator.h"

#include <string>

#include "base/check.h"
#include "base/types/to_address.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"
#include "chrome/browser/ui/views/location_bar/zoom_bubble_view.h"
#include "chrome/browser/ui/views/page_action/page_action_view.h"
#include "chrome/browser/ui/views/zoom/zoom_view_controller.h"
#include "components/zoom/zoom_controller.h"
#include "extensions/browser/extension_zoom_request_client.h"
#include "extensions/common/extension.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/ui/fullscreen_util_mac.h"
#endif

namespace {

// Retrieves the anchor view for the zoom bubble.
views::View* GetAnchorView(BrowserView* browser_view) {
  CHECK(browser_view);

#if BUILDFLAG(IS_MAC)
  if (fullscreen_utils::IsInContentFullscreen(browser_view->browser())) {
    return nullptr;
  }
#endif

  if (!browser_view->GetWidget()->IsFullscreen() ||
      browser_view->IsToolbarVisible() ||
      ImmersiveModeController::From(browser_view->browser())->IsRevealed()) {
    return browser_view->toolbar_button_provider()->GetAnchorView(
        kActionZoomNormal);
  }
  return nullptr;
}

// Find the extension that initiated the zoom change, if any.
const extensions::ExtensionZoomRequestClient* GetExtensionZoomRequestClient(
    const content::WebContents* web_contents) {
  auto* zoom_controller =
      web_contents ? zoom::ZoomController::FromWebContents(web_contents)
                   : nullptr;
  return zoom_controller
             ? static_cast<const extensions::ExtensionZoomRequestClient*>(
                   zoom_controller->last_client())
             : nullptr;
}

void UpdateBubbleVisibilityState(Browser* browser, bool is_bubble_visible) {
  if (!browser) {
    return;
  }

  auto* action_item = actions::ActionManager::Get().FindAction(
      kActionZoomNormal, browser->browser_actions()->root_action_item());
  CHECK(action_item);
  action_item->SetIsShowingBubble(is_bubble_visible);
}

}  // namespace

DEFINE_USER_DATA(ZoomBubbleCoordinator);

ZoomBubbleCoordinator::ZoomBubbleCoordinator(BrowserView& browser_view)
    : scoped_unowned_user_data_(
          browser_view.browser()->GetUnownedUserDataHost(),
          *this),
      browser_view_(browser_view) {
  immersive_mode_observation_.Observe(
      ImmersiveModeController::From(browser_view_->browser()));
}

ZoomBubbleCoordinator::~ZoomBubbleCoordinator() {
  Hide();
}

// static
ZoomBubbleCoordinator* ZoomBubbleCoordinator::From(
    BrowserWindowInterface* browser) {
  return browser ? Get(browser->GetUnownedUserDataHost()) : nullptr;
}

void ZoomBubbleCoordinator::OnWidgetVisibilityChanged(views::Widget* widget,
                                                      bool visible) {
  CHECK(widget_observation_.IsObservingSource(widget));
  UpdateZoomBubbleStateAndIconVisibility(
      /*is_bubble_visible=*/visible);
}

void ZoomBubbleCoordinator::OnWidgetDestroying(views::Widget* widget) {
  CHECK(widget_observation_.IsObservingSource(widget));
  widget_observation_.Reset();
}

void ZoomBubbleCoordinator::Show(
    content::WebContents* contents,
    LocationBarBubbleDelegateView::DisplayReason reason) {
  if (!browser_view_->browser()->GetActiveTabInterface()) {
    return;
  }

  if (RefreshIfShowing(contents)) {
    return;
  }

  Hide();

  if (widget_observation_.IsObserving()) {
    // Closing the widget is async, but IsClosed() will return true as soon as
    // the Close() method has been invoked.
    CHECK(widget_observation_.GetSource()->IsClosed());
    widget_observation_.Reset();
  }

  auto* anchor_view = GetAnchorView(base::to_address(browser_view_));
  auto bubble_view = std::make_unique<ZoomBubbleView>(
      browser_view_->browser(), anchor_view, contents, reason);

  if (const auto* client = GetExtensionZoomRequestClient(contents)) {
    bubble_view->SetExtensionInfo(client->extension());
  }

  views::Button* button =
      browser_view_->toolbar_button_provider()->GetPageActionView(
          kActionZoomNormal);
  bubble_view->SetHighlightedButton(button);

  // If we don't anchor to anything the BrowserView is our parent. This happens
  // in fullscreen cases.
  bubble_view->set_parent_window(
      bubble_view->anchor_widget()
          ? gfx::NativeView()
          : browser_view_->GetWidget()->GetNativeView());

  ZoomBubbleView* bubble_raw = bubble_view.get();

  auto* widget =
      views::BubbleDialogDelegate::CreateBubble(std::move(bubble_view));

  widget_observation_.Observe(widget);

  if (!anchor_view && browser_view_->browser()->window()->IsFullscreen()) {
    bubble_raw->AdjustForFullscreen(
        browser_view_->browser()->window()->GetBounds());
  }

  // Do not announce hotkey for refocusing inactive Zoom bubble as it
  // disappears after a short timeout.
  bubble_raw->ShowForReason(reason, /*allow_refocus_alert=*/false);
}

void ZoomBubbleCoordinator::OnImmersiveRevealStarted() {
  Hide();
}

void ZoomBubbleCoordinator::OnImmersiveModeControllerDestroyed() {
  immersive_mode_observation_.Reset();
}

void ZoomBubbleCoordinator::Hide() {
  if (!IsShowing()) {
    return;
  }

  widget_observation_.GetSource()->Close();
}

bool ZoomBubbleCoordinator::RefreshIfShowing(content::WebContents* contents) {
  if (!CanRefresh(bubble(), contents)) {
    return false;
  }

  CHECK(bubble());
  bubble()->Refresh();
  return true;
}

ZoomBubbleView* ZoomBubbleCoordinator::bubble() {
  if (!IsShowing()) {
    return nullptr;
  }

  return static_cast<ZoomBubbleView*>(
      widget_observation_.GetSource()->GetClientContentsView());
}

bool ZoomBubbleCoordinator::CanRefresh(ZoomBubbleView* current_bubble,
                                       content::WebContents* web_contents) {
  // Can't refresh when there's not already a bubble for this tab.
  if (!current_bubble || (current_bubble->web_contents() != web_contents)) {
    return false;
  }

  // If the anchor view has changed, we must create a new bubble.
  if (current_bubble->GetAnchorView() !=
      GetAnchorView(base::to_address(browser_view_))) {
    return false;
  }

  const extensions::ExtensionZoomRequestClient* client =
      GetExtensionZoomRequestClient(web_contents);

  // Allow refreshes when the client won't create its own bubble.
  if (client && client->ShouldSuppressBubble()) {
    return true;
  }

  // Allow refreshes only when the bubble has the same attribution.
  const std::string current_extension_id =
      client ? client->extension()->id() : std::string();
  return current_bubble->extension_id() == current_extension_id;
}

void ZoomBubbleCoordinator::UpdateZoomBubbleStateAndIconVisibility(
    bool is_bubble_visible) {
  // This method can be called during browser destruction since the bubble close
  // is async.
  auto* tab_interface = browser_view_->browser()->GetActiveTabInterface();
  if (!tab_interface) {
    return;
  }

  if (!IsPageActionMigrated(PageActionIconType::kZoom)) {
    browser_view_->browser()->window()->UpdatePageActionIcon(
        PageActionIconType::kZoom);
    return;
  }

  // Update the bubble visibility state before we refresh the icon so that
  // UpdateZoomIconVisibility() sees the correct value bubble state value.
  UpdateBubbleVisibilityState(browser_view_->browser(), is_bubble_visible);

  auto* tab_feature = tab_interface->GetTabFeatures();
  CHECK(tab_feature);
  tab_feature->zoom_view_controller()->UpdatePageActionIcon(is_bubble_visible);
}
