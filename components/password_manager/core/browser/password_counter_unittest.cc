// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_counter.h"

#include <string_view>

#include "base/location.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"

namespace password_manager {
namespace {

password_manager::StoredCredential CreateTestPasswordForm(
    std::string_view username,
    std::string_view password) {
  password_manager::StoredCredential cred;
  cred.url = GURL("https://example.com/login");
  cred.signon_realm = cred.url.GetWithEmptyPath().spec();
  cred.username_value = base::UTF8ToUTF16(username);
  cred.password_value = base::UTF8ToUTF16(password);
  return cred;
}

password_manager::StoredCredential CreateBlocklistedForm(
    std::string_view site) {
  password_manager::StoredCredential cred;
  cred.url = GURL(site);
  cred.signon_realm = cred.url.GetWithEmptyPath().spec();
  cred.blocked_by_user = true;
  return cred;
}

class MockObserver : public PasswordCounter::Observer {
 public:
  MOCK_METHOD(void, OnPasswordCounterChanged, (), (override));
};

class PasswordCounterTest : public testing::Test {
 public:
  PasswordCounterTest() {
    store_->Init(/*affiliated_match_helper=*/nullptr);
    account_store_->Init(/*affiliated_match_helper=*/nullptr);
  }
  ~PasswordCounterTest() override {
    store_->ShutdownOnUIThread();
    account_store_->ShutdownOnUIThread();
  }

  void RunUntilIdle() { task_environment.RunUntilIdle(); }

  MockObserver& observer() { return observer_; }
  TestPasswordStore* profile_store() { return store_.get(); }
  TestPasswordStore* account_store() { return account_store_.get(); }

 private:
  base::test::SingleThreadTaskEnvironment task_environment;
  scoped_refptr<TestPasswordStore> store_ =
      base::MakeRefCounted<TestPasswordStore>();
  scoped_refptr<TestPasswordStore> account_store_ =
      base::MakeRefCounted<TestPasswordStore>(
          password_manager::IsAccountStore(true));

