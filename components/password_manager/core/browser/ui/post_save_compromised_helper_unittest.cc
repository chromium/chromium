// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/ui/post_save_compromised_helper.h"

#include "base/feature_list.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store/mock_password_store_interface.h"
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
constexpr char16_t kPassword[] = u"unsafe";
constexpr char16_t kPassword2[] = u"unsafe2";
constexpr char16_t kPassword3[] = u"unsafe3";

// Creates a form.
PasswordForm CreateForm(
    const std::string& signon_realm,
    const std::u16string& username,
    const std::u16string& password,
    PasswordForm::Store store = PasswordForm::Store::kProfileStore) {
  PasswordForm form;
  form.signon_realm = signon_realm;
  form.username_value = username;
  form.password_value = password;
  form.in_store = store;
  return form;
}

PasswordForm CreateInsecureCredential(
    const std::u16string& username,
    const std::u16string& password,
    PasswordForm::Store store = PasswordForm::Store::kProfileStore,
    IsMuted muted = IsMuted(false)) {
  PasswordForm compromised =
      CreateForm(kSignonRealm, username, password, store);
  compromised.password_issues.insert(
      {InsecureType::kLeaked,
       InsecurityMetadata(base::Time(), muted,
                          TriggerBackendNotification(false))});
  return compromised;
}

}  // namespace

class PostSaveCompromisedHelperTest : public testing::Test {
 public:
  PostSaveCompromisedHelperTest() {
    mock_profile_store_ = new MockPasswordStoreInterface;
    prefs_.registry()->RegisterDoublePref(kLastTimePasswordCheckCompleted, 0.0);
  }

  ~PostSaveCompromisedHelperTest() override {
    mock_profile_store_->ShutdownOnUIThread();
  }

  void ExpectGetLoginsCall(std::vector<PasswordForm> password_forms) {
    EXPECT_CALL(*profile_store(), GetAutofillableLogins)
        .WillOnce(testing::WithArg<0>(
            [password_forms, store = mock_profile_store_.get()](
                base::WeakPtr<PasswordStoreConsumer> consumer) {
              consumer->OnGetPasswordStoreResultsOrErrorFrom(store,
                                                             password_forms);
            }));
  }

  void WaitForPasswordStore() { task_environment_.RunUntilIdle(); }

  MockPasswordStoreInterface* profile_store() {
    return mock_profile_store_.get();
  }
  virtual MockPasswordStoreInterface* account_store() { return nullptr; }
  TestingPrefServiceSimple* prefs() { return &prefs_; }

 protected:
  TestingPrefServiceSimple prefs_;

 private:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  scoped_refptr<MockPasswordStoreInterface> mock_profile_store_;
};

TEST_F(PostSaveCompromisedHelperTest, DefaultState) {
  PostSaveCompromisedHelper helper({}, kUsername);
  EXPECT_EQ(BubbleType::kNoBubble, helper.bubble_type());
  EXPECT_EQ(0u, helper.compromised_count());
}

TEST_F(PostSaveCompromisedHelperTest, EmptyStore) {
  prefs()->SetDouble(
      kLastTimePasswordCheckCompleted,
      (base::Time::Now() - base::Minutes(1)).InSecondsFSinceUnixEpoch());
  PostSaveCompromisedHelper helper({}, kUsername);
  base::MockCallback<PostSaveCompromisedHelper::BubbleCallback> callback;
  EXPECT_CALL(callback, Run(BubbleType::kNoBubble, 0));
  ExpectGetLoginsCall({});
  helper.AnalyzeLeakedCredentials(profile_store(), account_store(), prefs(),
                                  callback.Get());
  WaitForPasswordStore();
  EXPECT_EQ(BubbleType::kNoBubble, helper.bubble_type());
  EXPECT_EQ(0u, helper.compromised_count());
}

TEST_F(PostSaveCompromisedHelperTest, RandomSite_FullStore) {
  prefs()->SetDouble(
      kLastTimePasswordCheckCompleted,
      (base::Time::Now() - base::Minutes(1)).InSecondsFSinceUnixEpoch());
  PostSaveCompromisedHelper helper({}, kUsername);
  base::MockCallback<PostSaveCompromisedHelper::BubbleCallback> callback;
  EXPECT_CALL(callback, Run(BubbleType::kNoBubble, _));

  PasswordForm form = CreateForm(kSignonRealm, kUsername2, kPassword2);
  ExpectGetLoginsCall({form});
  helper.AnalyzeLeakedCredentials(profile_store(), account_store(), prefs(),
                                  callback.Get());
  WaitForPasswordStore();
  EXPECT_EQ(BubbleType::kNoBubble, helper.bubble_type());
}

