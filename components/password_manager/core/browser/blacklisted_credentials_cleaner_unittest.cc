// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/blacklisted_credentials_cleaner.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync/model/syncable_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

namespace {

using autofill::PasswordForm;
using ::testing::ElementsAre;

constexpr char kTestURL[] = "https://example.com/login/";
constexpr char kTestUsername[] = "Username";
constexpr char kTestUsernameElement[] = "username_element";
constexpr char kTestUsername2[] = "Username2";
constexpr char kTestPassword[] = "12345";
constexpr char kTestPasswordElement[] = "password_element";
constexpr char kTestSubmitElement[] = "submit_element";
// An arbitrary creation date different from a default constructed base::Time().
const base::Time kTestCreationDate =
    base::Time::FromDeltaSinceWindowsEpoch(base::TimeDelta::FromDays(1));

PasswordForm GetTestCredential() {
  PasswordForm form;
  form.scheme = PasswordForm::Scheme::kHtml;
  form.signon_realm = GURL(kTestURL).GetOrigin().spec();
  form.origin = GURL(kTestURL);
  form.username_element = base::ASCIIToUTF16(kTestUsernameElement);
  form.username_value = base::ASCIIToUTF16(kTestUsername);
  form.password_element = base::ASCIIToUTF16(kTestPasswordElement);
  form.password_value = base::ASCIIToUTF16(kTestPassword);
  form.submit_element = base::ASCIIToUTF16(kTestSubmitElement);
  form.date_created = kTestCreationDate;
  return form;
}

}  // namespace

class MockCredentialsCleanerObserver : public CredentialsCleaner::Observer {
 public:
  MOCK_METHOD0(CleaningCompleted, void());
};

class BlacklistedCredentialsCleanerTest : public ::testing::Test {
 public:
  BlacklistedCredentialsCleanerTest() {
    EXPECT_TRUE(
        store_->Init(syncer::SyncableService::StartSyncFlare(), nullptr));
    prefs_.registry()->RegisterBooleanPref(
        prefs::kBlacklistedCredentialsNormalized, false);
  }

  BlacklistedCredentialsCleanerTest(const BlacklistedCredentialsCleanerTest&) =
      delete;
  BlacklistedCredentialsCleanerTest& operator=(
      const BlacklistedCredentialsCleanerTest&) = delete;

  ~BlacklistedCredentialsCleanerTest() override {
    store_->ShutdownOnUIThread();
    task_environment_.RunUntilIdle();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  base::HistogramTester histogram_tester_;
  const scoped_refptr<TestPasswordStore> store_ =
      base::MakeRefCounted<TestPasswordStore>();
  TestingPrefServiceSimple prefs_;
  BlacklistedCredentialsCleaner cleaner_{store_, &prefs_};
};

TEST_F(BlacklistedCredentialsCleanerTest, NoCleaningWhenPrefIsSet) {
  prefs_.SetBoolean(prefs::kBlacklistedCredentialsNormalized, true);
  EXPECT_FALSE(cleaner_.NeedsCleaning());
  histogram_tester_.ExpectUniqueSample(
      "PasswordManager.BlacklistedSites.NeedNormalization", false, 1);
}

TEST_F(BlacklistedCredentialsCleanerTest, CleanerUpdatesPref) {
  EXPECT_TRUE(cleaner_.NeedsCleaning());
  histogram_tester_.ExpectUniqueSample(
      "PasswordManager.BlacklistedSites.NeedNormalization", true, 1);

  MockCredentialsCleanerObserver observer;
  cleaner_.StartCleaning(&observer);
  EXPECT_CALL(observer, CleaningCompleted);
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(prefs_.GetBoolean(prefs::kBlacklistedCredentialsNormalized));
}

TEST_F(BlacklistedCredentialsCleanerTest, CleanerNormalizesData) {
  PasswordForm test_credential = GetTestCredential();
  test_credential.blacklisted_by_user = true;
  store_->AddLogin(test_credential);

  MockCredentialsCleanerObserver observer;
  cleaner_.StartCleaning(&observer);
  EXPECT_CALL(observer, CleaningCompleted);
  task_environment_.RunUntilIdle();

  TestPasswordStore::PasswordMap stored_passwords = store_->stored_passwords();
  ASSERT_EQ(1u, stored_passwords.size());

  PasswordForm blacklisted_form;
  blacklisted_form.blacklisted_by_user = true;
  blacklisted_form.scheme = PasswordForm::Scheme::kHtml;
  blacklisted_form.signon_realm = GURL(kTestURL).GetOrigin().spec();
  blacklisted_form.origin = GURL(kTestURL).GetOrigin();
  blacklisted_form.date_created = kTestCreationDate;
  EXPECT_THAT(stored_passwords.begin()->second, ElementsAre(blacklisted_form));
}

TEST_F(BlacklistedCredentialsCleanerTest,
       CleanerDoesNotModifyNonBlacklistedEntries) {
  PasswordForm test_credential = GetTestCredential();
  store_->AddLogin(test_credential);

  MockCredentialsCleanerObserver observer;
  cleaner_.StartCleaning(&observer);
  EXPECT_CALL(observer, CleaningCompleted);
  task_environment_.RunUntilIdle();

  TestPasswordStore::PasswordMap stored_passwords = store_->stored_passwords();
  ASSERT_EQ(1u, stored_passwords.size());
  EXPECT_THAT(stored_passwords.begin()->second, ElementsAre(test_credential));
}

TEST_F(BlacklistedCredentialsCleanerTest, CleanerDeduplicatesForms) {
  PasswordForm test_credential = GetTestCredential();
  test_credential.blacklisted_by_user = true;
  store_->AddLogin(test_credential);

  // Create a duplicated entry for the same signon realm.
  test_credential.username_value = base::ASCIIToUTF16(kTestUsername2);
  store_->AddLogin(test_credential);
  task_environment_.RunUntilIdle();

  const TestPasswordStore::PasswordMap& stored_passwords =
      store_->stored_passwords();
  ASSERT_EQ(1u, stored_passwords.size());
  ASSERT_EQ(2u, stored_passwords.begin()->second.size());

  MockCredentialsCleanerObserver observer;
  cleaner_.StartCleaning(&observer);
  EXPECT_CALL(observer, CleaningCompleted);
  task_environment_.RunUntilIdle();

  ASSERT_EQ(1u, stored_passwords.size());
  // Only one credential for the signon realm is left.
  PasswordForm blacklisted_form;
  blacklisted_form.blacklisted_by_user = true;
  blacklisted_form.scheme = PasswordForm::Scheme::kHtml;
  blacklisted_form.signon_realm = GURL(kTestURL).GetOrigin().spec();
  blacklisted_form.origin = GURL(kTestURL).GetOrigin();
  blacklisted_form.date_created = kTestCreationDate;
  EXPECT_THAT(stored_passwords.begin()->second, ElementsAre(blacklisted_form));
}

}  // namespace password_manager
