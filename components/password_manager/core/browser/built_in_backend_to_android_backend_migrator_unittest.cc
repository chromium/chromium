// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/built_in_backend_to_android_backend_migrator.h"

#include "base/feature_list.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/password_manager/core/browser/fake_password_store_backend.h"
#include "components/password_manager/core/browser/mock_password_store_backend.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_store_backend.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/sync/driver/test_sync_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Eq;
using ::testing::Invoke;
using ::testing::IsEmpty;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::UnorderedElementsAreArray;
using ::testing::VariantWith;
using ::testing::WithArg;

namespace password_manager {

namespace {

constexpr base::TimeDelta kLatencyDelta = base::Milliseconds(123u);

PasswordForm CreateTestPasswordForm(int index = 0) {
  PasswordForm form;
  form.url = GURL("https://test" + base::NumberToString(index) + ".com");
  form.signon_realm = form.url.spec();
  form.username_value = u"username" + base::NumberToString16(index);
  form.password_value = u"password" + base::NumberToString16(index);
  form.in_store = PasswordForm::Store::kProfileStore;
  return form;
}

}  // namespace

// Checks that initial/rolling migration is started only when all the conditions
// are satisfied. It also check that migration result is properly recorded in
// prefs.
class BuiltInBackendToAndroidBackendMigratorTest : public testing::Test {
 protected:
  BuiltInBackendToAndroidBackendMigratorTest() = default;
  ~BuiltInBackendToAndroidBackendMigratorTest() override = default;

  void Init(int current_migration_version = 0) {
    prefs_.registry()->RegisterIntegerPref(
        prefs::kCurrentMigrationVersionToGoogleMobileServices, 0);
    prefs_.SetInteger(prefs::kCurrentMigrationVersionToGoogleMobileServices,
                      current_migration_version);
    prefs_.registry()->RegisterDoublePref(prefs::kTimeOfLastMigrationAttempt,
                                          0.0);
    prefs_.registry()->RegisterBooleanPref(
        prefs::kRequiresMigrationAfterSyncStatusChange, false);
    prefs_.registry()->RegisterStringPref(::prefs::kGoogleServicesLastUsername,
                                          "testaccount@gmail.com");
    CreateMigrator(&built_in_backend_, &android_backend_, &prefs_);
  }

  void InitSyncService(bool is_password_sync_enabled) {
    if (is_password_sync_enabled) {
      sync_service_.GetUserSettings()->SetSelectedTypes(
          /*sync_everything=*/false,
          /*types=*/{syncer::UserSelectableType::kPasswords});
    } else {
      sync_service_.GetUserSettings()->SetSelectedTypes(
          /*sync_everything=*/false, /*types=*/{});
    }
    migrator()->OnSyncServiceInitialized(&sync_service_);
  }

  void CreateMigrator(PasswordStoreBackend* built_in_backend,
                      PasswordStoreBackend* android_backend,
                      PrefService* prefs) {
    migrator_ = std::make_unique<BuiltInBackendToAndroidBackendMigrator>(
        built_in_backend, android_backend, prefs);
  }

  PasswordStoreBackend& built_in_backend() { return built_in_backend_; }
  PasswordStoreBackend& android_backend() { return android_backend_; }

  base::test::ScopedFeatureList& feature_list() { return feature_list_; }
  TestingPrefServiceSimple* prefs() { return &prefs_; }
  BuiltInBackendToAndroidBackendMigrator* migrator() { return migrator_.get(); }

  void RunUntilIdle() { task_env_.RunUntilIdle(); }
  void FastForwardBy(base::TimeDelta delta) { task_env_.FastForwardBy(delta); }

