// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_TEST_TEST_FEATURE_PROMO_PRECONDITION_H_
#define COMPONENTS_USER_EDUCATION_TEST_TEST_FEATURE_PROMO_PRECONDITION_H_

#include <optional>
#include <string>
#include <vector>

#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "components/user_education/common/feature_promo/feature_promo_controller.h"
#include "components/user_education/common/feature_promo/feature_promo_precondition.h"
#include "components/user_education/common/feature_promo/feature_promo_result.h"
#include "components/user_education/common/feature_promo/feature_promo_specification.h"
#include "components/user_education/common/feature_promo/impl/precondition_list_provider.h"
#include "components/user_education/common/user_education_context.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace user_education::test {

// Precondition list provider that forwards to a pre-generated list of caching
// preconditions (whose values can be individually set by id).
class TestPreconditionListProvider : public PreconditionListProvider {
 public:
  TestPreconditionListProvider();
  ~TestPreconditionListProvider() override;

  // Sets the expected promo specification for the next query. If the next call
  // does not match, or there are multiple calls, an error will be generated.
  //
  // To accept any value, call `ClearExpectedPromoForFutureQueries()`.
  void SetExpectedPromoForNextQuery(const FeaturePromoSpecification& spec,
                                    const UserEducationContextPtr& context);

  // Sets the expected promo specification to "don't care" for all future calls
  // to `GetPreconditions()` unless `SetExpectedPromoForNextQuery()` is called
  // again. This is the default.
  void ClearExpectedPromoForFutureQueries();

  // Adds a precondition with the specified parameters and default allowed
  // state.
  void Add(FeaturePromoPrecondition::Identifier identifier,
           std::string description,
           FeaturePromoResult initial_result);

  // Sets the default `result` for a particular `id` which has already been
  // added.
  void SetDefault(FeaturePromoPrecondition::Identifier id,
                  FeaturePromoResult result);

  // Sets the specific `result` for a particular `id` for `iph_feature` only.
  // This overrides the default value.
  void SetForFeature(const base::Feature& iph_feature,
                     FeaturePromoPrecondition::Identifier id,
                     FeaturePromoResult result);

  // PreconditionListProvider:
  FeaturePromoPreconditionList GetPreconditions(
      const FeaturePromoSpecification& spec,
      const FeaturePromoParams& params,
      const UserEducationContextPtr& context) const override;

 private:
  // Cache of preconditions that simulate values.
  struct PreconditionData;
  class TestPrecondition;
  std::vector<std::unique_ptr<PreconditionData>> data_;

  // Mutable so that it can be cleared out during calls to `GetPreconditions()`.
  mutable std::optional<raw_ptr<const FeaturePromoSpecification>>
      next_query_spec_;
  mutable UserEducationContextPtr next_query_context_;
};

class MockPreconditionListProvider : public PreconditionListProvider {
 public:
  MockPreconditionListProvider();
  ~MockPreconditionListProvider() override;

  MOCK_METHOD(FeaturePromoPreconditionList,
              GetPreconditions,
              (const FeaturePromoSpecification&,
               const FeaturePromoParams&,
               const UserEducationContextPtr&),
              (const, override));
};

}  // namespace user_education::test

#endif  // COMPONENTS_USER_EDUCATION_TEST_TEST_FEATURE_PROMO_PRECONDITION_H_
