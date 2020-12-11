// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/alert_indicator.h"

#include <utility>

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/pointer/touch_ui_controller.h"
#include "ui/gfx/animation/multi_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/animation/animation_delegate_views.h"

namespace {

// Fade-in/out duration for the tab indicator animations.  Fade-in is quick to
// immediately notify the user.  Fade-out is more gradual, so that the user has
// a chance of finding a tab that has quickly "blipped" on and off.
constexpr auto kIndicatorFadeInDuration =
    base::TimeDelta::FromMilliseconds(200);
constexpr auto kIndicatorFadeOutDuration =
    base::TimeDelta::FromMilliseconds(1000);

// Interval between frame updates of the tab indicator animations.  This is not
// the usual 60 FPS because a trade-off must be made between tab UI animation
// smoothness and media recording/playback performance on low-end hardware.
constexpr base::TimeDelta kIndicatorFrameInterval =
    base::TimeDelta::FromMilliseconds(50);  // 20 FPS

// Animation that throbs in (towards 1.0) and out (towards 0.0), and ends in the
// "in" state.
class TabRecordingIndicatorAnimation : public gfx::MultiAnimation {
 public:
  TabRecordingIndicatorAnimation(const gfx::MultiAnimation::Parts& parts,
                                 const base::TimeDelta interval)
      : MultiAnimation(parts, interval) {}
  ~TabRecordingIndicatorAnimation() override = default;

  // Overridden to provide alternating "towards in" and "towards out" behavior.
  double GetCurrentValue() const override;

  static std::unique_ptr<TabRecordingIndicatorAnimation> Create();
};

double TabRecordingIndicatorAnimation::GetCurrentValue() const {
  return current_part_index() % 2 ? 1.0 - MultiAnimation::GetCurrentValue()
                                  : MultiAnimation::GetCurrentValue();
}

std::unique_ptr<TabRecordingIndicatorAnimation>
TabRecordingIndicatorAnimation::Create() {
  // Number of times to "toggle throb" the recording and tab capture indicators
  // when they first appear.
  constexpr size_t kCaptureIndicatorThrobCycles = 5;

  MultiAnimation::Parts parts;
  static_assert(
      kCaptureIndicatorThrobCycles % 2 != 0,
      "odd number of cycles required so animation finishes in showing state");
  for (size_t i = 0; i < kCaptureIndicatorThrobCycles; ++i) {
    parts.push_back(MultiAnimation::Part(
        i % 2 ? kIndicatorFadeOutDuration : kIndicatorFadeInDuration,
        gfx::Tween::EASE_IN));
  }

  auto animation = std::make_unique<TabRecordingIndicatorAnimation>(
      parts, kIndicatorFrameInterval);
  animation->set_continuous(false);
  return animation;
}

// Returns a cached image, to be shown by the alert indicator for the given
// |alert_state|.  Uses the global ui::ResourceBundle shared instance.
gfx::Image GetTabAlertIndicatorImage(TabAlertState alert_state,
                                     SkColor button_color) {
  const gfx::VectorIcon* icon = nullptr;
  int image_width = GetLayoutConstant(TAB_ALERT_INDICATOR_ICON_WIDTH);
  const bool touch_ui = ui::TouchUiController::Get()->touch_ui();
  switch (alert_state) {
    case TabAlertState::AUDIO_PLAYING:
      icon = touch_ui ? &kTabAudioRoundedIcon : &kTabAudioIcon;
      break;
    case TabAlertState::AUDIO_MUTING:
      icon = touch_ui ? &kTabAudioMutingRoundedIcon : &kTabAudioMutingIcon;
      break;
    case TabAlertState::MEDIA_RECORDING:
    case TabAlertState::DESKTOP_CAPTURING:
      icon = &kTabMediaRecordingIcon;
      break;
    case TabAlertState::TAB_CAPTURING:
      icon =
          touch_ui ? &kTabMediaCapturingWithArrowIcon : &kTabMediaCapturingIcon;
      // Tab capturing and presenting icon uses a different width compared to
      // the other tab alert indicator icons.
      image_width = GetLayoutConstant(TAB_ALERT_INDICATOR_CAPTURE_ICON_WIDTH);
      break;
    case TabAlertState::BLUETOOTH_CONNECTED:
      icon = &kTabBluetoothConnectedIcon;
      break;
    case TabAlertState::BLUETOOTH_SCAN_ACTIVE:
      icon = &kTabBluetoothScanActiveIcon;
      break;
    case TabAlertState::USB_CONNECTED:
      icon = &kTabUsbConnectedIcon;
      break;
    case TabAlertState::HID_CONNECTED:
      icon = &vector_icons::kVideogameAssetIcon;
      break;
    case TabAlertState::SERIAL_CONNECTED:
      // TODO(https://crbug.com/917204): This icon is too large to fit properly
      // as a tab indicator and should be replaced.
      icon = &vector_icons::kSerialPortIcon;
      break;
    case TabAlertState::PIP_PLAYING:
      icon = &kPictureInPictureAltIcon;
      break;
    case TabAlertState::VR_PRESENTING_IN_HEADSET:
      icon = &vector_icons::kVrHeadsetIcon;
      break;
  }
  DCHECK(icon);
  return gfx::Image(gfx::CreateVectorIcon(*icon, image_width, button_color));
}

// Returns a non-continuous Animation that performs a fade-in or fade-out
// appropriate for the given |next_alert_state|.  This is used by the tab alert
// indicator to alert the user that recording, tab capture, or audio playback
// has started/stopped.
std::unique_ptr<gfx::Animation> CreateTabAlertIndicatorFadeAnimation(
    base::Optional<TabAlertState> alert_state) {
  if (alert_state == TabAlertState::MEDIA_RECORDING ||
      alert_state == TabAlertState::TAB_CAPTURING ||
      alert_state == TabAlertState::DESKTOP_CAPTURING) {
    return TabRecordingIndicatorAnimation::Create();
  }

  // Note: While it seems silly to use a one-part MultiAnimation, it's the only
  // gfx::Animation implementation that lets us control the frame interval.
  gfx::MultiAnimation::Parts parts;
  const bool is_for_fade_in = (alert_state.has_value());
  parts.push_back(gfx::MultiAnimation::Part(
      is_for_fade_in ? kIndicatorFadeInDuration : kIndicatorFadeOutDuration,
      gfx::Tween::EASE_IN));
  auto animation =
      std::make_unique<gfx::MultiAnimation>(parts, kIndicatorFrameInterval);
  animation->set_continuous(false);
  return std::move(animation);
}

}  // namespace

