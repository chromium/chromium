// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/triggers/trigger_throttler.h"

#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "components/prefs/testing_pref_service.h"
#include "components/safe_browsing/common/safe_browsing_prefs.h"
#include "components/safe_browsing/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ElementsAre;

namespace safe_browsing {

class TriggerThrottlerTest : public ::testing::Test {
 public:
  TriggerThrottlerTest() : trigger_throttler_(nullptr) {}

  void SetUp() override {
    safe_browsing::RegisterLocalStatePrefs(pref_service_.registry());
    trigger_throttler_.ResetPrefsForTesting(&pref_service_);
  }

  void SetQuotaForTriggerType(TriggerType trigger_type, size_t max_quota) {
    SetQuotaForTriggerType(&trigger_throttler_, trigger_type, max_quota);
  }

  void SetQuotaForTriggerType(TriggerThrottler* throttler,
                              TriggerType trigger_type,
                              size_t max_quota) {
    throttler->trigger_type_and_quota_list_.push_back(
        std::make_pair(trigger_type, max_quota));
  }

  TriggerThrottler* throttler() { return &trigger_throttler_; }

  void SetTestClock(base::Clock* clock) {
    trigger_throttler_.SetClockForTesting(clock);
  }

  std::vector<base::Time> GetEventTimestampsForTriggerType(
      TriggerType trigger_type) {
    return trigger_throttler_.trigger_events_[trigger_type];
  }

  PrefService* get_pref_service() { return &pref_service_; }

