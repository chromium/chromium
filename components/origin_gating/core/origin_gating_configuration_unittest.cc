// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/origin_gating/core/origin_gating_configuration.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "components/origin_gating/core/types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::ElementsAre;
using testing::Property;
using testing::VariantWith;

namespace origin_gating {
namespace {

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
      base::BindRepeating([](GatingDecisionContext*, const GURL&, const GURL&) {
        return Decision::kNoDecision;
      }),
      TestCustomPredicate::kCustom1);

  CustomPredicate custom2(
      base::BindRepeating([](GatingDecisionContext*, const GURL&, const GURL&,
                             base::OnceCallback<void(Decision)> callback) {
        std::move(callback).Run(Decision::kAllowed);
      }),
      TestCustomPredicate::kCustom2);

  OriginGatingConfiguration config(
      {
          {DecisionSource::kAllowSameOrigin, GateableEventSet::All()},
          {custom1, GateableEventSet::All()},
          {custom2, GateableEventSet::All()},
      },
      /*use_site_keyed_cache=*/false);

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
            /*use_site_keyed_cache=*/false);
      },
      "");
}

TEST(OriginGatingConfigurationTest, CheckFails_MultipleCustomPredicateDomains) {
  CustomPredicate custom1(
      base::BindRepeating([](GatingDecisionContext*, const GURL&, const GURL&) {
        return Decision::kNoDecision;
      }),
      TestCustomPredicate::kCustom1);

  CustomPredicate custom2(
      base::BindRepeating([](GatingDecisionContext*, const GURL&, const GURL&) {
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
            /*use_site_keyed_cache=*/false);
      },
      "");
}

TEST(PredicateConfigurationTest, AppliesToOnlyConfiguredEvents) {
  PredicateConfiguration config(
      DecisionSource::kAllowSameOrigin,
      {GateableEvent::kNavigationRequest, GateableEvent::kPageAction});

  EXPECT_TRUE(config.AppliesTo(GateableEvent::kNavigationRequest));
  EXPECT_FALSE(config.AppliesTo(GateableEvent::kNavigationResponse));
  EXPECT_TRUE(config.AppliesTo(GateableEvent::kPageAction));
}

TEST(PredicateConfigurationTest, AppliesToAllEvents) {
  PredicateConfiguration config(DecisionSource::kAllowSameOrigin,
                                GateableEventSet::All());

  EXPECT_TRUE(config.AppliesTo(GateableEvent::kNavigationRequest));
  EXPECT_TRUE(config.AppliesTo(GateableEvent::kNavigationResponse));
  EXPECT_TRUE(config.AppliesTo(GateableEvent::kPageAction));
}

TEST(PredicateConfigurationTest, AppliesToNoEvents) {
  PredicateConfiguration config(DecisionSource::kAllowSameOrigin,
                                GateableEventSet());

  EXPECT_FALSE(config.AppliesTo(GateableEvent::kNavigationRequest));
  EXPECT_FALSE(config.AppliesTo(GateableEvent::kNavigationResponse));
  EXPECT_FALSE(config.AppliesTo(GateableEvent::kPageAction));
}

}  // namespace
}  // namespace origin_gating
