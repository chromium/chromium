// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ui/frame/caption_buttons/frame_size_button.h"

#include <memory>

#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/user_metrics.h"
#include "chromeos/ui/base/window_properties.h"
#include "chromeos/ui/frame/caption_buttons/snap_controller.h"
#include "chromeos/ui/frame/frame_utils.h"
#include "chromeos/ui/frame/multitask_menu/multitask_menu.h"
#include "chromeos/ui/frame/multitask_menu/multitask_menu_nudge_controller.h"
#include "chromeos/utils/haptics_util.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/base/hit_test.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/display/screen.h"
#include "ui/display/tablet_state.h"
#include "ui/events/devices/haptic_touchpad_effects.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/views/animation/animation_delegate_views.h"
#include "ui/views/animation/compositor_animation_runner.h"
#include "ui/views/widget/widget.h"

namespace chromeos {

namespace {

// The default delay between the user pressing the size button and the buttons
// adjacent to the size button morphing into buttons for snapping left and
// right.
constexpr base::TimeDelta kSetButtonsToSnapModeDelay = base::Milliseconds(150);

// The amount that a user can overshoot one of the caption buttons while in
// "snap mode" and keep the button hovered/pressed.
constexpr int kMaxOvershootX = 200;
constexpr int kMaxOvershootY = 50;

// TODO(b/277770052): Adjust the press duration to reflect the shorter overall
// time to activate the multitask menu.
constexpr base::TimeDelta kPieAnimationPressDuration = base::Milliseconds(150);
constexpr base::TimeDelta kPieAnimationHoverDuration = base::Milliseconds(500);

// Returns true if a mouse drag while in "snap mode" at |location_in_screen|
// would hover/press |button| or keep it hovered/pressed.
bool HitTestButton(const views::FrameCaptionButton* button,
                   const gfx::Point& location_in_screen) {
  gfx::Rect expanded_bounds_in_screen = button->GetBoundsInScreen();
  if (button->GetState() == views::Button::STATE_HOVERED ||
      button->GetState() == views::Button::STATE_PRESSED) {
    expanded_bounds_in_screen.Inset(
        gfx::Insets::VH(-kMaxOvershootY, -kMaxOvershootX));
  }
  return expanded_bounds_in_screen.Contains(location_in_screen);
}

SnapDirection GetSnapDirection(const views::FrameCaptionButton* to_hover) {
  if (!to_hover)
    return SnapDirection::kNone;

  aura::Window* window = to_hover->GetWidget()->GetNativeWindow();
  switch (to_hover->GetIcon()) {
    case views::CAPTION_BUTTON_ICON_LEFT_TOP_SNAPPED:
      return GetSnapDirectionForWindow(window, /*left_top=*/true);
    case views::CAPTION_BUTTON_ICON_RIGHT_BOTTOM_SNAPPED:
      return GetSnapDirectionForWindow(window, /*left_top=*/false);
    case views::CAPTION_BUTTON_ICON_MAXIMIZE_RESTORE:
    case views::CAPTION_BUTTON_ICON_MINIMIZE:
    case views::CAPTION_BUTTON_ICON_CLOSE:
    case views::CAPTION_BUTTON_ICON_BACK:
    case views::CAPTION_BUTTON_ICON_LOCATION:
    case views::CAPTION_BUTTON_ICON_MENU:
    case views::CAPTION_BUTTON_ICON_ZOOM:
    case views::CAPTION_BUTTON_ICON_CENTER:
    case views::CAPTION_BUTTON_ICON_FLOAT:
    case views::CAPTION_BUTTON_ICON_CUSTOM:
    case views::CAPTION_BUTTON_ICON_COUNT:
      NOTREACHED_IN_MIGRATION();
      return SnapDirection::kNone;
  }
}

}  // namespace

// This view controls animating a pie on a parent button which indicates when
// long press or long hover will end.
class FrameSizeButton::PieAnimationView : public views::View,
                                          public views::AnimationDelegateViews {
  METADATA_HEADER(PieAnimationView, views::View)

 public:
  explicit PieAnimationView(FrameSizeButton* button)
      : views::AnimationDelegateViews(this), button_(button) {
    SetCanProcessEventsWithinSubtree(false);
    animation_.SetTweenType(gfx::Tween::LINEAR);
  }
  PieAnimationView(const PieAnimationView&) = delete;
  PieAnimationView& operator=(const PieAnimationView&) = delete;
  ~PieAnimationView() override = default;

  void Start(base::TimeDelta duration, MultitaskMenuEntryType entry_type) {
    entry_type_ = entry_type;

    const double animation_value =
        entry_type_ == MultitaskMenuEntryType::kFrameSizeButtonLongPress
            ? animation_.GetCurrentValue()
            : 0.0;

    animation_.Reset(animation_value);
    // `SlideAnimation` is unaffected by debug tools such as
    // "--ui-slow-animations" flag, so manually multiply the duration here. Note
    // that this will also cause `AnimationEnded` to run immediately if the test
    // is using zero duration. If we are partially through the animation when
    // the button is pressed, then we want the duration to be relative to the
    // percentage of the animation that still needs to be completed. For
    // example, if we are 1/4 through the animation when pressed, then we want
    // the remaining animation to only take 3/4 of the full long press duration.
    animation_.SetSlideDuration(
        ui::ScopedAnimationDurationScaleMode::duration_multiplier() * duration *
        (1 - animation_value));
    animation_.Show();
  }

  void Stop() {
    animation_.Reset(0.0);
    SchedulePaint();
  }

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override {
    if (!GetWidget()) {
      return;
    }

    const double animation_value = animation_.GetCurrentValue();
    if (animation_value == 0.0) {
      return;
    }

    // The pie is a filled arc which starts at the top and sweeps around
    // clockwise.
    const SkScalar start_angle = -90.f;
    const SkScalar sweep_angle = 360.f * animation_value;

    SkPath path;
    const gfx::Rect bounds = GetLocalBounds();
    path.moveTo(bounds.CenterPoint().x(), bounds.CenterPoint().y());
    path.arcTo(gfx::RectToSkRect(bounds), start_angle, sweep_angle,
               /*forceMoveTo=*/false);
    path.close();

    cc::PaintFlags flags;
    flags.setColor(
        GetWidget()->GetColorProvider()->GetColor(ui::kColorSysStateHover));
    flags.setAntiAlias(true);
    flags.setStyle(cc::PaintFlags::kFill_Style);
    canvas->DrawPath(path, flags);
  }

  // views::AnimationDelegateViews:
  void AnimationEnded(const gfx::Animation* animation) override {
    SchedulePaint();
    button_->ShowMultitaskMenu(entry_type_);
  }

  void AnimationProgressed(const gfx::Animation* animation) override {
    SchedulePaint();
  }

 private:
  gfx::SlideAnimation animation_{this};

  // Tracks the entry type that triggered the latests pie animation. Used for
  // recording metrics once the menu is shown.
  MultitaskMenuEntryType entry_type_ =
      MultitaskMenuEntryType::kFrameSizeButtonHover;

  // The button `this` is associated with. Unowned.
  const raw_ptr<FrameSizeButton> button_;
};

BEGIN_METADATA(FrameSizeButton, PieAnimationView)
END_METADATA

// The class to observe the to-be-snapped window during the waiting-for-snap
// mode. If the window's window state is changed or the window is put in
// overview during the waiting mode, cancel the snap.
class FrameSizeButton::SnappingWindowObserver : public aura::WindowObserver {
 public:
  SnappingWindowObserver(aura::Window* window, FrameSizeButton* size_button)
      : window_(window), size_button_(size_button) {
    window_->AddObserver(this);
  }

