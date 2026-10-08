// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/origin_gating/core/origin_gating_configuration.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/test/gtest_util.h"
#include "components/origin_gating/core/decision.h"
#include "components/origin_gating/core/decision_attribution.h"
#include "components/origin_gating/core/decision_source.h"
#include "components/origin_gating/core/gateable_event.h"
#include "components/origin_gating/core/gating_decision.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::ElementsAre;
using testing::Property;
using testing::VariantWith;

namespace origin_gating {
namespace {

using CacheScope = OriginGatingConfiguration::CacheScope;

enum class TestCustomPredicate {
  kCustom1,
  kCustom2,
};

enum class AnotherCustomPredicate {
  kCustom1,
};

}  // namespace

template <>
const CustomPredicateDomain
    CustomPredicateDomain::kInstance<TestCustomPredicate>{};

template <>
const CustomPredicateDomain
    CustomPredicateDomain::kInstance<AnotherCustomPredicate>{};

namespace {

TEST(OriginGatingConfigurationTest, StoresPredicatesInOrder) {
  CustomPredicate custom1(
      base::BindRepeating([](GatingDecisionContext*, const GateableEvent&) {
        return Decision::kNoDecision;
      }),
      TestCustomPredicate::kCustom1);

  CustomPredicate custom2(
      base::BindRepeating([](GatingDecisionContext*, const GateableEvent&,
                             base::OnceCallback<void(Decision)> callback) {
        std::move(callback).Run(Decision::kAllowed);
      }),
      TestCustomPredicate::kCustom2);

  OriginGatingConfiguration config(
      {
          {DecisionSource::kAllowSameOrigin,
           {GateableEvent::kNavigationRequest,
            GateableEvent::kNavigationResponse}},
          {custom1, GateableEventSet::All()},
          {custom2, GateableEventSet::All()},
      },
      CacheScope::kOrigin);

  EXPECT_THAT(
      config.predicates(),
      ElementsAre(Property(&PredicateConfiguration::predicate,
                           VariantWith<DecisionSource>(
                               DecisionSource::kAllowSameOrigin)),
                  Property(&PredicateConfiguration::predicate,
                           VariantWith<CustomPredicate>(Property(
                               &CustomPredicate::attribution,
                               DecisionAttribution::CustomPredicateAttribution(
                                   TestCustomPredicate::kCustom1)))),
                  Property(&PredicateConfiguration::predicate,
                           VariantWith<CustomPredicate>(Property(
                               &CustomPredicate::attribution,
                               DecisionAttribution::CustomPredicateAttribution(
                                   TestCustomPredicate::kCustom2))))));
}

TEST(OriginGatingConfigurationTest, CheckFails_NoVerdict) {
  EXPECT_DEATH_IF_SUPPORTED(
      {
        OriginGatingConfiguration config(
            {{DecisionSource::kNoVerdict, GateableEventSet::All()}},
            CacheScope::kOrigin);
      },
      "");
}

TEST(OriginGatingConfigurationTest, CheckFails_AllowSameOriginOnPageAction) {
  EXPECT_CHECK_DEATH(OriginGatingConfiguration(
      {{DecisionSource::kAllowSameOrigin, GateableEventSet::All()}},
      CacheScope::kOrigin));
}

TEST(OriginGatingConfigurationTest, CheckFails_MultipleCustomPredicateDomains) {
  CustomPredicate custom1(
      base::BindRepeating([](GatingDecisionContext*, const GateableEvent&) {
        return Decision::kNoDecision;
      }),
      TestCustomPredicate::kCustom1);

  CustomPredicate custom2(
      base::BindRepeating([](GatingDecisionContext*, const GateableEvent&) {
        return Decision::kNoDecision;
      }),
      AnotherCustomPredicate::kCustom1);

  EXPECT_DEATH_IF_SUPPORTED(
      {
        OriginGatingConfiguration config(
            {
                {custom1, GateableEventSet::All()},
                {custom2, GateableEventSet::All()},
            },
            CacheScope::kOrigin);
      },
      "");
}

TEST(OriginGatingConfigurationTest, UsesCache) {
  OriginGatingConfiguration config_without_cache(
      {{DecisionSource::kAllowSameOrigin,
        {GateableEvent::kNavigationRequest,
         GateableEvent::kNavigationResponse}}},
      CacheScope::kOrigin);
  EXPECT_EQ(config_without_cache.cache_scope(), std::nullopt);

  OriginGatingConfiguration config_with_user_confirmation_cache(
      {{DecisionSource::kCacheWithUserConfirmation, GateableEventSet::All()}},
      CacheScope::kOrigin);
  EXPECT_EQ(config_with_user_confirmation_cache.cache_scope(),
            CacheScope::kOrigin);

  OriginGatingConfiguration config_with_unconfirmed_cache(
      {{DecisionSource::kCacheWithoutUserConfirmation,
        GateableEventSet::All()}},
      CacheScope::kSite);
  EXPECT_EQ(config_with_unconfirmed_cache.cache_scope(), CacheScope::kSite);
}

TEST(PredicateConfigurationTest, AppliesToOnlyConfiguredEvents) {
  PredicateConfiguration config(
      DecisionSource::kAllowHttpLocalhost,
      {GateableEvent::kNavigationRequest, GateableEvent::kPageAction});

  EXPECT_TRUE(config.AppliesTo(GateableEvent::kNavigationRequest));
  EXPECT_FALSE(config.AppliesTo(GateableEvent::kNavigationResponse));
  EXPECT_TRUE(config.AppliesTo(GateableEvent::kPageAction));
}

TEST(PredicateConfigurationTest, AppliesToAllEvents) {
  PredicateConfiguration config(DecisionSource::kAllowHttpLocalhost,
                                GateableEventSet::All());

  EXPECT_TRUE(config.AppliesTo(GateableEvent::kNavigationRequest));
  EXPECT_TRUE(config.AppliesTo(GateableEvent::kNavigationResponse));
  EXPECT_TRUE(config.AppliesTo(GateableEvent::kPageAction));
}

TEST(PredicateConfigurationTest, AppliesToNoEvents) {
  PredicateConfiguration config(DecisionSource::kAllowHttpLocalhost,
                                GateableEventSet());

  EXPECT_FALSE(config.AppliesTo(GateableEvent::kNavigationRequest));
  EXPECT_FALSE(config.AppliesTo(GateableEvent::kNavigationResponse));
  EXPECT_FALSE(config.AppliesTo(GateableEvent::kPageAction));
}

}  // namespace
}  // namespace origin_gating
