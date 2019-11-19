// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"

#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/render_view_host.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/accessibility/view_accessibility.h"

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
    fullscreen_observer_.Add(
        browser->exclusive_access_manager()->fullscreen_controller());
  }
}

LocationBarBubbleDelegateView::~LocationBarBubbleDelegateView() = default;

void LocationBarBubbleDelegateView::ShowForReason(DisplayReason reason,
                                                  bool allow_refocus_alert) {
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
  if (GetAccessibleWindowRole() == ax::mojom::Role::kAlert ||
      GetAccessibleWindowRole() == ax::mojom::Role::kAlertDialog) {
    GetWidget()->GetRootView()->NotifyAccessibilityEvent(
        ax::mojom::Event::kAlert, true);
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
