// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/wallet/walletable_pass_bubble_view_base.h"

#include "chrome/browser/ui/wallet/walletable_pass_bubble_controller_base.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/view.h"

namespace wallet {

namespace {

using enum WalletablePassBubbleControllerBase::WalletablePassBubbleClosedReason;

WalletablePassBubbleControllerBase::WalletablePassBubbleClosedReason
GetWalletablePassBubbleClosedReasonFromWidget(const views::Widget* widget) {
  if (!widget || !widget->IsClosed()) {
    return WalletablePassBubbleControllerBase::
        WalletablePassBubbleClosedReason::kUnknown;
  }

  switch (widget->closed_reason()) {
    case views::Widget::ClosedReason::kUnspecified:
      return kUnknown;
    case views::Widget::ClosedReason::kLostFocus:
      return kLostFocus;
    case views::Widget::ClosedReason::kEscKeyPressed:
    case views::Widget::ClosedReason::kCloseButtonClicked:
      return kClosed;
    case views::Widget::ClosedReason::kAcceptButtonClicked:
      return kAccepted;
    case views::Widget::ClosedReason::kCancelButtonClicked:
      return kDeclined;
  }
}
}  // namespace

WalletablePassBubbleViewBase::WalletablePassBubbleViewBase(
    views::View* anchor_view,
    content::WebContents* web_contents,
    WalletablePassBubbleControllerBase* controller)
    : LocationBarBubbleDelegateView(anchor_view, web_contents),
      controller_(controller->GetWalletablePassBubbleControllerBaseWeakPtr()) {}

WalletablePassBubbleViewBase::~WalletablePassBubbleViewBase() = default;

void WalletablePassBubbleViewBase::WindowClosing() {
  if (auto controller = std::exchange(controller_, nullptr)) {
    controller->OnBubbleClosed(
        GetWalletablePassBubbleClosedReasonFromWidget(GetWidget()));
  }
}

bool WalletablePassBubbleViewBase::IsMouseHovered() const {
  return views::View::IsMouseHovered();
}

BEGIN_METADATA(WalletablePassBubbleViewBase)
END_METADATA
}  // namespace wallet
