// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/ui_lock_bubble.h"

#include <memory>

#include "ash/strings/grit/ash_strings.h"
#include "ash/wm/desks/desks_util.h"
#include "components/exo/surface.h"
#include "components/exo/wm_helper.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/canvas.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/metadata/metadata_impl_macros.h"
#include "ui/views/style/typography.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace exo {

UILockBubbleView::~UILockBubbleView() = default;

UILockBubbleView::UILockBubbleView(views::View* anchor_view)
    : BubbleDialogDelegateView(anchor_view, views::BubbleBorder::TOP_CENTER) {
  set_margins(views::LayoutProvider::Get()->GetInsetsMetric(
      views::InsetsMetric::INSETS_DIALOG));
  SetCanActivate(false);
  SetButtons(ui::DIALOG_BUTTON_NONE);
  set_color(SK_ColorBLACK);

  SetLayoutManager(std::make_unique<views::FillLayout>());

  views::Label* textLabel = AddChildView(
      std::make_unique<views::Label>(base::string16(l10n_util::GetStringUTF16(
          IDS_EXO_UI_LOCK_NOTIFICATION_BUBBLE_MESSAGE))));
  textLabel->SetEnabledColor(SK_ColorWHITE);
  textLabel->SetBackgroundColor(SK_ColorBLACK);
  textLabel->SetAutoColorReadabilityEnabled(true);
  textLabel->SetHorizontalAlignment(gfx::ALIGN_CENTER);
}

// TODO(crbug.com/1178861): Refactor this functionality to be provided by Ash
// directly
views::Widget* UILockBubbleView::DisplayBubble(views::View* bubbleAnchor) {
  views::Widget* bubbleViewWidget =
      views::BubbleDialogDelegateView::CreateBubble(
          new UILockBubbleView(bubbleAnchor));
  bubbleViewWidget->SetOpacity(0.5);
  bubbleViewWidget->SetZOrderLevel(ui::ZOrderLevel::kFloatingWindow);
  bubbleViewWidget->Show();
  return bubbleViewWidget;
}

BEGIN_METADATA(UILockBubbleView, views::BubbleDialogDelegateView)
END_METADATA

}  // namespace exo
