// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_counter.h"

#include <string_view>

#include "base/location.h"
#include "base/test/task_environment.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {
namespace {

password_manager::PasswordForm CreateTestPasswordForm(
    std::string_view username,
    std::string_view password) {
  password_manager::PasswordForm form;
  form.url = GURL("https://example.com/login");
  form.signon_realm = form.url.GetWithEmptyPath().spec();
  form.username_value = base::UTF8ToUTF16(username);
  form.password_value = base::UTF8ToUTF16(password);
  return form;
}

password_manager::PasswordForm CreateBlocklistedForm(std::string_view site) {
  password_manager::PasswordForm form;
  form.url = GURL(site);
  form.signon_realm = form.url.GetWithEmptyPath().spec();
  form.blocked_by_user = true;
  return form;
}

class MockDelegate : public PasswordCounter::Delegate {
 public:
  MOCK_METHOD(void, OnPasswordCounterChanged, (), (override));
};

class PasswordCounterTest : public testing::Test {
 public:
  PasswordCounterTest() {
    store_->Init(/*prefs=*/nullptr, /*affiliated_match_helper=*/nullptr);
    account_store_->Init(/*prefs=*/nullptr,
                         /*affiliated_match_helper=*/nullptr);
  }
  ~PasswordCounterTest() override {
    store_->ShutdownOnUIThread();
    account_store_->ShutdownOnUIThread();
  }

  void RunUntilIdle() { task_environment.RunUntilIdle(); }

  MockDelegate& delegate() { return delegate_; }
  TestPasswordStore* store() { return store_.get(); }
  TestPasswordStore* account_store() { return account_store_.get(); }

 private:
  base::test::SingleThreadTaskEnvironment task_environment;
  scoped_refptr<TestPasswordStore> store_ =
      base::MakeRefCounted<TestPasswordStore>();
  scoped_refptr<TestPasswordStore> account_store_ =
      base::MakeRefCounted<TestPasswordStore>(
          password_manager::IsAccountStore(true));

