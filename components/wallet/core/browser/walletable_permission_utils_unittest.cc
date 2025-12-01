// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/wallet/core/browser/walletable_permission_utils.h"

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/wallet/core/browser/data_models/country_type.h"
#include "components/wallet/core/common/wallet_features.h"
#include "components/wallet/core/common/wallet_prefs.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace wallet {

// Test fixture for WalletablePermissionUtils. Sets up the necessary pref
// service and identity environment to test account-keyed preferences.
class WalletablePermissionUtilsTest : public testing::Test {
 public:
  WalletablePermissionUtilsTest() {
    wallet::prefs::RegisterProfilePrefs(prefs_.registry());
  }

  signin::IdentityManager* identity_manager() {
    return identity_test_env_.identity_manager();
  }

  PrefService* prefs() { return &prefs_; }

  signin::IdentityTestEnvironment& identity_test_env() {
    return identity_test_env_;
  }

 private:
  base::test::TaskEnvironment task_environment_;
  sync_preferences::TestingPrefServiceSyncable prefs_;
  signin::IdentityTestEnvironment identity_test_env_;
};

TEST_F(WalletablePermissionUtilsTest, OptInStatus_SignedOut) {
  EXPECT_FALSE(
      GetWalletablePassDetectionOptInStatus(prefs(), identity_manager()));
}

TEST_F(WalletablePermissionUtilsTest, OptInStatus_NullIdentityManager) {
  EXPECT_FALSE(GetWalletablePassDetectionOptInStatus(prefs(), nullptr));

  // Should not crash.
  SetWalletablePassDetectionOptInStatus(prefs(), nullptr, true);
  EXPECT_FALSE(GetWalletablePassDetectionOptInStatus(prefs(), nullptr));
}

TEST_F(WalletablePermissionUtilsTest, OptInStatus_SignedInDefault) {
  identity_test_env().MakePrimaryAccountAvailable(
      "test@gmail.com", signin::ConsentLevel::kSignin);
  EXPECT_FALSE(
      GetWalletablePassDetectionOptInStatus(prefs(), identity_manager()));
}

TEST_F(WalletablePermissionUtilsTest, OptInStatus_SetAndGet) {
  identity_test_env().MakePrimaryAccountAvailable(
      "test@gmail.com", signin::ConsentLevel::kSignin);

  SetWalletablePassDetectionOptInStatus(prefs(), identity_manager(), true);
  EXPECT_TRUE(
      GetWalletablePassDetectionOptInStatus(prefs(), identity_manager()));

  SetWalletablePassDetectionOptInStatus(prefs(), identity_manager(), false);
  EXPECT_FALSE(
      GetWalletablePassDetectionOptInStatus(prefs(), identity_manager()));
}

#if !BUILDFLAG(IS_CHROMEOS)  // Signing out does not work on ChromeOS.
TEST_F(WalletablePermissionUtilsTest, OptInStatus_TiedToAccount) {
  AccountInfo account1 = identity_test_env().MakePrimaryAccountAvailable(
      "test1@gmail.com", signin::ConsentLevel::kSignin);
  SetWalletablePassDetectionOptInStatus(prefs(), identity_manager(), true);
  EXPECT_TRUE(
      GetWalletablePassDetectionOptInStatus(prefs(), identity_manager()));

  // Sign out.
  identity_test_env().ClearPrimaryAccount();
  EXPECT_FALSE(
      GetWalletablePassDetectionOptInStatus(prefs(), identity_manager()));

  // Sign in with a different account.
  identity_test_env().MakePrimaryAccountAvailable(
      "test2@gmail.com", signin::ConsentLevel::kSignin);
  EXPECT_FALSE(
      GetWalletablePassDetectionOptInStatus(prefs(), identity_manager()));

  // Switch back to the first account.
  identity_test_env().ClearPrimaryAccount();
  identity_test_env().SetPrimaryAccount(account1.email,
                                        signin::ConsentLevel::kSignin);
  EXPECT_TRUE(
      GetWalletablePassDetectionOptInStatus(prefs(), identity_manager()));
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

// Test fixture for WalletablePermissionUtils eligibility, initializing the
// `kWalletablePassDetection` feature with a country allowlist.
class WalletablePermissionUtilsForEligibilityTest
    : public WalletablePermissionUtilsTest {
 public:
  WalletablePermissionUtilsForEligibilityTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {{kWalletablePassDetection,
          {{"walletable_supported_country_allowlist", "US,BR"}}}},
        {});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(WalletablePermissionUtilsForEligibilityTest,
       IsEligibleForWalletablePassDetection_NotSignedIn) {
  EXPECT_FALSE(IsEligibleForWalletablePassDetection(identity_manager(),
                                                    GeoIpCountryCode("US")));
}

TEST_F(WalletablePermissionUtilsForEligibilityTest,
       IsEligibleForWalletablePassDetection_NullIdentityManager) {
  EXPECT_FALSE(
      IsEligibleForWalletablePassDetection(nullptr, GeoIpCountryCode("US")));
}

TEST_F(WalletablePermissionUtilsForEligibilityTest,
       IsEligibleForWalletablePassDetection_EmptyCountry) {
  identity_test_env().MakePrimaryAccountAvailable(
      "test @gmail.com", signin::ConsentLevel::kSignin);

  EXPECT_TRUE(IsEligibleForWalletablePassDetection(identity_manager(),
                                                   GeoIpCountryCode("")));
}

TEST_F(WalletablePermissionUtilsForEligibilityTest,
       IsEligibleForWalletablePassDetection_CountryInAllowlist) {
  identity_test_env().MakePrimaryAccountAvailable(
      "test @gmail.com", signin::ConsentLevel::kSignin);

  EXPECT_TRUE(IsEligibleForWalletablePassDetection(identity_manager(),
                                                   GeoIpCountryCode("US")));
}

TEST_F(WalletablePermissionUtilsForEligibilityTest,
       IsEligibleForWalletablePassDetection_CountryNotInAllowlist) {
  identity_test_env().MakePrimaryAccountAvailable(
      "test @gmail.com", signin::ConsentLevel::kSignin);

  EXPECT_FALSE(IsEligibleForWalletablePassDetection(identity_manager(),
                                                    GeoIpCountryCode("CA")));
}

TEST_F(WalletablePermissionUtilsForEligibilityTest,
       IsEligibleForWalletablePassDetection_WalletPassDetectionDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kWalletablePassDetection);
  identity_test_env().MakePrimaryAccountAvailable(
      "test @gmail.com", signin::ConsentLevel::kSignin);

  EXPECT_FALSE(IsEligibleForWalletablePassDetection(identity_manager(),
                                                    GeoIpCountryCode("US")));
}

}  // namespace wallet
