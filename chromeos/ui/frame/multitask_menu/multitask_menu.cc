// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ui/frame/multitask_menu/multitask_menu.h"

#include <memory>

#include "base/check.h"
#include "chromeos/ui/base/display_util.h"
#include "chromeos/ui/base/window_properties.h"
#include "chromeos/ui/frame/multitask_menu/float_controller_base.h"
#include "chromeos/ui/frame/multitask_menu/multitask_menu_view.h"
#include "chromeos/ui/wm/window_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/display/screen.h"
#include "ui/display/tablet_state.h"
#include "ui/views/layout/table_layout.h"

namespace chromeos {

namespace {

constexpr int kMultitaskMenuBubbleCornerRadius = 8;
// Padding between the edges of the menu and the elements.
constexpr int kPaddingWide = 12;
// Padding between the elements.
constexpr int kPaddingNarrow = 8;

}  // namespace

MultitaskMenu::MultitaskMenu(views::View* anchor,
                             views::Widget* parent_widget,
                             bool close_on_move_out) {
  DCHECK(parent_widget);

  set_corner_radius(kMultitaskMenuBubbleCornerRadius);
  set_close_on_deactivate(true);
  set_internal_name("MultitaskMenuBubbleWidget");
  set_margins(gfx::Insets());
  set_parent_window(parent_widget->GetNativeWindow());
  SetAnchorView(anchor);
  SetArrow(views::BubbleBorder::Arrow::TOP_CENTER);
  SetEnableArrowKeyTraversal(true);
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  SetCanActivate(parent_widget->IsActive());
  SetUseDefaultFillLayout(true);

  uint8_t buttons = MultitaskMenuView::kFullscreen;

  if (SnapController::Get()->CanSnap(parent_window())) {
    buttons |= MultitaskMenuView::kHalfSplit;
    buttons |= MultitaskMenuView::kPartialSplit;
  }

  if (chromeos::wm::CanFloatWindow(parent_window())) {
    buttons |= MultitaskMenuView::kFloat;
  }

  // Must be initialized after setting bounds.
  multitask_menu_view_ = AddChildView(std::make_unique<MultitaskMenuView>(
      parent_window(),
      base::BindRepeating(&MultitaskMenu::HideBubble, base::Unretained(this)),
      base::BindRepeating(&MultitaskMenu::HideBubble, base::Unretained(this)),
      buttons, close_on_move_out ? anchor : nullptr));

  multitask_menu_view_->SetLayoutManager(std::make_unique<views::TableLayout>())
      ->AddPaddingColumn(views::TableLayout::kFixedSize, kPaddingWide)
      .AddColumn(views::LayoutAlignment::kCenter,
                 views::LayoutAlignment::kCenter,
                 views::TableLayout::kFixedSize,
                 views::TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddPaddingColumn(views::TableLayout::kFixedSize, kPaddingNarrow)
      .AddColumn(views::LayoutAlignment::kCenter,
                 views::LayoutAlignment::kCenter,
                 views::TableLayout::kFixedSize,
                 views::TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddPaddingColumn(views::TableLayout::kFixedSize, kPaddingWide)
      .AddPaddingRow(views::TableLayout::kFixedSize, kPaddingWide)
      .AddRows(1, views::TableLayout::kFixedSize, 0)
      .AddPaddingRow(views::TableLayout::kFixedSize, kPaddingNarrow)
      .AddRows(1, views::TableLayout::kFixedSize, 0)
      .AddPaddingRow(views::TableLayout::kFixedSize, kPaddingWide);

  display_observer_.emplace(this);
  window_observation_.Observe(parent_widget->GetNativeWindow());
}

MultitaskMenu::~MultitaskMenu() = default;

void MultitaskMenu::HideBubble() {
  // Callers of this function are expected to alter the bounds of the parent
  // window. Do not animate in this case otherwise the bubble may fade out while
  // outside of the parent window's bounds.
  views::Widget* widget = GetWidget();
  widget->SetVisibilityAnimationTransition(views::Widget::ANIMATE_NONE);

  // Destroys `this`.
  widget->CloseWithReason(views::Widget::ClosedReason::kUnspecified);
}

base::WeakPtr<MultitaskMenu> MultitaskMenu::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
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

  if (changed_metrics & display::DisplayObserver::DISPLAY_METRIC_ROTATION)
    HideBubble();
}

void MultitaskMenu::OnWindowPropertyChanged(aura::Window* window,
                                            const void* key,
                                            intptr_t old) {
  if (key == kIsShowingInOverviewKey) {
    HideBubble();
  }
}

void MultitaskMenu::OnWindowDestroying(aura::Window* window) {
  window_observation_.Reset();
}

BEGIN_METADATA(MultitaskMenu)
END_METADATA

}  // namespace chromeos
