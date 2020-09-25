// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_reuse_detection_manager.h"

#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/password_manager/core/browser/mock_password_store.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "url/gurl.h"

using base::ASCIIToUTF16;
using testing::_;
using testing::AnyNumber;

namespace password_manager {

namespace {

constexpr size_t kMaxNumberOfCharactersToStore = 45;

class MockPasswordManagerClient : public StubPasswordManagerClient {
 public:
  MockPasswordManagerClient() = default;
  ~MockPasswordManagerClient() override = default;

  MOCK_CONST_METHOD0(GetProfilePasswordStore, PasswordStore*());
  MOCK_CONST_METHOD0(GetAccountPasswordStore, PasswordStore*());

  MOCK_METHOD4(CheckProtectedPasswordEntry,
               void(metrics_util::PasswordType,
                    const std::string&,
                    const std::vector<MatchingReusedCredential>&,
                    bool));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockPasswordManagerClient);
};

class PasswordReuseDetectionManagerTest : public ::testing::Test {
 public:
  PasswordReuseDetectionManagerTest() = default;
  void SetUp() override {
    store_ = new testing::StrictMock<MockPasswordStore>;
    CHECK(store_->Init(nullptr));
  }
  void TearDown() override {
    store_->ShutdownOnUIThread();
    store_ = nullptr;
  }

 protected:
  // It's needed for an initialisation of thread runners that are used in
  // MockPasswordStore.
  base::test::SingleThreadTaskEnvironment task_environment_;
  MockPasswordManagerClient client_;
  scoped_refptr<MockPasswordStore> store_;

