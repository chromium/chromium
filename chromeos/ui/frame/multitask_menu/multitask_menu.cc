// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ui/frame/multitask_menu/multitask_menu.h"

#include <memory>

#include "base/check.h"
#include "chromeos/ui/base/display_util.h"
#include "chromeos/ui/frame/frame_header.h"
#include "chromeos/ui/frame/multitask_menu/float_controller_base.h"
#include "chromeos/ui/frame/multitask_menu/multitask_menu_view.h"
#include "chromeos/ui/wm/window_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/display/screen.h"
#include "ui/display/tablet_state.h"
#include "ui/views/layout/table_layout.h"

namespace chromeos {

namespace {

constexpr int kMultitaskMenuBubbleCornerRadius = 8;
constexpr int kRowPadding = 16;

}  // namespace

MultitaskMenu::MultitaskMenu(views::View* anchor,
                             views::Widget* parent_widget) {
  DCHECK(parent_widget);
  aura::Window* parent_window = parent_widget->GetNativeWindow();

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

  // Check the model to see which buttons we should show. Since this menu is
  // triggered from the maximize button on the frame, it should have a frame
  // header, and can be maximized and therefore fullscreened. The exception is
  // in tests, where we show all the buttons.
  uint8_t buttons = MultitaskMenuView::kFullscreen;
  auto* frame_header = FrameHeader::Get(parent_widget);
  const CaptionButtonModel* caption_button_model =
      frame_header ? frame_header->GetCaptionButtonModel() : nullptr;

  if (!caption_button_model ||
      caption_button_model->IsVisible(
          views::CAPTION_BUTTON_ICON_LEFT_TOP_SNAPPED)) {
    buttons |= MultitaskMenuView::kHalfSplit;
    buttons |= MultitaskMenuView::kPartialSplit;
  }

  // The frame caption button to float/unfloat is only shown with the ash dev
  // flag on, or in tablet mode when a window is floated. The multitask menu
  // float button is shown whenever a window can be floated, so linking with the
  // model does not work here.
  if (chromeos::wm::CanFloatWindow(parent_window))
    buttons |= MultitaskMenuView::kFloat;

  // Must be initialized after setting bounds.
  multitask_menu_view_ = AddChildView(std::make_unique<MultitaskMenuView>(
      parent_window,
      base::BindRepeating(&MultitaskMenu::HideBubble, base::Unretained(this)),
      buttons));

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
      .AddPaddingColumn(views::TableLayout::kFixedSize, kRowPadding)
      .AddPaddingRow(views::TableLayout::kFixedSize, kRowPadding)
      .AddRows(1, views::TableLayout::kFixedSize, 0)
      .AddPaddingRow(views::TableLayout::kFixedSize, kRowPadding)
      .AddRows(1, views::TableLayout::kFixedSize, 0)
      .AddPaddingRow(views::TableLayout::kFixedSize, kRowPadding);

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
  if (changed_metrics & display::DisplayObserver::DISPLAY_METRIC_ROTATION)
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
