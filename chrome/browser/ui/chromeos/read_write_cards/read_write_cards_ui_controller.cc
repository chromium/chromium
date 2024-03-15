// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/chromeos/read_write_cards/read_write_cards_ui_controller.h"

#include <memory>

#include "base/check_is_test.h"
#include "base/check_op.h"
#include "base/no_destructor.h"
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

constexpr char kWidgetName[] = "QuickAnswersMahiMenuWidget";
constexpr int kQuickAnswersAndMahiSpacing = 10;

views::Widget::InitParams CreateWidgetInitParams() {
  views::Widget::InitParams params;
  params.activatable = views::Widget::InitParams::Activatable::kNo;
  // TODO(b/327786910): remove shadow in the widget and use shadow in individual
  // views.
  params.shadow_elevation = 2;
  params.shadow_type = views::Widget::InitParams::ShadowType::kDrop;
  params.type = views::Widget::InitParams::TYPE_POPUP;
  params.z_order = ui::ZOrderLevel::kFloatingUIElement;
  params.child = true;
  params.name = kWidgetName;
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

ReadWriteCardsUiController::~ReadWriteCardsUiController() = default;

views::View* ReadWriteCardsUiController::SetQuickAnswersView(
    std::unique_ptr<views::View> view) {
  CreateWidgetIfNeeded();

  CHECK(!quick_answers_view_.view());

  views::View* contents_view = widget_->GetContentsView();
  quick_answers_view_.SetView(contents_view->AddChildView(std::move(view)));

  UpdateWidgetBounds();

  return quick_answers_view_.view();
}

void ReadWriteCardsUiController::RemoveQuickAnswersView() {
  if (!quick_answers_view_.view()) {
    return;
  }

  widget_->GetContentsView()->RemoveChildViewT(quick_answers_view_.view());
  MaybeHideWidget();

  if (widget_) {
    UpdateWidgetBounds();
  }
}

views::View* ReadWriteCardsUiController::SetMahiView(
    std::unique_ptr<views::View> view) {
  CreateWidgetIfNeeded();

  CHECK(!mahi_view_.view());

  views::View* contents_view = widget_->GetContentsView();
  mahi_view_.SetView(contents_view->AddChildView(std::move(view)));

  UpdateWidgetBounds();

  return mahi_view_.view();
}

void ReadWriteCardsUiController::RemoveMahiView() {
  if (!mahi_view_.view()) {
    return;
  }

  widget_->GetContentsView()->RemoveChildViewT(mahi_view_.view());
  MaybeHideWidget();

  if (widget_) {
    UpdateWidgetBounds();
  }
}

views::View* ReadWriteCardsUiController::GetQuickAnswersViewForTest() {
  return quick_answers_view_.view();
}

views::View* ReadWriteCardsUiController::GetMahiViewForTest() {
  return mahi_view_.view();
}

void ReadWriteCardsUiController::UpdateWidgetBounds() {
  CHECK(widget_);
  int widget_width = context_menu_bounds_.width();
  int widget_height =
      widget_->GetContentsView()->GetHeightForWidth(widget_width);

  int x = context_menu_bounds_.x();
  int y =
      context_menu_bounds_.y() - widget_height - kQuickAnswersAndMahiSpacing;
  if (y < display::Screen::GetScreen()
              ->GetDisplayMatching(context_menu_bounds_)
              .work_area()
              .y()) {
    y = context_menu_bounds_.bottom() + kQuickAnswersAndMahiSpacing;
  }

  gfx::Rect bounds({x, y}, {widget_width, widget_height});
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // For Ash, convert the position relative to the screen.
  // For Lacros, `bounds` is already relative to the toplevel window and the
  // position will be calculated on server side.
  wm::ConvertRectFromScreen(widget_->GetNativeWindow()->parent(), &bounds);
#endif

  widget_->SetBounds(bounds);
}

void ReadWriteCardsUiController::SetContextMenuBounds(
    const gfx::Rect& context_menu_bounds) {
  context_menu_bounds_ = context_menu_bounds;

  if (widget_) {
    UpdateWidgetBounds();
  }
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
  if (quick_answers_view_.view() || mahi_view_.view()) {
    return;
  }

  // Close the widget if all the views are removed.
  widget_.reset();
}

}  // namespace chromeos
