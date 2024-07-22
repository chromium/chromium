// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"

#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_view_host.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "url/origin.h"

namespace {

ax::mojom::Role GetAccessibleRoleForReason(
    LocationBarBubbleDelegateView::DisplayReason reason) {
  if (reason == LocationBarBubbleDelegateView::USER_GESTURE) {
    // crbug.com/1132318: The bubble appears as a direct result of a user
    // action and will get focused. If we used an alert-like role, it would
    // produce an event that would cause double-speaking the bubble.
    return ax::mojom::Role::kDialog;
  }

  // crbug.com/1079320, crbug.com/1119367, crbug.com/1119734: The bubble
  // appears spontaneously over the course of the user's interaction with
  // Chrome and doesn't get focused. We need an alert-like role so the
  // corresponding event is triggered and ATs announce the bubble.
#if BUILDFLAG(IS_WIN)
  // crbug.com/1125118: Windows ATs only announce these bubbles if the alert
  // role is used, despite it not being the most appropriate choice.
  // TODO(accessibility): review the role mappings for alerts and dialogs,
  // making sure they are translated to the best candidate in each flatform
  // without resorting to hacks like this.
  return ax::mojom::Role::kAlert;
#else
  return ax::mojom::Role::kAlertDialog;
#endif
}

}  // namespace

LocationBarBubbleDelegateView::WebContentMouseHandler::WebContentMouseHandler(
    LocationBarBubbleDelegateView* bubble,
    content::WebContents* web_contents)
    : bubble_(bubble), web_contents_(web_contents) {
  DCHECK(bubble_);
  DCHECK(web_contents_);
  event_monitor_ = views::EventMonitor::CreateWindowMonitor(
      this, web_contents_->GetTopLevelNativeWindow(),
      {ui::EventType::kMousePressed, ui::EventType::kKeyPressed,
       ui::EventType::kTouchPressed});
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
    content::WebContents* web_contents,
    bool autosize)
    : BubbleDialogDelegateView(anchor_view,
                               views::BubbleBorder::TOP_RIGHT,
                               views::BubbleBorder::DIALOG_SHADOW,
                               autosize),
      WebContentsObserver(web_contents) {
  // Add observer to close the bubble if the fullscreen state changes.
  if (web_contents) {
    Browser* browser = chrome::FindBrowserWithTab(web_contents);
    // |browser| can be null in tests.
    if (browser) {
      fullscreen_observation_.Observe(
          browser->exclusive_access_manager()->fullscreen_controller());
      fullscreen_controller_ = browser->exclusive_access_manager()
                                   ->fullscreen_controller()
                                   ->GetWeakPtr();
    }
  }
  // TODO(pbos): Removing this seems to crash on linux-ozone-rel which seems
  // really wrong. If we need the accessible role before ShowForReason() we
  // can't rely on DisplayReason in there. It also really seems like this dialog
  // role should not depend on if it's showing in the foreground or not.
  SetAccessibleWindowRole(GetAccessibleRoleForReason(display_reason_));
}

LocationBarBubbleDelegateView::~LocationBarBubbleDelegateView() {
  CHECK(!fullscreen_controller_.WasInvalidated());
}

void LocationBarBubbleDelegateView::ShowForReason(DisplayReason reason,
                                                  bool allow_refocus_alert) {
  display_reason_ = reason;
  SetAccessibleWindowRole(GetAccessibleRoleForReason(reason));

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
  GetBubbleFrameView()->SetPreferredArrowAdjustment(
      views::BubbleFrameView::PreferredArrowAdjustment::kOffset);

  if (reason == USER_GESTURE) {
    GetWidget()->Show();
  } else {
    if (allow_refocus_alert) {
      // Since this will show as inactive, add a description for how to get to
      // it.
      GetWidget()->GetRootView()->GetViewAccessibility().SetDescription(
          l10n_util::GetStringUTF8(IDS_SHOW_BUBBLE_INACTIVE_DESCRIPTION));
    }
    GetWidget()->ShowInactive();
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
      !navigation_handle->IsInPrimaryMainFrame() ||
      !navigation_handle->HasCommitted()) {
    return;
  }

  // Close dialog when navigating to a different domain.
  if (!url::IsSameOriginWith(
          navigation_handle->GetPreviousPrimaryMainFrameURL(),
          navigation_handle->GetURL())) {
    CloseBubble();
  }
}

gfx::Rect LocationBarBubbleDelegateView::GetAnchorBoundsInScreen() const {
  gfx::Rect bounds = GetBoundsInScreen();
  bounds.Inset(gfx::Insets::VH(
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
  if (auto* const widget = GetWidget()) {
    widget->Close();
  }
}

void LocationBarBubbleDelegateView::SetCloseOnMainFrameOriginNavigation(
    bool close) {
  close_on_main_frame_origin_navigation_ = close;
}

bool LocationBarBubbleDelegateView::GetCloseOnMainFrameOriginNavigation()
    const {
  return close_on_main_frame_origin_navigation_;
}

BEGIN_METADATA(LocationBarBubbleDelegateView)
ADD_READONLY_PROPERTY_METADATA(bool, CloseOnMainFrameOriginNavigation)
END_METADATA
