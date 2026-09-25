// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/exclusive_access/exclusive_access_bubble_android.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/android/jni_string.h"
#include "base/functional/callback_helpers.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/android/exclusive_access/exclusive_access_context_android.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/fullscreen_control/fullscreen_features.h"
#include "components/url_formatter/elide_url.h"
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

TEST_F(ExclusiveAccessBubbleAndroidTest,
       DownloadCompletionRestoresOriginInNotice) {
  url::Origin origin = url::Origin::Create(GURL("https://example.com"));
  auto format_text = [&](bool has_download) {
    return exclusive_access_bubble::GetInstructionTextForTypeTouchBased(
        EXCLUSIVE_ACCESS_BUBBLE_TYPE_FULLSCREEN_EXIT_INSTRUCTION,
        url_formatter::FormatOriginForSecurityDisplay(
            origin, url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC),
        has_download, /*notify_overridden=*/has_download);
  };

  ExclusiveAccessBubbleParams params;
  params.type = EXCLUSIVE_ACCESS_BUBBLE_TYPE_FULLSCREEN_EXIT_INSTRUCTION;
  params.origin = origin;

  auto mock_bridge = std::make_unique<MockBridge>();
  auto* mock_bridge_ptr = mock_bridge.get();
  EXPECT_CALL(*mock_bridge_ptr, IsKeyboardConnected())
      .WillRepeatedly(Return(false));

  EXPECT_CALL(*mock_bridge_ptr, IsVisible()).WillOnce(Return(false));
  EXPECT_CALL(*mock_bridge_ptr, Update(format_text(false))).Times(1);
  EXPECT_CALL(*mock_bridge_ptr, Show()).Times(1);
  ExclusiveAccessBubbleAndroid bubble(params, base::DoNothing(),
                                      std::move(mock_bridge));

  // Download starts while in fullscreen (DownloadDisplayController passes an
  // empty/opaque origin), overriding the notice.
  params.type = EXCLUSIVE_ACCESS_BUBBLE_TYPE_NONE;
  params.origin = url::Origin();
  params.has_download = true;
  params.force_update = true;
  EXPECT_CALL(*mock_bridge_ptr, IsVisible()).WillOnce(Return(true));
  EXPECT_CALL(*mock_bridge_ptr, Update(format_text(true))).Times(1);
  EXPECT_CALL(*mock_bridge_ptr, Show()).Times(1);
  bubble.Update(params, base::DoNothing());
  EXPECT_TRUE(bubble.params().has_download);

  // Subsequent update with a non-opaque origin unlatches override and restores
  // the origin in the notice.
  params.type = EXCLUSIVE_ACCESS_BUBBLE_TYPE_FULLSCREEN_EXIT_INSTRUCTION;
  params.origin = origin;
  params.has_download = false;
  params.force_update = false;
  EXPECT_CALL(*mock_bridge_ptr, IsVisible()).WillOnce(Return(true));
  EXPECT_CALL(*mock_bridge_ptr, Update(format_text(false))).Times(1);
  EXPECT_CALL(*mock_bridge_ptr, Show()).Times(1);
  bubble.Update(params, base::DoNothing());
  EXPECT_FALSE(bubble.params().has_download);
}

