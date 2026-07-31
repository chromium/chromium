// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store/password_store_built_in_backend.h"

#include <cstddef>
#include <memory>
#include <utility>
#include <variant>

#include "base/check.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/types/pass_key.h"
#include "build/build_config.h"
#include "components/affiliations/core/browser/fake_affiliation_service.h"
#include "components/os_crypt/async/browser/test_utils.h"
#include "components/os_crypt/async/common/encryptor.h"
#include "components/password_manager/core/browser/affiliation/affiliated_match_helper.h"
#include "components/password_manager/core/browser/affiliation/mock_affiliated_match_helper.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_store/login_database.h"
#include "components/password_manager/core/browser/password_store/login_database_async_helper.h"
#include "components/password_manager/core/browser/password_store/password_form_converters.h"
#include "components/password_manager/core/browser/password_store/password_store_backend.h"
#include "components/password_manager/core/browser/password_store/password_store_change.h"
#include "components/password_manager/core/browser/password_store/password_store_consumer.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync/test/mock_sync_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ElementsAre;
using testing::ElementsAreArray;
using testing::Field;
using testing::Optional;
using testing::Property;
using testing::Return;
using testing::UnorderedElementsAreArray;
using testing::VariantWith;

namespace password_manager {

namespace {

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
constexpr PasswordFormData kTestCredentials[] = {
    {PasswordForm::Scheme::kHtml, kTestAndroidRealm1, "", "", u"", u"", u"",
     u"username_value_1", u"", kTestLastUsageTime, 1},
    {PasswordForm::Scheme::kHtml, kTestAndroidRealm2, "", "", u"", u"", u"",
     u"username_value_2", u"", kTestLastUsageTime, 1},
    {PasswordForm::Scheme::kHtml, kTestAndroidRealm3, "", "", u"", u"", u"",
     u"username_value_3", u"", kTestLastUsageTime, 1},
    {PasswordForm::Scheme::kHtml, kTestWebRealm1, kTestWebOrigin1, "", u"", u"",
     u"", u"username_value_4", u"", kTestLastUsageTime, 1},
    // A PasswordFormData with nullptr as the username_value will be converted
    // in a blocklisted PasswordForm in FillPasswordFormWithData().
    {PasswordForm::Scheme::kHtml, kTestWebRealm2, kTestWebOrigin2, "", u"", u"",
     u"", nullptr, u"", kTestLastUsageTime, 1},
    {PasswordForm::Scheme::kHtml, kTestWebRealm3, kTestWebOrigin3, "", u"", u"",
     u"", nullptr, u"", kTestLastUsageTime, 1}};
constexpr auto kLatencyDelta = base::Milliseconds(123u);
constexpr auto kStart = base::Time::FromTimeT(1000);
constexpr auto kEnd = base::Time::FromTimeT(2000);
constexpr const char kTestAndroidName1[] = "Example Android App 1";
constexpr const char kTestAndroidIconURL1[] = "https://example.com/icon_1.png";
constexpr const char kTestAndroidName2[] = "Example Android App 2";
constexpr const char kTestAndroidIconURL2[] = "https://example.com/icon_2.png";

class MockPasswordStoreBackendTester {
 public:
  MOCK_METHOD(void, LoginsReceivedConstRef, (const LoginsResult&));

  void HandleLoginsOrError(LoginsResultOrError results) {
    LoginsReceivedConstRef(std::move(std::get<LoginsResult>(results)));
  }
};

// A mock LoginDatabase that simulates a failing Init() method.
class BadLoginDatabase : public LoginDatabase {
 public:
  BadLoginDatabase(bool is_account_store)
      : LoginDatabase(base::FilePath(),
                      password_manager::IsAccountStore(is_account_store)) {}

  BadLoginDatabase(const BadLoginDatabase&) = delete;
  BadLoginDatabase& operator=(const BadLoginDatabase&) = delete;

  // LoginDatabase:
  bool Init(base::RepeatingCallback<void(password_manager::IsAccountStore)>
                on_undecryptable_passwords_removed,
            scoped_refptr<os_crypt_async::Encryptor> encryptor) override {
    return false;
  }
};

PasswordFormData CreateTestPasswordFormData() {
  PasswordFormData data = {PasswordForm::Scheme::kHtml,
                           "http://bar.example.com",
                           "http://bar.example.com/origin",
                           "http://bar.example.com/action",
                           u"submit_element",
                           u"username_element",
                           u"password_element",
                           u"username_value",
                           u"password_value",
                           true,
                           1};
  return data;
}

MATCHER_P(MatchesFormsIgnoringPrimaryKey, expected_forms, "") {
  std::vector<PasswordForm> actual_forms;
  for (const auto& cred : arg) {
    actual_forms.push_back(ToPasswordForm(cred));
  }
  return ExplainMatchResult(
      UnorderedElementsAreArray(FormsIgnoringPrimaryKey(expected_forms)),
      actual_forms, result_listener);
}

}  // anonymous namespace

class PasswordStoreBuiltInBackendBaseTest : public testing::Test {
 public:
  PasswordStoreBuiltInBackendBaseTest() = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    pref_service_.registry()->RegisterBooleanPref(
        prefs::kClearingUndecryptablePasswords, false);
    pref_service_.registry()->RegisterIntegerPref(
        password_manager::prefs::kPasswordRemovalReasonForAccount, 0);
    pref_service_.registry()->RegisterIntegerPref(
        password_manager::prefs::kPasswordRemovalReasonForProfile, 0);
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_IOS)
    pref_service_.registry()->RegisterBooleanPref(
        prefs::kDeletingUndecryptablePasswordsEnabled, true);
#endif
  }

  void TearDown() override {
    if (store_) {
      PasswordStoreBackend* backend = store_.get();
      backend->Shutdown(base::BindOnce(
          [](std::unique_ptr<PasswordStoreBackend> backend) {
            backend.reset();
          },
          std::move(store_)));
      RunUntilIdle();
    }
    ASSERT_TRUE(temp_dir_.Delete());
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  void AdvanceClock(base::TimeDelta millis) {
    // AdvanceClock is used here because FastForwardBy doesn't work for the
    // intended purpose. FastForwardBy performs the queued actions first and
    // then makes the clock tick and for the tests that follow we want to
    // advance the clock before certain async tasks happen.
    task_environment_.AdvanceClock(millis);
  }

 protected:
  base::FilePath test_login_db_file_path() const {
    return temp_dir_.GetPath().Append(FILE_PATH_LITERAL("login_test"));
  }
  TestingPrefServiceSimple* pref_service() { return &pref_service_; }

  std::unique_ptr<PasswordStoreBuiltInBackend> store_;
  affiliations::FakeAffiliationService fake_affiliation_service_;

 private:
  void SetupTempDir();

  void ClosePasswordStore();

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  base::ScopedTempDir temp_dir_;
  TestingPrefServiceSimple pref_service_;
};

class PasswordStoreBuiltInBackendTest
    : public testing::WithParamInterface<bool>,
      public PasswordStoreBuiltInBackendBaseTest {
 public:
  PasswordStoreBuiltInBackendTest() {
    os_crypt_async_ = os_crypt_async::GetTestOSCryptAsyncForTesting(
        /*is_sync_for_unittests=*/true);
  }

  PasswordStoreBuiltInBackend* CreateBackend(
      std::unique_ptr<LoginDatabase> database = nullptr,
      std::unique_ptr<AffiliatedMatchHelper> affiliated_match_helper =
          nullptr) {
    if (!database) {
      database = std::make_unique<LoginDatabase>(
          test_login_db_file_path(),
          password_manager::IsAccountStore(GetParam()));
    }

    store_ = std::make_unique<PasswordStoreBuiltInBackend>(
        std::move(database), syncer::WipeModelUponSyncDisabledBehavior::kNever,
        pref_service(), os_crypt_async_.get(),
        std::move(affiliated_match_helper));
    return store_.get();
  }

  void InitializeBackend(PasswordStoreBackend* backend) {
    backend->InitBackend(/*remote_form_changes_received=*/base::DoNothing(),
                         /*sync_enabled_or_disabled_cb=*/base::DoNothing(),
                         /*completion=*/base::DoNothing());
    RunUntilIdle();
  }

  void InitializeBackendWithSync(
      PasswordStoreBackend* backend,
      PasswordStoreBackend::RemoteChangesReceived remote_changes_callback,
      base::RepeatingClosure sync_enabled_or_disabled_cb,
      syncer::MockSyncService& sync_service) {
    EXPECT_CALL(
        sync_service,
        AddObserver(static_cast<PasswordStoreBuiltInBackend*>(backend)));
    backend->InitBackend(std::move(remote_changes_callback),
                         std::move(sync_enabled_or_disabled_cb),
                         /*completion=*/base::DoNothing());
    backend->OnSyncServiceInitialized(&sync_service);
    RunUntilIdle();
  }

 private:
  std::unique_ptr<os_crypt_async::OSCryptAsync> os_crypt_async_;
};

TEST_P(PasswordStoreBuiltInBackendTest,
       SyncServiceObservationUpdatesErrorState_NoErrorByDefault) {
#if BUILDFLAG(IS_IOS)
  if (!GetParam()) {
    GTEST_SKIP() << "On iOS, sync service is observed only for account store";
  }
#endif
  syncer::MockSyncService mock_sync_service;
  base::MockCallback<PasswordStoreBackend::RemoteChangesReceived>
      mock_remote_changes_callback;
  base::MockCallback<base::RepeatingClosure> mock_sync_enabled_or_disabled_cb;
  PasswordStoreBuiltInBackend* built_in_backend = CreateBackend();
  // Shorthands to expose overrides without casts:
  PasswordStoreBackend* as_backend = built_in_backend;
  syncer::SyncServiceObserver* as_sync_observer = built_in_backend;
  InitializeBackendWithSync(as_backend, mock_remote_changes_callback.Get(),
                            mock_sync_enabled_or_disabled_cb.Get(),
                            mock_sync_service);

  // Initial state: no error.
  EXPECT_CALL(mock_sync_service, GetUserActionableError())
      .WillOnce(Return(syncer::SyncService::UserActionableError::kNone));
  EXPECT_EQ(as_backend->GetError(), ActionableError::kNoError);

  // Cleans up the observer before mock_sync_service is destroyed.
  EXPECT_CALL(mock_sync_service, RemoveObserver(as_sync_observer));
  built_in_backend->OnSyncShutdown(&mock_sync_service);
}

