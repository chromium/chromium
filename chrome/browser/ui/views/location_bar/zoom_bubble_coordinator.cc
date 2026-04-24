// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/zoom_bubble_coordinator.h"

#include <string>

#include "base/check.h"
#include "base/types/to_address.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"
#include "chrome/browser/ui/views/location_bar/zoom_bubble_manager.h"
#include "chrome/browser/ui/views/location_bar/zoom_bubble_view.h"
#include "chrome/browser/ui/views/page_action/page_action_view.h"
#include "chrome/browser/ui/views/zoom/zoom_view_controller.h"
#include "components/zoom/zoom_controller.h"
#include "extensions/browser/extension_zoom_request_client.h"
#include "extensions/common/extension.h"
#include "ui/base/base_window.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/widget/widget.h"

namespace {

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

void UpdateBubbleVisibilityState(BrowserWindowInterface* browser,
                                 bool is_bubble_visible) {
  if (!browser) {
    return;
  }

  auto* action_item = actions::ActionManager::Get().FindAction(
      kActionZoomNormal, browser->GetActions()->root_action_item());
  CHECK(action_item);
  action_item->SetIsShowingBubble(is_bubble_visible);
}

}  // namespace

DEFINE_USER_DATA(ZoomBubbleCoordinator);

ZoomBubbleCoordinator::ZoomBubbleCoordinator(BrowserWindowInterface& browser,
                                             ZoomBubbleManager* manager)
    : scoped_unowned_user_data_(browser.GetUnownedUserDataHost(), *this),
      browser_(browser),
      manager_(manager) {
  if (auto* immersive_controller =
          ImmersiveModeController::From(base::to_address(browser_))) {
    immersive_mode_observation_.Observe(immersive_controller);
  }
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
  if (!browser_->GetActiveTabInterface()) {
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

  auto anchor = manager_->GetZoomBubbleAnchor();
  auto bubble_view = std::make_unique<ZoomBubbleView>(
      base::to_address(browser_), manager_, anchor, contents, reason);

  if (const auto* client = GetExtensionZoomRequestClient(contents)) {
    bubble_view->SetExtensionInfo(client->extension());
  }

  // If we don't anchor to anything the browser window is our parent. This
  // happens in fullscreen cases.
  bubble_view->set_parent_window(bubble_view->anchor_widget()
                                     ? gfx::NativeView()
                                     : manager_->GetNativeView());

  bubble_view->SetHighlightedElement(kActionItemZoomElementId);

  ZoomBubbleView* bubble_raw = bubble_view.get();

  auto* widget =
      views::BubbleDialogDelegate::CreateBubble(std::move(bubble_view));

  widget_observation_.Observe(widget);

  if (anchor.IsNull() && browser_->GetWindow()->IsFullscreen()) {
    bubble_raw->AdjustForFullscreen(browser_->GetWindow()->GetBounds());
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
  if (!current_bubble->IsSameAnchor(manager_->GetZoomBubbleAnchor())) {
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
  auto* tab_interface = browser_->GetActiveTabInterface();
  if (!tab_interface) {
    return;
  }

  if (!IsPageActionMigrated(PageActionIconType::kZoom)) {
    manager_->UpdateLegacyPageActionIcon();
    return;
  }

  // Update the bubble visibility state before we refresh the icon so that
  // UpdateZoomIconVisibility() sees the correct value bubble state value.
  UpdateBubbleVisibilityState(base::to_address(browser_), is_bubble_visible);

  auto* tab_feature = tab_interface->GetTabFeatures();
  CHECK(tab_feature);
  tab_feature->zoom_view_controller()->UpdatePageActionIconVisibility(
      is_bubble_visible);
}
