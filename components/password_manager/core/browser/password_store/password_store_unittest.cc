// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store/password_store.h"

#include <stddef.h>

#include <array>
#include <memory>
#include <tuple>
#include <utility>

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
#include "components/os_crypt/async/browser/test_utils.h"
#include "components/password_manager/core/browser/affiliation/mock_affiliated_match_helper.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/form_parsing/form_data_parser.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_store/fake_password_store_backend.h"
#include "components/password_manager/core/browser/password_store/mock_password_store_backend.h"
#include "components/password_manager/core/browser/password_store/mock_password_store_consumer.h"
#include "components/password_manager/core/browser/password_store/password_form_converters.h"
#include "components/password_manager/core/browser/password_store/password_store_backend.h"
#include "components/password_manager/core/browser/password_store/password_store_change.h"
#include "components/password_manager/core/browser/password_store/password_store_consumer.h"
#include "components/password_manager/core/browser/password_store/stored_credential.h"
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
using testing::IsEmpty;
using testing::Pointee;
using testing::SizeIs;
using testing::UnorderedElementsAre;
using testing::UnorderedElementsAreArray;
using testing::VariantWith;
using testing::WithArg;

namespace password_manager {

namespace {

struct CopyableStoredCredential {
  explicit CopyableStoredCredential(StoredCredential c) : cred(std::move(c)) {}
  CopyableStoredCredential(const CopyableStoredCredential& other)
      : cred(CloneStoredCredential(other.cred)) {}
  CopyableStoredCredential& operator=(const CopyableStoredCredential& other) {
    if (this != &other) {
      cred = CloneStoredCredential(other.cred);
    }
    return *this;
  }
  CopyableStoredCredential(CopyableStoredCredential&&) = default;
  CopyableStoredCredential& operator=(CopyableStoredCredential&&) = default;

  StoredCredential cred;
};

MATCHER_P(MatchesCredential, expected_wrapper, "") {
  return arg == expected_wrapper.cred;
}

MATCHER_P(MatchesCredentialIgnoringPrimaryKey, expected_wrapper, "") {
  StoredCredential expected_with_key =
      CloneStoredCredential(expected_wrapper.cred);
  expected_with_key.primary_key = arg.primary_key;
  expected_with_key.keychain_identifier = arg.keychain_identifier;
  return expected_with_key == arg;
}

std::vector<testing::Matcher<StoredCredential>>
StoredCredentialsIgnoringPrimaryKey(
    const std::vector<StoredCredential>& creds) {
  std::vector<testing::Matcher<StoredCredential>> result;
  for (const auto& cred : creds) {
    result.push_back(MatchesCredentialIgnoringPrimaryKey(
        CopyableStoredCredential(CloneStoredCredential(cred))));
  }
  return result;
}

constexpr const char kTestWebRealm1[] = "https://one.example.com/";
constexpr const char kTestWebOrigin1[] = "https://one.example.com/origin";
constexpr const char kTestWebRealm2[] = "https://two.example.com/";
constexpr const char kTestWebOrigin2[] = "https://two.example.com/origin";
constexpr const char kTestWebRealm3[] = "https://three.example.com/";
constexpr const char kTestWebOrigin3[] = "https://three.example.com/origin";
constexpr const char kTestAndroidRealm1[] =
    "android://hash@com.example.android/";
constexpr const char kTestAndroidRealm2[] =
    "android://hash@com.example.two.android/";
constexpr const char kTestAndroidRealm3[] =
    "android://hash@com.example.three.android/";
constexpr const time_t kTestLastUsageTime = 1546300800;  // 00:00 Jan 1 2019 UTC

const PasswordStoreBackendError kBackendError =
    PasswordStoreBackendError(PasswordStoreBackendErrorType::kUncategorized);

StoredCredential MakeStoredCredential(const std::string& signon_realm) {
  StoredCredential cred;
  cred.url = GURL("http://www.origin.com");
  cred.username_element = u"username_element";
  cred.username_value = u"username_value";
  cred.password_element = u"password_element";
  cred.signon_realm = signon_realm;
  return cred;
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
                                         StoredCredential cred) {
  PasswordStoreChangeList changes;
  changes.emplace_back(type, std::move(cred));
  return changes;
}

auto HasChangeType(PasswordStoreChange::Type type) {
  return testing::Property(&PasswordStoreChange::type, Eq(type));
}

auto HasCredential(const StoredCredential& cred) {
  return testing::Property(
      &PasswordStoreChange::credential,
      MatchesCredential(CopyableStoredCredential(CloneStoredCredential(cred))));
}

auto EqChange(PasswordStoreChange::Type type, const StoredCredential& cred) {
  return AllOf(HasChangeType(type), HasCredential(cred));
}

auto EqRemoval(const StoredCredential& cred) {
  return EqChange(PasswordStoreChange::REMOVE, cred);
}

auto EqAddition(const StoredCredential& cred) {
  return EqChange(PasswordStoreChange::ADD, cred);
}

auto EqUpdate(const StoredCredential& cred) {
  return EqChange(PasswordStoreChange::UPDATE, cred);
}

}  // namespace

class PasswordStoreTest : public testing::Test {
 public:
  PasswordStoreTest(const PasswordStoreTest&) = delete;
  PasswordStoreTest& operator=(const PasswordStoreTest&) = delete;

