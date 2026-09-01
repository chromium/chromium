// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/exclusive_access/exclusive_access_bubble_android.h"

#include <memory>
#include <string>
#include <utility>

#include "base/android/jni_string.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/ui/android/exclusive_access/exclusive_access_context_android.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"
#include "url/gurl.h"
#include "url/origin.h"

using testing::_;
using testing::Return;

namespace {

class MockBridge : public ExclusiveAccessBubbleAndroid::Bridge {
 public:
  MOCK_METHOD(void, Show, (), (override));
  MOCK_METHOD(void, Hide, (), (override));
  MOCK_METHOD(void, Update, (const std::u16string& text), (override));
  MOCK_METHOD(bool, IsVisible, (), (const override));
  MOCK_METHOD(bool, IsKeyboardConnected, (), (const override));
};

using ExclusiveAccessBubbleAndroidTest = ChromeRenderViewHostTestHarness;

TEST_F(ExclusiveAccessBubbleAndroidTest, UpdateEarlyOutsWhenAlreadyShown) {
  ExclusiveAccessBubbleParams params;
  params.type = EXCLUSIVE_ACCESS_BUBBLE_TYPE_FULLSCREEN_EXIT_INSTRUCTION;

  auto mock_bridge = std::make_unique<MockBridge>();
  auto* mock_bridge_ptr = mock_bridge.get();

  EXPECT_CALL(*mock_bridge_ptr, IsVisible()).WillOnce(Return(false));
  EXPECT_CALL(*mock_bridge_ptr, IsKeyboardConnected()).WillOnce(Return(false));
  EXPECT_CALL(*mock_bridge_ptr, Update(_)).Times(1);
  EXPECT_CALL(*mock_bridge_ptr, Show()).Times(1);

  ExclusiveAccessBubbleAndroid bubble(params, base::DoNothing(),
                                      std::move(mock_bridge));

  testing::Mock::VerifyAndClearExpectations(mock_bridge_ptr);

  // Set visibility to true.
  EXPECT_CALL(*mock_bridge_ptr, IsVisible()).WillOnce(Return(true));

  // Update with same params should early out because already_shown is true.
  EXPECT_CALL(*mock_bridge_ptr, Update(_)).Times(0);
  EXPECT_CALL(*mock_bridge_ptr, Show()).Times(0);
  bubble.Update(params, base::DoNothing());

  EXPECT_CALL(*mock_bridge_ptr, Hide()).Times(1);
}

TEST_F(ExclusiveAccessBubbleAndroidTest, WasShownFlagPreventsResurrection) {
  ExclusiveAccessBubbleParams params;
  params.type = EXCLUSIVE_ACCESS_BUBBLE_TYPE_FULLSCREEN_EXIT_INSTRUCTION;

  auto mock_bridge = std::make_unique<MockBridge>();
  auto* mock_bridge_ptr = mock_bridge.get();

  EXPECT_CALL(*mock_bridge_ptr, IsVisible()).WillOnce(Return(false));
  EXPECT_CALL(*mock_bridge_ptr, IsKeyboardConnected()).WillOnce(Return(false));
  EXPECT_CALL(*mock_bridge_ptr, Update(_)).Times(1);
  EXPECT_CALL(*mock_bridge_ptr, Show()).Times(1);

  ExclusiveAccessBubbleAndroid bubble(params, base::DoNothing(),
                                      std::move(mock_bridge));

  testing::Mock::VerifyAndClearExpectations(mock_bridge_ptr);

  // Second update with same params should early out because was_shown_ is true.
  EXPECT_CALL(*mock_bridge_ptr, IsVisible()).WillOnce(Return(false));
  EXPECT_CALL(*mock_bridge_ptr, Update(_)).Times(0);
  EXPECT_CALL(*mock_bridge_ptr, Show()).Times(0);
  bubble.Update(params, base::DoNothing());

  testing::Mock::VerifyAndClearExpectations(mock_bridge_ptr);

  EXPECT_CALL(*mock_bridge_ptr, IsVisible()).WillOnce(Return(false));
  EXPECT_CALL(*mock_bridge_ptr, IsKeyboardConnected()).WillOnce(Return(false));
  EXPECT_CALL(*mock_bridge_ptr, Update(_)).Times(1);
  EXPECT_CALL(*mock_bridge_ptr, Show()).Times(1);

  params.force_update = true;
  bubble.Update(params, base::DoNothing());

  testing::Mock::VerifyAndClearExpectations(mock_bridge_ptr);

  // After Hide(), was_shown_ should be reset to false.
  EXPECT_CALL(*mock_bridge_ptr, Hide()).Times(1);
  bubble.HideImmediately();

  testing::Mock::VerifyAndClearExpectations(mock_bridge_ptr);

  // Now Update with same params should work again.
  params.force_update = false;
  EXPECT_CALL(*mock_bridge_ptr, IsVisible()).WillOnce(Return(false));
  EXPECT_CALL(*mock_bridge_ptr, IsKeyboardConnected()).WillOnce(Return(false));
  EXPECT_CALL(*mock_bridge_ptr, Update(_)).Times(1);
  EXPECT_CALL(*mock_bridge_ptr, Show()).Times(1);
  bubble.Update(params, base::DoNothing());

  EXPECT_CALL(*mock_bridge_ptr, Hide()).Times(1);
}

TEST_F(ExclusiveAccessBubbleAndroidTest, SnoozeResetForciblyReshowsNotice) {
  ExclusiveAccessBubbleParams params;
  params.type = EXCLUSIVE_ACCESS_BUBBLE_TYPE_FULLSCREEN_EXIT_INSTRUCTION;

  auto mock_bridge = std::make_unique<MockBridge>();
  auto* mock_bridge_ptr = mock_bridge.get();

  // Initial show on creation.
  EXPECT_CALL(*mock_bridge_ptr, IsVisible()).WillOnce(Return(false));
  EXPECT_CALL(*mock_bridge_ptr, IsKeyboardConnected()).WillOnce(Return(false));
  EXPECT_CALL(*mock_bridge_ptr, Update(_)).Times(1);
  EXPECT_CALL(*mock_bridge_ptr, Show()).Times(1);

  auto bubble = std::make_unique<ExclusiveAccessBubbleAndroid>(
      params, base::DoNothing(), std::move(mock_bridge));

  testing::Mock::VerifyAndClearExpectations(mock_bridge_ptr);

  ExclusiveAccessContextAndroid context;
  context.SetBubbleForTesting(std::move(bubble));

  // Verify that the first 9 user inputs don't trigger any show or update on the
  // bridge.
  for (int i = 1; i <= 9; ++i) {
    context.OnExclusiveAccessUserInput();
  }

  // The 10th user input exceeds the snooze interaction threshold and must
  // forcibly re-show the security notice regardless of whether it was
  // previously shown in this session (i.e. force_update is set to true to
  // override the was_shown_ latch).
  EXPECT_CALL(*mock_bridge_ptr, IsVisible()).WillOnce(Return(false));
  EXPECT_CALL(*mock_bridge_ptr, IsKeyboardConnected()).WillOnce(Return(false));
  EXPECT_CALL(*mock_bridge_ptr, Update(_)).Times(1);
  EXPECT_CALL(*mock_bridge_ptr, Show()).Times(1);

  context.OnExclusiveAccessUserInput();

  testing::Mock::VerifyAndClearExpectations(mock_bridge_ptr);
}

TEST_F(ExclusiveAccessBubbleAndroidTest,
       ParamsAccessorReturnsLiveUpdatedParams) {
  ExclusiveAccessBubbleParams initial_params;
  initial_params.type =
      EXCLUSIVE_ACCESS_BUBBLE_TYPE_FULLSCREEN_EXIT_INSTRUCTION;

  auto mock_bridge = std::make_unique<MockBridge>();
  auto* mock_bridge_ptr = mock_bridge.get();

  EXPECT_CALL(*mock_bridge_ptr, IsVisible()).WillOnce(Return(false));
  EXPECT_CALL(*mock_bridge_ptr, IsKeyboardConnected()).WillOnce(Return(false));
  EXPECT_CALL(*mock_bridge_ptr, Update(_)).Times(1);
  EXPECT_CALL(*mock_bridge_ptr, Show()).Times(1);

  ExclusiveAccessBubbleAndroid bubble(initial_params, base::DoNothing(),
                                      std::move(mock_bridge));
  testing::Mock::VerifyAndClearExpectations(mock_bridge_ptr);

  EXPECT_EQ(bubble.params().type,
            EXCLUSIVE_ACCESS_BUBBLE_TYPE_FULLSCREEN_EXIT_INSTRUCTION);

  // Dynamically update the bubble params (e.g. keyboard lock acquired).
  ExclusiveAccessBubbleParams update_params;
  update_params.type =
      EXCLUSIVE_ACCESS_BUBBLE_TYPE_KEYBOARD_LOCK_EXIT_INSTRUCTION;
  update_params.origin = url::Origin::Create(GURL("https://example.com"));

  EXPECT_CALL(*mock_bridge_ptr, IsVisible()).WillOnce(Return(false));
  EXPECT_CALL(*mock_bridge_ptr, IsKeyboardConnected()).WillOnce(Return(false));
  EXPECT_CALL(*mock_bridge_ptr, Update(_)).Times(1);
  EXPECT_CALL(*mock_bridge_ptr, Show()).Times(1);

  bubble.Update(update_params, base::DoNothing());
  testing::Mock::VerifyAndClearExpectations(mock_bridge_ptr);

  // In the buggy baseline where ExclusiveAccessBubbleAndroid shadows params_,
  // ExclusiveAccessBubble::params() returns the frozen base class member,
  // which still has EXCLUSIVE_ACCESS_BUBBLE_TYPE_FULLSCREEN_EXIT_INSTRUCTION
  // and an empty origin.
  EXPECT_EQ(bubble.params().type,
            EXCLUSIVE_ACCESS_BUBBLE_TYPE_KEYBOARD_LOCK_EXIT_INSTRUCTION);
  EXPECT_EQ(bubble.params().origin, update_params.origin);
}

TEST_F(ExclusiveAccessBubbleAndroidTest,
       SnoozeResetPreservesDynamicallyUpdatedParams) {
  ExclusiveAccessBubbleParams initial_params;
  initial_params.type =
      EXCLUSIVE_ACCESS_BUBBLE_TYPE_FULLSCREEN_EXIT_INSTRUCTION;

  auto mock_bridge = std::make_unique<MockBridge>();
  auto* mock_bridge_ptr = mock_bridge.get();

  EXPECT_CALL(*mock_bridge_ptr, IsVisible()).WillOnce(Return(false));
  EXPECT_CALL(*mock_bridge_ptr, IsKeyboardConnected())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_bridge_ptr, Update(_)).Times(1);
  EXPECT_CALL(*mock_bridge_ptr, Show()).Times(1);