 private:
  TestingPrefServiceSimple pref_service_;
  TriggerThrottler trigger_throttler_;
};

TEST_F(TriggerThrottlerTest, SecurityInterstitialsHaveUnlimitedQuota) {
  // Make sure that security interstitials never run out of quota.
  for (int i = 0; i < 1000; ++i) {
    throttler()->TriggerFired(TriggerType::SECURITY_INTERSTITIAL);
    EXPECT_TRUE(
        throttler()->TriggerCanFire(TriggerType::SECURITY_INTERSTITIAL));
  }
}

TEST_F(TriggerThrottlerTest, SecurityInterstitialQuotaCanNotBeOverwritten) {
  // Make sure that security interstitials never run out of quota, even if we
  // try to configure quota for this trigger type.
  SetQuotaForTriggerType(TriggerType::SECURITY_INTERSTITIAL, 3);
  for (int i = 0; i < 1000; ++i) {
    throttler()->TriggerFired(TriggerType::SECURITY_INTERSTITIAL);
    EXPECT_TRUE(
        throttler()->TriggerCanFire(TriggerType::SECURITY_INTERSTITIAL));
  }
}

TEST_F(TriggerThrottlerTest, TriggerQuotaSetToOne) {
  // This is a corner case where we can exceed array bounds for triggers that
  // have quota set to 1 report per day. This can happen when quota is 1 and
  // exactly one event has fired. When deciding whether another event can fire,
  // we look at the Nth-from-last event to check if it was recent or not - in
  // this scenario, Nth-from-last is 1st-from-last (because quota is 1). An
  // off-by-one error in this calculation can cause us to look at position 1
  // instead of position 0 in the even list.
  SetQuotaForTriggerType(TriggerType::AD_SAMPLE, 1);

  // Fire the trigger, first event will be allowed.
  EXPECT_TRUE(throttler()->TriggerCanFire(TriggerType::AD_SAMPLE));
  throttler()->TriggerFired(TriggerType::AD_SAMPLE);

  // Ensure that checking whether this trigger can fire again does not cause
  // an error and also returns the expected result.
  EXPECT_FALSE(throttler()->TriggerCanFire(TriggerType::AD_SAMPLE));
}

TEST_F(TriggerThrottlerTest, TriggerExceedsQuota) {
  // Ensure that a trigger can't fire more than its quota allows.
  SetQuotaForTriggerType(TriggerType::AD_SAMPLE, 2);

  // First two triggers should work
  EXPECT_TRUE(throttler()->TriggerCanFire(TriggerType::AD_SAMPLE));
  throttler()->TriggerFired(TriggerType::AD_SAMPLE);
  EXPECT_TRUE(throttler()->TriggerCanFire(TriggerType::AD_SAMPLE));
  throttler()->TriggerFired(TriggerType::AD_SAMPLE);

  // Third attempt will fail since we're out of quota.
  EXPECT_FALSE(throttler()->TriggerCanFire(TriggerType::AD_SAMPLE));
}

TEST_F(TriggerThrottlerTest, TriggerQuotaResetsAfterOneDay) {
  // Ensure that trigger events older than a day are cleaned up and triggers can
  // resume firing.

  // We initialize the test clock to several days ago and fire some events to
  // use up quota. We then advance the clock by a day and ensure quota is
  // available again.
  base::SimpleTestClock test_clock;
  test_clock.SetNow(base::Time::Now() - base::TimeDelta::FromDays(10));
  base::Time base_ts = test_clock.Now();

  SetTestClock(&test_clock);
  SetQuotaForTriggerType(TriggerType::AD_SAMPLE, 2);

  // First two triggers should work
  EXPECT_TRUE(throttler()->TriggerCanFire(TriggerType::AD_SAMPLE));
  throttler()->TriggerFired(TriggerType::AD_SAMPLE);
  EXPECT_TRUE(throttler()->TriggerCanFire(TriggerType::AD_SAMPLE));
  throttler()->TriggerFired(TriggerType::AD_SAMPLE);

  // Third attempt will fail since we're out of quota.
  EXPECT_FALSE(throttler()->TriggerCanFire(TriggerType::AD_SAMPLE));

  // Also confirm that the throttler contains two event timestamps for the above
  // two events - since we use a test clock, it doesn't move unless we tell it
  // to.
  EXPECT_THAT(GetEventTimestampsForTriggerType(TriggerType::AD_SAMPLE),
              ElementsAre(base_ts, base_ts));

  // Move the clock forward by 1 day (and a bit) and try the trigger again,
  // quota should be available now.
  test_clock.Advance(base::TimeDelta::FromDays(1) +
                     base::TimeDelta::FromSeconds(1));
  base::Time advanced_ts = test_clock.Now();
  EXPECT_TRUE(throttler()->TriggerCanFire(TriggerType::AD_SAMPLE));

  // The previous time stamps should remain in the throttler.
  EXPECT_THAT(GetEventTimestampsForTriggerType(TriggerType::AD_SAMPLE),
              ElementsAre(base_ts, base_ts));

  // Firing the trigger will clean up the expired timestamps and insert the new
  // timestamp.
  throttler()->TriggerFired(TriggerType::AD_SAMPLE);
  EXPECT_THAT(GetEventTimestampsForTriggerType(TriggerType::AD_SAMPLE),
              ElementsAre(advanced_ts));
}

TEST_F(TriggerThrottlerTest, TriggerQuotaPersistence) {
  // Test that trigger quota is persisted in prefs when triggers fire, and
  // retrieved from prefs on startup.

  // Set some low quotas for two triggers
  SetQuotaForTriggerType(TriggerType::AD_SAMPLE, 3);
  SetQuotaForTriggerType(TriggerType::SUSPICIOUS_SITE, 3);

  // Ensure each trigger can fire.
  EXPECT_TRUE(throttler()->TriggerCanFire(TriggerType::AD_SAMPLE));
  EXPECT_TRUE(throttler()->TriggerCanFire(TriggerType::SUSPICIOUS_SITE));

  // Fire each trigger twice to store some events.
  throttler()->TriggerFired(TriggerType::AD_SAMPLE);
  throttler()->TriggerFired(TriggerType::AD_SAMPLE);
  throttler()->TriggerFired(TriggerType::AD_SAMPLE);
  throttler()->TriggerFired(TriggerType::SUSPICIOUS_SITE);
  throttler()->TriggerFired(TriggerType::SUSPICIOUS_SITE);

  // The AD_SAMPLE trigger is now out of quota, while SUSPICIOUS_SITE can still
  // fire one more time.
  EXPECT_FALSE(throttler()->TriggerCanFire(TriggerType::AD_SAMPLE));
  EXPECT_TRUE(throttler()->TriggerCanFire(TriggerType::SUSPICIOUS_SITE));

  // Check the pref directly, it should reflect the events for each trigger.
  PrefService* prefs = get_pref_service();
  const base::DictionaryValue* event_dict =
      prefs->GetDictionary(prefs::kSafeBrowsingTriggerEventTimestamps);

  const std::string kAdSampleKey = "2";
  const base::Value* ad_sample_events = event_dict->FindKey(kAdSampleKey);
  EXPECT_EQ(3u, ad_sample_events->GetList().size());

  const std::string kSuspiciousSiteKey = "4";
  const base::Value* suspicious_site_events =
      event_dict->FindKey(kSuspiciousSiteKey);
  EXPECT_EQ(2u, suspicious_site_events->GetList().size());

  // To simulate a new startup of the browser, we can create another throttler
  // using the same quota configuration and pref store. It should read the
  // events from prefs and and reflect the same status for each trigger.
  TriggerThrottler throttler2(prefs);
  SetQuotaForTriggerType(&throttler2, TriggerType::AD_SAMPLE, 3);
  SetQuotaForTriggerType(&throttler2, TriggerType::SUSPICIOUS_SITE, 3);
  EXPECT_FALSE(throttler2.TriggerCanFire(TriggerType::AD_SAMPLE));
  EXPECT_TRUE(throttler2.TriggerCanFire(TriggerType::SUSPICIOUS_SITE));
}

class TriggerThrottlerTestFinch : public ::testing::Test {
 public:
  void SetupQuotaParams(const TriggerType trigger_type,
                        const std::string& group_name,
                        int quota,
                        base::test::ScopedFeatureList* scoped_feature_list) {
    const base::Feature* feature = nullptr;
    std::string param_name = "";
    GetFeatureAndParamForTrigger(trigger_type, &feature, &param_name);

    base::FieldTrialParams feature_params;
    feature_params[param_name] =
        GetQuotaParamValueForTrigger(trigger_type, quota);
    scoped_feature_list->InitAndEnableFeatureWithParameters(*feature,
                                                            feature_params);
  }

