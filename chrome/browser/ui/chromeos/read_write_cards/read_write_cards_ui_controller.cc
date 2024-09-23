// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/chromeos/read_write_cards/read_write_cards_ui_controller.h"

#include <algorithm>
#include <memory>

#include "base/check_is_test.h"
#include "base/check_op.h"
#include "base/no_destructor.h"
#include "chrome/browser/ui/chromeos/read_write_cards/read_write_cards_view.h"
#include "chrome/browser/ui/views/editor_menu/utils/pre_target_handler.h"
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

constexpr int kQuickAnswersAndMahiSpacing = 8;

views::Widget::InitParams CreateWidgetInitParams() {
  views::Widget::InitParams params(
      views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_POPUP);
  params.activatable = views::Widget::InitParams::Activatable::kNo;
  params.shadow_type = views::Widget::InitParams::ShadowType::kNone;
  params.z_order = ui::ZOrderLevel::kFloatingUIElement;
  params.child = true;
  params.name = ReadWriteCardsUiController::kWidgetName;
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;

  auto* active_menu_controller = views::MenuController::GetActiveInstance();

  // The menu might already be closed.
  if (active_menu_controller && active_menu_controller->owner()) {
    // This widget has to be a child of menu owner's widget to make keyboard
    // focus work.
    params.parent = active_menu_controller->owner()->GetNativeView();
  }

  return params;
}

// `GetExtraHeightForWidth` returns an extra height a view might take given the
// current preferred size.
int GetExtraHeightForWidth(views::View* view, int width) {
  if (!view) {
    return 0;
  }

  // A view returns a maximum size via `GetMaximumSize`. If it returns zero
  // size, it means that the view doesn't require maximum size handling, i.e.,
  // the view height won't (should not) change with a re-layout.
  gfx::Size maximum_size = view->GetMaximumSize();
  if (maximum_size.IsZero()) {
    return 0;
  }

  // TODO(b/339166296): consider if we can make this test code only check.
  return std::max(0, maximum_size.height() - view->GetHeightForWidth(width));
}

gfx::Point GetWidgetOrigin(const gfx::Rect& context_menu,
                           const gfx::Size& widget_size,
                           bool above_context_menu) {
  if (above_context_menu) {
    return gfx::Point(
        context_menu.x(),
        context_menu.y() - widget_size.height() - kQuickAnswersAndMahiSpacing);
  }

  return gfx::Point(context_menu.x(),
                    context_menu.bottom() + kQuickAnswersAndMahiSpacing);
}

}  // namespace

ReadWriteCardsUiController::ReadWriteCardsUiController() = default;
ReadWriteCardsUiController::~ReadWriteCardsUiController() = default;

ReadWriteCardsView* ReadWriteCardsUiController::SetQuickAnswersUi(
    std::unique_ptr<ReadWriteCardsView> view) {
  MaybeCreateWidget();

  CHECK(!quick_answers_ui_observation_.IsObserving());
  quick_answers_ui_observation_.Observe(
      widget_->GetContentsView()->AddChildView(std::move(view)));

  Relayout();

  return quick_answers_ui();
}

void ReadWriteCardsUiController::RemoveQuickAnswersUi() {
  if (!quick_answers_ui()) {
    return;
  }

  widget_->GetContentsView()->RemoveChildViewT(quick_answers_ui());
}

views::View* ReadWriteCardsUiController::SetMahiUi(
    std::unique_ptr<views::View> view) {
  MaybeCreateWidget();

  CHECK(!mahi_ui_observation_.IsObserving());
  mahi_ui_observation_.Observe(
      widget_->GetContentsView()->AddChildView(std::move(view)));

  Relayout();

  return mahi_ui();
}

void ReadWriteCardsUiController::RemoveMahiUi() {
  if (!mahi_ui()) {
    return;
  }

  widget_->GetContentsView()->RemoveChildViewT(mahi_ui());
}

ReadWriteCardsView* ReadWriteCardsUiController::GetQuickAnswersUiForTest() {
  return quick_answers_ui();
}

views::View* ReadWriteCardsUiController::GetMahiUiForTest() {
  return mahi_ui();
}

void ReadWriteCardsUiController::MaybeRelayout() {
  if (!widget_) {
    return;
  }

  Relayout();
}