 private:
  base::test::SingleThreadTaskEnvironment task_env_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList feature_list_;
  TestingPrefServiceSimple prefs_;
  syncer::TestSyncService sync_service_;
  FakePasswordStoreBackend built_in_backend_;
  FakePasswordStoreBackend android_backend_{
      IsAccountStore(false),
      FakePasswordStoreBackend::UpdateAlwaysSucceeds(true)};
  std::unique_ptr<BuiltInBackendToAndroidBackendMigrator> migrator_;
};

TEST_F(BuiltInBackendToAndroidBackendMigratorTest,
       CurrentMigrationVersionIsUpdatedWhenMigrationIsNeeded_SyncOn) {
  feature_list().InitAndEnableFeatureWithParameters(
      /*enabled_feature=*/features::kUnifiedPasswordManagerAndroid,
      {{"migration_version", "1"}, {"stage", "0"}});
  Init();
  InitSyncService(/*is_password_sync_enabled=*/true);

  migrator()->StartMigrationIfNecessary();
  RunUntilIdle();

  EXPECT_EQ(1, prefs()->GetInteger(
                   prefs::kCurrentMigrationVersionToGoogleMobileServices));
  EXPECT_EQ(
      base::Time::Now().ToDoubleT(),
      prefs()->GetDouble(password_manager::prefs::kTimeOfLastMigrationAttempt));
}

TEST_F(BuiltInBackendToAndroidBackendMigratorTest,
       PrefsUnchangedWhenMigrationIsNeeded_SyncOff) {
  feature_list().InitAndEnableFeatureWithParameters(
      /*enabled_feature=*/features::kUnifiedPasswordManagerAndroid,
      {{"migration_version", "1"}, {"stage", "0"}});
  Init();

  InitSyncService(/*is_password_sync_enabled=*/false);

  migrator()->StartMigrationIfNecessary();
  RunUntilIdle();

  EXPECT_EQ(0, prefs()->GetInteger(
                   prefs::kCurrentMigrationVersionToGoogleMobileServices));
  EXPECT_EQ(0, prefs()->GetDouble(
                   password_manager::prefs::kTimeOfLastMigrationAttempt));
}

TEST_F(BuiltInBackendToAndroidBackendMigratorTest,
       AllPrefsAreUpdatedWhenMigrationIsNeeded_SyncOff) {
  feature_list().InitAndEnableFeatureWithParameters(
      /*enabled_feature=*/features::kUnifiedPasswordManagerAndroid,
      {{"migration_version", "1"}, {"stage", "3"}});
  Init();

  InitSyncService(/*is_password_sync_enabled=*/false);

  migrator()->StartMigrationIfNecessary();
  RunUntilIdle();

  EXPECT_EQ(1, prefs()->GetInteger(
                   prefs::kCurrentMigrationVersionToGoogleMobileServices));
  EXPECT_EQ(
      base::Time::Now().ToDoubleT(),
      prefs()->GetDouble(password_manager::prefs::kTimeOfLastMigrationAttempt));
}

TEST_F(BuiltInBackendToAndroidBackendMigratorTest,
       PrefsUnchangedWhenAttemptedMigrationEarlierToday) {
  feature_list().InitAndEnableFeatureWithParameters(
      features::kUnifiedPasswordManagerAndroid,
      {{"migration_version", "1"}, {"stage", "0"}});
  Init();

  prefs()->SetDouble(password_manager::prefs::kTimeOfLastMigrationAttempt,
                     (base::Time::Now() - base::Hours(2)).ToDoubleT());

  migrator()->StartMigrationIfNecessary();
  RunUntilIdle();

  EXPECT_EQ(0, prefs()->GetInteger(
                   prefs::kCurrentMigrationVersionToGoogleMobileServices));
  EXPECT_EQ(
      (base::Time::Now() - base::Hours(2)).ToDoubleT(),
      prefs()->GetDouble(password_manager::prefs::kTimeOfLastMigrationAttempt));
}

TEST_F(BuiltInBackendToAndroidBackendMigratorTest,
       LastAttemptUnchangedWhenRollingMigrationDisabled) {
  // Setup the pref to indicate that the initial migration has happened already.
  feature_list().InitWithFeaturesAndParameters(
      /*enabled_features=*/{{features::kUnifiedPasswordManagerAndroid,
                             {{"migration_version", "1"}, {"stage", "1"}}}},
      /*disabled_features=*/{});
  Init(/*current_migration_version=*/1);

  migrator()->StartMigrationIfNecessary();
  RunUntilIdle();

  EXPECT_EQ(1, prefs()->GetInteger(
                   prefs::kCurrentMigrationVersionToGoogleMobileServices));
  EXPECT_EQ(0, prefs()->GetDouble(
                   password_manager::prefs::kTimeOfLastMigrationAttempt));
}

TEST_F(BuiltInBackendToAndroidBackendMigratorTest,
       LastAttemptUpdatedInPrefsWhenRollingMigrationEnabled) {
  // Setup the pref to indicate that the initial migration has happened already.
  feature_list().InitWithFeaturesAndParameters(
      /*enabled_features=*/{{features::kUnifiedPasswordManagerAndroid,
                             {{"migration_version", "1"}, {"stage", "3"}}}},
      /*disabled_features=*/{});
  Init(/*current_migration_version=*/1);

  migrator()->StartMigrationIfNecessary();
  RunUntilIdle();

  EXPECT_EQ(1, prefs()->GetInteger(
                   prefs::kCurrentMigrationVersionToGoogleMobileServices));
  EXPECT_EQ(
      base::Time::Now().ToDoubleT(),
      prefs()->GetDouble(password_manager::prefs::kTimeOfLastMigrationAttempt));
}

TEST_F(BuiltInBackendToAndroidBackendMigratorTest,
       InitialMigrationNeverStartedMetrics) {
  base::HistogramTester histogram_tester;
  const char kMigrationFinishedMetric[] =
      "PasswordManager.UnifiedPasswordManager.WasMigrationDone";

  feature_list().InitAndEnableFeatureWithParameters(
      /*enabled_feature=*/features::kUnifiedPasswordManagerAndroid,
      {{"migration_version", "1"}, {"stage", "0"}});
  Init();

  histogram_tester.ExpectTotalCount(kMigrationFinishedMetric, 1);
  histogram_tester.ExpectBucketCount(kMigrationFinishedMetric, false, 1);
}

TEST_F(BuiltInBackendToAndroidBackendMigratorTest,
       InitialMigrationFinishedMetrics) {
  base::HistogramTester histogram_tester;
  const char kMigrationFinishedMetric[] =
      "PasswordManager.UnifiedPasswordManager.WasMigrationDone";

  feature_list().InitAndEnableFeatureWithParameters(
      /*enabled_feature=*/features::kUnifiedPasswordManagerAndroid,
      {{"migration_version", "1"}, {"stage", "0"}});
  Init(/*current_migration_version=*/1);

  histogram_tester.ExpectTotalCount(kMigrationFinishedMetric, 1);
  histogram_tester.ExpectBucketCount(kMigrationFinishedMetric, true, 1);
}

TEST_F(BuiltInBackendToAndroidBackendMigratorTest,
       InitialMigrationNeedsRestartMetrics) {
  base::HistogramTester histogram_tester;
  const char kMigrationFinishedMetric[] =
      "PasswordManager.UnifiedPasswordManager.WasMigrationDone";

  feature_list().InitAndEnableFeatureWithParameters(
      /*enabled_feature=*/features::kUnifiedPasswordManagerAndroid,
      {{"migration_version", "2"}, {"stage", "0"}});

  Init(/*current_migration_version=*/1);

  histogram_tester.ExpectTotalCount(kMigrationFinishedMetric, 1);
  histogram_tester.ExpectBucketCount(kMigrationFinishedMetric, false, 1);
}

TEST_F(BuiltInBackendToAndroidBackendMigratorTest,
       MigrationForSyncingUserShouldMoveLocalOnlyDataToAndroidBackend) {
  feature_list().InitAndEnableFeatureWithParameters(
      /*enabled_feature=*/features::kUnifiedPasswordManagerAndroid,
      {{"migration_version", "1"}, {"stage", "0"}});

  Init();
  InitSyncService(/*is_password_sync_enabled=*/true);

  PasswordForm form = CreateTestPasswordForm();
  android_backend().AddLoginAsync(form, base::DoNothing());

  // 'skip_zero_click' is a local only field in PasswordForm and hence not
  // available in Android backend before the migration.
  PasswordForm form_with_local_data = form;
  form_with_local_data.skip_zero_click = true;
  built_in_backend().AddLoginAsync(form_with_local_data, base::DoNothing());

  migrator()->StartMigrationIfNecessary();
  RunUntilIdle();

  base::MockCallback<LoginsOrErrorReply> mock_reply;
  std::vector<std::unique_ptr<PasswordForm>> expected_logins_android_backend;
  expected_logins_android_backend.push_back(
      std::make_unique<PasswordForm>(form_with_local_data));
  EXPECT_CALL(mock_reply,
              Run(LoginsResultsOrErrorAre(&expected_logins_android_backend)));
  android_backend().GetAllLoginsAsync(mock_reply.Get());
  RunUntilIdle();
}

TEST_F(BuiltInBackendToAndroidBackendMigratorTest,
       MigrationUserAfterSyncDisablingShouldMoveLocalOnlyDataToBuiltInBackend) {
  feature_list().InitAndEnableFeatureWithParameters(
      /*enabled_feature=*/features::kUnifiedPasswordManagerAndroid,
      {{"migration_version", "1"}, {"stage", "0"}});
  Init();

  // Simulate sync being recently disabled.
  prefs()->SetBoolean(prefs::kRequiresMigrationAfterSyncStatusChange, true);
  InitSyncService(/*is_password_sync_enabled=*/false);

  PasswordForm form = CreateTestPasswordForm();
  built_in_backend().AddLoginAsync(form, base::DoNothing());

  // 'skip_zero_click' is a local only field in PasswordForm and hence not
  // available in the built-in backend before the migration.
  PasswordForm form_with_local_data = form;
  form_with_local_data.skip_zero_click = true;
  android_backend().AddLoginAsync(form_with_local_data, base::DoNothing());

  migrator()->StartMigrationIfNecessary();
  RunUntilIdle();

  base::MockCallback<LoginsOrErrorReply> mock_reply;
  std::vector<std::unique_ptr<PasswordForm>> expected_logins_built_in_backend;
  expected_logins_built_in_backend.push_back(
      std::make_unique<PasswordForm>(form_with_local_data));
  EXPECT_CALL(mock_reply,
              Run(LoginsResultsOrErrorAre(&expected_logins_built_in_backend)));
  android_backend().GetAllLoginsAsync(mock_reply.Get());
  RunUntilIdle();
}

// Tests that migration removes blocklisted entries with non-empty username or
// values from the built in backlend before writing to the Android backend.
TEST_F(BuiltInBackendToAndroidBackendMigratorTest,
       MigrationClearsBlocklistedCredentials) {
  feature_list().InitAndEnableFeatureWithParameters(
      /*enabled_feature=*/features::kUnifiedPasswordManagerAndroid,
      {{"migration_version", "1"}, {"stage", "0"}});

  Init();
  InitSyncService(/*is_password_sync_enabled=*/true);

  // Add two incorrect entries to the local database to check if they will be
  // removed before writing to the android backend
  PasswordForm form_1 = CreateTestPasswordForm(1);
  form_1.blocked_by_user = true;
  form_1.username_value.clear();
  built_in_backend().AddLoginAsync(form_1, base::DoNothing());

  PasswordForm form_2 = CreateTestPasswordForm(2);
  form_2.blocked_by_user = true;
  form_1.password_value.clear();
  built_in_backend().AddLoginAsync(form_2, base::DoNothing());

  migrator()->StartMigrationIfNecessary();
  RunUntilIdle();

  base::MockCallback<LoginsOrErrorReply> mock_reply;
  // Credentials should be cleaned in both android and built in backends.
  EXPECT_CALL(mock_reply, Run(VariantWith<LoginsResult>((IsEmpty())))).Times(2);
  android_backend().GetAllLoginsAsync(mock_reply.Get());
  built_in_backend().GetAllLoginsAsync(mock_reply.Get());
  RunUntilIdle();
}

// Tests that migration does not affect username and password for
// non-blocklisted entries.
TEST_F(BuiltInBackendToAndroidBackendMigratorTest,
       MigrationDoesNotClearNonBlocklistedCredentials) {
  feature_list().InitAndEnableFeatureWithParameters(
      /*enabled_feature=*/features::kUnifiedPasswordManagerAndroid,
      {{"migration_version", "1"}, {"stage", "0"}});

  Init();
  InitSyncService(/*is_password_sync_enabled=*/true);

  // Add two incorrect entries to the local database to check if they will be
  // fixed before writing to the android backend
  PasswordForm form_1 = CreateTestPasswordForm(1);
  built_in_backend().AddLoginAsync(form_1, base::DoNothing());

  PasswordForm form_2 = CreateTestPasswordForm(2);
  built_in_backend().AddLoginAsync(form_2, base::DoNothing());

  // Add one form to be updated.
  android_backend().AddLoginAsync(form_1, base::DoNothing());
  RunUntilIdle();

  migrator()->StartMigrationIfNecessary();
  RunUntilIdle();

  base::MockCallback<LoginsOrErrorReply> mock_reply;
  std::vector<std::unique_ptr<PasswordForm>> expected_logins;
  expected_logins.push_back(std::make_unique<PasswordForm>(form_1));
  expected_logins.push_back(std::make_unique<PasswordForm>(form_2));

  // Credentials should be cleaned in both android and built in backends.
  EXPECT_CALL(mock_reply, Run(LoginsResultsOrErrorAre(&expected_logins)))
      .Times(2);
  android_backend().GetAllLoginsAsync(mock_reply.Get());
  built_in_backend().GetAllLoginsAsync(mock_reply.Get());
  RunUntilIdle();
}

// Holds the built in and android backend's logins and the expected result after
// the migration.
struct MigrationParam {
  struct Entry {
    Entry(int index,
          std::string password = "",
          base::TimeDelta date_created = base::TimeDelta())
        : index(index), password(password), date_created(date_created) {}

