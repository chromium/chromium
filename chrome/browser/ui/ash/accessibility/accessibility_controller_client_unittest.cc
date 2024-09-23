// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/accessibility/accessibility_controller_client.h"

#include <optional>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/public/cpp/accessibility_controller_enums.h"
#include "ash/test/ash_test_base.h"
#include "base/time/time.h"
#include "chromeos/ash/components/audio/sounds.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/gfx/geometry/point_f.h"

namespace {

using ::ash::Sound;

constexpr base::TimeDelta kShutdownSoundDuration = base::Milliseconds(1000);

class FakeAccessibilityControllerClient : public AccessibilityControllerClient {
 public:
  FakeAccessibilityControllerClient() = default;

  FakeAccessibilityControllerClient(const FakeAccessibilityControllerClient&) =
      delete;
  FakeAccessibilityControllerClient& operator=(
      const FakeAccessibilityControllerClient&) = delete;

  ~FakeAccessibilityControllerClient() override = default;

  // AccessibilityControllerClient:
  void TriggerAccessibilityAlert(ash::AccessibilityAlert alert) override {
    last_a11y_alert_ = alert;
  }
  void PlayEarcon(Sound sound_key) override { last_sound_key_ = sound_key; }
  base::TimeDelta PlayShutdownSound() override {
    return kShutdownSoundDuration;
  }
  void HandleAccessibilityGesture(ax::mojom::Gesture gesture,
                                  gfx::PointF location) override {
    last_a11y_gesture_ = gesture;
    last_a11y_gesture_point_ = location;
  }
  bool ToggleDictation() override {
    ++toggle_dictation_count_;
    dictation_on_ = !dictation_on_;
    return dictation_on_;
  }
  void SilenceSpokenFeedback() override { ++silence_spoken_feedback_count_; }
  bool ShouldToggleSpokenFeedbackViaTouch() const override { return true; }
  void PlaySpokenFeedbackToggleCountdown(int tick_count) override {
    spoken_feedback_toggle_count_down_ = tick_count;
  }
  void RequestSelectToSpeakStateChange() override {
    ++select_to_speak_state_changes_;
  }
  void OnSelectToSpeakPanelAction(ash::SelectToSpeakPanelAction action,
                                  double value) override {
    last_select_to_speak_panel_action_ = action;
    last_select_to_speak_panel_value_ = value;
  }

  ash::AccessibilityAlert last_a11y_alert_ = ash::AccessibilityAlert::NONE;
  std::optional<Sound> last_sound_key_;
  ax::mojom::Gesture last_a11y_gesture_ = ax::mojom::Gesture::kNone;
  gfx::PointF last_a11y_gesture_point_;
  int toggle_dictation_count_ = 0;
  int silence_spoken_feedback_count_ = 0;
  int on_two_finger_touch_start_count_ = 0;
  int on_two_finger_touch_stop_count_ = 0;
  int spoken_feedback_toggle_count_down_ = -1;
  int select_to_speak_state_changes_ = 0;
  ash::SelectToSpeakPanelAction last_select_to_speak_panel_action_ =
      ash::SelectToSpeakPanelAction::kNone;
  double last_select_to_speak_panel_value_ = 0.0;

 private:
  bool dictation_on_ = false;
};

}  // namespace

class AccessibilityControllerClientTest : public ash::AshTestBase {
 public:
  AccessibilityControllerClientTest() = default;

  AccessibilityControllerClientTest(const AccessibilityControllerClientTest&) =
      delete;
  AccessibilityControllerClientTest& operator=(
      const AccessibilityControllerClientTest&) = delete;

  ~AccessibilityControllerClientTest() override = default;
};

TEST_F(AccessibilityControllerClientTest, MethodCalls) {
  FakeAccessibilityControllerClient client;

  ash::AccessibilityController* controller =
      ash::AccessibilityController::Get();
  controller->SetClient(&client);

  // Tests TriggerAccessibilityAlert method call.
  const ash::AccessibilityAlert alert = ash::AccessibilityAlert::SCREEN_ON;
  client.TriggerAccessibilityAlert(alert);
  EXPECT_EQ(alert, client.last_a11y_alert_);

  // Tests PlayEarcon method call.
  const Sound sound_key = Sound::kShutdown;
  client.PlayEarcon(sound_key);
  EXPECT_EQ(sound_key, client.last_sound_key_);

  // Tests PlayShutdownSound method call.
  EXPECT_EQ(kShutdownSoundDuration, client.PlayShutdownSound());

  // Tests HandleAccessibilityGesture method call.
  ax::mojom::Gesture gesture = ax::mojom::Gesture::kClick;
  gfx::PointF gesture_point(1, 1);
  client.HandleAccessibilityGesture(gesture, gesture_point);
  EXPECT_EQ(gesture, client.last_a11y_gesture_);
  EXPECT_EQ(gesture_point, client.last_a11y_gesture_point_);

  // Tests ToggleDictation method call.
  EXPECT_EQ(0, client.toggle_dictation_count_);
  EXPECT_TRUE(client.ToggleDictation());
  EXPECT_EQ(1, client.toggle_dictation_count_);

  EXPECT_EQ(0, client.silence_spoken_feedback_count_);
  client.SilenceSpokenFeedback();
  EXPECT_EQ(1, client.silence_spoken_feedback_count_);

  // Tests ShouldToggleSpokenFeedbackViaTouch method call.
  EXPECT_TRUE(client.ShouldToggleSpokenFeedbackViaTouch());

  // Tests PlaySpokenFeedbackToggleCountdown method call.
  const int tick_count = 2;
  client.PlaySpokenFeedbackToggleCountdown(tick_count);
  EXPECT_EQ(tick_count, client.spoken_feedback_toggle_count_down_);

  // Tests RequestSelectToSpeakStateChange method call.
  client.RequestSelectToSpeakStateChange();
  EXPECT_EQ(1, client.select_to_speak_state_changes_);

  // Tests OnSelectToSpeakPanelAction method call.
  const ash::SelectToSpeakPanelAction action =
      ash::SelectToSpeakPanelAction::kChangeSpeed;
  double panel_value = 1.5;
  client.OnSelectToSpeakPanelAction(action, panel_value);
  EXPECT_EQ(action, client.last_select_to_speak_panel_action_);
  EXPECT_EQ(panel_value, client.last_select_to_speak_panel_value_);
}
