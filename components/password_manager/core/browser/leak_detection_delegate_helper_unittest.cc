// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/leak_detection_delegate_helper.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/password_manager/core/browser/form_parsing/form_parser.h"
#include "components/password_manager/core/browser/leak_detection_dialog_utils.h"
#include "components/password_manager/core/browser/mock_password_store.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;
using base::BindOnce;
using base::MockCallback;
using base::Unretained;
using testing::_;
using testing::ByMove;
using testing::Return;
using testing::StrictMock;
using testing::WithArg;

namespace password_manager {

namespace {
constexpr char kLeakedPassword[] = "leaked_password";
constexpr char kLeakedUsername[] = "leaked_username";
constexpr char kLeakedUsernameNonCanonicalized[] = "Leaked_Username@gmail.com";
constexpr char kOtherUsername[] = "other_username";
constexpr char kLeakedOrigin[] = "https://www.leaked_origin.de/login";
constexpr char kOtherOrigin[] = "https://www.other_origin.de/login";

// Creates a |PasswordForm| with the supplied |origin|, |username|, |password|.
PasswordForm CreateForm(base::StringPiece origin,
                        base::StringPiece username,
                        base::StringPiece password = kLeakedPassword) {
  PasswordForm form;
  form.url = GURL(ASCIIToUTF16(origin));
  form.username_value = ASCIIToUTF16(username);
  form.password_value = ASCIIToUTF16(password);
  form.signon_realm = form.url.GetOrigin().spec();
  return form;
}

}  // namespace

class LeakDetectionDelegateHelperTest : public testing::Test {
 public:
  LeakDetectionDelegateHelperTest() = default;
  ~LeakDetectionDelegateHelperTest() override = default;

 protected:
  void SetUp() override {
    store_ = new testing::StrictMock<MockPasswordStore>;
    feature_list_.InitAndEnableFeature(
        password_manager::features::kPasswordCheck);
    CHECK(store_->Init(nullptr));

    delegate_helper_ = std::make_unique<LeakDetectionDelegateHelper>(
        store_, /*account_store=*/nullptr, callback_.Get());
  }

  void TearDown() override {
    store_->ShutdownOnUIThread();
    store_ = nullptr;
  }

  // Initiates determining the credential leak type.
  void InitiateGetCredentialLeakType() {
    delegate_helper_->ProcessLeakedPassword(GURL(kLeakedOrigin),
                                            ASCIIToUTF16(kLeakedUsername),
                                            ASCIIToUTF16(kLeakedPassword));
    task_environment_.RunUntilIdle();
  }

  // Sets the |PasswordForm|s which are retrieve from the |PasswordStore|.
  void SetGetLoginByPasswordConsumerInvocation(
      std::vector<PasswordForm> password_forms) {
    std::vector<std::unique_ptr<PasswordForm>> results;
    for (auto& form : password_forms) {
      results.push_back(std::make_unique<PasswordForm>(std::move(form)));
    }
    EXPECT_CALL(*store_, FillMatchingLoginsByPassword)
        .WillOnce(Return(ByMove(std::move(results))));
  }

