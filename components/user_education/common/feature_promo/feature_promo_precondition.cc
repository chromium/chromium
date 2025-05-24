// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/feature_promo/feature_promo_precondition.h"

#include "base/check.h"
#include "base/functional/callback_forward.h"
#include "components/user_education/common/feature_promo/feature_promo_result.h"

namespace user_education {

FeaturePromoPrecondition::ComputedData::ComputedData() = default;
FeaturePromoPrecondition::ComputedData::ComputedData(ComputedData&&) noexcept =
    default;
FeaturePromoPrecondition::ComputedData&
FeaturePromoPrecondition::ComputedData::operator=(ComputedData&&) noexcept =
    default;
FeaturePromoPrecondition::ComputedData::~ComputedData() = default;

FeaturePromoPreconditionBase::FeaturePromoPreconditionBase(
    Identifier identifier,
    std::string description)
    : identifier_(identifier),
      description_(std::move(description)) {}

FeaturePromoPreconditionBase::~FeaturePromoPreconditionBase() = default;

FeaturePromoPreconditionBase::Identifier
FeaturePromoPreconditionBase::GetIdentifier() const {
  return identifier_;
}

const std::string& FeaturePromoPreconditionBase::GetDescription() const {
  return description_;
}

void FeaturePromoPreconditionBase::ExtractCachedData(
    internal::PreconditionData::Collection& to_add_to) {
  for (auto& [id, data] : data_) {
    const auto result = to_add_to.emplace(id, std::move(data));
    CHECK(result.second) << "Two different providers for precondition data: "
                         << id;
  }
  data_.clear();
}

CachingFeaturePromoPrecondition::CachingFeaturePromoPrecondition(
    Identifier identifier,
    std::string description,
    FeaturePromoResult initial_state)
    : FeaturePromoPreconditionBase(identifier, std::move(description)),
      check_result_(initial_state) {}

CachingFeaturePromoPrecondition::~CachingFeaturePromoPrecondition() = default;

FeaturePromoResult CachingFeaturePromoPrecondition::CheckPrecondition(
    ComputedData&) const {
  return check_result_;
}

CallbackFeaturePromoPrecondition::CallbackFeaturePromoPrecondition(
    Identifier identifier,
    std::string description,
    SimpleCallback check_result_callback)
    : FeaturePromoPreconditionBase(identifier, std::move(description)),
      check_result_callback_(
          base::BindRepeating([](const SimpleCallback& callback,
                                 ComputedData& data) { return callback.Run(); },
                              std::move(check_result_callback))) {
  CHECK(!check_result_callback_.is_null());
}

CallbackFeaturePromoPrecondition::CallbackFeaturePromoPrecondition(
    Identifier identifier,
    std::string description,
    CallbackWithData check_result_callback)
    : FeaturePromoPreconditionBase(identifier, std::move(description)),
      check_result_callback_(std::move(check_result_callback)) {
  CHECK(!check_result_callback_.is_null());
}

CallbackFeaturePromoPrecondition::~CallbackFeaturePromoPrecondition() = default;

FeaturePromoResult CallbackFeaturePromoPrecondition::CheckPrecondition(
    ComputedData& data) const {
  return check_result_callback_.Run(data);
}

ForwardingFeaturePromoPrecondition::ForwardingFeaturePromoPrecondition(
    const FeaturePromoPrecondition& source)
    : source_(source) {}

ForwardingFeaturePromoPrecondition::~ForwardingFeaturePromoPrecondition() =
    default;

ForwardingFeaturePromoPrecondition::Identifier
ForwardingFeaturePromoPrecondition::GetIdentifier() const {
  return source_->GetIdentifier();
}

const std::string& ForwardingFeaturePromoPrecondition::GetDescription() const {
  return source_->GetDescription();
}

FeaturePromoResult ForwardingFeaturePromoPrecondition::CheckPrecondition(
    ComputedData& data) const {
  return source_->CheckPrecondition(data);
}

FeaturePromoPreconditionList::FeaturePromoPreconditionList(
    FeaturePromoPreconditionList&&) noexcept = default;
FeaturePromoPreconditionList& FeaturePromoPreconditionList::operator=(
    FeaturePromoPreconditionList&&) noexcept = default;
FeaturePromoPreconditionList::~FeaturePromoPreconditionList() = default;

FeaturePromoPreconditionList::CheckResult
FeaturePromoPreconditionList::CheckPreconditions(
    ComputedData& computed_data) const {
  for (const auto& precondition : preconditions_) {
    const auto result = precondition->CheckPrecondition(computed_data);
    if (!result) {
      return CheckResult(result, precondition->GetIdentifier());
    }
  }
  return CheckResult(FeaturePromoResult::Success(), {});
}

void FeaturePromoPreconditionList::AppendAll(
    FeaturePromoPreconditionList other) {
  std::move(other.preconditions_.begin(), other.preconditions_.end(),
            std::back_inserter(preconditions_));
}

void FeaturePromoPreconditionList::AddPrecondition(
    std::unique_ptr<FeaturePromoPrecondition> precondition) {
  CHECK(precondition);
  // TODO(dfried): Maybe check for duplicate IDs in DCHECK mode?
  preconditions_.emplace_back(std::move(precondition));
}

void FeaturePromoPreconditionList::ExtractCachedData(
    internal::PreconditionData::Collection& to_add_to) {
  for (auto& precondition : preconditions_) {
    precondition->ExtractCachedData(to_add_to);
  }
}

}  // namespace user_education