TEST_P(PasswordStoreBuiltInBackendTest,
       SyncServiceObservationUpdatesErrorState_SignInNeeded) {
#if BUILDFLAG(IS_IOS)
  if (!GetParam()) {
    GTEST_SKIP() << "On iOS, sync service is observed only for account store";
  }
#endif
  syncer::MockSyncService mock_sync_service;
  base::MockCallback<PasswordStoreBackend::RemoteChangesReceived>
      mock_remote_changes_callback;
  base::MockCallback<base::RepeatingClosure> mock_sync_enabled_or_disabled_cb;
  PasswordStoreBuiltInBackend* built_in_backend = CreateBackend();
  // Shorthands to expose overrides without casts:
  PasswordStoreBackend* as_backend = built_in_backend;
  syncer::SyncServiceObserver* as_sync_observer = built_in_backend;
  InitializeBackendWithSync(as_backend, mock_remote_changes_callback.Get(),
                            mock_sync_enabled_or_disabled_cb.Get(),
                            mock_sync_service);
  // Change state to error: kSignInNeedsUpdate.
  EXPECT_CALL(mock_sync_service, GetUserActionableError())
      .WillRepeatedly(
          Return(syncer::SyncService::UserActionableError::kSignInNeedsUpdate));

  EXPECT_CALL(mock_remote_changes_callback,
              Run(VariantWith<PasswordStoreBackendError>(
                  Field(&PasswordStoreBackendError::type,
                        PasswordStoreBackendErrorType::kAuthErrorResolvable))));
  EXPECT_CALL(mock_sync_enabled_or_disabled_cb, Run());
  built_in_backend->OnStateChanged(&mock_sync_service);
  EXPECT_EQ(as_backend->GetError(), ActionableError::kSignInNeeded);

  EXPECT_CALL(mock_sync_service, RemoveObserver(as_sync_observer));
  built_in_backend->OnSyncShutdown(&mock_sync_service);
}

TEST_P(PasswordStoreBuiltInBackendTest,
       SyncServiceObservationUpdatesErrorState_NeedsPassphrase) {
#if BUILDFLAG(IS_IOS)
  if (!GetParam()) {
    GTEST_SKIP() << "On iOS, sync service is observed only for account store";
  }
#endif
  syncer::MockSyncService mock_sync_service;
  base::MockCallback<PasswordStoreBackend::RemoteChangesReceived>
      mock_remote_changes_callback;
  base::MockCallback<base::RepeatingClosure> mock_sync_enabled_or_disabled_cb;
  PasswordStoreBuiltInBackend* built_in_backend = CreateBackend();
  // Shorthands to expose overrides without casts:
  PasswordStoreBackend* as_backend = built_in_backend;
  syncer::SyncServiceObserver* as_sync_observer = built_in_backend;
  InitializeBackendWithSync(as_backend, mock_remote_changes_callback.Get(),
                            mock_sync_enabled_or_disabled_cb.Get(),
                            mock_sync_service);
  EXPECT_CALL(mock_sync_service, GetUserActionableError())
      .WillRepeatedly(
          Return(syncer::SyncService::UserActionableError::kNeedsPassphrase));

  EXPECT_CALL(mock_remote_changes_callback,
              Run(VariantWith<PasswordStoreBackendError>(
                  Field(&PasswordStoreBackendError::type,
                        PasswordStoreBackendErrorType::kNeedsPassphrase))));
  EXPECT_CALL(mock_sync_enabled_or_disabled_cb, Run());
  built_in_backend->OnStateChanged(&mock_sync_service);

  EXPECT_EQ(as_backend->GetError(), ActionableError::kNeedsPassphrase);

  EXPECT_CALL(mock_sync_service, RemoveObserver(as_sync_observer));
  built_in_backend->OnSyncShutdown(&mock_sync_service);
}

TEST_P(PasswordStoreBuiltInBackendTest,
       SyncServiceObservationUpdatesErrorState_NeedsTrustedVaultKey) {
#if BUILDFLAG(IS_IOS)
  if (!GetParam()) {
    GTEST_SKIP() << "On iOS, sync service is observed only for account store";
  }
#endif
  syncer::MockSyncService mock_sync_service;
  base::MockCallback<PasswordStoreBackend::RemoteChangesReceived>
      mock_remote_changes_callback;
  base::MockCallback<base::RepeatingClosure> mock_sync_enabled_or_disabled_cb;
  PasswordStoreBuiltInBackend* built_in_backend = CreateBackend();
  // Shorthands to expose overrides without casts:
  PasswordStoreBackend* as_backend = built_in_backend;
  syncer::SyncServiceObserver* as_sync_observer = built_in_backend;
  InitializeBackendWithSync(as_backend, mock_remote_changes_callback.Get(),
                            mock_sync_enabled_or_disabled_cb.Get(),
                            mock_sync_service);
  EXPECT_CALL(mock_sync_service, GetUserActionableError())
      .WillRepeatedly(Return(syncer::SyncService::UserActionableError::
                                 kNeedsTrustedVaultKeyForPasswords));

  EXPECT_CALL(mock_remote_changes_callback,
              Run(VariantWith<PasswordStoreBackendError>(Field(
                  &PasswordStoreBackendError::type,
                  PasswordStoreBackendErrorType::kKeyRetrievalRequired))));
  EXPECT_CALL(mock_sync_enabled_or_disabled_cb, Run());
  built_in_backend->OnStateChanged(&mock_sync_service);

  EXPECT_EQ(as_backend->GetError(), ActionableError::kTrustedVaultKeyNeeded);

  EXPECT_CALL(mock_sync_service, RemoveObserver(as_sync_observer));
  built_in_backend->OnSyncShutdown(&mock_sync_service);
}

#if BUILDFLAG(IS_IOS)
TEST_P(PasswordStoreBuiltInBackendTest,
       DoesNotObserveSyncServiceForProfileStoreOnIos) {
  if (GetParam()) {
    GTEST_SKIP() << "This test is only for the profile store";
  }
  syncer::MockSyncService mock_sync_service;
  PasswordStoreBuiltInBackend* built_in_backend = CreateBackend();
  PasswordStoreBackend* as_backend = built_in_backend;

  EXPECT_CALL(mock_sync_service, AddObserver(built_in_backend)).Times(0);

  as_backend->InitBackend(base::DoNothing(), base::DoNothing(),
                          base::DoNothing());
  as_backend->OnSyncServiceInitialized(&mock_sync_service);
  RunUntilIdle();

  built_in_backend->OnSyncShutdown(&mock_sync_service);
}

#endif

TEST_P(PasswordStoreBuiltInBackendTest, NonASCIIData) {
  PasswordStoreBackend* backend = CreateBackend();
  InitializeBackend(backend);

  // Some non-ASCII password form data.
  static const PasswordFormData form_data = {PasswordForm::Scheme::kHtml,
                                             "http://foo.example.com",
                                             "http://foo.example.com/origin",
                                             "http://foo.example.com/action",
                                             u"มีสีสัน",
                                             u"お元気ですか?",
                                             u"盆栽",
                                             u"أحب كرة",
                                             u"£éä국수çà",
                                             true,
                                             1};

  PasswordForm expected_form(*FillPasswordFormWithData(form_data, GetParam()));
  backend->AddLoginAsync(FromPasswordForm(expected_form), base::DoNothing());

  base::MockCallback<BackendLoginsOrErrorReply> mock_reply;
  EXPECT_CALL(
      mock_reply,
      Run(VariantWith<BackendLoginsResult>(MatchesFormsIgnoringPrimaryKey(
          std::vector<PasswordForm>{expected_form}))));
  backend->GetAutofillableLoginsAsync(mock_reply.Get());

  RunUntilIdle();
}

TEST_P(PasswordStoreBuiltInBackendTest, TestAddLoginAsync) {
  PasswordStoreBackend* backend = CreateBackend();
  InitializeBackend(backend);
  StoredCredential cred =
      FillStoredCredentialWithData(CreateTestPasswordFormData(), GetParam());

  const PasswordStoreChange add_change = PasswordStoreChange(
      PasswordStoreChange::ADD, CloneStoredCredential(cred));

  base::MockCallback<PasswordChangesOrErrorReply> mock_reply;
  EXPECT_CALL(
      mock_reply,
      Run(VariantWith<PasswordChanges>(Optional(ElementsAre(add_change)))));
  backend->AddLoginAsync(std::move(cred), mock_reply.Get());
  RunUntilIdle();
}

TEST_P(PasswordStoreBuiltInBackendTest, TestUpdateLoginAsync) {
  PasswordStoreBackend* backend = CreateBackend();
  InitializeBackend(backend);
  StoredCredential cred =
      FillStoredCredentialWithData(CreateTestPasswordFormData(), GetParam());

  backend->AddLoginAsync(CloneStoredCredential(cred), base::DoNothing());
  RunUntilIdle();

  cred.password_value =
      password_manager::PasswordString(u"a different password");
  const PasswordStoreChange update_change = PasswordStoreChange(
      PasswordStoreChange::UPDATE, CloneStoredCredential(cred),
      /*password_changed=*/true);

  base::MockCallback<PasswordChangesOrErrorReply> mock_reply;
  EXPECT_CALL(
      mock_reply,
      Run(VariantWith<PasswordChanges>(Optional(ElementsAre(update_change)))));
  backend->UpdateLoginAsync(std::move(cred), mock_reply.Get());
  RunUntilIdle();
}

