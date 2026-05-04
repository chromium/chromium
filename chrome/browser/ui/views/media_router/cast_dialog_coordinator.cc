// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_router/cast_dialog_coordinator.h"

#include <memory>

#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/media_router/cast_dialog_controller.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/browser/ui/views/media_router/cast_dialog_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "components/media_router/browser/media_router_metrics.h"
#include "ui/actions/actions.h"
#include "ui/views/view.h"
#include "ui/views/view_tracker.h"
#include "ui/views/widget/widget.h"

namespace media_router {

CastDialogCoordinator::CastDialogCoordinator() = default;
CastDialogCoordinator::~CastDialogCoordinator() = default;

void CastDialogCoordinator::ShowDialogWithToolbarAction(
    base::WeakPtr<CastDialogController> controller,
    BrowserWindowInterface* browser,
    const base::Time& start_time,
    MediaRouterDialogActivationLocation activation_location,
    AfterShownCallback after_shown_callback) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  browser_view->toolbar_button_provider()
      ->GetPinnedToolbarActions()
      ->GetBubbleAnchorAsync(
          kActionRouteMedia,
          base::BindOnce(&CastDialogCoordinator::OnBubbleAnchorVisible,
                         weak_ptr_factory_.GetWeakPtr(), controller,
                         browser->GetWeakPtr(), start_time, activation_location,
                         std::move(after_shown_callback)));
}

void CastDialogCoordinator::OnBubbleAnchorVisible(
    base::WeakPtr<CastDialogController> controller,
    base::WeakPtr<BrowserWindowInterface> browser,
    const base::Time& start_time,
    MediaRouterDialogActivationLocation activation_location,
    AfterShownCallback after_shown_callback,
    base::expected<views::BubbleAnchor, GetAnchorFailureReason> anchor) {
  if (!controller) {
    std::move(after_shown_callback).Run(ShowCastDialogStatus::kCastCanceled);
    return;
  }
  if (!browser) {
    std::move(after_shown_callback).Run(ShowCastDialogStatus::kWindowClosed);
    return;
  }
  if (anchor.has_value()) {
    auto* action_item = actions::ActionManager::Get().FindAction(
        kActionRouteMedia, browser->GetActions()->root_action_item());
    Show(anchor.value(), views::BubbleBorder::TOP_RIGHT, controller.get(),
         browser->GetProfile(), start_time, activation_location, action_item,
         std::move(after_shown_callback));

    if (action_item &&
        activation_location == MediaRouterDialogActivationLocation::TOOLBAR) {
      action_item->SetIsShowingBubble(true);
    }
  } else {
    ShowDialogCenteredForBrowserWindow(controller.get(), browser.get(),
                                       start_time, activation_location,
                                       std::move(after_shown_callback));
  }
}

void CastDialogCoordinator::ShowDialogCenteredForBrowserWindow(
    CastDialogController* controller,
    BrowserWindowInterface* browser,
    const base::Time& start_time,
    MediaRouterDialogActivationLocation activation_location,
    AfterShownCallback after_shown_callback) {
  Show(views::BubbleAnchor(
           BrowserView::GetBrowserViewForBrowser(browser)->top_container()),
       views::BubbleBorder::TOP_CENTER, controller, browser->GetProfile(),
       start_time, activation_location, nullptr,
       std::move(after_shown_callback));
}

void CastDialogCoordinator::ShowDialogCentered(
    const gfx::Rect& bounds,
    CastDialogController* controller,
    Profile* profile,
    const base::Time& start_time,
    MediaRouterDialogActivationLocation activation_location,
    AfterShownCallback after_shown_callback) {
  Show(views::BubbleAnchor(), views::BubbleBorder::TOP_CENTER, controller,
       profile, start_time, activation_location, nullptr,
       std::move(after_shown_callback));
  GetCastDialogView()->SetAnchorRect(bounds);
}

void CastDialogCoordinator::Hide() {
  if (IsShowing()) {
    cast_dialog_view_tracker_.view()->GetWidget()->Close();
  }
  // Immediately set the view tracked to nullptr. Widget will be destroyed
  // asynchronously.
  cast_dialog_view_tracker_.SetView(nullptr);
}

bool CastDialogCoordinator::IsShowing() const {
  return cast_dialog_view_tracker_.view() != nullptr;
}

void CastDialogCoordinator::Show(
    views::BubbleAnchor anchor,
    views::BubbleBorder::Arrow anchor_position,
    CastDialogController* controller,
    Profile* profile,
    const base::Time& start_time,
    MediaRouterDialogActivationLocation activation_location,
    actions::ActionItem* action_item,
    AfterShownCallback after_shown_callback) {
  DCHECK(!start_time.is_null());
  // Hide the previous dialog instance if it exists, since there can only be one
  // instance at a time.
  Hide();
  auto cast_dialog_view = std::make_unique<CastDialogView>(
      anchor, anchor_position, controller, profile, start_time,
      activation_location, action_item);
  cast_dialog_view_tracker_.SetView(cast_dialog_view.get());
  views::Widget* widget = views::BubbleDialogDelegateView::CreateBubble(
      std::move(cast_dialog_view));
  widget->Show();
  std::move(after_shown_callback).Run(ShowCastDialogStatus::kSuccess);
}

CastDialogView* CastDialogCoordinator::GetCastDialogView() {
  return static_cast<CastDialogView*>(cast_dialog_view_tracker_.view());
}

views::Widget* CastDialogCoordinator::GetCastDialogWidget() {
  return IsShowing() ? GetCastDialogView()->GetWidget() : nullptr;
}

}  // namespace media_router
