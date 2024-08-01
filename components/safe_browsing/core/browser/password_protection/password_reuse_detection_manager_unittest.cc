// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/password_protection/password_reuse_detection_manager.h"

#include <array>
#include <optional>

#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/password_manager/core/browser/mock_password_reuse_manager.h"
#include "components/safe_browsing/core/browser/password_protection/stub_password_reuse_detection_manager_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "url/gurl.h"

using testing::_;
using testing::AnyNumber;

namespace safe_browsing {

namespace {

constexpr size_t kMaxNumberOfCharactersToStore = 45;

class MockPasswordReuseDetectionManagerClient
    : public StubPasswordReuseDetectionManagerClient {
 public:
  MockPasswordReuseDetectionManagerClient() = default;

  MockPasswordReuseDetectionManagerClient(
      const MockPasswordReuseDetectionManagerClient&) = delete;
  MockPasswordReuseDetectionManagerClient& operator=(
      const MockPasswordReuseDetectionManagerClient&) = delete;

  ~MockPasswordReuseDetectionManagerClient() override = default;

  MOCK_METHOD(password_manager::PasswordReuseManager*,
              GetPasswordReuseManager,
              (),
              (const, override));

  MOCK_METHOD(void,
              CheckProtectedPasswordEntry,
              (password_manager::metrics_util::PasswordType,
               const std::string&,
               const std::vector<password_manager::MatchingReusedCredential>&,
               bool,
               uint64_t,
               const std::string&),
              (override));

#if BUILDFLAG(IS_ANDROID)
  MOCK_METHOD(void,
              OnPasswordSelected,
              (const std::u16string& text_str),
              (override));
#endif
};

class PasswordReuseDetectionManagerTest : public ::testing::Test {
 public:
  PasswordReuseDetectionManagerTest() = default;

