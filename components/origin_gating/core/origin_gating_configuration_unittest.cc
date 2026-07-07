// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/origin_gating/core/origin_gating_configuration.h"

#include <utility>

#include "base/functional/callback.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace origin_gating {
namespace {

TEST(OriginGatingConfigurationTest, StoresPredicatesInOrder) {
  CustomPredicate custom1(
      base::BindRepeating([](const GatingDecisionContext*, const GURL&,
                             const GURL&,
                             base::OnceCallback<void(Decision)> callback) {
        std::move(callback).Run(Decision::kNoDecision);
      }),
      "custom_1");

  CustomPredicate custom2(
      base::BindRepeating([](const GatingDecisionContext*, const GURL&,
                             const GURL&,
                             base::OnceCallback<void(Decision)> callback) {
        std::move(callback).Run(Decision::kAllowed);
      }),
      "custom_2");

  OriginGatingConfiguration config(
      {
          DecisionSource::kAllowSameOrigin,
          custom1,
          custom2,
      },
      /*use_site_keyed_cache=*/false);

  EXPECT_THAT(config.predicates(),
              testing::ElementsAre(
                  testing::VariantWith<DecisionSource>(
                      DecisionSource::kAllowSameOrigin),
                  testing::VariantWith<CustomPredicate>(
                      testing::Property(&CustomPredicate::name, "custom_1")),
                  testing::VariantWith<CustomPredicate>(
                      testing::Property(&CustomPredicate::name, "custom_2"))));
}

TEST(OriginGatingConfigurationTest, CheckFails_NoVerdict) {
  EXPECT_DEATH_IF_SUPPORTED(
      {
        OriginGatingConfiguration config({DecisionSource::kNoVerdict},
                                         /*use_site_keyed_cache=*/false);
      },
      "");
}

}  // namespace
}  // namespace origin_gating
