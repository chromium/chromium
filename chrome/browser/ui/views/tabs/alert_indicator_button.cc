// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/alert_indicator_button.h"

#include <utility>

#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/user_metrics.h"
#include "base/time/time.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_slot_controller.h"
#include "chrome/grit/generated_resources.h"
#include "components/content_settings/core/common/features.h"
#include "components/vector_icons/vector_icons.h"
#include "media/base/media_switches.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/pointer/touch_ui_controller.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/animation/multi_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/animation_delegate_views.h"
#include "ui/views/metrics.h"
#include "ui/views/view_class_properties.h"

namespace {

// Fade-in/out duration for the tab indicator animations.  Fade-in is quick to
// immediately notify the user.  Fade-out is more gradual, so that the user has
// a chance of finding a tab that has quickly "blipped" on and off.
constexpr auto kIndicatorFadeInDuration = base::Milliseconds(200);
constexpr auto kIndicatorFadeOutDuration = base::Milliseconds(1000);

// A minimum delay before the alert indicator disappears.
constexpr auto kAlertIndicatorMinimumHoldDuration = base::Seconds(5);

// Interval between frame updates of the tab indicator animations.  This is not
// the usual 60 FPS because a trade-off must be made between tab UI animation
// smoothness and media recording/playback performance on low-end hardware.
constexpr base::TimeDelta kIndicatorFrameInterval =
    base::Milliseconds(50);  // 20 FPS

std::unique_ptr<gfx::MultiAnimation> CreateTabRecordingIndicatorAnimation() {
  // Number of times the throbber fades in and out. After these cycles a final
  // fade-in animation is played to end visible.
  constexpr size_t kFadeInFadeOutCycles = 2;

  gfx::MultiAnimation::Parts parts;
  for (size_t i = 0; i < kFadeInFadeOutCycles; ++i) {
    // Fade-in:
    parts.push_back(gfx::MultiAnimation::Part(kIndicatorFadeInDuration,
                                              gfx::Tween::EASE_IN));
    // Fade-out (from 1 to 0):
    parts.push_back(gfx::MultiAnimation::Part(kIndicatorFadeOutDuration,
                                              gfx::Tween::EASE_IN, 1.0, 0.0));
  }
  // Finish by fading in to show the indicator.
  parts.push_back(
      gfx::MultiAnimation::Part(kIndicatorFadeInDuration, gfx::Tween::EASE_IN));

  auto animation =
      std::make_unique<gfx::MultiAnimation>(parts, kIndicatorFrameInterval);
  animation->set_continuous(false);
  return animation;
}

// The minimum required click-to-select area of an inactive Tab before allowing
// the click-to-mute functionality to be enabled.  These values are in terms of
// some percentage of the AlertIndicatorButton's width.  See comments in
// UpdateEnabledForMuteToggle().
const int kMinMouseSelectableAreaPercent = 250;
const int kMinGestureSelectableAreaPercent = 400;

// Returns true if either Shift or Control are being held down.  In this case,
// mouse events are delegated to the Tab, to perform tab selection in the tab
// strip instead.
bool IsShiftOrControlDown(const ui::Event& event) {
  return (event.flags() & (ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN)) != 0;
}

ui::ImageModel GetTabAlertIndicatorImageForPressedState(
    TabAlertState alert_state,
    ui::ColorId button_color) {
  switch (alert_state) {
    case TabAlertState::AUDIO_PLAYING:
      return AlertIndicatorButton::GetTabAlertIndicatorImage(
          TabAlertState::AUDIO_MUTING, button_color);
    case TabAlertState::AUDIO_MUTING:
      return AlertIndicatorButton::GetTabAlertIndicatorImage(
          TabAlertState::AUDIO_PLAYING, button_color);
    case TabAlertState::MEDIA_RECORDING:
    case TabAlertState::AUDIO_RECORDING:
    case TabAlertState::VIDEO_RECORDING:
    case TabAlertState::TAB_CAPTURING:
    case TabAlertState::BLUETOOTH_CONNECTED:
    case TabAlertState::USB_CONNECTED:
    case TabAlertState::PIP_PLAYING:
    case TabAlertState::DESKTOP_CAPTURING:
    case TabAlertState::BLUETOOTH_SCAN_ACTIVE:
    case TabAlertState::HID_CONNECTED:
    case TabAlertState::SERIAL_CONNECTED:
    case TabAlertState::VR_PRESENTING_IN_HEADSET:
      return AlertIndicatorButton::GetTabAlertIndicatorImage(alert_state,
                                                             button_color);
  }
  NOTREACHED();
}

}  // namespace

