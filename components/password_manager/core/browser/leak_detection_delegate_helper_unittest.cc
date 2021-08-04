// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/leak_detection_delegate_helper.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/password_manager/core/browser/form_parsing/form_parser.h"
#include "components/password_manager/core/browser/leak_detection_dialog_utils.h"
#include "components/password_manager/core/browser/mock_password_store.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/test_password_store.h"
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
constexpr char16_t kLeakedUsername[] = u"leaked_username";
constexpr char16_t kLeakedUsernameNonCanonicalized[] =
    u"Leaked_Username@gmail.com";
constexpr char16_t kOtherUsername[] = u"other_username";
constexpr char kLeakedOrigin[] = "https://www.leaked_origin.de/login";
constexpr char kOtherOrigin[] = "https://www.other_origin.de/login";

// Creates a |PasswordForm| with the supplied |origin|, |username|, |password|.
PasswordForm CreateForm(base::StringPiece origin,
                        base::StringPiece16 username,
                        base::StringPiece16 password = kLeakedPassword) {
  PasswordForm form;
  form.url = GURL(origin);
  form.username_value = std::u16string(username);
  form.password_value = std::u16string(password);
  form.signon_realm = form.url.GetOrigin().spec();
  form.in_store = PasswordForm::Store::kProfileStore;
  return form;
}

}  // namespace

class LeakDetectionDelegateHelperTest : public testing::Test {
 public:
  LeakDetectionDelegateHelperTest() = default;
  ~LeakDetectionDelegateHelperTest() override = default;

 protected:
  void SetUp() override {
    // TODO(crbug.com/1223022): Use StrickMock after MockPasswordStore is
    // replaced with the MockPasswordStoreInterface.
    store_ = base::MakeRefCounted<testing::NiceMock<MockPasswordStore>>();
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
                                            kLeakedUsername, kLeakedPassword);
    task_environment_.RunUntilIdle();
  }

  // Sets the |PasswordForm|s which are retrieve from the |PasswordStore|.
  void SetGetLoginByPasswordConsumerInvocation(
      std::vector<PasswordForm> password_forms) {
    EXPECT_CALL(*store_, GetAutofillableLogins)
        .WillOnce(testing::WithArg<0>([password_forms](
                                          PasswordStoreConsumer* consumer) {
          std::vector<std::unique_ptr<PasswordForm>> results;
          for (auto& form : password_forms)
            results.push_back(std::make_unique<PasswordForm>(std::move(form)));
          consumer->OnGetPasswordStoreResults(std::move(results));
        }));
  }

  // Set the expectation for the |CredentialLeakType| in the callback_.
  void SetOnShowLeakDetectionNotificationExpectation(IsSaved is_saved,
                                                     IsReused is_reused) {
    EXPECT_CALL(callback_, Run(is_saved, is_reused, GURL(kLeakedOrigin),
                               std::u16string(kLeakedUsername)))
        .Times(1);
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  MockCallback<LeakDetectionDelegateHelper::LeakTypeReply> callback_;
  scoped_refptr<MockPasswordStore> store_;
  std::unique_ptr<LeakDetectionDelegateHelper> delegate_helper_;
};

// Credentials are neither saved nor is the password reused.
TEST_F(LeakDetectionDelegateHelperTest, NeitherSaveNotReused) {
  std::vector<PasswordForm> password_forms;

  SetGetLoginByPasswordConsumerInvocation(std::move(password_forms));
  SetOnShowLeakDetectionNotificationExpectation(IsSaved(false),
                                                IsReused(false));
  InitiateGetCredentialLeakType();
}

// Credentials are saved but the password is not reused.
TEST_F(LeakDetectionDelegateHelperTest, SavedLeakedCredentials) {
  std::vector<PasswordForm> password_forms = {
      CreateForm(kLeakedOrigin, kLeakedUsername)};

  SetGetLoginByPasswordConsumerInvocation(std::move(password_forms));
  SetOnShowLeakDetectionNotificationExpectation(IsSaved(true), IsReused(false));
  EXPECT_CALL(*store_, UpdateLogin);
  InitiateGetCredentialLeakType();
}

// Credentials are saved and the password is reused on a different origin.
TEST_F(LeakDetectionDelegateHelperTest,
       SavedCredentialsAndReusedPasswordOnOtherOrigin) {
  std::vector<PasswordForm> password_forms = {
      CreateForm(kLeakedOrigin, kLeakedUsername),
      CreateForm(kOtherOrigin, kLeakedUsername)};

  SetGetLoginByPasswordConsumerInvocation(std::move(password_forms));
  SetOnShowLeakDetectionNotificationExpectation(IsSaved(true), IsReused(true));
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

  SetGetLoginByPasswordConsumerInvocation(std::move(password_forms));
  SetOnShowLeakDetectionNotificationExpectation(IsSaved(true), IsReused(true));
  EXPECT_CALL(*store_, UpdateLogin);
  InitiateGetCredentialLeakType();
}

