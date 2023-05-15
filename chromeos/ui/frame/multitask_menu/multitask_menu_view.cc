// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ui/frame/multitask_menu/multitask_menu_view.h"

#include <memory>

#include "base/check.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/user_metrics.h"
#include "base/timer/timer.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "chromeos/ui/base/display_util.h"
#include "chromeos/ui/base/window_properties.h"
#include "chromeos/ui/base/window_state_type.h"
#include "chromeos/ui/frame/caption_buttons/snap_controller.h"
#include "chromeos/ui/frame/frame_utils.h"
#include "chromeos/ui/frame/multitask_menu/float_controller_base.h"
#include "chromeos/ui/frame/multitask_menu/multitask_button.h"
#include "chromeos/ui/frame/multitask_menu/multitask_menu_metrics.h"
#include "chromeos/ui/frame/multitask_menu/split_button_view.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/base/default_style.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/display/screen.h"
#include "ui/events/types/event_type.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/background.h"
#include "ui/views/controls/label.h"
#include "ui/views/widget/widget.h"

namespace chromeos {

namespace {

bool g_skip_mouse_out_delay_for_testing = false;

constexpr int kCenterPadding = 4;
constexpr int kLabelFontSize = 13;

// If the menu was opened as a result of hovering over the frame size button,
// moving the mouse outside the menu or size button will result in closing it
// after 250 ms have elapsed.
constexpr base::TimeDelta kMouseExitMenuTimeout = base::Milliseconds(250);

// The multitask menu fade out duration after the exit timer finishes.
constexpr base::TimeDelta kFadeDuration = base::Milliseconds(100);

// Creates multitask button with label.
std::unique_ptr<views::View> CreateButtonContainer(
    std::unique_ptr<views::View> button_view,
    int label_message_id) {
  auto container = std::make_unique<views::BoxLayoutView>();
  container->SetOrientation(views::BoxLayout::Orientation::kVertical);
  container->SetBetweenChildSpacing(kCenterPadding);
  container->AddChildView(std::move(button_view));
  views::Label* label = container->AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(label_message_id)));
  label->SetFontList(gfx::FontList({"Roboto"}, gfx::Font::NORMAL,
                                   kLabelFontSize, gfx::Font::Weight::NORMAL));
  label->SetEnabledColor(gfx::kGoogleGrey900);
  label->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  return container;
}

}  // namespace

// -----------------------------------------------------------------------------
// MultitaskMenuView::MenuPreTargetHandler:
// Auto-closes the multitask menu on click outside or after timeout.
class MultitaskMenuView::MenuPreTargetHandler : public ui::EventHandler {
 public:
  MenuPreTargetHandler(views::Widget* menu_widget,
                       base::RepeatingClosure close_callback,
                       views::View* anchor_view)
      : menu_widget_(menu_widget),
        anchor_view_(anchor_view),
        close_callback_(std::move(close_callback)) {
    aura::Env::GetInstance()->AddPreTargetHandler(
        this, ui::EventTarget::Priority::kSystem);
  }
  MenuPreTargetHandler(const MenuPreTargetHandler&) = delete;
  MenuPreTargetHandler& operator=(const MenuPreTargetHandler&) = delete;
  ~MenuPreTargetHandler() override {
    aura::Env::GetInstance()->RemovePreTargetHandler(this);
  }

  void OnMouseEvent(ui::MouseEvent* event) override {
    if (!menu_widget_ || menu_widget_->IsClosed()) {
      return;
    }

    if (event->type() == ui::ET_MOUSE_PRESSED) {
      ProcessPressedEvent(*event);
      return;
    }

    if (event->type() == ui::ET_MOUSE_MOVED && anchor_view_) {
      const gfx::Point screen_location =
          event->target()->GetScreenLocation(*event);
      // Stop the existing timer if either the anchor or the menu contain the
      // event.
      if (menu_widget_->GetWindowBoundsInScreen().Contains(screen_location) ||
          anchor_view_->GetBoundsInScreen().Contains(screen_location)) {
        exit_timer_.Stop();
      } else if (g_skip_mouse_out_delay_for_testing) {
        OnExitTimerFinished();
      } else if (!exit_timer_.IsRunning()) {
        exit_timer_.Start(FROM_HERE, kMouseExitMenuTimeout, this,
                          &MenuPreTargetHandler::OnExitTimerFinished);
      }
    }
  }

