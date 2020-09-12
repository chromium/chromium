// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/realtime/policy_engine.h"

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/common/safebrowsing_constants.h"
#include "components/safe_browsing/core/common/test_task_environment.h"
#include "components/safe_browsing/core/features.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/driver/test_sync_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/unified_consent/pref_names.h"
#include "components/unified_consent/unified_consent_service.h"
#include "components/user_prefs/user_prefs.h"
#include "testing/platform_test.h"

#if defined(OS_ANDROID)
#include "base/strings/string_number_conversions.h"
#include "base/system/sys_info.h"
#endif

namespace safe_browsing {

class RealTimePolicyEngineTest : public PlatformTest {
 public:
  RealTimePolicyEngineTest() : task_environment_(CreateTestTaskEnvironment()) {}

  void SetUp() override {
    RegisterProfilePrefs(pref_service_.registry());
    unified_consent::UnifiedConsentService::RegisterPrefs(
        pref_service_.registry());
  }

  bool IsUserMbbOptedIn() {
    return RealTimePolicyEngine::IsUserMbbOptedIn(&pref_service_);
  }

  bool CanPerformFullURLLookup(bool is_off_the_record) {
    return RealTimePolicyEngine::CanPerformFullURLLookup(
        &pref_service_, is_off_the_record, /*variations_service=*/nullptr);
  }

  bool CanPerformFullURLLookupWithToken(
      bool is_off_the_record,
      syncer::SyncService* sync_service,
      signin::IdentityManager* identity_manager) {
    return RealTimePolicyEngine::CanPerformFullURLLookupWithToken(
        &pref_service_, is_off_the_record, sync_service, identity_manager,
        /*variations_service=*/nullptr);
  }

  bool CanPerformEnterpriseFullURLLookup(bool has_valid_dm_token,
                                         bool is_off_the_record) {
    return RealTimePolicyEngine::CanPerformEnterpriseFullURLLookup(
        &pref_service_, has_valid_dm_token, is_off_the_record);
  }

  bool IsInExcludedCountry(const std::string& country_code) {
    return RealTimePolicyEngine::IsInExcludedCountry(country_code);
  }

  std::unique_ptr<base::test::TaskEnvironment> task_environment_;
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
  EXPECT_TRUE(CanPerformFullURLLookup(/* is_off_the_record */ false));
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
      /* disabled_features */ {kRealTimeUrlLookupEnabledForAllAndroidDevices});
  pref_service_.SetUserPref(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled,
      std::make_unique<base::Value>(true));
  EXPECT_FALSE(CanPerformFullURLLookup(/* is_off_the_record */ false));
}

TEST_F(RealTimePolicyEngineTest,
       TestCanPerformFullURLLookup_SmallMemorySizeWithAllDevicesFlagEnabled) {
  base::test::ScopedFeatureList feature_list;
  int system_memory_size = base::SysInfo::AmountOfPhysicalMemoryMB();
  int memory_size_threshold = system_memory_size + 1;
  feature_list.InitWithFeaturesAndParameters(
      /* enabled_features */ {{kRealTimeUrlLookupEnabled,
                               {{kRealTimeUrlLookupMemoryThresholdMb,
                                 base::NumberToString(memory_size_threshold)}}},
                              {kRealTimeUrlLookupEnabledForAllAndroidDevices,
                               {}}},
      /* disabled_features */ {});
  pref_service_.SetUserPref(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled,
      std::make_unique<base::Value>(true));
  EXPECT_TRUE(CanPerformFullURLLookup(/* is_off_the_record */ false));
}

TEST_F(RealTimePolicyEngineTest,
       TestCanPerformFullURLLookup_DisabledUrlLookupWithLargeMemorySize) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      /* enabled_features */ {},
      /* disabled_features */ {kRealTimeUrlLookupEnabled});
  EXPECT_FALSE(CanPerformFullURLLookup(/* is_off_the_record */ false));
}

TEST_F(RealTimePolicyEngineTest,
       TestCanPerformFullURLLookup_DisabledUrlLookupWithAllDevicesFlagEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      /* enabled_features */ {{kRealTimeUrlLookupEnabledForAllAndroidDevices,
                               {}}},
      /* disabled_features */ {kRealTimeUrlLookupEnabled});
  pref_service_.SetUserPref(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled,
      std::make_unique<base::Value>(true));
  // |kRealTimeUrlLookupEnabledForAllAndroidDevices| is in effect only if
  // |kRealTimeUrlLookupEnabled| is set to true.
  EXPECT_FALSE(CanPerformFullURLLookup(/* is_off_the_record */ false));
}
#endif  // defined(OS_ANDROID)

