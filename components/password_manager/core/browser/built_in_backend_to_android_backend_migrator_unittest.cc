// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/built_in_backend_to_android_backend_migrator.h"

#include "base/feature_list.h"
#include "base/test/scoped_feature_list.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

class BuiltInBackendToAndroidBackendMigratorTest : public testing::Test {
 protected:
  BuiltInBackendToAndroidBackendMigratorTest() {
    prefs_ = std::make_unique<TestingPrefServiceSimple>();
    prefs_->registry()->RegisterIntegerPref(
        prefs::kCurrentMigrationVersionToGoogleMobileServices, 0);
    migrator_ =
        std::make_unique<BuiltInBackendToAndroidBackendMigrator>(prefs_.get());
  }

  ~BuiltInBackendToAndroidBackendMigratorTest() override = default;

  base::test::ScopedFeatureList& feature_list() { return feature_list_; }
  TestingPrefServiceSimple* prefs() { return prefs_.get(); }
  BuiltInBackendToAndroidBackendMigrator* migrator() { return migrator_.get(); }

 private:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<TestingPrefServiceSimple> prefs_;
  std::unique_ptr<BuiltInBackendToAndroidBackendMigrator> migrator_;
};

TEST_F(BuiltInBackendToAndroidBackendMigratorTest, PrefUpdatedToNewerVersion) {
  feature_list().InitAndEnableFeatureWithParameters(
      features::kUnifiedPasswordManagerMigration, {{"migration_version", "1"}});
  migrator()->StartMigrationIfNecessary();

  EXPECT_EQ(1, prefs()->GetInteger(
                   prefs::kCurrentMigrationVersionToGoogleMobileServices));
}

TEST_F(BuiltInBackendToAndroidBackendMigratorTest, PrefUnchanged) {
  feature_list().InitAndEnableFeatureWithParameters(
      features::kUnifiedPasswordManagerMigration, {{"migration_version", "1"}});
  prefs()->SetInteger(prefs::kCurrentMigrationVersionToGoogleMobileServices, 2);
  migrator()->StartMigrationIfNecessary();

  EXPECT_EQ(2, prefs()->GetInteger(
                   prefs::kCurrentMigrationVersionToGoogleMobileServices));
}

}  // namespace password_manager