    std::unique_ptr<PasswordForm> ToPasswordForm() const {
      PasswordForm form = CreateTestPasswordForm(index);
      form.password_value = base::ASCIIToUTF16(password);
      form.date_created = base::Time() + date_created;
      return std::make_unique<PasswordForm>(form);
    }

    int index;
    std::string password;
    base::TimeDelta date_created;
  };

  std::vector<std::unique_ptr<PasswordForm>> GetBuiltInLogins() const {
    return EntriesToPasswordForms(built_in_logins);
  }

  std::vector<std::unique_ptr<PasswordForm>> GetAndroidLogins() const {
    return EntriesToPasswordForms(android_logins);
  }

  std::vector<std::unique_ptr<PasswordForm>> GetMergedLogins() const {
    return EntriesToPasswordForms(merged_logins);
  }

  std::vector<std::unique_ptr<PasswordForm>> GetUpdatedAndroidLogins() const {
    return EntriesToPasswordForms(updated_android_logins);
  }

  std::vector<std::unique_ptr<PasswordForm>> EntriesToPasswordForms(
      const std::vector<Entry>& entries) const {
    std::vector<std::unique_ptr<PasswordForm>> v;
    base::ranges::transform(entries, std::back_inserter(v),
                            &Entry::ToPasswordForm);
    return v;
  }