TEST_F(PostSaveCompromisedHelperTest, CompromisedSite_ItemStayed) {
  prefs()->SetDouble(
      kLastTimePasswordCheckCompleted,
      (base::Time::Now() - base::Minutes(1)).InSecondsFSinceUnixEpoch());
  PasswordForm form1 = CreateForm(kSignonRealm, kUsername, kPassword);
  form1.password_issues.insert({InsecureType::kLeaked, InsecurityMetadata()});
  PasswordForm form2 = CreateForm(kSignonRealm, kUsername2, kPassword2);
  form2.password_issues.insert({InsecureType::kLeaked, InsecurityMetadata()});
  PasswordForm insecure_credential =
      CreateInsecureCredential(kUsername, kPassword);

  PostSaveCompromisedHelper helper(std::vector{insecure_credential}, kUsername);
  base::MockCallback<PostSaveCompromisedHelper::BubbleCallback> callback;
  ExpectGetLoginsCall({form1, form2});
  EXPECT_CALL(callback, Run(BubbleType::kNoBubble, 2));
  helper.AnalyzeLeakedCredentials(profile_store(), account_store(), prefs(),
                                  callback.Get());
  WaitForPasswordStore();
  EXPECT_EQ(BubbleType::kNoBubble, helper.bubble_type());
}

TEST_F(PostSaveCompromisedHelperTest, CompromisedSite_ItemGone) {
  prefs()->SetDouble(
      kLastTimePasswordCheckCompleted,
      (base::Time::Now() - base::Minutes(1)).InSecondsFSinceUnixEpoch());
  PasswordForm form1 = CreateForm(kSignonRealm, kUsername, kPassword);
  PasswordForm form2 = CreateInsecureCredential(kUsername2, kPassword2);
  PasswordForm form3 = CreateInsecureCredential(kUsername, kPassword);

  PostSaveCompromisedHelper helper(std::vector{form2, form3}, kUsername);
  base::MockCallback<PostSaveCompromisedHelper::BubbleCallback> callback;
  EXPECT_CALL(callback, Run(BubbleType::kPasswordUpdatedWithMoreToFix, 1));
  ExpectGetLoginsCall({form1, form2});
  helper.AnalyzeLeakedCredentials(profile_store(), account_store(), prefs(),
                                  callback.Get());
  WaitForPasswordStore();
  EXPECT_EQ(BubbleType::kPasswordUpdatedWithMoreToFix, helper.bubble_type());
  EXPECT_EQ(1u, helper.compromised_count());
}

TEST_F(PostSaveCompromisedHelperTest, FixedLast_BulkCheckNeverDone) {
  PasswordForm insecure_credential =
      CreateInsecureCredential(kUsername, kPassword);
  PostSaveCompromisedHelper helper(std::vector{insecure_credential}, kUsername);
  base::MockCallback<PostSaveCompromisedHelper::BubbleCallback> callback;
  EXPECT_CALL(callback, Run(BubbleType::kNoBubble, 0));
  EXPECT_CALL(*profile_store(), GetAutofillableLogins).Times(0);
  helper.AnalyzeLeakedCredentials(profile_store(), account_store(), prefs(),
                                  callback.Get());
  WaitForPasswordStore();
  EXPECT_EQ(BubbleType::kNoBubble, helper.bubble_type());
  EXPECT_EQ(0u, helper.compromised_count());
}

TEST_F(PostSaveCompromisedHelperTest, FixedLast_BulkCheckDoneLongAgo) {
  prefs()->SetDouble(
      kLastTimePasswordCheckCompleted,
      (base::Time::Now() - base::Days(5)).InSecondsFSinceUnixEpoch());
  PasswordForm insecure_credential =
      CreateInsecureCredential(kUsername, kPassword);
  PostSaveCompromisedHelper helper(std::vector{insecure_credential}, kUsername);
  base::MockCallback<PostSaveCompromisedHelper::BubbleCallback> callback;
  EXPECT_CALL(callback, Run(BubbleType::kNoBubble, 0));
  EXPECT_CALL(*profile_store(), GetAutofillableLogins).Times(0);
  helper.AnalyzeLeakedCredentials(profile_store(), account_store(), prefs(),
                                  callback.Get());
  WaitForPasswordStore();
  EXPECT_EQ(BubbleType::kNoBubble, helper.bubble_type());
  EXPECT_EQ(0u, helper.compromised_count());
}

TEST_F(PostSaveCompromisedHelperTest, FixedLast_BulkCheckDoneRecently) {
  prefs()->SetDouble(
      kLastTimePasswordCheckCompleted,
      (base::Time::Now() - base::Minutes(1)).InSecondsFSinceUnixEpoch());
  PasswordForm insecure_credential =
      CreateInsecureCredential(kUsername, kPassword);
  PostSaveCompromisedHelper helper(std::vector{insecure_credential}, kUsername);
  base::MockCallback<PostSaveCompromisedHelper::BubbleCallback> callback;
  EXPECT_CALL(callback, Run(BubbleType::kPasswordUpdatedSafeState, 0));
  ExpectGetLoginsCall({CreateForm(kSignonRealm, kUsername, kPassword)});
  helper.AnalyzeLeakedCredentials(profile_store(), account_store(), prefs(),
                                  callback.Get());
  WaitForPasswordStore();
  EXPECT_EQ(BubbleType::kPasswordUpdatedSafeState, helper.bubble_type());
  EXPECT_EQ(0u, helper.compromised_count());
}

