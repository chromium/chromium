// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/leak_detection_delegate_helper.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/password_manager/core/browser/form_parsing/form_data_parser.h"
#include "components/password_manager/core/browser/leak_detection_dialog_utils.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store/mock_password_store_interface.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

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
constexpr char16_t kLeakedPassword[] = u"leaked_password";
constexpr char16_t kOtherPassword[] = u"other_password";
constexpr char16_t kLeakedUsername[] = u"leaked_username";
constexpr char16_t kLeakedUsernameNonCanonicalized[] =
    u"Leaked_Username@gmail.com";
constexpr char16_t kOtherUsername[] = u"other_username";
constexpr char kLeakedOrigin[] = "https://www.leaked_origin.de/login";
constexpr char kOtherOrigin[] = "https://www.other_origin.de/login";

// Creates a |PasswordForm| with the supplied |origin|, |username|, |password|.
PasswordForm CreateForm(std::string_view origin,
                        std::u16string_view username,
                        std::u16string_view password = kLeakedPassword) {
  PasswordForm form;
  form.url = GURL(origin);
  form.username_value = std::u16string(username);
  form.password_value = std::u16string(password);
  form.signon_realm = form.url.DeprecatedGetOriginAsURL().spec();
  form.in_store = PasswordForm::Store::kProfileStore;
  return form;
}

}  // namespace

// Base class containing common helpers for multiple test fixtures.
class LeakDetectionDelegateHelperTestBase {
 protected:
  // Initiates determining the credential leak type.
  void InitiateGetCredentialLeakType() {
    delegate_helper_->ProcessLeakedPassword(GURL(kLeakedOrigin),
                                            kLeakedUsername, kLeakedPassword);
    task_environment_.RunUntilIdle();
  }