TEST_P(PasswordStoreBuiltInBackendTest, TestRemoveLoginAsync) {
  PasswordStoreBackend* backend = CreateBackend();
  InitializeBackend(backend);
  StoredCredential cred =
      FillStoredCredentialWithData(CreateTestPasswordFormData(), GetParam());

  backend->AddLoginAsync(CloneStoredCredential(cred), base::DoNothing());
  RunUntilIdle();

  PasswordStoreChange remove_change = PasswordStoreChange(
      PasswordStoreChange::REMOVE, CloneStoredCredential(cred),
      /*password_changed=*/true);

  base::MockCallback<PasswordChangesOrErrorReply> mock_reply;
  EXPECT_CALL(
      mock_reply,
      Run(VariantWith<PasswordChanges>(Optional(ElementsAre(remove_change)))));
  backend->RemoveLoginAsync(FROM_HERE, std::move(cred), mock_reply.Get());
  RunUntilIdle();
}

TEST_P(PasswordStoreBuiltInBackendTest, GetAllLoginsAsync) {
  PasswordStoreBackend* backend = CreateBackend();
  InitializeBackend(backend);

  // Populate store with test credentials.
  std::vector<std::unique_ptr<PasswordForm>> all_credentials;
  base::MockCallback<PasswordChangesOrErrorReply> reply;
  EXPECT_CALL(reply, Run).Times(6);
  for (const auto& test_credential : kTestCredentials) {
    all_credentials.push_back(
        FillPasswordFormWithData(test_credential, GetParam()));
    backend->AddLoginAsync(FromPasswordForm(*all_credentials.back()),
                           reply.Get());
  }
  RunUntilIdle();

  // Verify that the store returns all test credentials.
  std::vector<PasswordForm> expected_results;
  for (const auto& credential : all_credentials) {
    expected_results.push_back(*credential);
  }
  base::MockCallback<BackendLoginsOrErrorReply> mock_reply;
  EXPECT_CALL(mock_reply,
              Run(VariantWith<BackendLoginsResult>(
                  MatchesFormsIgnoringPrimaryKey(expected_results))));
  backend->GetAllLoginsAsync(mock_reply.Get());

  RunUntilIdle();
}

TEST_P(PasswordStoreBuiltInBackendTest, GetAllLoginsAsyncMetrics) {
  const char kDurationMetric[] =
      "PasswordManager.PasswordStoreBuiltInBackend.GetAllLoginsAsync.Latency";
  const char kSuccessMetric[] =
      "PasswordManager.PasswordStoreBuiltInBackend.GetAllLoginsAsync.Success";
  base::HistogramTester histogram_tester;

  PasswordStoreBackend* backend = CreateBackend();
  InitializeBackend(backend);

  // Fill the store
  StoredCredential cred =
      FillStoredCredentialWithData(CreateTestPasswordFormData(), GetParam());

  const PasswordStoreChange add_change = PasswordStoreChange(
      PasswordStoreChange::ADD, CloneStoredCredential(cred));

  base::MockCallback<PasswordChangesOrErrorReply> mock_reply;
  EXPECT_CALL(
      mock_reply,
      Run(VariantWith<PasswordChanges>(Optional(ElementsAre(add_change)))));
  backend->AddLoginAsync(std::move(cred), mock_reply.Get());

  // Get the logins
  backend->GetAllLoginsAsync(base::DoNothing());

  AdvanceClock(kLatencyDelta);
  RunUntilIdle();

  histogram_tester.ExpectTotalCount(kDurationMetric, 1);
  histogram_tester.ExpectTimeBucketCount(kDurationMetric, kLatencyDelta, 1);
  histogram_tester.ExpectTotalCount(kSuccessMetric, 1);
  histogram_tester.ExpectBucketCount(kSuccessMetric, true, 1);
}

TEST_P(PasswordStoreBuiltInBackendTest, GetAllLoginsAsyncFailsMetrics) {
  const char kDurationMetric[] =
      "PasswordManager.PasswordStoreBuiltInBackend.GetAllLoginsAsync.Latency";
  const char kSuccessMetric[] =
      "PasswordManager.PasswordStoreBuiltInBackend.GetAllLoginsAsync.Success";
  base::HistogramTester histogram_tester;
  PasswordStoreBackend* bad_backend =
      CreateBackend(std::make_unique<BadLoginDatabase>(GetParam()));
  InitializeBackend(bad_backend);

  bad_backend->GetAllLoginsAsync(base::DoNothing());

  AdvanceClock(kLatencyDelta);
  RunUntilIdle();

  histogram_tester.ExpectTotalCount(kDurationMetric, 1);
  histogram_tester.ExpectTimeBucketCount(kDurationMetric, kLatencyDelta, 1);
  histogram_tester.ExpectTotalCount(kSuccessMetric, 1);
  histogram_tester.ExpectBucketCount(kSuccessMetric, false, 1);
}

TEST_P(PasswordStoreBuiltInBackendTest, GetAutofillableLoginsAsyncMetrics) {
  const char kDurationMetricGetLogins[] =
      "PasswordManager.PasswordStoreBuiltInBackend."
      "GetAutofillableLoginsAsync.Latency";
  const char kSuccessMetricGetLogins[] =
      "PasswordManager.PasswordStoreBuiltInBackend."
      "GetAutofillableLoginsAsync.Success";
  const char kDurationMetricAddLogin[] =
      "PasswordManager.PasswordStoreBuiltInBackend.AddLoginAsync.Latency";
  const char kSuccessMetricAddLogin[] =
      "PasswordManager.PasswordStoreBuiltInBackend.AddLoginAsync.Success";
  base::HistogramTester histogram_tester;

  PasswordStoreBackend* backend = CreateBackend();
  InitializeBackend(backend);

  // Fill the store
  StoredCredential cred =
      FillStoredCredentialWithData(CreateTestPasswordFormData(), GetParam());

  const PasswordStoreChange add_change = PasswordStoreChange(
      PasswordStoreChange::ADD, CloneStoredCredential(cred));

  base::MockCallback<PasswordChangesOrErrorReply> mock_reply;
  EXPECT_CALL(
      mock_reply,
      Run(VariantWith<PasswordChanges>(Optional(ElementsAre(add_change)))));
  backend->AddLoginAsync(std::move(cred), mock_reply.Get());

  // Get the logins
  backend->GetAutofillableLoginsAsync(base::DoNothing());

  AdvanceClock(kLatencyDelta);
  RunUntilIdle();

  // Metrics for GetAutofillableLoginsAsyncMetrics
  histogram_tester.ExpectTotalCount(kDurationMetricGetLogins, 1);
  histogram_tester.ExpectTimeBucketCount(kDurationMetricGetLogins,
                                         kLatencyDelta, 1);
  histogram_tester.ExpectTotalCount(kSuccessMetricGetLogins, 1);
  histogram_tester.ExpectBucketCount(kSuccessMetricGetLogins, true, 1);
  // Metrics for AddLoginAsync that also gets called in this test
  histogram_tester.ExpectTotalCount(kDurationMetricAddLogin, 1);
  histogram_tester.ExpectTimeBucketCount(kDurationMetricAddLogin, kLatencyDelta,
                                         1);
  histogram_tester.ExpectTotalCount(kSuccessMetricAddLogin, 1);
  histogram_tester.ExpectBucketCount(kSuccessMetricAddLogin, true, 1);
}

TEST_P(PasswordStoreBuiltInBackendTest,
       GetAutofillableLoginsAsyncFailsMetrics) {
  const char kDurationMetricGetLogins[] =
      "PasswordManager.PasswordStoreBuiltInBackend."
      "GetAutofillableLoginsAsync.Latency";
  const char kSuccessMetricGetLogins[] =
      "PasswordManager.PasswordStoreBuiltInBackend."
      "GetAutofillableLoginsAsync.Success";
  const char kDurationMetricAddLogin[] =
      "PasswordManager.PasswordStoreBuiltInBackend.AddLoginAsync.Latency";
  const char kSuccessMetricAddLogin[] =
      "PasswordManager.PasswordStoreBuiltInBackend.AddLoginAsync.Success";
  base::HistogramTester histogram_tester;

  PasswordStoreBackend* bad_backend =
      CreateBackend(std::make_unique<BadLoginDatabase>(GetParam()));
  InitializeBackend(bad_backend);

  // Fill the store
  StoredCredential cred =
      FillStoredCredentialWithData(CreateTestPasswordFormData(), GetParam());
  bad_backend->AddLoginAsync(std::move(cred), base::DoNothing());

  // Get the logins
  bad_backend->GetAutofillableLoginsAsync(base::DoNothing());

  AdvanceClock(kLatencyDelta);
  RunUntilIdle();

  // Metrics for GetAutofillableLoginsAsyncMetrics
  histogram_tester.ExpectTotalCount(kDurationMetricGetLogins, 1);
  histogram_tester.ExpectTimeBucketCount(kDurationMetricGetLogins,
                                         kLatencyDelta, 1);
  histogram_tester.ExpectTotalCount(kSuccessMetricGetLogins, 1);
  histogram_tester.ExpectBucketCount(kSuccessMetricGetLogins, false, 1);
  // Metrics for AddLoginAsync that also gets called in this test
  histogram_tester.ExpectTotalCount(kDurationMetricAddLogin, 1);
  histogram_tester.ExpectTimeBucketCount(kDurationMetricAddLogin, kLatencyDelta,
                                         1);
  histogram_tester.ExpectTotalCount(kSuccessMetricAddLogin, 1);
  histogram_tester.ExpectBucketCount(kSuccessMetricAddLogin, false, 1);
}

