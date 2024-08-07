// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store/password_store.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_move_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/affiliations/core/browser/fake_affiliation_service.h"
#include "components/autofill/core/common/signatures.h"
#include "components/os_crypt/sync/os_crypt_mocker.h"
#include "components/password_manager/core/browser/affiliation/mock_affiliated_match_helper.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/form_parsing/form_data_parser.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_store/fake_password_store_backend.h"
#include "components/password_manager/core/browser/password_store/login_database.h"
#include "components/password_manager/core/browser/password_store/mock_password_store_backend.h"
#include "components/password_manager/core/browser/password_store/mock_password_store_consumer.h"
#include "components/password_manager/core/browser/password_store/password_store_backend.h"
#include "components/password_manager/core/browser/password_store/password_store_built_in_backend.h"
#include "components/password_manager/core/browser/password_store/password_store_consumer.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync/model/data_type_controller_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::WaitableEvent;
using testing::_;
using testing::DoAll;
using testing::ElementsAre;
using testing::ElementsAreArray;
using testing::Eq;
using testing::Invoke;
using testing::IsEmpty;
using testing::Pointee;
using testing::SizeIs;
using testing::UnorderedElementsAre;
using testing::UnorderedElementsAreArray;
using testing::VariantWith;
using testing::WithArg;

namespace password_manager {

namespace {

constexpr const char kTestAffiliatedRealm[] = "https://one.example/";
constexpr const char kTestAffiliatedURL[] = "https://one.example/path";
constexpr const char kTestAffiliatedPSLWebRealm[] = "https://two.example/";
constexpr const char kTestAffiliatedPSLWebURL[] = "https://two.example/path";
constexpr const char kTestGroupRealm[] = "https://one-good.example/";
constexpr const char kTestGroupURL[] = "https://one-good.example/path";
constexpr const char kTestWebRealm1[] = "https://one.example.com/";
constexpr const char kTestWebOrigin1[] = "https://one.example.com/origin";
constexpr const char kTestWebRealm2[] = "https://two.example.com/";
constexpr const char kTestWebOrigin2[] = "https://two.example.com/origin";
constexpr const char kTestWebRealm3[] = "https://three.example.com/";
constexpr const char kTestWebOrigin3[] = "https://three.example.com/origin";
constexpr const char kTestPSLMatchingWebRealm[] = "https://psl.example.com/";
constexpr const char kTestPSLMatchingWebOrigin[] =
    "https://psl.example.com/origin";
constexpr const char kTestUnrelatedWebRealm[] = "https://notexample.com/";
constexpr const char kTestUnrelatedWebOrigin[] = "https:/notexample.com/origin";
constexpr const char kTestUnrelatedWebRealm2[] = "https://notexample2.com/";
constexpr const char kTestUnrelatedWebOrigin2[] =
    "https:/notexample2.com/origin";
constexpr const char kTestAndroidRealm1[] =
    "android://hash@com.example.android/";
constexpr const char kTestAndroidRealm2[] =
    "android://hash@com.example.two.android/";
constexpr const char kTestAndroidRealm3[] =
    "android://hash@com.example.three.android/";
constexpr const char kTestUnrelatedAndroidRealm[] =
    "android://hash@com.notexample.android/";
constexpr const char kTestAndroidName1[] = "Example Android App 1";
constexpr const char kTestAndroidIconURL1[] = "https://example.com/icon_1.png";
constexpr const char kTestAndroidName2[] = "Example Android App 2";
constexpr const char kTestAndroidIconURL2[] = "https://example.com/icon_2.png";
constexpr const time_t kTestLastUsageTime = 1546300800;  // 00:00 Jan 1 2019 UTC

const PasswordStoreBackendError kBackendError =
    PasswordStoreBackendError(PasswordStoreBackendErrorType::kUncategorized);

PasswordForm MakePasswordForm(const std::string& signon_realm) {
  PasswordForm form;
  form.url = GURL("http://www.origin.com");
  form.username_element = u"username_element";
  form.username_value = u"username_value";
  form.password_element = u"password_element";
  form.signon_realm = signon_realm;
  return form;
}

bool MatchesOrigin(const GURL& origin, const GURL& url) {
  return origin.DeprecatedGetOriginAsURL() == url.DeprecatedGetOriginAsURL();
}

std::tuple<scoped_refptr<PasswordStore>, MockPasswordStoreBackend*>
CreateUnownedStoreWithOwnedMockBackend() {
  auto backend = std::make_unique<MockPasswordStoreBackend>();
  MockPasswordStoreBackend* mock_backend = backend.get();
  return std::make_tuple(
      base::MakeRefCounted<PasswordStore>(std::move(backend)), mock_backend);
}

PasswordFormData CreateTestPasswordFormDataByOrigin(const char* origin_url) {
  PasswordFormData data = {PasswordForm::Scheme::kHtml,
                           origin_url,
                           origin_url,
                           "login_element",
                           u"submit_element",
                           u"username_element",
                           u"password_element",
                           u"username_value",
                           u"password_value",
                           true,
                           1};
  return data;
}

PasswordStoreChangeList CreateChangeList(PasswordStoreChange::Type type,
                                         PasswordForm form) {
  PasswordStoreChangeList changes;
  changes.emplace_back(type, std::move(form));
  return changes;
}

auto HasChangeType(PasswordStoreChange::Type type) {
  return testing::Property(&PasswordStoreChange::type, Eq(type));
}

auto HasForm(const PasswordForm& form) {
  return testing::Property(&PasswordStoreChange::form, Eq(form));
}

auto EqChange(PasswordStoreChange::Type type, const PasswordForm& form) {
  return AllOf(HasChangeType(type), HasForm(form));
}

auto EqRemoval(const PasswordForm& form) {
  return EqChange(PasswordStoreChange::REMOVE, form);
}

auto EqAddition(const PasswordForm& form) {
  return EqChange(PasswordStoreChange::ADD, form);
}

auto EqUpdate(const PasswordForm& form) {
  return EqChange(PasswordStoreChange::UPDATE, form);
}

}  // namespace

class PasswordStoreTest : public testing::Test {
 public:
  PasswordStoreTest(const PasswordStoreTest&) = delete;
  PasswordStoreTest& operator=(const PasswordStoreTest&) = delete;

 protected:
  PasswordStoreTest() = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    // Mock OSCrypt. There is a call to OSCrypt on initializling
    // PasswordReuseDetector, so it should be mocked.
    OSCryptMocker::SetUp();

    feature_list_.InitWithFeatures({features::kPasswordReuseDetectionEnabled},
                                   {});
    pref_service_.registry()->RegisterBooleanPref(
        password_manager::prefs::kWereOldGoogleLoginsRemoved, false);
#if !BUILDFLAG(IS_ANDROID)
    pref_service_.registry()->RegisterBooleanPref(
        prefs::kClearingUndecryptablePasswords, false);
#endif
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_IOS)
    pref_service_.registry()->RegisterBooleanPref(
        prefs::kDeletingUndecryptablePasswordsEnabled, true);
#endif
  }

  void TearDown() override { OSCryptMocker::TearDown(); }

  void WaitForPasswordStore() { task_environment_.RunUntilIdle(); }

  base::FilePath test_login_db_file_path() const {
    return temp_dir_.GetPath().Append(FILE_PATH_LITERAL("login_test"));
  }

  scoped_refptr<PasswordStore> CreatePasswordStore() {
    return new PasswordStore(std::make_unique<PasswordStoreBuiltInBackend>(
        std::make_unique<LoginDatabase>(
            test_login_db_file_path(), password_manager::IsAccountStore(false)),
        syncer::WipeModelUponSyncDisabledBehavior::kNever, &pref_service_));
  }

  TestingPrefServiceSimple* pref_service() { return &pref_service_; }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

 protected:
  base::test::ScopedFeatureList feature_list_;

 private:
  base::ScopedTempDir temp_dir_;
  TestingPrefServiceSimple pref_service_;
};

std::optional<PasswordHashData> GetPasswordFromPref(const std::string& username,
                                                    bool is_gaia_password,
                                                    PrefService* prefs) {
  HashPasswordManager hash_password_manager;
  hash_password_manager.set_prefs(prefs);

  return hash_password_manager.RetrievePasswordHash(username, is_gaia_password);
}

TEST_F(PasswordStoreTest, UpdateLoginPrimaryKeyFields) {
  base::HistogramTester histogram_tester;
  /* clang-format off */
  static const PasswordFormData kTestCredentials[] = {
      // The old credential.
      {PasswordForm::Scheme::kHtml,
       kTestWebRealm1,
       kTestWebOrigin1,
       "", u"", u"username_element_1",  u"password_element_1",
       u"username_value_1",
       u"", kTestLastUsageTime, 1},
      // The new credential with different values for all primary key fields.
      {PasswordForm::Scheme::kHtml,
       kTestWebRealm2,
       kTestWebOrigin2,
       "", u"", u"username_element_2",  u"password_element_2",
       u"username_value_2",
       u"", kTestLastUsageTime, 1}};
  /* clang-format on */

  scoped_refptr<PasswordStore> store = CreatePasswordStore();
  store->Init(/*prefs=*/nullptr, /*affiliated_match_helper=*/nullptr);

  std::unique_ptr<PasswordForm> old_form(FillPasswordFormWithData(
      kTestCredentials[0], /*is_account_store=*/false));
  old_form->password_issues = {
      {InsecureType::kLeaked,
       InsecurityMetadata(base::Time(), IsMuted(false),
                          TriggerBackendNotification(false))}};
  store->AddLogin(*old_form);
  WaitForPasswordStore();

  MockPasswordStoreObserver mock_observer;
  store->AddObserver(&mock_observer);

  std::unique_ptr<PasswordForm> new_form(FillPasswordFormWithData(
      kTestCredentials[1], /*is_account_store=*/false));
  new_form->password_issues = old_form->password_issues;
  EXPECT_CALL(mock_observer, OnLoginsChanged(_, testing::SizeIs(2u)));
  PasswordForm old_primary_key;
  old_primary_key.signon_realm = old_form->signon_realm;
  old_primary_key.url = old_form->url;
  old_primary_key.username_element = old_form->username_element;
  old_primary_key.username_value = old_form->username_value;
  old_primary_key.password_element = old_form->password_element;
  store->UpdateLoginWithPrimaryKey(*new_form, old_primary_key);
  WaitForPasswordStore();
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.PasswordStore.BuiltInBackend.AddLoginCalledOnStore",
      true, 2);
  testing::Mock::VerifyAndClearExpectations(&mock_observer);

  MockPasswordStoreConsumer mock_consumer;
  PasswordForm expected_form(*new_form);
  // The expected form should have no password_issues.
  expected_form.password_issues =
      base::flat_map<InsecureType, InsecurityMetadata>();
  EXPECT_CALL(mock_consumer,
              OnGetPasswordStoreResultsOrErrorFrom(
                  store.get(), VariantWith<LoginsResult>(ElementsAre(
                                   HasPrimaryKeyAndEquals(expected_form)))));
  store->GetAutofillableLogins(mock_consumer.GetWeakPtr());
  WaitForPasswordStore();

  store->RemoveObserver(&mock_observer);
  store->ShutdownOnUIThread();
}

