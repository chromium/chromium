// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/optimization_guide_permissions_util.h"

#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_pref_names.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
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
    pref_service_.registry()->RegisterBooleanPref(
        data_reduction_proxy::prefs::kDataSaverEnabled, false);
  }

  void SetDataSaverEnabled(bool enabled) {
    data_reduction_proxy::DataReductionProxySettings::
        SetDataSaverEnabledForTesting(&pref_service_, enabled);
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
       IsUserPermittedToFetchHintsNonDataSaverUser) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      {optimization_guide::features::kRemoteOptimizationGuideFetching});
  SetDataSaverEnabled(false);

  EXPECT_FALSE(IsUserPermittedToFetchFromRemoteOptimizationGuide(
      /*is_off_the_record=*/false, pref_service()));
}

TEST_F(OptimizationGuidePermissionsUtilTest,
       IsUserPermittedToFetchHintsDataSaverUser) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      {optimization_guide::features::kRemoteOptimizationGuideFetching});
  SetDataSaverEnabled(true);

  EXPECT_TRUE(IsUserPermittedToFetchFromRemoteOptimizationGuide(
      /*is_off_the_record=*/false, pref_service()));
}

TEST_F(
    OptimizationGuidePermissionsUtilTest,
    IsUserPermittedToFetchHintsNonDataSaverUserAnonymousDataCollectionEnabledFeatureEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {optimization_guide::features::kRemoteOptimizationGuideFetching,
       optimization_guide::features::
           kRemoteOptimizationGuideFetchingAnonymousDataConsent},
      {});
  SetDataSaverEnabled(false);
  SetUrlKeyedAnonymizedDataCollectionEnabled(true);

  EXPECT_TRUE(IsUserPermittedToFetchFromRemoteOptimizationGuide(
      /*is_off_the_record=*/false, pref_service()));
}

TEST_F(
    OptimizationGuidePermissionsUtilTest,
    IsUserPermittedToFetchHintsNonDataSaverUserAnonymousDataCollectionDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {optimization_guide::features::kRemoteOptimizationGuideFetching,
       optimization_guide::features::
           kRemoteOptimizationGuideFetchingAnonymousDataConsent},
      {});
  SetDataSaverEnabled(false);
  SetUrlKeyedAnonymizedDataCollectionEnabled(false);

  EXPECT_FALSE(IsUserPermittedToFetchFromRemoteOptimizationGuide(
      /*is_off_the_record=*/false, pref_service()));
}

TEST_F(
    OptimizationGuidePermissionsUtilTest,
    IsUserPermittedToFetchHintsNonDataSaverUserAnonymousDataCollectionEnabledFeatureNotEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {optimization_guide::features::kRemoteOptimizationGuideFetching},
      {optimization_guide::features::
           kRemoteOptimizationGuideFetchingAnonymousDataConsent});
  SetDataSaverEnabled(false);
  SetUrlKeyedAnonymizedDataCollectionEnabled(true);

  EXPECT_FALSE(IsUserPermittedToFetchFromRemoteOptimizationGuide(
      /*is_off_the_record=*/false, pref_service()));
}

TEST_F(OptimizationGuidePermissionsUtilTest,
       IsUserPermittedToFetchHintsAllConsentsEnabledButHintsFetchingDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {}, {optimization_guide::features::kRemoteOptimizationGuideFetching});
  SetDataSaverEnabled(true);
  SetUrlKeyedAnonymizedDataCollectionEnabled(true);

  EXPECT_FALSE(IsUserPermittedToFetchFromRemoteOptimizationGuide(
      /*is_off_the_record=*/false, pref_service()));
}

TEST_F(OptimizationGuidePermissionsUtilTest,
       IsUserPermittedToFetchHintsPerformanceInfoFlagExplicitlyAllows) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {optimization_guide::features::kRemoteOptimizationGuideFetching,
       optimization_guide::features::
           kContextMenuPerformanceInfoAndRemoteHintFetching},
      {});
  SetDataSaverEnabled(false);
  SetUrlKeyedAnonymizedDataCollectionEnabled(false);

  EXPECT_TRUE(IsUserPermittedToFetchFromRemoteOptimizationGuide(
      /*is_off_the_record=*/false, pref_service()));
}

TEST_F(OptimizationGuidePermissionsUtilTest,
       IsUserPermittedToFetchHintsAllConsentsEnabledIncognitoProfile) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {optimization_guide::features::kRemoteOptimizationGuideFetching,
       optimization_guide::features::
           kRemoteOptimizationGuideFetchingAnonymousDataConsent,
       optimization_guide::features::
           kContextMenuPerformanceInfoAndRemoteHintFetching},
      {});
  SetDataSaverEnabled(true);
  SetUrlKeyedAnonymizedDataCollectionEnabled(true);

  EXPECT_FALSE(IsUserPermittedToFetchFromRemoteOptimizationGuide(
      /*is_off_the_record=*/true, pref_service()));
}

}  // namespace optimization_guide