TEST_F(RealTimePolicyEngineTest,
       TestCanPerformFullURLLookup_DisabledUrlLookup) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kRealTimeUrlLookupEnabled);
  EXPECT_FALSE(CanPerformFullURLLookup(/* is_off_the_record */ false));
}

TEST_F(RealTimePolicyEngineTest,
       TestCanPerformFullURLLookup_DisabledOffTheRecord) {
  base::test::ScopedFeatureList feature_list;
  pref_service_.SetBoolean(prefs::kSafeBrowsingEnhanced, true);
  feature_list.InitAndEnableFeature(kEnhancedProtection);
  EXPECT_FALSE(CanPerformFullURLLookup(/* is_off_the_record */ true));
}

TEST_F(RealTimePolicyEngineTest,
       TestCanPerformFullURLLookup_DisabledUserOptin) {
  ASSERT_FALSE(IsUserMbbOptedIn());
}

TEST_F(RealTimePolicyEngineTest, TestCanPerformFullURLLookup_EnabledUserOptin) {
  pref_service_.SetUserPref(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled,
      std::make_unique<base::Value>(true));
  ASSERT_TRUE(IsUserMbbOptedIn());
}

TEST_F(RealTimePolicyEngineTest,
       TestCanPerformFullURLLookup_EnhancedProtection) {
  pref_service_.SetBoolean(prefs::kSafeBrowsingEnhanced, true);
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(kEnhancedProtection);
    ASSERT_FALSE(CanPerformFullURLLookup(/* is_off_the_record */ false));
  }
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(kEnhancedProtection);
    ASSERT_TRUE(CanPerformFullURLLookup(/* is_off_the_record */ false));
  }
}

TEST_F(RealTimePolicyEngineTest,
       TestCanPerformFullURLLookup_RTLookupForEpDisabled) {
  base::test::ScopedFeatureList feature_list;
  pref_service_.SetBoolean(prefs::kSafeBrowsingEnhanced, true);
  feature_list.InitWithFeatures(
      /* enabled_features */ {kEnhancedProtection},
      /* disabled_features */ {kRealTimeUrlLookupEnabledForEP});
  EXPECT_FALSE(CanPerformFullURLLookup(/* is_off_the_record */ false));
}

TEST_F(RealTimePolicyEngineTest,
       TestCanPerformFullURLLookup_RTLookupForEpEnabled_WithTokenDisabled) {
  std::unique_ptr<signin::IdentityTestEnvironment> identity_test_env =
      std::make_unique<signin::IdentityTestEnvironment>();
  signin::IdentityManager* identity_manager =
      identity_test_env->identity_manager();
  syncer::TestSyncService sync_service;
  // User is signed in.
  identity_test_env->MakeUnconsentedPrimaryAccountAvailable("test@example.com");

  pref_service_.SetBoolean(prefs::kSafeBrowsingEnhanced, true);
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures(
        /* enabled_features */ {kEnhancedProtection,
                                kRealTimeUrlLookupEnabledForEP},
        /* disabled_features */ {});
    EXPECT_TRUE(CanPerformFullURLLookup(/* is_off_the_record */ false));
    EXPECT_TRUE(CanPerformFullURLLookupWithToken(
        /* is_off_the_record */ false, &sync_service, identity_manager));
  }
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures(
        /* enabled_features */ {kEnhancedProtection,
                                kRealTimeUrlLookupEnabledForEP},
        /* disabled_features */ {kRealTimeUrlLookupEnabledForEPWithToken,
                                 kRealTimeUrlLookupEnabledWithToken});
    EXPECT_TRUE(CanPerformFullURLLookup(/* is_off_the_record */ false));
    EXPECT_FALSE(CanPerformFullURLLookupWithToken(
        /* is_off_the_record */ false, &sync_service, identity_manager));
  }
}