TEST_F(PasswordStoreTest, AddLogins) {
  base::HistogramTester histogram_tester;
  std::vector<PasswordForm> all_credentials;
  all_credentials.push_back(*FillPasswordFormWithData(
      CreateTestPasswordFormDataByOrigin(kTestWebRealm1),
      /*is_account_store=*/false));
  all_credentials.push_back(*FillPasswordFormWithData(
      CreateTestPasswordFormDataByOrigin(kTestAndroidRealm1),
      /*is_account_store=*/false));

  scoped_refptr<PasswordStore> store = CreatePasswordStore();
  store->Init(/*prefs=*/nullptr, /*affiliated_match_helper=*/nullptr);

  MockPasswordStoreObserver mock_observer;
  store->AddObserver(&mock_observer);

  EXPECT_CALL(mock_observer, OnLoginsChanged(_, testing::SizeIs(2u)));
  store->AddLogins({all_credentials[0], all_credentials[1]});
  WaitForPasswordStore();
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.PasswordStore.BuiltInBackend.AddLoginCalledOnStore",
      true, all_credentials.size());

  testing::Mock::VerifyAndClearExpectations(&mock_observer);

  MockPasswordStoreConsumer mock_consumer;

  EXPECT_CALL(
      mock_consumer,
      OnGetPasswordStoreResultsOrErrorFrom(
          store.get(), VariantWith<LoginsResult>(UnorderedElementsAreArray(
                           FormsIgnoringPrimaryKey(all_credentials)))));
  store->GetAutofillableLogins(mock_consumer.GetWeakPtr());
  WaitForPasswordStore();

  store->RemoveObserver(&mock_observer);
  store->ShutdownOnUIThread();
}

TEST_F(PasswordStoreTest, UpdateLogins) {
  base::HistogramTester histogram_tester;

  PasswordFormData form_data_1 =
      CreateTestPasswordFormDataByOrigin(kTestWebRealm1);
  PasswordFormData form_data_2 =
      CreateTestPasswordFormDataByOrigin(kTestAndroidRealm1);
  std::vector<PasswordForm> all_credentials = {
      *FillPasswordFormWithData(form_data_1, /*is_account_store=*/false),
      *FillPasswordFormWithData(form_data_2, /*is_account_store=*/false)};

  scoped_refptr<PasswordStore> store = CreatePasswordStore();
  store->Init(/*prefs=*/nullptr, /*affiliated_match_helper=*/nullptr);

  store->AddLogins(all_credentials);

  WaitForPasswordStore();
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.PasswordStore.BuiltInBackend.AddLoginCalledOnStore",
      true, all_credentials.size());

  form_data_1.password_value = u"new_password1";
  form_data_2.password_value = u"new_password2";

  std::unique_ptr<PasswordForm> updated_form_1 =
      FillPasswordFormWithData(form_data_1, /*is_account_store=*/false);
  std::unique_ptr<PasswordForm> updated_form_2 =
      FillPasswordFormWithData(form_data_2, /*is_account_store=*/false);

  std::vector<PasswordForm> updated_credentials = {*updated_form_1,
                                                   *updated_form_2};

  MockPasswordStoreObserver mock_observer;
  store->AddObserver(&mock_observer);

  EXPECT_CALL(mock_observer, OnLoginsChanged(_, testing::SizeIs(2u)));
  store->UpdateLogins(updated_credentials);
  WaitForPasswordStore();
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.PasswordStore.BuiltInBackend.UpdateLoginCalledOnStore",
      true, all_credentials.size());

  testing::Mock::VerifyAndClearExpectations(&mock_observer);

  MockPasswordStoreConsumer mock_consumer;

  EXPECT_CALL(
      mock_consumer,
      OnGetPasswordStoreResultsOrErrorFrom(
          store.get(), VariantWith<LoginsResult>(UnorderedElementsAreArray(
                           FormsIgnoringPrimaryKey(updated_credentials)))));
  store->GetAutofillableLogins(mock_consumer.GetWeakPtr());
  WaitForPasswordStore();

  store->RemoveObserver(&mock_observer);
  store->ShutdownOnUIThread();
}

// Verify that RemoveLoginsCreatedBetween() fires the completion callback after
// deletions have been performed and notifications have been sent out. Whether
// the correct logins are removed or not is verified in detail in other tests.
TEST_F(PasswordStoreTest, RemoveLoginsCreatedBetweenCallbackIsCalled) {
  /* clang-format off */
  static const PasswordFormData kTestCredential =
      {PasswordForm::Scheme::kHtml,
       kTestWebRealm1,
       kTestWebOrigin1,
       "", u"", u"username_element_1",  u"password_element_1",
       u"username_value_1",
       u"", kTestLastUsageTime, 1};
  /* clang-format on */

  scoped_refptr<PasswordStore> store = CreatePasswordStore();
  store->Init(/*prefs=*/nullptr, /*affiliated_match_helper=*/nullptr);

  std::unique_ptr<PasswordForm> test_form(
      FillPasswordFormWithData(kTestCredential, /*is_account_store=*/false));
  store->AddLogin(*test_form);
  WaitForPasswordStore();

  MockPasswordStoreObserver mock_observer;
  store->AddObserver(&mock_observer);

  EXPECT_CALL(mock_observer, OnLoginsChanged(_, testing::SizeIs(1u)));
  base::test::TestFuture<bool> completion_future;

  store->RemoveLoginsCreatedBetween(FROM_HERE,
                                    base::Time::FromSecondsSinceUnixEpoch(0),
                                    base::Time::FromSecondsSinceUnixEpoch(2),
                                    completion_future.GetCallback());
  EXPECT_TRUE(completion_future.Take());
  testing::Mock::VerifyAndClearExpectations(&mock_observer);

  store->RemoveObserver(&mock_observer);
  store->ShutdownOnUIThread();
}

TEST_F(PasswordStoreTest,
       RemoveLoginsCreatedBetweenCompletedSuccessfullyWithEmtpyListOfChanges) {
  auto [store, mock_backend] = CreateUnownedStoreWithOwnedMockBackend();
  EXPECT_CALL(*mock_backend, InitBackend)
      .WillOnce(
          WithArg<3>(Invoke([](base::OnceCallback<void(bool)> completion) {
            std::move(completion).Run(true);
          })));
  store->Init(/*prefs=*/nullptr, /*affiliated_match_helper=*/nullptr);

  base::test::TestFuture<bool> completion_future;
  EXPECT_CALL(*mock_backend, RemoveLoginsCreatedBetweenAsync)
      .WillOnce(WithArg<3>(Invoke([](PasswordChangesOrErrorReply reply) {
        std::move(reply).Run(PasswordStoreChangeList());
      })));
  store->RemoveLoginsCreatedBetween(FROM_HERE,
                                    base::Time::FromSecondsSinceUnixEpoch(0),
                                    base::Time::FromSecondsSinceUnixEpoch(2),
                                    completion_future.GetCallback());
  EXPECT_TRUE(completion_future.Take());

  store->ShutdownOnUIThread();
}

TEST_F(PasswordStoreTest,
       RemoveLoginsCreatedBetweenCompletionFailedWithBackendError) {
  auto [store, mock_backend] = CreateUnownedStoreWithOwnedMockBackend();
  EXPECT_CALL(*mock_backend, InitBackend)
      .WillOnce(
          WithArg<3>(Invoke([](base::OnceCallback<void(bool)> completion) {
            std::move(completion).Run(true);
          })));
  store->Init(/*prefs=*/nullptr, /*affiliated_match_helper=*/nullptr);

  base::test::TestFuture<bool> completion_future;
  EXPECT_CALL(*mock_backend, RemoveLoginsCreatedBetweenAsync)
      .WillOnce(
          WithArg<3>(Invoke([](PasswordChangesOrErrorReply reply) -> void {
            std::move(reply).Run(kBackendError);
          })));
  store->RemoveLoginsCreatedBetween(FROM_HERE,
                                    base::Time::FromSecondsSinceUnixEpoch(0),
                                    base::Time::FromSecondsSinceUnixEpoch(2),
                                    completion_future.GetCallback());
  EXPECT_FALSE(completion_future.Take());

  store->ShutdownOnUIThread();
}

// Verify that when a login password is updated that the corresponding row is
// removed from the insecure credentials table.
TEST_F(PasswordStoreTest, InsecureCredentialsObserverOnLoginUpdated) {
  scoped_refptr<PasswordStore> store = CreatePasswordStore();
  store->Init(/*prefs=*/nullptr, /*affiliated_match_helper=*/nullptr);

  /* clang-format off */
  PasswordFormData kTestCredential =
      {PasswordForm::Scheme::kHtml,
       kTestWebRealm1,
       kTestWebOrigin1,
       "", u"", u"username_element_1",  u"password_element_1",
       u"username_value_1",
       u"password_value_1", kTestLastUsageTime, 1};
  /* clang-format on */

  std::unique_ptr<PasswordForm> test_form(
      FillPasswordFormWithData(kTestCredential, /*is_account_store=*/false));
  test_form->password_issues = {
      {InsecureType::kLeaked,
       InsecurityMetadata(base::Time::FromTimeT(1), IsMuted(false),
                          TriggerBackendNotification(false))}};
  store->AddLogin(*test_form);
  WaitForPasswordStore();

  kTestCredential.password_value = u"password_value_2";
  PasswordForm test_form_2(
      *FillPasswordFormWithData(kTestCredential, /*is_account_store=*/false));
  store->UpdateLogin(test_form_2);
  WaitForPasswordStore();

  MockPasswordStoreConsumer mock_consumer;
  EXPECT_CALL(mock_consumer,
              OnGetPasswordStoreResultsOrErrorFrom(
                  store.get(), VariantWith<LoginsResult>(ElementsAre(
                                   HasPrimaryKeyAndEquals(test_form_2)))));
  store->GetAllLogins(mock_consumer.GetWeakPtr());
  WaitForPasswordStore();

  store->ShutdownOnUIThread();
}

// Verify that when a login password is added with the password changed
// the insecure credentials associated with it are cleared.
TEST_F(PasswordStoreTest, InsecureCredentialsObserverOnLoginAdded) {
  scoped_refptr<PasswordStore> store = CreatePasswordStore();
  store->Init(/*prefs=*/nullptr, /*affiliated_match_helper=*/nullptr);

  /* clang-format off */
  PasswordFormData kTestCredential =
      {PasswordForm::Scheme::kHtml,
       kTestWebRealm1,
       kTestWebOrigin1,
       "", u"", u"username_element_1",  u"password_element_1",
       u"username_value_1",
       u"password_value_1", kTestLastUsageTime, 1};
  /* clang-format on */

  std::unique_ptr<PasswordForm> test_form(
      FillPasswordFormWithData(kTestCredential, /*is_account_store=*/false));
  test_form->password_issues = {
      {InsecureType::kLeaked,
       InsecurityMetadata(base::Time::FromTimeT(1), IsMuted(false),
                          TriggerBackendNotification(false))}};
  store->AddLogin(*test_form);
  WaitForPasswordStore();

  kTestCredential.password_value = u"password_value_2";
  std::unique_ptr<PasswordForm> test_form_2(
      FillPasswordFormWithData(kTestCredential, /*is_account_store=*/false));
  store->AddLogin(*test_form_2);
  WaitForPasswordStore();

  MockPasswordStoreConsumer mock_consumer;
  EXPECT_CALL(mock_consumer,
              OnGetPasswordStoreResultsOrErrorFrom(
                  store.get(), VariantWith<LoginsResult>(ElementsAre(
                                   HasPrimaryKeyAndEquals(*test_form_2)))));
  store->GetAllLogins(mock_consumer.GetWeakPtr());
  WaitForPasswordStore();

  store->ShutdownOnUIThread();
}

