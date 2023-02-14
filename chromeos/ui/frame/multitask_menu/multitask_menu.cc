// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ui/frame/multitask_menu/multitask_menu.h"

#include <memory>

#include "base/check.h"
#include "chromeos/ui/base/display_util.h"
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
// Padding between the edges of the menu and the elements.
constexpr int kPaddingWide = 12;
// Padding between the elements.
constexpr int kPaddingNarrow = 8;

// Dogfood feedback button layout values.
constexpr int kButtonWidth = 130;
constexpr int kButtonHeight = 28;

}  // namespace

MultitaskMenu::MultitaskMenu(views::View* anchor,
                             views::Widget* parent_widget,
                             base::OnceClosure close_callback) {
  DCHECK(parent_widget);

  set_corner_radius(kMultitaskMenuBubbleCornerRadius);
  set_close_on_deactivate(true);
  set_internal_name("MultitaskMenuBubbleWidget");
  set_margins(gfx::Insets());
  set_parent_window(parent_widget->GetNativeWindow());
  SetAnchorView(anchor);
  SetArrow(views::BubbleBorder::Arrow::TOP_CENTER);
  SetButtons(ui::DIALOG_BUTTON_NONE);
  SetUseDefaultFillLayout(true);

  RegisterWindowClosingCallback(std::move(close_callback));

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
      buttons));

  auto* layout = multitask_menu_view_->SetLayoutManager(
      std::make_unique<views::TableLayout>());
  layout->AddPaddingColumn(views::TableLayout::kFixedSize, kPaddingWide)
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
      .AddPaddingRow(views::TableLayout::kFixedSize, kPaddingWide)
      .AddRows(1, views::TableLayout::kFixedSize, kButtonHeight)
      .AddPaddingRow(views::TableLayout::kFixedSize, kPaddingWide);
  layout->SetChildViewIgnoredByLayout(multitask_menu_view_->feedback_button(),
                                      true);
  auto pref_size = multitask_menu_view_->GetPreferredSize();
  multitask_menu_view_->feedback_button()->SetBounds(
      (pref_size.width() - kButtonWidth) / 2,
      pref_size.height() - kButtonHeight - kPaddingWide, kButtonWidth,
      kButtonHeight);

  display_observer_.emplace(this);
}

MultitaskMenu::~MultitaskMenu() = default;

bool MultitaskMenu::IsBubbleShown() const {
  return bubble_widget_ && !bubble_widget_->IsClosed();
}

void MultitaskMenu::ToggleBubble() {
  if (!bubble_widget_) {
    ShowBubble();
  } else {
    // If the menu is toggle closed by the accelerator on a browser window, the
    // menu will get closed by deactivation and `HideBubble()` will do nothing
    // since `IsClosed()` would be true. For non-browser Ash windows and
    // non-accelerator close actions, `HideBubble()` will call `CloseNow()`.
    HideBubble();
  }
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
  parent_window_observation_.Observe(parent_window());
}

void MultitaskMenu::HideBubble() {
  // `CloseWithReason` calls into `OnWidgetDestroying()` asynchronously so
  // `bubble_widget_` will be reset to nullptr safely. And since
  // `bubble_widget_` owns `MultitaskMenu`, no house keeping is needed at
  // destructor.
  if (bubble_widget_ && !bubble_widget_->IsClosed()) {
    bubble_widget_->CloseWithReason(views::Widget::ClosedReason::kUnspecified);
  }
}

void MultitaskMenu::OnWindowDestroying(aura::Window* root_window) {
  DCHECK(parent_window_observation_.IsObservingSource(root_window));
  HideBubble();
}

void MultitaskMenu::OnWindowBoundsChanged(aura::Window* window,
                                          const gfx::Rect& old_bounds,
                                          const gfx::Rect& new_bounds,
                                          ui::PropertyChangeReason reason) {
  DCHECK(parent_window_observation_.IsObservingSource(window));
  HideBubble();
}

void MultitaskMenu::OnWidgetDestroying(views::Widget* widget) {
  DCHECK_EQ(bubble_widget_, widget);
  bubble_widget_observer_.Reset();
  parent_window_observation_.Reset();
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

BEGIN_METADATA(MultitaskMenu, views::BubbleDialogDelegateView)
END_METADATA

}  // namespace chromeos