  std::vector<Entry> built_in_logins;
  std::vector<Entry> android_logins;
  std::vector<Entry> merged_logins;
  std::vector<Entry> updated_android_logins;
};

// Tests that initial and rolling migration actually works by comparing
// passwords in built-in/android backend before and after migration.
class BuiltInBackendToAndroidBackendMigratorTestWithMigrationParams
    : public BuiltInBackendToAndroidBackendMigratorTest,
      public testing::WithParamInterface<MigrationParam> {};

// Tests the initial migration result.
TEST_P(BuiltInBackendToAndroidBackendMigratorTestWithMigrationParams,
       InitialMigrationForSyncingUsers) {
  BuiltInBackendToAndroidBackendMigratorTest::Init();

  InitSyncService(/*is_password_sync_enabled=*/true);

  feature_list().InitAndEnableFeatureWithParameters(
      /*enabled_feature=*/features::kUnifiedPasswordManagerAndroid,
      {{"migration_version", "1"}, {"stage", "0"}});

  const MigrationParam& p = GetParam();

  for (const auto& login : p.GetBuiltInLogins()) {
    built_in_backend().AddLoginAsync(*login, base::DoNothing());
  }
  for (const auto& login : p.GetAndroidLogins()) {
    android_backend().AddLoginAsync(*login, base::DoNothing());
  }
  RunUntilIdle();

  migrator()->StartMigrationIfNecessary();
  RunUntilIdle();

  // The built-in logins should not be affected.
  base::MockCallback<LoginsOrErrorReply> built_in_reply;
  auto built_in_logins = p.GetBuiltInLogins();
  EXPECT_CALL(built_in_reply, Run(LoginsResultsOrErrorAre(&built_in_logins)));
  built_in_backend().GetAllLoginsAsync(built_in_reply.Get());

  // The android logins are updated. Existing logins are retained.
  base::MockCallback<LoginsOrErrorReply> android_reply;
  auto updated_logins = p.GetUpdatedAndroidLogins();
  EXPECT_CALL(android_reply, Run(LoginsResultsOrErrorAre(&updated_logins)));
  android_backend().GetAllLoginsAsync(android_reply.Get());
  RunUntilIdle();
}

// Tests the initial migration result.
TEST_P(BuiltInBackendToAndroidBackendMigratorTestWithMigrationParams,
       InitialMigration) {
  BuiltInBackendToAndroidBackendMigratorTest::Init();

  InitSyncService(/*is_password_sync_enabled=*/false);

  feature_list().InitAndEnableFeatureWithParameters(
      /*enabled_feature=*/features::kUnifiedPasswordManagerAndroid,
      {{"migration_version", "1"}, {"stage", "3"}});

  const MigrationParam& p = GetParam();

  for (const auto& login : p.GetBuiltInLogins()) {
    built_in_backend().AddLoginAsync(*login, base::DoNothing());
  }
  for (const auto& login : p.GetAndroidLogins()) {
    android_backend().AddLoginAsync(*login, base::DoNothing());
  }
  RunUntilIdle();

  migrator()->StartMigrationIfNecessary();
  RunUntilIdle();

  for (auto* const backend : {&android_backend(), &built_in_backend()}) {
    base::MockCallback<LoginsOrErrorReply> mock_reply;
    auto expected_logins = p.GetMergedLogins();
    EXPECT_CALL(mock_reply, Run(LoginsResultsOrErrorAre(&expected_logins)));
    backend->GetAllLoginsAsync(mock_reply.Get());
    RunUntilIdle();
  }
}

TEST_P(BuiltInBackendToAndroidBackendMigratorTestWithMigrationParams,
       RollingMigration) {
  // Setup the pref to indicate that the initial migration has happened already.
  // This implies that rolling migration will take place!
  feature_list().InitWithFeaturesAndParameters(
      /*enabled_features=*/{{features::kUnifiedPasswordManagerAndroid,
                             {{"migration_version", "1"}, {"stage", "3"}}}},
      /*disabled_features=*/{});
  BuiltInBackendToAndroidBackendMigratorTest::Init(
      /*current_migration_version=*/1);

  const MigrationParam& p = GetParam();

  for (const auto& login : p.GetBuiltInLogins()) {
    built_in_backend().AddLoginAsync(*login, base::DoNothing());
  }
  for (const auto& login : p.GetAndroidLogins()) {
    android_backend().AddLoginAsync(*login, base::DoNothing());
  }
  RunUntilIdle();

  migrator()->StartMigrationIfNecessary();
  RunUntilIdle();

  for (auto* const backend : {&android_backend(), &built_in_backend()}) {
    base::MockCallback<LoginsOrErrorReply> mock_reply;
    auto expected_logins = p.GetAndroidLogins();
    EXPECT_CALL(mock_reply, Run(LoginsResultsOrErrorAre(&expected_logins)));
    backend->GetAllLoginsAsync(mock_reply.Get());
    RunUntilIdle();
  }
}

INSTANTIATE_TEST_SUITE_P(
    BuiltInBackendToAndroidBackendMigratorTest,
    BuiltInBackendToAndroidBackendMigratorTestWithMigrationParams,
    testing::Values(
        MigrationParam{.built_in_logins = {},
                       .android_logins = {},
                       .merged_logins = {},
                       .updated_android_logins = {}},
        MigrationParam{.built_in_logins = {{1}, {2}},
                       .android_logins = {},
                       .merged_logins = {{1}, {2}},
                       .updated_android_logins = {{1}, {2}}},
        MigrationParam{.built_in_logins = {},
                       .android_logins = {{1}, {2}},
                       .merged_logins = {{1}, {2}},
                       .updated_android_logins = {{1}, {2}}},
        MigrationParam{.built_in_logins = {{1}, {2}},
                       .android_logins = {{3}},
                       .merged_logins = {{1}, {2}, {3}},
                       .updated_android_logins = {{1}, {2}, {3}}},
        MigrationParam{.built_in_logins = {{1}, {2}, {3}},
                       .android_logins = {{1}, {2}, {3}},
                       .merged_logins = {{1}, {2}, {3}},
                       .updated_android_logins = {{1}, {2}, {3}}},
        MigrationParam{
            .built_in_logins = {{1, "old_password", base::Days(1)}, {2}},
            .android_logins = {{1, "new_password", base::Days(2)}, {3}},
            .merged_logins = {{1, "new_password", base::Days(2)}, {2}, {3}},
            .updated_android_logins = {{1, "old_password", base::Days(1)},
                                       {2},
                                       {3}}},
        MigrationParam{
            .built_in_logins = {{1, "new_password", base::Days(2)}, {2}},
            .android_logins = {{1, "old_password", base::Days(1)}, {3}},
            .merged_logins = {{1, "new_password", base::Days(2)}, {2}, {3}},
            .updated_android_logins = {{1, "new_password", base::Days(2)},
                                       {2},
                                       {3}}}));

struct MigrationParamForMetrics {
  // Whether this is initial or rolling migration.
  bool is_initial_migration;
  // Whether this migration only affects local-only data of sync users.
  bool is_sync_enabled;
  // Whether migration was completed successfully or not.
  bool is_successful_migration;
};

class BuiltInBackendToAndroidBackendMigratorTestMetrics
    : public BuiltInBackendToAndroidBackendMigratorTest,
      public testing::WithParamInterface<MigrationParamForMetrics> {
 protected:
  BuiltInBackendToAndroidBackendMigratorTestMetrics() {
    prefs()->registry()->RegisterIntegerPref(
        prefs::kCurrentMigrationVersionToGoogleMobileServices, 0);
    prefs()->registry()->RegisterDoublePref(prefs::kTimeOfLastMigrationAttempt,
                                            0.0);
    prefs()->registry()->RegisterBooleanPref(
        prefs::kRequiresMigrationAfterSyncStatusChange, false);
    if (GetParam().is_initial_migration) {
      feature_list().InitAndEnableFeatureWithParameters(
          /*enabled_feature=*/features::kUnifiedPasswordManagerAndroid,
          {{"migration_version", "1"}, {"stage", "0"}});
      latency_metric_ =
          "PasswordManager.UnifiedPasswordManager.InitialMigration.Latency";
      success_metric_ =
          "PasswordManager.UnifiedPasswordManager.InitialMigration.Success";
    } else {
      feature_list().InitWithFeaturesAndParameters(
          /*enabled_features=*/{{features::kUnifiedPasswordManagerAndroid,
                                 {{"migration_version", "1"}, {"stage", "0"}}}},
          /*disabled_features=*/{});
      // Setup the pref to indicate that the initial migration has happened
      // already.
      prefs()->SetInteger(prefs::kCurrentMigrationVersionToGoogleMobileServices,
                          1);
      latency_metric_ =
          "PasswordManager.UnifiedPasswordManager.RollingMigration.Latency";
      success_metric_ =
          "PasswordManager.UnifiedPasswordManager.RollingMigration.Success";
    }

    CreateMigrator(&built_in_backend_, &android_backend_, prefs());
  }

  std::string latency_metric_;
  std::string success_metric_;
  ::testing::StrictMock<MockPasswordStoreBackend> built_in_backend_;
  ::testing::StrictMock<MockPasswordStoreBackend> android_backend_;
};

TEST_P(BuiltInBackendToAndroidBackendMigratorTestMetrics,
       MigrationMetricsTest) {
  base::HistogramTester histogram_tester;

  // Initial migration only happens with sync enabled for now.
  InitSyncService(/*is_password_sync_enabled=*/GetParam().is_sync_enabled);

  EXPECT_CALL(built_in_backend_, GetAllLoginsAsync)
      .WillOnce(WithArg<0>(Invoke([](LoginsOrErrorReply reply) -> void {
        LoginsResultOrError result =
            GetParam().is_successful_migration
                ? LoginsResultOrError(LoginsResult())
                : LoginsResultOrError(PasswordStoreBackendError::kUnspecified);
        base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
            FROM_HERE, base::BindOnce(std::move(reply), std::move(result)),
            kLatencyDelta);
      })));