TEST_F(PasswordStoreTest, InsecurePasswordObserverOnInsecureCredentialAdded) {
  constexpr PasswordFormData kTestCredentials = {PasswordForm::Scheme::kHtml,
                                                 kTestWebRealm1,
                                                 kTestWebRealm1,
                                                 "",
                                                 u"",
                                                 u"",
                                                 u"",
                                                 u"username_value_1",
                                                 u"password",
                                                 kTestLastUsageTime,
                                                 1};
  scoped_refptr<PasswordStore> store = CreatePasswordStore();
  store->Init(/*prefs=*/nullptr, /*affiliated_match_helper=*/nullptr);
  std::unique_ptr<PasswordForm> test_form(
      FillPasswordFormWithData(kTestCredentials, /*is_account_store=*/false));
  store->AddLogin(*test_form);
  WaitForPasswordStore();

  MockPasswordStoreObserver mock_observer;
  store->AddObserver(&mock_observer);

  // Expect a notification after adding a credential.
  EXPECT_CALL(mock_observer, OnLoginsChanged);
  test_form->password_issues = {
      {InsecureType::kLeaked,
       InsecurityMetadata(base::Time::FromTimeT(1), IsMuted(false),
                          TriggerBackendNotification(false))}};
  store->UpdateLogin(*test_form);

  WaitForPasswordStore();

  store->RemoveObserver(&mock_observer);
  store->ShutdownOnUIThread();
}

TEST_F(PasswordStoreTest, InsecurePasswordObserverOnInsecureCredentialRemoved) {
  constexpr PasswordFormData kTestCredentials = {PasswordForm::Scheme::kHtml,
                                                 kTestWebRealm1,
                                                 kTestWebRealm1,
                                                 "",
                                                 u"",
                                                 u"",
                                                 u"",
                                                 u"username_value_1",
                                                 u"password",
                                                 kTestLastUsageTime,
                                                 1};
  scoped_refptr<PasswordStore> store = CreatePasswordStore();
  store->Init(/*prefs=*/nullptr, /*affiliated_match_helper=*/nullptr);
  std::unique_ptr<PasswordForm> test_form(
      FillPasswordFormWithData(kTestCredentials, /*is_account_store=*/false));
  test_form->password_issues = {
      {InsecureType::kLeaked,
       InsecurityMetadata(base::Time::FromTimeT(1), IsMuted(false),
                          TriggerBackendNotification(false))}};
  store->AddLogin(*test_form);
  WaitForPasswordStore();

  MockPasswordStoreObserver mock_observer;
  store->AddObserver(&mock_observer);

  // Expect a notification after removing a credential.
  EXPECT_CALL(mock_observer, OnLoginsChanged);
  test_form->password_issues.clear();
  store->UpdateLogin(*test_form);

  WaitForPasswordStore();

  store->RemoveObserver(&mock_observer);
  store->ShutdownOnUIThread();
}

// Makes sure that the PSL forms are included in GetLogins.
TEST_F(PasswordStoreTest, GetLoginsWithPSL) {
  static constexpr const struct {
    PasswordFormData form_data;
    bool use_federated_login;
  } kTestCredentials[] = {
      // Credential that is an exact match of the observed form.
      {
          {PasswordForm::Scheme::kHtml, kTestWebRealm1, kTestWebOrigin1, "",
           u"", u"", u"", u"username_1", u"12345"},
          false,
      },
      // Credential that is a PSL match.
      {
          {PasswordForm::Scheme::kHtml, kTestPSLMatchingWebRealm,
           kTestPSLMatchingWebOrigin, "", u"", u"", u"", u"username_2",
           u"123456"},
          false,
      },
      // Credential that is a federated PSL.
      {
          {PasswordForm::Scheme::kHtml, kTestPSLMatchingWebRealm,
           kTestPSLMatchingWebOrigin, "", u"", u"", u"", u"username_3",
           u"password"},
          true,
      },
      // Credential for unrelated origin.
      {
          {PasswordForm::Scheme::kHtml, kTestUnrelatedWebRealm2,
           kTestUnrelatedWebOrigin2, "", u"", u"", u"", u"username_4",
           u"password2"},
          false,
      }};

  scoped_refptr<PasswordStore> store = CreatePasswordStore();
  store->Init(/*prefs=*/nullptr, /*affiliated_match_helper=*/nullptr);

  std::vector<std::unique_ptr<PasswordForm>> all_credentials;
  for (const auto& i : kTestCredentials) {
    all_credentials.push_back(FillPasswordFormWithData(
        i.form_data, /*is_account_store=*/false, i.use_federated_login));
    store->AddLogin(*all_credentials.back());
  }

  PasswordFormDigest observed_form = {PasswordForm::Scheme::kHtml,
                                      kTestWebRealm1, GURL(kTestWebOrigin1)};

  std::vector<PasswordForm> expected_results;
  expected_results.push_back(*all_credentials[0]);
  expected_results.push_back(*all_credentials[1]);
  expected_results.push_back(*all_credentials[2]);
  expected_results[0].match_type = PasswordForm::MatchType::kExact;
  expected_results[1].match_type = PasswordForm::MatchType::kPSL;
  expected_results[2].match_type = PasswordForm::MatchType::kPSL;

  MockPasswordStoreConsumer mock_consumer;
  EXPECT_CALL(
      mock_consumer,
      OnGetPasswordStoreResultsOrErrorFrom(
          store.get(), VariantWith<LoginsResult>(UnorderedElementsAreArray(
                           FormsIgnoringPrimaryKey(expected_results)))));

  store->GetLogins(observed_form, mock_consumer.GetWeakPtr());
  WaitForPasswordStore();
  store->ShutdownOnUIThread();
}

// Makes sure that the PSL forms are not returned on Google domains.
TEST_F(PasswordStoreTest, GetLoginsPSLDisabled) {
  static constexpr PasswordFormData kTestCredentials[] = {
      // Credential that is an exact match of the observed form.
      {PasswordForm::Scheme::kHtml, "https://accounts.google.com/",
       "https://accounts.google.com/login", "", u"", u"", u"", u"username_1",
       u"12345"},
      // Credential that looks like a PSL match.
      {PasswordForm::Scheme::kHtml, "https://some.other.google.com/",
       "https://some.other.google.com/path", "", u"", u"", u"", u"username_2",
       u"123456"}};

  scoped_refptr<PasswordStore> store = CreatePasswordStore();
  store->Init(/*prefs=*/nullptr, /*affiliated_match_helper=*/nullptr);

  std::vector<std::unique_ptr<PasswordForm>> all_credentials;
  for (const auto& i : kTestCredentials) {
    all_credentials.push_back(PasswordFormFromData(i));
    store->AddLogin(*all_credentials.back());
    all_credentials.back()->in_store = PasswordForm::Store::kProfileStore;
  }

  PasswordFormDigest observed_form = {PasswordForm::Scheme::kHtml,
                                      "https://accounts.google.com/",
                                      GURL("https://accounts.google.com/")};

  MockPasswordStoreConsumer mock_consumer;
  PasswordForm expected_form(*all_credentials[0]);
  expected_form.match_type = PasswordForm::MatchType::kExact;
  EXPECT_CALL(mock_consumer,
              OnGetPasswordStoreResultsOrErrorFrom(
                  store.get(), VariantWith<LoginsResult>(ElementsAre(
                                   HasPrimaryKeyAndEquals(expected_form)))));

  store->GetLogins(observed_form, mock_consumer.GetWeakPtr());
  WaitForPasswordStore();
  store->ShutdownOnUIThread();
}

// When no Android applications are actually affiliated with the realm of the
// observed form, GetLogins() should still return the exact and PSL matching
// results, but not any stored Android credentials.
TEST_F(PasswordStoreTest, GetLoginsWithoutAffiliations) {
  /* clang-format off */
  static const PasswordFormData kTestCredentials[] = {
      // Credential that is an exact match of the observed form.
      {PasswordForm::Scheme::kHtml,
       kTestWebRealm1,
       kTestWebOrigin1,
       "", u"", u"",  u"",
       u"username_value_1",
       u"", kTestLastUsageTime, 1},
      // Credential that is a PSL match of the observed form.
      {PasswordForm::Scheme::kHtml,
       kTestPSLMatchingWebRealm,
       kTestPSLMatchingWebOrigin,
       "", u"", u"",  u"",
       u"username_value_2",
       u"", kTestLastUsageTime, 1},
      // Credential for an unrelated Android application.
      {PasswordForm::Scheme::kHtml,
       kTestUnrelatedAndroidRealm,
       "", "", u"", u"", u"",
       u"username_value_3",
       u"", kTestLastUsageTime, 1}};
  /* clang-format on */

  scoped_refptr<PasswordStore> store = CreatePasswordStore();
  affiliations::FakeAffiliationService fake_affiliation_service;
  auto owning_mock_match_helper =
      std::make_unique<MockAffiliatedMatchHelper>(&fake_affiliation_service);
  MockAffiliatedMatchHelper* mock_affiliated_match_helper =
      owning_mock_match_helper.get();
  store->Init(/*prefs=*/nullptr, std::move(owning_mock_match_helper));

  std::vector<std::unique_ptr<PasswordForm>> all_credentials;
  for (const auto& credential : kTestCredentials) {
    all_credentials.push_back(
        FillPasswordFormWithData(credential, /*is_account_store=*/false));
    store->AddLogin(*all_credentials.back());
  }

  PasswordFormDigest observed_form = {PasswordForm::Scheme::kHtml,
                                      kTestWebRealm1, GURL(kTestWebOrigin1)};

  std::vector<PasswordForm> expected_results;
  expected_results.push_back(*all_credentials[0]);
  expected_results.push_back(*all_credentials[1]);
  for (auto& result : expected_results) {
    if (result.signon_realm != observed_form.signon_realm) {
      result.match_type = PasswordForm::MatchType::kPSL;
    } else {
      result.match_type = PasswordForm::MatchType::kExact;
    }
  }

  std::vector<std::string> no_affiliated_android_realms;
  mock_affiliated_match_helper->ExpectCallToGetAffiliatedAndGrouped(
      observed_form, no_affiliated_android_realms);

  MockPasswordStoreConsumer mock_consumer;
  EXPECT_CALL(
      mock_consumer,
      OnGetPasswordStoreResultsOrErrorFrom(
          store.get(), VariantWith<LoginsResult>(UnorderedElementsAreArray(
                           FormsIgnoringPrimaryKey(expected_results)))));
  store->GetLogins(observed_form, mock_consumer.GetWeakPtr());
  WaitForPasswordStore();
  store->ShutdownOnUIThread();
}

