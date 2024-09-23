// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/realtime/policy_engine.h"

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/common/safebrowsing_constants.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/unified_consent/pref_names.h"
#include "components/unified_consent/unified_consent_service.h"
#include "components/user_prefs/user_prefs.h"
#include "testing/platform_test.h"

#if BUILDFLAG(USE_BLINK)
#include "components/enterprise/connectors/core/common.h"
#include "components/enterprise/connectors/core/connectors_prefs.h"
#endif  // BUILDFLAG(USE_BLINK)

namespace safe_browsing {

// Used in tests of CanPerformFullURLLookupWithToken().
bool AreTokenFetchesEnabledInClient(bool expected_ep_enabled_value,
                                    bool return_value,
                                    bool user_has_enabled_enhanced_protection) {
  EXPECT_EQ(expected_ep_enabled_value, user_has_enabled_enhanced_protection);

  return return_value;
}

class RealTimePolicyEngineTest : public PlatformTest {
 public:
  RealTimePolicyEngineTest() = default;

  void SetUp() override {
    RegisterProfilePrefs(pref_service_.registry());
#if BUILDFLAG(USE_BLINK)
    enterprise_connectors::RegisterProfilePrefs(pref_service_.registry());
#endif  // BUILDFLAG(USE_BLINK)
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
      RealTimePolicyEngine::ClientConfiguredForTokenFetchesCallback
          client_callback) {
    return RealTimePolicyEngine::CanPerformFullURLLookupWithToken(
        &pref_service_, is_off_the_record, std::move(client_callback),
        /*variations_service=*/nullptr);
  }

  bool CanPerformEnterpriseFullURLLookup(bool has_valid_dm_token,
                                         bool is_off_the_record,
                                         bool is_guest_mode) {
    return RealTimePolicyEngine::CanPerformEnterpriseFullURLLookup(
        &pref_service_, has_valid_dm_token, is_off_the_record, is_guest_mode);
  }

  bool IsInExcludedCountry(const std::string& country_code) {
    return RealTimePolicyEngine::IsInExcludedCountry(country_code);
  }