TEST_F(PostSaveCompromisedHelperTest, BubbleShownEvenIfIssueIsMuted) {
  prefs()->SetDouble(
      kLastTimePasswordCheckCompleted,
      (base::Time::Now() - base::Minutes(1)).InSecondsFSinceUnixEpoch());
  PasswordForm insecure_credential = CreateInsecureCredential(
      kUsername, kPassword, PasswordForm::Store::kProfileStore, IsMuted(true));
  PostSaveCompromisedHelper helper(std::vector{insecure_credential}, kUsername);
  base::MockCallback<PostSaveCompromisedHelper::BubbleCallback> callback;
  EXPECT_CALL(callback, Run(BubbleType::kPasswordUpdatedSafeState, 0));
  ExpectGetLoginsCall({CreateForm(kSignonRealm, kUsername, kPassword)});
  helper.AnalyzeLeakedCredentials(profile_store(), account_store(), prefs(),
                                  callback.Get());
  WaitForPasswordStore();
  EXPECT_EQ(BubbleType::kPasswordUpdatedSafeState, helper.bubble_type());
  EXPECT_EQ(0u, helper.compromised_count());
}

TEST_F(PostSaveCompromisedHelperTest, MutedIssuesNotIncludedToCount) {
  prefs()->SetDouble(
      kLastTimePasswordCheckCompleted,
      (base::Time::Now() - base::Minutes(1)).InSecondsFSinceUnixEpoch());
  PasswordForm insecure_credential =
      CreateInsecureCredential(kUsername, kPassword);
  PostSaveCompromisedHelper helper(std::vector{insecure_credential}, kUsername);
  base::MockCallback<PostSaveCompromisedHelper::BubbleCallback> callback;
  EXPECT_CALL(callback, Run(BubbleType::kPasswordUpdatedWithMoreToFix, 1));
  PasswordForm form1 = CreateForm(kSignonRealm, kUsername, kPassword);
  PasswordForm form2 = CreateForm(kSignonRealm, kUsername2, kPassword2);
  form2.password_issues.insert({InsecureType::kLeaked, InsecurityMetadata()});
  PasswordForm form3 = CreateForm(kSignonRealm, kUsername3, kPassword3);
  form3.password_issues.insert(
      {InsecureType::kLeaked,
       InsecurityMetadata(base::Time(), IsMuted(true),
                          TriggerBackendNotification(false))});

  ExpectGetLoginsCall({form1, form2, form3});
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
    mock_account_store_ = new MockPasswordStoreInterface;
  }

  ~PostSaveCompromisedHelperWithTwoStoreTest() override = default;

  MockPasswordStoreInterface* account_store() override {
    return mock_account_store_.get();
  }

 private:
  scoped_refptr<MockPasswordStoreInterface> mock_account_store_;
};

}  // namespace

TEST_F(PostSaveCompromisedHelperWithTwoStoreTest,
       CompromisedSiteInAccountStore_ItemStayed) {
  prefs()->SetDouble(
      kLastTimePasswordCheckCompleted,
      (base::Time::Now() - base::Minutes(1)).InSecondsFSinceUnixEpoch());
  PasswordForm compromised_profile_credential = CreateInsecureCredential(
      kUsername, kPassword, PasswordForm::Store::kProfileStore);
  PasswordForm compromised_account_credential = CreateInsecureCredential(
      kUsername, kPassword, PasswordForm::Store::kAccountStore);

  PostSaveCompromisedHelper helper(std::vector{compromised_profile_credential,
                                               compromised_account_credential},
                                   kUsername);
  EXPECT_CALL(*profile_store(), GetAutofillableLogins)
      .WillOnce(testing::WithArg<0>(
          [store =
               profile_store()](base::WeakPtr<PasswordStoreConsumer> consumer) {
            std::vector<PasswordForm> results;
            results.push_back(CreateForm(kSignonRealm, kUsername, kPassword));
            results.back().password_issues.insert(
                {InsecureType::kLeaked, InsecurityMetadata()});
            consumer->OnGetPasswordStoreResultsOrErrorFrom(store,
                                                           std::move(results));
          }));
  EXPECT_CALL(*account_store(), GetAutofillableLogins)
      .WillOnce(testing::WithArg<0>(
          [store =
               account_store()](base::WeakPtr<PasswordStoreConsumer> consumer) {
            std::vector<PasswordForm> results;
            results.push_back(CreateForm(kSignonRealm, kUsername, kPassword,
                                         PasswordForm::Store::kAccountStore));
            results.back().password_issues.insert(
                {InsecureType::kLeaked, InsecurityMetadata()});
            consumer->OnGetPasswordStoreResultsOrErrorFrom(store,
                                                           std::move(results));
          }));
  base::MockCallback<PostSaveCompromisedHelper::BubbleCallback> callback;
  EXPECT_CALL(callback, Run(BubbleType::kNoBubble, _));
  helper.AnalyzeLeakedCredentials(profile_store(), account_store(), prefs(),
                                  callback.Get());
  WaitForPasswordStore();
  EXPECT_EQ(BubbleType::kNoBubble, helper.bubble_type());
}

}  // namespace password_manager
