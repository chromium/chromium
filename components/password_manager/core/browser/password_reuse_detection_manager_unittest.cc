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
using testing::AnyNumber;
using testing::_;

namespace password_manager {

namespace {

constexpr size_t kMaxNumberOfCharactersToStore = 45;

class MockPasswordManagerClient : public StubPasswordManagerClient {
 public:
  MockPasswordManagerClient() = default;
  ~MockPasswordManagerClient() override = default;

  MOCK_CONST_METHOD0(GetProfilePasswordStore, PasswordStore*());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockPasswordManagerClient);
};

class PasswordReuseDetectionManagerTest : public ::testing::Test {
 public:
  PasswordReuseDetectionManagerTest() {}
  void SetUp() override {
    store_ = new testing::StrictMock<MockPasswordStore>;
    CHECK(store_->Init(syncer::SyncableService::StartSyncFlare(), nullptr));
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
  manager.OnReuseFound(0ul, base::nullopt, {"https://example.com"}, 0);

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

}  // namespace

}  // namespace password_manager