  auto bubble = std::make_unique<ExclusiveAccessBubbleAndroid>(
      initial_params, base::DoNothing(), std::move(mock_bridge));
  auto* bubble_ptr = bubble.get();
  testing::Mock::VerifyAndClearExpectations(mock_bridge_ptr);

  // Transition to keyboard lock.
  ExclusiveAccessBubbleParams lock_params;
  lock_params.type =
      EXCLUSIVE_ACCESS_BUBBLE_TYPE_KEYBOARD_LOCK_EXIT_INSTRUCTION;

  EXPECT_CALL(*mock_bridge_ptr, IsVisible()).WillOnce(Return(false));
  EXPECT_CALL(*mock_bridge_ptr, IsKeyboardConnected())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_bridge_ptr, Update(_)).Times(1);
  EXPECT_CALL(*mock_bridge_ptr, Show()).Times(1);

  bubble_ptr->Update(lock_params, base::DoNothing());
  testing::Mock::VerifyAndClearExpectations(mock_bridge_ptr);

  ExclusiveAccessContextAndroid context;
  context.SetBubbleForTesting(std::move(bubble));

  for (int i = 1; i <= 9; ++i) {
    context.OnExclusiveAccessUserInput();
  }

  // The 10th input forces a re-show by reading bubble->params().
  // If params_ is shadowed, bubble->params() returns the initial FULLSCREEN
  // type, overwriting the live KEYBOARD_LOCK type and sending the wrong exit
  // text to the bridge.
  EXPECT_CALL(*mock_bridge_ptr, IsVisible()).WillOnce(Return(false));
  EXPECT_CALL(*mock_bridge_ptr, IsKeyboardConnected())
      .WillRepeatedly(Return(true));
  std::u16string expected_text =
      exclusive_access_bubble::GetInstructionTextForType(
          EXCLUSIVE_ACCESS_BUBBLE_TYPE_KEYBOARD_LOCK_EXIT_INSTRUCTION,
          l10n_util::GetStringUTF16(IDS_APP_ESC_KEY), std::nullopt,
          /*has_download=*/false, /*notify_overridden=*/false);
  EXPECT_CALL(*mock_bridge_ptr, Update(expected_text)).Times(1);
  EXPECT_CALL(*mock_bridge_ptr, Show()).Times(1);

  context.OnExclusiveAccessUserInput();
  testing::Mock::VerifyAndClearExpectations(mock_bridge_ptr);
}

}  // namespace
