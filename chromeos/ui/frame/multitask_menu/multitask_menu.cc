// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ui/frame/multitask_menu/multitask_menu.h"

#include <memory>

#include "base/check.h"
#include "chromeos/ui/frame/multitask_menu/float_controller_base.h"
#include "chromeos/ui/frame/multitask_menu/multitask_menu_view.h"
#include "ui/views/layout/table_layout.h"

namespace chromeos {

namespace {
constexpr int kMultitaskMenuBubbleCornerRadius = 8;
constexpr int kRowPadding = 16;
constexpr int KMultitaskMenuWidth = 270;
constexpr int kMultitaskMenuHeight = 248;

}  // namespace

MultitaskMenu::MultitaskMenu(views::View* anchor, aura::Window* parent_window) {
  DCHECK(parent_window);
  SetAnchorView(anchor);
  SetPaintToLayer();
  set_corner_radius(kMultitaskMenuBubbleCornerRadius);
  // TODO(shidi): Confirm with UX/UI for additional arrow choices when parent
  // window has no space for `MultitaskMenu` to arrow at `TOP_CENTER`.
  SetArrow(views::BubbleBorder::Arrow::TOP_CENTER);
  SetButtons(ui::DIALOG_BUTTON_NONE);
  set_parent_window(parent_window);
  set_close_on_deactivate(true);
  SetPreferredSize(gfx::Size(KMultitaskMenuWidth, kMultitaskMenuHeight));
  SetUseDefaultFillLayout(true);
  // TODO(sammiequon/sophiewen): Check that `CalculatePreferredSize` gets the
  // size based on the child button sizes.

  // Must be initialized after setting bounds.
  multitask_menu_view_ = AddChildView(std::make_unique<MultitaskMenuView>(
      parent_window,
      base::BindRepeating(&MultitaskMenu::HideBubble, base::Unretained(this))));

  multitask_menu_view_->SetLayoutManager(std::make_unique<views::TableLayout>())
      ->AddPaddingColumn(views::TableLayout::kFixedSize, kRowPadding)
      .AddColumn(views::LayoutAlignment::kCenter,
                 views::LayoutAlignment::kCenter,
                 views::TableLayout::kFixedSize,
                 views::TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddPaddingColumn(views::TableLayout::kFixedSize, kRowPadding)
      .AddColumn(views::LayoutAlignment::kCenter,
                 views::LayoutAlignment::kCenter,
                 views::TableLayout::kFixedSize,
                 views::TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddPaddingRow(views::TableLayout::kFixedSize, kRowPadding)
      .AddRows(1, views::TableLayout::kFixedSize, 0)
      .AddPaddingRow(views::TableLayout::kFixedSize, kRowPadding)
      .AddRows(1, views::TableLayout::kFixedSize, 0);
}

MultitaskMenu::~MultitaskMenu() {
  if (bubble_widget_)
    HideBubble();
  bubble_widget_ = nullptr;
}

void MultitaskMenu::OnWidgetDestroying(views::Widget* widget) {
  DCHECK_EQ(bubble_widget_, widget);
  bubble_widget_observer_.Reset();
  bubble_widget_ = nullptr;
}

void MultitaskMenu::ShowBubble() {
  DCHECK(parent_window());
  bubble_widget_ = views::BubbleDialogDelegateView::CreateBubble(this);

  // This gets reset to the platform default when we call `CreateBubble()`,
  // which for Lacros is false.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  set_adjust_if_offscreen(true);
  SizeToContents();
#endif

  bubble_widget_->Show();
  bubble_widget_observer_.Observe(bubble_widget_.get());
}

void MultitaskMenu::HideBubble() {
  // This calls into OnWidgetDestroying() so `bubble_widget_` should have been
  // reset to nullptr.
  if (bubble_widget_ && !bubble_widget_->IsClosed())
    bubble_widget_->CloseNow();
}

}  // namespace chromeos
