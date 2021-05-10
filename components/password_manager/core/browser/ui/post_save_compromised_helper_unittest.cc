// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/ui/post_save_compromised_helper.h"

#include "base/feature_list.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/password_manager/core/browser/mock_password_store.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {
namespace {

using BubbleType = PostSaveCompromisedHelper::BubbleType;
using prefs::kLastTimePasswordCheckCompleted;
using testing::_;
using testing::Return;

constexpr char kSignonRealm[] = "https://example.com/";
constexpr char16_t kUsername[] = u"user";
constexpr char16_t kUsername2[] = u"user2";
constexpr char16_t kUsername3[] = u"user3";

InsecureCredential CreateInsecureCredential(
    base::StringPiece16 username,
    PasswordForm::Store store = PasswordForm::Store::kProfileStore,
    IsMuted muted = IsMuted(false)) {
  InsecureCredential compromised(kSignonRealm, std::u16string(username),
                                 base::Time(), InsecureType::kLeaked, muted);
  compromised.in_store = store;
  return compromised;
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
  PostSaveCompromisedHelper helper({}, kUsername);
  EXPECT_EQ(BubbleType::kNoBubble, helper.bubble_type());
  EXPECT_EQ(0u, helper.compromised_count());
}

TEST_F(PostSaveCompromisedHelperTest, EmptyStore) {
  prefs()->SetDouble(
      kLastTimePasswordCheckCompleted,
      (base::Time::Now() - base::TimeDelta::FromMinutes(1)).ToDoubleT());
  PostSaveCompromisedHelper helper({}, kUsername);
  base::MockCallback<PostSaveCompromisedHelper::BubbleCallback> callback;
  EXPECT_CALL(callback, Run(BubbleType::kNoBubble, 0));
  EXPECT_CALL(*profile_store(), GetAllInsecureCredentialsImpl);
  helper.AnalyzeLeakedCredentials(profile_store(), account_store(), prefs(),
                                  callback.Get());
  WaitForPasswordStore();
  EXPECT_EQ(BubbleType::kNoBubble, helper.bubble_type());
  EXPECT_EQ(0u, helper.compromised_count());
}

TEST_F(PostSaveCompromisedHelperTest, RandomSite_FullStore) {
  prefs()->SetDouble(
      kLastTimePasswordCheckCompleted,
      (base::Time::Now() - base::TimeDelta::FromMinutes(1)).ToDoubleT());
  PostSaveCompromisedHelper helper({}, kUsername);
  base::MockCallback<PostSaveCompromisedHelper::BubbleCallback> callback;
  EXPECT_CALL(callback, Run(BubbleType::kNoBubble, _));
  std::vector<InsecureCredential> saved = {
      CreateInsecureCredential(kUsername2)};
  EXPECT_CALL(*profile_store(), GetAllInsecureCredentialsImpl)
      .WillOnce(Return(saved));
  helper.AnalyzeLeakedCredentials(profile_store(), account_store(), prefs(),
                                  callback.Get());
  WaitForPasswordStore();
  EXPECT_EQ(BubbleType::kNoBubble, helper.bubble_type());
}

TEST_F(PostSaveCompromisedHelperTest, CompromisedSite_ItemStayed) {
  prefs()->SetDouble(
      kLastTimePasswordCheckCompleted,
      (base::Time::Now() - base::TimeDelta::FromMinutes(1)).ToDoubleT());
  std::vector<InsecureCredential> saved = {
      CreateInsecureCredential(kUsername),
      CreateInsecureCredential(kUsername2)};
  PostSaveCompromisedHelper helper({saved}, kUsername);
  base::MockCallback<PostSaveCompromisedHelper::BubbleCallback> callback;
  EXPECT_CALL(callback, Run(BubbleType::kNoBubble, _));
  EXPECT_CALL(*profile_store(), GetAllInsecureCredentialsImpl)
      .WillOnce(Return(saved));
  helper.AnalyzeLeakedCredentials(profile_store(), account_store(), prefs(),
                                  callback.Get());
  WaitForPasswordStore();
  EXPECT_EQ(BubbleType::kNoBubble, helper.bubble_type());
}

TEST_F(PostSaveCompromisedHelperTest, CompromisedSite_ItemGone) {
  prefs()->SetDouble(
      kLastTimePasswordCheckCompleted,
      (base::Time::Now() - base::TimeDelta::FromMinutes(1)).ToDoubleT());
  std::vector<InsecureCredential> saved = {
      CreateInsecureCredential(kUsername),
      CreateInsecureCredential(kUsername2)};
  PostSaveCompromisedHelper helper({saved}, kUsername);
  base::MockCallback<PostSaveCompromisedHelper::BubbleCallback> callback;
  EXPECT_CALL(callback, Run(BubbleType::kPasswordUpdatedWithMoreToFix, 1));
  saved = {CreateInsecureCredential(kUsername2)};
  EXPECT_CALL(*profile_store(), GetAllInsecureCredentialsImpl)
      .WillOnce(Return(saved));
  helper.AnalyzeLeakedCredentials(profile_store(), account_store(), prefs(),
                                  callback.Get());
  WaitForPasswordStore();
  EXPECT_EQ(BubbleType::kPasswordUpdatedWithMoreToFix, helper.bubble_type());
  EXPECT_EQ(1u, helper.compromised_count());
}

TEST_F(PostSaveCompromisedHelperTest, FixedLast_BulkCheckNeverDone) {
  std::vector<InsecureCredential> saved = {CreateInsecureCredential(kUsername)};
  PostSaveCompromisedHelper helper({saved}, kUsername);
  base::MockCallback<PostSaveCompromisedHelper::BubbleCallback> callback;
  EXPECT_CALL(callback, Run(BubbleType::kNoBubble, 0));
  EXPECT_CALL(*profile_store(), GetAllInsecureCredentialsImpl).Times(0);
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
  std::vector<InsecureCredential> saved = {CreateInsecureCredential(kUsername)};
  PostSaveCompromisedHelper helper({saved}, kUsername);
  base::MockCallback<PostSaveCompromisedHelper::BubbleCallback> callback;
  EXPECT_CALL(callback, Run(BubbleType::kNoBubble, 0));
  EXPECT_CALL(*profile_store(), GetAllInsecureCredentialsImpl).Times(0);
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
  std::vector<InsecureCredential> saved = {CreateInsecureCredential(kUsername)};
  PostSaveCompromisedHelper helper({saved}, kUsername);
  base::MockCallback<PostSaveCompromisedHelper::BubbleCallback> callback;
  EXPECT_CALL(callback, Run(BubbleType::kPasswordUpdatedSafeState, 0));
  saved = {};
  EXPECT_CALL(*profile_store(), GetAllInsecureCredentialsImpl)
      .WillOnce(Return(saved));
  helper.AnalyzeLeakedCredentials(profile_store(), account_store(), prefs(),
                                  callback.Get());
  WaitForPasswordStore();
  EXPECT_EQ(BubbleType::kPasswordUpdatedSafeState, helper.bubble_type());
  EXPECT_EQ(0u, helper.compromised_count());
}

TEST_F(PostSaveCompromisedHelperTest, BubbleShownEvenIfIssueIsMuted) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatureState(features::kMutingCompromisedCredentials,
                                    true);