  base::test::TaskEnvironment task_environment_;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
};

TEST_F(RealTimePolicyEngineTest,
       TestCanPerformFullURLLookup_DisabledOffTheRecord) {
  pref_service_.SetBoolean(prefs::kSafeBrowsingEnhanced, true);
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
  ASSERT_TRUE(CanPerformFullURLLookup(/* is_off_the_record */ false));
}

TEST_F(RealTimePolicyEngineTest,
       TestCanPerformFullURLLookup_DisabledEnhancedProtection) {
  pref_service_.SetBoolean(prefs::kSafeBrowsingEnhanced, false);
  ASSERT_FALSE(CanPerformFullURLLookup(/* is_off_the_record */ false));
}

TEST_F(RealTimePolicyEngineTest,
       TestCanPerformFullURLLookup_RTLookupForEpEnabled_WithTokenDisabled) {
  pref_service_.SetBoolean(prefs::kSafeBrowsingEnhanced, true);
  EXPECT_TRUE(CanPerformFullURLLookup(/* is_off_the_record */ false));
  EXPECT_TRUE(CanPerformFullURLLookupWithToken(
      /* is_off_the_record */ false,
      base::BindOnce(&AreTokenFetchesEnabledInClient,
                     /*expected_ep_enabled_value=*/true,
                     /*return_value=*/true)));
}

TEST_F(
    RealTimePolicyEngineTest,
    TestCanPerformFullURLLookupWithToken_ClientControlledWithoutEnhancedProtection) {
  pref_service_.SetUserPref(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled,
      std::make_unique<base::Value>(true));

  // Token fetches are not configured in the client.
  EXPECT_FALSE(CanPerformFullURLLookupWithToken(
      /* is_off_the_record */ false,
      base::BindOnce(&AreTokenFetchesEnabledInClient,
                     /*expected_ep_enabled_value=*/false,
                     /*return_value=*/false)));

  // Token fetches are configured in the client.
  EXPECT_TRUE(CanPerformFullURLLookupWithToken(
      /* is_off_the_record */ false,
      base::BindOnce(&AreTokenFetchesEnabledInClient,
                     /*expected_ep_enabled_value=*/false,
                     /*return_value=*/true)));
}

TEST_F(
    RealTimePolicyEngineTest,
    TestCanPerformFullURLLookupWithToken_ClientControlledWithEnhancedProtection) {
  // Enhanced protection is disabled: token fetches should be disallowed whether
  // or not they are configured in the client.
  EXPECT_FALSE(CanPerformFullURLLookupWithToken(
      /* is_off_the_record */ false,
      base::BindOnce(&AreTokenFetchesEnabledInClient,
                     /*expected_ep_enabled_value=*/false,
                     /*return_value=*/false)));
  EXPECT_FALSE(CanPerformFullURLLookupWithToken(
      /* is_off_the_record */ false,
      base::BindOnce(&AreTokenFetchesEnabledInClient,
                     /*expected_ep_enabled_value=*/false,
                     /*return_value=*/true)));

  // With enhanced protection enabled, whether token fetches are allowed should
  // be dependent on the configuration of the client
  pref_service_.SetBoolean(prefs::kSafeBrowsingEnhanced, true);
  EXPECT_FALSE(CanPerformFullURLLookupWithToken(
      /* is_off_the_record */ false,
      base::BindOnce(&AreTokenFetchesEnabledInClient,
                     /*expected_ep_enabled_value=*/true,
                     /*return_value=*/false)));
  EXPECT_TRUE(CanPerformFullURLLookupWithToken(
      /* is_off_the_record */ false,
      base::BindOnce(&AreTokenFetchesEnabledInClient,
                     /*expected_ep_enabled_value=*/true,
                     /*return_value=*/true)));
}

TEST_F(RealTimePolicyEngineTest, TestCanPerformEnterpriseFullURLLookup){
    // Is off the record non-guest profile.
    {EXPECT_FALSE(CanPerformEnterpriseFullURLLookup(/*has_valid_dm_token=*/true,
                                                    /*is_off_the_record=*/true,
                                                    /*is_guest_mode=*/false));
  }
  // No valid DM token.
  {
    EXPECT_FALSE(CanPerformEnterpriseFullURLLookup(
        /*has_valid_dm_token=*/false, /*is_off_the_record=*/false,
        /*is_guest_mode=*/false));
  }

#if BUILDFLAG(USE_BLINK)
  // Policy disabled.
  {
    pref_service_.SetUserPref(
        enterprise_connectors::kEnterpriseRealTimeUrlCheckMode,
        std::make_unique<base::Value>(
            enterprise_connectors::REAL_TIME_CHECK_DISABLED));
    EXPECT_FALSE(CanPerformEnterpriseFullURLLookup(
        /*has_valid_dm_token=*/true, /*is_off_the_record=*/false,
        /*is_guest_mode=*/false));
  }
  // Policy enabled.
  {
    pref_service_.SetUserPref(
        enterprise_connectors::kEnterpriseRealTimeUrlCheckMode,
        std::make_unique<base::Value>(
            enterprise_connectors::REAL_TIME_CHECK_FOR_MAINFRAME_ENABLED));
    EXPECT_TRUE(CanPerformEnterpriseFullURLLookup(
        /*has_valid_dm_token=*/true, /*is_off_the_record=*/false,
        /*is_guest_mode=*/false));
  }
  // Policy enabled in guest mode.
  {
    pref_service_.SetUserPref(
        enterprise_connectors::kEnterpriseRealTimeUrlCheckMode,
        std::make_unique<base::Value>(
            enterprise_connectors::REAL_TIME_CHECK_FOR_MAINFRAME_ENABLED));
    EXPECT_TRUE(CanPerformEnterpriseFullURLLookup(
        /*has_valid_dm_token=*/true, /*is_off_the_record=*/true,
        /*is_guest_mode=*/true));
  }
#endif  // BUILDFLAG(USE_BLINK)
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