class AlertIndicatorButton::FadeAnimationDelegate
    : public views::AnimationDelegateViews {
 public:
  explicit FadeAnimationDelegate(AlertIndicatorButton* button)
      : AnimationDelegateViews(button), button_(button) {}
  FadeAnimationDelegate(const FadeAnimationDelegate&) = delete;
  FadeAnimationDelegate& operator=(const FadeAnimationDelegate&) = delete;
  ~FadeAnimationDelegate() override = default;

 private:
  // views::AnimationDelegateViews
  void AnimationProgressed(const gfx::Animation* animation) override {
    button_->SchedulePaint();
  }

  void AnimationCanceled(const gfx::Animation* animation) override {
    AnimationEnded(animation);
  }

  void AnimationEnded(const gfx::Animation* animation) override {
    button_->showing_alert_state_ = button_->alert_state_;
    button_->SchedulePaint();
    button_->parent_tab_->AlertStateChanged();
  }

  const raw_ptr<AlertIndicatorButton> button_;
};

AlertIndicatorButton::AlertIndicatorButton(Tab* parent_tab)
    : parent_tab_(parent_tab) {
  DCHECK(parent_tab_);
  SetEventTargeter(std::make_unique<views::ViewTargeter>(this));

  GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_ACCNAME_MUTE_TAB));

  SetProperty(views::kElementIdentifierKey, kTabAlertIndicatorButtonElementId);
}

AlertIndicatorButton::~AlertIndicatorButton() = default;

void AlertIndicatorButton::TransitionToAlertState(
    std::optional<TabAlertState> next_state) {
  if (next_state == alert_state_) {
    return;
  }

  std::optional<TabAlertState> previous_alert_showing_state =
      showing_alert_state_;

  if (next_state) {
    UpdateIconForAlertState(next_state.value());
  }

  if ((alert_state_ == TabAlertState::AUDIO_PLAYING &&
       next_state == TabAlertState::AUDIO_MUTING) ||
      (alert_state_ == TabAlertState::AUDIO_MUTING &&
       next_state == TabAlertState::AUDIO_PLAYING)) {
    // Instant user feedback: No fade animation.
    showing_alert_state_ = next_state;
    fade_animation_.reset();
  } else {
    if (!next_state) {
      showing_alert_state_ = alert_state_;  // Fading-out indicator.
    } else {
      showing_alert_state_ = next_state;  // Fading-in to next indicator.
    }
    fade_animation_ = CreateTabAlertIndicatorFadeAnimation(next_state);
    if (!fade_animation_delegate_) {
      fade_animation_delegate_ = std::make_unique<FadeAnimationDelegate>(this);
    }
    fade_animation_->set_delegate(fade_animation_delegate_.get());
    fade_animation_->Start();
  }

  alert_state_ = next_state;

  if (previous_alert_showing_state != showing_alert_state_) {
    parent_tab_->AlertStateChanged();
  }

  UpdateEnabledForMuteToggle();
}

