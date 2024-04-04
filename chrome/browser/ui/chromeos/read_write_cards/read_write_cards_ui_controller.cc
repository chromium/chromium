// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/chromeos/read_write_cards/read_write_cards_ui_controller.h"

#include <memory>

#include "base/check_is_test.h"
#include "base/check_op.h"
#include "base/no_destructor.h"
#include "chrome/browser/ui/chromeos/read_write_cards/read_write_cards_view.h"
#include "ui/aura/window.h"
#include "ui/base/ui_base_types.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/background.h"
#include "ui/views/controls/menu/menu_config.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/metadata/view_factory_internal.h"
#include "ui/views/view.h"
#include "ui/views/view_tracker.h"
#include "ui/views/widget/tooltip_manager.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace chromeos {

namespace {

constexpr int kQuickAnswersAndMahiSpacing = 10;

views::Widget::InitParams CreateWidgetInitParams() {
  views::Widget::InitParams params;
  params.activatable = views::Widget::InitParams::Activatable::kNo;
  params.shadow_type = views::Widget::InitParams::ShadowType::kNone;
  params.type = views::Widget::InitParams::TYPE_POPUP;
  params.z_order = ui::ZOrderLevel::kFloatingUIElement;
  params.child = true;
  params.name = ReadWriteCardsUiController::kWidgetName;
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;

  // Parent the widget to the owner of the menu.
  auto* active_menu_controller = views::MenuController::GetActiveInstance();

  if (!active_menu_controller || !active_menu_controller->owner()) {
    CHECK_IS_TEST();
    return params;
  }

  // This widget has to be a child of menu owner's widget to make keyboard focus
  // work.
  params.parent = active_menu_controller->owner()->GetNativeView();

  return params;
}

}  // namespace

ReadWriteCardsUiController::ReadWriteCardsUiController() = default;

ReadWriteCardsUiController::~ReadWriteCardsUiController() {
  if (quick_answers_view_) {
    OnViewIsDeleting(quick_answers_view_);
  }
}

ReadWriteCardsView* ReadWriteCardsUiController::SetQuickAnswersView(
    std::unique_ptr<ReadWriteCardsView> view) {
  CreateWidgetIfNeeded();

  CHECK(!quick_answers_view_);

  views::View* contents_view = widget_->GetContentsView();
  quick_answers_view_ = contents_view->AddChildView(std::move(view));
  quick_answers_view_->AddObserver(this);

  Relayout();

  return quick_answers_view_;
}

void ReadWriteCardsUiController::RemoveQuickAnswersView() {
  if (!quick_answers_view_) {
    return;
  }

  widget_->GetContentsView()->RemoveChildViewT(
      quick_answers_view_.ExtractAsDangling());
  quick_answers_view_ = nullptr;
  MaybeHideWidget();

  if (widget_) {
    Relayout();
  }
}

views::View* ReadWriteCardsUiController::SetMahiView(
    std::unique_ptr<views::View> view) {
  CreateWidgetIfNeeded();

  CHECK(!mahi_view_.view());

  views::View* contents_view = widget_->GetContentsView();
  mahi_view_.SetView(contents_view->AddChildView(std::move(view)));

  Relayout();

  return mahi_view_.view();
}

void ReadWriteCardsUiController::RemoveMahiView() {
  if (!mahi_view_.view()) {
    return;
  }

  widget_->GetContentsView()->RemoveChildViewT(mahi_view_.view());
  MaybeHideWidget();

  if (widget_) {
    Relayout();
  }
}

ReadWriteCardsView* ReadWriteCardsUiController::GetQuickAnswersViewForTest() {
  return quick_answers_view_;
}

views::View* ReadWriteCardsUiController::GetMahiViewForTest() {
  return mahi_view_.view();
}