// There are 3 Android applications affiliated with the realm of the observed
// form, with the PasswordStore having credentials for two of these (even two
// credentials for one). GetLogins() should return the exact, and PSL matching
// credentials, and the credentials for these two Android applications, but not
// for the unaffiliated Android application.
TEST_F(PasswordStoreTest, GetLoginsWithAffiliations) {
  static constexpr const struct {
    PasswordFormData form_data;
    bool use_federated_login;
  } kTestCredentials[] = {
      // Credential that is an exact match of the observed form.
      {
          {PasswordForm::Scheme::kHtml, kTestWebRealm1, kTestWebOrigin1, "",
           u"", u"", u"", u"username_value_1", u"", kTestLastUsageTime, 1},
          false,
      },
      // Credential that is a PSL match of the observed form.
      {
          {PasswordForm::Scheme::kHtml, kTestPSLMatchingWebRealm,
           kTestPSLMatchingWebOrigin, "", u"", u"", u"", u"username_value_2",
           u"", true, 1},
          false,
      },
      // Credential for an Android application affiliated with the realm of the
      // observed from.
      {
          {PasswordForm::Scheme::kHtml, kTestAndroidRealm1, "", "", u"", u"",
           u"", u"username_value_3", u"", kTestLastUsageTime, 1},
          false,
      },
      // Second credential for the same Android application.
      {
          {PasswordForm::Scheme::kHtml, kTestAndroidRealm1, "", "", u"", u"",
           u"", u"username_value_3b", u"", kTestLastUsageTime, 1},
          false,
      },
      // Third credential for the same application which is username-only.
      {
          {PasswordForm::Scheme::kUsernameOnly, kTestAndroidRealm1, "", "", u"",
           u"", u"", u"username_value_3c", u"", kTestLastUsageTime, 1},
          false,
      },
      // Credential for another Android application affiliated with the realm
      // of the observed from.
      {
          {PasswordForm::Scheme::kHtml, kTestAndroidRealm2, "", "", u"", u"",
           u"", u"username_value_4", u"", kTestLastUsageTime, 1},
          false,
      },
      // Federated credential for this second Android application.
      {
          {PasswordForm::Scheme::kHtml, kTestAndroidRealm2, "", "", u"", u"",
           u"", u"username_value_4b", u"", kTestLastUsageTime, 1},
          true,
      },
      // Credential for an unrelated Android application.
      {
          {PasswordForm::Scheme::kHtml, kTestUnrelatedAndroidRealm, "", "", u"",
           u"", u"", u"username_value_5", u"", kTestLastUsageTime, 1},
          false,
      }};

  scoped_refptr<PasswordStore> store = CreatePasswordStore();
  affiliations::FakeAffiliationService fake_affiliation_service;
  auto owning_mock_match_helper =
      std::make_unique<MockAffiliatedMatchHelper>(&fake_affiliation_service);
  MockAffiliatedMatchHelper* mock_affiliated_match_helper =
      owning_mock_match_helper.get();
  store->Init(/*prefs=*/nullptr, std::move(owning_mock_match_helper));

  std::vector<std::unique_ptr<PasswordForm>> all_credentials;
  for (const auto& i : kTestCredentials) {
    all_credentials.push_back(FillPasswordFormWithData(
        i.form_data, /*is_account_store=*/false, i.use_federated_login));
    store->AddLogin(*all_credentials.back());
  }

  PasswordFormDigest observed_form = {PasswordForm::Scheme::kHtml,
                                      kTestWebRealm1, GURL(kTestWebOrigin1)};

  std::vector<PasswordForm> expected_results;
  expected_results.push_back(*all_credentials[0]);
  expected_results.push_back(*all_credentials[1]);
  expected_results.push_back(*all_credentials[2]);
  expected_results.push_back(*all_credentials[3]);
  expected_results.push_back(*all_credentials[5]);
  expected_results.push_back(*all_credentials[6]);

  for (auto& result : expected_results) {
    if (result.signon_realm != observed_form.signon_realm) {
      if (affiliations::IsValidAndroidFacetURI(result.signon_realm)) {
        result.match_type = PasswordForm::MatchType::kAffiliated;
      } else {
        result.match_type = PasswordForm::MatchType::kPSL;
      }
    } else {
      result.match_type = PasswordForm::MatchType::kExact;
    }
  }

  std::vector<std::string> affiliated_android_realms;
  affiliated_android_realms.push_back(kTestAndroidRealm1);
  affiliated_android_realms.push_back(kTestAndroidRealm2);
  affiliated_android_realms.push_back(kTestAndroidRealm3);

  mock_affiliated_match_helper->ExpectCallToGetAffiliatedAndGrouped(
      observed_form, affiliated_android_realms);

  MockPasswordStoreConsumer mock_consumer;
  EXPECT_CALL(
      mock_consumer,
      OnGetPasswordStoreResultsOrErrorFrom(
          store.get(), VariantWith<LoginsResult>(UnorderedElementsAreArray(
                           FormsIgnoringPrimaryKey(expected_results)))));

  store->GetLogins(observed_form, mock_consumer.GetWeakPtr());
  WaitForPasswordStore();
  store->ShutdownOnUIThread();
}

TEST_F(PasswordStoreTest, GetLoginsWithBrandingInformationForExactMatch) {
  scoped_refptr<PasswordStore> store = CreatePasswordStore();
  affiliations::FakeAffiliationService fake_affiliation_service;
  auto owning_mock_match_helper =
      std::make_unique<MockAffiliatedMatchHelper>(&fake_affiliation_service);
  MockAffiliatedMatchHelper* mock_affiliated_match_helper =
      owning_mock_match_helper.get();
  store->Init(/*prefs=*/nullptr, std::move(owning_mock_match_helper));

  PasswordFormData form_data = {PasswordForm::Scheme::kHtml,
                                kTestWebRealm1,
                                kTestWebOrigin1,
                                "",
                                u"",
                                u"",
                                u"",
                                u"username_value_1",
                                u"",
                                kTestLastUsageTime,
                                1};
  std::unique_ptr<PasswordForm> credential =
      FillPasswordFormWithData(form_data, /*is_account_store=*/false);
  store->AddLogin(*credential);

  PasswordFormDigest observed_form = {PasswordForm::Scheme::kHtml,
                                      kTestWebRealm1, GURL(kTestWebOrigin1)};

  std::vector<MockAffiliatedMatchHelper::AffiliationAndBrandingInformation>
      affiliation_info_for_results = {
          {kTestWebRealm1, kTestAndroidName1, GURL(kTestAndroidIconURL1)}};
  mock_affiliated_match_helper
      ->ExpectCallToInjectAffiliationAndBrandingInformation(
          std::move(affiliation_info_for_results));

  PasswordForm expected_result(*credential);
  expected_result.match_type = PasswordForm::MatchType::kExact;
  expected_result.affiliated_web_realm = kTestWebRealm1;
  expected_result.app_display_name = kTestAndroidName1;
  expected_result.app_icon_url = GURL(kTestAndroidIconURL1);

  MockPasswordStoreConsumer mock_consumer;
  EXPECT_CALL(mock_consumer,
              OnGetPasswordStoreResultsOrErrorFrom(
                  store.get(), VariantWith<LoginsResult>(ElementsAre(
                                   HasPrimaryKeyAndEquals(expected_result)))));

  store->GetLogins(observed_form, mock_consumer.GetWeakPtr());
  WaitForPasswordStore();
  store->ShutdownOnUIThread();
}

TEST_F(PasswordStoreTest, GetLoginsWithBrandingInformationForAffiliatedLogins) {
  scoped_refptr<PasswordStore> store = CreatePasswordStore();
  affiliations::FakeAffiliationService fake_affiliation_service;
  auto owning_mock_match_helper =
      std::make_unique<MockAffiliatedMatchHelper>(&fake_affiliation_service);
  MockAffiliatedMatchHelper* mock_affiliated_match_helper =
      owning_mock_match_helper.get();
  store->Init(/*prefs=*/nullptr, std::move(owning_mock_match_helper));

  PasswordFormData form_data = {PasswordForm::Scheme::kHtml,
                                kTestAndroidRealm1,
                                "",
                                "",
                                u"",
                                u"",
                                u"",
                                u"username_value_3",
                                u"",
                                kTestLastUsageTime,
                                1};
  PasswordForm credential =
      *FillPasswordFormWithData(form_data, /*is_account_store=*/false);
  store->AddLogin(credential);

  PasswordFormDigest observed_form = {PasswordForm::Scheme::kHtml,
                                      kTestWebRealm1, GURL(kTestWebOrigin1)};

  credential.match_type = PasswordForm::MatchType::kAffiliated;

  mock_affiliated_match_helper->ExpectCallToGetAffiliatedAndGrouped(
      observed_form, {kTestAndroidRealm1});
  std::vector<MockAffiliatedMatchHelper::AffiliationAndBrandingInformation>
      affiliation_info_for_results = {
          {kTestWebRealm1, kTestAndroidName1, GURL(kTestAndroidIconURL1)}};
  mock_affiliated_match_helper
      ->ExpectCallToInjectAffiliationAndBrandingInformation(
          std::move(affiliation_info_for_results));

  credential.affiliated_web_realm = kTestWebRealm1;
  credential.app_display_name = kTestAndroidName1;
  credential.app_icon_url = GURL(kTestAndroidIconURL1);

  MockPasswordStoreConsumer mock_consumer;
  EXPECT_CALL(mock_consumer,
              OnGetPasswordStoreResultsOrErrorFrom(
                  store.get(), VariantWith<LoginsResult>(ElementsAre(
                                   HasPrimaryKeyAndEquals(credential)))));

  store->GetLogins(observed_form, mock_consumer.GetWeakPtr());
  WaitForPasswordStore();
  store->ShutdownOnUIThread();
}

TEST_F(PasswordStoreTest, PasswordManagerTimeSinceInitMetric) {
  base::HistogramTester histogram_tester;
  scoped_refptr<PasswordStore> store = CreatePasswordStore();
  store->Init(/*prefs=*/nullptr, /*affiliated_match_helper=*/nullptr);

  task_environment_.FastForwardBy(base::Seconds(1));

  MockPasswordStoreConsumer mock_consumer;
  store->GetAllLogins(mock_consumer.GetWeakPtr());
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.GetAllLogins.TimeSinceInit", 1000, 1);

  task_environment_.FastForwardBy(base::Seconds(2));
  store->GetAutofillableLogins(mock_consumer.GetWeakPtr());
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.GetAutofillableLogins.TimeSinceInit", 3000, 1);

  task_environment_.FastForwardBy(base::Seconds(3));
  store->GetAllLoginsWithAffiliationAndBrandingInformation(
      mock_consumer.GetWeakPtr());
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.GetAllLoginsWithAffiliationAndBrandingInformation."
      "TimeSinceInit",
      6000, 1);

  WaitForPasswordStore();
  store->ShutdownOnUIThread();
}

// The 'bool' param corresponds to 'use_federated_login' in the test.
class PasswordStoreFederationTest : public PasswordStoreTest,
                                    public testing::WithParamInterface<bool> {};

