// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/wallet/walletable_pass_consent_bubble_view.h"

#include "chrome/browser/ui/wallet/walletable_pass_consent_bubble_controller.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/layout/flex_layout.h"

namespace wallet {

WalletablePassConsentBubbleView::WalletablePassConsentBubbleView(
    views::View* anchor_view,
    content::WebContents* web_contents,
    WalletablePassConsentBubbleController* controller)
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

  // TODO(crbug.com/445826875): Set UI once get assets.
}

WalletablePassConsentBubbleView::~WalletablePassConsentBubbleView() = default;

BEGIN_METADATA(WalletablePassConsentBubbleView)
END_METADATA

}  // namespace wallet
