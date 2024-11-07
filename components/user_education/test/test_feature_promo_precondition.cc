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
  FeaturePromoResult::Failure failure;
  std::string description;
  bool default_value = true;
  std::map<const base::Feature*, bool> overrides;
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
  FeaturePromoResult::Failure GetFailure() const override {
    return data_->failure;
  }
  const std::string& GetDescription() const override {
    return data_->description;
  }

  bool IsAllowed() const override {
    const bool* value = base::FindOrNull(data_->overrides, &iph_feature_.get());
    return value ? *value : data_->default_value;
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
    FeaturePromoResult::Failure failure,
    std::string description,
    bool initially_allowed) {
  auto data = std::make_unique<PreconditionData>();
  data->identifier = identifier;
  data->failure = failure;
  data->description = description;
  data->default_value = initially_allowed;
  data_.emplace_back(std::move(data));
}

void TestPreconditionListProvider::SetDefault(
    FeaturePromoPrecondition::Identifier id,
    bool is_allowed) {
  bool found = false;
  for (const auto& entry : data_) {
    if (entry->identifier == id) {
      entry->default_value = is_allowed;
      found = true;
      break;
    }
  }
  CHECK(found);
}

void TestPreconditionListProvider::SetForFeature(
    const base::Feature& iph_feature,
    FeaturePromoPrecondition::Identifier id,
    bool is_allowed) {
  bool found = false;
  for (auto& entry : data_) {
    if (entry->identifier == id) {
      entry->overrides[&iph_feature] = is_allowed;
      found = true;
      break;
    }
  }
  CHECK(found);
}

void TestPreconditionListProvider::SetFailure(
    FeaturePromoPrecondition::Identifier id,
    FeaturePromoResult::Failure failure) {
  bool found = false;
  for (const auto& entry : data_) {
    if (entry->identifier == id) {
      entry->failure = failure;
      found = true;
      break;
    }
  }
  CHECK(found);
}

FeaturePromoPreconditionList TestPreconditionListProvider::GetPreconditions(
    const FeaturePromoSpecification& spec) const {
  if (next_query_spec_.has_value()) {
    EXPECT_EQ(*next_query_spec_, &spec);
    next_query_spec_ = nullptr;
  }
  FeaturePromoPreconditionList result;
  for (const auto& entry : data_) {
    result.AddPrecondition(
        std::make_unique<TestPrecondition>(*spec.feature(), *entry.get()));
  }
  return result;
}

}  // namespace user_education::test
