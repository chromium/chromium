// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/feature_promo/impl/precondition_list_provider.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_education/common/feature_promo/feature_promo_precondition.h"
#include "components/user_education/common/feature_promo/feature_promo_result.h"
#include "components/user_education/common/feature_promo/feature_promo_specification.h"
#include "components/user_education/test/test_feature_promo_precondition.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"

namespace user_education {

namespace {

BASE_FEATURE(kTestFeature1, "kTestFeature1", base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kTestFeature2, "kTestFeature2", base::FEATURE_ENABLED_BY_DEFAULT);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kAnchorId);
DEFINE_LOCAL_FEATURE_PROMO_PRECONDITION_IDENTIFIER_VALUE(kPrecond1);
DEFINE_LOCAL_FEATURE_PROMO_PRECONDITION_IDENTIFIER_VALUE(kPrecond2);
DEFINE_LOCAL_FEATURE_PROMO_PRECONDITION_IDENTIFIER_VALUE(kPrecond3);
DEFINE_LOCAL_FEATURE_PROMO_PRECONDITION_IDENTIFIER_VALUE(kPrecond4);
constexpr FeaturePromoResult::Failure kFailure1 =
    FeaturePromoResult::kBlockedByPromo;
constexpr FeaturePromoResult::Failure kFailure2 =
    FeaturePromoResult::kBlockedByConfig;
constexpr FeaturePromoResult::Failure kFailure3 =
    FeaturePromoResult::kBlockedByCooldown;
constexpr FeaturePromoResult::Failure kFailure4 =
    FeaturePromoResult::kBlockedByGracePeriod;
constexpr char kPrecond1Name[] = "Precond1";
constexpr char kPrecond2Name[] = "Precond2";
constexpr char kPrecond3Name[] = "Precond3";
constexpr char kPrecond4Name[] = "Precond4";

}  // namespace

TEST(PreconditionListProviderTest,
     ComposingPreconditionListProviderWithSingleInnerProvider) {
  // Dummy promo specification (details aren't important).
  const auto spec = FeaturePromoSpecification::CreateForTesting(
      kTestFeature1, kAnchorId, IDS_OK);

  // Create an inner provider with two preconditions, one of which fails.
  test::TestPreconditionListProvider inner;
  inner.Add(kPrecond1, kFailure1, kPrecond1Name, true);
  inner.Add(kPrecond2, kFailure2, kPrecond2Name, false);

  // Wrap the inner provider in a composing provider.
  ComposingPreconditionListProvider provider;
  provider.AddProvider(&inner);

  // Check that the composing provider calls the inner provider and returns the
  // correct result.
  inner.SetExpectedPromoForNextQuery(spec);
  auto result = provider.GetPreconditions(spec).CheckPreconditions();
  EXPECT_EQ(kFailure2, result.result());
  EXPECT_EQ(kPrecond2, result.failed_precondition());

  // Modify the inner provider so all its preconditions pass and try again.
  inner.SetDefault(kPrecond2, true);
  inner.SetExpectedPromoForNextQuery(spec);
  result = provider.GetPreconditions(spec).CheckPreconditions();
  EXPECT_EQ(FeaturePromoResult::Success(), result.result());
}

TEST(PreconditionListProviderTest,
     ComposingPreconditionListProviderWithMultipleInnerProviders) {
  // Dummy promo specification (details aren't important).
  const auto spec = FeaturePromoSpecification::CreateForTesting(
      kTestFeature1, kAnchorId, IDS_OK);

  // Create an inner provider with two preconditions, one of which fails.
  test::TestPreconditionListProvider inner;
  inner.Add(kPrecond1, kFailure1, kPrecond1Name, true);
  inner.Add(kPrecond2, kFailure2, kPrecond2Name, false);

  // Create a second inner provider, also with a failing condition.
  test::TestPreconditionListProvider inner2;
  inner2.Add(kPrecond3, kFailure3, kPrecond3Name, false);
  inner2.Add(kPrecond4, kFailure4, kPrecond4Name, true);

  // Wrap the inner provider in a composing provider.
  ComposingPreconditionListProvider provider;
  provider.AddProvider(&inner);
  provider.AddProvider(&inner2);

  // Check that the composing provider calls the inner providers and returns the
  // correct results.
  inner.SetExpectedPromoForNextQuery(spec);
  inner2.SetExpectedPromoForNextQuery(spec);
  auto result = provider.GetPreconditions(spec).CheckPreconditions();
  EXPECT_EQ(kFailure2, result.failure());
  EXPECT_EQ(kPrecond2, result.failed_precondition());

  // Modify the first inner provider so all its preconditions pass and try
  // again.
  inner.SetDefault(kPrecond2, true);
  inner.SetExpectedPromoForNextQuery(spec);
  inner2.SetExpectedPromoForNextQuery(spec);
  result = provider.GetPreconditions(spec).CheckPreconditions();
  EXPECT_EQ(kFailure3, result.failure());
  EXPECT_EQ(kPrecond3, result.failed_precondition());

  // Modify the second inner provider so that all preconditions pass.
  inner2.SetDefault(kPrecond3, true);
  inner.SetExpectedPromoForNextQuery(spec);
  inner2.SetExpectedPromoForNextQuery(spec);
  result = provider.GetPreconditions(spec).CheckPreconditions();
  EXPECT_EQ(FeaturePromoResult::Success(), result.result());
}

TEST(PreconditionListProviderTest,
     ComposingPreconditionListProviderPassesCorrectSpecificationToInner) {
  // Dummy promo specifications (details aren't important).
  const auto spec = FeaturePromoSpecification::CreateForTesting(
      kTestFeature1, kAnchorId, IDS_OK);
  const auto spec2 = FeaturePromoSpecification::CreateForTesting(
      kTestFeature2, kAnchorId, IDS_OK);

  // Create an inner provider and a composing provider that delegates to it.
  test::TestPreconditionListProvider inner;
  ComposingPreconditionListProvider provider;
  provider.AddProvider(&inner);

  // Ensure that the provider passes the correct specifications.
  // Do not need to look at the result, as we've already tested this.
  inner.SetExpectedPromoForNextQuery(spec);
  provider.GetPreconditions(spec);
  inner.SetExpectedPromoForNextQuery(spec2);
  provider.GetPreconditions(spec2);
}

}  // namespace user_education