// Retrieve matching passwords for affiliated, affiliated/PSL-matched,
// PSL-matched, exact matched credentials and make sure the properties are set
// correctly.
TEST_P(PasswordStoreFederationTest, GetLoginsWithWebAffiliations) {
  static const PasswordFormData kTestCredentials[] = {
      // Credential that is an exact match of the observed form.
      {PasswordForm::Scheme::kHtml, kTestWebRealm1, kTestWebOrigin1, "", u"",
       u"", u"", u"username_1", u"12345"},

      // Credential that is a PSL, non affiliated match of the observed form.
      {PasswordForm::Scheme::kHtml, kTestPSLMatchingWebRealm,
       kTestPSLMatchingWebOrigin, "", u"", u"", u"", u"username_2", u"asdf"},

      // Credential that is a PSL and affiliated match of the observed form.
      {PasswordForm::Scheme::kHtml, kTestWebRealm2, kTestWebOrigin2, "", u"",
       u"", u"", u"username_3", u"password"},

      // Credential that is an affiliated match of the observed form.
      {PasswordForm::Scheme::kHtml, kTestAffiliatedRealm, kTestAffiliatedURL,
       "", u"", u"", u"", u"username_4", u"password1"},

      // Credential that is a PSL match of an affiliated form. It should be
      // filtered out.
      {PasswordForm::Scheme::kHtml, kTestAffiliatedPSLWebRealm,
       kTestAffiliatedPSLWebURL, "", u"", u"", u"", u"username_5",
       u"password3"},

      // Credential for unrelated origin.
      {PasswordForm::Scheme::kUsernameOnly, kTestUnrelatedWebRealm2,
       kTestUnrelatedWebOrigin2, "", u"", u"", u"", u"username_6",
       u"password2"}};

  scoped_refptr<PasswordStore> store = CreatePasswordStore();
  affiliations::FakeAffiliationService fake_affiliation_service;
  auto owning_mock_match_helper =
      std::make_unique<MockAffiliatedMatchHelper>(&fake_affiliation_service);
  MockAffiliatedMatchHelper* mock_affiliated_match_helper =
      owning_mock_match_helper.get();
  store->Init(/*prefs=*/nullptr, std::move(owning_mock_match_helper));

  std::vector<std::unique_ptr<PasswordForm>> all_credentials;
  for (const PasswordFormData& i : kTestCredentials) {
    all_credentials.push_back(
        FillPasswordFormWithData(i, /*is_account_store=*/false, GetParam()));
    store->AddLogin(*all_credentials.back());
  }

  PasswordFormDigest observed_form = {PasswordForm::Scheme::kHtml,
                                      kTestWebRealm1, GURL(kTestWebOrigin1)};

  std::vector<PasswordForm> expected_results;
  expected_results.push_back(*all_credentials[0]);
  expected_results.push_back(*all_credentials[1]);
  expected_results.push_back(*all_credentials[2]);
  expected_results.push_back(*all_credentials[3]);

  expected_results[0].match_type = PasswordForm::MatchType::kExact;
  expected_results[1].match_type = PasswordForm::MatchType::kPSL;
  expected_results[2].match_type =
      PasswordForm::MatchType::kAffiliated | PasswordForm::MatchType::kPSL;
  expected_results[3].match_type = PasswordForm::MatchType::kAffiliated;

  // In the production 'kTestWebRealm1' won't be in the list but the code should
  // protect against it.
  std::vector<std::string> affiliated_realms = {kTestWebRealm1, kTestWebRealm2,
                                                kTestAffiliatedRealm};

  mock_affiliated_match_helper->ExpectCallToGetAffiliatedAndGrouped(
      observed_form, affiliated_realms);

  MockPasswordStoreConsumer mock_consumer;
  EXPECT_CALL(
      mock_consumer,
      OnGetPasswordStoreResultsOrErrorFrom(
          store.get(), VariantWith<LoginsResult>(ElementsAreArray(
                           FormsIgnoringPrimaryKey(expected_results)))));

  store->GetLogins(observed_form, mock_consumer.GetWeakPtr());
  WaitForPasswordStore();
  store->ShutdownOnUIThread();
}

INSTANTIATE_TEST_SUITE_P(Federation,
                         PasswordStoreFederationTest,
                         testing::Bool());

class PasswordStoreGroupsTest : public PasswordStoreTest {
  void SetUp() override {
    PasswordStoreTest::SetUp();
    store_ = CreatePasswordStore();
    auto owning_mock_match_helper =
        std::make_unique<MockAffiliatedMatchHelper>(&affiliation_service_);
    mock_affiliated_match_helper_ = owning_mock_match_helper.get();
    store_->Init(/*prefs=*/nullptr, std::move(owning_mock_match_helper));
  }

  void TearDown() override {
    // The store owns the mocked match helper, so null the raw pointer to avoid
    // dangling.
    mock_affiliated_match_helper_ = nullptr;
    store_->ShutdownOnUIThread();
  }

 protected:
  std::vector<std::unique_ptr<PasswordForm>> CreateCredentialsAndAddToStore() {
    static const PasswordFormData kTestCredentials[] = {
        // Credential that is an exact match of the observed form.
        {PasswordForm::Scheme::kHtml, kTestWebRealm1, kTestWebOrigin1, "", u"",
         u"", u"", u"username_1", u"12345"},

        // Credential that is a PSL, non affiliated match of the observed form.
        {PasswordForm::Scheme::kHtml, kTestPSLMatchingWebRealm,
         kTestPSLMatchingWebOrigin, "", u"", u"", u"", u"username_2", u"asdf"},

        // Credential that is a PSL and affiliated match of the observed form.
        {PasswordForm::Scheme::kHtml, kTestWebRealm2, kTestWebOrigin2, "", u"",
         u"", u"", u"username_3", u"password"},

        // Credential that is a group match of the observed form.
        {PasswordForm::Scheme::kHtml, kTestGroupRealm, kTestGroupURL, "", u"",
         u"", u"", u"username_4", u"password1"},

        // Credential that is a PSL match of an affiliated form. It should be
        // filtered out.
        {PasswordForm::Scheme::kHtml, kTestAffiliatedPSLWebRealm,
         kTestAffiliatedPSLWebURL, "", u"", u"", u"", u"username_5",
         u"password3"},

        // Credential for unrelated origin.
        {PasswordForm::Scheme::kUsernameOnly, kTestUnrelatedWebRealm2,
         kTestUnrelatedWebOrigin2, "", u"", u"", u"", u"username_6",
         u"password2"}};
    std::vector<std::unique_ptr<PasswordForm>> credentials;
    for (const auto& i : kTestCredentials) {
      credentials.push_back(FillPasswordFormWithData(
          i, /*is_account_store=*/false, /*use_federated_login=*/false));
      store_->AddLogin(*credentials.back());
    }
    return credentials;
  }

  scoped_refptr<PasswordStore> store_;
  raw_ptr<MockAffiliatedMatchHelper> mock_affiliated_match_helper_ = nullptr;

 private:
  affiliations::FakeAffiliationService affiliation_service_;
};

// Retrieve matching passwords for affiliated groups credentials and make sure
// the properties are set correctly.
TEST_F(PasswordStoreGroupsTest, GetLoginsWithWebGroup) {
  std::vector<std::unique_ptr<PasswordForm>> all_credentials =
      CreateCredentialsAndAddToStore();

  PasswordFormDigest observed_form = {PasswordForm::Scheme::kHtml,
                                      kTestWebRealm1, GURL(kTestWebOrigin1)};

  std::vector<PasswordForm> expected_results;

  // Credential that is an exact match of the observed form.
  expected_results.push_back(*all_credentials[0]);
  expected_results.back().match_type = PasswordForm::MatchType::kExact;
  // Credential that is a PSL, non affiliated match of the observed form.
  expected_results.push_back(*all_credentials[1]);
  expected_results.back().match_type = PasswordForm::MatchType::kPSL;

  // Credential that is a PSL and affiliated match of the observed form.
  expected_results.push_back(*all_credentials[2]);
  expected_results.back().match_type =
      PasswordForm::MatchType::kAffiliated | PasswordForm::MatchType::kPSL;

  // In the production 'kTestWebRealm1' won't be in the list but the code should
  // protect against it.
  std::vector<std::string> affiliated_realms = {kTestWebRealm1, kTestWebRealm2};
  std::vector<std::string> grouped_realms;
  mock_affiliated_match_helper_->ExpectCallToGetAffiliatedAndGrouped(
      observed_form, affiliated_realms, grouped_realms);

  MockPasswordStoreConsumer mock_consumer;
  EXPECT_CALL(
      mock_consumer,
      OnGetPasswordStoreResultsOrErrorFrom(
          store_.get(), VariantWith<LoginsResult>(ElementsAreArray(
                            FormsIgnoringPrimaryKey(expected_results)))));

  store_->GetLogins(observed_form, mock_consumer.GetWeakPtr());
  WaitForPasswordStore();
}

TEST_F(PasswordStoreTest, DelegatesGetAllLoginsToBackend) {
  auto [store, mock_backend] = CreateUnownedStoreWithOwnedMockBackend();
  EXPECT_CALL(*mock_backend, InitBackend)
      .WillOnce(
          WithArg<3>(Invoke([](base::OnceCallback<void(bool)> completion) {
            std::move(completion).Run(true);
          })));
  store->Init(/*prefs=*/nullptr, /*affiliated_match_helper=*/nullptr);

  MockPasswordStoreConsumer mock_consumer;
  EXPECT_CALL(*mock_backend, GetAllLoginsAsync(_));
  store->GetAllLogins(mock_consumer.GetWeakPtr());
  WaitForPasswordStore();
  store->ShutdownOnUIThread();
}

TEST_F(PasswordStoreTest, DelegatesGetAutofillableLoginsToBackend) {
  auto [store, mock_backend] = CreateUnownedStoreWithOwnedMockBackend();
  EXPECT_CALL(*mock_backend, InitBackend)
      .WillOnce(
          WithArg<3>(Invoke([](base::OnceCallback<void(bool)> completion) {
            std::move(completion).Run(true);
          })));
  store->Init(/*prefs=*/nullptr, /*affiliated_match_helper=*/nullptr);

  MockPasswordStoreConsumer mock_consumer;
  EXPECT_CALL(*mock_backend, GetAutofillableLoginsAsync(_));
  store->GetAutofillableLogins(mock_consumer.GetWeakPtr());
  WaitForPasswordStore();
  store->ShutdownOnUIThread();
}

TEST_F(PasswordStoreTest, CallOnLoginsChangedIfRemovalProvidesChanges) {
  const PasswordForm kTestForm = MakePasswordForm(kTestWebRealm1);
  MockPasswordStoreObserver mock_observer;
  auto [store, mock_backend] = CreateUnownedStoreWithOwnedMockBackend();
  EXPECT_CALL(*mock_backend, InitBackend)
      .WillOnce(
          WithArg<3>(Invoke([](base::OnceCallback<void(bool)> completion) {
            std::move(completion).Run(true);
          })));
  store->Init(/*prefs=*/nullptr, /*affiliated_match_helper=*/nullptr);
  store->AddObserver(&mock_observer);

  // Expect that observers receive the removal when the backend invokes the
  // reply with a `PasswordStoreChangeList`.
  EXPECT_CALL(*mock_backend, RemoveLoginAsync(_, Eq(kTestForm), _))
      .WillOnce(
          WithArg<2>(Invoke([&](PasswordChangesOrErrorReply reply) -> void {
            std::move(reply).Run(
                CreateChangeList(PasswordStoreChange::REMOVE, kTestForm));
          })));
  EXPECT_CALL(mock_observer, OnLoginsRetained).Times(0);
  EXPECT_CALL(mock_observer,
              OnLoginsChanged(store.get(), ElementsAre(EqRemoval(kTestForm))));
  store->RemoveLogin(FROM_HERE, kTestForm);
  WaitForPasswordStore();

  store->RemoveObserver(&mock_observer);
  store->ShutdownOnUIThread();
}

