// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/sharesheet/sharesheet_bubble_view_delegate.h"

#include <utility>

#include "base/functional/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sharesheet/sharesheet_service_delegator.h"
#include "chrome/browser/ui/ash/sharesheet/sharesheet_bubble_view.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace sharesheet {

SharesheetBubbleViewDelegate::SharesheetBubbleViewDelegate(
    gfx::NativeWindow native_window,
    ::sharesheet::SharesheetServiceDelegator* sharesheet_service_delegator)
    : sharesheet_bubble_view_owned_(
          std::make_unique<SharesheetBubbleView>(native_window,
                                                 sharesheet_service_delegator)),
      sharesheet_bubble_view_(sharesheet_bubble_view_owned_.get()) {}

SharesheetBubbleViewDelegate::~SharesheetBubbleViewDelegate() = default;

void SharesheetBubbleViewDelegate::ShowBubble(
    std::vector<::sharesheet::TargetInfo> targets,
    apps::IntentPtr intent,
    ::sharesheet::DeliveredCallback delivered_callback,
    ::sharesheet::CloseCallback close_callback) {
  if (IsBubbleVisible()) {
    if (delivered_callback) {
      std::move(delivered_callback)
          .Run(::sharesheet::SharesheetResult::kErrorAlreadyOpen);
    }
    if (close_callback) {
      std::move(close_callback).Run(views::Widget::ClosedReason::kUnspecified);
    }
    return;
  }
  DCHECK(sharesheet_bubble_view_owned_);
  // The BubbleView gives its own ownership to the widget in ShowBubble(), so we
  // relinquish our ownership here.
  sharesheet_bubble_view_owned_.release()->ShowBubble(
      std::move(targets), std::move(intent), std::move(delivered_callback),
      std::move(close_callback));
}

void SharesheetBubbleViewDelegate::ShowNearbyShareBubbleForArc(
    apps::IntentPtr intent,
    ::sharesheet::DeliveredCallback delivered_callback,
    ::sharesheet::CloseCallback close_callback) {
  if (IsBubbleVisible()) {
    if (delivered_callback) {
      std::move(delivered_callback)
          .Run(::sharesheet::SharesheetResult::kErrorAlreadyOpen);
    }
    if (close_callback) {
      std::move(close_callback).Run(views::Widget::ClosedReason::kUnspecified);
    }
    return;
  }
  DCHECK(sharesheet_bubble_view_owned_);
  // The BubbleView gives its own ownership to the widget in
  // ShowNearbyShareBubbleForArc(), so we relinquish our ownership here.
  sharesheet_bubble_view_owned_.release()->ShowNearbyShareBubbleForArc(
      std::move(intent), std::move(delivered_callback),
      std::move(close_callback));
}

void SharesheetBubbleViewDelegate::OnActionLaunched(bool has_action_view) {
  DCHECK(sharesheet_bubble_view_);
  if (has_action_view) {
    sharesheet_bubble_view_->ShowActionView();
  }
}

void SharesheetBubbleViewDelegate::SetBubbleSize(int width, int height) {
  DCHECK(sharesheet_bubble_view_);
  DCHECK_GT(width, 0);
  DCHECK_GT(height, 0);
  sharesheet_bubble_view_->ResizeBubble(width, height);
}

void SharesheetBubbleViewDelegate::CloseBubble(
    ::sharesheet::SharesheetResult result) {
  views::Widget::ClosedReason reason =
      views::Widget::ClosedReason::kUnspecified;

  if (result == ::sharesheet::SharesheetResult::kSuccess) {
    reason = views::Widget::ClosedReason::kAcceptButtonClicked;
  } else if (result == ::sharesheet::SharesheetResult::kCancel) {
    reason = views::Widget::ClosedReason::kCancelButtonClicked;
  }

  DCHECK(sharesheet_bubble_view_);
  sharesheet_bubble_view_->CloseBubble(reason);
}

bool SharesheetBubbleViewDelegate::IsBubbleVisible() const {
  DCHECK(sharesheet_bubble_view_);
  return sharesheet_bubble_view_->GetWidget() &&
         sharesheet_bubble_view_->GetWidget()->IsVisible();
}

SharesheetBubbleView* SharesheetBubbleViewDelegate::GetBubbleViewForTesting() {
  return sharesheet_bubble_view_;
}

}  // namespace sharesheet
}  // namespace ash
