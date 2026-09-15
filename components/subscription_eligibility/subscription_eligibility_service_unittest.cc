// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subscription_eligibility/subscription_eligibility_service.h"

#include <string>

#include "base/command_line.h"
#include "base/containers/flat_set.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/scoped_command_line.h"
#include "base/values.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/subscription_eligibility/subscription_eligibility_prefs.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace subscription_eligibility {

namespace {
class TestObserver : public SubscriptionEligibilityService::Observer {
 public:
  void OnAiSubscriptionTierUpdated(int32_t new_subscription_tier) override {
    last_updated_tier_ = new_subscription_tier;
    ++update_count_;
  }

  void OnSubscriptionBenefitsUpdated(
      const base::flat_set<std::string>& subscription_benefits) override {
    last_updated_benefits_ = subscription_benefits;
    ++benefits_update_count_;
  }

  int32_t last_updated_tier_ = -1;
  int update_count_ = 0;
  base::flat_set<std::string> last_updated_benefits_;
  int benefits_update_count_ = 0;
};
}  // namespace

class SubscriptionEligibilityServiceTest : public testing::Test {
 protected:
  void SetUp() override {
    prefs::RegisterProfilePrefs(pref_service_.registry());
  }

  sync_preferences::TestingPrefServiceSyncable pref_service_;
};

// Tests that by default (no command-line flag), the service returns the pref
// value.
TEST_F(SubscriptionEligibilityServiceTest, GetAiSubscriptionTier_Default) {
  pref_service_.SetInteger(prefs::kAiSubscriptionTier, 42);
  SubscriptionEligibilityService service(&pref_service_);
  EXPECT_EQ(service.GetAiSubscriptionTier(), 42);
}

// Tests that when the command-line flag is set, it overrides the pref value.
TEST_F(SubscriptionEligibilityServiceTest, GetAiSubscriptionTier_Forced) {
  pref_service_.SetInteger(prefs::kAiSubscriptionTier, 42);

  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
      kForceAiSubscriptionTier, "100");

  SubscriptionEligibilityService service(&pref_service_);
  EXPECT_EQ(service.GetAiSubscriptionTier(), 100);
}

// Tests that when the command-line flag is set to an invalid integer, it falls
// back to the pref value.
TEST_F(SubscriptionEligibilityServiceTest, GetAiSubscriptionTier_Invalid) {
  pref_service_.SetInteger(prefs::kAiSubscriptionTier, 42);

  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
      kForceAiSubscriptionTier, "invalid_value");

  SubscriptionEligibilityService service(&pref_service_);
  EXPECT_EQ(service.GetAiSubscriptionTier(), 42);
}

TEST_F(SubscriptionEligibilityServiceTest, ObserverNotifiedOnPrefChange) {
  SubscriptionEligibilityService service(&pref_service_);
  TestObserver observer;
  service.AddObserver(&observer);

  pref_service_.SetInteger(prefs::kAiSubscriptionTier, 2);
  EXPECT_EQ(observer.last_updated_tier_, 2);
  EXPECT_EQ(observer.update_count_, 1);

  service.RemoveObserver(&observer);
  pref_service_.SetInteger(prefs::kAiSubscriptionTier, 3);
  EXPECT_EQ(observer.update_count_, 1);
}

TEST_F(SubscriptionEligibilityServiceTest, GetSubscriptionBenefits_Default) {
  SubscriptionEligibilityService service(&pref_service_);
  EXPECT_TRUE(service.GetSubscriptionBenefits().empty());

  base::ListValue benefits;
  benefits.Append("benefit_1");
  benefits.Append("benefit_2");
  pref_service_.SetList(prefs::kSubscriptionBenefits, benefits.Clone());

  base::flat_set<std::string> expected_benefits = {"benefit_1", "benefit_2"};
  EXPECT_EQ(service.GetSubscriptionBenefits(), expected_benefits);
}

TEST_F(SubscriptionEligibilityServiceTest,
       ObserverNotifiedOnBenefitsPrefChange) {
  SubscriptionEligibilityService service(&pref_service_);
  TestObserver observer;
  service.AddObserver(&observer);

  base::ListValue benefits;
  benefits.Append("benefit_1");
  pref_service_.SetList(prefs::kSubscriptionBenefits, benefits.Clone());

  base::flat_set<std::string> expected_benefits = {"benefit_1"};
  EXPECT_EQ(observer.last_updated_benefits_, expected_benefits);
  EXPECT_EQ(observer.benefits_update_count_, 1);

  service.RemoveObserver(&observer);
  benefits.Append("benefit_2");
  pref_service_.SetList(prefs::kSubscriptionBenefits, benefits.Clone());
  EXPECT_EQ(observer.benefits_update_count_, 1);
}

}  // namespace subscription_eligibility
