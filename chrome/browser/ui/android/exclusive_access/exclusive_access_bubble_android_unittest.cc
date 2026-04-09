// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/exclusive_access/exclusive_access_bubble_android.h"

#include "base/android/jni_string.h"
#include "base/functional/callback_helpers.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

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

}  // namespace