  MockObserver observer_;
};

TEST_F(PasswordCounterTest, Empty) {
  PasswordCounter counter(profile_store(), account_store());

  EXPECT_EQ(counter.autofillable_passwords(), 0u);
}

TEST_F(PasswordCounterTest, PreexistingPasswords) {
  std::vector<password_manager::StoredCredential> creds;
  creds.push_back(CreateTestPasswordForm("user1", "123"));
  creds.push_back(CreateTestPasswordForm("user2", "12325"));
  profile_store()->AddLogins(std::move(creds));
  account_store()->AddLogin(CreateTestPasswordForm("user3", "123256"));
  RunUntilIdle();
  PasswordCounter counter(profile_store(), account_store());
  counter.AddObserver(&observer());
  absl::Cleanup remove([&] { counter.RemoveObserver(&observer()); });
  EXPECT_EQ(counter.autofillable_passwords(), 0u);

  EXPECT_CALL(observer(), OnPasswordCounterChanged).Times(2);
  RunUntilIdle();
  EXPECT_EQ(counter.autofillable_passwords(), 3u);
}

TEST_F(PasswordCounterTest, AddPasswordBeforeInit) {
  // The counter should not react to those changes before it gets the full list.
  profile_store()->AddLogin(CreateTestPasswordForm("user1", "123"));
  profile_store()->AddLogin(CreateTestPasswordForm("user2", "123256"));
  PasswordCounter counter(profile_store(), nullptr);
  counter.AddObserver(&observer());
  absl::Cleanup remove([&] { counter.RemoveObserver(&observer()); });
  EXPECT_EQ(counter.autofillable_passwords(), 0u);

  EXPECT_CALL(observer(), OnPasswordCounterChanged);
  RunUntilIdle();
  EXPECT_EQ(counter.autofillable_passwords(), 2u);
}

TEST_F(PasswordCounterTest, AddPasswordAfterInit) {
  PasswordCounter counter(profile_store(), account_store());
  counter.AddObserver(&observer());
  absl::Cleanup remove([&] { counter.RemoveObserver(&observer()); });
  RunUntilIdle();
  EXPECT_EQ(counter.autofillable_passwords(), 0u);

  profile_store()->AddLogin(CreateTestPasswordForm("user1", "123"));
  account_store()->AddLogin(CreateTestPasswordForm("user2", "123256"));

  EXPECT_CALL(observer(), OnPasswordCounterChanged).Times(2);
  RunUntilIdle();
  EXPECT_EQ(counter.autofillable_passwords(), 2u);
}

TEST_F(PasswordCounterTest, UpdateLogin) {
  profile_store()->AddLogin(CreateTestPasswordForm("user1", "123"));
  account_store()->AddLogin(CreateTestPasswordForm("user2", "123256"));
  RunUntilIdle();
  PasswordCounter counter(profile_store(), account_store());
  counter.AddObserver(&observer());
  absl::Cleanup remove([&] { counter.RemoveObserver(&observer()); });

  EXPECT_CALL(observer(), OnPasswordCounterChanged).Times(2);
  RunUntilIdle();
  EXPECT_EQ(counter.autofillable_passwords(), 2u);

  profile_store()->UpdateLogin(CreateTestPasswordForm("user1", "1234567"));
  account_store()->UpdateLogin(CreateTestPasswordForm("user2", "123256789"));
  EXPECT_CALL(observer(), OnPasswordCounterChanged).Times(0);
  RunUntilIdle();
  EXPECT_EQ(counter.autofillable_passwords(), 2u);
}

TEST_F(PasswordCounterTest, RemoveLogin) {
  profile_store()->AddLogin(CreateTestPasswordForm("user1", "123"));
  account_store()->AddLogin(CreateTestPasswordForm("user2", "123256"));
  RunUntilIdle();
  PasswordCounter counter(profile_store(), account_store());
  counter.AddObserver(&observer());
  absl::Cleanup remove([&] { counter.RemoveObserver(&observer()); });

  EXPECT_CALL(observer(), OnPasswordCounterChanged).Times(2);
  RunUntilIdle();
  EXPECT_EQ(counter.autofillable_passwords(), 2u);

  profile_store()->RemoveLogin(FROM_HERE,
                               CreateTestPasswordForm("user1", "123"));
  EXPECT_CALL(observer(), OnPasswordCounterChanged);
  RunUntilIdle();
  EXPECT_EQ(counter.autofillable_passwords(), 1u);

  account_store()->RemoveLogin(FROM_HERE,
                               CreateTestPasswordForm("user2", "123256"));
  EXPECT_CALL(observer(), OnPasswordCounterChanged);
  RunUntilIdle();
  EXPECT_EQ(counter.autofillable_passwords(), 0u);
}

TEST_F(PasswordCounterTest, RemoveNonexistingLogin) {
  profile_store()->AddLogin(CreateTestPasswordForm("user1", "123"));
  RunUntilIdle();
  PasswordCounter counter(profile_store(), nullptr);
  counter.AddObserver(&observer());
  absl::Cleanup remove([&] { counter.RemoveObserver(&observer()); });

  EXPECT_CALL(observer(), OnPasswordCounterChanged);
  RunUntilIdle();
  EXPECT_EQ(counter.autofillable_passwords(), 1u);

  profile_store()->RemoveLogin(FROM_HERE,
                               CreateTestPasswordForm("user2", "123"));
  EXPECT_CALL(observer(), OnPasswordCounterChanged).Times(0);
  RunUntilIdle();
  EXPECT_EQ(counter.autofillable_passwords(), 1u);
}

TEST_F(PasswordCounterTest, RemoveAllLogins) {
  profile_store()->AddLogin(CreateTestPasswordForm("user1", "123"));
  profile_store()->AddLogin(CreateTestPasswordForm("user2", "123"));
  profile_store()->AddLogin(CreateTestPasswordForm("user3", "123"));
  RunUntilIdle();
  PasswordCounter counter(profile_store(), nullptr);
  counter.AddObserver(&observer());
  absl::Cleanup remove([&] { counter.RemoveObserver(&observer()); });

  EXPECT_CALL(observer(), OnPasswordCounterChanged);
  RunUntilIdle();
  EXPECT_EQ(counter.autofillable_passwords(), 3u);

  profile_store()->RemoveLoginsCreatedBetween(FROM_HERE, base::Time(),
                                              base::Time::Max());
  EXPECT_CALL(observer(), OnPasswordCounterChanged);
  RunUntilIdle();
  EXPECT_EQ(counter.autofillable_passwords(), 0u);
}

TEST_F(PasswordCounterTest, IgnoreBlocklisted) {
  profile_store()->AddLogin(CreateBlocklistedForm("https://abc.com/"));
  account_store()->AddLogin(CreateBlocklistedForm("https://abc.com/"));
  RunUntilIdle();
  PasswordCounter counter(profile_store(), account_store());
  RunUntilIdle();
  EXPECT_EQ(counter.autofillable_passwords(), 0u);
}

TEST_F(PasswordCounterTest, IgnoreAddBlocklisted) {
  EXPECT_CALL(observer(), OnPasswordCounterChanged).Times(2);
  PasswordCounter counter(profile_store(), account_store());
  counter.AddObserver(&observer());
  absl::Cleanup remove([&] { counter.RemoveObserver(&observer()); });
  RunUntilIdle();

  EXPECT_CALL(observer(), OnPasswordCounterChanged).Times(0);
  profile_store()->AddLogin(CreateBlocklistedForm("https://abc.com/"));
  account_store()->AddLogin(CreateBlocklistedForm("https://abc.com/"));
  RunUntilIdle();
  EXPECT_EQ(counter.autofillable_passwords(), 0u);
}

TEST_F(PasswordCounterTest, IgnoreDeleteBlocklisted) {
  profile_store()->AddLogin(CreateTestPasswordForm("user1", "123"));
  profile_store()->AddLogin(CreateBlocklistedForm("https://abc.com/"));
  PasswordCounter counter(profile_store(), account_store());
  counter.AddObserver(&observer());
  absl::Cleanup remove([&] { counter.RemoveObserver(&observer()); });
  EXPECT_CALL(observer(), OnPasswordCounterChanged).Times(2);
  RunUntilIdle();
  EXPECT_EQ(counter.autofillable_passwords(), 1u);

  EXPECT_CALL(observer(), OnPasswordCounterChanged).Times(0);
  profile_store()->RemoveLogin(FROM_HERE,
                               CreateBlocklistedForm("https://abc.com/"));
  RunUntilIdle();
  EXPECT_EQ(counter.autofillable_passwords(), 1u);
}

}  // namespace
}  // namespace password_manager
