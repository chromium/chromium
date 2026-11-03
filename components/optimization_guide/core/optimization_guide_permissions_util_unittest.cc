// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/optimization_guide_permissions_util.h"

#include "base/command_line.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/unified_consent/pref_names.h"
#include "components/unified_consent/unified_consent_service.h"
#include "google_apis/api_key_cache.h"
#include "google_apis/default_api_keys.h"
#include "google_apis/google_api_keys.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
#include "google_apis/default_api_keys-inc.cc"
}  // namespace

namespace optimization_guide {

class OptimizationGuidePermissionsUtilTest : public testing::Test {
 public:
  void SetUp() override {
    unified_consent::UnifiedConsentService::RegisterPrefs(
        pref_service_.registry());

    scoped_command_line_.GetProcessCommandLine()->AppendSwitch(
        kGoogleApiKeyConfigurationCheckOverrideSwitch);
  }

  void SetUrlKeyedAnonymizedDataCollectionEnabled(bool enabled) {
    pref_service_.SetBoolean(
        unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled,
        enabled);
  }

  PrefService* pref_service() { return &pref_service_; }

 protected:
  base::test::ScopedCommandLine scoped_command_line_;

 private:
  base::test::TaskEnvironment task_environment_;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
};

TEST_F(OptimizationGuidePermissionsUtilTest,
       IsUserPermittedToFetchHintsDefaultUser) {
  EXPECT_FALSE(IsUserPermittedToFetchFromRemoteOptimizationGuide(
      /*is_off_the_record=*/false, pref_service()));
}

TEST_F(OptimizationGuidePermissionsUtilTest,
       IsUserPermittedToFetchHintsDefaultUserAnonymousDataCollectionEnabled) {
  SetUrlKeyedAnonymizedDataCollectionEnabled(true);

  EXPECT_TRUE(IsUserPermittedToFetchFromRemoteOptimizationGuide(
      /*is_off_the_record=*/false, pref_service()));
}

TEST_F(OptimizationGuidePermissionsUtilTest,
       IsUserPermittedToFetchHintsDefaultUserAnonymousDataCollectionDisabled) {
  SetUrlKeyedAnonymizedDataCollectionEnabled(false);

  EXPECT_FALSE(IsUserPermittedToFetchFromRemoteOptimizationGuide(
      /*is_off_the_record=*/false, pref_service()));
}

TEST_F(OptimizationGuidePermissionsUtilTest,
       IsUserPermittedToFetchHintsRequiresGoogleChromeApiKey) {
  scoped_command_line_.GetProcessCommandLine()->RemoveSwitch(
      kGoogleApiKeyConfigurationCheckOverrideSwitch);
  SetUrlKeyedAnonymizedDataCollectionEnabled(true);

  {
    google_apis::DefaultApiKeys default_keys =
        GetDefaultApiKeysFromDefinedValues();
    default_keys.allow_unset_values = true;
    default_keys.google_api_key = "non_official_api_key";
    default_keys.is_using_google_chrome_keys = false;
    google_apis::ApiKeyCache api_key_cache(default_keys);
    auto scoped_override =
        google_apis::SetScopedApiKeyCacheForTesting(&api_key_cache);

    EXPECT_TRUE(google_apis::HasAPIKeyConfigured());
    EXPECT_FALSE(google_apis::IsGoogleChromeAPIKeyUsed());
    EXPECT_FALSE(IsUserPermittedToFetchFromRemoteOptimizationGuide(
        /*is_off_the_record=*/false, pref_service()));
  }

  {
    google_apis::DefaultApiKeys default_keys =
        GetDefaultApiKeysFromDefinedValues();
    default_keys.allow_unset_values = true;
    default_keys.google_api_key = "official_api_key";
    default_keys.is_using_google_chrome_keys = true;
    google_apis::ApiKeyCache api_key_cache(default_keys);
    auto scoped_override =
        google_apis::SetScopedApiKeyCacheForTesting(&api_key_cache);

    EXPECT_TRUE(google_apis::IsGoogleChromeAPIKeyUsed());
    EXPECT_TRUE(IsUserPermittedToFetchFromRemoteOptimizationGuide(
        /*is_off_the_record=*/false, pref_service()));
  }
}

}  // namespace optimization_guide