  // Set the expectation for the `CredentialLeakType` in the `callback_`.
  void SetOnShowLeakDetectionNotificationExpectation(
      PasswordForm::Store in_stores,
      IsReused is_reused,
      std::vector<GURL> all_urls_with_leaked_credentials = {}) {
    EXPECT_CALL(callback_, Run(in_stores, is_reused, GURL(kLeakedOrigin),
                               std::u16string(kLeakedUsername),
                               all_urls_with_leaked_credentials))
        .Times(1);
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  MockCallback<LeakDetectionDelegateHelper::LeakTypeReply> callback_;
  std::unique_ptr<LeakDetectionDelegateHelper> delegate_helper_;
};

class LeakDetectionDelegateHelperTest
    : public testing::Test,
      public LeakDetectionDelegateHelperTestBase {
 public:
  LeakDetectionDelegateHelperTest() = default;
  ~LeakDetectionDelegateHelperTest() override = default;

 protected:
  void SetUp() override {
    store_ =
        base::MakeRefCounted<testing::StrictMock<MockPasswordStoreInterface>>();

    delegate_helper_ = std::make_unique<LeakDetectionDelegateHelper>(
        store_, /*account_store=*/nullptr, callback_.Get());
  }

  void TearDown() override { store_ = nullptr; }

  // Sets the |PasswordForm|s which are retrieve from the |PasswordStore|.
  void SetGetAutofillableLoginsConsumerInvocation(
      std::vector<PasswordForm> password_forms) {
    EXPECT_CALL(*store_, GetAutofillableLogins)
        .WillOnce(testing::WithArg<0>(
            [password_forms, store = store_.get()](
                base::WeakPtr<PasswordStoreConsumer> consumer) {
              consumer->OnGetPasswordStoreResultsOrErrorFrom(
                  store, std::move(password_forms));
            }));
  }

  scoped_refptr<MockPasswordStoreInterface> store_;
};

// Credentials are neither saved nor is the password reused.
TEST_F(LeakDetectionDelegateHelperTest, NeitherSaveNotReused) {
  std::vector<PasswordForm> password_forms = {
      CreateForm(kOtherOrigin, kOtherUsername, kOtherPassword)};

  SetGetAutofillableLoginsConsumerInvocation(std::move(password_forms));
  SetOnShowLeakDetectionNotificationExpectation(PasswordForm::Store::kNotSet,
                                                IsReused(false));
  InitiateGetCredentialLeakType();
}

// Credentials are saved but the password is not reused.
TEST_F(LeakDetectionDelegateHelperTest, SavedLeakedCredentials) {
  std::vector<PasswordForm> password_forms = {
      CreateForm(kLeakedOrigin, kLeakedUsername)};

  SetGetAutofillableLoginsConsumerInvocation(std::move(password_forms));
  SetOnShowLeakDetectionNotificationExpectation(
      PasswordForm::Store::kProfileStore, IsReused(false),
      {GURL(kLeakedOrigin)});
  EXPECT_CALL(*store_, UpdateLogin);
  InitiateGetCredentialLeakType();
}

// Credentials are saved and the password is reused on a different origin.
TEST_F(LeakDetectionDelegateHelperTest,
       SavedCredentialsAndReusedPasswordOnOtherOrigin) {
  std::vector<PasswordForm> password_forms = {
      CreateForm(kLeakedOrigin, kLeakedUsername),
      CreateForm(kOtherOrigin, kLeakedUsername)};

  SetGetAutofillableLoginsConsumerInvocation(std::move(password_forms));
  SetOnShowLeakDetectionNotificationExpectation(
      PasswordForm::Store::kProfileStore, IsReused(true),
      {GURL(kLeakedOrigin), GURL(kOtherOrigin)});
  EXPECT_CALL(*store_, UpdateLogin).Times(2);
  InitiateGetCredentialLeakType();
}

// Credentials are saved and the password is reused on the same origin with
// a different username.
TEST_F(LeakDetectionDelegateHelperTest,
       SavedCredentialsAndReusedPasswordWithOtherUsername) {
  std::vector<PasswordForm> password_forms = {
      CreateForm(kLeakedOrigin, kLeakedUsername),
      CreateForm(kLeakedOrigin, kOtherUsername)};

  SetGetAutofillableLoginsConsumerInvocation(std::move(password_forms));
  SetOnShowLeakDetectionNotificationExpectation(
      PasswordForm::Store::kProfileStore, IsReused(true),
      {GURL(kLeakedOrigin)});
  EXPECT_CALL(*store_, UpdateLogin);
  InitiateGetCredentialLeakType();
}

// Credentials are not saved but the password is reused.
TEST_F(LeakDetectionDelegateHelperTest, ReusedPasswordWithOtherUsername) {
  std::vector<PasswordForm> password_forms = {
      CreateForm(kLeakedOrigin, kOtherUsername)};

  SetGetAutofillableLoginsConsumerInvocation(std::move(password_forms));
  // Don't expect anything in |all_urls_with_leaked_credentials| since it should
  // only contain url:username pairs for which both the username and password
  // match.
  SetOnShowLeakDetectionNotificationExpectation(PasswordForm::Store::kNotSet,
                                                IsReused(true));
  InitiateGetCredentialLeakType();
}

// Credentials are not saved but the password is reused on a different origin.
TEST_F(LeakDetectionDelegateHelperTest, ReusedPasswordOnOtherOrigin) {
  std::vector<PasswordForm> password_forms = {
      CreateForm(kOtherOrigin, kLeakedUsername)};

  SetGetAutofillableLoginsConsumerInvocation(std::move(password_forms));
  SetOnShowLeakDetectionNotificationExpectation(
      PasswordForm::Store::kNotSet, IsReused(true), {GURL(kOtherOrigin)});
  EXPECT_CALL(*store_, UpdateLogin);
  InitiateGetCredentialLeakType();
}

// Credentials are not saved but the password is reused with a different
// username on a different origin.
TEST_F(LeakDetectionDelegateHelperTest, ReusedPassword) {
  std::vector<PasswordForm> password_forms = {
      CreateForm(kOtherOrigin, kOtherUsername)};

  SetGetAutofillableLoginsConsumerInvocation(std::move(password_forms));
  SetOnShowLeakDetectionNotificationExpectation(PasswordForm::Store::kNotSet,
                                                IsReused(true));
  InitiateGetCredentialLeakType();
}

// All the credentials with the same username/password are marked as leaked.
TEST_F(LeakDetectionDelegateHelperTest, SaveLeakedCredentials) {
  PasswordForm leaked_origin =
      CreateForm(kLeakedOrigin, kLeakedUsername, kLeakedPassword);
  PasswordForm other_origin_same_credential =
      CreateForm(kOtherOrigin, kLeakedUsername, kLeakedPassword);
  PasswordForm leaked_origin_other_username =
      CreateForm(kLeakedOrigin, kOtherUsername, kLeakedPassword);
  SetGetAutofillableLoginsConsumerInvocation({leaked_origin,
                                              other_origin_same_credential,
                                              leaked_origin_other_username});

  SetOnShowLeakDetectionNotificationExpectation(
      PasswordForm::Store::kProfileStore, IsReused(true),
      {GURL(kLeakedOrigin), GURL(kOtherOrigin)});
  // The expected updated forms should have leaked entries.
  leaked_origin.password_issues.insert_or_assign(
      InsecureType::kLeaked,
      InsecurityMetadata(base::Time::Now(), IsMuted(false),
                         TriggerBackendNotification(false)));
  other_origin_same_credential.password_issues.insert_or_assign(
      InsecureType::kLeaked,
      InsecurityMetadata(base::Time::Now(), IsMuted(false),
                         TriggerBackendNotification(false)));
  EXPECT_CALL(*store_, UpdateLogin(leaked_origin, _));
  EXPECT_CALL(*store_, UpdateLogin(other_origin_same_credential, _));
  InitiateGetCredentialLeakType();
}

// Credential with the same canonicalized username marked as leaked.
TEST_F(LeakDetectionDelegateHelperTest, SaveLeakedCredentialsCanonicalized) {
  PasswordForm non_canonicalized_username = CreateForm(
      kOtherOrigin, kLeakedUsernameNonCanonicalized, kLeakedPassword);
  SetGetAutofillableLoginsConsumerInvocation({non_canonicalized_username});
  SetOnShowLeakDetectionNotificationExpectation(
      PasswordForm::Store::kNotSet, IsReused(true), {GURL(kOtherOrigin)});

  // The expected updated form should have leaked entries.
  non_canonicalized_username.password_issues.insert_or_assign(
      InsecureType::kLeaked,
      InsecurityMetadata(base::Time::Now(), IsMuted(false),
                         TriggerBackendNotification(false)));
  EXPECT_CALL(*store_, UpdateLogin(non_canonicalized_username, _));
  InitiateGetCredentialLeakType();
}

// Credential with the same canonicalized username was marked as leaked before.
// It's not touched again.
TEST_F(LeakDetectionDelegateHelperTest, DontUpdateAlreadyLeakedCredentials) {
  PasswordForm non_canonicalized_username = CreateForm(
      kOtherOrigin, kLeakedUsernameNonCanonicalized, kLeakedPassword);
  non_canonicalized_username.password_issues.insert_or_assign(
      InsecureType::kLeaked,
      InsecurityMetadata(base::Time::Now(), IsMuted(false),
                         TriggerBackendNotification(false)));
  SetGetAutofillableLoginsConsumerInvocation({non_canonicalized_username});
  SetOnShowLeakDetectionNotificationExpectation(
      PasswordForm::Store::kNotSet, IsReused(true), {GURL(kOtherOrigin)});

  EXPECT_CALL(*store_, UpdateLogin).Times(0);
  InitiateGetCredentialLeakType();
}

namespace {
class LeakDetectionDelegateHelperWithTwoStoreTest
    : public testing::Test,
      public LeakDetectionDelegateHelperTestBase {
 protected:
  void SetUp() override {
    profile_store_->Init(/*prefs=*/nullptr,
                         /*affiliated_match_helper=*/nullptr);
    account_store_->Init(/*prefs=*/nullptr,
                         /*affiliated_match_helper=*/nullptr);

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
  PasswordForm profile_store_form = CreateForm(kLeakedOrigin, kLeakedUsername);
  PasswordForm account_store_form = CreateForm(kOtherOrigin, kLeakedUsername);

  profile_store_->AddLogin(profile_store_form);
  account_store_->AddLogin(account_store_form);

  SetOnShowLeakDetectionNotificationExpectation(
      PasswordForm::Store::kProfileStore, IsReused(true),
      {GURL(kLeakedOrigin), GURL(kOtherOrigin)});

  InitiateGetCredentialLeakType();

  EXPECT_FALSE(profile_store_->stored_passwords()
                   .at(profile_store_form.signon_realm)
                   .at(0)
                   .password_issues.empty());
  EXPECT_FALSE(account_store_->stored_passwords()
                   .at(account_store_form.signon_realm)
                   .at(0)
                   .password_issues.empty());
}

TEST_F(LeakDetectionDelegateHelperWithTwoStoreTest,
       SavedLeakedCredentialInBothStores) {
  PasswordForm profile_store_form = CreateForm(kLeakedOrigin, kLeakedUsername);
  PasswordForm account_store_form = CreateForm(kLeakedOrigin, kLeakedUsername);

  profile_store_->AddLogin(profile_store_form);
  account_store_->AddLogin(account_store_form);

  SetOnShowLeakDetectionNotificationExpectation(
      PasswordForm::Store::kProfileStore | PasswordForm::Store::kAccountStore,
      IsReused(false), {GURL(kLeakedOrigin), GURL(kLeakedOrigin)});

  InitiateGetCredentialLeakType();

  EXPECT_FALSE(profile_store_->stored_passwords()
                   .at(profile_store_form.signon_realm)
                   .at(0)
                   .password_issues.empty());
  EXPECT_FALSE(account_store_->stored_passwords()
                   .at(account_store_form.signon_realm)
                   .at(0)
                   .password_issues.empty());
}

}  // namespace password_manager