  prefs()->SetDouble(
      kLastTimePasswordCheckCompleted,
      (base::Time::Now() - base::TimeDelta::FromMinutes(1)).ToDoubleT());
  std::vector<InsecureCredential> saved = {CreateInsecureCredential(
      kUsername, PasswordForm::Store::kProfileStore, IsMuted(true))};
  PostSaveCompromisedHelper helper({saved}, kUsername);
  base::MockCallback<PostSaveCompromisedHelper::BubbleCallback> callback;
  EXPECT_CALL(callback, Run(BubbleType::kPasswordUpdatedSafeState, 0));
  saved = {};
  EXPECT_CALL(*profile_store(), GetAllInsecureCredentialsImpl)
      .WillOnce(Return(saved));
  helper.AnalyzeLeakedCredentials(profile_store(), account_store(), prefs(),
                                  callback.Get());
  WaitForPasswordStore();
  EXPECT_EQ(BubbleType::kPasswordUpdatedSafeState, helper.bubble_type());
  EXPECT_EQ(0u, helper.compromised_count());
}

TEST_F(PostSaveCompromisedHelperTest, MutedIssuesNotIncludedToCount) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatureState(features::kMutingCompromisedCredentials,
                                    true);

  prefs()->SetDouble(
      kLastTimePasswordCheckCompleted,
      (base::Time::Now() - base::TimeDelta::FromMinutes(1)).ToDoubleT());
  std::vector<InsecureCredential> saved = {CreateInsecureCredential(kUsername)};
  PostSaveCompromisedHelper helper({saved}, kUsername);
  base::MockCallback<PostSaveCompromisedHelper::BubbleCallback> callback;
  EXPECT_CALL(callback, Run(BubbleType::kPasswordUpdatedWithMoreToFix, 1));
  saved = {
      CreateInsecureCredential(kUsername2),
      CreateInsecureCredential(kUsername3, PasswordForm::Store::kProfileStore,
                               IsMuted(true)),
  };
  EXPECT_CALL(*profile_store(), GetAllInsecureCredentialsImpl)
      .WillOnce(Return(saved));
  helper.AnalyzeLeakedCredentials(profile_store(), account_store(), prefs(),
                                  callback.Get());
  WaitForPasswordStore();
  EXPECT_EQ(BubbleType::kPasswordUpdatedWithMoreToFix, helper.bubble_type());
  EXPECT_EQ(1u, helper.compromised_count());
}