TEST_F(RealTimePolicyEngineTest,
       TestCanPerformFullURLLookup_NonEpUsersEnabledWhenRTLookupForEpDisabled) {
  base::test::ScopedFeatureList feature_list;
#if defined(OS_ANDROID)
  int system_memory_size = base::SysInfo::AmountOfPhysicalMemoryMB();
  int memory_size_threshold = system_memory_size - 1;
  feature_list.InitWithFeaturesAndParameters(
      /* enabled_features */ {{kRealTimeUrlLookupEnabled,
                               {{kRealTimeUrlLookupMemoryThresholdMb,
                                 base::NumberToString(memory_size_threshold)}}},
                              {kRealTimeUrlLookupEnabledWithToken, {}}},
      /* disabled_features */ {kRealTimeUrlLookupEnabledForEP});
#else
  feature_list.InitWithFeatures(
      /* enabled_features */ {kRealTimeUrlLookupEnabled,
                              kRealTimeUrlLookupEnabledWithToken},
      /* disabled_features */ {kRealTimeUrlLookupEnabledForEP});
#endif
  pref_service_.SetUserPref(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled,
      std::make_unique<base::Value>(true));

  // kRealTimeUrlLookupEnabledForEP should only control EP users.
  EXPECT_TRUE(CanPerformFullURLLookup(/* is_off_the_record */ false));
}

TEST_F(RealTimePolicyEngineTest,
       TestCanPerformFullURLLookupWithToken_SyncControlled) {
  base::test::ScopedFeatureList feature_list;
#if defined(OS_ANDROID)
  int system_memory_size = base::SysInfo::AmountOfPhysicalMemoryMB();
  int memory_size_threshold = system_memory_size - 1;
  feature_list.InitWithFeaturesAndParameters(
      /* enabled_features */ {{kRealTimeUrlLookupEnabled,
                               {{kRealTimeUrlLookupMemoryThresholdMb,
                                 base::NumberToString(memory_size_threshold)}}},
                              {kRealTimeUrlLookupEnabledWithToken, {}}},
      /* disabled_features */ {});
#else
  feature_list.InitWithFeatures(
      /* enabled_features */ {kRealTimeUrlLookupEnabled,
                              kRealTimeUrlLookupEnabledWithToken},
      /* disabled_features */ {});
#endif
  pref_service_.SetUserPref(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled,
      std::make_unique<base::Value>(true));
  std::unique_ptr<signin::IdentityTestEnvironment> identity_test_env =
      std::make_unique<signin::IdentityTestEnvironment>();
  signin::IdentityManager* identity_manager =
      identity_test_env->identity_manager();
  syncer::TestSyncService sync_service;

  // Sync is disabled.
  sync_service.SetDisableReasons(
      {syncer::SyncService::DISABLE_REASON_USER_CHOICE});
  sync_service.SetTransportState(syncer::SyncService::TransportState::DISABLED);
  EXPECT_FALSE(CanPerformFullURLLookupWithToken(
      /* is_off_the_record */ false, &sync_service, identity_manager));

  // Sync is enabled.
  sync_service.SetDisableReasons({});
  sync_service.SetTransportState(syncer::SyncService::TransportState::ACTIVE);
  EXPECT_TRUE(CanPerformFullURLLookupWithToken(
      /* is_off_the_record */ false, &sync_service, identity_manager));

  // History sync is disabled.
  sync_service.GetUserSettings()->SetSelectedTypes(
      /* sync_everything */ false, {});
  EXPECT_FALSE(CanPerformFullURLLookupWithToken(
      /* is_off_the_record */ false, &sync_service, identity_manager));

  // Custom passphrase is enabled.
  sync_service.GetUserSettings()->SetSelectedTypes(
      false, {syncer::UserSelectableType::kHistory});
  sync_service.SetIsUsingSecondaryPassphrase(true);
  EXPECT_FALSE(CanPerformFullURLLookupWithToken(
      /* is_off_the_record */ false, &sync_service, identity_manager));
}

TEST_F(RealTimePolicyEngineTest,
       TestCanPerformFullURLLookupWithToken_EnhancedProtection) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /* enabled_features */ {kEnhancedProtection},
      /* disabled_features */ {kRealTimeUrlLookupEnabledWithToken});
  std::unique_ptr<signin::IdentityTestEnvironment> identity_test_env =
      std::make_unique<signin::IdentityTestEnvironment>();
  signin::IdentityManager* identity_manager =
      identity_test_env->identity_manager();
  syncer::TestSyncService sync_service;

  // Enhanced protection is on but user is not signed in.
  pref_service_.SetBoolean(prefs::kSafeBrowsingEnhanced, true);
  EXPECT_FALSE(CanPerformFullURLLookupWithToken(
      /* is_off_the_record */ false, &sync_service, identity_manager));

  // User is signed in.
  identity_test_env->MakeUnconsentedPrimaryAccountAvailable("test@example.com");
  EXPECT_TRUE(CanPerformFullURLLookupWithToken(
      /* is_off_the_record */ false, &sync_service, identity_manager));

  // Sync and history sync is disabled but enhanced protection is enabled.
  sync_service.SetDisableReasons(
      {syncer::SyncService::DISABLE_REASON_USER_CHOICE});
  sync_service.SetTransportState(syncer::SyncService::TransportState::DISABLED);
  sync_service.GetUserSettings()->SetSelectedTypes(
      /* sync_everything */ false, {});
  EXPECT_TRUE(CanPerformFullURLLookupWithToken(
      /*is_off_the_record=*/false, &sync_service, identity_manager));
}

