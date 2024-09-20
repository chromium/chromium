// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/optimization_guide_permissions_util.h"

#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/unified_consent/pref_names.h"
#include "components/unified_consent/unified_consent_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

class OptimizationGuidePermissionsUtilTest : public testing::Test {
 public:
  void SetUp() override {
    unified_consent::UnifiedConsentService::RegisterPrefs(
        pref_service_.registry());

    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kGoogleApiKeyConfigurationCheckOverride);
  }

  void SetUrlKeyedAnonymizedDataCollectionEnabled(bool enabled) {
    pref_service_.SetBoolean(
        unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled,
        enabled);
  }

  PrefService* pref_service() { return &pref_service_; }

 private:
  base::test::TaskEnvironment task_environment_;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
};

TEST_F(OptimizationGuidePermissionsUtilTest,
       IsUserPermittedToFetchHintsDefaultUser) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      {optimization_guide::features::kRemoteOptimizationGuideFetching});

  EXPECT_FALSE(IsUserPermittedToFetchFromRemoteOptimizationGuide(
      /*is_off_the_record=*/false, pref_service()));
}

TEST_F(
    OptimizationGuidePermissionsUtilTest,
    IsUserPermittedToFetchHintsDefaultUserAnonymousDataCollectionEnabledFeatureEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {optimization_guide::features::kRemoteOptimizationGuideFetching,
       optimization_guide::features::
           kRemoteOptimizationGuideFetchingAnonymousDataConsent},
      {});
  SetUrlKeyedAnonymizedDataCollectionEnabled(true);

  EXPECT_TRUE(IsUserPermittedToFetchFromRemoteOptimizationGuide(
      /*is_off_the_record=*/false, pref_service()));
}

TEST_F(OptimizationGuidePermissionsUtilTest,
       IsUserPermittedToFetchHintsDefaultUserAnonymousDataCollectionDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {optimization_guide::features::kRemoteOptimizationGuideFetching,
       optimization_guide::features::
           kRemoteOptimizationGuideFetchingAnonymousDataConsent},
      {});
  SetUrlKeyedAnonymizedDataCollectionEnabled(false);

  EXPECT_FALSE(IsUserPermittedToFetchFromRemoteOptimizationGuide(
      /*is_off_the_record=*/false, pref_service()));
}

TEST_F(
    OptimizationGuidePermissionsUtilTest,
    IsUserPermittedToFetchHintsDefaultUserAnonymousDataCollectionEnabledFeatureNotEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {optimization_guide::features::kRemoteOptimizationGuideFetching},
      {optimization_guide::features::
           kRemoteOptimizationGuideFetchingAnonymousDataConsent});
  SetUrlKeyedAnonymizedDataCollectionEnabled(true);

  EXPECT_FALSE(IsUserPermittedToFetchFromRemoteOptimizationGuide(
      /*is_off_the_record=*/false, pref_service()));
}

TEST_F(OptimizationGuidePermissionsUtilTest,
       IsUserPermittedToFetchHintsAllConsentsEnabledButHintsFetchingDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {}, {optimization_guide::features::kRemoteOptimizationGuideFetching});
  SetUrlKeyedAnonymizedDataCollectionEnabled(true);

  EXPECT_FALSE(IsUserPermittedToFetchFromRemoteOptimizationGuide(
      /*is_off_the_record=*/false, pref_service()));
}

}  // namespace optimization_guide