void AlertIndicatorButton::UpdateEnabledForMuteToggle() {
  const bool was_enabled = GetEnabled();

  bool enable = base::FeatureList::IsEnabled(media::kEnableTabMuting) &&
                (alert_state_ == TabAlertState::AUDIO_PLAYING ||
                 alert_state_ == TabAlertState::AUDIO_MUTING);

  // If the tab is not the currently-active tab, make sure it is wide enough
  // before enabling click-to-mute.  This ensures that there is enough click
  // area for the user to activate a tab rather than unintentionally muting it.
  // Note that IsTriggerableEvent() is also overridden to provide an even wider
  // requirement for tap gestures.
  if (enable && !GetTab()->IsActive()) {
    const int required_width = width() * kMinMouseSelectableAreaPercent / 100;
    enable = GetTab()->GetWidthOfLargestSelectableRegion() >= required_width;
  }

  if (enable == was_enabled) {
    return;
  }

  SetEnabled(enable);
}

void AlertIndicatorButton::OnParentTabButtonColorChanged() {
  if (alert_state_ == TabAlertState::AUDIO_PLAYING ||
      alert_state_ == TabAlertState::AUDIO_MUTING) {
    UpdateIconForAlertState(alert_state_.value());
  }
}

views::View* AlertIndicatorButton::GetTooltipHandlerForPoint(
    const gfx::Point& point) {
  return nullptr;  // Tab (the parent View) provides the tooltip.
}

bool AlertIndicatorButton::OnMousePressed(const ui::MouseEvent& event) {
  // Do not handle this mouse event when anything but the left mouse button is
  // pressed or when any modifier keys are being held down.  Instead, the Tab
  // should react (e.g., middle-click for close, right-click for context menu).
  if (!event.IsOnlyLeftMouseButton() || IsShiftOrControlDown(event)) {
    return false;  // Event to be handled by Tab.
  }

  return ImageButton::OnMousePressed(event);
}

void AlertIndicatorButton::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  UpdateEnabledForMuteToggle();
}

bool AlertIndicatorButton::DoesIntersectRect(const views::View* target,
                                             const gfx::Rect& rect) const {
  // If this button is not enabled, Tab (the parent View) handles all mouse
  // events.
  return GetEnabled() &&
         views::ViewTargeterDelegate::DoesIntersectRect(target, rect);
}

void AlertIndicatorButton::NotifyClick(const ui::Event& event) {
  // Call TransitionToAlertState() to change the image, providing the user with
  // instant feedback.  In the very unlikely event that the mute toggle fails,
  // TransitionToAlertState() will be called again, via another code path, to
  // set the image to be consistent with the final outcome.
  if (alert_state_ == TabAlertState::AUDIO_PLAYING) {
    base::RecordAction(base::UserMetricsAction("AlertIndicatorButton_Mute"));
    TransitionToAlertState(TabAlertState::AUDIO_MUTING);
  } else {
    DCHECK(alert_state_ == TabAlertState::AUDIO_MUTING);
    base::RecordAction(base::UserMetricsAction("AlertIndicatorButton_Unmute"));
    TransitionToAlertState(TabAlertState::AUDIO_PLAYING);
  }

  GetTab()->controller()->ToggleTabAudioMute(GetTab());
}

bool AlertIndicatorButton::IsTriggerableEvent(const ui::Event& event) {
  // For mouse events, only trigger on the left mouse button and when no
  // modifier keys are being held down.
  if (event.IsMouseEvent() &&
      (!static_cast<const ui::MouseEvent*>(&event)->IsOnlyLeftMouseButton() ||
       IsShiftOrControlDown(event))) {
    return false;
  }

  // For gesture events on an inactive tab, require an even wider tab before
  // click-to-mute can be triggered.  See comments in
  // UpdateEnabledForMuteToggle().
  if (event.IsGestureEvent() && !GetTab()->IsActive()) {
    const int required_width = width() * kMinGestureSelectableAreaPercent / 100;
    if (GetTab()->GetWidthOfLargestSelectableRegion() < required_width) {
      return false;
    }
  }

  return views::ImageButton::IsTriggerableEvent(event);
}