  // With sync enabled, the android backend should not contain relevant
  // differences and the additional call is unnecessary.
  if (!GetParam().is_sync_enabled) {
    EXPECT_CALL(android_backend_, GetAllLoginsAsync)
        .WillOnce(WithArg<0>(Invoke([](LoginsOrErrorReply reply) -> void {
          base::SequencedTaskRunnerHandle::Get()->PostTask(
              FROM_HERE, base::BindOnce(std::move(reply), LoginsResult()));
        })));
  }

  migrator()->StartMigrationIfNecessary();
  FastForwardBy(kLatencyDelta);

  histogram_tester.ExpectTotalCount(latency_metric_, 1);
  histogram_tester.ExpectTimeBucketCount(latency_metric_, kLatencyDelta, 1);
  histogram_tester.ExpectTotalCount(success_metric_, 1);
  histogram_tester.ExpectBucketCount(success_metric_, true,
                                     GetParam().is_successful_migration);
  histogram_tester.ExpectBucketCount(success_metric_, false,
                                     !GetParam().is_successful_migration);
}

// TODO(crbug.com/1306001): Add cases for rolling migration and non-syncing
// users or clean up.
INSTANTIATE_TEST_SUITE_P(
    BuiltInBackendToAndroidBackendMigratorTest,
    BuiltInBackendToAndroidBackendMigratorTestMetrics,
    testing::Values(MigrationParamForMetrics{.is_initial_migration = true,
                                             .is_sync_enabled = true,
                                             .is_successful_migration = true},
                    MigrationParamForMetrics{
                        .is_initial_migration = true,
                        .is_sync_enabled = true,
                        .is_successful_migration = false}));

