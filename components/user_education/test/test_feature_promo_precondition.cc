// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/test/test_feature_promo_precondition.h"

#include <map>
#include <string>

#include "base/containers/map_util.h"
#include "base/feature_list.h"
#include "components/user_education/common/feature_promo/feature_promo_precondition.h"
#include "components/user_education/common/feature_promo/feature_promo_result.h"
#include "components/user_education/common/feature_promo/feature_promo_specification.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace user_education::test {

struct TestPreconditionListProvider::PreconditionData {
  PreconditionData() = default;
  PreconditionData(const PreconditionData&) = delete;
  void operator=(const PreconditionData&) = delete;
  ~PreconditionData() = default;

  FeaturePromoPrecondition::Identifier identifier;
  std::string description;
  FeaturePromoResult default_result = FeaturePromoResult::Success();
  std::map<const base::Feature*, FeaturePromoResult> overrides;
};

class TestPreconditionListProvider::TestPrecondition
    : public FeaturePromoPrecondition {
 public:
  TestPrecondition(const base::Feature& iph_feature,
                   const PreconditionData& data)
      : iph_feature_(iph_feature), data_(data) {}
  ~TestPrecondition() override = default;

  // FeaturePromoPrecondition:
  Identifier GetIdentifier() const override { return data_->identifier; }
  const std::string& GetDescription() const override {
    return data_->description;
  }

  FeaturePromoResult CheckPrecondition(ComputedData&) const override {
    const auto* result =
        base::FindOrNull(data_->overrides, &iph_feature_.get());
    return result ? *result : data_->default_result;
  }

 private:
  const raw_ref<const base::Feature> iph_feature_;
  const raw_ref<const PreconditionData> data_;
};

TestPreconditionListProvider::TestPreconditionListProvider() = default;
TestPreconditionListProvider::~TestPreconditionListProvider() = default;

void TestPreconditionListProvider::SetExpectedPromoForNextQuery(
    const FeaturePromoSpecification& spec) {
  next_query_spec_ = &spec;
}

void TestPreconditionListProvider::ClearExpectedPromoForFutureQueries() {
  next_query_spec_.reset();
}

void TestPreconditionListProvider::Add(
    FeaturePromoPrecondition::Identifier identifier,
    std::string description,
    FeaturePromoResult default_result) {
  auto data = std::make_unique<PreconditionData>();
  data->identifier = identifier;
  data->description = description;
  data->default_result = default_result;
  data_.emplace_back(std::move(data));
}

void TestPreconditionListProvider::SetDefault(
    FeaturePromoPrecondition::Identifier id,
    FeaturePromoResult default_result) {
  bool found = false;
  for (const auto& entry : data_) {
    if (entry->identifier == id) {
      entry->default_result = default_result;
      found = true;
      break;
    }
  }
  CHECK(found);
}

void TestPreconditionListProvider::SetForFeature(
    const base::Feature& iph_feature,
    FeaturePromoPrecondition::Identifier id,
    FeaturePromoResult result) {
  bool found = false;
  for (auto& entry : data_) {
    if (entry->identifier == id) {
      entry->overrides[&iph_feature] = result;
      found = true;
      break;
    }
  }
  CHECK(found);
}

FeaturePromoPreconditionList TestPreconditionListProvider::GetPreconditions(
    const FeaturePromoSpecification& spec,
    const FeaturePromoParams& params) const {
  if (next_query_spec_.has_value()) {
    EXPECT_EQ(*next_query_spec_, &spec);
    next_query_spec_ = nullptr;
  }
  EXPECT_EQ(spec.feature(), &params.feature.get());
  FeaturePromoPreconditionList result;
  for (const auto& entry : data_) {
    result.AddPrecondition(
        std::make_unique<TestPrecondition>(*spec.feature(), *entry.get()));
  }
  return result;
}

MockPreconditionListProvider::MockPreconditionListProvider() = default;
MockPreconditionListProvider::~MockPreconditionListProvider() = default;

}  // namespace user_education::test
