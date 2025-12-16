// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_ALERT_INDICATOR_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_ALERT_INDICATOR_BUTTON_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/lottie/animation.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/view_targeter_delegate.h"

namespace gfx {
class Animation;
class AnimationDelegate;
}  // namespace gfx

namespace views {
class AnimatedImageView;
}  // namespace views

namespace tabs {
enum class TabAlert;
}  // namespace tabs

// This is an ImageButton subclass that serves as both the alert indicator icon
// (audio, tab capture, etc.), and as a mute button. It is intended to be a
// child view of a tab, which must implement the Delegate interface.
//
// When the indicator is transitioned to the audio playing or muting state, the
// button functionality is enabled and begins handling mouse events.  Otherwise,
// this view behaves like an image and all mouse events will be handled by the
// parent tab.
class AlertIndicatorButton : public views::ImageButton,
                             public views::ViewTargeterDelegate {
  METADATA_HEADER(AlertIndicatorButton, views::ImageButton)

 public:
  // An interface for the parent tab of the alert indicator.
  class Delegate {
   public:
    // Whether click-to-mute should be enabled, given the required width for
    // activating the tab. Returns false only if the tab is inactive, and the
    // selectable region is smaller than the required width.
    virtual bool ShouldEnableMuteToggle(int required_width) = 0;

    // Toggles tab-wide audio muting.
    virtual void ToggleTabAudioMute() = 0;

    // Returns whether the tab appears more like the active state than the
    // inactive state, given the current opacity.
    virtual bool IsApparentlyActive() const = 0;

    // Called when the alert indicator has changed states.
    virtual void AlertStateChanged() = 0;
  };

  explicit AlertIndicatorButton(Delegate* delegate);
  AlertIndicatorButton(const AlertIndicatorButton&) = delete;
  AlertIndicatorButton& operator=(const AlertIndicatorButton&) = delete;
  ~AlertIndicatorButton() override;

  // Returns the current TabAlert except, while the indicator image is
  // fading out, returns the prior TabAlert.
  std::optional<tabs::TabAlert> showing_alert_state() const {
    return showing_alert_state_;
  }

  // Calls ResetImages(), starts fade animations, and activates/deactivates
  // button functionality as appropriate.
  void TransitionToAlertState(std::optional<tabs::TabAlert> next_state);

  // Determines whether the AlertIndicatorButton will be clickable for toggling
  // muting.  This should be called whenever the active/inactive state of a tab
  // has changed.  Internally, TransitionToAlertState() and OnBoundsChanged()
  // calls this when the TabAlert or the bounds have changed.
  void UpdateEnabledForMuteToggle();

  // Called when the parent tab's button color changes.  Determines whether
  // ResetImages() needs to be called.
  void OnParentTabButtonColorChanged();

  // Sets visibility of animation around alert indicator icon.
  void UpdateAlertIndicatorAnimation();

  // Returns the current TabAlert for testing.
  std::optional<tabs::TabAlert> alert_state_for_testing() const {
    return alert_state_;
  }

  // For testing purposes.
  views::AnimatedImageView* GetActorIndicatorSpinnerForTesting() {
    return actor_indicator_spinner_;
  }

 protected:
  // views::View:
  void OnThemeChanged() override;
  View* GetTooltipHandlerForPoint(const gfx::Point& point) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
  void Layout(PassKey) override;

  // views::ViewTargeterDelegate
  bool DoesIntersectRect(const View* target,
                         const gfx::Rect& rect) const override;

  // views::Button:
  void NotifyClick(const ui::Event& event) override;

  // views::Button:
  bool IsTriggerableEvent(const ui::Event& event) override;
  void PaintButtonContents(gfx::Canvas* canvas) override;

  // views::ImageButton:
  gfx::ImageSkia GetImageToPaint() override;

 private:
  friend class TabContentsTest;
  friend class TabTest;
  class FadeAnimationDelegate;

  // Returns a non-continuous Animation that performs a fade-in or fade-out
  // appropriate for the given `alert_state`.  This is used by the tab alert
  // indicator to alert the user that recording, tab capture, or audio playback
  // has started/stopped.
  std::unique_ptr<gfx::Animation> CreateTabAlertIndicatorFadeAnimation(
      std::optional<tabs::TabAlert> alert_state);

  // Resets the images to display on the button to reflect `state` and the
  // parent tab's button color.  Should be called when either of these changes.
  void UpdateIconForAlertState(tabs::TabAlert state);

  // Reloads the spinner and reapplies color and sizing when the theme changes.
  void UpdateSpinnerTheme();

  // Loads the resources needed for the actor_indicator_spinner, if not already
  // loaded.
  void MaybeLoadActorAccessingSpinner();

  // Sets the bounds for the actor_indicator_spinner depending on button
  // location and size.
  void SetActorAccessingSpinnerBounds();

  const raw_ptr<Delegate> delegate_;

  std::optional<tabs::TabAlert> alert_state_;

  // Alert indicator fade-in/out animation (i.e., only on show/hide, not a
  // continuous animation).
  std::unique_ptr<gfx::AnimationDelegate> fade_animation_delegate_;
  std::unique_ptr<gfx::Animation> fade_animation_;
  std::optional<tabs::TabAlert> showing_alert_state_;

  // The time when the alert indicator is displayed when a camera and/or a
  // microphone are captured.
  base::Time camera_mic_indicator_start_time_;
  // Duration of the fade-out animation to verify it in unit tests. Despite
  // `fade_animation_` being properly initialized, in tests, it does not show
  // the correct duration.
  base::TimeDelta fadeout_animation_duration_for_testing_;

  // The view that contains the spinner displayed around ACTOR_ACCESSING alert
  // icons.
  raw_ptr<views::AnimatedImageView> actor_indicator_spinner_;
  // The playback config for the actor_indicator_spinner.
  std::optional<lottie::Animation::PlaybackConfig> actor_indicator_config_;
  // The scaled size of the spinner, stored at creation time.
  std::optional<gfx::Size> actor_spinner_scaled_size_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_ALERT_INDICATOR_BUTTON_H_
