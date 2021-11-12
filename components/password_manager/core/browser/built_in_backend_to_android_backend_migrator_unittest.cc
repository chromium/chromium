// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/built_in_backend_to_android_backend_migrator.h"

#include "base/feature_list.h"
#include "base/strings/utf_string_conversions.h"
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

using ::testing::ElementsAre;
using ::testing::Pair;

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
    migrator_ = std::make_unique<BuiltInBackendToAndroidBackendMigrator>(
        &built_in_backend_, &android_backend_, prefs_.get(),
        /*is_syncing_passwords_callback=*/base::BindRepeating([]() {
          return false;
        }));
  }

  ~BuiltInBackendToAndroidBackendMigratorTest() override = default;

  PasswordStoreBackend& built_in_backend() { return built_in_backend_; }
  FakePasswordStoreBackend& android_backend() { return android_backend_; }

  base::test::ScopedFeatureList& feature_list() { return feature_list_; }
  TestingPrefServiceSimple* prefs() { return prefs_.get(); }
  BuiltInBackendToAndroidBackendMigrator* migrator() { return migrator_.get(); }

  void RunUntilIdle() { task_env_.RunUntilIdle(); }

 private:
  base::test::SingleThreadTaskEnvironment task_env_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<TestingPrefServiceSimple> prefs_;
  FakePasswordStoreBackend built_in_backend_;
  FakePasswordStoreBackend android_backend_;
  std::unique_ptr<BuiltInBackendToAndroidBackendMigrator> migrator_;
};

TEST_F(BuiltInBackendToAndroidBackendMigratorTest,
       PrefUpdatedToNewerVersionWhenMigrationIsNecessary) {
  feature_list().InitAndEnableFeatureWithParameters(
      features::kUnifiedPasswordManagerMigration, {{"migration_version", "1"}});

  migrator()->StartMigrationIfNecessary();
  RunUntilIdle();

  EXPECT_EQ(1, prefs()->GetInteger(
                   prefs::kCurrentMigrationVersionToGoogleMobileServices));
}

TEST_F(BuiltInBackendToAndroidBackendMigratorTest,
       PrefUnchangedWhenMigrationIsNotNecessary) {
  feature_list().InitAndEnableFeatureWithParameters(
      features::kUnifiedPasswordManagerMigration, {{"migration_version", "1"}});
  prefs()->SetInteger(prefs::kCurrentMigrationVersionToGoogleMobileServices, 2);
  migrator()->StartMigrationIfNecessary();
  RunUntilIdle();

  EXPECT_EQ(2, prefs()->GetInteger(
                   prefs::kCurrentMigrationVersionToGoogleMobileServices));
}

TEST_F(BuiltInBackendToAndroidBackendMigratorTest,
       InitialMigrationCompleted_NoMergeRequired) {
  feature_list().InitAndEnableFeatureWithParameters(
      features::kUnifiedPasswordManagerMigration, {{"migration_version", "1"}});

  std::vector<PasswordForm> expected_logins = {CreateTestPasswordForm(0),
                                               CreateTestPasswordForm(1)};
  for (const auto& login : expected_logins) {
    built_in_backend().AddLoginAsync(login, base::DoNothing());
  }
  RunUntilIdle();

  migrator()->StartMigrationIfNecessary();
  RunUntilIdle();

  EXPECT_EQ(1, prefs()->GetInteger(
                   prefs::kCurrentMigrationVersionToGoogleMobileServices));
  EXPECT_THAT(android_backend().stored_passwords(),
              ElementsAre(Pair(expected_logins[0].signon_realm,
                               ElementsAre(expected_logins[0])),
                          Pair(expected_logins[1].signon_realm,
                               ElementsAre(expected_logins[1]))));
}

}  // namespace password_manager