TEST_F(PasswordStoreTest, CallOnLoginsChangedIfAdditionProvidesChanges) {
  const PasswordForm kTestForm = MakePasswordForm(kTestWebRealm1);
  MockPasswordStoreObserver mock_observer;
  auto [store, mock_backend] = CreateUnownedStoreWithOwnedMockBackend();
  EXPECT_CALL(*mock_backend, InitBackend)
      .WillOnce(
          WithArg<3>(Invoke([](base::OnceCallback<void(bool)> completion) {
            std::move(completion).Run(true);
          })));
  store->Init(/*prefs=*/nullptr, /*affiliated_match_helper=*/nullptr);
  store->AddObserver(&mock_observer);

  // Expect that observers receive the addition when the backend invokes the
  // reply with a `PasswordStoreChangeList`.
  EXPECT_CALL(*mock_backend, AddLoginAsync(Eq(kTestForm), _))
      .WillOnce(
          WithArg<1>(Invoke([&](PasswordChangesOrErrorReply reply) -> void {
            std::move(reply).Run(
                CreateChangeList(PasswordStoreChange::ADD, kTestForm));
          })));
  EXPECT_CALL(mock_observer, OnLoginsRetained).Times(0);
  EXPECT_CALL(mock_observer,
              OnLoginsChanged(store.get(), ElementsAre(EqAddition(kTestForm))));
  store->AddLogin(kTestForm);
  WaitForPasswordStore();

  store->RemoveObserver(&mock_observer);
  store->ShutdownOnUIThread();
}

TEST_F(PasswordStoreTest, CallOnLoginsChangedIfUpdateProvidesChanges) {
  const PasswordForm kTestForm = MakePasswordForm(kTestWebRealm1);
  MockPasswordStoreObserver mock_observer;
  auto [store, mock_backend] = CreateUnownedStoreWithOwnedMockBackend();
  EXPECT_CALL(*mock_backend, InitBackend)
      .WillOnce(
          WithArg<3>(Invoke([](base::OnceCallback<void(bool)> completion) {
            std::move(completion).Run(true);
          })));
  store->Init(/*prefs=*/nullptr, /*affiliated_match_helper=*/nullptr);
  store->AddObserver(&mock_observer);

  // Expect that observers receive the update when the backend invokes the
  // reply with a `PasswordStoreChangeList`.
  EXPECT_CALL(*mock_backend, UpdateLoginAsync(Eq(kTestForm), _))
      .WillOnce(
          WithArg<1>(Invoke([&](PasswordChangesOrErrorReply reply) -> void {
            std::move(reply).Run(
                CreateChangeList(PasswordStoreChange::UPDATE, kTestForm));
          })));
  EXPECT_CALL(mock_observer, OnLoginsRetained).Times(0);
  EXPECT_CALL(mock_observer,
              OnLoginsChanged(store.get(), ElementsAre(EqUpdate(kTestForm))));
  store->UpdateLogin(kTestForm);
  WaitForPasswordStore();

  store->RemoveObserver(&mock_observer);
  store->ShutdownOnUIThread();
}

TEST_F(PasswordStoreTest, DoNotCallOnLoginsChangedIfAdditionReturnsError) {
  const PasswordForm kTestForm = MakePasswordForm(kTestWebRealm1);
  MockPasswordStoreObserver mock_observer;
  auto [store, mock_backend] = CreateUnownedStoreWithOwnedMockBackend();
  EXPECT_CALL(*mock_backend, InitBackend)
      .WillOnce(
          WithArg<3>(Invoke([](base::OnceCallback<void(bool)> completion) {
            std::move(completion).Run(true);
          })));
  store->Init(/*prefs=*/nullptr, /*affiliated_match_helper=*/nullptr);
  store->AddObserver(&mock_observer);

  // Expect that observers does not receive the change when backend fails.
  EXPECT_CALL(*mock_backend, AddLoginAsync(Eq(kTestForm), _))
      .WillOnce(
          WithArg<1>(Invoke([&](PasswordChangesOrErrorReply reply) -> void {
            std::move(reply).Run(kBackendError);
          })));
  EXPECT_CALL(mock_observer, OnLoginsRetained).Times(0);
  EXPECT_CALL(mock_observer, OnLoginsChanged).Times(0);
  store->AddLogin(kTestForm);
  WaitForPasswordStore();

  store->RemoveObserver(&mock_observer);
  store->ShutdownOnUIThread();
}

TEST_F(PasswordStoreTest, DoNotCallOnLoginsChangedIfUpdateReturnsError) {
  const PasswordForm kTestForm = MakePasswordForm(kTestWebRealm1);
  MockPasswordStoreObserver mock_observer;
  auto [store, mock_backend] = CreateUnownedStoreWithOwnedMockBackend();
  EXPECT_CALL(*mock_backend, InitBackend)
      .WillOnce(
          WithArg<3>(Invoke([](base::OnceCallback<void(bool)> completion) {
            std::move(completion).Run(true);
          })));
  store->Init(/*prefs=*/nullptr, /*affiliated_match_helper=*/nullptr);
  store->AddObserver(&mock_observer);

  // Expect that observers does not receive the update when backend fails.
  EXPECT_CALL(*mock_backend, UpdateLoginAsync(Eq(kTestForm), _))
      .WillOnce(
          WithArg<1>(Invoke([&](PasswordChangesOrErrorReply reply) -> void {
            std::move(reply).Run(kBackendError);
          })));
  EXPECT_CALL(mock_observer, OnLoginsRetained).Times(0);
  EXPECT_CALL(mock_observer, OnLoginsChanged).Times(0);
  store->UpdateLogin(kTestForm);
  WaitForPasswordStore();

  store->RemoveObserver(&mock_observer);
  store->ShutdownOnUIThread();
}

#if BUILDFLAG(IS_ANDROID)
TEST_F(PasswordStoreTest, CallOnLoginsRetainedIfUpdateProvidesNoChanges) {
  base::HistogramTester histogram_tester;
  const char kOnLoginRetainedMetric[] =
      "PasswordManager.PasswordStore.OnLoginsRetained";
  std::vector<PasswordForm> all_credentials;
  all_credentials.push_back(*FillPasswordFormWithData(
      CreateTestPasswordFormDataByOrigin(kTestWebRealm1),
      /*is_account_store=*/false));
  all_credentials.push_back(*FillPasswordFormWithData(
      CreateTestPasswordFormDataByOrigin(kTestAndroidRealm1),
      /*is_account_store=*/false));
  const PasswordForm kTestForm = all_credentials[0];
  const PasswordForm kOtherForm = all_credentials[1];
  MockPasswordStoreObserver mock_observer;
  auto [store, mock_backend] = CreateUnownedStoreWithOwnedMockBackend();
  EXPECT_CALL(*mock_backend, InitBackend)
      .WillOnce(
          WithArg<3>(Invoke([](base::OnceCallback<void(bool)> completion) {
            std::move(completion).Run(true);
          })));
  store->Init(/*prefs=*/nullptr, /*affiliated_match_helper=*/nullptr);
  store->AddObserver(&mock_observer);

  // Expect that observers receive the full list if the backend invokes the
  // reply with a nullopt.
  EXPECT_CALL(*mock_backend, UpdateLoginAsync(Eq(kTestForm), _))
      .WillOnce(
          WithArg<1>(Invoke([](PasswordChangesOrErrorReply reply) -> void {
            std::move(reply).Run(std::nullopt);
          })));
  EXPECT_CALL(*mock_backend, GetAllLoginsAsync(_))
      .WillOnce(WithArg<0>(
          Invoke([&all_credentials](LoginsOrErrorReply reply) -> void {
            std::move(reply).Run(std::move(all_credentials));
          })));
  EXPECT_CALL(mock_observer, OnLoginsChanged).Times(0);
  EXPECT_CALL(mock_observer,
              OnLoginsRetained(store.get(),
                               UnorderedElementsAre(kTestForm, kOtherForm)));
  store->UpdateLogin(kTestForm);
  WaitForPasswordStore();

  store->RemoveObserver(&mock_observer);
  store->ShutdownOnUIThread();
  histogram_tester.ExpectUniqueSample(kOnLoginRetainedMetric, /*Update*/ 2, 1);
  histogram_tester.ExpectTotalCount(kOnLoginRetainedMetric, 1);
}
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
TEST_F(PasswordStoreTest, RecordsPotentialOnLoginsRetainedInvokations) {
  base::HistogramTester histogram_tester;
  const PasswordForm kTestForm = MakePasswordForm(kTestWebRealm1);
  MockPasswordStoreObserver mock_observer;
  auto [store, mock_backend] = CreateUnownedStoreWithOwnedMockBackend();
  EXPECT_CALL(*mock_backend, InitBackend)
      .WillOnce(
          WithArg<3>(Invoke([](base::OnceCallback<void(bool)> completion) {
            std::move(completion).Run(true);
          })));
  store->Init(/*prefs=*/nullptr, /*affiliated_match_helper=*/nullptr);
  store->AddObserver(&mock_observer);

  // A changelist will be returned, so `OnLoginsRetained` call is not issued.
  // But the changelist is empty, so `OnLoginsChanged` is not issued either.
  // Regardless, this is a case where `OnLoginsRetained` would be called if the
  // GMS backend was active — therefore record the potential call.
  EXPECT_CALL(*mock_backend, UpdateLoginAsync(Eq(kTestForm), _))
      .WillOnce(
          WithArg<1>(Invoke([](PasswordChangesOrErrorReply reply) -> void {
            std::move(reply).Run(PasswordStoreChangeList());
          })));
  EXPECT_CALL(mock_observer, OnLoginsRetained).Times(0);
  EXPECT_CALL(mock_observer, OnLoginsChanged).Times(0);
  store->UpdateLogin(kTestForm);
  WaitForPasswordStore();

  const char kOnLoginRetainedMetric[] =
      "PasswordManager.PasswordStore.OnLoginsRetained";
  histogram_tester.ExpectUniqueSample(kOnLoginRetainedMetric, /*Update*/ 2, 1);
  histogram_tester.ExpectTotalCount(kOnLoginRetainedMetric, 1);

  store->RemoveObserver(&mock_observer);
  store->ShutdownOnUIThread();
}
#endif  // BUILDFLAG(IS_ANDROID)

TEST_F(PasswordStoreTest, AbleToSavePasswords) {
  auto [store, mock_backend] = CreateUnownedStoreWithOwnedMockBackend();
  EXPECT_CALL(*mock_backend, InitBackend)
      .WillOnce(
          WithArg<3>(Invoke([](base::OnceCallback<void(bool)> completion) {
            std::move(completion).Run(true);
          })));
  store->Init(/*prefs=*/nullptr, /*affiliated_match_helper=*/nullptr);
  EXPECT_CALL(*mock_backend, IsAbleToSavePasswords)
      .WillOnce(testing::Return(true));

  EXPECT_TRUE(store->IsAbleToSavePasswords());
  store->ShutdownOnUIThread();
}

TEST_F(PasswordStoreTest, NotAbleToSavePasswords) {
  auto [store, mock_backend] = CreateUnownedStoreWithOwnedMockBackend();
  EXPECT_CALL(*mock_backend, InitBackend)
      .WillOnce(
          WithArg<3>(Invoke([](base::OnceCallback<void(bool)> completion) {
            std::move(completion).Run(true);
          })));
  store->Init(/*prefs=*/nullptr, /*affiliated_match_helper=*/nullptr);
  EXPECT_CALL(*mock_backend, IsAbleToSavePasswords)
      .WillOnce(testing::Return(false));

  EXPECT_FALSE(store->IsAbleToSavePasswords());
  store->ShutdownOnUIThread();
}