void AlertIndicatorButton::PaintButtonContents(gfx::Canvas* canvas) {
  double opaqueness = 1.0;
  if (fade_animation_) {
    opaqueness = fade_animation_->GetCurrentValue();
    if (!alert_state_) {
      opaqueness = 1.0 - opaqueness;  // Fading out, not in.
    }
  }
  if (opaqueness < 1.0) {
    canvas->SaveLayerAlpha(opaqueness * SK_AlphaOPAQUE);
  }
  ImageButton::PaintButtonContents(canvas);
  if (opaqueness < 1.0) {
    canvas->Restore();
  }
}

gfx::ImageSkia AlertIndicatorButton::GetImageToPaint() {
  return views::ImageButton::GetImageToPaint();
}

std::unique_ptr<gfx::Animation>
AlertIndicatorButton::CreateTabAlertIndicatorFadeAnimation(
    std::optional<TabAlertState> alert_state) {
  if (alert_state == TabAlertState::MEDIA_RECORDING ||
      alert_state == TabAlertState::AUDIO_RECORDING ||
      alert_state == TabAlertState::VIDEO_RECORDING ||
      alert_state == TabAlertState::TAB_CAPTURING ||
      alert_state == TabAlertState::DESKTOP_CAPTURING) {
    if (base::FeatureList::IsEnabled(
            content_settings::features::kImprovedSemanticsActivityIndicators) &&
        (alert_state == TabAlertState::MEDIA_RECORDING ||
         alert_state == TabAlertState::AUDIO_RECORDING ||
         alert_state == TabAlertState::VIDEO_RECORDING) &&
        camera_mic_indicator_start_time_ == base::Time()) {
      camera_mic_indicator_start_time_ = base::Time::Now();
    }

    return CreateTabRecordingIndicatorAnimation();
  }

  // TODO(pbos): Investigate if this functionality can be pushed down into a
  // parent class somehow, or leave a better paper trail of why doing so is not
  // feasible.
  // Note: While it seems silly to use a one-part MultiAnimation, it's the only
  // gfx::Animation implementation that lets us control the frame interval.
  gfx::MultiAnimation::Parts parts;
  const bool is_for_fade_in = alert_state.has_value();

  if (base::FeatureList::IsEnabled(
          content_settings::features::kImprovedSemanticsActivityIndicators) &&
      !is_for_fade_in && camera_mic_indicator_start_time_ != base::Time()) {
    base::TimeDelta delay =
        base::Time::Now() - camera_mic_indicator_start_time_;
    camera_mic_indicator_start_time_ = base::Time();

    // `delay` should not be smaller than `kIndicatorFadeOutDuration`.
    delay = std::max(kAlertIndicatorMinimumHoldDuration - delay,
                     kIndicatorFadeOutDuration);

    fadeout_animation_duration_for_testing_ = delay;
    parts.push_back(gfx::MultiAnimation::Part(delay, gfx::Tween::EASE_IN));
  } else {
    parts.push_back(gfx::MultiAnimation::Part(
        is_for_fade_in ? kIndicatorFadeInDuration : kIndicatorFadeOutDuration,
        gfx::Tween::EASE_IN));
  }

  auto animation =
      std::make_unique<gfx::MultiAnimation>(parts, kIndicatorFrameInterval);
  animation->set_continuous(false);
  return std::move(animation);
}

Tab* AlertIndicatorButton::GetTab() {
  DCHECK_EQ(static_cast<views::View*>(parent_tab_), parent());
  return parent_tab_;
}