TEST(ExclusiveAccessBubbleAndroidOriginElisionTest, GetOriginString) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kFullscreenBubbleShowOrigin);

  struct TestCase {
    const char* url;
    std::optional<std::u16string> expected;
  } const kTestCases[] = {
      // 1. Standard origin well within the 40-character budget (no elision).
      {"https://example.com", u"example.com"},
      {"http://example.com", u"http://example.com"},

      // 2. Exactly 40 characters (https scheme omitted by security formatter):
      // "abcdefghijklmnopqrstuvwxyz01.example.com" has 28 + 1 + 11 = 40 chars.
      {"https://abcdefghijklmnopqrstuvwxyz01.example.com",
       u"abcdefghijklmnopqrstuvwxyz01.example.com"},

      // 3. Exceeds budget: front-elides leading characters to fit within 40
      // chars.
      {"https://a.sub.1234567890123456789012345.example.com",
       u"\u2026b.1234567890123456789012345.example.com"},

      // 4. Realistic spoof attempt: an attacker using Google-themed subdomains.
      // Front-elides leading subdomains while keeping the trailing registrable
      // domain within the 40-char budget.
      {"http://accounts.google.com.signin.verify.device-check.signin-attempt-4."
       "evil.example",
       u"\u2026ice-check.signin-attempt-4.evil.example"},

      // 5. Max-length single label (63 chars) immediately preceding the eTLD+1.
      {"https://"
       "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa."
       "evil.example",
       u"\u2026aaaaaaaaaaaaaaaaaaaaaaaaaa.evil.example"},

      // 6. Non-standard port preservation.
      {"http://verylongsubdomainprefix.evil.example:8080",
       u"\u2026rylongsubdomainprefix.evil.example:8080"},

      // 7. Extremely long registrable domain (> 39 chars).
      {"https://"
       "a-very-long-domain-name-that-exceeds-forty-characters-all-by-itself."
       "example",
       u"\u2026-forty-characters-all-by-itself.example"},

      // 8. IP address and intranet host.
      {"http://192.168.1.1:8080", u"http://192.168.1.1:8080"},
      {"http://localhost:8080", u"http://localhost:8080"},
  };

  for (const auto& test_case : kTestCases) {
    url::Origin origin = url::Origin::Create(GURL(test_case.url));
    std::optional<std::u16string> actual =
        ExclusiveAccessBubbleAndroid::GetOriginStringForTesting(origin);
    EXPECT_EQ(actual, test_case.expected)
        << "Failed for URL: " << test_case.url;
    if (actual.has_value()) {
      EXPECT_LE(actual->size(), 40u)
          << "Exceeded 40 char budget for URL: " << test_case.url;
    }
  }

  // Opaque origin returns std::nullopt.
  EXPECT_EQ(
      ExclusiveAccessBubbleAndroid::GetOriginStringForTesting(url::Origin()),
      std::nullopt);
}

TEST(ExclusiveAccessBubbleAndroidOriginElisionTest,
     FeatureDisabledReturnsNullopt) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kFullscreenBubbleShowOrigin);

  EXPECT_EQ(ExclusiveAccessBubbleAndroid::GetOriginStringForTesting(
                url::Origin::Create(GURL("https://example.com"))),
            std::nullopt);
}

TEST_F(ExclusiveAccessBubbleAndroidTest, LongOriginIsElidedInBubbleNotice) {
  ExclusiveAccessBubbleParams params;
  params.type = EXCLUSIVE_ACCESS_BUBBLE_TYPE_FULLSCREEN_EXIT_INSTRUCTION;
  params.origin = url::Origin::Create(
      GURL("http://accounts.google.com.signin.verify.device-check."
           "signin-attempt-4.evil.example"));

  auto mock_bridge = std::make_unique<MockBridge>();
  auto* mock_bridge_ptr = mock_bridge.get();

  std::u16string captured_text;
  EXPECT_CALL(*mock_bridge_ptr, IsVisible()).WillOnce(Return(false));
  EXPECT_CALL(*mock_bridge_ptr, IsKeyboardConnected()).WillOnce(Return(true));
  EXPECT_CALL(*mock_bridge_ptr, Update(_))
      .WillOnce(testing::SaveArg<0>(&captured_text));
  EXPECT_CALL(*mock_bridge_ptr, Show()).Times(1);

  ExclusiveAccessBubbleAndroid bubble(params, base::DoNothing(),
                                      std::move(mock_bridge));
  testing::Mock::VerifyAndClearExpectations(mock_bridge_ptr);

  std::u16string expected_text =
      exclusive_access_bubble::GetInstructionTextForType(
          EXCLUSIVE_ACCESS_BUBBLE_TYPE_FULLSCREEN_EXIT_INSTRUCTION,
          l10n_util::GetStringUTF16(IDS_APP_ESC_KEY),
          u"\u2026ice-check.signin-attempt-4.evil.example",
          /*has_download=*/false, /*notify_overridden=*/false);
  EXPECT_EQ(captured_text, expected_text);
  EXPECT_EQ(captured_text.find(u"accounts.google.com"), std::u16string::npos);
}

}  // namespace
