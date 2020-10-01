// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"

#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_account_icon_container_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_view_host.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "url/origin.h"

LocationBarBubbleDelegateView::WebContentMouseHandler::WebContentMouseHandler(
    LocationBarBubbleDelegateView* bubble,
    content::WebContents* web_contents)
    : bubble_(bubble), web_contents_(web_contents) {
  DCHECK(bubble_);
  DCHECK(web_contents_);
  event_monitor_ = views::EventMonitor::CreateWindowMonitor(
      this, web_contents_->GetTopLevelNativeWindow(),
      {ui::ET_MOUSE_PRESSED, ui::ET_KEY_PRESSED, ui::ET_TOUCH_PRESSED});
}

LocationBarBubbleDelegateView::WebContentMouseHandler::
    ~WebContentMouseHandler() = default;

void LocationBarBubbleDelegateView::WebContentMouseHandler::OnEvent(
    const ui::Event& event) {
  if (event.IsKeyEvent() && event.AsKeyEvent()->key_code() != ui::VKEY_ESCAPE &&
      !web_contents_->IsFocusedElementEditable()) {
    return;
  }

  bubble_->CloseBubble();
}

LocationBarBubbleDelegateView::LocationBarBubbleDelegateView(
    views::View* anchor_view,
    content::WebContents* web_contents)
    : BubbleDialogDelegateView(anchor_view, views::BubbleBorder::TOP_RIGHT),
      WebContentsObserver(web_contents) {
  // Add observer to close the bubble if the fullscreen state changes.
  if (web_contents) {
    Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
    // |browser| can be null in tests.
    if (browser)
      fullscreen_observer_.Add(
          browser->exclusive_access_manager()->fullscreen_controller());
  }
}

LocationBarBubbleDelegateView::~LocationBarBubbleDelegateView() = default;

void LocationBarBubbleDelegateView::ShowForReason(DisplayReason reason,
                                                  bool allow_refocus_alert) {
  // These bubbles all anchor to the location bar or toolbar. We selectively
  // anchor location bar bubbles to one end or the other of the toolbar based on
  // whether their normal anchor point is visible. However, if part or all of
  // the toolbar is off-screen, we should ajust the bubbles so that they are
  // visible on the screen and not cut off.
  //
  // Note: These must be set after the bubble is created.
  // Note also: |set_adjust_if_offscreen| is disabled by default on some
  // platforms for arbitrary dialog bubbles to be consistent with platform
  // standards, however in this case there is no good reason not to ensure the
  // bubbles are displayed on-screen.
  set_adjust_if_offscreen(true);
  GetBubbleFrameView()->set_preferred_arrow_adjustment(
      views::BubbleFrameView::PreferredArrowAdjustment::kOffset);

  if (reason == USER_GESTURE) {
    GetWidget()->Show();
  } else {
    GetWidget()->ShowInactive();

    if (allow_refocus_alert) {
      // Since this widget is inactive (but shown), accessibility tools won't
      // alert the user to its presence. Accessibility tools such as screen
      // readers work by tracking system focus. Give users of these tools a hint
      // description and alert them to the presence of this widget.
      GetWidget()->GetRootView()->GetViewAccessibility().OverrideDescription(
          l10n_util::GetStringUTF8(IDS_SHOW_BUBBLE_INACTIVE_DESCRIPTION));
    }
  }
}

void LocationBarBubbleDelegateView::OnFullscreenStateChanged() {
  GetWidget()->SetVisibilityAnimationTransition(views::Widget::ANIMATE_NONE);
  CloseBubble();
}

void LocationBarBubbleDelegateView::OnVisibilityChanged(
    content::Visibility visibility) {
  if (visibility == content::Visibility::HIDDEN)
    CloseBubble();
}

void LocationBarBubbleDelegateView::WebContentsDestroyed() {
  CloseBubble();
}

void LocationBarBubbleDelegateView::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!close_on_main_frame_origin_navigation_ ||
      !navigation_handle->IsInMainFrame() ||
      !navigation_handle->HasCommitted()) {
    return;
  }

  // Close dialog when navigating to a different domain.
  if (!url::IsSameOriginWith(navigation_handle->GetPreviousURL(),
                             navigation_handle->GetURL())) {
    CloseBubble();
  }
}

gfx::Rect LocationBarBubbleDelegateView::GetAnchorBoundsInScreen() const {
  gfx::Rect bounds = GetBoundsInScreen();
  bounds.Inset(gfx::Insets(
      GetLayoutConstant(LOCATION_BAR_BUBBLE_ANCHOR_VERTICAL_INSET), 0));
  return bounds;
}

void LocationBarBubbleDelegateView::AdjustForFullscreen(
    const gfx::Rect& screen_bounds) {
  if (GetAnchorView())
    return;

  const int kBubblePaddingFromScreenEdge = 20;
  int horizontal_offset = width() / 2 + kBubblePaddingFromScreenEdge;
  const int x_pos = base::i18n::IsRTL()
                        ? (screen_bounds.x() + horizontal_offset)
                        : (screen_bounds.right() - horizontal_offset);
  SetAnchorRect(gfx::Rect(x_pos, screen_bounds.y(), 0, 0));
}

void LocationBarBubbleDelegateView::CloseBubble() {
  GetWidget()->Close();
}
