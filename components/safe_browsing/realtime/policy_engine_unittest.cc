// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/realtime/policy_engine.h"

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/safe_browsing/common/safe_browsing_prefs.h"
#include "components/safe_browsing/features.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/unified_consent/pref_names.h"
#include "components/unified_consent/unified_consent_service.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "testing/platform_test.h"

#if defined(OS_ANDROID)
#include "base/strings/string_number_conversions.h"
#include "base/system/sys_info.h"
#endif

namespace safe_browsing {

class RealTimePolicyEngineTest : public PlatformTest {
 public:
  void SetUp() override {
    user_prefs::UserPrefs::Set(&test_context_, &pref_service_);
    RegisterProfilePrefs(pref_service_.registry());
    unified_consent::UnifiedConsentService::RegisterPrefs(
        pref_service_.registry());
  }

  bool IsUserOptedIn() {
    return RealTimePolicyEngine::IsUserOptedIn(&test_context_);
  }

  bool CanPerformFullURLLookup() {
    return RealTimePolicyEngine::CanPerformFullURLLookup(&test_context_);
  }

  content::BrowserTaskEnvironment task_environment_;
  content::TestBrowserContext test_context_;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
};

#if defined(OS_ANDROID)
// Real time URL check on Android is controlled by system memory size, the
// following tests test that logic.
TEST_F(RealTimePolicyEngineTest, TestCanPerformFullURLLookup_LargeMemorySize) {
  base::test::ScopedFeatureList feature_list;
  int system_memory_size = base::SysInfo::AmountOfPhysicalMemoryMB();
  int memory_size_threshold = system_memory_size - 1;
  feature_list.InitWithFeaturesAndParameters(
      /* enabled_features */ {{kRealTimeUrlLookupEnabled,
                               {{kRealTimeUrlLookupMemoryThresholdMb,
                                 base::NumberToString(
                                     memory_size_threshold)}}}},
      /* disabled_features */ {});
  pref_service_.SetUserPref(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled,
      std::make_unique<base::Value>(true));
  EXPECT_TRUE(CanPerformFullURLLookup());
}

TEST_F(RealTimePolicyEngineTest, TestCanPerformFullURLLookup_SmallMemorySize) {
  base::test::ScopedFeatureList feature_list;
  int system_memory_size = base::SysInfo::AmountOfPhysicalMemoryMB();
  int memory_size_threshold = system_memory_size + 1;
  feature_list.InitWithFeaturesAndParameters(
      /* enabled_features */ {{kRealTimeUrlLookupEnabled,
                               {{kRealTimeUrlLookupMemoryThresholdMb,
                                 base::NumberToString(
                                     memory_size_threshold)}}}},
      /* disabled_features */ {});
  pref_service_.SetUserPref(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled,
      std::make_unique<base::Value>(true));
  EXPECT_FALSE(CanPerformFullURLLookup());
}

TEST_F(RealTimePolicyEngineTest,
       TestCanPerformFullURLLookup_DisabledUrlLookupWithLargeMemorySize) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      /* enabled_features */ {},
      /* disabled_features */ {kRealTimeUrlLookupEnabled});
  EXPECT_FALSE(CanPerformFullURLLookup());
}
#endif  // defined(OS_ANDROID)

TEST_F(RealTimePolicyEngineTest, TestCanPerformFullURLLookup_EnabledByPolicy) {
  base::test::ScopedFeatureList feature_list;
  pref_service_.SetManagedPref(prefs::kSafeBrowsingRealTimeLookupEnabled,
                               std::make_unique<base::Value>(true));
  EXPECT_TRUE(CanPerformFullURLLookup());
}

TEST_F(RealTimePolicyEngineTest,
       TestCanPerformFullURLLookup_DisabledUrlLookup) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kRealTimeUrlLookupEnabled);
  EXPECT_FALSE(CanPerformFullURLLookup());
}

TEST_F(RealTimePolicyEngineTest,
       TestCanPerformFullURLLookup_DisabledUserOptin) {
  ASSERT_FALSE(IsUserOptedIn());
}

TEST_F(RealTimePolicyEngineTest, TestCanPerformFullURLLookup_EnabledUserOptin) {
  pref_service_.SetUserPref(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled,
      std::make_unique<base::Value>(true));
  ASSERT_TRUE(IsUserOptedIn());
}

TEST_F(RealTimePolicyEngineTest,
       TestCanPerformFullURLLookup_EnabledMainFrameOnly) {
  for (int i = 0; i <= static_cast<int>(content::ResourceType::kMaxValue);
       i++) {
    content::ResourceType resource_type = static_cast<content::ResourceType>(i);
    bool enabled = RealTimePolicyEngine::CanPerformFullURLLookupForResourceType(
        resource_type);
    switch (resource_type) {
      case content::ResourceType::kMainFrame:
        EXPECT_TRUE(enabled);
        break;
      default:
        EXPECT_FALSE(enabled);
        break;
    }
  }
}

}  // namespace safe_browsing