class AlertIndicator::FadeAnimationDelegate
    : public views::AnimationDelegateViews {
 public:
  explicit FadeAnimationDelegate(AlertIndicator* indicator)
      : AnimationDelegateViews(indicator), indicator_(indicator) {}
  FadeAnimationDelegate(const FadeAnimationDelegate&) = delete;
  FadeAnimationDelegate& operator=(const FadeAnimationDelegate&) = delete;
  ~FadeAnimationDelegate() override = default;

 private:
  // views::AnimationDelegateViews
  void AnimationProgressed(const gfx::Animation* animation) override {
    indicator_->SchedulePaint();
  }

  void AnimationCanceled(const gfx::Animation* animation) override {
    AnimationEnded(animation);
  }

  void AnimationEnded(const gfx::Animation* animation) override {
    indicator_->showing_alert_state_ = indicator_->alert_state_;
    indicator_->SchedulePaint();
    indicator_->parent_tab_->AlertStateChanged();
  }

  AlertIndicator* const indicator_;
};

AlertIndicator::AlertIndicator(Tab* parent_tab)
    : views::ImageView(), parent_tab_(parent_tab) {
  DCHECK(parent_tab_);
}

AlertIndicator::~AlertIndicator() {}

void AlertIndicator::OnPaint(gfx::Canvas* canvas) {
  double opaqueness = 1.0;
  if (fade_animation_) {
    opaqueness = fade_animation_->GetCurrentValue();
    if (!alert_state_)
      opaqueness = 1.0 - opaqueness;  // Fading out, not in.
  }
  if (opaqueness < 1.0)
    canvas->SaveLayerAlpha(opaqueness * SK_AlphaOPAQUE);
  ImageView::OnPaint(canvas);
  if (opaqueness < 1.0)
    canvas->Restore();
}

void AlertIndicator::TransitionToAlertState(
    base::Optional<TabAlertState> next_state) {
  if (next_state == alert_state_)
    return;

  base::Optional<TabAlertState> previous_alert_showing_state =
      showing_alert_state_;

  if (next_state)
    ResetImage(next_state.value());

  if ((alert_state_ == TabAlertState::AUDIO_PLAYING &&
       next_state == TabAlertState::AUDIO_MUTING) ||
      (alert_state_ == TabAlertState::AUDIO_MUTING &&
       next_state == TabAlertState::AUDIO_PLAYING)) {
    // Instant user feedback: No fade animation.
    showing_alert_state_ = next_state;
    fade_animation_.reset();
  } else {
    if (!next_state)
      showing_alert_state_ = alert_state_;  // Fading-out indicator.
    else
      showing_alert_state_ = next_state;  // Fading-in to next indicator.
    fade_animation_ = CreateTabAlertIndicatorFadeAnimation(next_state);
    if (!fade_animation_delegate_)
      fade_animation_delegate_ = std::make_unique<FadeAnimationDelegate>(this);
    fade_animation_->set_delegate(fade_animation_delegate_.get());
    fade_animation_->Start();
  }

  alert_state_ = next_state;

  if (previous_alert_showing_state != showing_alert_state_)
    parent_tab_->AlertStateChanged();
}

void AlertIndicator::OnParentTabButtonColorChanged() {
  if (alert_state_ == TabAlertState::AUDIO_PLAYING ||
      alert_state_ == TabAlertState::AUDIO_MUTING)
    ResetImage(alert_state_.value());
}

views::View* AlertIndicator::GetTooltipHandlerForPoint(
    const gfx::Point& point) {
  return nullptr;  // Tab (the parent View) provides the tooltip.
}

void AlertIndicator::ResetImage(TabAlertState state) {
  SkColor color = parent_tab_->GetAlertIndicatorColor(state);
  gfx::ImageSkia image = GetTabAlertIndicatorImage(state, color).AsImageSkia();
  SetImage(&image);
}