  SnappingWindowObserver(const SnappingWindowObserver&) = delete;
  SnappingWindowObserver& operator=(const SnappingWindowObserver&) = delete;

  ~SnappingWindowObserver() override {
    if (window_) {
      window_->RemoveObserver(this);
      window_ = nullptr;
    }
  }

  // aura::WindowObserver:
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override {
    DCHECK_EQ(window_, window);
    if ((key == chromeos::kIsShowingInOverviewKey &&
         window_->GetProperty(chromeos::kIsShowingInOverviewKey)) ||
        key == chromeos::kWindowStateTypeKey) {
      // If the window is put in overview while we're in waiting-for-snapping
      // mode, or the window's window state has changed, cancel the snap.
      size_button_->CancelSnap();
    }
  }

  void OnWindowDestroying(aura::Window* window) override {
    DCHECK_EQ(window_, window);
    window_->RemoveObserver(this);
    window_ = nullptr;
    size_button_->CancelSnap();
  }

 private:
  raw_ptr<aura::Window> window_;
  raw_ptr<FrameSizeButton> size_button_;
};

FrameSizeButton::FrameSizeButton(PressedCallback callback,
                                 FrameSizeButtonDelegate* delegate)
    : views::FrameCaptionButton(std::move(callback),
                                views::CAPTION_BUTTON_ICON_MAXIMIZE_RESTORE,
                                HTMAXBUTTON),
      delegate_(delegate),
      long_tap_delay_(kSetButtonsToSnapModeDelay) {
  display_observer_.emplace(this);

  pie_animation_view_ = AddChildView(std::make_unique<PieAnimationView>(this));
}

FrameSizeButton::~FrameSizeButton() = default;

bool FrameSizeButton::IsMultitaskMenuShown() const {
  return multitask_menu_widget_ && !multitask_menu_widget_->IsClosed();
}

void FrameSizeButton::ShowMultitaskMenu(MultitaskMenuEntryType entry_type) {
  CHECK(!display::Screen::GetScreen()->InTabletMode());
  RecordMultitaskMenuEntryType(entry_type);
  // Owned by the bubble which contains this view. If there is an existing
  // bubble, it will be deactivated and then close and destroy itself.
  auto menu_delegate = std::make_unique<MultitaskMenu>(
      /*anchor=*/this, GetWidget(),
      /*close_on_move_out=*/entry_type ==
          MultitaskMenuEntryType::kFrameSizeButtonHover);
  multitask_menu_ = menu_delegate->GetWeakPtr();
  multitask_menu_widget_ = base::WrapUnique(
      views::BubbleDialogDelegateView::CreateBubble(std::move(menu_delegate)));
  multitask_menu_widget_->Show();
  delegate_->GetMultitaskMenuNudgeController()->OnMenuOpened(
      /*tablet_mode=*/false);

  haptics_util::PlayHapticTouchpadEffect(
      ui::HapticTouchpadEffect::kSnap,
      ui::HapticTouchpadEffectStrength::kMedium);
}

void FrameSizeButton::ToggleMultitaskMenu() {
  CHECK(!display::Screen::GetScreen()->InTabletMode());
  if (!multitask_menu_widget_) {
    ShowMultitaskMenu(MultitaskMenuEntryType::kAccel);
  } else {
    multitask_menu_widget_->Close();
  }
}

bool FrameSizeButton::OnMousePressed(const ui::MouseEvent& event) {
  if (IsTriggerableEvent(event)) {
    StartLongTapDelayTimer(event);
  }

  return views::FrameCaptionButton::OnMousePressed(event);
}

bool FrameSizeButton::OnMouseDragged(const ui::MouseEvent& event) {
  UpdateSnapPreview(event);
  // By default a FrameCaptionButton reverts to STATE_NORMAL once the mouse
  // leaves its bounds. Skip FrameCaptionButton's handling when
  // |in_snap_mode_| == true because we want different behavior.
  if (!in_snap_mode_)
    views::FrameCaptionButton::OnMouseDragged(event);

  if (multitask_menu_) {
    multitask_menu_->multitask_menu_view()->OnSizeButtonDrag(
        views::View::ConvertPointToScreen(this, event.location()));
  }

  return true;
}

void FrameSizeButton::OnMouseReleased(const ui::MouseEvent& event) {
  if (IsTriggerableEvent(event)) {
    CommitSnap(event);

    if (multitask_menu_) {
      multitask_menu_->multitask_menu_view()->OnSizeButtonRelease(
          views::View::ConvertPointToScreen(this, event.location()));
    }
  }

  pie_animation_view_->Stop();

  views::FrameCaptionButton::OnMouseReleased(event);
}

void FrameSizeButton::OnMouseCaptureLost() {
  SetButtonsToNormalMode(FrameSizeButtonDelegate::Animate::kYes);
  views::FrameCaptionButton::OnMouseCaptureLost();
}

void FrameSizeButton::OnMouseMoved(const ui::MouseEvent& event) {
  // Ignore any synthetic mouse moves during a drag.
  if (!in_snap_mode_)
    views::FrameCaptionButton::OnMouseMoved(event);
}

void FrameSizeButton::OnGestureEvent(ui::GestureEvent* event) {
  if (event->details().touch_points() > 1) {
    SetButtonsToNormalMode(FrameSizeButtonDelegate::Animate::kYes);
    return;
  }
  if (event->type() == ui::EventType::kGestureTapDown && delegate_->CanSnap() &&
      !display::Screen::GetScreen()->InTabletMode()) {
    StartLongTapDelayTimer(*event);

    // Go through FrameCaptionButton's handling so that the button gets pressed.
    views::FrameCaptionButton::OnGestureEvent(event);
    return;
  }

  if (event->type() == ui::EventType::kGestureScrollBegin ||
      event->type() == ui::EventType::kGestureScrollUpdate) {
    UpdateSnapPreview(*event);

    if (multitask_menu_) {
      multitask_menu_->multitask_menu_view()->OnSizeButtonDrag(
          views::View::ConvertPointToScreen(this, event->location()));
    }

    event->SetHandled();
    return;
  }

  if (event->type() == ui::EventType::kGestureTap ||
      event->type() == ui::EventType::kGestureScrollEnd ||
      event->type() == ui::EventType::kScrollFlingStart ||
      event->type() == ui::EventType::kGestureEnd) {
    if (multitask_menu_ && !multitask_menu_->GetWidget()->IsClosed() &&
        multitask_menu_->multitask_menu_view()->OnSizeButtonRelease(
            views::View::ConvertPointToScreen(this, event->location()))) {
      event->SetHandled();
      return;
    }

    if (CommitSnap(*event)) {
      event->SetHandled();
      return;
    }
  }

  views::FrameCaptionButton::OnGestureEvent(event);
}

void FrameSizeButton::StateChanged(views::Button::ButtonState old_state) {
  views::FrameCaptionButton::StateChanged(old_state);

  // Ignore if there is no native window, which can happen during widget
  // shutdown.
  if (!GetWidget()->GetNativeWindow()) {
    return;
  }

  // Pie animation will start on both active/inactive window.
  if (aura::client::CursorClient* cursor_client = aura::client::GetCursorClient(
          GetWidget()->GetNativeWindow()->GetRootWindow());
      cursor_client && cursor_client->IsCursorVisible() &&
      GetState() == views::Button::STATE_HOVERED) {
    // On animation end we should show the multitask menu.
    // Note that if the window is not active, after the pie animation this will
    // activate the window.
    StartPieAnimation(kPieAnimationHoverDuration,
                      MultitaskMenuEntryType::kFrameSizeButtonHover);
  } else if (old_state == views::Button::STATE_HOVERED &&
             GetState() != views::Button::STATE_PRESSED) {
    // We want to continue the animation if the button was pressed while it was
    // already hovered, so only stop in other instances.
    pie_animation_view_->Stop();
  }
}

void FrameSizeButton::Layout(PassKey) {
  // Use the bounds of the inkdrop for the pie animation.
  gfx::Rect bounds = GetLocalBounds();
  bounds.Inset(GetInkdropInsets(bounds.size()));
  pie_animation_view_->SetBoundsRect(bounds);

  LayoutSuperclass<views::FrameCaptionButton>(this);
}

void FrameSizeButton::OnDisplayTabletStateChanged(display::TabletState state) {
  if (state == display::TabletState::kEnteringTabletMode) {
    pie_animation_view_->Stop();
    long_tap_delay_timer_.Stop();
  }
}

void FrameSizeButton::StartLongTapDelayTimer(const ui::LocatedEvent& event) {
  const bool is_mouse = event.IsMouseEvent();
  if (long_tap_delay_.is_zero()) {
    OnLongTapDelayTimerEnded(is_mouse, event.location());
  } else {
    long_tap_delay_timer_.Start(
        FROM_HERE, long_tap_delay_,
        base::BindOnce(&FrameSizeButton::OnLongTapDelayTimerEnded,
                       base::Unretained(this), is_mouse, event.location()));
  }
}

void FrameSizeButton::StartPieAnimation(base::TimeDelta duration,
                                        MultitaskMenuEntryType entry_type) {
  if (display::Screen::GetScreen()->InTabletMode() || IsMultitaskMenuShown()) {
    return;
  }

  pie_animation_view_->Start(duration, entry_type);
}

void FrameSizeButton::AnimateButtonsToSnapMode() {
  SetButtonsToSnapMode(FrameSizeButtonDelegate::Animate::kYes);

  // Start observing the to-be-snapped window.
  snapping_window_observer_ = std::make_unique<SnappingWindowObserver>(
      GetWidget()->GetNativeWindow(), this);
}

void FrameSizeButton::SetButtonsToSnapMode(
    FrameSizeButtonDelegate::Animate animate) {
  DCHECK(!display::Screen::GetScreen()->InTabletMode());
  in_snap_mode_ = true;

  // When using a right-to-left layout the close button is left of the size
  // button and the minimize button is right of the size button.
  if (base::i18n::IsRTL()) {
    delegate_->SetButtonIcons(views::CAPTION_BUTTON_ICON_RIGHT_BOTTOM_SNAPPED,
                              views::CAPTION_BUTTON_ICON_LEFT_TOP_SNAPPED,
                              animate);
  } else {
    delegate_->SetButtonIcons(views::CAPTION_BUTTON_ICON_LEFT_TOP_SNAPPED,
                              views::CAPTION_BUTTON_ICON_RIGHT_BOTTOM_SNAPPED,
                              animate);
  }
}

void FrameSizeButton::UpdateSnapPreview(const ui::LocatedEvent& event) {
  if (!in_snap_mode_) {
    // Set the buttons adjacent to the size button to snap left and right early
    // if the user drags past the drag threshold.
    // |set_buttons_to_snap_mode_timer_| is checked to avoid entering the snap
    // mode as a result of an unsupported drag type (e.g. only the right mouse
    // button is pressed).
    gfx::Vector2d delta(event.location() -
                        set_buttons_to_snap_mode_timer_event_location_);
    if (!long_tap_delay_timer_.IsRunning() ||
        !views::View::ExceededDragThreshold(delta)) {
      return;
    }
    AnimateButtonsToSnapMode();
  }

  const views::FrameCaptionButton* to_hover = GetButtonToHover(event);
  SnapDirection snap = GetSnapDirection(to_hover);

  gfx::Point event_location_in_screen(event.location());
  views::View::ConvertPointToScreen(this, &event_location_in_screen);
  bool press_size_button =
      to_hover || HitTestButton(this, event_location_in_screen);

  if (to_hover) {
    // Progress the minimize and close icon morph animations to the end if they
    // are in progress.
    SetButtonsToSnapMode(FrameSizeButtonDelegate::Animate::kNo);
  }

  delegate_->SetHoveredAndPressedButtons(to_hover,
                                         press_size_button ? this : nullptr);
  delegate_->ShowSnapPreview(snap,
                             /*allow_haptic_feedback=*/event.IsMouseEvent());
}

const views::FrameCaptionButton* FrameSizeButton::GetButtonToHover(
    const ui::LocatedEvent& event) const {
  gfx::Point event_location_in_screen(event.location());
  views::View::ConvertPointToScreen(this, &event_location_in_screen);
  const views::FrameCaptionButton* closest_button =
      delegate_->GetButtonClosestTo(event_location_in_screen);
  if ((closest_button->GetIcon() ==
           views::CAPTION_BUTTON_ICON_LEFT_TOP_SNAPPED ||
       closest_button->GetIcon() ==
           views::CAPTION_BUTTON_ICON_RIGHT_BOTTOM_SNAPPED) &&
      HitTestButton(closest_button, event_location_in_screen)) {
    return closest_button;
  }
  return nullptr;
}

bool FrameSizeButton::CommitSnap(const ui::LocatedEvent& event) {
  snapping_window_observer_.reset();
  SnapDirection snap = GetSnapDirection(GetButtonToHover(event));
  delegate_->CommitSnap(snap);
  delegate_->SetHoveredAndPressedButtons(nullptr, nullptr);

  if (snap == SnapDirection::kPrimary) {
    base::RecordAction(base::UserMetricsAction("MaxButton_MaxLeft"));
  } else if (snap == SnapDirection::kSecondary) {
    base::RecordAction(base::UserMetricsAction("MaxButton_MaxRight"));
  } else {
    SetButtonsToNormalMode(FrameSizeButtonDelegate::Animate::kYes);
    return false;
  }

  SetButtonsToNormalMode(FrameSizeButtonDelegate::Animate::kNo);
  return true;
}

void FrameSizeButton::CancelSnap() {
  snapping_window_observer_.reset();
  delegate_->CommitSnap(SnapDirection::kNone);
  delegate_->SetHoveredAndPressedButtons(nullptr, nullptr);
  SetButtonsToNormalMode(FrameSizeButtonDelegate::Animate::kYes);
}

void FrameSizeButton::SetButtonsToNormalMode(
    FrameSizeButtonDelegate::Animate animate) {
  in_snap_mode_ = false;
  pie_animation_view_->Stop();
  long_tap_delay_timer_.Stop();
  delegate_->SetButtonsToNormal(animate);
}

void FrameSizeButton::OnLongTapDelayTimerEnded(bool is_mouse,
                                               const gfx::Point& location) {
  StartPieAnimation(kPieAnimationPressDuration,
                    is_mouse
                        ? MultitaskMenuEntryType::kFrameSizeButtonLongPress
                        : MultitaskMenuEntryType::kFrameSizeButtonLongTouch);

  // The minimize and close buttons are set to snap left and right when
  // snapping is enabled. Do not enable snapping if the minimize button is not
  // visible. The close button is always visible.
  if (!in_snap_mode_ && delegate_->CanSnap() &&
      delegate_->IsMinimizeButtonVisible()) {
    set_buttons_to_snap_mode_timer_event_location_ = location;
    AnimateButtonsToSnapMode();
  }
}

BEGIN_METADATA(FrameSizeButton)
END_METADATA

}  // namespace chromeos