  size_t GetDailyQuotaForTrigger(const TriggerThrottler& throttler,
                                 const TriggerType trigger_type) {
    return throttler.GetDailyQuotaForTrigger(trigger_type);
  }

 private:
  void GetFeatureAndParamForTrigger(const TriggerType trigger_type,
                                    const base::Feature** out_feature,
                                    std::string* out_param) {
    switch (trigger_type) {
      case TriggerType::AD_SAMPLE:
        *out_feature = &safe_browsing::kTriggerThrottlerDailyQuotaFeature;
        *out_param = safe_browsing::kTriggerTypeAndQuotaParam;
        break;

      case TriggerType::SUSPICIOUS_SITE:
        *out_feature = &safe_browsing::kSuspiciousSiteTriggerQuotaFeature;
        *out_param = safe_browsing::kSuspiciousSiteTriggerQuotaParam;
        break;

      default:
        NOTREACHED() << "Unhandled trigger type: "
                     << static_cast<int>(trigger_type);
    }
  }

  std::string GetQuotaParamValueForTrigger(const TriggerType trigger_type,
                                           int quota) {
    if (trigger_type == TriggerType::AD_SAMPLE)
      return base::StringPrintf("%d,%d", trigger_type, quota);
    else
      return base::StringPrintf("%d", quota);
  }
};

TEST_F(TriggerThrottlerTestFinch, ConfigureQuotaViaFinch) {
  base::test::ScopedFeatureList scoped_feature_list;
  SetupQuotaParams(TriggerType::AD_SAMPLE, "Group_ConfigureQuotaViaFinch", 3,
                   &scoped_feature_list);
  // Make sure that setting the quota param via Finch params works as expected.

  // The throttler has been configured (above) to allow ad samples to fire three
  // times per day.
  TriggerThrottler throttler(nullptr);

  // First three triggers should work
  EXPECT_TRUE(throttler.TriggerCanFire(TriggerType::AD_SAMPLE));
  throttler.TriggerFired(TriggerType::AD_SAMPLE);
  EXPECT_TRUE(throttler.TriggerCanFire(TriggerType::AD_SAMPLE));
  throttler.TriggerFired(TriggerType::AD_SAMPLE);
  EXPECT_TRUE(throttler.TriggerCanFire(TriggerType::AD_SAMPLE));
  throttler.TriggerFired(TriggerType::AD_SAMPLE);

  // Fourth attempt will fail since we're out of quota.
  EXPECT_FALSE(throttler.TriggerCanFire(TriggerType::AD_SAMPLE));
}

TEST_F(TriggerThrottlerTestFinch, AdSamplerDefaultQuota) {
  // Make sure that the ad sampler gets its own default quota when no finch
  // config exists, but the quota can be overwritten through Finch.
  TriggerThrottler throttler_default(nullptr);
  EXPECT_EQ(kAdSamplerTriggerDefaultQuota,
            GetDailyQuotaForTrigger(throttler_default, TriggerType::AD_SAMPLE));
  EXPECT_TRUE(throttler_default.TriggerCanFire(TriggerType::AD_SAMPLE));

  base::test::ScopedFeatureList scoped_feature_list;
  SetupQuotaParams(TriggerType::AD_SAMPLE, "Group_AdSamplerDefaultQuota", 4,
                   &scoped_feature_list);
  TriggerThrottler throttler_finch(nullptr);
  EXPECT_EQ(4u,
            GetDailyQuotaForTrigger(throttler_finch, TriggerType::AD_SAMPLE));
}

TEST_F(TriggerThrottlerTestFinch, SuspiciousSiteTriggerDefaultQuota) {
  // Ensure that suspicious site trigger is enabled with default quota.
  TriggerThrottler throttler_default(nullptr);
  EXPECT_EQ(
      kSuspiciousSiteTriggerDefaultQuota,
      GetDailyQuotaForTrigger(throttler_default, TriggerType::SUSPICIOUS_SITE));
  EXPECT_TRUE(throttler_default.TriggerCanFire(TriggerType::SUSPICIOUS_SITE));

  base::test::ScopedFeatureList scoped_feature_list;
  SetupQuotaParams(TriggerType::SUSPICIOUS_SITE,
                   "Group_SuspiciousSiteTriggerDefaultQuota", 7,
                   &scoped_feature_list);
  TriggerThrottler throttler_finch(nullptr);
  EXPECT_EQ(7u, GetDailyQuotaForTrigger(throttler_finch,
                                        TriggerType::SUSPICIOUS_SITE));
}

}  // namespace safe_browsing
