// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/features/password_manager_features_util.h"

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/sync/base/features.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/test/test_sync_service.h"
#include "components/sync/test/test_sync_user_settings.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager::features_util {
namespace {

class PasswordManagerFeaturesUtilTest : public testing::Test {
 public:
  PasswordManagerFeaturesUtilTest() = default;

  PasswordManagerFeaturesUtilTest(const PasswordManagerFeaturesUtilTest&) =
      delete;
  PasswordManagerFeaturesUtilTest& operator=(
      const PasswordManagerFeaturesUtilTest&) = delete;

  ~PasswordManagerFeaturesUtilTest() override = default;

 protected:
  syncer::TestSyncService sync_service_;
};

TEST_F(PasswordManagerFeaturesUtilTest, IsAccountStorageActive_SignedOut) {
  sync_service_.SetSignedOut();

  EXPECT_FALSE(IsAccountStorageActive(&sync_service_));
}

TEST_F(PasswordManagerFeaturesUtilTest,
       IsAccountStorageActive_SignedInWithLocalSyncEnabled) {
  sync_service_.SetSignedIn(signin::ConsentLevel::kSignin);
  sync_service_.SetLocalSyncEnabled(true);

  EXPECT_FALSE(IsAccountStorageActive(&sync_service_));
}

TEST_F(PasswordManagerFeaturesUtilTest,
       IsAccountStorageActive_SignedInWithAuthError) {
  sync_service_.SetSignedIn(signin::ConsentLevel::kSignin);
  sync_service_.SetPersistentAuthError();

  EXPECT_FALSE(IsAccountStorageActive(&sync_service_));
}

TEST_F(PasswordManagerFeaturesUtilTest,
       IsAccountStorageActive_SignedInWithPassphraseError) {
  sync_service_.SetSignedIn(signin::ConsentLevel::kSignin);
  sync_service_.SetPassphraseRequired();

  EXPECT_FALSE(IsAccountStorageActive(&sync_service_));
}

TEST_F(PasswordManagerFeaturesUtilTest,
       IsAccountStorageActive_SignedInWithTrustedVaultError) {
  sync_service_.SetSignedIn(signin::ConsentLevel::kSignin);
  sync_service_.SetTrustedVaultKeyRequired(true);

  EXPECT_FALSE(IsAccountStorageActive(&sync_service_));
}

TEST_F(PasswordManagerFeaturesUtilTest,
       IsAccountStorageActive_SignedInWithPasswordsDataTypeDisabled) {
  sync_service_.SetSignedIn(signin::ConsentLevel::kSignin);
  sync_service_.GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kPasswords, false);

  EXPECT_FALSE(IsAccountStorageActive(&sync_service_));
}

TEST_F(PasswordManagerFeaturesUtilTest,
       IsAccountStorageActive_SignedInHealthy) {
  sync_service_.SetSignedIn(signin::ConsentLevel::kSignin);

  EXPECT_TRUE(IsAccountStorageActive(&sync_service_));
}

TEST_F(PasswordManagerFeaturesUtilTest, IsAccountStorageActive_Syncing) {
  sync_service_.SetSignedIn(signin::ConsentLevel::kSync);

  EXPECT_EQ(IsAccountStorageActive(&sync_service_), BUILDFLAG(IS_ANDROID));
}

#if !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)
TEST_F(PasswordManagerFeaturesUtilTest,
       ShouldShowAccountStorageSettingToggle_SyncToSigninOff) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      syncer::kReplaceSyncPromosWithSignInPromos);
  sync_service_.SetSignedIn(signin::ConsentLevel::kSignin);

  EXPECT_TRUE(ShouldShowAccountStorageSettingToggle(&sync_service_));
}

TEST_F(PasswordManagerFeaturesUtilTest,
       ShouldShowAccountStorageSettingToggle_SyncToSigninOn) {
  base::test::ScopedFeatureList feature_list(
      syncer::kReplaceSyncPromosWithSignInPromos);
  sync_service_.SetSignedIn(signin::ConsentLevel::kSignin);

  EXPECT_FALSE(ShouldShowAccountStorageSettingToggle(&sync_service_));
}
#endif  // !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)

}  // namespace
}  // namespace password_manager::features_util