namespace {
class PostSaveCompromisedHelperWithTwoStoreTest
    : public PostSaveCompromisedHelperTest {
 public:
  PostSaveCompromisedHelperWithTwoStoreTest() {
    mock_account_store_ = new MockPasswordStore;
    EXPECT_TRUE(mock_account_store_->Init(prefs()));
  }

  ~PostSaveCompromisedHelperWithTwoStoreTest() override {
    mock_account_store_->ShutdownOnUIThread();
  }

  MockPasswordStore* account_store() override {
    return mock_account_store_.get();
  }

 private:
  scoped_refptr<MockPasswordStore> mock_account_store_;
};

}  // namespace

TEST_F(PostSaveCompromisedHelperWithTwoStoreTest,
       CompromisedSiteInAccountStore_ItemStayed) {
  prefs()->SetDouble(
      kLastTimePasswordCheckCompleted,
      (base::Time::Now() - base::TimeDelta::FromMinutes(1)).ToDoubleT());
  InsecureCredential profile_store_compromised_credential =
      CreateInsecureCredential(kUsername, PasswordForm::Store::kProfileStore);
  InsecureCredential account_store_compromised_credential =
      CreateInsecureCredential(kUsername, PasswordForm::Store::kAccountStore);

  std::vector<InsecureCredential> compromised_credentials = {
      profile_store_compromised_credential,
      account_store_compromised_credential};

  PostSaveCompromisedHelper helper({compromised_credentials}, kUsername);
  EXPECT_CALL(*profile_store(), GetAllInsecureCredentialsImpl)
      .WillOnce(Return(std::vector<InsecureCredential>{
          profile_store_compromised_credential}));
  EXPECT_CALL(*account_store(), GetAllInsecureCredentialsImpl)
      .WillOnce(Return(std::vector<InsecureCredential>{
          account_store_compromised_credential}));
  base::MockCallback<PostSaveCompromisedHelper::BubbleCallback> callback;
  EXPECT_CALL(callback, Run(BubbleType::kNoBubble, _));
  helper.AnalyzeLeakedCredentials(profile_store(), account_store(), prefs(),
                                  callback.Get());
  WaitForPasswordStore();
  EXPECT_EQ(BubbleType::kNoBubble, helper.bubble_type());
}

}  // namespace password_manager