class BuiltInBackendToAndroidBackendMigratorWithMockAndroidBackendTest
    : public BuiltInBackendToAndroidBackendMigratorTest {
 protected:
  BuiltInBackendToAndroidBackendMigratorWithMockAndroidBackendTest() {
    prefs()->registry()->RegisterIntegerPref(
        prefs::kCurrentMigrationVersionToGoogleMobileServices, 0);
    prefs()->registry()->RegisterDoublePref(prefs::kTimeOfLastMigrationAttempt,
                                            0.0);
    prefs()->registry()->RegisterBooleanPref(
        prefs::kRequiresMigrationAfterSyncStatusChange, false);
    feature_list().InitAndEnableFeatureWithParameters(
        /*enabled_feature=*/features::kUnifiedPasswordManagerAndroid,
        {{"migration_version", "1"}, {"stage", "0"}});

    CreateMigrator(&built_in_backend_, &android_backend_, prefs());
  }

  PasswordStoreBackend& built_in_backend() { return built_in_backend_; }

  ::testing::NiceMock<MockPasswordStoreBackend> android_backend_;

 private:
  FakePasswordStoreBackend built_in_backend_;
};

TEST_F(BuiltInBackendToAndroidBackendMigratorWithMockAndroidBackendTest,
       DoesNotCompleteMigrationWhenWritingToAndroidBackendFails_SyncOn) {
  InitSyncService(/*is_password_sync_enabled=*/true);

  // Add two credentials to the built-in backend.
  built_in_backend().AddLoginAsync(CreateTestPasswordForm(/*index=*/1),
                                   base::DoNothing());
  built_in_backend().AddLoginAsync(CreateTestPasswordForm(/*index=*/2),
                                   base::DoNothing());

  // Simulate an Android backend that fails to write.
  ON_CALL(android_backend_, UpdateLoginAsync)
      .WillByDefault(
          WithArg<1>(Invoke([](PasswordChangesOrErrorReply callback) -> void {
            base::SequencedTaskRunnerHandle::Get()->PostTask(
                FROM_HERE,
                base::BindOnce(std::move(callback),
                               PasswordStoreBackendError::kUnspecified));
          })));

  // Once one UpdateLoginAsync() call fails, all consecutive ones will not be
  // executed. Check that exactly one UpdateLoginAsync() is called.
  EXPECT_CALL(android_backend_, UpdateLoginAsync).Times(1);

  migrator()->StartMigrationIfNecessary();

  // Migration version is still 0 since migration didn't complete.
  EXPECT_EQ(0, prefs()->GetInteger(
                   prefs::kCurrentMigrationVersionToGoogleMobileServices));
  RunUntilIdle();
}