  // Set the expectation for the |CredentialLeakType| in the callback_.
  void SetOnShowLeakDetectionNotificationExpectation(
      IsSaved is_saved,
      IsReused is_reused,
      CompromisedSitesCount other_sites) {
    EXPECT_CALL(callback_, Run(is_saved, is_reused, GURL(kLeakedOrigin),
                               ASCIIToUTF16(kLeakedUsername), other_sites))
        .Times(1);
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  MockCallback<LeakDetectionDelegateHelper::LeakTypeReply> callback_;
  scoped_refptr<MockPasswordStore> store_;
  std::unique_ptr<LeakDetectionDelegateHelper> delegate_helper_;
  base::test::ScopedFeatureList feature_list_;
};

// Credentials are neither saved nor is the password reused.
TEST_F(LeakDetectionDelegateHelperTest, NeitherSaveNotReused) {
  std::vector<PasswordForm> password_forms;

  SetGetLoginByPasswordConsumerInvocation(std::move(password_forms));
  SetOnShowLeakDetectionNotificationExpectation(IsSaved(false), IsReused(false),
                                                CompromisedSitesCount(0));
  InitiateGetCredentialLeakType();
}

// Credentials are saved but the password is not reused.
TEST_F(LeakDetectionDelegateHelperTest, SavedLeakedCredentials) {
  std::vector<PasswordForm> password_forms = {
      CreateForm(kLeakedOrigin, kLeakedUsername)};

  SetGetLoginByPasswordConsumerInvocation(std::move(password_forms));
  SetOnShowLeakDetectionNotificationExpectation(IsSaved(true), IsReused(false),
                                                CompromisedSitesCount(1));
  EXPECT_CALL(*store_, AddCompromisedCredentialsImpl)
      .Times(base::FeatureList::IsEnabled(features::kPasswordCheck));
  InitiateGetCredentialLeakType();
}

// Credentials are saved and the password is reused on a different origin.
TEST_F(LeakDetectionDelegateHelperTest,
       SavedCredentialsAndReusedPasswordOnOtherOrigin) {
  std::vector<PasswordForm> password_forms = {
      CreateForm(kLeakedOrigin, kLeakedUsername),
      CreateForm(kOtherOrigin, kLeakedUsername)};

  SetGetLoginByPasswordConsumerInvocation(std::move(password_forms));
  SetOnShowLeakDetectionNotificationExpectation(IsSaved(true), IsReused(true),
                                                CompromisedSitesCount(2));
  EXPECT_CALL(*store_, AddCompromisedCredentialsImpl)
      .Times(2 * base::FeatureList::IsEnabled(features::kPasswordCheck));
  InitiateGetCredentialLeakType();
}

// Credentials are saved and the password is reused on the same origin with
// a different username.
TEST_F(LeakDetectionDelegateHelperTest,
       SavedCredentialsAndReusedPasswordWithOtherUsername) {
  std::vector<PasswordForm> password_forms = {
      CreateForm(kLeakedOrigin, kLeakedUsername),
      CreateForm(kLeakedOrigin, kOtherUsername)};

  SetGetLoginByPasswordConsumerInvocation(std::move(password_forms));
  SetOnShowLeakDetectionNotificationExpectation(IsSaved(true), IsReused(true),
                                                CompromisedSitesCount(1));
  EXPECT_CALL(*store_, AddCompromisedCredentialsImpl)
      .Times(base::FeatureList::IsEnabled(features::kPasswordCheck));
  InitiateGetCredentialLeakType();
}

// Credentials are not saved but the password is reused.
TEST_F(LeakDetectionDelegateHelperTest, ReusedPasswordWithOtherUsername) {
  std::vector<PasswordForm> password_forms = {
      CreateForm(kLeakedOrigin, kOtherUsername)};

  SetGetLoginByPasswordConsumerInvocation(std::move(password_forms));
  SetOnShowLeakDetectionNotificationExpectation(IsSaved(false), IsReused(true),
                                                CompromisedSitesCount(0));
  InitiateGetCredentialLeakType();
}

// Credentials are not saved but the password is reused on a different origin.
TEST_F(LeakDetectionDelegateHelperTest, ReusedPasswordOnOtherOrigin) {
  std::vector<PasswordForm> password_forms = {
      CreateForm(kOtherOrigin, kLeakedUsername)};

  SetGetLoginByPasswordConsumerInvocation(std::move(password_forms));
  SetOnShowLeakDetectionNotificationExpectation(IsSaved(false), IsReused(true),
                                                CompromisedSitesCount(1));
  EXPECT_CALL(*store_, AddCompromisedCredentialsImpl)
      .Times(base::FeatureList::IsEnabled(features::kPasswordCheck));
  InitiateGetCredentialLeakType();
}

// Credentials are not saved but the password is reused with a different
// username on a different origin.
TEST_F(LeakDetectionDelegateHelperTest, ReusedPassword) {
  std::vector<PasswordForm> password_forms = {
      CreateForm(kOtherOrigin, kOtherUsername)};

  SetGetLoginByPasswordConsumerInvocation(std::move(password_forms));
  SetOnShowLeakDetectionNotificationExpectation(IsSaved(false), IsReused(true),
                                                CompromisedSitesCount(0));
  InitiateGetCredentialLeakType();
}

// All the credentials with the same username/password are marked as leaked.
TEST_F(LeakDetectionDelegateHelperTest, SaveLeakedCredentials) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kPasswordCheck);