  MockDelegate delegate_;
};

TEST_F(PasswordCounterTest, Empty) {
  PasswordCounter counter(store(), account_store(), &delegate());

  EXPECT_EQ(counter.profile_passwords(), 0u);
  EXPECT_EQ(counter.account_passwords(), 0u);
}

TEST_F(PasswordCounterTest, PreexistingPasswords) {
  store()->AddLogins({CreateTestPasswordForm("user1", "123"),
                      CreateTestPasswordForm("user2", "12325")});
  account_store()->AddLogin(CreateTestPasswordForm("user3", "123256"));
  RunUntilIdle();
  PasswordCounter counter(store(), account_store(), &delegate());
  EXPECT_EQ(counter.profile_passwords(), 0u);
  EXPECT_EQ(counter.account_passwords(), 0u);

  EXPECT_CALL(delegate(), OnPasswordCounterChanged).Times(2);
  RunUntilIdle();
  EXPECT_EQ(counter.profile_passwords(), 2u);
  EXPECT_EQ(counter.account_passwords(), 1u);
}

TEST_F(PasswordCounterTest, AddPasswordBeforeInit) {
  // The counter should not react to those changes before it gets the full list.
  store()->AddLogin(CreateTestPasswordForm("user1", "123"));
  store()->AddLogin(CreateTestPasswordForm("user2", "123256"));
  PasswordCounter counter(store(), nullptr, &delegate());
  EXPECT_EQ(counter.profile_passwords(), 0u);
  EXPECT_EQ(counter.account_passwords(), 0u);

  EXPECT_CALL(delegate(), OnPasswordCounterChanged);
  RunUntilIdle();
  EXPECT_EQ(counter.profile_passwords(), 2u);
  EXPECT_EQ(counter.account_passwords(), 0u);
}

TEST_F(PasswordCounterTest, AddPasswordAfterInit) {
  PasswordCounter counter(store(), account_store(), &delegate());
  RunUntilIdle();
  EXPECT_EQ(counter.profile_passwords(), 0u);
  EXPECT_EQ(counter.account_passwords(), 0u);

  store()->AddLogin(CreateTestPasswordForm("user1", "123"));
  account_store()->AddLogin(CreateTestPasswordForm("user2", "123256"));

  EXPECT_CALL(delegate(), OnPasswordCounterChanged).Times(2);
  RunUntilIdle();
  EXPECT_EQ(counter.profile_passwords(), 1u);
  EXPECT_EQ(counter.account_passwords(), 1u);
}

TEST_F(PasswordCounterTest, UpdateLogin) {
  store()->AddLogin(CreateTestPasswordForm("user1", "123"));
  account_store()->AddLogin(CreateTestPasswordForm("user2", "123256"));
  RunUntilIdle();
  PasswordCounter counter(store(), account_store(), &delegate());

  EXPECT_CALL(delegate(), OnPasswordCounterChanged).Times(2);
  RunUntilIdle();
  EXPECT_EQ(counter.profile_passwords(), 1u);
  EXPECT_EQ(counter.account_passwords(), 1u);

  store()->UpdateLogin(CreateTestPasswordForm("user1", "1234567"));
  account_store()->UpdateLogin(CreateTestPasswordForm("user2", "123256789"));
  EXPECT_CALL(delegate(), OnPasswordCounterChanged).Times(0);
  RunUntilIdle();
  EXPECT_EQ(counter.profile_passwords(), 1u);
  EXPECT_EQ(counter.account_passwords(), 1u);
}

TEST_F(PasswordCounterTest, RemoveLogin) {
  store()->AddLogin(CreateTestPasswordForm("user1", "123"));
  account_store()->AddLogin(CreateTestPasswordForm("user2", "123256"));
  RunUntilIdle();
  PasswordCounter counter(store(), account_store(), &delegate());

  EXPECT_CALL(delegate(), OnPasswordCounterChanged).Times(2);
  RunUntilIdle();
  EXPECT_EQ(counter.profile_passwords(), 1u);
  EXPECT_EQ(counter.account_passwords(), 1u);

  store()->RemoveLogin(FROM_HERE, CreateTestPasswordForm("user1", "123"));
  EXPECT_CALL(delegate(), OnPasswordCounterChanged);
  RunUntilIdle();
  EXPECT_EQ(counter.profile_passwords(), 0u);
  EXPECT_EQ(counter.account_passwords(), 1u);

  account_store()->RemoveLogin(FROM_HERE,
                               CreateTestPasswordForm("user2", "123256"));
  EXPECT_CALL(delegate(), OnPasswordCounterChanged);
  RunUntilIdle();
  EXPECT_EQ(counter.profile_passwords(), 0u);
  EXPECT_EQ(counter.account_passwords(), 0u);
}

TEST_F(PasswordCounterTest, RemoveNonexistingLogin) {
  store()->AddLogin(CreateTestPasswordForm("user1", "123"));
  RunUntilIdle();
  PasswordCounter counter(store(), nullptr, &delegate());

  EXPECT_CALL(delegate(), OnPasswordCounterChanged);
  RunUntilIdle();
  EXPECT_EQ(counter.profile_passwords(), 1u);
  EXPECT_EQ(counter.account_passwords(), 0u);

  store()->RemoveLogin(FROM_HERE, CreateTestPasswordForm("user2", "123"));
  EXPECT_CALL(delegate(), OnPasswordCounterChanged).Times(0);
  RunUntilIdle();
  EXPECT_EQ(counter.profile_passwords(), 1u);
  EXPECT_EQ(counter.account_passwords(), 0u);
}

TEST_F(PasswordCounterTest, RemoveAllLogins) {
  store()->AddLogin(CreateTestPasswordForm("user1", "123"));
  store()->AddLogin(CreateTestPasswordForm("user2", "123"));
  store()->AddLogin(CreateTestPasswordForm("user3", "123"));
  RunUntilIdle();
  PasswordCounter counter(store(), nullptr, &delegate());

  EXPECT_CALL(delegate(), OnPasswordCounterChanged);
  RunUntilIdle();
  EXPECT_EQ(counter.profile_passwords(), 3u);
  EXPECT_EQ(counter.account_passwords(), 0u);

  store()->RemoveLoginsCreatedBetween(FROM_HERE, base::Time(),
                                      base::Time::Max());
  EXPECT_CALL(delegate(), OnPasswordCounterChanged);
  RunUntilIdle();
  EXPECT_EQ(counter.profile_passwords(), 0u);
  EXPECT_EQ(counter.account_passwords(), 0u);
}

TEST_F(PasswordCounterTest, IgnoreBlocklisted) {
  store()->AddLogin(CreateBlocklistedForm("https://abc.com/"));
  account_store()->AddLogin(CreateBlocklistedForm("https://abc.com/"));
  RunUntilIdle();
  PasswordCounter counter(store(), account_store(), &delegate());
  RunUntilIdle();
  EXPECT_EQ(counter.profile_passwords(), 0u);
  EXPECT_EQ(counter.account_passwords(), 0u);
}

TEST_F(PasswordCounterTest, IgnoreAddBlocklisted) {
  EXPECT_CALL(delegate(), OnPasswordCounterChanged).Times(2);
  PasswordCounter counter(store(), account_store(), &delegate());
  RunUntilIdle();

  EXPECT_CALL(delegate(), OnPasswordCounterChanged).Times(0);
  store()->AddLogin(CreateBlocklistedForm("https://abc.com/"));
  account_store()->AddLogin(CreateBlocklistedForm("https://abc.com/"));
  RunUntilIdle();
  EXPECT_EQ(counter.profile_passwords(), 0u);
  EXPECT_EQ(counter.account_passwords(), 0u);
}

TEST_F(PasswordCounterTest, IgnoreDeleteBlocklisted) {
  store()->AddLogin(CreateTestPasswordForm("user1", "123"));
  store()->AddLogin(CreateBlocklistedForm("https://abc.com/"));
  PasswordCounter counter(store(), account_store(), &delegate());
  EXPECT_CALL(delegate(), OnPasswordCounterChanged).Times(2);
  RunUntilIdle();
  EXPECT_EQ(counter.profile_passwords(), 1u);
  EXPECT_EQ(counter.account_passwords(), 0u);

  EXPECT_CALL(delegate(), OnPasswordCounterChanged).Times(0);
  store()->RemoveLogin(FROM_HERE, CreateBlocklistedForm("https://abc.com/"));
  RunUntilIdle();
  EXPECT_EQ(counter.profile_passwords(), 1u);
  EXPECT_EQ(counter.account_passwords(), 0u);
}

}  // namespace
}  // namespace password_manager
