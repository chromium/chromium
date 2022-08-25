// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ui/frame/multitask_menu/multitask_menu.h"

#include <memory>

#include "base/check.h"
#include "chromeos/ui/base/display_util.h"
#include "chromeos/ui/frame/multitask_menu/float_controller_base.h"
#include "chromeos/ui/frame/multitask_menu/multitask_menu_view.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/display/screen.h"
#include "ui/display/tablet_state.h"
#include "ui/views/layout/table_layout.h"

namespace chromeos {

namespace {

constexpr int kMultitaskMenuBubbleCornerRadius = 8;
constexpr int kRowPadding = 16;

}  // namespace

MultitaskMenu::MultitaskMenu(views::View* anchor, aura::Window* parent_window) {
  DCHECK(parent_window);

  set_corner_radius(kMultitaskMenuBubbleCornerRadius);
  set_close_on_deactivate(true);
  set_internal_name("MultitaskMenuBubbleWidget");
  set_margins(gfx::Insets());
  set_parent_window(parent_window);

  SetAnchorView(anchor);
  // TODO(shidi): Confirm with UX/UI for additional arrow choices when parent
  // window has no space for `MultitaskMenu` to arrow at `TOP_CENTER`.
  SetArrow(views::BubbleBorder::Arrow::TOP_CENTER);
  SetButtons(ui::DIALOG_BUTTON_NONE);

  SetUseDefaultFillLayout(true);

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

  display_observer_.emplace(this);
}

MultitaskMenu::~MultitaskMenu() {
  HideBubble();
}

void MultitaskMenu::OnWidgetDestroying(views::Widget* widget) {
  DCHECK_EQ(bubble_widget_, widget);
  bubble_widget_observer_.Reset();
  bubble_widget_ = nullptr;
}

void MultitaskMenu::OnDisplayTabletStateChanged(display::TabletState state) {
  if (state == display::TabletState::kEnteringTabletMode)
    HideBubble();
}

void MultitaskMenu::OnDisplayMetricsChanged(const display::Display& display,
                                            uint32_t changed_metrics) {
  // Ignore changes to displays that aren't showing the menu.
  if (display.id() !=
      display::Screen::GetScreen()
          ->GetDisplayNearestView(GetWidget()->GetNativeWindow())
          .id()) {
    return;
  }
  // TODO(shidi): Will do the rotate transition on a separate cl. Close the
  // bubble at rotation for now.
  if (changed_metrics & (DISPLAY_METRIC_BOUNDS | DISPLAY_METRIC_ROTATION))
    HideBubble();
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

BEGIN_METADATA(MultitaskMenu, views::BubbleDialogDelegateView)
END_METADATA

}  // namespace chromeos
