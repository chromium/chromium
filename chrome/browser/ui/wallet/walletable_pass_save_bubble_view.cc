// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/wallet/walletable_pass_save_bubble_view.h"

#include "chrome/browser/ui/wallet/walletable_pass_save_bubble_controller.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/layout/flex_layout.h"

namespace wallet {

WalletablePassSaveBubbleView::WalletablePassSaveBubbleView(
    views::View* anchor_view,
    content::WebContents* web_contents,
    WalletablePassSaveBubbleController* controller)
    : WalletablePassBubbleViewBase(anchor_view, web_contents, controller) {
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH));
  SetShowCloseButton(true);
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kOk) |
             static_cast<int>(ui::mojom::DialogButton::kCancel));
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetMainAxisAlignment(views::LayoutAlignment::kStart)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStretch);

  // TODO(crbug.com/451833977): Set UI once get assets.
}

WalletablePassSaveBubbleView::~WalletablePassSaveBubbleView() = default;

BEGIN_METADATA(WalletablePassSaveBubbleView)
END_METADATA

}  // namespace wallet
