// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ui/frame/multitask_menu/multitask_menu_view.h"

#include <memory>
#include <vector>

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
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/display/screen.h"
#include "ui/events/types/event_type.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/background.h"
#include "ui/views/controls/label.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/public/activation_client.h"

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
    int label_message_id,
    int label_max_width) {
  auto container = std::make_unique<views::BoxLayoutView>();

  // TODO(crbug.com/40232718): See View::SetLayoutManagerUseConstrainedSpace.
  container->SetLayoutManagerUseConstrainedSpace(false);
  container->SetOrientation(views::BoxLayout::Orientation::kVertical);
  container->SetBetweenChildSpacing(kCenterPadding);
  container->AddChildView(std::move(button_view));
  views::Label* label = container->AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(label_message_id)));
  label->SetElideBehavior(gfx::ElideBehavior::ELIDE_TAIL);
  label->SetEnabledColorId(ui::kColorSysOnSurface);
  label->SetFontList(gfx::FontList({"Roboto"}, gfx::Font::NORMAL,
                                   kLabelFontSize, gfx::Font::Weight::NORMAL));
  label->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  label->SetMaximumWidthSingleLine(label_max_width);
  return container;
}

}  // namespace

// -----------------------------------------------------------------------------
// MultitaskMenuView::MenuPreTargetHandler:
// Auto-closes the multitask menu on click outside or after timeout.
class MultitaskMenuView::MenuPreTargetHandler : public ui::EventHandler {
 public:
  MenuPreTargetHandler(views::Widget* menu_widget,
                       base::RepeatingClosure dismiss_callback,
                       views::View* anchor_view)
      : menu_widget_(menu_widget),
        anchor_view_(anchor_view),
        dismiss_callback_(std::move(dismiss_callback)) {
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

    if (event->type() == ui::EventType::kMousePressed) {
      ProcessPressedEvent(*event);
      return;
    }

    if (event->type() == ui::EventType::kMouseMoved && anchor_view_) {
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

    if (event->type() == ui::EventType::kTouchPressed) {
      ProcessPressedEvent(*event);
    }
  }

  void ProcessPressedEvent(const ui::LocatedEvent& event) {
    const gfx::Point screen_location = event.target()->GetScreenLocation(event);
    // If the event is out of menu bounds, close the menu.
    if (!menu_widget_->GetWindowBoundsInScreen().Contains(screen_location)) {
      dismiss_callback_.Run();
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

  void OnFadeOutFinished() { dismiss_callback_.Run(); }

  // The widget of the multitask menu that is currently shown. Guaranteed to
  // outlive `this`, which will get destroyed when the menu is destructed in
  // `close_callback_`.
  const raw_ptr<views::Widget> menu_widget_;

  // The anchor of the menu's widget if it exists. Set if there is an anchor and
  // we want the menu to close if the mouse has exited the menu bounds.
  raw_ptr<views::View, DanglingUntriaged> anchor_view_ = nullptr;

  base::OneShotTimer exit_timer_;

  base::RepeatingClosure dismiss_callback_;

  // Chrome's compiler toolchain enforces that any `WeakPtrFactory`
  // fields are declared last, to avoid destruction ordering issues.
  base::WeakPtrFactory<MenuPreTargetHandler> weak_factory_{this};
};

// -----------------------------------------------------------------------------
// MultitaskMenuView:

MultitaskMenuView::MultitaskMenuView(aura::Window* window,
                                     base::RepeatingClosure close_callback,
                                     base::RepeatingClosure dismiss_callback,
                                     uint8_t buttons,
                                     views::View* anchor_view)
    : window_(window),
      anchor_view_(anchor_view),
      close_callback_(std::move(close_callback)),
      dismiss_callback_(std::move(dismiss_callback)) {
  DCHECK(window);
  DCHECK(close_callback_);
  DCHECK(dismiss_callback_);
  SetBackground(views::CreateThemedSolidBackground(ui::kColorSysSurface3));
  SetUseDefaultFillLayout(true);

  window_observation_.Observe(window);

  // The display orientation. This determines whether menu is in
  // landscape/portrait mode.
  const bool is_portrait_mode = !display::Screen::GetScreen()
                                     ->GetDisplayNearestWindow(window)
                                     .is_landscape();
  const gfx::Size preferred_size = is_portrait_mode
                                       ? kMultitaskButtonPortraitSize
                                       : kMultitaskButtonLandscapeSize;
  const int label_max_length = preferred_size.width();

  // Half button.
  if (buttons & kHalfSplit) {
    auto half_button = std::make_unique<SplitButtonView>(
        SplitButtonView::SplitButtonType::kHalfButtons,
        base::BindRepeating(&MultitaskMenuView::HalfButtonPressed,
                            base::Unretained(this)),
        window, is_portrait_mode);
    half_button->SetPreferredSize(preferred_size);
    half_button_ = half_button.get();
    AddChildView(CreateButtonContainer(std::move(half_button),
                                       IDS_MULTITASK_MENU_HALF_BUTTON_NAME,
                                       label_max_length));
  }

  // Partial button.
  if (buttons & kPartialSplit) {
    auto partial_button = std::make_unique<SplitButtonView>(
        SplitButtonView::SplitButtonType::kPartialButtons,
        base::BindRepeating(&MultitaskMenuView::PartialButtonPressed,
                            base::Unretained(this)),
        window, is_portrait_mode);
    partial_button->SetPreferredSize(preferred_size);
    partial_button_ = partial_button.get();
    AddChildView(CreateButtonContainer(std::move(partial_button),
                                       IDS_MULTITASK_MENU_PARTIAL_BUTTON_NAME,
                                       label_max_length));
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
    full_button->SetPreferredSize(preferred_size);
    full_button_ = full_button.get();
    AddChildView(CreateButtonContainer(std::move(full_button), message_id,
                                       label_max_length));
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
    float_button->SetPreferredSize(preferred_size);
    float_button_ = float_button.get();
    AddChildView(CreateButtonContainer(std::move(float_button), message_id,
                                       label_max_length));
  }

  AddAccelerator(ui::Accelerator(ui::VKEY_MENU, ui::EF_ALT_DOWN));
}

MultitaskMenuView::~MultitaskMenuView() {
  event_handler_.reset();
}

void MultitaskMenuView::OnSizeButtonDrag(
    const gfx::Point& event_screen_location) {
  auto update_button_state = [event_screen_location](views::Button* button) {
    if (!button || !button->GetEnabled()) {
      return;
    }

    const views::Button::ButtonState state =
        button->GetBoundsInScreen().Contains(event_screen_location)
            ? views::Button::STATE_HOVERED
            : views::Button::STATE_NORMAL;
    button->SetState(state);
  };

  update_button_state(full_button_);
  update_button_state(float_button_);

  if (half_button_) {
    update_button_state(half_button_->GetLeftTopButton());
    update_button_state(half_button_->GetRightBottomButton());
  }
  if (partial_button_) {
    update_button_state(partial_button_->GetLeftTopButton());
    update_button_state(partial_button_->GetRightBottomButton());
  }
}

bool MultitaskMenuView::OnSizeButtonRelease(
    const gfx::Point& event_screen_location) {
  auto event_on_button = [event_screen_location](views::Button* button) {
    return button && button->GetEnabled() &&
           button->GetBoundsInScreen().Contains(event_screen_location);
  };

  // For multitask buttons, if they contain the release events run their
  // callback.
  if (event_on_button(full_button_)) {
    FullScreenButtonPressed();
    return true;
  }
  if (event_on_button(float_button_)) {
    FloatButtonPressed();
    return true;
  }

  // For split buttons, check the individual buttons.
  if (half_button_) {
    if (event_on_button(half_button_->GetLeftTopButton())) {
      HalfButtonPressed(GetSnapDirectionForWindow(window_, /*left_top=*/true));
      return true;
    }
    if (event_on_button(half_button_->GetRightBottomButton())) {
      HalfButtonPressed(GetSnapDirectionForWindow(window_, /*left_top=*/false));
      return true;
    }
  }
  if (partial_button_) {
    if (event_on_button(partial_button_->GetLeftTopButton())) {
      PartialButtonPressed(
          GetSnapDirectionForWindow(window_, /*left_top=*/true));
      return true;
    }
    if (event_on_button(partial_button_->GetRightBottomButton())) {
      PartialButtonPressed(
          GetSnapDirectionForWindow(window_, /*left_top=*/false));
      return true;
    }
  }

  return false;
}

void MultitaskMenuView::AddedToWidget() {
  // When the menu widget is shown, we install `MenuPreTargetHandler` to close
  // the menu on any events outside.
  event_handler_ = std::make_unique<MenuPreTargetHandler>(
      GetWidget(), dismiss_callback_, anchor_view_);
}

bool MultitaskMenuView::AcceleratorPressed(const ui::Accelerator& accelerator) {
  CHECK_EQ(ui::VKEY_MENU, accelerator.key_code());
  is_reversed_ = !is_reversed_;

  if (partial_button_) {
    // Update the visual appearance of the split buttons. The callbacks will be
    // updated in `PartialButtonPressed()`.
    partial_button_->UpdateButtons(/*is_portrait_mode=*/
                                   !display::Screen::GetScreen()
                                        ->GetDisplayNearestWindow(window_)
                                        .is_landscape(),
                                   is_reversed_);
  }

  if (float_button_) {
    // The callback will be updated in `FloatButtonPressed()`.
    float_button_->SetMirrored(is_reversed_);
    float_button_->SchedulePaint();
  }

  return true;
}

bool MultitaskMenuView::OnKeyPressed(const ui::KeyEvent& event) {
  // Eat up and down key events so they don't trigger keyboard traversal.
  if (event.key_code() == ui::VKEY_UP || event.key_code() == ui::VKEY_DOWN) {
    return true;
  }
  return views::View::OnKeyPressed(event);
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

void MultitaskMenuView::HalfButtonPressed(SnapDirection direction) {
  wm::GetActivationClient(window_->GetRootWindow())->ActivateWindow(window_);
  SnapController::Get()->CommitSnap(
      window_, direction, kDefaultSnapRatio,
      SnapController::SnapRequestSource::kWindowLayoutMenu);
  close_callback_.Run();
  base::RecordAction(base::UserMetricsAction(
      direction == SnapDirection::kPrimary ? kHalfSplitPrimaryUserAction
                                           : kHalfSplitSecondaryUserAction));
  RecordMultitaskMenuActionType(MultitaskMenuActionType::kHalfSplitButton);
}

void MultitaskMenuView::PartialButtonPressed(SnapDirection direction) {
  wm::GetActivationClient(window_->GetRootWindow())->ActivateWindow(window_);
  const bool is_primary_display_layout = chromeos::IsDisplayLayoutPrimary(
      display::Screen::GetScreen()->GetDisplayNearestWindow(window_));
  const bool is_primary_partial_split =
      (is_primary_display_layout && direction == SnapDirection::kPrimary) ||
      (!is_primary_display_layout && direction == SnapDirection::kSecondary);
  SnapController::Get()->CommitSnap(
      window_, direction,
      is_primary_partial_split ? (is_reversed_ ? chromeos::kOneThirdSnapRatio
                                               : chromeos::kTwoThirdSnapRatio)
                               : (is_reversed_ ? chromeos::kTwoThirdSnapRatio
                                               : chromeos::kOneThirdSnapRatio),
      SnapController::SnapRequestSource::kWindowLayoutMenu);
  close_callback_.Run();
  base::RecordAction(base::UserMetricsAction(
      is_primary_partial_split ? kPartialSplitTwoThirdsUserAction
                               : kPartialSplitOneThirdUserAction));
  RecordMultitaskMenuActionType(MultitaskMenuActionType::kPartialSplitButton);
}

void MultitaskMenuView::FullScreenButtonPressed() {
  wm::GetActivationClient(window_->GetRootWindow())->ActivateWindow(window_);
  auto* widget = views::Widget::GetWidgetForNativeWindow(window_);
  const bool is_fullscreen = widget->IsFullscreen();
  widget->SetFullscreen(!is_fullscreen);
  close_callback_.Run();
  base::RecordAction(base::UserMetricsAction(
      is_fullscreen ? kExitFullscreenUserAction : kFullscreenUserAction));
  RecordMultitaskMenuActionType(MultitaskMenuActionType::kFullscreenButton);
}

void MultitaskMenuView::FloatButtonPressed() {
  wm::GetActivationClient(window_->GetRootWindow())->ActivateWindow(window_);
  if (window_->GetProperty(kWindowStateTypeKey) == WindowStateType::kFloated) {
    base::RecordAction(base::UserMetricsAction(kUnFloatUserAction));
    FloatControllerBase::Get()->UnsetFloat(window_);
  } else {
    base::RecordAction(base::UserMetricsAction(kFloatUserAction));
    FloatControllerBase::Get()->SetFloat(
        window_, is_reversed_ ? FloatStartLocation::kBottomLeft
                              : FloatStartLocation::kBottomRight);
  }

  close_callback_.Run();
  RecordMultitaskMenuActionType(MultitaskMenuActionType::kFloatButton);
}

BEGIN_METADATA(MultitaskMenuView)
END_METADATA

}  // namespace chromeos