void ReadWriteCardsUiController::Relayout() {
  CHECK(widget_);

  gfx::Size widget_size(context_menu_bounds_.width(),
                        widget_->GetContentsView()->GetHeightForWidth(
                            context_menu_bounds_.width()));

  // Calculate maximum size to decide whether to put the widget above or below
  // the context menu. This is to avoid flipping the position of the widget for
  // running out of space after a view re-layout.
  gfx::Size maximum_widget_size = widget_size;
  maximum_widget_size.Enlarge(
      0, GetExtraHeightForWidth(quick_answers_ui(), widget_size.width()));
  maximum_widget_size.Enlarge(
      0, GetExtraHeightForWidth(mahi_ui(), widget_size.width()));

  gfx::Point widget_origin_with_maximum_size =
      GetWidgetOrigin(context_menu_bounds_, maximum_widget_size,
                      /*above_context_menu=*/true);
  widget_above_context_menu_ = display::Screen::GetScreen()
                                   ->GetDisplayMatching(context_menu_bounds_)
                                   .work_area()
                                   .Contains(widget_origin_with_maximum_size);
  gfx::Point widget_origin = GetWidgetOrigin(context_menu_bounds_, widget_size,
                                             widget_above_context_menu_);

  ReorderChildViews();

  gfx::Rect bounds(widget_origin, widget_size);
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

  if (quick_answers_ui()) {
    quick_answers_ui()->SetContextMenuBounds(context_menu_bounds);
  }

  MaybeRelayout();
}

views::View* ReadWriteCardsUiController::GetRootView() {
  return widget_ ? widget_->GetContentsView() : nullptr;
}

std::vector<views::View*>
ReadWriteCardsUiController::GetTraversableViewsByUpDownKeys() {
  std::vector<views::View*> views;
  if (!widget_) {
    return views;
  }

  if (quick_answers_ui()) {
    views.emplace_back(quick_answers_ui());
  }

  if (mahi_ui()) {
    views.emplace_back(mahi_ui());
  }

  if (!widget_above_context_menu_) {
    std::reverse(views.begin(), views.end());
  }

  return views;
}

void ReadWriteCardsUiController::OnViewIsDeleting(views::View* view) {
  if (view == quick_answers_ui()) {
    CHECK(quick_answers_ui_observation_.IsObserving());
    quick_answers_ui_observation_.Reset();
    MaybeHideWidget();
    MaybeRelayout();
    return;
  } else if (view == mahi_ui()) {
    CHECK(mahi_ui_observation_.IsObserving());
    mahi_ui_observation_.Reset();
    MaybeHideWidget();
    MaybeRelayout();
    return;
  } else {
    // This is for a developer to notice forgetting handling of an added view.
    LOG(FATAL) << "Observing an uninterested view.";
  }
}

void ReadWriteCardsUiController::OnViewLayoutInvalidated(views::View* view) {
  MaybeRelayout();
}

void ReadWriteCardsUiController::MaybeCreateWidget() {
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

    pre_target_handler_.emplace(/*delegate=*/*this);

    // Allow tooltips to be shown despite menu-controller owning capture.
    widget_->SetNativeWindowProperty(
        views::TooltipManager::kGroupingPropertyKey,
        reinterpret_cast<void*>(views::MenuConfig::kMenuControllerGroupingId));
  }

  widget_->ShowInactive();
}

void ReadWriteCardsUiController::MaybeHideWidget() {
  if (quick_answers_ui() || mahi_ui()) {
    return;
  }

  // Close the widget if all the views are removed.
  pre_target_handler_.reset();
  widget_.reset();
}

void ReadWriteCardsUiController::ReorderChildViews() {
  // No need to reorder if one of the view is not set.
  if (!quick_answers_ui() || !mahi_ui()) {
    return;
  }

  CHECK(widget_);
  auto* contents_view = widget_->GetContentsView();

  // Quick Answers view should be on top if the widget is above the context
  // menu. The order should be reversed otherwise.
  if (widget_above_context_menu_) {
    contents_view->ReorderChildView(quick_answers_ui(), /*index=*/0);
  } else {
    contents_view->ReorderChildView(mahi_ui(), /*index=*/0);
  }
}

}  // namespace chromeos
