// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/leak_detection_delegate_helper.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/password_manager/core/browser/form_parsing/form_parser.h"
#include "components/password_manager/core/browser/leak_detection_dialog_utils.h"
#include "components/password_manager/core/browser/mock_password_store_interface.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_scripts_fetcher.h"
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
constexpr char16_t kOtherPassword[] = u"other_password";
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

  // Set the expectation for the |CredentialLeakType| in the callback_.
  void SetOnShowLeakDetectionNotificationExpectation(
      IsSaved is_saved,
      IsReused is_reused,
      HasChangeScript has_change_script,
      std::vector<GURL> all_urls_with_leaked_credentials = {}) {
    EXPECT_CALL(
        callback_,
        Run(is_saved, is_reused, has_change_script, GURL(kLeakedOrigin),
            std::u16string(kLeakedUsername), all_urls_with_leaked_credentials))
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
        store_, /*account_store=*/nullptr, /*scripts_fetcher=*/nullptr,
        callback_.Get());
  }

  void TearDown() override {
    store_ = nullptr;
  }

  // Sets the |PasswordForm|s which are retrieve from the |PasswordStore|.
  void SetGetAutofillableLoginsConsumerInvocation(
      std::vector<PasswordForm> password_forms) {
    EXPECT_CALL(*store_, GetAutofillableLogins)
        .WillOnce(testing::WithArg<0>(
            [password_forms, store = store_.get()](
                base::WeakPtr<PasswordStoreConsumer> consumer) {
              std::vector<std::unique_ptr<PasswordForm>> results;
              for (auto& form : password_forms) {
                results.push_back(
                    std::make_unique<PasswordForm>(std::move(form)));
              }
              consumer->OnGetPasswordStoreResultsOrErrorFrom(
                  store, std::move(results));
            }));
  }

  scoped_refptr<MockPasswordStoreInterface> store_;
};

// Credentials are neither saved nor is the password reused.
TEST_F(LeakDetectionDelegateHelperTest, NeitherSaveNotReused) {
  std::vector<PasswordForm> password_forms = {
      CreateForm(kOtherOrigin, kOtherUsername, kOtherPassword)};

  SetGetAutofillableLoginsConsumerInvocation(std::move(password_forms));
  SetOnShowLeakDetectionNotificationExpectation(IsSaved(false), IsReused(false),
                                                HasChangeScript(false));
  InitiateGetCredentialLeakType();
}