TEST_P(PasswordStoreBuiltInBackendTest, UpdateLoginAsyncMetrics) {
  const char kDurationMetric[] =
      "PasswordManager.PasswordStoreBuiltInBackend.UpdateLoginAsync.Latency";
  const char kSuccessMetric[] =
      "PasswordManager.PasswordStoreBuiltInBackend.UpdateLoginAsync.Success";
  base::HistogramTester histogram_tester;

  PasswordStoreBackend* backend = CreateBackend();
  InitializeBackend(backend);
  StoredCredential cred =
      FillStoredCredentialWithData(CreateTestPasswordFormData(), GetParam());

  backend->AddLoginAsync(CloneStoredCredential(cred), base::DoNothing());
  RunUntilIdle();

  cred.password_value =
      password_manager::PasswordString(u"a different password");
  const PasswordStoreChange update_change = PasswordStoreChange(
      PasswordStoreChange::UPDATE, CloneStoredCredential(cred),
      /*password_changed=*/true);

  base::MockCallback<PasswordChangesOrErrorReply> mock_reply;
  EXPECT_CALL(
      mock_reply,
      Run(VariantWith<PasswordChanges>(Optional(ElementsAre(update_change)))));
  backend->UpdateLoginAsync(std::move(cred), mock_reply.Get());

  AdvanceClock(kLatencyDelta);
  RunUntilIdle();

  histogram_tester.ExpectTotalCount(kDurationMetric, 1);
  histogram_tester.ExpectTimeBucketCount(kDurationMetric, kLatencyDelta, 1);
  histogram_tester.ExpectTotalCount(kSuccessMetric, 1);
  histogram_tester.ExpectBucketCount(kSuccessMetric, true, 1);
}

TEST_P(PasswordStoreBuiltInBackendTest, UpdateLoginAsyncFailsMetrics) {
  const char kDurationMetric[] =
      "PasswordManager.PasswordStoreBuiltInBackend.UpdateLoginAsync.Latency";
  const char kSuccessMetric[] =
      "PasswordManager.PasswordStoreBuiltInBackend.UpdateLoginAsync.Success";
  base::HistogramTester histogram_tester;

  PasswordStoreBackend* bad_backend =
      CreateBackend(std::make_unique<BadLoginDatabase>(GetParam()));
  InitializeBackend(bad_backend);
  PasswordForm form =
      *FillPasswordFormWithData(CreateTestPasswordFormData(), GetParam());

  bad_backend->UpdateLoginAsync(FromPasswordForm(form), base::DoNothing());

  AdvanceClock(kLatencyDelta);
  RunUntilIdle();

  histogram_tester.ExpectTotalCount(kDurationMetric, 1);
  histogram_tester.ExpectTimeBucketCount(kDurationMetric, kLatencyDelta, 1);
  histogram_tester.ExpectTotalCount(kSuccessMetric, 1);
  histogram_tester.ExpectBucketCount(kSuccessMetric, false, 1);
}

TEST_P(PasswordStoreBuiltInBackendTest, RemoveLoginAsyncMetrics) {
  const char kDurationMetric[] =
      "PasswordManager.PasswordStoreBuiltInBackend.RemoveLoginAsync.Latency";
  const char kSuccessMetric[] =
      "PasswordManager.PasswordStoreBuiltInBackend.RemoveLoginAsync.Success";
  base::HistogramTester histogram_tester;

  PasswordStoreBackend* backend = CreateBackend();
  InitializeBackend(backend);
  PasswordForm form =
      *FillPasswordFormWithData(CreateTestPasswordFormData(), GetParam());

  backend->AddLoginAsync(FromPasswordForm(form), base::DoNothing());
  RunUntilIdle();

  PasswordStoreChange remove_change =
      PasswordStoreChange(PasswordStoreChange::REMOVE, FromPasswordForm(form));

  backend->RemoveLoginAsync(FROM_HERE, FromPasswordForm(form),
                            base::DoNothing());

  AdvanceClock(kLatencyDelta);
  RunUntilIdle();

  histogram_tester.ExpectTotalCount(kDurationMetric, 1);
  histogram_tester.ExpectTimeBucketCount(kDurationMetric, kLatencyDelta, 1);
  histogram_tester.ExpectTotalCount(kSuccessMetric, 1);
  histogram_tester.ExpectBucketCount(kSuccessMetric, true, 1);
}

TEST_P(PasswordStoreBuiltInBackendTest, RemoveLoginAsyncFailsMetrics) {
  const char kDurationMetric[] =
      "PasswordManager.PasswordStoreBuiltInBackend.RemoveLoginAsync.Latency";
  base::HistogramTester histogram_tester;

  PasswordStoreBackend* bad_backend =
      CreateBackend(std::make_unique<BadLoginDatabase>(GetParam()));
  InitializeBackend(bad_backend);
  PasswordForm form =
      *FillPasswordFormWithData(CreateTestPasswordFormData(), GetParam());

  bad_backend->AddLoginAsync(FromPasswordForm(form), base::DoNothing());
  RunUntilIdle();

  PasswordStoreChange remove_change =
      PasswordStoreChange(PasswordStoreChange::REMOVE, FromPasswordForm(form));

  bad_backend->RemoveLoginAsync(FROM_HERE, FromPasswordForm(form),
                                base::DoNothing());

  AdvanceClock(kLatencyDelta);
  RunUntilIdle();

  histogram_tester.ExpectTotalCount(kDurationMetric, 1);
  histogram_tester.ExpectTimeBucketCount(kDurationMetric, kLatencyDelta, 1);
}

TEST_P(PasswordStoreBuiltInBackendTest,
       RemoveLoginsCreatedBetweenAsyncMetrics) {
  const char kDurationMetric[] =
      "PasswordManager.PasswordStoreBuiltInBackend."
      "RemoveLoginsCreatedBetweenAsync."
      "Latency";
  const char kSuccessMetric[] =
      "PasswordManager.PasswordStoreBuiltInBackend."
      "RemoveLoginsCreatedBetweenAsync."
      "Success";
  base::HistogramTester histogram_tester;

  PasswordStoreBackend* backend = CreateBackend();
  InitializeBackend(backend);
  PasswordForm form =
      *FillPasswordFormWithData(CreateTestPasswordFormData(), GetParam());
  form.date_created = base::Time::FromTimeT(1500);
  backend->AddLoginAsync(FromPasswordForm(form), base::DoNothing());
  RunUntilIdle();

  backend->RemoveLoginsCreatedBetweenAsync(FROM_HERE, kStart, kEnd,
                                           base::DoNothing());

  AdvanceClock(kLatencyDelta);
  RunUntilIdle();

  histogram_tester.ExpectTotalCount(kDurationMetric, 1);
  histogram_tester.ExpectTimeBucketCount(kDurationMetric, kLatencyDelta, 1);
  histogram_tester.ExpectTotalCount(kSuccessMetric, 1);
  histogram_tester.ExpectBucketCount(kSuccessMetric, true, 1);
}

TEST_P(PasswordStoreBuiltInBackendTest,
       RemoveLoginsCreatedBetweenAsyncNothingToDeleteMetrics) {
  const char kDurationMetric[] =
      "PasswordManager.PasswordStoreBuiltInBackend."
      "RemoveLoginsCreatedBetweenAsync."
      "Latency";
  const char kSuccessMetric[] =
      "PasswordManager.PasswordStoreBuiltInBackend."
      "RemoveLoginsCreatedBetweenAsync."
      "Success";
  base::HistogramTester histogram_tester;

  PasswordStoreBackend* backend = CreateBackend();
  InitializeBackend(backend);
  PasswordForm form =
      *FillPasswordFormWithData(CreateTestPasswordFormData(), GetParam());
  form.date_created = base::Time::FromTimeT(300);
  backend->AddLoginAsync(FromPasswordForm(form), base::DoNothing());
  RunUntilIdle();

  backend->RemoveLoginsCreatedBetweenAsync(FROM_HERE, kStart, kEnd,
                                           base::DoNothing());

  AdvanceClock(kLatencyDelta);
  RunUntilIdle();

  histogram_tester.ExpectTotalCount(kDurationMetric, 1);
  histogram_tester.ExpectTimeBucketCount(kDurationMetric, kLatencyDelta, 1);
  histogram_tester.ExpectTotalCount(kSuccessMetric, 1);
  histogram_tester.ExpectBucketCount(kSuccessMetric, true, 1);
}

TEST_P(PasswordStoreBuiltInBackendTest,
       RemoveLoginsCreatedBetweenAsyncFailsMetrics) {
  const char kDurationMetric[] =
      "PasswordManager.PasswordStoreBuiltInBackend."
      "RemoveLoginsCreatedBetweenAsync."
      "Latency";
  const char kSuccessMetric[] =
      "PasswordManager.PasswordStoreBuiltInBackend."
      "RemoveLoginsCreatedBetweenAsync."
      "Success";
  base::HistogramTester histogram_tester;

  PasswordStoreBackend* bad_backend =
      CreateBackend(std::make_unique<BadLoginDatabase>(GetParam()));
  InitializeBackend(bad_backend);

  bad_backend->RemoveLoginsCreatedBetweenAsync(FROM_HERE, kStart, kEnd,
                                               base::DoNothing());

  AdvanceClock(kLatencyDelta);
  RunUntilIdle();

  histogram_tester.ExpectTotalCount(kDurationMetric, 1);
  histogram_tester.ExpectTimeBucketCount(kDurationMetric, kLatencyDelta, 1);
  histogram_tester.ExpectTotalCount(kSuccessMetric, 1);
  histogram_tester.ExpectBucketCount(kSuccessMetric, false, 1);
}