// Credentials are not saved but the password is reused.
TEST_F(LeakDetectionDelegateHelperTest, ReusedPasswordWithOtherUsername) {
  std::vector<PasswordForm> password_forms = {
      CreateForm(kLeakedOrigin, kOtherUsername)};

  SetGetLoginByPasswordConsumerInvocation(std::move(password_forms));
  SetOnShowLeakDetectionNotificationExpectation(IsSaved(false), IsReused(true));
  InitiateGetCredentialLeakType();
}

// Credentials are not saved but the password is reused on a different origin.
TEST_F(LeakDetectionDelegateHelperTest, ReusedPasswordOnOtherOrigin) {
  std::vector<PasswordForm> password_forms = {
      CreateForm(kOtherOrigin, kLeakedUsername)};

  SetGetLoginByPasswordConsumerInvocation(std::move(password_forms));
  SetOnShowLeakDetectionNotificationExpectation(IsSaved(false), IsReused(true));
  EXPECT_CALL(*store_, UpdateLogin);
  InitiateGetCredentialLeakType();
}

// Credentials are not saved but the password is reused with a different
// username on a different origin.
TEST_F(LeakDetectionDelegateHelperTest, ReusedPassword) {
  std::vector<PasswordForm> password_forms = {
      CreateForm(kOtherOrigin, kOtherUsername)};

  SetGetLoginByPasswordConsumerInvocation(std::move(password_forms));
  SetOnShowLeakDetectionNotificationExpectation(IsSaved(false), IsReused(true));
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
  SetGetLoginByPasswordConsumerInvocation({leaked_origin,
                                           other_origin_same_credential,
                                           leaked_origin_other_username});

  SetOnShowLeakDetectionNotificationExpectation(IsSaved(true), IsReused(true));
  // The expected updated forms should have leaked entries.
  leaked_origin.password_issues.insert_or_assign(
      InsecureType::kLeaked,
      InsecurityMetadata(base::Time::Now(), IsMuted(false)));
  other_origin_same_credential.password_issues.insert_or_assign(
      InsecureType::kLeaked,
      InsecurityMetadata(base::Time::Now(), IsMuted(false)));
  EXPECT_CALL(*store_, UpdateLogin(leaked_origin));
  EXPECT_CALL(*store_, UpdateLogin(other_origin_same_credential));
  InitiateGetCredentialLeakType();
}

// Credential with the same canonicalized username marked as leaked.
TEST_F(LeakDetectionDelegateHelperTest, SaveLeakedCredentialsCanonicalized) {
  PasswordForm non_canonicalized_username = CreateForm(
      kOtherOrigin, kLeakedUsernameNonCanonicalized, kLeakedPassword);
  SetGetLoginByPasswordConsumerInvocation({non_canonicalized_username});
  SetOnShowLeakDetectionNotificationExpectation(IsSaved(false), IsReused(true));

  // The expected updated form should have leaked entries.
  non_canonicalized_username.password_issues.insert_or_assign(
      InsecureType::kLeaked,
      InsecurityMetadata(base::Time::Now(), IsMuted(false)));
  EXPECT_CALL(*store_, UpdateLogin(non_canonicalized_username));
  InitiateGetCredentialLeakType();
}

// Credentials are saved and the password is reused on the same origin &
// username but with a different password.
TEST_F(LeakDetectionDelegateHelperTest,
       SavedCredentialsAndReusedPasswordWithOtherPassword) {
  std::vector<PasswordForm> password_forms = {
      CreateForm(kLeakedOrigin, kLeakedUsername),
      CreateForm(kLeakedOrigin, kLeakedUsername)};
  password_forms.back().password_value = u"another_password";

  SetGetLoginByPasswordConsumerInvocation(password_forms);
  SetOnShowLeakDetectionNotificationExpectation(IsSaved(true), IsReused(true));
  password_forms.at(0).password_issues.insert_or_assign(
      InsecureType::kLeaked,
      InsecurityMetadata(base::Time::Now(), IsMuted(false)));
  EXPECT_CALL(*store_, UpdateLogin(password_forms[0]));
  InitiateGetCredentialLeakType();
}

namespace {
class LeakDetectionDelegateHelperWithTwoStoreTest
    : public LeakDetectionDelegateHelperTest {
 protected:
  void SetUp() override {
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
  PasswordForm profile_store_form = CreateForm(kLeakedOrigin, kLeakedUsername);
  PasswordForm account_store_form = CreateForm(kOtherOrigin, kLeakedUsername);

  profile_store_->AddLogin(profile_store_form);
  account_store_->AddLogin(account_store_form);

  SetOnShowLeakDetectionNotificationExpectation(IsSaved(true), IsReused(true));

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