// Credentials are saved but the password is not reused.
TEST_F(LeakDetectionDelegateHelperTest, SavedLeakedCredentials) {
  std::vector<PasswordForm> password_forms = {
      CreateForm(kLeakedOrigin, kLeakedUsername)};

  SetGetAutofillableLoginsConsumerInvocation(std::move(password_forms));
  SetOnShowLeakDetectionNotificationExpectation(IsSaved(true), IsReused(false),
                                                HasChangeScript(false),
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
      IsSaved(true), IsReused(true), HasChangeScript(false),
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
  SetOnShowLeakDetectionNotificationExpectation(IsSaved(true), IsReused(true),
                                                HasChangeScript(false),
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
  SetOnShowLeakDetectionNotificationExpectation(IsSaved(false), IsReused(true),
                                                HasChangeScript(false));
  InitiateGetCredentialLeakType();
}

// Credentials are not saved but the password is reused on a different origin.
TEST_F(LeakDetectionDelegateHelperTest, ReusedPasswordOnOtherOrigin) {
  std::vector<PasswordForm> password_forms = {
      CreateForm(kOtherOrigin, kLeakedUsername)};

  SetGetAutofillableLoginsConsumerInvocation(std::move(password_forms));
  SetOnShowLeakDetectionNotificationExpectation(IsSaved(false), IsReused(true),
                                                HasChangeScript(false),
                                                {GURL(kOtherOrigin)});
  EXPECT_CALL(*store_, UpdateLogin);
  InitiateGetCredentialLeakType();
}

// Credentials are not saved but the password is reused with a different
// username on a different origin.
TEST_F(LeakDetectionDelegateHelperTest, ReusedPassword) {
  std::vector<PasswordForm> password_forms = {
      CreateForm(kOtherOrigin, kOtherUsername)};

  SetGetAutofillableLoginsConsumerInvocation(std::move(password_forms));
  SetOnShowLeakDetectionNotificationExpectation(IsSaved(false), IsReused(true),
                                                HasChangeScript(false));
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
      IsSaved(true), IsReused(true), HasChangeScript(false),
      {GURL(kLeakedOrigin), GURL(kOtherOrigin)});
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
  SetGetAutofillableLoginsConsumerInvocation({non_canonicalized_username});
  SetOnShowLeakDetectionNotificationExpectation(IsSaved(false), IsReused(true),
                                                HasChangeScript(false),
                                                {GURL(kOtherOrigin)});

  // The expected updated form should have leaked entries.
  non_canonicalized_username.password_issues.insert_or_assign(
      InsecureType::kLeaked,
      InsecurityMetadata(base::Time::Now(), IsMuted(false)));
  EXPECT_CALL(*store_, UpdateLogin(non_canonicalized_username));
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
        profile_store_, account_store_, /*scripts_fetcher=*/nullptr,
        callback_.Get());
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
      IsSaved(true), IsReused(true), HasChangeScript(false),
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

class FakePasswordScriptsFetcher : public PasswordScriptsFetcher {
 public:
  void PrewarmCache() override {}
  void RefreshScriptsIfNecessary(
      base::OnceClosure fetch_finished_callback) override {}

  void FetchScriptAvailability(const url::Origin& origin,
                               ResponseCallback callback) override {
    callback_ = std::move(callback);
  }

  bool IsScriptAvailable(const url::Origin& origin) const override {
    // The synchronous script availability check isn't used by
    // LeakDetectionDelegateHelper.
    NOTREACHED();
    return false;
  }

  bool HasScriptAvailabilityCallback() const { return !callback_.is_null(); }

  void RunScriptAvailabilityCallback(bool is_script_available) {
    std::move(callback_).Run(is_script_available);
  }

  bool IsCacheStale() const override { return true; }

  base::Value::Dict GetDebugInformationForInternals() const override {
    return base::Value::Dict();
  }

  base::Value::List GetCacheEntries() const override {
    return base::Value::List();
  }

 private:
  ResponseCallback callback_;
};

class LeakDetectionDelegateHelperWithScriptsFetcherTest
    : public testing::Test,
      public LeakDetectionDelegateHelperTestBase {
 protected:
  void SetUp() override {
    profile_store_->Init(/*prefs=*/nullptr,
                         /*affiliated_match_helper=*/nullptr);

    delegate_helper_ = std::make_unique<LeakDetectionDelegateHelper>(
        profile_store_, /*account_store=*/nullptr, &scripts_fetcher_,
        callback_.Get());
  }

  void TearDown() override {
    delegate_helper_.reset();
    profile_store_->ShutdownOnUIThread();
    profile_store_ = nullptr;
    task_environment_.RunUntilIdle();
  }

  scoped_refptr<TestPasswordStore> profile_store_ =
      base::MakeRefCounted<TestPasswordStore>(IsAccountStore(false));
  FakePasswordScriptsFetcher scripts_fetcher_;
};

TEST_F(LeakDetectionDelegateHelperWithScriptsFetcherTest,
       NoPasswordChangeScriptAvailable) {
  profile_store_->AddLogin(CreateForm(kLeakedOrigin, kLeakedUsername));

  InitiateGetCredentialLeakType();
  ASSERT_TRUE(scripts_fetcher_.HasScriptAvailabilityCallback());

  // The result should only be available once the script fetcher finishes.
  SetOnShowLeakDetectionNotificationExpectation(IsSaved(true), IsReused(false),
                                                HasChangeScript(false),
                                                {GURL(kLeakedOrigin)});
  scripts_fetcher_.RunScriptAvailabilityCallback(/*is_script_available=*/false);
}

TEST_F(LeakDetectionDelegateHelperWithScriptsFetcherTest,
       PasswordChangeScriptAvailable) {
  profile_store_->AddLogin(CreateForm(kLeakedOrigin, kLeakedUsername));

  InitiateGetCredentialLeakType();
  ASSERT_TRUE(scripts_fetcher_.HasScriptAvailabilityCallback());

  // The result should only be available once the script fetcher finishes.
  SetOnShowLeakDetectionNotificationExpectation(IsSaved(true), IsReused(false),
                                                HasChangeScript(true),
                                                {GURL(kLeakedOrigin)});
  scripts_fetcher_.RunScriptAvailabilityCallback(/*is_script_available=*/true);
}

}  // namespace password_manager