  void OnTouchEvent(ui::TouchEvent* event) override {
    if (!menu_widget_ || menu_widget_->IsClosed()) {
      return;
    }

    if (event->type() == ui::ET_TOUCH_PRESSED) {
      ProcessPressedEvent(*event);
    }
  }

  void ProcessPressedEvent(const ui::LocatedEvent& event) {
    const gfx::Point screen_location = event.target()->GetScreenLocation(event);
    // If the event is out of menu bounds, close the menu.
    if (!menu_widget_->GetWindowBoundsInScreen().Contains(screen_location)) {
      close_callback_.Run();
    }
  }

 private:
  void OnExitTimerFinished() {
    if (!menu_widget_->GetLayer()->GetAnimator()->is_animating()) {
      views::AnimationBuilder()
          .SetPreemptionStrategy(
              ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
          .OnEnded(base::BindOnce(&MenuPreTargetHandler::OnFadeOutFinished,
                                  weak_factory_.GetWeakPtr()))
          .Once()
          .SetDuration(kFadeDuration)
          .SetOpacity(menu_widget_->GetLayer(), 0.0f, gfx::Tween::LINEAR);
    }
  }

  void OnFadeOutFinished() { close_callback_.Run(); }

  // The widget of the multitask menu that is currently shown. Guaranteed to
  // outlive `this`, which will get destroyed when the menu is destructed in
  // `close_callback_`.
  const raw_ptr<views::Widget, ExperimentalAsh> menu_widget_;

  // The anchor of the menu's widget if it exists. Set if there is an anchor and
  // we want the menu to close if the mouse has exited the menu bounds.
  raw_ptr<views::View, ExperimentalAsh> anchor_view_ = nullptr;

  base::OneShotTimer exit_timer_;

  base::RepeatingClosure close_callback_;

  // Chrome's compiler toolchain enforces that any `WeakPtrFactory`
  // fields are declared last, to avoid destruction ordering issues.
  base::WeakPtrFactory<MenuPreTargetHandler> weak_factory_{this};
};

// -----------------------------------------------------------------------------
// MultitaskMenuView:

MultitaskMenuView::MultitaskMenuView(aura::Window* window,
                                     base::RepeatingClosure close_callback,
                                     uint8_t buttons,
                                     views::View* anchor_view)
    : window_(window),
      anchor_view_(anchor_view),
      close_callback_(std::move(close_callback)) {
  DCHECK(window);
  DCHECK(close_callback_);
  SetUseDefaultFillLayout(true);

  window_observation_.Observe(window);

  // The display orientation. This determines whether menu is in
  // landscape/portrait mode.
  const bool is_portrait_mode = !chromeos::IsDisplayLayoutHorizontal(
      display::Screen::GetScreen()->GetDisplayNearestWindow(window));

  // Half button.
  if (buttons & kHalfSplit) {
    auto half_button = std::make_unique<SplitButtonView>(
        SplitButtonView::SplitButtonType::kHalfButtons,
        base::BindRepeating(&MultitaskMenuView::SplitButtonPressed,
                            base::Unretained(this)),
        window, is_portrait_mode);
    half_button_for_testing_ = half_button.get();
    AddChildView(CreateButtonContainer(std::move(half_button),
                                       IDS_MULTITASK_MENU_HALF_BUTTON_NAME));
  }

  // Partial button.
  if (buttons & kPartialSplit) {
    auto partial_button = std::make_unique<SplitButtonView>(
        SplitButtonView::SplitButtonType::kPartialButtons,
        base::BindRepeating(&MultitaskMenuView::PartialButtonPressed,
                            base::Unretained(this)),
        window, is_portrait_mode);
    partial_button_ = partial_button.get();
    AddChildView(CreateButtonContainer(std::move(partial_button),
                                       IDS_MULTITASK_MENU_PARTIAL_BUTTON_NAME));
  }

  // Full screen button.
  if (buttons & kFullscreen) {
    const bool fullscreened = window->GetProperty(kWindowStateTypeKey) ==
                              WindowStateType::kFullscreen;
    int message_id = fullscreened
                         ? IDS_MULTITASK_MENU_EXIT_FULLSCREEN_BUTTON_NAME
                         : IDS_MULTITASK_MENU_FULLSCREEN_BUTTON_NAME;
    auto full_button = std::make_unique<MultitaskButton>(
        base::BindRepeating(&MultitaskMenuView::FullScreenButtonPressed,
                            base::Unretained(this)),
        MultitaskButton::Type::kFull, is_portrait_mode,
        /*paint_as_active=*/fullscreened,
        l10n_util::GetStringUTF16(message_id));
    full_button_for_testing_ = full_button.get();
    AddChildView(CreateButtonContainer(std::move(full_button), message_id));
  }

  // Float on top button.
  if (buttons & kFloat) {
    const bool floated =
        window->GetProperty(kWindowStateTypeKey) == WindowStateType::kFloated;
    int message_id = floated ? IDS_MULTITASK_MENU_EXIT_FLOAT_BUTTON_NAME
                             : IDS_MULTITASK_MENU_FLOAT_BUTTON_NAME;
    auto float_button = std::make_unique<MultitaskButton>(
        base::BindRepeating(&MultitaskMenuView::FloatButtonPressed,
                            base::Unretained(this)),
        MultitaskButton::Type::kFloat, is_portrait_mode,
        /*paint_as_active=*/floated, l10n_util::GetStringUTF16(message_id));
    float_button_for_testing_ = float_button.get();
    AddChildView(CreateButtonContainer(std::move(float_button), message_id));
  }
}

MultitaskMenuView::~MultitaskMenuView() {
  event_handler_.reset();
}

void MultitaskMenuView::AddedToWidget() {
  // When the menu widget is shown, we install `MenuPreTargetHandler` to close
  // the menu on any events outside.
  event_handler_ = std::make_unique<MenuPreTargetHandler>(
      GetWidget(), close_callback_, anchor_view_);
}

void MultitaskMenuView::OnWindowDestroying(aura::Window* window) {
  CHECK(window_observation_.IsObservingSource(window));

  window_observation_.Reset();
  window_ = nullptr;
  close_callback_.Run();
}

void MultitaskMenuView::OnWindowBoundsChanged(aura::Window* window,
                                              const gfx::Rect& old_bounds,
                                              const gfx::Rect& new_bounds,
                                              ui::PropertyChangeReason reason) {
  CHECK(window_observation_.IsObservingSource(window));
  close_callback_.Run();
}

void MultitaskMenuView::OnWindowVisibilityChanging(aura::Window* window,
                                                   bool visible) {
  CHECK(window_observation_.IsObservingSource(window));
  if (!visible) {
    close_callback_.Run();
  }
}

// static
void MultitaskMenuView::SetSkipMouseOutDelayForTesting(bool val) {
  g_skip_mouse_out_delay_for_testing = val;
}

void MultitaskMenuView::SplitButtonPressed(SnapDirection direction) {
  SnapController::Get()->CommitSnap(
      window_, direction, kDefaultSnapRatio,
      SnapController::SnapRequestSource::kWindowLayoutMenu);
  close_callback_.Run();
  RecordMultitaskMenuActionType(MultitaskMenuActionType::kHalfSplitButton);
}

void MultitaskMenuView::PartialButtonPressed(SnapDirection direction) {
  SnapController::Get()->CommitSnap(
      window_, direction,
      direction == SnapDirection::kPrimary ? kTwoThirdSnapRatio
                                           : kOneThirdSnapRatio,
      SnapController::SnapRequestSource::kWindowLayoutMenu);
  close_callback_.Run();

  base::RecordAction(base::UserMetricsAction(
      direction == SnapDirection::kPrimary ? kPartialSplitTwoThirdsUserAction
                                           : kPartialSplitOneThirdUserAction));
  RecordMultitaskMenuActionType(MultitaskMenuActionType::kPartialSplitButton);
}

void MultitaskMenuView::FullScreenButtonPressed() {
  auto* widget = views::Widget::GetWidgetForNativeWindow(window_);
  widget->SetFullscreen(!widget->IsFullscreen());
  close_callback_.Run();
  RecordMultitaskMenuActionType(MultitaskMenuActionType::kFullscreenButton);
}

void MultitaskMenuView::FloatButtonPressed() {
  FloatControllerBase::Get()->ToggleFloat(window_);
  close_callback_.Run();
  RecordMultitaskMenuActionType(MultitaskMenuActionType::kFloatButton);
}

BEGIN_METADATA(MultitaskMenuView, View)
END_METADATA

}  // namespace chromeos