TEST_F(BuiltInBackendToAndroidBackendMigratorWithMockAndroidBackendTest,
       DoesNotCompleteMigrationWhenWritingToAndroidBackendFails_SyncOff) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      /*enabled_feature=*/features::kUnifiedPasswordManagerAndroid,
      {{"migration_version", "1"}, {"stage", "3"}});

  // Sync state doesn't affect this test, run it arbitrarily for non-sync'ing
  // users.
  InitSyncService(/*is_password_sync_enabled=*/false);

  // Add two credentials to the built-in backend.
  built_in_backend().AddLoginAsync(CreateTestPasswordForm(/*index=*/1),
                                   base::DoNothing());
  built_in_backend().AddLoginAsync(CreateTestPasswordForm(/*index=*/2),
                                   base::DoNothing());

  // Simulate an empty Android backend.
  EXPECT_CALL(android_backend_, GetAllLoginsAsync)
      .WillOnce(WithArg<0>(Invoke([](LoginsOrErrorReply reply) -> void {
        base::SequencedTaskRunnerHandle::Get()->PostTask(
            FROM_HERE, base::BindOnce(std::move(reply), LoginsResult()));
      })));

  // Simulate an Android backend that fails to write.
  ON_CALL(android_backend_, AddLoginAsync)
      .WillByDefault(
          WithArg<1>(Invoke([](PasswordChangesOrErrorReply callback) -> void {
            base::SequencedTaskRunnerHandle::Get()->PostTask(
                FROM_HERE,
                base::BindOnce(std::move(callback),
                               PasswordStoreBackendError::kUnspecified));
          })));

  // Once one AddLoginAsync() call fails, all consecutive ones will not be
  // executed. Check that exactly one AddLoginAsync() is called.
  EXPECT_CALL(android_backend_, AddLoginAsync).Times(1);

  migrator()->StartMigrationIfNecessary();

  // Migration version is still 0 since migration didn't complete.
  EXPECT_EQ(0, prefs()->GetInteger(
                   prefs::kCurrentMigrationVersionToGoogleMobileServices));
  RunUntilIdle();
}

}  // namespace password_manager