TEST_P(PasswordStoreBuiltInBackendTest, FillMatchingLoginsAsyncMetrics) {
  const char kDurationMetric[] =
      "PasswordManager.PasswordStoreBuiltInBackend."
      "FillMatchingLoginsAsync."
      "Latency";
  const char kSuccessMetric[] =
      "PasswordManager.PasswordStoreBuiltInBackend."
      "FillMatchingLoginsAsync."
      "Success";
  base::HistogramTester histogram_tester;

  PasswordStoreBackend* backend = CreateBackend();
  InitializeBackend(backend);
  PasswordForm form =
      *FillPasswordFormWithData(CreateTestPasswordFormData(), GetParam());
  const std::string kTestPasswordFormURL = form.signon_realm;
  backend->AddLoginAsync(FromPasswordForm(std::move(form)), base::DoNothing());
  RunUntilIdle();

  std::vector<PasswordFormDigest> forms;
  forms.emplace_back(PasswordFormDigest(PasswordForm::Scheme::kHtml,
                                        kTestPasswordFormURL,
                                        GURL(kTestPasswordFormURL)));

  backend->FillMatchingLoginsAsync(base::DoNothing(), /*include_psl=*/false,
                                   std::move(forms));
  AdvanceClock(kLatencyDelta);
  RunUntilIdle();

  histogram_tester.ExpectTotalCount(kDurationMetric, 1);
  histogram_tester.ExpectTimeBucketCount(kDurationMetric, kLatencyDelta, 1);
  histogram_tester.ExpectTotalCount(kSuccessMetric, 1);
  histogram_tester.ExpectBucketCount(kSuccessMetric, true, 1);
}

TEST_P(PasswordStoreBuiltInBackendTest,
       FillMatchingLoginsAsyncNothingToFillMetrics) {
  const char kDurationMetric[] =
      "PasswordManager.PasswordStoreBuiltInBackend."
      "FillMatchingLoginsAsync."
      "Latency";
  const char kSuccessMetric[] =
      "PasswordManager.PasswordStoreBuiltInBackend."
      "FillMatchingLoginsAsync."
      "Success";
  base::HistogramTester histogram_tester;
  std::string kTestPasswordFormURL("http://foo.example.com");

  PasswordStoreBackend* backend = CreateBackend();
  InitializeBackend(backend);

  std::vector<PasswordFormDigest> forms;
  forms.emplace_back(PasswordFormDigest(PasswordForm::Scheme::kHtml,
                                        kTestPasswordFormURL,
                                        GURL(kTestPasswordFormURL)));

  backend->FillMatchingLoginsAsync(base::DoNothing(), /*include_psl=*/false,
                                   std::move(forms));
  AdvanceClock(kLatencyDelta);
  RunUntilIdle();

  histogram_tester.ExpectTotalCount(kDurationMetric, 1);
  histogram_tester.ExpectTimeBucketCount(kDurationMetric, kLatencyDelta, 1);
  histogram_tester.ExpectTotalCount(kSuccessMetric, 1);
  histogram_tester.ExpectBucketCount(kSuccessMetric, true, 1);
}

// This test verifies Android to web affiliations and grouped logins are
// correctly handled when GetGroupedMatchingLoginsAsync() is called.
TEST_P(PasswordStoreBuiltInBackendTest, GetLoginsWithAffiliationsAndGroups) {
  static constexpr char kTestWebRealmNonPSL[] = "https://example.gov/";
  static constexpr char kTestWebOriginNonPSL[] = "https://example.gov/origin";
  static constexpr PasswordFormData kCredentials[] = {
      // Affiliated Android credential.
      {PasswordForm::Scheme::kHtml, kTestAndroidRealm1, "", "", u"", u"", u"",
       u"username_value_1", u"password_value_1", kTestLastUsageTime, 1},
      // Exact match credential.
      {PasswordForm::Scheme::kHtml, kTestWebRealm1, kTestWebOrigin1, "", u"",
       u"", u"", u"username_value_4", u"password_value_4", kTestLastUsageTime,
       1},
      // PSL matching credential.
      {PasswordForm::Scheme::kHtml, kTestWebRealm2, kTestWebOrigin2, "", u"",
       u"", u"", u"username_value_5", u"password_value_5", kTestLastUsageTime,
       1},
      // Non-PSL matching grouped credential.
      {PasswordForm::Scheme::kHtml, kTestWebRealmNonPSL, kTestWebOriginNonPSL,
       "", u"", u"", u"", u"username_value_6", u"password_value_6",
       kTestLastUsageTime, 1}};

  auto owning_mock_match_helper =
      std::make_unique<MockAffiliatedMatchHelper>(&fake_affiliation_service_);
  MockAffiliatedMatchHelper* mock_affiliated_match_helper =
      owning_mock_match_helper.get();
  PasswordStoreBackend* backend =
      CreateBackend(nullptr, std::move(owning_mock_match_helper));
  InitializeBackend(backend);

  std::vector<std::unique_ptr<PasswordForm>> all_credentials;
  for (const auto& test_credential : kCredentials) {
    all_credentials.push_back(
        FillPasswordFormWithData(test_credential, GetParam()));
    backend->AddLoginAsync(FromPasswordForm(*all_credentials.back()),
                           base::DoNothing());
    RunUntilIdle();
  }

  std::vector<PasswordForm> expected_results;
  expected_results.push_back(*all_credentials[0]);
  expected_results.back().match_type = PasswordForm::MatchType::kAffiliated;
  expected_results.push_back(*all_credentials[1]);
  expected_results.back().match_type = PasswordForm::MatchType::kExact;
  expected_results.push_back(*all_credentials[2]);
  expected_results.back().match_type = PasswordForm::MatchType::kPSL;
  expected_results.push_back(*all_credentials[3]);
  expected_results.back().match_type = PasswordForm::MatchType::kGrouped;

  PasswordFormDigest observed_form = {PasswordForm::Scheme::kHtml,
                                      kTestWebRealm1, GURL(kTestWebOrigin1)};

  std::vector<std::string> affiliated_android_realms;
  affiliated_android_realms.push_back(kTestAndroidRealm1);
  std::vector<std::string> grouped_realms;
  grouped_realms.push_back(kTestWebRealmNonPSL);

  mock_affiliated_match_helper->ExpectCallToGetAffiliatedAndGrouped(
      observed_form, affiliated_android_realms, grouped_realms);
  mock_affiliated_match_helper
      ->ExpectCallToInjectAffiliationAndBrandingInformation({});
  base::MockCallback<BackendLoginsOrErrorReply> mock_reply;
  EXPECT_CALL(mock_reply,
              Run(VariantWith<BackendLoginsResult>(
                  MatchesFormsIgnoringPrimaryKey(expected_results))));

  backend->GetGroupedMatchingLoginsAsync(observed_form, mock_reply.Get());
  RunUntilIdle();
}

// This test verifies that, when there are no affiliated credentials for the
// realm of the observed form, GetGroupedMatchingLoginsAsync() should still
// return the exact and PSL matching results.
TEST_P(PasswordStoreBuiltInBackendTest, GetLoginsWithoutAffiliations) {
  static constexpr char kTestPSLMatchingWebRealm[] = "https://psl.example.com/";
  static constexpr char kTestPSLMatchingWebOrigin[] =
      "https://psl.example.com/origin";
  static constexpr char kTestUnrelatedAndroidRealm[] =
      "android://hash@com.notexample.android/";

  /* clang-format off */
  static constexpr const PasswordFormData kCredentials[] = {
      // Credential that is an exact match of the observed form.
      {PasswordForm::Scheme::kHtml,
       kTestWebRealm1,
       kTestWebOrigin1,
       "", u"", u"",  u"",
       u"username_value_1",
       u"password_value_1", kTestLastUsageTime, 1},
      // Credential that is a PSL match of the observed form.
      {PasswordForm::Scheme::kHtml,
       kTestPSLMatchingWebRealm,
       kTestPSLMatchingWebOrigin,
       "", u"", u"",  u"",
       u"username_value_2",
       u"password_value_2", kTestLastUsageTime, 1},
      // Credential for an unrelated Android application.
      {PasswordForm::Scheme::kHtml,
       kTestUnrelatedAndroidRealm,
       "", "", u"", u"", u"",
       u"username_value_3",
       u"password_value_3", kTestLastUsageTime, 1}};
  /* clang-format on */

  auto owning_mock_match_helper =
      std::make_unique<MockAffiliatedMatchHelper>(&fake_affiliation_service_);
  MockAffiliatedMatchHelper* mock_affiliated_match_helper =
      owning_mock_match_helper.get();
  PasswordStoreBackend* backend =
      CreateBackend(nullptr, std::move(owning_mock_match_helper));
  InitializeBackend(backend);

  std::vector<std::unique_ptr<PasswordForm>> all_credentials;
  for (const auto& credential : kCredentials) {
    all_credentials.push_back(FillPasswordFormWithData(credential, GetParam()));
    backend->AddLoginAsync(FromPasswordForm(*all_credentials.back()),
                           base::DoNothing());
    RunUntilIdle();
  }

  PasswordFormDigest observed_form = {PasswordForm::Scheme::kHtml,
                                      kTestWebRealm1, GURL(kTestWebOrigin1)};

  all_credentials[0]->match_type = PasswordForm::MatchType::kExact;
  all_credentials[1]->match_type = PasswordForm::MatchType::kPSL;
  std::vector<PasswordForm> expected_results = {*all_credentials[0],
                                                *all_credentials[1]};

  std::vector<std::string> no_affiliated_android_realms;
  mock_affiliated_match_helper->ExpectCallToGetAffiliatedAndGrouped(
      observed_form, no_affiliated_android_realms);
  mock_affiliated_match_helper
      ->ExpectCallToInjectAffiliationAndBrandingInformation({});

  base::MockCallback<BackendLoginsOrErrorReply> mock_reply;
  EXPECT_CALL(mock_reply,
              Run(VariantWith<BackendLoginsResult>(
                  MatchesFormsIgnoringPrimaryKey(expected_results))));

  backend->GetGroupedMatchingLoginsAsync(observed_form, mock_reply.Get());
  RunUntilIdle();
}