void ReadWriteCardsUiController::Relayout() {
  CHECK(widget_);
  int widget_width = context_menu_bounds_.width();
  int widget_height =
      widget_->GetContentsView()->GetHeightForWidth(widget_width);

  int x = context_menu_bounds_.x();
  int y =
      context_menu_bounds_.y() - widget_height - kQuickAnswersAndMahiSpacing;

  // Include the extra reserved height in our decision to place the widget
  // above or below the context menu, since we should reserve space at the top
  // to avoid running out of space when a view re-layout. We will use the
  // view's `GetMaximumSize()` to calculate this reserved height.
  int extra_reserved_height = 0;
  if (quick_answers_view_ && !quick_answers_view_->GetMaximumSize().IsZero()) {
    CHECK_GE(quick_answers_view_->GetMaximumSize().height(),
             quick_answers_view_->size().height());
    extra_reserved_height = quick_answers_view_->GetMaximumSize().height() -
                            quick_answers_view_->size().height();
  }

  bool widget_above_context_menu = true;
  if (y - extra_reserved_height < display::Screen::GetScreen()
                                      ->GetDisplayMatching(context_menu_bounds_)
                                      .work_area()
                                      .y()) {
    y = context_menu_bounds_.bottom() + kQuickAnswersAndMahiSpacing;
    widget_above_context_menu = false;
  }

  ReorderChildViews(widget_above_context_menu);

  gfx::Rect bounds({x, y}, {widget_width, widget_height});
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // For Ash, convert the position relative to the screen.
  // For Lacros, `bounds` is already relative to the toplevel window and the
  // position will be calculated on server side.
  wm::ConvertRectFromScreen(widget_->GetNativeWindow()->parent(), &bounds);
#endif

  widget_->SetBounds(bounds);
}

void ReadWriteCardsUiController::MaybeRelayout() {
  if (!widget_) {
    return;
  }

  Relayout();
}

void ReadWriteCardsUiController::SetContextMenuBounds(
    const gfx::Rect& context_menu_bounds) {
  context_menu_bounds_ = context_menu_bounds;

  if (quick_answers_view_) {
    quick_answers_view_->SetContextMenuBounds(context_menu_bounds);
  }

  if (widget_) {
    Relayout();
  }
}

void ReadWriteCardsUiController::OnViewIsDeleting(views::View* observed_view) {
  if (!quick_answers_view_) {
    return;
  }

  CHECK_EQ(quick_answers_view_, observed_view);
  quick_answers_view_->RemoveObserver(this);
  quick_answers_view_ = nullptr;
}

void ReadWriteCardsUiController::CreateWidgetIfNeeded() {
  if (!widget_) {
    widget_ = std::make_unique<views::Widget>(CreateWidgetInitParams());

    widget_->SetContentsView(
        views::Builder<views::BoxLayoutView>()
            .SetOrientation(views::BoxLayout::Orientation::kVertical)
            .SetBetweenChildSpacing(kQuickAnswersAndMahiSpacing)
            // Widget contents view should be transparent to reveal the gap
            // between quick answers and mahi cards.
            .SetBackground(views::CreateSolidBackground(SK_ColorTRANSPARENT))
            .Build());

    // Allow tooltips to be shown despite menu-controller owning capture.
    widget_->SetNativeWindowProperty(
        views::TooltipManager::kGroupingPropertyKey,
        reinterpret_cast<void*>(views::MenuConfig::kMenuControllerGroupingId));
  }

  widget_->ShowInactive();
}

void ReadWriteCardsUiController::MaybeHideWidget() {
  if (quick_answers_view_ || mahi_view_.view()) {
    return;
  }

  // Close the widget if all the views are removed.
  widget_.reset();
}

void ReadWriteCardsUiController::ReorderChildViews(
    bool widget_above_context_menu) {
  // No need to reorder if one of the view is not set.
  if (!quick_answers_view_ || !mahi_view_) {
    return;
  }

  CHECK(widget_);
  auto* contents_view = widget_->GetContentsView();

  // Quick Answers view should be on top if the widget is above the context
  // menu. The order should be reversed otherwise.
  if (widget_above_context_menu) {
    contents_view->ReorderChildView(quick_answers_view_, /*index=*/0);
  } else {
    contents_view->ReorderChildView(mahi_view_.view(), /*index=*/0);
  }
}

}  // namespace chromeos