// Returns a cached image, to be shown by the alert indicator for the given
// |alert_state|.  Uses the global ui::ResourceBundle shared instance.
ui::ImageModel AlertIndicatorButton::GetTabAlertIndicatorImage(
    TabAlertState alert_state,
    ui::ColorId button_color) {
  const gfx::VectorIcon* icon = nullptr;
  int image_width = GetLayoutConstant(TAB_ALERT_INDICATOR_ICON_WIDTH);
  switch (alert_state) {
    case TabAlertState::AUDIO_PLAYING:
        icon = &vector_icons::kVolumeUpChromeRefreshIcon;
      break;
    case TabAlertState::AUDIO_MUTING:
        icon = &vector_icons::kVolumeOffChromeRefreshIcon;
      break;
    case TabAlertState::MEDIA_RECORDING:
    case TabAlertState::AUDIO_RECORDING:
    case TabAlertState::VIDEO_RECORDING:
    case TabAlertState::DESKTOP_CAPTURING:
        icon = &vector_icons::kRadioButtonCheckedIcon;
      break;
    case TabAlertState::TAB_CAPTURING:
        icon = &vector_icons::kCaptureIcon;

      // Tab capturing and presenting icon uses a different width compared to
      // the other tab alert indicator icons.
      image_width = GetLayoutConstant(TAB_ALERT_INDICATOR_CAPTURE_ICON_WIDTH);
      break;
    case TabAlertState::BLUETOOTH_CONNECTED:
        icon = &vector_icons::kBluetoothConnectedIcon;
      break;
    case TabAlertState::BLUETOOTH_SCAN_ACTIVE:
        icon = &vector_icons::kBluetoothScanningChromeRefreshIcon;
      break;
    case TabAlertState::USB_CONNECTED:
        icon = &vector_icons::kUsbChromeRefreshIcon;
      icon = &kTabUsbConnectedIcon;
      break;
    case TabAlertState::HID_CONNECTED:
        icon = &vector_icons::kVideogameAssetChromeRefreshIcon;
      break;
    case TabAlertState::SERIAL_CONNECTED:
        icon = &vector_icons::kSerialPortChromeRefreshIcon;
      break;
    case TabAlertState::PIP_PLAYING:
        icon = &vector_icons::kPictureInPictureAltIcon;
      break;
    case TabAlertState::VR_PRESENTING_IN_HEADSET:
        icon = &vector_icons::kCardboardIcon;
      break;
  }
  DCHECK(icon);
  return ui::ImageModel::FromVectorIcon(*icon, button_color, image_width);
}

ui::ImageModel AlertIndicatorButton::GetTabAlertIndicatorImageForHoverCard(
    TabAlertState alert_state) {
  switch (alert_state) {
    case TabAlertState::MEDIA_RECORDING:
    case TabAlertState::AUDIO_RECORDING:
    case TabAlertState::VIDEO_RECORDING:
    case TabAlertState::DESKTOP_CAPTURING:
      return AlertIndicatorButton::GetTabAlertIndicatorImage(
          alert_state, kColorHoverCardTabAlertMediaRecordingIcon);
    case TabAlertState::TAB_CAPTURING:
    case TabAlertState::PIP_PLAYING:
      return AlertIndicatorButton::GetTabAlertIndicatorImage(
          alert_state, kColorHoverCardTabAlertPipPlayingIcon);
    case TabAlertState::AUDIO_PLAYING:
    case TabAlertState::AUDIO_MUTING:
    case TabAlertState::BLUETOOTH_CONNECTED:
    case TabAlertState::BLUETOOTH_SCAN_ACTIVE:
    case TabAlertState::USB_CONNECTED:
    case TabAlertState::HID_CONNECTED:
    case TabAlertState::SERIAL_CONNECTED:
    case TabAlertState::VR_PRESENTING_IN_HEADSET:
      return AlertIndicatorButton::GetTabAlertIndicatorImage(
          alert_state, kColorHoverCardTabAlertAudioPlayingIcon);
  }
  NOTREACHED();
}

void AlertIndicatorButton::UpdateIconForAlertState(TabAlertState state) {
  const ui::ColorId color = parent_tab_->GetAlertIndicatorColor(state);
  const ui::ImageModel indicator_image =
      GetTabAlertIndicatorImage(state, color);
  SetImageModel(views::Button::STATE_NORMAL, indicator_image);
  SetImageModel(views::Button::STATE_DISABLED, indicator_image);
  SetImageModel(views::Button::STATE_PRESSED,
                GetTabAlertIndicatorImageForPressedState(state, color));
}

BEGIN_METADATA(AlertIndicatorButton)
END_METADATA
