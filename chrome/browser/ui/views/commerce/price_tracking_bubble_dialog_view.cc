// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/commerce/price_tracking_bubble_dialog_view.h"

#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view_tracker.h"
#include "ui/views/view_utils.h"

PriceTrackingBubbleDialogView::PriceTrackingBubbleDialogView(
    View* anchor_view,
    content::WebContents* web_contents,
    OnTrackPriceCallback on_track_price_callback,
    Type type)
    : LocationBarBubbleDelegateView(anchor_view, web_contents),
      type_(type),
      action_callback_(std::move(on_track_price_callback)) {
  SetShowCloseButton(true);
  SetButtons(ui::DIALOG_BUTTON_CANCEL | ui::DIALOG_BUTTON_OK);
  auto run_callback = [](PriceTrackingBubbleDialogView* bubble, bool is_track) {
    std::move(bubble->action_callback_).Run(is_track);
  };
  // TODO(meiliang@): Update the body to use the string that includes the
  // bookmark name.
  auto body = l10n_util::GetStringUTF16(
      IDS_OMNIBOX_TRACK_PRICE_DIALOG_EMAIL_DESCRIPTION);
  if (type == PriceTrackingBubbleDialogView::Type::TYPE_FUE) {
    SetTitle(l10n_util::GetStringUTF16(
        IDS_OMNIBOX_TRACK_PRICE_DIALOG_TITLE_FIRST_RUN));
    SetButtonLabel(ui::DIALOG_BUTTON_OK,
                   l10n_util::GetStringUTF16(
                       IDS_OMNIBOX_TRACK_PRICE_DIALOG_ACTION_BUTTON));
    SetButtonLabel(ui::DIALOG_BUTTON_CANCEL,
                   l10n_util::GetStringUTF16(
                       IDS_OMNIBOX_TRACK_PRICE_DIALOG_CANCEL_BUTTON));
    SetAcceptCallback(
        base::BindOnce(run_callback, base::Unretained(this), true));
  } else if (type == PriceTrackingBubbleDialogView::Type::TYPE_NORMAL) {
    SetTitle(
        l10n_util::GetStringUTF16(IDS_OMNIBOX_TRACKING_PRICE_DIALOG_TITLE));
    SetButtonLabel(ui::DIALOG_BUTTON_OK,
                   l10n_util::GetStringUTF16(
                       IDS_OMNIBOX_TRACKING_PRICE_DIALOG_ACTION_BUTTON));
    SetButtonLabel(ui::DIALOG_BUTTON_CANCEL,
                   l10n_util::GetStringUTF16(
                       IDS_OMNIBOX_TRACKING_PRICE_DIALOG_UNTRACK_BUTTON));
    SetCancelCallback(
        base::BindOnce(run_callback, base::Unretained(this), false));
    // TODO(meiliang@): Update the body to use the new normal bubble string.
  }

  SetLayoutManager(std::make_unique<views::FillLayout>());
  AddChildView(views::Builder<views::Label>()
                   .CopyAddressTo(&body_label_)
                   .SetTextStyle(views::style::STYLE_SECONDARY)
                   .SetTextContext(views::style::CONTEXT_DIALOG_BODY_TEXT)
                   .SetText(body)
                   .SetMultiLine(true)
                   .SetHorizontalAlignment(gfx::ALIGN_LEFT)
                   .Build());
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH));
}

PriceTrackingBubbleDialogView::~PriceTrackingBubbleDialogView() = default;

// PriceTrackingBubbleCoordinator
PriceTrackingBubbleCoordinator::PriceTrackingBubbleCoordinator(
    views::View* anchor_view)
    : anchor_view_(anchor_view) {}

PriceTrackingBubbleCoordinator::~PriceTrackingBubbleCoordinator() = default;

void PriceTrackingBubbleCoordinator::Show(
    content::WebContents* web_contents,
    PriceTrackingBubbleDialogView::OnTrackPriceCallback callback,
    PriceTrackingBubbleDialogView::Type type) {
  DCHECK(!tracker_.view());

  auto bubble = std::make_unique<PriceTrackingBubbleDialogView>(
      anchor_view_, web_contents, std::move(callback), type);
  tracker_.SetView(bubble.get());
  PriceTrackingBubbleDialogView::CreateBubble(std::move(bubble))->Show();
}

PriceTrackingBubbleDialogView* PriceTrackingBubbleCoordinator::GetBubble()
    const {
  return tracker_.view() ? views::AsViewClass<PriceTrackingBubbleDialogView>(
                               const_cast<views::View*>(tracker_.view()))
                         : nullptr;
}