  PasswordReuseDetectionManagerTest(const PasswordReuseDetectionManagerTest&) =
      delete;
  PasswordReuseDetectionManagerTest& operator=(
      const PasswordReuseDetectionManagerTest&) = delete;

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  MockPasswordReuseDetectionManagerClient client_;
  password_manager::MockPasswordReuseManager reuse_manager_;
};

// Verify that CheckReuse is called on each key pressed event with an argument
// equal to the last 30 keystrokes typed after the last main frame navigation.
TEST_F(PasswordReuseDetectionManagerTest, CheckReuseCalled) {
  const auto gurls = std::to_array(
      {GURL("https://www.example.com"), GURL("https://www.otherexample.com")});
  const std::array<std::u16string, 2> input{
      {u"1234567890abcdefghijklmnopqrstuvxyzABCDEFGHIJKLMNOPQRSTUVXYZ",
       u"?<>:'{}ABCDEF"}};

  EXPECT_CALL(client_, GetPasswordReuseManager())
      .WillRepeatedly(testing::Return(&reuse_manager_));
  PasswordReuseDetectionManager manager(&client_);

  for (size_t test = 0; test < std::size(gurls); ++test) {
    manager.DidNavigateMainFrame(gurls[test]);
    for (size_t i = 0; i < input[test].size(); ++i) {
      std::u16string expected_input = input[test].substr(0, i + 1);
      if (expected_input.size() > kMaxNumberOfCharactersToStore) {
        expected_input = expected_input.substr(expected_input.size() -
                                               kMaxNumberOfCharactersToStore);
      }
      EXPECT_CALL(
          reuse_manager_,
          CheckReuse(expected_input,
                     gurls[test].DeprecatedGetOriginAsURL().spec(), &manager));
      manager.OnKeyPressedCommitted(input[test].substr(i, 1));
      testing::Mock::VerifyAndClearExpectations(&reuse_manager_);
    }
  }
}

// Verify that the keystroke buffer is cleared after 10 seconds of user
// inactivity.
TEST_F(PasswordReuseDetectionManagerTest,
       CheckThatBufferClearedAfterInactivity) {
  EXPECT_CALL(client_, GetPasswordReuseManager())
      .WillRepeatedly(testing::Return(&reuse_manager_));
  PasswordReuseDetectionManager manager(&client_);

  base::SimpleTestClock clock;
  base::Time now = base::Time::Now();
  clock.SetNow(now);
  manager.SetClockForTesting(&clock);

  EXPECT_CALL(reuse_manager_, CheckReuse(std::u16string(u"1"), _, _));
  manager.OnKeyPressedCommitted(u"1");

  // Simulate 10 seconds of inactivity.
  clock.SetNow(now + base::Seconds(10));
  // Expect that a keystroke typed before inactivity is cleared.
  EXPECT_CALL(reuse_manager_, CheckReuse(std::u16string(u"2"), _, _));
  manager.OnKeyPressedCommitted(u"2");
}

// Verify that the keystroke buffer is cleared after user presses enter.
TEST_F(PasswordReuseDetectionManagerTest, CheckThatBufferClearedAfterEnter) {
  EXPECT_CALL(client_, GetPasswordReuseManager())
      .WillRepeatedly(testing::Return(&reuse_manager_));
  PasswordReuseDetectionManager manager(&client_);

  EXPECT_CALL(reuse_manager_, CheckReuse(std::u16string(u"1"), _, _));
  manager.OnKeyPressedCommitted(u"1");

  std::u16string enter_text(1, ui::VKEY_RETURN);
  EXPECT_CALL(reuse_manager_, CheckReuse).Times(0);
  manager.OnKeyPressedCommitted(enter_text);

  // Expect only a keystroke typed after enter.
  EXPECT_CALL(reuse_manager_, CheckReuse(std::u16string(u"2"), _, _));
  manager.OnKeyPressedCommitted(u"2");
}

// Verify that after reuse found, no reuse checking happens till next main frame
// navigation.
TEST_F(PasswordReuseDetectionManagerTest, NoReuseCheckingAfterReuseFound) {
  EXPECT_CALL(client_, GetPasswordReuseManager())
      .WillRepeatedly(testing::Return(&reuse_manager_));
  PasswordReuseDetectionManager manager(&client_);

  // Simulate that reuse found.
  manager.OnReuseCheckDone(true, 0ul, std::nullopt, {{"https://example.com"}},
                           0, std::string(), 0);

  // Expect no checking of reuse.
  EXPECT_CALL(reuse_manager_, CheckReuse).Times(0);
  manager.OnKeyPressedCommitted(u"1");

  // Expect that after main frame navigation checking is restored.
  manager.DidNavigateMainFrame(GURL("https://www.example.com"));
  EXPECT_CALL(reuse_manager_, CheckReuse(std::u16string(u"1"), _, _));
  manager.OnKeyPressedCommitted(u"1");
}

// Verify that keystroke buffer is cleared only on cross host navigation.
TEST_F(PasswordReuseDetectionManagerTest, DidNavigateMainFrame) {
  EXPECT_CALL(client_, GetPasswordReuseManager())
      .WillRepeatedly(testing::Return(&reuse_manager_));
  PasswordReuseDetectionManager manager(&client_);

  manager.DidNavigateMainFrame(GURL("https://www.example1.com/123"));
  EXPECT_CALL(reuse_manager_, CheckReuse(std::u16string(u"1"), _, _));
  manager.OnKeyPressedCommitted(u"1");

  // Check that the buffer is not cleared on the same host navigation.
  manager.DidNavigateMainFrame(GURL("https://www.example1.com/456"));
  EXPECT_CALL(reuse_manager_, CheckReuse(std::u16string(u"12"), _, _));
  manager.OnKeyPressedCommitted(u"2");

  // Check that the buffer is cleared on the cross host navigation.
  manager.DidNavigateMainFrame(GURL("https://www.example2.com/123"));
  EXPECT_CALL(reuse_manager_, CheckReuse(std::u16string(u"3"), _, _));
  manager.OnKeyPressedCommitted(u"3");
}

// Verify that CheckReuse is called on a paste event.
TEST_F(PasswordReuseDetectionManagerTest, CheckReuseCalledOnPaste) {
  const std::array<GURL, 2> gurls = {GURL("https://www.example.com"),
                                     GURL("https://www.example.test")};
  const std::array<std::u16string, 2> input = {
      u"1234567890abcdefghijklmnopqrstuvxyzABCDEFGHIJKLMNOPQRSTUVXYZ",
      u"?<>:'{}ABCDEF"};

  EXPECT_CALL(client_, GetPasswordReuseManager())
      .WillRepeatedly(testing::Return(&reuse_manager_));
  PasswordReuseDetectionManager manager(&client_);

  for (size_t test = 0; test < std::size(gurls); ++test) {
    manager.DidNavigateMainFrame(gurls[test]);
    std::u16string expected_input = input[test];
    if (expected_input.size() > kMaxNumberOfCharactersToStore) {
      expected_input = expected_input.substr(expected_input.size() -
                                             kMaxNumberOfCharactersToStore);
    }
    EXPECT_CALL(
        reuse_manager_,
        CheckReuse(expected_input,
                   gurls[test].DeprecatedGetOriginAsURL().spec(), &manager));
    manager.OnPaste(input[test]);
    testing::Mock::VerifyAndClearExpectations(&reuse_manager_);
  }
}

TEST_F(PasswordReuseDetectionManagerTest,
       CheckReuseCalledOnPasteTwiceProduceNoDuplicates) {
  const GURL kURL("https://www.example.com");
  const std::u16string kInput = u"1234567890abcdefghijklmnopqrstuvxyz";

  EXPECT_CALL(client_, GetPasswordReuseManager())
      .WillRepeatedly(testing::Return(&reuse_manager_));
  PasswordReuseDetectionManager manager(&client_);

  manager.DidNavigateMainFrame(kURL);
  EXPECT_CALL(
      reuse_manager_,
      CheckReuse(kInput, kURL.DeprecatedGetOriginAsURL().spec(), &manager))
      .Times(2);
  // The user paste the text twice before the store gets to respond.
  manager.OnPaste(kInput);
  manager.OnPaste(kInput);
  testing::Mock::VerifyAndClearExpectations(&reuse_manager_);

  std::vector<password_manager::MatchingReusedCredential> reused_credentials = {
      {.signon_realm = "www.example2.com",
       .username = u"username1",
       .in_store = password_manager::PasswordForm::Store::kProfileStore}};

  // CheckProtectedPasswordEntry should get called once, and the reused
  // credentials get used reported once in this call.
  EXPECT_CALL(client_,
              CheckProtectedPasswordEntry(_, _, reused_credentials, _, _, _));
  manager.OnReuseCheckDone(/*is_reuse_found=*/true, /*password_length=*/10,
                           /*reused_protected_password_hash=*/std::nullopt,
                           reused_credentials, /*saved_passwords=*/1,
                           /*domain=*/std::string(),
                           /*reused_password_hash=*/0);
}

#if BUILDFLAG(IS_ANDROID)
TEST_F(PasswordReuseDetectionManagerTest,
       CheckReusedCalledWithUncommittedText) {
  EXPECT_CALL(client_, GetPasswordReuseManager())
      .WillRepeatedly(testing::Return(&reuse_manager_));
  PasswordReuseDetectionManager manager(&client_);
  GURL test_url("https://www.example.com");
  manager.DidNavigateMainFrame(test_url);

  std::u16string init_text = u"init_text";
  std::u16string uncommitted_text = u"uncommitted_text";
  std::u16string committed_text = u"committed_text";

  EXPECT_CALL(reuse_manager_,
              CheckReuse(init_text, test_url.DeprecatedGetOriginAsURL().spec(),
                         &manager));
  manager.OnKeyPressedCommitted(init_text);
  EXPECT_CALL(reuse_manager_,
              CheckReuse(init_text + uncommitted_text,
                         test_url.DeprecatedGetOriginAsURL().spec(), &manager));
  manager.OnKeyPressedUncommitted(uncommitted_text);
  // Uncommitted text should not be stored.
  EXPECT_CALL(reuse_manager_,
              CheckReuse(init_text + committed_text,
                         test_url.DeprecatedGetOriginAsURL().spec(), &manager));
  manager.OnKeyPressedCommitted(committed_text);
}
#endif

}  // namespace

}  // namespace safe_browsing