  SetGetLoginByPasswordConsumerInvocation(
      {CreateForm(kLeakedOrigin, kLeakedUsername, kLeakedPassword),
       CreateForm(kOtherOrigin, kLeakedUsername, kLeakedPassword),
       CreateForm(kLeakedOrigin, kOtherUsername, kLeakedPassword)});
  SetOnShowLeakDetectionNotificationExpectation(IsSaved(true), IsReused(true),
                                                CompromisedSitesCount(2));
  EXPECT_CALL(*store_, AddCompromisedCredentialsImpl(CompromisedCredentials{
                           GetSignonRealm(GURL(kLeakedOrigin)),
                           ASCIIToUTF16(kLeakedUsername), base::Time::Now(),
                           CompromiseType::kLeaked}));
  EXPECT_CALL(*store_, AddCompromisedCredentialsImpl(CompromisedCredentials{
                           GetSignonRealm(GURL(kOtherOrigin)),
                           ASCIIToUTF16(kLeakedUsername), base::Time::Now(),
                           CompromiseType::kLeaked}));
  InitiateGetCredentialLeakType();
}

// Credential with the same canonicalized username marked as leaked.
TEST_F(LeakDetectionDelegateHelperTest, SaveLeakedCredentialsCanonicalized) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kPasswordCheck);

  SetGetLoginByPasswordConsumerInvocation({CreateForm(
      kOtherOrigin, kLeakedUsernameNonCanonicalized, kLeakedPassword)});
  SetOnShowLeakDetectionNotificationExpectation(IsSaved(false), IsReused(true),
                                                CompromisedSitesCount(1));

  EXPECT_CALL(*store_, AddCompromisedCredentialsImpl(CompromisedCredentials{
                           GetSignonRealm(GURL(kOtherOrigin)),
                           ASCIIToUTF16(kLeakedUsernameNonCanonicalized),
                           base::Time::Now(), CompromiseType::kLeaked}));
  InitiateGetCredentialLeakType();
}

namespace {
class LeakDetectionDelegateHelperWithTwoStoreTest
    : public LeakDetectionDelegateHelperTest {
 protected:
  void SetUp() override {
    feature_list_.InitAndEnableFeature(
        password_manager::features::kPasswordCheck);
    profile_store_->Init(/*prefs=*/nullptr);
    account_store_->Init(/*prefs=*/nullptr);

    delegate_helper_ = std::make_unique<LeakDetectionDelegateHelper>(
        profile_store_, account_store_, callback_.Get());
  }

  void TearDown() override {
    account_store_->ShutdownOnUIThread();
    profile_store_->ShutdownOnUIThread();
    task_environment_.RunUntilIdle();
  }

  scoped_refptr<TestPasswordStore> profile_store_ =
      base::MakeRefCounted<TestPasswordStore>(IsAccountStore(false));
  scoped_refptr<TestPasswordStore> account_store_ =
      base::MakeRefCounted<TestPasswordStore>(IsAccountStore(true));
};

}  // namespace

TEST_F(LeakDetectionDelegateHelperWithTwoStoreTest, SavedLeakedCredentials) {
  profile_store_->AddLogin(CreateForm(kLeakedOrigin, kLeakedUsername));
  account_store_->AddLogin(CreateForm(kOtherOrigin, kLeakedUsername));

  SetOnShowLeakDetectionNotificationExpectation(IsSaved(true), IsReused(true),
                                                CompromisedSitesCount(2));

  InitiateGetCredentialLeakType();

  EXPECT_EQ(base::FeatureList::IsEnabled(features::kPasswordCheck),
            !profile_store_->compromised_credentials().empty());
  EXPECT_EQ(base::FeatureList::IsEnabled(features::kPasswordCheck),
            !account_store_->compromised_credentials().empty());
}

}  // namespace password_manager