// There are 3 Android applications affiliated with the realm of the observed
// form, with the `PasswordStore` having credentials for two of these (even two
// credentials for one). `GetGroupedMatchingLoginsAsync()` should return the
// exact, and PSL matching credentials, and the credentials for these two
// Android applications (including one federated), but not for the
// unaffiliated Android application.
TEST_P(PasswordStoreBuiltInBackendTest,
       GetLoginsWithAffiliationsIncludingFederated) {
  static constexpr char kTestPSLMatchingWebRealm[] = "https://psl.example.com/";
  static constexpr char kTestPSLMatchingWebOrigin[] =
      "https://psl.example.com/origin";
  static constexpr char kTestUnrelatedAndroidRealm[] =
      "android://hash@com.notexample.android/";

  static constexpr const struct {
    PasswordFormData form_data;
    bool use_federated_login;
  } kCredentials[] = {
      // Credential that is an exact match of the observed form.
      {
          {PasswordForm::Scheme::kHtml, kTestWebRealm1, kTestWebOrigin1, "",
           u"", u"", u"", u"username_value_1", u"password_value_1",
           kTestLastUsageTime, 1},
          /*use_federated_login=*/false,
      },
      // Credential that is a PSL match of the observed form.
      {
          {PasswordForm::Scheme::kHtml, kTestPSLMatchingWebRealm,
           kTestPSLMatchingWebOrigin, "", u"", u"", u"", u"username_value_2",
           u"password_value_2", true, 1},
          /*use_federated_login=*/false,
      },
      // Credential for an Android application affiliated with the realm of the
      // observed from.
      {
          {PasswordForm::Scheme::kHtml, kTestAndroidRealm1, "", "", u"", u"",
           u"", u"username_value_3", u"password_value_3", kTestLastUsageTime,
           1},
          /*use_federated_login=*/false,
      },
      // Second credential for the same Android application.
      {
          {PasswordForm::Scheme::kHtml, kTestAndroidRealm1, "", "", u"", u"",
           u"", u"username_value_3b", u"password_value_3b", kTestLastUsageTime,
           1},
          /*use_federated_login=*/false,
      },
      // Third credential for the same application which is username-only.
      {
          {PasswordForm::Scheme::kUsernameOnly, kTestAndroidRealm1, "", "", u"",
           u"", u"", u"username_value_3c", u"password_value_3c",
           kTestLastUsageTime, 1},
          /*use_federated_login=*/false,
      },
      // Credential for another Android application affiliated with the realm
      // of the observed from.
      {
          {PasswordForm::Scheme::kHtml, kTestAndroidRealm2, "", "", u"", u"",
           u"", u"username_value_4", u"password_value_4", kTestLastUsageTime,
           1},
          /*use_federated_login=*/false,
      },
      // Federated credential for this second Android application.
      {
          {PasswordForm::Scheme::kHtml, kTestAndroidRealm2, "", "", u"", u"",
           u"", u"username_value_4b", u"password_value_4b", kTestLastUsageTime,
           1},
          /*use_federated_login=*/true,
      },
      // Credential for an unrelated Android application.
      {
          {PasswordForm::Scheme::kHtml, kTestUnrelatedAndroidRealm, "", "", u"",
           u"", u"", u"username_value_5", u"password_value_5",
           kTestLastUsageTime, 1},
          /*use_federated_login=*/false,
      }};

  auto owning_mock_match_helper =
      std::make_unique<MockAffiliatedMatchHelper>(&fake_affiliation_service_);
  MockAffiliatedMatchHelper* mock_affiliated_match_helper =
      owning_mock_match_helper.get();
  PasswordStoreBackend* backend =
      CreateBackend(nullptr, std::move(owning_mock_match_helper));
  InitializeBackend(backend);

  std::vector<std::unique_ptr<PasswordForm>> all_credentials;
  for (const auto& i : kCredentials) {
    all_credentials.push_back(FillPasswordFormWithData(i.form_data, GetParam(),
                                                       i.use_federated_login));
    backend->AddLoginAsync(FromPasswordForm(*all_credentials.back()),
                           base::DoNothing());
    RunUntilIdle();
  }

  PasswordFormDigest observed_form = {PasswordForm::Scheme::kHtml,
                                      kTestWebRealm1, GURL(kTestWebOrigin1)};

  all_credentials[0]->match_type = PasswordForm::MatchType::kExact;
  all_credentials[1]->match_type = PasswordForm::MatchType::kPSL;
  all_credentials[2]->match_type = PasswordForm::MatchType::kAffiliated;
  all_credentials[3]->match_type = PasswordForm::MatchType::kAffiliated;
  all_credentials[5]->match_type = PasswordForm::MatchType::kAffiliated;
  all_credentials[6]->match_type = PasswordForm::MatchType::kAffiliated;
  std::vector<PasswordForm> expected_results = {
      *all_credentials[0], *all_credentials[1], *all_credentials[2],
      *all_credentials[3], *all_credentials[5], *all_credentials[6]};

  std::vector<std::string> affiliated_android_realms;
  affiliated_android_realms.push_back(kTestAndroidRealm1);
  affiliated_android_realms.push_back(kTestAndroidRealm2);
  // kTestAndroidRealm3 is also affiliated, but we don't have credentials for
  // it.
  affiliated_android_realms.push_back(kTestAndroidRealm3);
  // kTestUnrelatedAndroidRealm is NOT added here, so it is unaffiliated.

  mock_affiliated_match_helper->ExpectCallToGetAffiliatedAndGrouped(
      observed_form, affiliated_android_realms);
  mock_affiliated_match_helper
      ->ExpectCallToInjectAffiliationAndBrandingInformation({});

  base::MockCallback<BackendLoginsOrErrorReply> mock_reply;
  EXPECT_CALL(mock_reply,
              Run(VariantWith<BackendLoginsResult>(
                  MatchesFormsIgnoringPrimaryKey(expected_results))));

  backend->GetGroupedMatchingLoginsAsync(observed_form, mock_reply.Get());
  RunUntilIdle();
}

// This test is testing all the combinations of affiliated and PSL matching.
TEST_P(PasswordStoreBuiltInBackendTest,
       GetLoginsWithWebAffiliationsAndPSLMatching) {
  static constexpr char kTestPSLMatchingWebRealm[] = "https://psl.example.com/";
  static constexpr char kTestPSLMatchingWebOrigin[] =
      "https://psl.example.com/origin";
  static constexpr char kTestAffiliatedRealm[] = "https://one.example/";
  static constexpr char kTestAffiliatedURL[] = "https://one.example/path";
  static constexpr char kTestAffiliatedPSLWebRealm[] = "https://two.example/";
  static constexpr char kTestAffiliatedPSLWebURL[] = "https://two.example/path";
  static constexpr char kTestUnrelatedWebRealm2[] = "https://notexample2.com/";
  static constexpr char kTestUnrelatedWebOrigin2[] =
      "https://notexample2.com/origin";

  /* clang-format off */
  static constexpr const PasswordFormData kCredentials[] = {
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
       u"password2"},
      // Credential that is a PSL and a group match of the observed form.
      {PasswordForm::Scheme::kHtml, kTestWebRealm3, kTestWebOrigin3, "", u"",
       u"", u"", u"username_7", u"password7"}};
  /* clang-format on */

  auto owning_mock_match_helper =
      std::make_unique<MockAffiliatedMatchHelper>(&fake_affiliation_service_);
  MockAffiliatedMatchHelper* mock_affiliated_match_helper =
      owning_mock_match_helper.get();
  PasswordStoreBackend* backend =
      CreateBackend(nullptr, std::move(owning_mock_match_helper));
  InitializeBackend(backend);

  std::vector<std::unique_ptr<PasswordForm>> all_credentials;
  for (const auto& credential : kCredentials) {
    all_credentials.push_back(FillPasswordFormWithData(
        credential, GetParam(), /*use_federated_login=*/false));
    backend->AddLoginAsync(FromPasswordForm(*all_credentials.back()),
                           base::DoNothing());
    RunUntilIdle();
  }

  PasswordFormDigest observed_form = {PasswordForm::Scheme::kHtml,
                                      kTestWebRealm1, GURL(kTestWebOrigin1)};

  all_credentials[0]->match_type = PasswordForm::MatchType::kExact;
  all_credentials[1]->match_type = PasswordForm::MatchType::kPSL;
  all_credentials[2]->match_type =
      PasswordForm::MatchType::kAffiliated | PasswordForm::MatchType::kPSL;
  all_credentials[3]->match_type = PasswordForm::MatchType::kAffiliated;
  all_credentials[6]->match_type =
      PasswordForm::MatchType::kPSL | PasswordForm::MatchType::kGrouped;
  std::vector<PasswordForm> expected_results = {
      *all_credentials[0], *all_credentials[1], *all_credentials[2],
      *all_credentials[3], *all_credentials[6]};

  std::vector<std::string> affiliated_realms = {kTestWebRealm1, kTestWebRealm2,
                                                kTestAffiliatedRealm};
  std::vector<std::string> grouped_realms = {kTestWebRealm3};

  mock_affiliated_match_helper->ExpectCallToGetAffiliatedAndGrouped(
      observed_form, affiliated_realms, grouped_realms);
  mock_affiliated_match_helper
      ->ExpectCallToInjectAffiliationAndBrandingInformation({});

  base::MockCallback<BackendLoginsOrErrorReply> mock_reply;
  EXPECT_CALL(mock_reply,
              Run(VariantWith<BackendLoginsResult>(
                  MatchesFormsIgnoringPrimaryKey(expected_results))));

  backend->GetGroupedMatchingLoginsAsync(observed_form, mock_reply.Get());
  RunUntilIdle();
}