TEST_F(PasswordStoreTest, GetAllLogins) {
  static constexpr PasswordFormData kTestCredentials[] = {
      {PasswordForm::Scheme::kHtml, kTestAndroidRealm1, "", "", u"", u"", u"",
       u"username_value_1", u"", kTestLastUsageTime, 1},
      {PasswordForm::Scheme::kHtml, kTestAndroidRealm2, "", "", u"", u"", u"",
       u"username_value_2", u"", kTestLastUsageTime, 1},
      {PasswordForm::Scheme::kHtml, kTestAndroidRealm3, "", "", u"", u"", u"",
       u"username_value_3", u"", kTestLastUsageTime, 1},
      {PasswordForm::Scheme::kHtml, kTestWebRealm1, kTestWebOrigin1, "", u"",
       u"", u"", u"username_value_4", u"", kTestLastUsageTime, 1},
      // A PasswordFormData with nullptr as the username_value will be converted
      // in a blocklisted PasswordForm in FillPasswordFormWithData().
      {PasswordForm::Scheme::kHtml, kTestWebRealm2, kTestWebOrigin2, "", u"",
       u"", u"", nullptr, u"", kTestLastUsageTime, 1},
      {PasswordForm::Scheme::kHtml, kTestWebRealm3, kTestWebOrigin3, "", u"",
       u"", u"", nullptr, u"", kTestLastUsageTime, 1}};

  scoped_refptr<PasswordStore> store = CreatePasswordStore();
  store->Init(/*prefs=*/nullptr, /*affiliated_match_helper=*/nullptr);

  std::vector<std::unique_ptr<PasswordForm>> all_credentials;
  for (const auto& test_credential : kTestCredentials) {
    all_credentials.push_back(
        FillPasswordFormWithData(test_credential, /*is_account_store=*/false));
    store->AddLogin(*all_credentials.back());
  }

  MockPasswordStoreConsumer mock_consumer;
  std::vector<PasswordForm> expected_results;
  for (const auto& credential : all_credentials) {
    expected_results.push_back(*credential);
  }

  EXPECT_CALL(mock_consumer,
              OnGetPasswordStoreResultsOrErrorFrom(
                  _, VariantWith<LoginsResult>(ElementsAreArray(
                         FormsIgnoringPrimaryKey(expected_results)))));
  store->GetAllLogins(mock_consumer.GetWeakPtr());
  WaitForPasswordStore();
  store->ShutdownOnUIThread();
}

TEST_F(PasswordStoreTest, GetAllLoginsWithAffiliationAndBrandingInformation) {
  auto store = base::MakeRefCounted<PasswordStore>(
      std::make_unique<FakePasswordStoreBackend>());

  affiliations::FakeAffiliationService fake_affiliation_service;
  auto mock_match_helper =
      std::make_unique<MockAffiliatedMatchHelper>(&fake_affiliation_service);
  MockAffiliatedMatchHelper* match_helper = mock_match_helper.get();
  store->Init(/*prefs=*/nullptr, std::move(mock_match_helper));

  static constexpr PasswordFormData kTestCredentials[] = {
      {PasswordForm::Scheme::kHtml, kTestAndroidRealm1, "", "", u"", u"", u"",
       u"username_value_1", u"", kTestLastUsageTime, 1},
      {PasswordForm::Scheme::kHtml, kTestAndroidRealm3, "", "", u"", u"", u"",
       u"username_value_3", u"", kTestLastUsageTime, 1},
      {PasswordForm::Scheme::kHtml, kTestAndroidRealm2, "", "", u"", u"", u"",
       u"username_value_2", u"", kTestLastUsageTime, 1},
      {PasswordForm::Scheme::kHtml, kTestWebRealm1, kTestWebOrigin1, "", u"",
       u"", u"", u"username_value_4", u"", kTestLastUsageTime, 1},
      // A PasswordFormData with nullptr as the username_value will be converted
      // in a blocklisted PasswordForm in FillPasswordFormWithData().
      {PasswordForm::Scheme::kHtml, kTestWebRealm2, kTestWebOrigin2, "", u"",
       u"", u"", nullptr, u"", kTestLastUsageTime, 1},
      {PasswordForm::Scheme::kHtml, kTestWebRealm3, kTestWebOrigin3, "", u"",
       u"", u"", nullptr, u"", kTestLastUsageTime, 1}};

  std::vector<std::unique_ptr<PasswordForm>> all_credentials;
  for (const auto& test_credential : kTestCredentials) {
    all_credentials.push_back(
        FillPasswordFormWithData(test_credential, /*is_account_store=*/false));
    store->AddLogin(*all_credentials.back());
  }
  WaitForPasswordStore();

  MockPasswordStoreConsumer mock_consumer;
  std::vector<PasswordForm> expected_results;
  for (const auto& credential : all_credentials) {
    expected_results.push_back(*credential);
  }

  std::vector<MockAffiliatedMatchHelper::AffiliationAndBrandingInformation>
      affiliation_info_for_results = {
          {kTestWebRealm1, kTestAndroidName1, GURL(kTestAndroidIconURL1)},
          {/* Pretend affiliation or branding info is unavailable. */},
          {kTestWebRealm2, kTestAndroidName2, GURL(kTestAndroidIconURL2)},
          {/* Pretend affiliation or branding info is unavailable. */},
          {/* Pretend affiliation or branding info is unavailable. */},
          {/* Pretend affiliation or branding info is unavailable. */}};

  match_helper->ExpectCallToInjectAffiliationAndBrandingInformation(
      affiliation_info_for_results);

  for (size_t i = 0; i < expected_results.size(); ++i) {
    expected_results[i].affiliated_web_realm =
        affiliation_info_for_results[i].affiliated_web_realm;
    expected_results[i].app_display_name =
        affiliation_info_for_results[i].app_display_name;
    expected_results[i].app_icon_url =
        affiliation_info_for_results[i].app_icon_url;
  }

  EXPECT_CALL(
      mock_consumer,
      OnGetPasswordStoreResultsOrErrorFrom(
          store.get(), VariantWith<LoginsResult>(UnorderedElementsAreArray(
                           FormsIgnoringPrimaryKey(expected_results)))));
  store->GetAllLoginsWithAffiliationAndBrandingInformation(
      mock_consumer.GetWeakPtr());

  // Since GetAutofillableLoginsWithAffiliationAndBrandingInformation
  // schedules a request for affiliation information to UI thread, don't
  // shutdown UI thread until there are no tasks in the UI queue.
  WaitForPasswordStore();
  store->ShutdownOnUIThread();
}

TEST_F(PasswordStoreTest, Unblocklisting) {
  static const PasswordFormData kTestCredentials[] = {
      // A PasswordFormData with nullptr as the username_value will be converted
      // in a blocklisted PasswordForm in FillPasswordFormWithData().

      // Blocklisted entry for the observed domain.
      {PasswordForm::Scheme::kHtml, kTestWebRealm1, kTestWebOrigin1, "", u"",
       u"", u"", nullptr, u"", kTestLastUsageTime, 1},
      // Blocklisted entry for a PSL match of the observed form.
      {PasswordForm::Scheme::kHtml, kTestPSLMatchingWebRealm,
       kTestPSLMatchingWebOrigin, "", u"", u"", u"", nullptr, u"",
       kTestLastUsageTime, 1},
      // Blocklisted entry for another domain
      {PasswordForm::Scheme::kHtml, kTestUnrelatedWebRealm,
       kTestUnrelatedWebOrigin, "", u"", u"", u"", nullptr, u"",
       kTestLastUsageTime, 1},
      // Non-blocklisted for the observed domain with a username.
      {PasswordForm::Scheme::kHtml, kTestWebRealm1, kTestWebOrigin1, "", u"",
       u"", u"", u"username", u"", kTestLastUsageTime, 1},
      // Non-blocklisted for the observed domain without a username.
      {PasswordForm::Scheme::kHtml, kTestWebRealm1, kTestWebOrigin1, "", u"",
       u"", u"username_element", u"", u"", kTestLastUsageTime, 1},
      // Non-blocklisted entry for a PSL match of the observed form.
      {PasswordForm::Scheme::kHtml, kTestPSLMatchingWebRealm,
       kTestPSLMatchingWebOrigin, "", u"", u"", u"", u"username", u"",
       kTestLastUsageTime, 1},
      // Non-blocklisted entry for another domain
      {PasswordForm::Scheme::kHtml, kTestUnrelatedWebRealm2,
       kTestUnrelatedWebOrigin2, "", u"", u"", u"", u"username", u"",
       kTestLastUsageTime, 1}};

  scoped_refptr<PasswordStore> store = CreatePasswordStore();
  store->Init(/*prefs=*/nullptr, /*affiliated_match_helper=*/nullptr);

  std::vector<PasswordForm> all_credentials;
  for (const auto& test_credential : kTestCredentials) {
    all_credentials.push_back(
        *FillPasswordFormWithData(test_credential, /*is_account_store=*/false));
    store->AddLogin(all_credentials.back());
  }
  WaitForPasswordStore();

  MockPasswordStoreObserver mock_observer;
  store->AddObserver(&mock_observer);

  // Only the related non-PSL match should be deleted.
  EXPECT_CALL(mock_observer, OnLoginsChanged(_, testing::SizeIs(1u)));
  base::RunLoop run_loop;
  PasswordFormDigest observed_form_digest = {
      PasswordForm::Scheme::kHtml, kTestWebRealm1, GURL(kTestWebOrigin1)};
  store->Unblocklist(observed_form_digest, run_loop.QuitClosure());
  run_loop.Run();
  testing::Mock::VerifyAndClearExpectations(&mock_observer);

  // Unblocklisting will delete only the first credential. It should leave the
  // PSL match as well as the unrelated blocklisting entry and all
  // non-blocklisting entries.
  all_credentials.erase(all_credentials.begin());

  MockPasswordStoreConsumer mock_consumer;
  EXPECT_CALL(
      mock_consumer,
      OnGetPasswordStoreResultsOrErrorFrom(
          store.get(), VariantWith<LoginsResult>(UnorderedElementsAreArray(
                           FormsIgnoringPrimaryKey(all_credentials)))));
  store->GetAllLogins(mock_consumer.GetWeakPtr());
  WaitForPasswordStore();

  store->RemoveObserver(&mock_observer);
  store->ShutdownOnUIThread();
}

// Test that updating a password in the store deletes the corresponding
// insecure credential synchronously.
TEST_F(PasswordStoreTest, RemoveInsecureCredentialsSyncOnUpdate) {
  scoped_refptr<PasswordStore> store = CreatePasswordStore();
  store->Init(/*prefs=*/nullptr, /*affiliated_match_helper=*/nullptr);

  constexpr PasswordFormData kTestCredential = {PasswordForm::Scheme::kHtml,
                                                kTestWebRealm1,
                                                kTestWebOrigin1,
                                                "",
                                                u"",
                                                u"username_element_1",
                                                u"password_element_1",
                                                u"username1",
                                                u"12345",
                                                10,
                                                5};
  PasswordForm form(
      *FillPasswordFormWithData(kTestCredential, /*is_account_store=*/false));
  form.password_issues = {
      {InsecureType::kLeaked,
       InsecurityMetadata(base::Time::FromTimeT(100), IsMuted(false),
                          TriggerBackendNotification(false))}};
  store->AddLogin(form);

  WaitForPasswordStore();

  // Update the password value and immediately get the logins which are
  // expected to NiceMock<no longer have password_issues.
  form.password_value = u"new_password";
  form.password_issues.clear();
  store->UpdateLogin(form);

  MockPasswordStoreConsumer mock_consumer;
  store->GetAllLogins(mock_consumer.GetWeakPtr());

  EXPECT_CALL(mock_consumer,
              OnGetPasswordStoreResultsOrErrorFrom(
                  store.get(), VariantWith<LoginsResult>(
                                   ElementsAre(HasPrimaryKeyAndEquals(form)))));

  WaitForPasswordStore();

  store->ShutdownOnUIThread();
}

