// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/accessibility/accessibility_controller_client.h"

#include "ash/public/interfaces/accessibility_controller.mojom.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "base/time/time.h"
#include "chromeos/audio/chromeos_sounds.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_enums.mojom.h"

namespace {

constexpr base::TimeDelta kShutdownSoundDuration =
    base::TimeDelta::FromMilliseconds(1000);

class TestAccessibilityController : ash::mojom::AccessibilityController {
 public:
  TestAccessibilityController() : binding_(this) {}
  ~TestAccessibilityController() override = default;

  ash::mojom::AccessibilityControllerPtr CreateInterfacePtr() {
    ash::mojom::AccessibilityControllerPtr ptr;
    binding_.Bind(mojo::MakeRequest(&ptr));
    return ptr;
  }

  // ash::mojom::AccessibilityController:
  void SetClient(ash::mojom::AccessibilityControllerClientPtr client) override {
    was_client_set_ = true;
  }
  void SetDarkenScreen(bool darken) override {}
  void BrailleDisplayStateChanged(bool connected) override {}
  void SetFocusHighlightRect(const gfx::Rect& bounds_in_screen) override {}
  void SetCaretBounds(const gfx::Rect& bounds_in_screen) override {}
  void SetAccessibilityPanelAlwaysVisible(bool always_visible) override {}
  void SetAccessibilityPanelBounds(
      const gfx::Rect& bounds,
      ash::mojom::AccessibilityPanelState state) override {}
  void SetSelectToSpeakState(ash::mojom::SelectToSpeakState state) override {}
  void SetSelectToSpeakEventHandlerDelegate(
      ash::mojom::SelectToSpeakEventHandlerDelegatePtr delegate) override {}
  void ToggleDictationFromSource(
      ash::mojom::DictationToggleSource source) override {}

  bool was_client_set() const { return was_client_set_; }

 private:
  mojo::Binding<ash::mojom::AccessibilityController> binding_;
  bool was_client_set_ = false;

  DISALLOW_COPY_AND_ASSIGN(TestAccessibilityController);
};

class FakeAccessibilityControllerClient : public AccessibilityControllerClient {
 public:
  FakeAccessibilityControllerClient() = default;
  ~FakeAccessibilityControllerClient() override = default;

  // AccessibilityControllerClient:
  void TriggerAccessibilityAlert(
      ash::mojom::AccessibilityAlert alert) override {
    last_a11y_alert_ = alert;
  }

  void PlayEarcon(int32_t sound_key) override { last_sound_key_ = sound_key; }

  void PlayShutdownSound(PlayShutdownSoundCallback callback) override {
    std::move(callback).Run(kShutdownSoundDuration);
  }

  void HandleAccessibilityGesture(ax::mojom::Gesture gesture) override {
    last_a11y_gesture_ = gesture;
  }

  void ToggleDictation(ToggleDictationCallback callback) override {
    ++toggle_dictation_count_;
    dictation_on_ = !dictation_on_;
    std::move(callback).Run(dictation_on_);
  }

  void SilenceSpokenFeedback() override { ++silence_spoken_feedback_count_; }

  void OnTwoFingerTouchStart() override { ++on_two_finger_touch_start_count_; }

  void OnTwoFingerTouchStop() override { ++on_two_finger_touch_stop_count_; }

  void ShouldToggleSpokenFeedbackViaTouch(
      ShouldToggleSpokenFeedbackViaTouchCallback callback) override {
    std::move(callback).Run(true);
  }

  void PlaySpokenFeedbackToggleCountdown(int tick_count) override {
    spoken_feedback_toggle_count_down_ = tick_count;
  }

  void RequestSelectToSpeakStateChange() override {
    ++select_to_speak_state_changes_;
  }

  ash::mojom::AccessibilityAlert last_a11y_alert_ =
      ash::mojom::AccessibilityAlert::NONE;
  int32_t last_sound_key_ = -1;
  ax::mojom::Gesture last_a11y_gesture_ = ax::mojom::Gesture::kNone;
  int toggle_dictation_count_ = 0;
  int silence_spoken_feedback_count_ = 0;
  int on_two_finger_touch_start_count_ = 0;
  int on_two_finger_touch_stop_count_ = 0;
  int spoken_feedback_toggle_count_down_ = -1;
  int select_to_speak_state_changes_ = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeAccessibilityControllerClient);
  bool dictation_on_ = false;
};

}  // namespace

class AccessibilityControllerClientTest : public testing::Test {
 public:
  AccessibilityControllerClientTest() = default;
  ~AccessibilityControllerClientTest() override = default;

 private:
  base::test::ScopedTaskEnvironment scoped_task_enviroment_;

  DISALLOW_COPY_AND_ASSIGN(AccessibilityControllerClientTest);
};

TEST_F(AccessibilityControllerClientTest, MethodCalls) {
  FakeAccessibilityControllerClient client;
  TestAccessibilityController controller;
  client.InitForTesting(controller.CreateInterfacePtr());
  client.FlushForTesting();

  // Tests client is set.
  EXPECT_TRUE(controller.was_client_set());

  // Tests TriggerAccessibilityAlert method call.
  const ash::mojom::AccessibilityAlert alert =
      ash::mojom::AccessibilityAlert::SCREEN_ON;
  client.TriggerAccessibilityAlert(alert);
  EXPECT_EQ(alert, client.last_a11y_alert_);

  // Tests PlayEarcon method call.
  const int32_t sound_key = chromeos::SOUND_SHUTDOWN;
  client.PlayEarcon(sound_key);
  EXPECT_EQ(sound_key, client.last_sound_key_);

  // Tests PlayShutdownSound method call.
  base::TimeDelta sound_duration;
  client.PlayShutdownSound(base::BindOnce(
      [](base::TimeDelta* dst, base::TimeDelta duration) { *dst = duration; },
      base::Unretained(&sound_duration)));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kShutdownSoundDuration, sound_duration);

  // Tests HandleAccessibilityGesture method call.
  ax::mojom::Gesture gesture = ax::mojom::Gesture::kClick;
  client.HandleAccessibilityGesture(gesture);
  EXPECT_EQ(gesture, client.last_a11y_gesture_);

  // Tests ToggleDictation method call.
  EXPECT_EQ(0, client.toggle_dictation_count_);
  client.ToggleDictation(base::BindOnce([](bool b) {}));
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
  bool should_toggle = false;
  client.ShouldToggleSpokenFeedbackViaTouch(base::BindOnce(
      [](bool* dst, bool should_toggle) { *dst = should_toggle; },
      base::Unretained(&should_toggle)));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(should_toggle);

  // Tests PlaySpokenFeedbackToggleCountdown method call.
  const int tick_count = 2;
  client.PlaySpokenFeedbackToggleCountdown(tick_count);
  EXPECT_EQ(tick_count, client.spoken_feedback_toggle_count_down_);

  // Tests RequestSelectToSpeakStateChange method call.
  client.RequestSelectToSpeakStateChange();
  EXPECT_EQ(1, client.select_to_speak_state_changes_);
}
