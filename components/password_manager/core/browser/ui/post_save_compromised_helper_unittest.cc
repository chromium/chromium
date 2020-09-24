// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/ui/post_save_compromised_helper.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/password_manager/core/browser/mock_password_store.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {
namespace {

using BubbleType = PostSaveCompromisedHelper::BubbleType;
using prefs::kLastTimePasswordCheckCompleted;
using testing::Return;

constexpr char kSignonRealm[] = "https://example.com/";
constexpr char kUsername[] = "user";
constexpr char kUsername2[] = "user2";

CompromisedCredentials CreateCompromised(
    base::StringPiece username,
    PasswordForm::Store store = PasswordForm::Store::kProfileStore) {
  return CompromisedCredentials{
      .signon_realm = kSignonRealm,
      .username = base::ASCIIToUTF16(username),
      .compromise_type = CompromiseType::kLeaked,
      .in_store = store,
  };
}

}  // namespace

class PostSaveCompromisedHelperTest : public testing::Test {
 public:
  PostSaveCompromisedHelperTest() {
    mock_profile_store_ = new MockPasswordStore;
    EXPECT_TRUE(mock_profile_store_->Init(&prefs_));
    prefs_.registry()->RegisterDoublePref(kLastTimePasswordCheckCompleted, 0.0);
  }

  ~PostSaveCompromisedHelperTest() override {
    mock_profile_store_->ShutdownOnUIThread();
  }

  void WaitForPasswordStore() { task_environment_.RunUntilIdle(); }

  MockPasswordStore* profile_store() { return mock_profile_store_.get(); }
  virtual MockPasswordStore* account_store() { return nullptr; }
  TestingPrefServiceSimple* prefs() { return &prefs_; }

 protected:
  TestingPrefServiceSimple prefs_;

 private:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  scoped_refptr<MockPasswordStore> mock_profile_store_;
};

TEST_F(PostSaveCompromisedHelperTest, DefaultState) {
  PostSaveCompromisedHelper helper({}, base::ASCIIToUTF16(kUsername));
  EXPECT_EQ(BubbleType::kNoBubble, helper.bubble_type());
  EXPECT_EQ(0u, helper.compromised_count());
}

TEST_F(PostSaveCompromisedHelperTest, EmptyStore) {
  PostSaveCompromisedHelper helper({}, base::ASCIIToUTF16(kUsername));
  base::MockCallback<PostSaveCompromisedHelper::BubbleCallback> callback;
  EXPECT_CALL(callback, Run(BubbleType::kNoBubble, 0));
  EXPECT_CALL(*profile_store(), GetAllCompromisedCredentialsImpl);
  helper.AnalyzeLeakedCredentials(profile_store(), account_store(), prefs(),
                                  callback.Get());
  WaitForPasswordStore();
  EXPECT_EQ(BubbleType::kNoBubble, helper.bubble_type());
  EXPECT_EQ(0u, helper.compromised_count());
}

TEST_F(PostSaveCompromisedHelperTest, RandomSite_FullStore) {
  PostSaveCompromisedHelper helper({}, base::ASCIIToUTF16(kUsername));
  base::MockCallback<PostSaveCompromisedHelper::BubbleCallback> callback;
  EXPECT_CALL(callback, Run(BubbleType::kUnsafeState, 1));
  std::vector<CompromisedCredentials> saved = {CreateCompromised(kUsername2)};
  EXPECT_CALL(*profile_store(), GetAllCompromisedCredentialsImpl)
      .WillOnce(Return(saved));
  helper.AnalyzeLeakedCredentials(profile_store(), account_store(), prefs(),
                                  callback.Get());
  WaitForPasswordStore();
  EXPECT_EQ(BubbleType::kUnsafeState, helper.bubble_type());
  EXPECT_EQ(1u, helper.compromised_count());
}

TEST_F(PostSaveCompromisedHelperTest, CompromisedSite_ItemStayed) {
  std::vector<CompromisedCredentials> saved = {CreateCompromised(kUsername),
                                               CreateCompromised(kUsername2)};
  PostSaveCompromisedHelper helper({saved}, base::ASCIIToUTF16(kUsername));
  base::MockCallback<PostSaveCompromisedHelper::BubbleCallback> callback;
  EXPECT_CALL(callback, Run(BubbleType::kUnsafeState, 2));
  EXPECT_CALL(*profile_store(), GetAllCompromisedCredentialsImpl)
      .WillOnce(Return(saved));
  helper.AnalyzeLeakedCredentials(profile_store(), account_store(), prefs(),
                                  callback.Get());
  WaitForPasswordStore();
  EXPECT_EQ(BubbleType::kUnsafeState, helper.bubble_type());
  EXPECT_EQ(2u, helper.compromised_count());
}

TEST_F(PostSaveCompromisedHelperTest, CompromisedSite_ItemGone) {
  std::vector<CompromisedCredentials> saved = {CreateCompromised(kUsername),
                                               CreateCompromised(kUsername2)};
  PostSaveCompromisedHelper helper({saved}, base::ASCIIToUTF16(kUsername));
  base::MockCallback<PostSaveCompromisedHelper::BubbleCallback> callback;
  EXPECT_CALL(callback, Run(BubbleType::kPasswordUpdatedWithMoreToFix, 1));
  saved = {CreateCompromised(kUsername2)};
  EXPECT_CALL(*profile_store(), GetAllCompromisedCredentialsImpl)
      .WillOnce(Return(saved));
  helper.AnalyzeLeakedCredentials(profile_store(), account_store(), prefs(),
                                  callback.Get());
  WaitForPasswordStore();
  EXPECT_EQ(BubbleType::kPasswordUpdatedWithMoreToFix, helper.bubble_type());
  EXPECT_EQ(1u, helper.compromised_count());
}

