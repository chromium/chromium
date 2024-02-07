// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store/split_stores_and_local_upm.h"

#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {
namespace {

class SplitStoresAndLocalUpmTest : public ::testing::Test {
 public:
  SplitStoresAndLocalUpmTest() {
    pref_service_.registry()->RegisterIntegerPref(
        prefs::kPasswordsUseUPMLocalAndSeparateStores,
        static_cast<int>(
            password_manager::prefs::UseUpmLocalAndSeparateStoresState::kOff));
  }

  PrefService* pref_service() { return &pref_service_; }

 private:
  TestingPrefServiceSimple pref_service_;
};

TEST_F(SplitStoresAndLocalUpmTest, UpmPrefOff) {
  EXPECT_FALSE(UsesSplitStoresAndUPMForLocal(pref_service()));
}

TEST_F(SplitStoresAndLocalUpmTest, UpmPrefOffAndMigrationPending) {
  pref_service()->SetInteger(
      prefs::kPasswordsUseUPMLocalAndSeparateStores,
      static_cast<int>(
          password_manager::prefs::UseUpmLocalAndSeparateStoresState::
              kOffAndMigrationPending));

  EXPECT_FALSE(UsesSplitStoresAndUPMForLocal(pref_service()));
}

TEST_F(SplitStoresAndLocalUpmTest, UpmPrefOn) {
  pref_service()->SetInteger(
      prefs::kPasswordsUseUPMLocalAndSeparateStores,
      static_cast<int>(
          password_manager::prefs::UseUpmLocalAndSeparateStoresState::kOn));

  EXPECT_TRUE(UsesSplitStoresAndUPMForLocal(pref_service()));
}

}  // namespace
}  // namespace password_manager
