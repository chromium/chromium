// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_router/cast_dialog_coordinator.h"

#include <memory>

#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/media_router/cast_dialog_controller.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/browser/ui/views/media_router/cast_dialog_view.h"
#include "chrome/browser/ui/views/media_router/cast_toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "components/media_router/browser/media_router_metrics.h"
#include "ui/views/view.h"
#include "ui/views/view_tracker.h"
#include "ui/views/widget/widget.h"

namespace media_router {

void CastDialogCoordinator::ShowDialogWithToolbarAction(
    CastDialogController* controller,
    Browser* browser,
    const base::Time& start_time,
    MediaRouterDialogActivationLocation activation_location) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  views::View* action_view = browser_view->toolbar()->cast_button();
  DCHECK(action_view);
  Show(action_view, views::BubbleBorder::TOP_RIGHT, controller,
       browser->profile(), start_time, activation_location);
}

void CastDialogCoordinator::ShowDialogCenteredForBrowserWindow(
    CastDialogController* controller,
    Browser* browser,
    const base::Time& start_time,
    MediaRouterDialogActivationLocation activation_location) {
  Show(BrowserView::GetBrowserViewForBrowser(browser)->top_container(),
       views::BubbleBorder::TOP_CENTER, controller, browser->profile(),
       start_time, activation_location);
}

void CastDialogCoordinator::ShowDialogCentered(
    const gfx::Rect& bounds,
    CastDialogController* controller,
    Profile* profile,
    const base::Time& start_time,
    MediaRouterDialogActivationLocation activation_location) {
  Show(/* anchor_view */ nullptr, views::BubbleBorder::TOP_CENTER, controller,
       profile, start_time, activation_location);
  GetCastDialogView()->SetAnchorRect(bounds);
}

void CastDialogCoordinator::Hide() {
  if (IsShowing())
    cast_dialog_view_tracker_.view()->GetWidget()->Close();
  // Immediately set the view tracked to nullptr. Widget will be destroyed
  // asynchronously.
  cast_dialog_view_tracker_.SetView(nullptr);
}

bool CastDialogCoordinator::IsShowing() const {
  return cast_dialog_view_tracker_.view() != nullptr;
}

void CastDialogCoordinator::Show(
    views::View* anchor_view,
    views::BubbleBorder::Arrow anchor_position,
    CastDialogController* controller,
    Profile* profile,
    const base::Time& start_time,
    MediaRouterDialogActivationLocation activation_location) {
  DCHECK(!start_time.is_null());
  // Hide the previous dialog instance if it exists, since there can only be one
  // instance at a time.
  Hide();
  auto cast_dialog_view = std::make_unique<CastDialogView>(
      anchor_view, anchor_position, controller, profile, start_time,
      activation_location);
  cast_dialog_view_tracker_.SetView(cast_dialog_view.get());
  views::Widget* widget = views::BubbleDialogDelegateView::CreateBubble(
      std::move(cast_dialog_view));
  widget->Show();
}

CastDialogView* CastDialogCoordinator::GetCastDialogView() {
  return static_cast<CastDialogView*>(cast_dialog_view_tracker_.view());
}

views::Widget* CastDialogCoordinator::GetCastDialogWidget() {
  return IsShowing() ? GetCastDialogView()->GetWidget() : nullptr;
}

}  // namespace media_router