 protected:
  PasswordStoreTest() {
    os_crypt_async_ = os_crypt_async::GetTestOSCryptAsyncForTesting(
        /*is_sync_for_unittests=*/true);
  }

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
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

  void TearDown() override {}

  void WaitForPasswordStore() { task_environment_.RunUntilIdle(); }

  base::FilePath test_login_db_file_path() const {
    return temp_dir_.GetPath().Append(FILE_PATH_LITERAL("login_test"));
  }

  scoped_refptr<PasswordStore> CreatePasswordStore() {
    auto backend = std::make_unique<FakePasswordStoreBackend>(
        password_manager::IsAccountStore(false));
    return base::MakeRefCounted<PasswordStore>(std::move(backend));
  }
  TestingPrefServiceSimple* pref_service() { return &pref_service_; }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

 private:
  base::ScopedTempDir temp_dir_;
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<os_crypt_async::OSCryptAsync> os_crypt_async_;
};

TEST_F(PasswordStoreTest, AddLogins) {
  base::HistogramTester histogram_tester;
  std::vector<StoredCredential> all_credentials;
  all_credentials.push_back(FillStoredCredentialWithData(
      CreateTestPasswordFormDataByOrigin(kTestWebRealm1),
      /*is_account_store=*/false));
  all_credentials.push_back(FillStoredCredentialWithData(
      CreateTestPasswordFormDataByOrigin(kTestAndroidRealm1),
      /*is_account_store=*/false));

  scoped_refptr<PasswordStore> store = CreatePasswordStore();
  store->Init();

  MockPasswordStoreObserver mock_observer;
  store->AddObserver(&mock_observer);

  EXPECT_CALL(mock_observer, OnLoginsChanged(_, testing::SizeIs(2u)));
  std::vector<StoredCredential> stored_credentials;
  stored_credentials.push_back(CloneStoredCredential(all_credentials[0]));
  stored_credentials.push_back(CloneStoredCredential(all_credentials[1]));
  store->AddLogins(std::move(stored_credentials));
  WaitForPasswordStore();

  testing::Mock::VerifyAndClearExpectations(&mock_observer);

  MockPasswordStoreConsumer mock_consumer;

  EXPECT_CALL(mock_consumer,
              OnGetPasswordStoreResultsOrErrorFrom(
                  store.get(),
                  VariantWith<LoginsResult>(UnorderedElementsAreArray(
                      StoredCredentialsIgnoringPrimaryKey(all_credentials)))));
  store->GetAutofillableLogins(mock_consumer.GetWeakPtr());
  WaitForPasswordStore();

  store->RemoveObserver(&mock_observer);
  store->ShutdownOnUIThread();
}

TEST_F(PasswordStoreTest, UpdateLogins) {
  PasswordFormData form_data_1 =
      CreateTestPasswordFormDataByOrigin(kTestWebRealm1);
  PasswordFormData form_data_2 =
      CreateTestPasswordFormDataByOrigin(kTestAndroidRealm1);
  std::vector<StoredCredential> all_credentials;
  all_credentials.push_back(
      FillStoredCredentialWithData(form_data_1, /*is_account_store=*/false));
  all_credentials.push_back(
      FillStoredCredentialWithData(form_data_2, /*is_account_store=*/false));

  scoped_refptr<PasswordStore> store = CreatePasswordStore();
  store->Init();

  std::vector<StoredCredential> stored_credentials;
  for (const auto& cred : all_credentials) {
    stored_credentials.push_back(CloneStoredCredential(cred));
  }
  store->AddLogins(std::move(stored_credentials));
  WaitForPasswordStore();

  form_data_1.password_value = u"new_password1";
  form_data_2.password_value = u"new_password2";

  std::vector<StoredCredential> updated_credentials;
  updated_credentials.push_back(
      FillStoredCredentialWithData(form_data_1, /*is_account_store=*/false));
  updated_credentials.push_back(
      FillStoredCredentialWithData(form_data_2, /*is_account_store=*/false));

  MockPasswordStoreObserver mock_observer;
  store->AddObserver(&mock_observer);

  EXPECT_CALL(mock_observer, OnLoginsChanged(_, testing::SizeIs(2u)));
  std::vector<StoredCredential> stored_updated_credentials;
  for (const auto& cred : updated_credentials) {
    stored_updated_credentials.push_back(CloneStoredCredential(cred));
  }
  store->UpdateLogins(std::move(stored_updated_credentials));
  WaitForPasswordStore();

  testing::Mock::VerifyAndClearExpectations(&mock_observer);

  MockPasswordStoreConsumer mock_consumer;

  EXPECT_CALL(
      mock_consumer,
      OnGetPasswordStoreResultsOrErrorFrom(
          store.get(),
          VariantWith<LoginsResult>(UnorderedElementsAreArray(
              StoredCredentialsIgnoringPrimaryKey(updated_credentials)))));
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
  store->Init();

  StoredCredential test_cred =
      FillStoredCredentialWithData(kTestCredential, /*is_account_store=*/false);
  store->AddLogin(std::move(test_cred));
  WaitForPasswordStore();

  MockPasswordStoreObserver mock_observer;
  store->AddObserver(&mock_observer);

  EXPECT_CALL(mock_observer, OnLoginsChanged(_, testing::SizeIs(1u)));
  base::test::TestFuture<bool> completion_future;

  store->RemoveLoginsCreatedBetween(FROM_HERE,
                                    base::Time::FromSecondsSinceUnixEpoch(0),
                                    base::Time::FromSecondsSinceUnixEpoch(2),
                                    completion_future.GetCallback());
  WaitForPasswordStore();

  EXPECT_TRUE(completion_future.IsReady());
  EXPECT_TRUE(completion_future.Take());
  testing::Mock::VerifyAndClearExpectations(&mock_observer);

  store->RemoveObserver(&mock_observer);
  store->ShutdownOnUIThread();
}

TEST_F(PasswordStoreTest,
       RemoveLoginsCreatedBetweenCompletedSuccessfullyWithEmtpyListOfChanges) {
  auto [store, mock_backend] = CreateUnownedStoreWithOwnedMockBackend();
  EXPECT_CALL(*mock_backend, InitBackend)
      .WillOnce(WithArg<2>([](base::OnceCallback<void(bool)> completion) {
        std::move(completion).Run(true);
      }));
  store->Init();

  base::test::TestFuture<bool> completion_future;
  EXPECT_CALL(*mock_backend, RemoveLoginsCreatedBetweenAsync)
      .WillOnce(WithArg<3>([](PasswordChangesOrErrorReply reply) {
        std::move(reply).Run(PasswordStoreChangeList());
      }));
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
      .WillOnce(WithArg<2>([](base::OnceCallback<void(bool)> completion) {
        std::move(completion).Run(true);
      }));
  store->Init();

  base::test::TestFuture<bool> completion_future;
  MockPasswordStoreObserver mock_observer;
  store->AddObserver(&mock_observer);
  EXPECT_CALL(*mock_backend, RemoveLoginsCreatedBetweenAsync)
      .WillOnce(WithArg<3>([](PasswordChangesOrErrorReply reply) -> void {
        std::move(reply).Run(PasswordChangesOrError(kBackendError));
      }));
  EXPECT_CALL(mock_observer,
              OnErrorStateChanged(store.get(), ActionableError::kInactionable));
  store->RemoveLoginsCreatedBetween(FROM_HERE,
                                    base::Time::FromSecondsSinceUnixEpoch(0),
                                    base::Time::FromSecondsSinceUnixEpoch(2),
                                    completion_future.GetCallback());
  EXPECT_FALSE(completion_future.Take());

  store->RemoveObserver(&mock_observer);
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
  store->Init();
  StoredCredential test_cred = FillStoredCredentialWithData(
      kTestCredentials, /*is_account_store=*/false);
  store->AddLogin(CloneStoredCredential(test_cred));
  WaitForPasswordStore();

  MockPasswordStoreObserver mock_observer;
  store->AddObserver(&mock_observer);

  // Expect a notification after adding a credential.
  EXPECT_CALL(mock_observer, OnLoginsChanged);
  test_cred.password_issues = {
      {InsecureType::kLeaked,
       InsecurityMetadata(base::Time::FromTimeT(1), IsMuted(false),
                          TriggerBackendNotification(false))}};
  store->UpdateLogin(std::move(test_cred));

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
  store->Init();
  StoredCredential test_cred = FillStoredCredentialWithData(
      kTestCredentials, /*is_account_store=*/false);
  test_cred.password_issues = {
      {InsecureType::kLeaked,
       InsecurityMetadata(base::Time::FromTimeT(1), IsMuted(false),
                          TriggerBackendNotification(false))}};
  store->AddLogin(CloneStoredCredential(test_cred));
  WaitForPasswordStore();

  MockPasswordStoreObserver mock_observer;
  store->AddObserver(&mock_observer);

  // Expect a notification after removing a credential.
  EXPECT_CALL(mock_observer, OnLoginsChanged);
  test_cred.password_issues.clear();
  store->UpdateLogin(std::move(test_cred));

  WaitForPasswordStore();

  store->RemoveObserver(&mock_observer);
  store->ShutdownOnUIThread();
}

TEST_F(PasswordStoreTest, PasswordManagerTimeSinceInitMetric) {
  base::HistogramTester histogram_tester;
  scoped_refptr<PasswordStore> store = CreatePasswordStore();
  store->Init();

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

TEST_F(PasswordStoreTest, DelegatesGetAllLoginsToBackend) {
  auto [store, mock_backend] = CreateUnownedStoreWithOwnedMockBackend();
  EXPECT_CALL(*mock_backend, InitBackend)
      .WillOnce(WithArg<2>([](base::OnceCallback<void(bool)> completion) {
        std::move(completion).Run(true);
      }));
  store->Init();

  MockPasswordStoreConsumer mock_consumer;
  EXPECT_CALL(*mock_backend, GetAllLoginsAsync(_));
  store->GetAllLogins(mock_consumer.GetWeakPtr());
  WaitForPasswordStore();
  store->ShutdownOnUIThread();
}

TEST_F(PasswordStoreTest, DelegatesGetAutofillableLoginsToBackend) {
  auto [store, mock_backend] = CreateUnownedStoreWithOwnedMockBackend();
  EXPECT_CALL(*mock_backend, InitBackend)
      .WillOnce(WithArg<2>([](base::OnceCallback<void(bool)> completion) {
        std::move(completion).Run(true);
      }));
  store->Init();

  MockPasswordStoreConsumer mock_consumer;
  EXPECT_CALL(*mock_backend, GetAutofillableLoginsAsync(_));
  store->GetAutofillableLogins(mock_consumer.GetWeakPtr());
  WaitForPasswordStore();
  store->ShutdownOnUIThread();
}

TEST_F(PasswordStoreTest, CallOnLoginsChangedIfRemovalProvidesChanges) {
  const StoredCredential kTestForm = MakeStoredCredential(kTestWebRealm1);
  MockPasswordStoreObserver mock_observer;
  auto [store, mock_backend] = CreateUnownedStoreWithOwnedMockBackend();
  EXPECT_CALL(*mock_backend, InitBackend)
      .WillOnce(WithArg<2>([](base::OnceCallback<void(bool)> completion) {
        std::move(completion).Run(true);
      }));
  store->Init();
  store->AddObserver(&mock_observer);

  // Expect that observers receive the removal when the backend invokes the
  // reply with a `PasswordStoreChangeList`.
  EXPECT_CALL(*mock_backend,
              RemoveLoginAsync(_,
                               MatchesCredential(CopyableStoredCredential(
                                   CloneStoredCredential(kTestForm))),
                               _))
      .WillOnce(WithArg<2>([&](PasswordChangesOrErrorReply reply) -> void {
        std::move(reply).Run(CreateChangeList(
            PasswordStoreChange::REMOVE, CloneStoredCredential(kTestForm)));
      }));
  EXPECT_CALL(mock_observer, OnLoginsRetained).Times(0);
  EXPECT_CALL(mock_observer,
              OnLoginsChanged(store.get(), ElementsAre(EqRemoval(kTestForm))));
  store->RemoveLogin(FROM_HERE, CloneStoredCredential(kTestForm));
  WaitForPasswordStore();

  store->RemoveObserver(&mock_observer);
  store->ShutdownOnUIThread();
}

TEST_F(PasswordStoreTest, DoNotCallOnLoginsChangedIfRemovalReturnsError) {
  const StoredCredential kTestForm = MakeStoredCredential(kTestWebRealm1);
  MockPasswordStoreObserver mock_observer;
  auto [store, mock_backend] = CreateUnownedStoreWithOwnedMockBackend();
  EXPECT_CALL(*mock_backend, InitBackend)
      .WillOnce(WithArg<2>([](base::OnceCallback<void(bool)> completion) {
        std::move(completion).Run(true);
      }));
  store->Init();
  store->AddObserver(&mock_observer);

  // Expect that observers does not receive the removal when backend fails.
  EXPECT_CALL(*mock_backend,
              RemoveLoginAsync(_,
                               MatchesCredential(CopyableStoredCredential(
                                   CloneStoredCredential(kTestForm))),
                               _))
      .WillOnce(WithArg<2>([&](PasswordChangesOrErrorReply reply) -> void {
        std::move(reply).Run(PasswordChangesOrError(kBackendError));
      }));
  EXPECT_CALL(mock_observer, OnLoginsRetained).Times(0);
  EXPECT_CALL(mock_observer, OnLoginsChanged).Times(0);
  EXPECT_CALL(mock_observer,
              OnErrorStateChanged(store.get(), ActionableError::kInactionable));
  store->RemoveLogin(FROM_HERE, CloneStoredCredential(kTestForm));
  WaitForPasswordStore();

  store->RemoveObserver(&mock_observer);
  store->ShutdownOnUIThread();
}

TEST_F(PasswordStoreTest, CallOnLoginsChangedIfAdditionProvidesChanges) {
  const StoredCredential kTestForm = MakeStoredCredential(kTestWebRealm1);
  MockPasswordStoreObserver mock_observer;
  auto [store, mock_backend] = CreateUnownedStoreWithOwnedMockBackend();
  EXPECT_CALL(*mock_backend, InitBackend)
      .WillOnce(WithArg<2>([](base::OnceCallback<void(bool)> completion) {
        std::move(completion).Run(true);
      }));
  store->Init();
  store->AddObserver(&mock_observer);

  // Expect that observers receive the addition when the backend invokes the
  // reply with a `PasswordStoreChangeList`.
  EXPECT_CALL(*mock_backend,
              AddLoginAsync(MatchesCredential(CopyableStoredCredential(
                                CloneStoredCredential(kTestForm))),
                            _))
      .WillOnce(WithArg<1>([&](PasswordChangesOrErrorReply reply) -> void {
        std::move(reply).Run(CreateChangeList(
            PasswordStoreChange::ADD, CloneStoredCredential(kTestForm)));
      }));
  EXPECT_CALL(mock_observer, OnLoginsRetained).Times(0);
  EXPECT_CALL(mock_observer,
              OnLoginsChanged(store.get(), ElementsAre(EqAddition(kTestForm))));
  store->AddLogin(CloneStoredCredential(kTestForm));
  WaitForPasswordStore();

  store->RemoveObserver(&mock_observer);
  store->ShutdownOnUIThread();
}

TEST_F(PasswordStoreTest, CallOnLoginsChangedIfUpdateProvidesChanges) {
  const StoredCredential kTestForm = MakeStoredCredential(kTestWebRealm1);
  MockPasswordStoreObserver mock_observer;
  auto [store, mock_backend] = CreateUnownedStoreWithOwnedMockBackend();
  EXPECT_CALL(*mock_backend, InitBackend)
      .WillOnce(WithArg<2>([](base::OnceCallback<void(bool)> completion) {
        std::move(completion).Run(true);
      }));
  store->Init();
  store->AddObserver(&mock_observer);

  // Expect that observers receive the update when the backend invokes the
  // reply with a `PasswordStoreChangeList`.
  EXPECT_CALL(*mock_backend,
              UpdateLoginAsync(MatchesCredential(CopyableStoredCredential(
                                   CloneStoredCredential(kTestForm))),
                               _))
      .WillOnce(WithArg<1>([&](PasswordChangesOrErrorReply reply) -> void {
        std::move(reply).Run(CreateChangeList(
            PasswordStoreChange::UPDATE, CloneStoredCredential(kTestForm)));
      }));
  EXPECT_CALL(mock_observer, OnLoginsRetained).Times(0);
  EXPECT_CALL(mock_observer,
              OnLoginsChanged(store.get(), ElementsAre(EqUpdate(kTestForm))));
  store->UpdateLogin(CloneStoredCredential(kTestForm));
  WaitForPasswordStore();

  store->RemoveObserver(&mock_observer);
  store->ShutdownOnUIThread();
}

TEST_F(PasswordStoreTest, DoNotCallOnLoginsChangedIfAdditionReturnsError) {
  const StoredCredential kTestForm = MakeStoredCredential(kTestWebRealm1);
  MockPasswordStoreObserver mock_observer;
  auto [store, mock_backend] = CreateUnownedStoreWithOwnedMockBackend();
  EXPECT_CALL(*mock_backend, InitBackend)
      .WillOnce(WithArg<2>([](base::OnceCallback<void(bool)> completion) {
        std::move(completion).Run(true);
      }));
  store->Init();
  store->AddObserver(&mock_observer);

  // Expect that observers does not receive the change when backend fails.
  EXPECT_CALL(*mock_backend,
              AddLoginAsync(MatchesCredential(CopyableStoredCredential(
                                CloneStoredCredential(kTestForm))),
                            _))
      .WillOnce(WithArg<1>([&](PasswordChangesOrErrorReply reply) -> void {
        std::move(reply).Run(PasswordChangesOrError(kBackendError));
      }));
  EXPECT_CALL(mock_observer, OnLoginsRetained).Times(0);
  EXPECT_CALL(mock_observer, OnLoginsChanged).Times(0);
  EXPECT_CALL(mock_observer,
              OnErrorStateChanged(store.get(), ActionableError::kInactionable));
  store->AddLogin(CloneStoredCredential(kTestForm));
  WaitForPasswordStore();

  store->RemoveObserver(&mock_observer);
  store->ShutdownOnUIThread();
}

TEST_F(PasswordStoreTest,
       DoNotCallOnErrorStateChangedIfAdditionReturnsErrorAndFeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kPasswordStorePropagatesActionableErrors);

  const StoredCredential kTestForm = MakeStoredCredential(kTestWebRealm1);
  MockPasswordStoreObserver mock_observer;
  auto [store, mock_backend] = CreateUnownedStoreWithOwnedMockBackend();
  EXPECT_CALL(*mock_backend, InitBackend)
      .WillOnce(WithArg<2>([](base::OnceCallback<void(bool)> completion) {
        std::move(completion).Run(true);
      }));
  store->Init();
  store->AddObserver(&mock_observer);

  // Expect that observers does not receive the change when backend fails.
  EXPECT_CALL(*mock_backend,
              AddLoginAsync(MatchesCredential(CopyableStoredCredential(
                                CloneStoredCredential(kTestForm))),
                            _))
      .WillOnce(WithArg<1>([&](PasswordChangesOrErrorReply reply) -> void {
        std::move(reply).Run(PasswordChangesOrError(kBackendError));
      }));
  EXPECT_CALL(mock_observer, OnLoginsRetained).Times(0);
  EXPECT_CALL(mock_observer, OnLoginsChanged).Times(0);
  EXPECT_CALL(mock_observer, OnErrorStateChanged).Times(0);
  store->AddLogin(CloneStoredCredential(kTestForm));
  WaitForPasswordStore();

  store->RemoveObserver(&mock_observer);
  store->ShutdownOnUIThread();
}

TEST_F(PasswordStoreTest, DoNotCallOnLoginsChangedIfUpdateReturnsError) {
  const StoredCredential kTestForm = MakeStoredCredential(kTestWebRealm1);
  MockPasswordStoreObserver mock_observer;
  auto [store, mock_backend] = CreateUnownedStoreWithOwnedMockBackend();
  EXPECT_CALL(*mock_backend, InitBackend)
      .WillOnce(WithArg<2>([](base::OnceCallback<void(bool)> completion) {
        std::move(completion).Run(true);
      }));
  store->Init();
  store->AddObserver(&mock_observer);

  // Expect that observers does not receive the update when backend fails.
  EXPECT_CALL(*mock_backend,
              UpdateLoginAsync(MatchesCredential(CopyableStoredCredential(
                                   CloneStoredCredential(kTestForm))),
                               _))
      .WillOnce(WithArg<1>([&](PasswordChangesOrErrorReply reply) -> void {
        std::move(reply).Run(PasswordChangesOrError(kBackendError));
      }));
  EXPECT_CALL(mock_observer, OnLoginsRetained).Times(0);
  EXPECT_CALL(mock_observer, OnLoginsChanged).Times(0);
  EXPECT_CALL(mock_observer,
              OnErrorStateChanged(store.get(), ActionableError::kInactionable));
  store->UpdateLogin(CloneStoredCredential(kTestForm));
  WaitForPasswordStore();

  store->RemoveObserver(&mock_observer);
  store->ShutdownOnUIThread();
}

TEST_F(PasswordStoreTest, AbleToSavePasswords) {
  auto [store, mock_backend] = CreateUnownedStoreWithOwnedMockBackend();
  EXPECT_CALL(*mock_backend, InitBackend)
      .WillOnce(WithArg<2>([](base::OnceCallback<void(bool)> completion) {
        std::move(completion).Run(true);
      }));
  store->Init();
  EXPECT_CALL(*mock_backend, GetError)
      .WillOnce(testing::Return(ActionableError::kNoError));

  EXPECT_EQ(store->GetError(), ActionableError::kNoError);
  store->ShutdownOnUIThread();
}

TEST_F(PasswordStoreTest, NotAbleToSavePasswords) {
  auto [store, mock_backend] = CreateUnownedStoreWithOwnedMockBackend();
  EXPECT_CALL(*mock_backend, InitBackend)
      .WillOnce(WithArg<2>([](base::OnceCallback<void(bool)> completion) {
        std::move(completion).Run(true);
      }));
  store->Init();
  EXPECT_CALL(*mock_backend, GetError)
      .WillOnce(testing::Return(ActionableError::kInactionable));

  EXPECT_EQ(store->GetError(), ActionableError::kInactionable);
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
  store->Init();

  std::vector<StoredCredential> all_credentials;
  for (const auto& test_credential : kTestCredentials) {
    all_credentials.push_back(FillStoredCredentialWithData(
        test_credential, /*is_account_store=*/false));
    store->AddLogin(CloneStoredCredential(all_credentials.back()));
  }

  MockPasswordStoreConsumer mock_consumer;
  std::vector<StoredCredential> expected_results;
  for (const auto& credential : all_credentials) {
    expected_results.push_back(CloneStoredCredential(credential));
  }

  EXPECT_CALL(
      mock_consumer,
      OnGetPasswordStoreResultsOrErrorFrom(
          _, VariantWith<LoginsResult>(UnorderedElementsAreArray(
                 StoredCredentialsIgnoringPrimaryKey(expected_results)))));
  store->GetAllLogins(mock_consumer.GetWeakPtr());
  WaitForPasswordStore();
  store->ShutdownOnUIThread();
}

TEST_F(PasswordStoreTest, TestGetLoginRequestCancelable) {
  scoped_refptr<PasswordStore> store = CreatePasswordStore();
  store->Init();
  WaitForPasswordStore();

  PasswordFormDigest observed_form = {PasswordForm::Scheme::kHtml,
                                      kTestWebRealm1, GURL(kTestWebRealm1)};

  MockPasswordStoreConsumer mock_consumer;
  EXPECT_CALL(mock_consumer, OnGetPasswordStoreResultsOrErrorFrom).Times(0);
  store->GetLogins(observed_form, mock_consumer.GetWeakPtr());
  mock_consumer.CancelAllRequests();
  WaitForPasswordStore();

  store->ShutdownOnUIThread();
}

TEST_F(PasswordStoreTest, TestUnblockListEmptyStore) {
  scoped_refptr<PasswordStore> store = CreatePasswordStore();
  store->Init();
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

TEST_F(PasswordStoreTest, Unblocklisting) {
  scoped_refptr<PasswordStore> store = CreatePasswordStore();
  store->Init();

  PasswordFormDigest observed_form_digest = {
      PasswordForm::Scheme::kHtml, kTestWebRealm1, GURL(kTestWebOrigin1)};

  StoredCredential blocklisted_form;
  blocklisted_form.signon_realm = kTestWebRealm1;
  blocklisted_form.url = GURL(kTestWebOrigin1);
  blocklisted_form.blocked_by_user = true;
  blocklisted_form.in_store = PasswordForm::Store::kProfileStore;

  StoredCredential non_blocklisted_form;
  non_blocklisted_form.signon_realm = kTestWebRealm1;
  non_blocklisted_form.url = GURL(kTestWebOrigin1);
  non_blocklisted_form.username_value = u"username";
  non_blocklisted_form.password_value = u"password";

  store->AddLogin(CloneStoredCredential(blocklisted_form));
  store->AddLogin(CloneStoredCredential(non_blocklisted_form));
  WaitForPasswordStore();

  MockPasswordStoreObserver mock_observer;
  store->AddObserver(&mock_observer);

  // Expect `OnLoginsChanged` to be called with the removed blocklisted form.
  EXPECT_CALL(
      mock_observer,
      OnLoginsChanged(store.get(), ElementsAre(EqRemoval(blocklisted_form))));

  base::test::TestFuture<void> completion_future;
  store->Unblocklist(observed_form_digest, completion_future.GetCallback());
  EXPECT_TRUE(completion_future.Wait());

  store->RemoveObserver(&mock_observer);
  store->ShutdownOnUIThread();
}

TEST_F(PasswordStoreTest, UpdateLoginWithPrimaryKey_PasswordChanges) {
  scoped_refptr<PasswordStore> store = CreatePasswordStore();
  store->Init();

  StoredCredential old_cred;
  old_cred.signon_realm = kTestWebRealm1;
  old_cred.url = GURL(kTestWebOrigin1);
  old_cred.username_value = u"username";
  old_cred.password_value = u"password";
  old_cred.password_issues = {{InsecureType::kLeaked, InsecurityMetadata()},
                              {InsecureType::kPhished, InsecurityMetadata()},
                              {InsecureType::kWeak, InsecurityMetadata()}};

  StoredCredential new_cred = CloneStoredCredential(old_cred);
  new_cred.password_value = u"new_password";

  StoredCredential expected_new_cred = CloneStoredCredential(new_cred);
  expected_new_cred.password_issues.clear();

  store->AddLogin(CloneStoredCredential(old_cred));
  WaitForPasswordStore();

  MockPasswordStoreObserver mock_observer;
  store->AddObserver(&mock_observer);

  EXPECT_CALL(mock_observer,
              OnLoginsChanged(store.get(), UnorderedElementsAre(
                                               EqRemoval(old_cred),
                                               EqAddition(expected_new_cred))));

  base::test::TestFuture<void> completion_future;
  store->UpdateLoginWithPrimaryKey(std::move(new_cred), old_cred,
                                   completion_future.GetCallback());
  EXPECT_TRUE(completion_future.Wait());

  store->RemoveObserver(&mock_observer);
  store->ShutdownOnUIThread();
}

TEST_F(PasswordStoreTest, UpdateLoginWithPrimaryKey_UsernameChanges) {
  scoped_refptr<PasswordStore> store = CreatePasswordStore();
  store->Init();

  StoredCredential old_cred;
  old_cred.signon_realm = kTestWebRealm1;
  old_cred.url = GURL(kTestWebOrigin1);
  old_cred.username_value = u"username";
  old_cred.password_value = u"password";
  old_cred.password_issues = {{InsecureType::kLeaked, InsecurityMetadata()},
                              {InsecureType::kPhished, InsecurityMetadata()},
                              {InsecureType::kWeak, InsecurityMetadata()}};

  StoredCredential new_cred = CloneStoredCredential(old_cred);
  new_cred.username_value = u"new_username";

  StoredCredential expected_new_cred = CloneStoredCredential(new_cred);
  expected_new_cred.password_issues = {
      {InsecureType::kWeak, InsecurityMetadata()}};

  store->AddLogin(CloneStoredCredential(old_cred));
  WaitForPasswordStore();

  MockPasswordStoreObserver mock_observer;
  store->AddObserver(&mock_observer);

  EXPECT_CALL(mock_observer,
              OnLoginsChanged(store.get(), UnorderedElementsAre(
                                               EqRemoval(old_cred),
                                               EqAddition(expected_new_cred))));

  base::test::TestFuture<void> completion_future;
  store->UpdateLoginWithPrimaryKey(std::move(new_cred), old_cred,
                                   completion_future.GetCallback());
  EXPECT_TRUE(completion_future.Wait());

  store->RemoveObserver(&mock_observer);
  store->ShutdownOnUIThread();
}

#if BUILDFLAG(IS_ANDROID)
// Tests that when foreground transitions trigger a refresh (which starts by
// running the remote changes callback with std::nullopt and results in an async
// call to GetAllLoginsAsync), the password store does not notify observers that
// the error is resolved (with ActionableError::kNoError) before it
// executes/completes the query. If the query subsequently fails with an error,
// the store must correctly propagate the failure (e.g.
// ActionableError::kInactionable) to observers.
TEST_F(PasswordStoreTest,
       OnErrorStateChangedFlowOnAndroidForegroundRefreshFailure) {
  base::test::ScopedFeatureList feature_list(
      features::kPasswordStorePropagatesActionableErrors);

  MockPasswordStoreObserver mock_observer;
  auto [store, mock_backend] = CreateUnownedStoreWithOwnedMockBackend();

  PasswordStoreBackend::RemoteChangesReceived remote_form_changes_received;
  EXPECT_CALL(*mock_backend, InitBackend)
      .WillOnce(testing::WithArgs<0, 2>(
          [&](PasswordStoreBackend::RemoteChangesReceived remote_changes,
              base::OnceCallback<void(bool)> completion) {
            remote_form_changes_received = std::move(remote_changes);
            std::move(completion).Run(true);
          }));

  store->Init();
  store->AddObserver(&mock_observer);

  EXPECT_CALL(*mock_backend, GetAllLoginsAsync)
      .WillOnce(testing::WithArg<0>([&](BackendLoginsOrErrorReply callback) {
        std::move(callback).Run(kBackendError);
      }));

  EXPECT_CALL(mock_observer,
              OnErrorStateChanged(store.get(), ActionableError::kNoError))
      .Times(0);
  EXPECT_CALL(mock_observer,
              OnErrorStateChanged(store.get(), ActionableError::kInactionable));

  remote_form_changes_received.Run(std::nullopt);

  WaitForPasswordStore();

  store->RemoveObserver(&mock_observer);
  store->ShutdownOnUIThread();
}
#endif

// Collection of origin-related testcases common to all platform-specific
// stores.
class PasswordStoreOriginTest : public PasswordStoreTest {
 public:
  void SetUp() override {
    PasswordStoreTest::SetUp();
    store_ = CreatePasswordStore();
    store_->Init();
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
  StoredCredential cred = FillStoredCredentialWithData(
      CreateTestPasswordFormDataByOrigin("http://foo.example.com/"),
      /*is_account_store=*/false);

  base::OnceCallback<void(bool)> init_callback;
  EXPECT_CALL(*backend(), InitBackend).WillOnce(MoveArg<2>(&init_callback));
  store()->Init();

  base::MockOnceClosure mock_callback;
  store()->AddLogin(std::move(cred), mock_callback.Get());

  EXPECT_CALL(mock_callback, Run).Times(0);
  RunUntilIdle();

  EXPECT_CALL(*backend(), AddLoginAsync)
      .WillOnce(base::test::RunOnceCallback<1>(PasswordChanges()));
  EXPECT_CALL(mock_callback, Run);
  std::move(init_callback).Run(true);
}

TEST_F(PasswordStoreDelayedInitTest, UpdateLogin) {
  StoredCredential cred = FillStoredCredentialWithData(
      CreateTestPasswordFormDataByOrigin("http://foo.example.com/"),
      /*is_account_store=*/false);

  base::OnceCallback<void(bool)> init_callback;
  EXPECT_CALL(*backend(), InitBackend).WillOnce(MoveArg<2>(&init_callback));
  store()->Init();

  base::MockOnceClosure mock_callback;
  store()->UpdateLogin(std::move(cred), mock_callback.Get());

  EXPECT_CALL(mock_callback, Run).Times(0);
  RunUntilIdle();

  EXPECT_CALL(*backend(), UpdateLoginAsync)
      .WillOnce(base::test::RunOnceCallback<1>(PasswordChanges()));
  EXPECT_CALL(mock_callback, Run);
  std::move(init_callback).Run(true);
}

TEST_F(PasswordStoreDelayedInitTest, GetAutofillableLogins) {
  StoredCredential cred = FillStoredCredentialWithData(
      CreateTestPasswordFormDataByOrigin("http://foo.example.com/"),
      /*is_account_store=*/false);

  base::OnceCallback<void(bool)> init_callback;
  EXPECT_CALL(*backend(), InitBackend).WillOnce(MoveArg<2>(&init_callback));
  store()->Init();

  MockPasswordStoreConsumer mock_consumer;
  store()->GetAutofillableLogins(mock_consumer.GetWeakPtr());

  EXPECT_CALL(mock_consumer, OnGetPasswordStoreResultsOrErrorFrom).Times(0);
  RunUntilIdle();

  EXPECT_CALL(*backend(), GetAutofillableLoginsAsync)
      .WillOnce(base::test::RunOnceCallback<0>(BackendLoginsResult()));
  EXPECT_CALL(mock_consumer, OnGetPasswordStoreResultsOrErrorFrom);
  std::move(init_callback).Run(true);
}

}  // namespace password_manager