TEST_F(PostSaveCompromisedHelperTest, FixedLast_BulkCheckNeverDone) {
  std::vector<CompromisedCredentials> saved = {CreateCompromised(kUsername)};
  PostSaveCompromisedHelper helper({saved}, base::ASCIIToUTF16(kUsername));
  base::MockCallback<PostSaveCompromisedHelper::BubbleCallback> callback;
  EXPECT_CALL(callback, Run(BubbleType::kNoBubble, 0));
  saved = {};
  EXPECT_CALL(*profile_store(), GetAllCompromisedCredentialsImpl)
      .WillOnce(Return(saved));
  helper.AnalyzeLeakedCredentials(profile_store(), account_store(), prefs(),
                                  callback.Get());
  WaitForPasswordStore();
  EXPECT_EQ(BubbleType::kNoBubble, helper.bubble_type());
  EXPECT_EQ(0u, helper.compromised_count());
}

TEST_F(PostSaveCompromisedHelperTest, FixedLast_BulkCheckDoneLongAgo) {
  prefs()->SetDouble(
      kLastTimePasswordCheckCompleted,
      (base::Time::Now() - base::TimeDelta::FromDays(5)).ToDoubleT());
  std::vector<CompromisedCredentials> saved = {CreateCompromised(kUsername)};
  PostSaveCompromisedHelper helper({saved}, base::ASCIIToUTF16(kUsername));
  base::MockCallback<PostSaveCompromisedHelper::BubbleCallback> callback;
  EXPECT_CALL(callback, Run(BubbleType::kNoBubble, 0));
  saved = {};
  EXPECT_CALL(*profile_store(), GetAllCompromisedCredentialsImpl)
      .WillOnce(Return(saved));
  helper.AnalyzeLeakedCredentials(profile_store(), account_store(), prefs(),
                                  callback.Get());
  WaitForPasswordStore();
  EXPECT_EQ(BubbleType::kNoBubble, helper.bubble_type());
  EXPECT_EQ(0u, helper.compromised_count());
}

TEST_F(PostSaveCompromisedHelperTest, FixedLast_BulkCheckDoneRecently) {
  prefs()->SetDouble(
      kLastTimePasswordCheckCompleted,
      (base::Time::Now() - base::TimeDelta::FromMinutes(1)).ToDoubleT());
  std::vector<CompromisedCredentials> saved = {CreateCompromised(kUsername)};
  PostSaveCompromisedHelper helper({saved}, base::ASCIIToUTF16(kUsername));
  base::MockCallback<PostSaveCompromisedHelper::BubbleCallback> callback;
  EXPECT_CALL(callback, Run(BubbleType::kPasswordUpdatedSafeState, 0));
  saved = {};
  EXPECT_CALL(*profile_store(), GetAllCompromisedCredentialsImpl)
      .WillOnce(Return(saved));
  helper.AnalyzeLeakedCredentials(profile_store(), account_store(), prefs(),
                                  callback.Get());
  WaitForPasswordStore();
  EXPECT_EQ(BubbleType::kPasswordUpdatedSafeState, helper.bubble_type());
  EXPECT_EQ(0u, helper.compromised_count());
}

namespace {
class PostSaveCompromisedHelperWithTwoStoreTest
    : public PostSaveCompromisedHelperTest {
 public:
  PostSaveCompromisedHelperWithTwoStoreTest() {
    mock_account_store_ = new MockPasswordStore;
    EXPECT_TRUE(mock_account_store_->Init(&prefs_));
  }

  ~PostSaveCompromisedHelperWithTwoStoreTest() override {
    mock_account_store_->ShutdownOnUIThread();
  }

  MockPasswordStore* account_store() override {
    return mock_account_store_.get();
  }
  TestingPrefServiceSimple* prefs() { return &prefs_; }

  TestingPrefServiceSimple prefs_;

 private:
  scoped_refptr<MockPasswordStore> mock_account_store_;
};

}  // namespace

TEST_F(PostSaveCompromisedHelperWithTwoStoreTest,
       CompromisedSiteInAccountStore_ItemStayed) {
  CompromisedCredentials profile_store_compromised_credential =
      CreateCompromised(kUsername, PasswordForm::Store::kProfileStore);
  CompromisedCredentials account_store_compromised_credential =
      CreateCompromised(kUsername, PasswordForm::Store::kAccountStore);

  std::vector<CompromisedCredentials> compromised_credentials = {
      profile_store_compromised_credential,
      account_store_compromised_credential};

  PostSaveCompromisedHelper helper({compromised_credentials},
                                   base::ASCIIToUTF16(kUsername));
  EXPECT_CALL(*profile_store(), GetAllCompromisedCredentialsImpl)
      .WillOnce(Return(std::vector<CompromisedCredentials>{
          profile_store_compromised_credential}));
  EXPECT_CALL(*account_store(), GetAllCompromisedCredentialsImpl)
      .WillOnce(Return(std::vector<CompromisedCredentials>{
          account_store_compromised_credential}));
  base::MockCallback<PostSaveCompromisedHelper::BubbleCallback> callback;
  EXPECT_CALL(callback, Run(BubbleType::kUnsafeState, 2));
  helper.AnalyzeLeakedCredentials(profile_store(), account_store(), prefs(),
                                  callback.Get());
  WaitForPasswordStore();
  EXPECT_EQ(BubbleType::kUnsafeState, helper.bubble_type());
  EXPECT_EQ(2u, helper.compromised_count());
}

}  // namespace password_manager