  DISALLOW_COPY_AND_ASSIGN(PasswordReuseDetectionManagerTest);
};

// Verify that CheckReuse is called on each key pressed event with an argument
// equal to the last 30 keystrokes typed after the last main frame navigation.
TEST_F(PasswordReuseDetectionManagerTest, CheckReuseCalled) {
  const GURL gurls[] = {GURL("https://www.example.com"),
                        GURL("https://www.otherexample.com")};
  const base::string16 input[] = {
      base::ASCIIToUTF16(
          "1234567890abcdefghijklmnopqrstuvxyzABCDEFGHIJKLMNOPQRSTUVXYZ"),
      base::ASCIIToUTF16("?<>:'{}ABCDEF")};

  EXPECT_CALL(client_, GetProfilePasswordStore())
      .WillRepeatedly(testing::Return(store_.get()));
  PasswordReuseDetectionManager manager(&client_);

  for (size_t test = 0; test < base::size(gurls); ++test) {
    manager.DidNavigateMainFrame(gurls[test]);
    for (size_t i = 0; i < input[test].size(); ++i) {
      base::string16 expected_input = input[test].substr(0, i + 1);
      if (expected_input.size() > kMaxNumberOfCharactersToStore)
        expected_input = expected_input.substr(expected_input.size() -
                                               kMaxNumberOfCharactersToStore);
      EXPECT_CALL(
          *store_,
          CheckReuse(expected_input, gurls[test].GetOrigin().spec(), &manager));
      manager.OnKeyPressedCommitted(input[test].substr(i, 1));
      testing::Mock::VerifyAndClearExpectations(store_.get());
    }
  }
}

// Verify that the keystroke buffer is cleared after 10 seconds of user
// inactivity.
TEST_F(PasswordReuseDetectionManagerTest,
       CheckThatBufferClearedAfterInactivity) {
  EXPECT_CALL(client_, GetProfilePasswordStore())
      .WillRepeatedly(testing::Return(store_.get()));
  PasswordReuseDetectionManager manager(&client_);

  base::SimpleTestClock clock;
  base::Time now = base::Time::Now();
  clock.SetNow(now);
  manager.SetClockForTesting(&clock);

  EXPECT_CALL(*store_, CheckReuse(base::ASCIIToUTF16("1"), _, _));
  manager.OnKeyPressedCommitted(base::ASCIIToUTF16("1"));

  // Simulate 10 seconds of inactivity.
  clock.SetNow(now + base::TimeDelta::FromSeconds(10));
  // Expect that a keystroke typed before inactivity is cleared.
  EXPECT_CALL(*store_, CheckReuse(base::ASCIIToUTF16("2"), _, _));
  manager.OnKeyPressedCommitted(base::ASCIIToUTF16("2"));
}

// Verify that the keystroke buffer is cleared after user presses enter.
TEST_F(PasswordReuseDetectionManagerTest, CheckThatBufferClearedAfterEnter) {
  EXPECT_CALL(client_, GetProfilePasswordStore())
      .WillRepeatedly(testing::Return(store_.get()));
  PasswordReuseDetectionManager manager(&client_);

  EXPECT_CALL(*store_, CheckReuse(base::ASCIIToUTF16("1"), _, _));
  manager.OnKeyPressedCommitted(base::ASCIIToUTF16("1"));

  base::string16 enter_text(1, ui::VKEY_RETURN);
  EXPECT_CALL(*store_, CheckReuse(_, _, _)).Times(0);
  manager.OnKeyPressedCommitted(enter_text);

  // Expect only a keystroke typed after enter.
  EXPECT_CALL(*store_, CheckReuse(base::ASCIIToUTF16("2"), _, _));
  manager.OnKeyPressedCommitted(base::ASCIIToUTF16("2"));
}

// Verify that after reuse found, no reuse checking happens till next main frame
// navigation.
TEST_F(PasswordReuseDetectionManagerTest, NoReuseCheckingAfterReuseFound) {
  EXPECT_CALL(client_, GetProfilePasswordStore())
      .WillRepeatedly(testing::Return(store_.get()));
  PasswordReuseDetectionManager manager(&client_);

  // Simulate that reuse found.
  manager.OnReuseCheckDone(true, 0ul, base::nullopt, {{"https://example.com"}},
                           0);

  // Expect no checking of reuse.
  EXPECT_CALL(*store_, CheckReuse(_, _, _)).Times(0);
  manager.OnKeyPressedCommitted(base::ASCIIToUTF16("1"));

  // Expect that after main frame navigation checking is restored.
  manager.DidNavigateMainFrame(GURL("https://www.example.com"));
  EXPECT_CALL(*store_, CheckReuse(base::ASCIIToUTF16("1"), _, _));
  manager.OnKeyPressedCommitted(base::ASCIIToUTF16("1"));
}

// Verify that keystroke buffer is cleared only on cross host navigation.
TEST_F(PasswordReuseDetectionManagerTest, DidNavigateMainFrame) {
  EXPECT_CALL(client_, GetProfilePasswordStore())
      .WillRepeatedly(testing::Return(store_.get()));
  PasswordReuseDetectionManager manager(&client_);

  manager.DidNavigateMainFrame(GURL("https://www.example1.com/123"));
  EXPECT_CALL(*store_, CheckReuse(base::ASCIIToUTF16("1"), _, _));
  manager.OnKeyPressedCommitted(base::ASCIIToUTF16("1"));

  // Check that the buffer is not cleared on the same host navigation.
  manager.DidNavigateMainFrame(GURL("https://www.example1.com/456"));
  EXPECT_CALL(*store_, CheckReuse(base::ASCIIToUTF16("12"), _, _));
  manager.OnKeyPressedCommitted(base::ASCIIToUTF16("2"));

  // Check that the buffer is cleared on the cross host navigation.
  manager.DidNavigateMainFrame(GURL("https://www.example2.com/123"));
  EXPECT_CALL(*store_, CheckReuse(base::ASCIIToUTF16("3"), _, _));
  manager.OnKeyPressedCommitted(base::ASCIIToUTF16("3"));
}

// Verify that CheckReuse is called on a paste event.
TEST_F(PasswordReuseDetectionManagerTest, CheckReuseCalledOnPaste) {
  const GURL gurls[] = {GURL("https://www.example.com"),
                        GURL("https://www.example.test")};
  const base::string16 input[] = {
      base::ASCIIToUTF16(
          "1234567890abcdefghijklmnopqrstuvxyzABCDEFGHIJKLMNOPQRSTUVXYZ"),
      base::ASCIIToUTF16("?<>:'{}ABCDEF")};

  EXPECT_CALL(client_, GetProfilePasswordStore())
      .WillRepeatedly(testing::Return(store_.get()));
  PasswordReuseDetectionManager manager(&client_);

  for (size_t test = 0; test < base::size(gurls); ++test) {
    manager.DidNavigateMainFrame(gurls[test]);
    base::string16 expected_input = input[test];
    if (expected_input.size() > kMaxNumberOfCharactersToStore)
      expected_input = expected_input.substr(expected_input.size() -
                                             kMaxNumberOfCharactersToStore);
    EXPECT_CALL(*store_, CheckReuse(expected_input,
                                    gurls[test].GetOrigin().spec(), &manager));
    manager.OnPaste(input[test]);
    testing::Mock::VerifyAndClearExpectations(store_.get());
  }
}

TEST_F(PasswordReuseDetectionManagerTest,
       CheckReuseCalledOnPasteTwiceProduceNoDuplicates) {
  const GURL kURL("https://www.example.com");
  const base::string16 kInput =
      base::ASCIIToUTF16("1234567890abcdefghijklmnopqrstuvxyz");

  EXPECT_CALL(client_, GetProfilePasswordStore())
      .WillRepeatedly(testing::Return(store_.get()));
  PasswordReuseDetectionManager manager(&client_);

  manager.DidNavigateMainFrame(kURL);
  EXPECT_CALL(*store_, CheckReuse(kInput, kURL.GetOrigin().spec(), &manager))
      .Times(2);
  // The user paste the text twice before the store gets to respond.
  manager.OnPaste(kInput);
  manager.OnPaste(kInput);
  testing::Mock::VerifyAndClearExpectations(store_.get());

  std::vector<MatchingReusedCredential> reused_credentials = {
      {.signon_realm = "www.example2.com",
       .username = base::ASCIIToUTF16("username1"),
       .in_store = PasswordForm::Store::kProfileStore}};

  // CheckProtectedPasswordEntry should get called once, and the reused
  // credentials get used reported once in this call.
  EXPECT_CALL(client_,
              CheckProtectedPasswordEntry(_, _, reused_credentials, _));
  // Simulate 2 responses from the store with the same reused credentials.
  manager.OnReuseCheckDone(/*is_reuse_found=*/true, /*password_length=*/10,
                           /*reused_protected_password_hash=*/base::nullopt,
                           reused_credentials, /*saved_passwords=*/1);
  manager.OnReuseCheckDone(/*is_reuse_found=*/true, /*password_length=*/10,
                           /*reused_protected_password_hash=*/base::nullopt,
                           reused_credentials, /*saved_passwords=*/1);
}

#if defined(OS_ANDROID)
TEST_F(PasswordReuseDetectionManagerTest,
       CheckReusedCalledWithUncommittedText) {
  EXPECT_CALL(client_, GetProfilePasswordStore())
      .WillRepeatedly(testing::Return(store_.get()));
  PasswordReuseDetectionManager manager(&client_);
  GURL test_url("https://www.example.com");
  manager.DidNavigateMainFrame(test_url);

  base::string16 init_text = base::ASCIIToUTF16("init_text");
  base::string16 uncommitted_text = base::ASCIIToUTF16("uncommitted_text");
  base::string16 committed_text = base::ASCIIToUTF16("committed_text");

  EXPECT_CALL(*store_,
              CheckReuse(init_text, test_url.GetOrigin().spec(), &manager));
  manager.OnKeyPressedCommitted(init_text);
  EXPECT_CALL(*store_, CheckReuse(init_text + uncommitted_text,
                                  test_url.GetOrigin().spec(), &manager));
  manager.OnKeyPressedUncommitted(uncommitted_text);
  // Uncommitted text should not be stored.
  EXPECT_CALL(*store_, CheckReuse(init_text + committed_text,
                                  test_url.GetOrigin().spec(), &manager));
  manager.OnKeyPressedCommitted(committed_text);
}
#endif

class PasswordReuseDetectionManagerWithTwoStoresTest
    : public PasswordReuseDetectionManagerTest {
 public:
  PasswordReuseDetectionManagerWithTwoStoresTest() = default;
  void SetUp() override {
    PasswordReuseDetectionManagerTest::SetUp();
    account_store_ = new testing::StrictMock<MockPasswordStore>;
    CHECK(account_store_->Init(nullptr));
  }
  void TearDown() override {
    account_store_->ShutdownOnUIThread();
    account_store_ = nullptr;
    PasswordReuseDetectionManagerTest::TearDown();
  }

 protected:
  scoped_refptr<MockPasswordStore> account_store_;
};

TEST_F(PasswordReuseDetectionManagerWithTwoStoresTest,
       CheckReuseCalledOnPasteReuseExistsInBothStores) {
  const GURL kURL("https://www.example.com");
  const base::string16 kInput =
      base::ASCIIToUTF16("1234567890abcdefghijklmnopqrstuvxyz");

  EXPECT_CALL(client_, GetProfilePasswordStore())
      .WillRepeatedly(testing::Return(store_.get()));
  EXPECT_CALL(client_, GetAccountPasswordStore())
      .WillRepeatedly(testing::Return(account_store_.get()));

  PasswordReuseDetectionManager manager(&client_);

  manager.DidNavigateMainFrame(kURL);
  EXPECT_CALL(*store_, CheckReuse(kInput, kURL.GetOrigin().spec(), &manager));
  EXPECT_CALL(*account_store_,
              CheckReuse(kInput, kURL.GetOrigin().spec(), &manager));
  manager.OnPaste(kInput);
  testing::Mock::VerifyAndClearExpectations(store_.get());
  testing::Mock::VerifyAndClearExpectations(account_store_.get());

  std::vector<MatchingReusedCredential> profile_reused_credentials = {
      {.signon_realm = "www.example2.com",
       .username = base::ASCIIToUTF16("username1"),
       .in_store = PasswordForm::Store::kProfileStore}};
  // Simulate response from the profile store.
  manager.OnReuseCheckDone(/*is_reuse_found=*/true, /*password_length=*/10,
                           /*reused_protected_password_hash=*/base::nullopt,
                           profile_reused_credentials, /*saved_passwords=*/1);

  std::vector<MatchingReusedCredential> account_reused_credentials{
      {.signon_realm = "www.example2.com",
       .username = base::ASCIIToUTF16("username2"),
       .in_store = PasswordForm::Store::kAccountStore}};

  // The callback is run only after both stores respond.
  EXPECT_CALL(client_,
              CheckProtectedPasswordEntry(
                  _, _,
                  testing::UnorderedElementsAre(profile_reused_credentials[0],
                                                account_reused_credentials[0]),
                  _));
  // Simulate response from the account store.
  manager.OnReuseCheckDone(/*is_reuse_found=*/true, /*password_length=*/10,
                           /*reused_protected_password_hash=*/base::nullopt,
                           account_reused_credentials, /*saved_passwords=*/1);
}

TEST_F(PasswordReuseDetectionManagerWithTwoStoresTest,
       CheckReuseCalledOnPasteReuseExistsInFirstStoreResponse) {
  const GURL kURL("https://www.example.com");
  const base::string16 kInput =
      base::ASCIIToUTF16("1234567890abcdefghijklmnopqrstuvxyz");

  EXPECT_CALL(client_, GetProfilePasswordStore())
      .WillRepeatedly(testing::Return(store_.get()));
  EXPECT_CALL(client_, GetAccountPasswordStore())
      .WillRepeatedly(testing::Return(account_store_.get()));

  PasswordReuseDetectionManager manager(&client_);

  manager.DidNavigateMainFrame(kURL);
  EXPECT_CALL(*store_, CheckReuse(kInput, kURL.GetOrigin().spec(), &manager));
  EXPECT_CALL(*account_store_,
              CheckReuse(kInput, kURL.GetOrigin().spec(), &manager));
  manager.OnPaste(kInput);
  testing::Mock::VerifyAndClearExpectations(store_.get());
  testing::Mock::VerifyAndClearExpectations(account_store_.get());

  std::vector<MatchingReusedCredential> profile_reused_credentials = {
      {.signon_realm = "www.example2.com",
       .username = base::ASCIIToUTF16("username1"),
       .in_store = PasswordForm::Store::kProfileStore}};
  // Simulate response from the profile store.
  manager.OnReuseCheckDone(/*is_reuse_found=*/true, /*password_length=*/10,
                           /*reused_protected_password_hash=*/base::nullopt,
                           profile_reused_credentials, /*saved_passwords=*/1);

  // The callback is run only after both stores respond.
  EXPECT_CALL(client_,
              CheckProtectedPasswordEntry(_, _, profile_reused_credentials, _));
  // Simulate response from the account store with no reuse found.
  manager.OnReuseCheckDone(/*is_reuse_found=*/false, /*password_length=*/0,
                           /*reused_protected_password_hash=*/base::nullopt, {},
                           /*saved_passwords=*/0);
}

TEST_F(PasswordReuseDetectionManagerWithTwoStoresTest,
       CheckReuseCalledOnPasteReuseExistsInSecondStoreResponse) {
  const GURL kURL("https://www.example.com");
  const base::string16 kInput =
      base::ASCIIToUTF16("1234567890abcdefghijklmnopqrstuvxyz");

  EXPECT_CALL(client_, GetProfilePasswordStore())
      .WillRepeatedly(testing::Return(store_.get()));
  EXPECT_CALL(client_, GetAccountPasswordStore())
      .WillRepeatedly(testing::Return(account_store_.get()));

  PasswordReuseDetectionManager manager(&client_);

  manager.DidNavigateMainFrame(kURL);
  EXPECT_CALL(*store_, CheckReuse(kInput, kURL.GetOrigin().spec(), &manager));
  EXPECT_CALL(*account_store_,
              CheckReuse(kInput, kURL.GetOrigin().spec(), &manager));
  manager.OnPaste(kInput);
  testing::Mock::VerifyAndClearExpectations(store_.get());
  testing::Mock::VerifyAndClearExpectations(account_store_.get());

  // Simulate response from the account store with no reuse found.
  manager.OnReuseCheckDone(/*is_reuse_found=*/false, /*password_length=*/0,
                           /*reused_protected_password_hash=*/base::nullopt, {},
                           /*saved_passwords=*/0);

  std::vector<MatchingReusedCredential> profile_reused_credentials = {
      {.signon_realm = "www.example2.com",
       .username = base::ASCIIToUTF16("username1"),
       .in_store = PasswordForm::Store::kProfileStore}};

  // The callback is run only after both stores respond.
  EXPECT_CALL(client_,
              CheckProtectedPasswordEntry(_, _, profile_reused_credentials, _));
  // Simulate response from the profile store.
  manager.OnReuseCheckDone(/*is_reuse_found=*/true, /*password_length=*/10,
                           /*reused_protected_password_hash=*/base::nullopt,
                           profile_reused_credentials, /*saved_passwords=*/1);
}

}  // namespace

}  // namespace password_manager