TEST_F(RealTimePolicyEngineTest, TestCanPerformEnterpriseFullURLLookup) {
  // Is off the record profile.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(kRealTimeUrlLookupEnabledForEnterprise);
    EXPECT_FALSE(CanPerformEnterpriseFullURLLookup(/*has_valid_dm_token=*/true,
                                                   /*is_off_the_record=*/true));
  }
  // Feature flag disabled.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(kRealTimeUrlLookupEnabledForEnterprise);
    EXPECT_FALSE(CanPerformEnterpriseFullURLLookup(
        /*has_valid_dm_token=*/true, /*is_off_the_record=*/false));
  }
  // No valid DM token.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(kRealTimeUrlLookupEnabledForEnterprise);
    EXPECT_FALSE(CanPerformEnterpriseFullURLLookup(
        /*has_valid_dm_token=*/false, /*is_off_the_record=*/false));
  }
  // Policy disabled.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(kRealTimeUrlLookupEnabledForEnterprise);
    pref_service_.SetUserPref(
        prefs::kSafeBrowsingEnterpriseRealTimeUrlCheckMode,
        std::make_unique<base::Value>(REAL_TIME_CHECK_DISABLED));
    EXPECT_FALSE(CanPerformEnterpriseFullURLLookup(
        /*has_valid_dm_token=*/true, /*is_off_the_record=*/false));
  }
  // Policy enabled.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(kRealTimeUrlLookupEnabledForEnterprise);
    pref_service_.SetUserPref(
        prefs::kSafeBrowsingEnterpriseRealTimeUrlCheckMode,
        std::make_unique<base::Value>(REAL_TIME_CHECK_FOR_MAINFRAME_ENABLED));
    EXPECT_TRUE(CanPerformEnterpriseFullURLLookup(
        /*has_valid_dm_token=*/true, /*is_off_the_record=*/false));
  }
}

TEST_F(
    RealTimePolicyEngineTest,
    TestCanPerformFullURLLookup_EnabledMainFrameOnlyForSubresourceDisabledUser) {
  for (int i = 0; i <= static_cast<int>(ResourceType::kMaxValue); i++) {
    ResourceType resource_type = static_cast<ResourceType>(i);
    bool enabled = RealTimePolicyEngine::CanPerformFullURLLookupForResourceType(
        resource_type, /*can_rt_check_subresource_url=*/false);
    switch (resource_type) {
      case ResourceType::kMainFrame:
        EXPECT_TRUE(enabled);
        break;
      default:
        EXPECT_FALSE(enabled);
        break;
    }
  }
}

TEST_F(
    RealTimePolicyEngineTest,
    TestCanPerformFullURLLookup_EnabledNonMainFrameForSubresourceEnabledUser) {
  for (int i = 0; i <= static_cast<int>(ResourceType::kMaxValue); i++) {
    ResourceType resource_type = static_cast<ResourceType>(i);
    bool enabled = RealTimePolicyEngine::CanPerformFullURLLookupForResourceType(
        resource_type, /*can_rt_check_subresource_url=*/true);
    switch (resource_type) {
      case ResourceType::kMainFrame:
      case ResourceType::kSubFrame:
        EXPECT_TRUE(enabled);
        break;
      default:
        EXPECT_FALSE(enabled);
        break;
    }
  }
}

TEST_F(RealTimePolicyEngineTest, TestIsInExcludedCountry) {
  const std::string non_excluded_countries[] = {"be", "br", "ca", "de", "es",
                                                "fr", "ie", "in", "jp", "nl",
                                                "ru", "se", "us"};
  for (auto country : non_excluded_countries) {
    EXPECT_FALSE(IsInExcludedCountry(country));
  }

  const std::string excluded_countries[] = {"cn"};
  for (auto country : excluded_countries) {
    EXPECT_TRUE(IsInExcludedCountry(country));
  }
}

}  // namespace safe_browsing