TEST_F(PasswordStoreTest, TestGetLoginRequestCancelable) {
  scoped_refptr<PasswordStore> store = CreatePasswordStore();
  affiliations::FakeAffiliationService fake_affiliation_service;
  auto owning_mock_match_helper =
      std::make_unique<MockAffiliatedMatchHelper>(&fake_affiliation_service);
  MockAffiliatedMatchHelper* mock_affiliated_match_helper =
      owning_mock_match_helper.get();
  store->Init(/*prefs=*/nullptr, std::move(owning_mock_match_helper));
  WaitForPasswordStore();

  store->AddLogin(MakePasswordForm(kTestAndroidRealm1));
  WaitForPasswordStore();

  PasswordFormDigest observed_form = {PasswordForm::Scheme::kHtml,
                                      kTestWebRealm1, GURL(kTestWebRealm1)};

  // Add affiliated android form corresponding to a 'observed_form'.
  mock_affiliated_match_helper->ExpectCallToGetAffiliatedAndGrouped(
      observed_form, {kTestAndroidRealm1});

  MockPasswordStoreConsumer mock_consumer;
  EXPECT_CALL(mock_consumer, OnGetPasswordStoreResults).Times(0);
  store->GetLogins(observed_form, mock_consumer.GetWeakPtr());
  mock_consumer.CancelAllRequests();
  WaitForPasswordStore();

  store->ShutdownOnUIThread();
}

TEST_F(PasswordStoreTest, TestUnblockListEmptyStore) {
  scoped_refptr<PasswordStore> store = CreatePasswordStore();
  store->Init(/*prefs=*/nullptr, /*affiliated_match_helper=*/nullptr);
  WaitForPasswordStore();

  MockPasswordStoreObserver observer;
  store->AddObserver(&observer);

  PasswordFormDigest digest = {PasswordForm::Scheme::kHtml, kTestWebRealm1,
                               GURL(kTestWebOrigin1)};

  base::RunLoop run_loop;
  EXPECT_CALL(observer, OnLoginsChanged).Times(0);
  store->Unblocklist(digest, run_loop.QuitClosure());
  run_loop.Run();

  store->RemoveObserver(&observer);
  store->ShutdownOnUIThread();
}

// Collection of origin-related testcases common to all platform-specific
// stores.
class PasswordStoreOriginTest : public PasswordStoreTest {
 public:
  void SetUp() override {
    PasswordStoreTest::SetUp();
    store_ = CreatePasswordStore();
    store_->Init(/*prefs=*/nullptr, /*affiliated_match_helper=*/nullptr);
  }

  void TearDown() override {
    PasswordStoreTest::TearDown();
    store_->ShutdownOnUIThread();
    WaitForPasswordStore();
  }

  PasswordStore* store() { return store_.get(); }

 private:
  scoped_refptr<PasswordStore> store_;
};

TEST_F(PasswordStoreOriginTest,
       RemoveLoginsByURLAndTimeImpl_AllFittingOriginAndTime) {
  constexpr char origin_url[] = "http://foo.example.com/";
  std::unique_ptr<PasswordForm> form =
      FillPasswordFormWithData(CreateTestPasswordFormDataByOrigin(origin_url),
                               /*is_account_store=*/false);
  store()->AddLogin(*form);
  WaitForPasswordStore();

  MockPasswordStoreObserver observer;
  store()->AddObserver(&observer);

  const GURL origin = GURL(origin_url);
  base::RepeatingCallback<bool(const GURL&)> filter =
      base::BindRepeating(&MatchesOrigin, origin);
  base::RunLoop run_loop;
  EXPECT_CALL(observer,
              OnLoginsChanged(_, ElementsAre(PasswordStoreChange(
                                     PasswordStoreChange::REMOVE, *form))));
  store()->RemoveLoginsByURLAndTime(FROM_HERE, filter, base::Time(),
                                    base::Time::Max(), run_loop.QuitClosure());
  run_loop.Run();

  store()->RemoveObserver(&observer);
}

TEST_F(PasswordStoreOriginTest,
       RemoveLoginsByURLAndTimeImpl_SomeFittingOriginAndTime) {
  constexpr char fitting_url[] = "http://foo.example.com/";
  std::unique_ptr<PasswordForm> form =
      FillPasswordFormWithData(CreateTestPasswordFormDataByOrigin(fitting_url),
                               /*is_account_store=*/false);
  store()->AddLogin(*form);

  const char nonfitting_url[] = "http://bar.example.com/";
  store()->AddLogin(*FillPasswordFormWithData(
      CreateTestPasswordFormDataByOrigin(nonfitting_url),
      /*is_account_store=*/false));

  WaitForPasswordStore();

  MockPasswordStoreObserver observer;
  store()->AddObserver(&observer);

  const GURL fitting_origin = GURL(fitting_url);
  base::RepeatingCallback<bool(const GURL&)> filter =
      base::BindRepeating(&MatchesOrigin, fitting_origin);
  base::RunLoop run_loop;
  EXPECT_CALL(observer,
              OnLoginsChanged(_, ElementsAre(PasswordStoreChange(
                                     PasswordStoreChange::REMOVE, *form))));
  store()->RemoveLoginsByURLAndTime(FROM_HERE, filter, base::Time(),
                                    base::Time::Max(), run_loop.QuitClosure());
  run_loop.Run();

  store()->RemoveObserver(&observer);
}

TEST_F(PasswordStoreOriginTest,
       RemoveLoginsByURLAndTimeImpl_NonMatchingOrigin) {
  constexpr char origin_url[] = "http://foo.example.com/";
  std::unique_ptr<PasswordForm> form =
      FillPasswordFormWithData(CreateTestPasswordFormDataByOrigin(origin_url),
                               /*is_account_store=*/false);
  store()->AddLogin(*form);
  WaitForPasswordStore();

  MockPasswordStoreObserver observer;
  store()->AddObserver(&observer);

  const GURL other_origin = GURL("http://bar.example.com/");
  base::RepeatingCallback<bool(const GURL&)> filter =
      base::BindRepeating(&MatchesOrigin, other_origin);
  base::RunLoop run_loop;
  EXPECT_CALL(observer, OnLoginsChanged).Times(0);
  store()->RemoveLoginsByURLAndTime(FROM_HERE, filter, base::Time(),
                                    base::Time::Max(), run_loop.QuitClosure());
  run_loop.Run();

  store()->RemoveObserver(&observer);
}

TEST_F(PasswordStoreOriginTest,
       RemoveLoginsByURLAndTimeImpl_NotWithinTimeInterval) {
  constexpr char origin_url[] = "http://foo.example.com/";
  std::unique_ptr<PasswordForm> form =
      FillPasswordFormWithData(CreateTestPasswordFormDataByOrigin(origin_url),
                               /*is_account_store=*/false);
  store()->AddLogin(*form);
  WaitForPasswordStore();

  MockPasswordStoreObserver observer;
  store()->AddObserver(&observer);

  const GURL origin = GURL(origin_url);
  base::RepeatingCallback<bool(const GURL&)> filter =
      base::BindRepeating(&MatchesOrigin, origin);
  base::Time time_after_creation_date = form->date_created + base::Days(1);
  base::RunLoop run_loop;
  EXPECT_CALL(observer, OnLoginsChanged).Times(0);
  store()->RemoveLoginsByURLAndTime(FROM_HERE, filter, time_after_creation_date,
                                    base::Time::Max(), run_loop.QuitClosure());
  run_loop.Run();

  store()->RemoveObserver(&observer);
}

class PasswordStoreDelayedInitTest : public testing::Test {
 public:
  void SetUp() override {
    auto [store, mock_backend] = CreateUnownedStoreWithOwnedMockBackend();
    store_ = std::move(store);
    mock_backend_ = mock_backend;
  }

  void TearDown() override {
    mock_backend_ = nullptr;
    store_->ShutdownOnUIThread();
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }
  void AdvanceClock(base::TimeDelta delta) {
    task_environment_.AdvanceClock(delta);
  }

  PasswordStore* store() { return store_.get(); }
  MockPasswordStoreBackend* backend() { return mock_backend_.get(); }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  scoped_refptr<PasswordStore> store_;
  raw_ptr<MockPasswordStoreBackend> mock_backend_;
};

TEST_F(PasswordStoreDelayedInitTest, AddLogin) {
  std::unique_ptr<PasswordForm> form = FillPasswordFormWithData(
      CreateTestPasswordFormDataByOrigin("http://foo.example.com/"),
      /*is_account_store=*/false);

  base::OnceCallback<void(bool)> init_callback;
  EXPECT_CALL(*backend(), InitBackend).WillOnce(MoveArg<3>(&init_callback));
  store()->Init(/*prefs=*/nullptr, /*affiliated_match_helper=*/nullptr);

  base::MockOnceClosure mock_callback;
  store()->AddLogin(*form, mock_callback.Get());

  EXPECT_CALL(mock_callback, Run).Times(0);
  RunUntilIdle();

  EXPECT_CALL(*backend(), AddLoginAsync)
      .WillOnce(base::test::RunOnceCallback<1>(PasswordChanges()));
  EXPECT_CALL(mock_callback, Run);
  std::move(init_callback).Run(true);
}

TEST_F(PasswordStoreDelayedInitTest, UpdateLogin) {
  std::unique_ptr<PasswordForm> form = FillPasswordFormWithData(
      CreateTestPasswordFormDataByOrigin("http://foo.example.com/"),
      /*is_account_store=*/false);

  base::OnceCallback<void(bool)> init_callback;
  EXPECT_CALL(*backend(), InitBackend).WillOnce(MoveArg<3>(&init_callback));
  store()->Init(/*prefs=*/nullptr, /*affiliated_match_helper=*/nullptr);

  base::MockOnceClosure mock_callback;
  store()->UpdateLogin(*form, mock_callback.Get());

  EXPECT_CALL(mock_callback, Run).Times(0);
  RunUntilIdle();

  EXPECT_CALL(*backend(), UpdateLoginAsync)
      .WillOnce(base::test::RunOnceCallback<1>(PasswordChanges()));
  EXPECT_CALL(mock_callback, Run);
  std::move(init_callback).Run(true);
}

TEST_F(PasswordStoreDelayedInitTest, GetAutofillableLogins) {
  std::unique_ptr<PasswordForm> form = FillPasswordFormWithData(
      CreateTestPasswordFormDataByOrigin("http://foo.example.com/"),
      /*is_account_store=*/false);

  base::OnceCallback<void(bool)> init_callback;
  EXPECT_CALL(*backend(), InitBackend).WillOnce(MoveArg<3>(&init_callback));
  store()->Init(/*prefs=*/nullptr, /*affiliated_match_helper=*/nullptr);

  MockPasswordStoreConsumer mock_consumer;
  store()->GetAutofillableLogins(mock_consumer.GetWeakPtr());

  EXPECT_CALL(mock_consumer, OnGetPasswordStoreResultsOrErrorFrom).Times(0);
  RunUntilIdle();

  EXPECT_CALL(*backend(), GetAutofillableLoginsAsync)
      .WillOnce(base::test::RunOnceCallback<0>(LoginsResult()));
  EXPECT_CALL(mock_consumer, OnGetPasswordStoreResultsOrErrorFrom);
  std::move(init_callback).Run(true);
}

}  // namespace password_manager
