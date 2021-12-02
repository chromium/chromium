// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/built_in_backend_to_android_backend_migrator.h"

#include "base/feature_list.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/password_manager/core/browser/fake_password_store_backend.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Eq;
using ::testing::Pointee;
using ::testing::UnorderedElementsAreArray;

namespace password_manager {

namespace {

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

class BuiltInBackendToAndroidBackendMigratorTest : public testing::Test {
 protected:
  BuiltInBackendToAndroidBackendMigratorTest() {
    prefs_ = std::make_unique<TestingPrefServiceSimple>();
    prefs_->registry()->RegisterIntegerPref(
        prefs::kCurrentMigrationVersionToGoogleMobileServices, 0);
    prefs_->registry()->RegisterDoublePref(prefs::kTimeOfLastMigrationAttempt,
                                           0.0);
    migrator_ = std::make_unique<BuiltInBackendToAndroidBackendMigrator>(
        &built_in_backend_, &android_backend_, prefs_.get(),
        /*is_syncing_passwords_callback=*/is_sync_enabled_callback_.Get());
  }

  ~BuiltInBackendToAndroidBackendMigratorTest() override = default;

  PasswordStoreBackend& built_in_backend() { return built_in_backend_; }
  PasswordStoreBackend& android_backend() { return android_backend_; }

  base::test::ScopedFeatureList& feature_list() { return feature_list_; }
  TestingPrefServiceSimple* prefs() { return prefs_.get(); }
  BuiltInBackendToAndroidBackendMigrator* migrator() { return migrator_.get(); }

  void RunUntilIdle() { task_env_.RunUntilIdle(); }

  void ExpectSyncCallbackAndSetResult(bool enabled) {
    EXPECT_CALL(is_sync_enabled_callback_, Run())
        .WillOnce(testing::Return(enabled));
  }

 private:
  base::test::SingleThreadTaskEnvironment task_env_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<TestingPrefServiceSimple> prefs_;
  FakePasswordStoreBackend built_in_backend_;
  FakePasswordStoreBackend android_backend_;
  std::unique_ptr<BuiltInBackendToAndroidBackendMigrator> migrator_;
  base::MockCallback<base::RepeatingCallback<bool(void)>>
      is_sync_enabled_callback_;
};

TEST_F(BuiltInBackendToAndroidBackendMigratorTest,
       CurrentMigrationVersionIsUpdatedWhenMigrationIsNeeded_SyncOn) {
  feature_list().InitAndEnableFeatureWithParameters(
      /*enabled_feature=*/features::kUnifiedPasswordManagerMigration,
      {{"migration_version", "1"}});
  ExpectSyncCallbackAndSetResult(true);

  migrator()->StartMigrationIfNecessary();
  RunUntilIdle();

  EXPECT_EQ(1, prefs()->GetInteger(
                   prefs::kCurrentMigrationVersionToGoogleMobileServices));
  // Since for syncing users we don't manually migrate passwords
  // |kTimeOfLastMigrationAttempt| shouldn't be updated.
  EXPECT_EQ(0, prefs()->GetDouble(
                   password_manager::prefs::kTimeOfLastMigrationAttempt));
}

TEST_F(BuiltInBackendToAndroidBackendMigratorTest,
       AllPrefsAreUpdatedWhenMigrationIsNeeded_SyncOff) {
  feature_list().InitAndEnableFeatureWithParameters(
      /*enabled_feature=*/features::kUnifiedPasswordManagerMigration,
      {{"migration_version", "1"}});
  ExpectSyncCallbackAndSetResult(false);

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
      features::kUnifiedPasswordManagerMigration, {{"migration_version", "1"}});
  prefs()->SetDouble(password_manager::prefs::kTimeOfLastMigrationAttempt,
                     (base::Time::Now() - base::Hours(2)).ToDoubleT());
  ExpectSyncCallbackAndSetResult(false);

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
      /*enabled_features=*/{{features::kUnifiedPasswordManagerMigration,
                             {{"migration_version", "1"}}}},
      /*disabled_features=*/{features::kUnifiedPasswordManagerAndroid});
  prefs()->SetInteger(prefs::kCurrentMigrationVersionToGoogleMobileServices, 1);

  ExpectSyncCallbackAndSetResult(false);

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
      /*enabled_features=*/{{features::kUnifiedPasswordManagerMigration,
                             {{"migration_version", "1"}}},
                            {features::kUnifiedPasswordManagerAndroid, {{}}}},
      /*disabled_features=*/{});
  prefs()->SetInteger(prefs::kCurrentMigrationVersionToGoogleMobileServices, 1);

  ExpectSyncCallbackAndSetResult(false);

  migrator()->StartMigrationIfNecessary();
  RunUntilIdle();

  EXPECT_EQ(1, prefs()->GetInteger(
                   prefs::kCurrentMigrationVersionToGoogleMobileServices));
  EXPECT_EQ(
      base::Time::Now().ToDoubleT(),
      prefs()->GetDouble(password_manager::prefs::kTimeOfLastMigrationAttempt));
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
};

class BuiltInBackendToAndroidBackendMigratorTestWithMigrationParams
    : public BuiltInBackendToAndroidBackendMigratorTest,
      public testing::WithParamInterface<MigrationParam> {};

// Tests the initial migration result.
TEST_P(BuiltInBackendToAndroidBackendMigratorTestWithMigrationParams,
       InitialMigration) {
  ExpectSyncCallbackAndSetResult(false);

  feature_list().InitAndEnableFeatureWithParameters(
      /*enabled_feature=*/features::kUnifiedPasswordManagerMigration,
      {{"migration_version", "1"}});

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
      /*enabled_features=*/{{features::kUnifiedPasswordManagerMigration,
                             {{"migration_version", "1"}}},
                            {features::kUnifiedPasswordManagerAndroid, {{}}}},
      /*disabled_features=*/{});
  prefs()->SetDouble(password_manager::prefs::kTimeOfLastMigrationAttempt,
                     (base::Time::Now() - base::Days(2)).ToDoubleT());
  prefs()->SetInteger(prefs::kCurrentMigrationVersionToGoogleMobileServices, 1);

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
                       .merged_logins = {}},
        MigrationParam{.built_in_logins = {{1}, {2}},
                       .android_logins = {},
                       .merged_logins = {{1}, {2}}},
        MigrationParam{.built_in_logins = {},
                       .android_logins = {{1}, {2}},
                       .merged_logins = {{1}, {2}}},
        MigrationParam{.built_in_logins = {{1}, {2}},
                       .android_logins = {{3}},
                       .merged_logins = {{1}, {2}, {3}}},
        MigrationParam{.built_in_logins = {{1}, {2}, {3}},
                       .android_logins = {{1}, {2}, {3}},
                       .merged_logins = {{1}, {2}, {3}}},
        MigrationParam{
            .built_in_logins = {{1, "old_password", base::Days(1)}, {2}},
            .android_logins = {{1, "new_password", base::Days(2)}, {3}},
            .merged_logins = {{1, "new_password", base::Days(2)}, {2}, {3}}},
        MigrationParam{
            .built_in_logins = {{1, "new_password", base::Days(2)}, {2}},
            .android_logins = {{1, "old_password", base::Days(1)}, {3}},
            .merged_logins = {{1, "new_password", base::Days(2)}, {2}, {3}}}));

}  // namespace password_manager
