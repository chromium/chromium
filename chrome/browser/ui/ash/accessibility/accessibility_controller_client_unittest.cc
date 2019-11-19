// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/accessibility/accessibility_controller_client.h"

#include "ash/public/cpp/accessibility_controller_enums.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "chrome/browser/ui/ash/accessibility/fake_accessibility_controller.h"
#include "chromeos/audio/chromeos_sounds.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_service_manager_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_enums.mojom.h"

namespace {

constexpr base::TimeDelta kShutdownSoundDuration =
    base::TimeDelta::FromMilliseconds(1000);

class FakeAccessibilityControllerClient : public AccessibilityControllerClient {
 public:
  FakeAccessibilityControllerClient() = default;
  ~FakeAccessibilityControllerClient() override = default;

  // AccessibilityControllerClient:
  void TriggerAccessibilityAlert(ash::AccessibilityAlert alert) override {
    last_a11y_alert_ = alert;
  }
  void PlayEarcon(int32_t sound_key) override { last_sound_key_ = sound_key; }
  base::TimeDelta PlayShutdownSound() override {
    return kShutdownSoundDuration;
  }
  void HandleAccessibilityGesture(ax::mojom::Gesture gesture) override {
    last_a11y_gesture_ = gesture;
  }
  bool ToggleDictation() override {
    ++toggle_dictation_count_;
    dictation_on_ = !dictation_on_;
    return dictation_on_;
  }
  void SilenceSpokenFeedback() override { ++silence_spoken_feedback_count_; }
  void OnTwoFingerTouchStart() override { ++on_two_finger_touch_start_count_; }
  void OnTwoFingerTouchStop() override { ++on_two_finger_touch_stop_count_; }
  bool ShouldToggleSpokenFeedbackViaTouch() const override { return true; }
  void PlaySpokenFeedbackToggleCountdown(int tick_count) override {
    spoken_feedback_toggle_count_down_ = tick_count;
  }
  void RequestSelectToSpeakStateChange() override {
    ++select_to_speak_state_changes_;
  }

  ash::AccessibilityAlert last_a11y_alert_ = ash::AccessibilityAlert::NONE;
  int32_t last_sound_key_ = -1;
  ax::mojom::Gesture last_a11y_gesture_ = ax::mojom::Gesture::kNone;
  int toggle_dictation_count_ = 0;
  int silence_spoken_feedback_count_ = 0;
  int on_two_finger_touch_start_count_ = 0;
  int on_two_finger_touch_stop_count_ = 0;
  int spoken_feedback_toggle_count_down_ = -1;
  int select_to_speak_state_changes_ = 0;

 private:
  bool dictation_on_ = false;
  DISALLOW_COPY_AND_ASSIGN(FakeAccessibilityControllerClient);
};

}  // namespace

class AccessibilityControllerClientTest : public testing::Test {
 public:
  AccessibilityControllerClientTest() = default;
  ~AccessibilityControllerClientTest() override = default;

 private:
  content::BrowserTaskEnvironment task_environment_;
  content::TestServiceManagerContext context_;

  DISALLOW_COPY_AND_ASSIGN(AccessibilityControllerClientTest);
};

TEST_F(AccessibilityControllerClientTest, MethodCalls) {
  FakeAccessibilityController controller;
  FakeAccessibilityControllerClient client;

  // Tests client is set.
  EXPECT_TRUE(controller.was_client_set());

  // Tests TriggerAccessibilityAlert method call.
  const ash::AccessibilityAlert alert = ash::AccessibilityAlert::SCREEN_ON;
  client.TriggerAccessibilityAlert(alert);
  EXPECT_EQ(alert, client.last_a11y_alert_);

  // Tests PlayEarcon method call.
  const int32_t sound_key = chromeos::SOUND_SHUTDOWN;
  client.PlayEarcon(sound_key);
  EXPECT_EQ(sound_key, client.last_sound_key_);

  // Tests PlayShutdownSound method call.
  EXPECT_EQ(kShutdownSoundDuration, client.PlayShutdownSound());

  // Tests HandleAccessibilityGesture method call.
  ax::mojom::Gesture gesture = ax::mojom::Gesture::kClick;
  client.HandleAccessibilityGesture(gesture);
  EXPECT_EQ(gesture, client.last_a11y_gesture_);

  // Tests ToggleDictation method call.
  EXPECT_EQ(0, client.toggle_dictation_count_);
  EXPECT_TRUE(client.ToggleDictation());
  EXPECT_EQ(1, client.toggle_dictation_count_);

  EXPECT_EQ(0, client.silence_spoken_feedback_count_);
  client.SilenceSpokenFeedback();
  EXPECT_EQ(1, client.silence_spoken_feedback_count_);

  // Tests OnTwoFingerTouchStart method call.
  EXPECT_EQ(0, client.on_two_finger_touch_start_count_);
  client.OnTwoFingerTouchStart();
  EXPECT_EQ(1, client.on_two_finger_touch_start_count_);

  // Tests OnTwoFingerTouchStop method call.
  EXPECT_EQ(0, client.on_two_finger_touch_stop_count_);
  client.OnTwoFingerTouchStop();
  EXPECT_EQ(1, client.on_two_finger_touch_stop_count_);

  // Tests ShouldToggleSpokenFeedbackViaTouch method call.
  EXPECT_TRUE(client.ShouldToggleSpokenFeedbackViaTouch());

  // Tests PlaySpokenFeedbackToggleCountdown method call.
  const int tick_count = 2;
  client.PlaySpokenFeedbackToggleCountdown(tick_count);
  EXPECT_EQ(tick_count, client.spoken_feedback_toggle_count_down_);

  // Tests RequestSelectToSpeakStateChange method call.
  client.RequestSelectToSpeakStateChange();
  EXPECT_EQ(1, client.select_to_speak_state_changes_);
}