// Retrieve matching passwords for affiliated, affiliated/PSL-matched,
// PSL-matched, exact matched credentials and make sure the properties are set
// correctly. This test is different than the previous one because it uses
// federated credentials.
TEST_P(PasswordStoreBuiltInBackendTest,
       GetLoginsWithWebAffiliationsAndPSLMatchingFederated) {
  static constexpr char kTestPSLMatchingWebRealm[] = "https://psl.example.com/";
  static constexpr char kTestPSLMatchingWebOrigin[] =
      "https://psl.example.com/origin";
  static constexpr char kTestAffiliatedRealm[] = "https://one.example/";
  static constexpr char kTestAffiliatedURL[] = "https://one.example/path";
  static constexpr char kTestAffiliatedPSLWebRealm[] = "https://two.example/";
  static constexpr char kTestAffiliatedPSLWebURL[] = "https://two.example/path";
  static constexpr char kTestUnrelatedWebRealm2[] = "https://notexample2.com/";
  static constexpr char kTestUnrelatedWebOrigin2[] =
      "https://notexample2.com/origin";

  /* clang-format off */
  static constexpr const PasswordFormData kCredentials[] = {
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
  /* clang-format on */

  auto owning_mock_match_helper =
      std::make_unique<MockAffiliatedMatchHelper>(&fake_affiliation_service_);
  MockAffiliatedMatchHelper* mock_affiliated_match_helper =
      owning_mock_match_helper.get();
  PasswordStoreBackend* backend =
      CreateBackend(nullptr, std::move(owning_mock_match_helper));
  InitializeBackend(backend);

  std::vector<std::unique_ptr<PasswordForm>> all_credentials;
  for (const auto& credential : kCredentials) {
    all_credentials.push_back(FillPasswordFormWithData(
        credential, GetParam(), /*use_federated_login=*/true));
    backend->AddLoginAsync(FromPasswordForm(*all_credentials.back()),
                           base::DoNothing());
    RunUntilIdle();
  }

  PasswordFormDigest observed_form = {PasswordForm::Scheme::kHtml,
                                      kTestWebRealm1, GURL(kTestWebOrigin1)};

  all_credentials[0]->match_type = PasswordForm::MatchType::kExact;
  all_credentials[1]->match_type = PasswordForm::MatchType::kPSL;
  all_credentials[2]->match_type =
      PasswordForm::MatchType::kAffiliated | PasswordForm::MatchType::kPSL;
  all_credentials[3]->match_type = PasswordForm::MatchType::kAffiliated;
  std::vector<PasswordForm> expected_results = {
      *all_credentials[0], *all_credentials[1], *all_credentials[2],
      *all_credentials[3]};

  std::vector<std::string> affiliated_realms = {kTestWebRealm1, kTestWebRealm2,
                                                kTestAffiliatedRealm};

  mock_affiliated_match_helper->ExpectCallToGetAffiliatedAndGrouped(
      observed_form, affiliated_realms);
  mock_affiliated_match_helper
      ->ExpectCallToInjectAffiliationAndBrandingInformation({});

  base::MockCallback<BackendLoginsOrErrorReply> mock_reply;
  EXPECT_CALL(mock_reply,
              Run(VariantWith<BackendLoginsResult>(
                  MatchesFormsIgnoringPrimaryKey(expected_results))));

  backend->GetGroupedMatchingLoginsAsync(observed_form, mock_reply.Get());
  RunUntilIdle();
}

// This test verifies web to web affiliated and grouped credentials are
// correctly handled when GetGroupedMatchingLoginsAsync() is called.
TEST_P(PasswordStoreBuiltInBackendTest, GetLoginsWithWebGroup) {
  static constexpr char kTestPSLMatchingWebRealm[] = "https://psl.example.com/";
  static constexpr char kTestPSLMatchingWebOrigin[] =
      "https://psl.example.com/origin";
  static constexpr char kTestGroupRealm[] = "https://one-good.example/";
  static constexpr char kTestGroupURL[] = "https://one-good.example/path";
  static constexpr char kTestAffiliatedPSLWebRealm[] = "https://two.example/";
  static constexpr char kTestAffiliatedPSLWebURL[] = "https://two.example/path";
  static constexpr char kTestUnrelatedWebRealm2[] = "https://notexample2.com/";
  static constexpr char kTestUnrelatedWebOrigin2[] =
      "https://notexample2.com/origin";

  /* clang-format off */
  static constexpr const PasswordFormData kCredentials[] = {
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
  /* clang-format on */

  auto owning_mock_match_helper =
      std::make_unique<MockAffiliatedMatchHelper>(&fake_affiliation_service_);
  MockAffiliatedMatchHelper* mock_affiliated_match_helper =
      owning_mock_match_helper.get();
  PasswordStoreBackend* backend =
      CreateBackend(nullptr, std::move(owning_mock_match_helper));
  InitializeBackend(backend);

  std::vector<std::unique_ptr<PasswordForm>> all_credentials;
  for (const auto& credential : kCredentials) {
    all_credentials.push_back(FillPasswordFormWithData(credential, GetParam()));
    backend->AddLoginAsync(FromPasswordForm(*all_credentials.back()),
                           base::DoNothing());
    RunUntilIdle();
  }

  PasswordFormDigest observed_form = {PasswordForm::Scheme::kHtml,
                                      kTestWebRealm1, GURL(kTestWebOrigin1)};

  all_credentials[0]->match_type = PasswordForm::MatchType::kExact;
  all_credentials[1]->match_type = PasswordForm::MatchType::kPSL;
  all_credentials[2]->match_type =
      PasswordForm::MatchType::kAffiliated | PasswordForm::MatchType::kPSL;
  all_credentials[3]->match_type = PasswordForm::MatchType::kGrouped;
  std::vector<PasswordForm> expected_results = {
      *all_credentials[0], *all_credentials[1], *all_credentials[2],
      *all_credentials[3]};

  std::vector<std::string> affiliated_realms = {kTestWebRealm1, kTestWebRealm2};
  std::vector<std::string> grouped_realms = {kTestGroupRealm};

  mock_affiliated_match_helper->ExpectCallToGetAffiliatedAndGrouped(
      observed_form, affiliated_realms, grouped_realms);
  mock_affiliated_match_helper
      ->ExpectCallToInjectAffiliationAndBrandingInformation({});

  base::MockCallback<BackendLoginsOrErrorReply> mock_reply;
  EXPECT_CALL(mock_reply,
              Run(VariantWith<BackendLoginsResult>(
                  MatchesFormsIgnoringPrimaryKey(expected_results))));

  backend->GetGroupedMatchingLoginsAsync(observed_form, mock_reply.Get());
  RunUntilIdle();
}

// Retrieve matching passwords for exact match credentials and make sure the
// affiliation and branding information is set correctly.
TEST_P(PasswordStoreBuiltInBackendTest,
       GetLoginsWithBrandingInformationForExactMatch) {
  auto owning_mock_match_helper =
      std::make_unique<MockAffiliatedMatchHelper>(&fake_affiliation_service_);
  MockAffiliatedMatchHelper* mock_affiliated_match_helper =
      owning_mock_match_helper.get();
  PasswordStoreBackend* backend =
      CreateBackend(nullptr, std::move(owning_mock_match_helper));
  InitializeBackend(backend);

  PasswordFormData form_data = {PasswordForm::Scheme::kHtml,
                                kTestWebRealm1,
                                kTestWebOrigin1,
                                "",
                                u"",
                                u"",
                                u"",
                                u"username_value_1",
                                u"password_value_1",
                                kTestLastUsageTime,
                                1};
  std::unique_ptr<PasswordForm> credential =
      FillPasswordFormWithData(form_data, /*is_account_store=*/GetParam());
  backend->AddLoginAsync(FromPasswordForm(*credential), base::DoNothing());
  RunUntilIdle();

  PasswordFormDigest observed_form = {PasswordForm::Scheme::kHtml,
                                      kTestWebRealm1, GURL(kTestWebOrigin1)};

  std::vector<MockAffiliatedMatchHelper::AffiliationAndBrandingInformation>
      affiliation_info_for_results = {
          {kTestWebRealm1, kTestAndroidName1, GURL(kTestAndroidIconURL1)}};
  mock_affiliated_match_helper
      ->ExpectCallToInjectAffiliationAndBrandingInformation(
          std::move(affiliation_info_for_results));

  credential->match_type = PasswordForm::MatchType::kExact;
  credential->affiliated_web_realm = kTestWebRealm1;
  credential->app_display_name = kTestAndroidName1;
  credential->app_icon_url = GURL(kTestAndroidIconURL1);

  std::vector<std::string> no_affiliated_android_realms;
  mock_affiliated_match_helper->ExpectCallToGetAffiliatedAndGrouped(
      observed_form, no_affiliated_android_realms);

  std::vector<PasswordForm> expected_results = {*credential};
  base::MockCallback<BackendLoginsOrErrorReply> mock_reply;
  EXPECT_CALL(mock_reply,
              Run(VariantWith<BackendLoginsResult>(
                  MatchesFormsIgnoringPrimaryKey(expected_results))));

  backend->GetGroupedMatchingLoginsAsync(observed_form, mock_reply.Get());
  RunUntilIdle();
}

// Retrieve matching passwords for affiliated match credentials and make sure
// the affiliation and branding information is set correctly.
TEST_P(PasswordStoreBuiltInBackendTest,
       GetLoginsWithBrandingInformationForAffiliatedLogins) {
  auto owning_mock_match_helper =
      std::make_unique<MockAffiliatedMatchHelper>(&fake_affiliation_service_);
  MockAffiliatedMatchHelper* mock_affiliated_match_helper =
      owning_mock_match_helper.get();
  PasswordStoreBackend* backend =
      CreateBackend(nullptr, std::move(owning_mock_match_helper));
  InitializeBackend(backend);

  PasswordFormData form_data = {PasswordForm::Scheme::kHtml,
                                kTestAndroidRealm1,
                                "",
                                "",
                                u"",
                                u"",
                                u"",
                                u"username_value_3",
                                u"password_value_3",
                                kTestLastUsageTime,
                                1};
  std::unique_ptr<PasswordForm> credential =
      FillPasswordFormWithData(form_data, /*is_account_store=*/GetParam());
  backend->AddLoginAsync(FromPasswordForm(*credential), base::DoNothing());
  RunUntilIdle();

  PasswordFormDigest observed_form = {PasswordForm::Scheme::kHtml,
                                      kTestWebRealm1, GURL(kTestWebOrigin1)};

  mock_affiliated_match_helper->ExpectCallToGetAffiliatedAndGrouped(
      observed_form, {kTestAndroidRealm1});

  std::vector<MockAffiliatedMatchHelper::AffiliationAndBrandingInformation>
      affiliation_info_for_results = {
          {kTestWebRealm1, kTestAndroidName1, GURL(kTestAndroidIconURL1)}};
  mock_affiliated_match_helper
      ->ExpectCallToInjectAffiliationAndBrandingInformation(
          std::move(affiliation_info_for_results));

  credential->match_type = PasswordForm::MatchType::kAffiliated;
  credential->affiliated_web_realm = kTestWebRealm1;
  credential->app_display_name = kTestAndroidName1;
  credential->app_icon_url = GURL(kTestAndroidIconURL1);

  std::vector<PasswordForm> expected_results = {*credential};
  base::MockCallback<BackendLoginsOrErrorReply> mock_reply;
  EXPECT_CALL(mock_reply,
              Run(VariantWith<BackendLoginsResult>(
                  MatchesFormsIgnoringPrimaryKey(expected_results))));

  backend->GetGroupedMatchingLoginsAsync(observed_form, mock_reply.Get());
  RunUntilIdle();
}

TEST_P(PasswordStoreBuiltInBackendTest,
       GetAllLoginsWithAffiliationAndBrandingInformation) {
  auto owning_mock_match_helper =
      std::make_unique<MockAffiliatedMatchHelper>(&fake_affiliation_service_);
  MockAffiliatedMatchHelper* mock_affiliated_match_helper =
      owning_mock_match_helper.get();
  PasswordStoreBackend* backend =
      CreateBackend(nullptr, std::move(owning_mock_match_helper));
  InitializeBackend(backend);

  std::vector<std::unique_ptr<PasswordForm>> all_credentials;
  for (const auto& test_credential : kTestCredentials) {
    all_credentials.push_back(FillPasswordFormWithData(
        test_credential, /*is_account_store=*/GetParam()));
    backend->AddLoginAsync(FromPasswordForm(*all_credentials.back()),
                           base::DoNothing());
    RunUntilIdle();
  }

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

  mock_affiliated_match_helper
      ->ExpectCallToInjectAffiliationAndBrandingInformation(
          affiliation_info_for_results);

  for (size_t i = 0; i < expected_results.size(); ++i) {
    expected_results[i].affiliated_web_realm =
        affiliation_info_for_results[i].affiliated_web_realm;
    expected_results[i].app_display_name =
        affiliation_info_for_results[i].app_display_name;
    expected_results[i].app_icon_url =
        affiliation_info_for_results[i].app_icon_url;
  }

  base::MockCallback<BackendLoginsOrErrorReply> mock_reply;
  EXPECT_CALL(mock_reply,
              Run(VariantWith<BackendLoginsResult>(
                  MatchesFormsIgnoringPrimaryKey(expected_results))));

  backend->GetAllLoginsWithAffiliationAndBrandingAsync(mock_reply.Get());
  RunUntilIdle();
}

TEST_P(PasswordStoreBuiltInBackendTest, NotAbleSavePasswordsWhenDatabaseIsBad) {
  PasswordStoreBackend* bad_backend =
      CreateBackend(std::make_unique<BadLoginDatabase>(GetParam()));
  InitializeBackend(bad_backend);

  EXPECT_EQ(bad_backend->GetError(), ActionableError::kInactionable);
}

INSTANTIATE_TEST_SUITE_P(, PasswordStoreBuiltInBackendTest, ::testing::Bool());

struct PasswordLossMetricsTestCase {
  bool is_account_store;
  PasswordStoreChange::Type change_type;
  int account_removals_bitmask;
  int profile_removals_bitmask;
};

class PasswordStoreBuiltInBackendPasswordLossMetricsTest
    : public testing::WithParamInterface<PasswordLossMetricsTestCase>,
      public PasswordStoreBuiltInBackendBaseTest {
 public:
  PasswordStoreBuiltInBackendPasswordLossMetricsTest() {
    os_crypt_async_ = os_crypt_async::GetTestOSCryptAsyncForTesting(
        /*is_sync_for_unittests=*/true);
  }

  PasswordStoreBackend* Initialize() {
    std::unique_ptr<LoginDatabase> database = std::make_unique<LoginDatabase>(
        test_login_db_file_path(),
        password_manager::IsAccountStore(GetParam().is_account_store));

    auto mock_match_helper =
        std::make_unique<MockAffiliatedMatchHelper>(&fake_affiliation_service_);

    store_ = std::make_unique<PasswordStoreBuiltInBackend>(
        std::move(database), syncer::WipeModelUponSyncDisabledBehavior::kNever,
        pref_service(), os_crypt_async_.get(), std::move(mock_match_helper));
    PasswordStoreBackend* backend = store_.get();
    backend->InitBackend(
        /*remote_form_changes_received=*/base::DoNothing(),
        /*sync_enabled_or_disabled_cb=*/base::DoNothing(),
        /*completion=*/base::DoNothing());
    RunUntilIdle();
    return backend;
  }

 protected:
  base::PassKey<class PasswordStoreBuiltInBackendPasswordLossMetricsTest>
      pass_key = base::PassKey<
          class PasswordStoreBuiltInBackendPasswordLossMetricsTest>();

 private:
  std::unique_ptr<os_crypt_async::OSCryptAsync> os_crypt_async_;
};

TEST_P(PasswordStoreBuiltInBackendPasswordLossMetricsTest,
       SyncChangeRecordsPasswordRemoval) {
  const PasswordLossMetricsTestCase& test_case = GetParam();

  PasswordStoreBackend* backend = Initialize();
  PasswordForm form = *FillPasswordFormWithData(CreateTestPasswordFormData(),
                                                test_case.is_account_store);

  backend->AddLoginAsync(FromPasswordForm(form),
                         /*callback=*/base::DoNothing());
  RunUntilIdle();

  PasswordStoreChangeList changes;
  changes.emplace_back(test_case.change_type,
                       FromPasswordForm(std::move(form)));
  (static_cast<PasswordStoreBuiltInBackend*>(backend))
      ->NotifyCredentialsChangedForTesting(pass_key, changes);
  RunUntilIdle();

  // Verify that password removal reason was tracked in the pref for the correct
  // store and only for removal change type.
  EXPECT_EQ(pref_service()->GetInteger(
                password_manager::prefs::kPasswordRemovalReasonForAccount),
            test_case.account_removals_bitmask);
  EXPECT_EQ(pref_service()->GetInteger(
                password_manager::prefs::kPasswordRemovalReasonForProfile),
            test_case.profile_removals_bitmask);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    PasswordStoreBuiltInBackendPasswordLossMetricsTest,
    ::testing::Values(
        PasswordLossMetricsTestCase(/*is_account_store=*/true,
                                    PasswordStoreChange::Type::ADD,
                                    /*account_removals_bitmask=*/0,
                                    /*profile_removals_bitmask=*/0),
        PasswordLossMetricsTestCase(/*is_account_store=*/true,
                                    PasswordStoreChange::Type::UPDATE,
                                    /*account_removals_bitmask=*/0,
                                    /*profile_removals_bitmask=*/0),
        PasswordLossMetricsTestCase(
            /*is_account_store=*/true,
            PasswordStoreChange::Type::REMOVE,
            /*account_removals_bitmask=*/
            (1 << static_cast<int>(
                 metrics_util::PasswordManagerCredentialRemovalReason::kSync)),
            /*profile_removals_bitmask=*/0),
        PasswordLossMetricsTestCase(/*is_account_store=*/false,
                                    PasswordStoreChange::Type::ADD,
                                    /*account_removals_bitmask=*/0,
                                    /*profile_removals_bitmask=*/0),
        PasswordLossMetricsTestCase(/*is_account_store=*/false,
                                    PasswordStoreChange::Type::UPDATE,
                                    /*account_removals_bitmask=*/0,
                                    /*profile_removals_bitmask=*/0),
        PasswordLossMetricsTestCase(
            /*is_account_store=*/false,
            PasswordStoreChange::Type::REMOVE,
            /*account_removals_bitmask=*/0,
            /*profile_removals_bitmask=*/
            (1 << static_cast<int>(
                 metrics_util::PasswordManagerCredentialRemovalReason::
                     kSync)))),
    [](const ::testing::TestParamInfo<
        PasswordStoreBuiltInBackendPasswordLossMetricsTest::ParamType>& info) {
      std::string test_suffix =
          info.param.is_account_store ? "AccountStore" : "ProfileStore";
      switch (info.param.change_type) {
        case PasswordStoreChange::Type::ADD:
          test_suffix += "_PwdAddition";
          break;
        case PasswordStoreChange::Type::UPDATE:
          test_suffix += "_PwdUpdate";
          break;
        case PasswordStoreChange::Type::REMOVE:
          test_suffix += "_PwdRemoval";
          break;
      }
      return test_suffix;
    });

}  // namespace password_manager
