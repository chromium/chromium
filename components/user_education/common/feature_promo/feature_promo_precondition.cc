// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/feature_promo/feature_promo_precondition.h"

#include "base/check.h"
#include "components/user_education/common/feature_promo/feature_promo_result.h"
#include "ui/base/interaction/typed_data_collection.h"

namespace user_education {

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
    ui::OwnedTypedDataCollection& to_add_to) {
  to_add_to.Append(std::move(data_));
}

CachingFeaturePromoPrecondition::CachingFeaturePromoPrecondition(
    Identifier identifier,
    std::string description,
    FeaturePromoResult initial_state)
    : FeaturePromoPreconditionBase(identifier, std::move(description)),
      check_result_(initial_state) {}

CachingFeaturePromoPrecondition::~CachingFeaturePromoPrecondition() = default;

FeaturePromoResult CachingFeaturePromoPrecondition::CheckPrecondition(
    ui::UnownedTypedDataCollection&) const {
  return check_result_;
}

CallbackFeaturePromoPrecondition::CallbackFeaturePromoPrecondition(
    Identifier identifier,
    std::string description,
    SimpleCallback check_result_callback)
    : FeaturePromoPreconditionBase(identifier, std::move(description)),
      check_result_callback_(base::BindRepeating(
          [](const SimpleCallback& callback,
             ui::UnownedTypedDataCollection& data) { return callback.Run(); },
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
    ui::UnownedTypedDataCollection& data) const {
  return check_result_callback_.Run(data);
}

ForwardingFeaturePromoPrecondition::ForwardingFeaturePromoPrecondition(
    const FeaturePromoPrecondition& source)
    : source_(&source),
      cached_identifier_(source.GetIdentifier()),
      cached_description_(source.GetDescription()) {}

ForwardingFeaturePromoPrecondition::~ForwardingFeaturePromoPrecondition() =
    default;

ForwardingFeaturePromoPrecondition::Identifier
ForwardingFeaturePromoPrecondition::GetIdentifier() const {
  return source_ ? source_->GetIdentifier() : cached_identifier_;
}

const std::string& ForwardingFeaturePromoPrecondition::GetDescription() const {
  return source_ ? source_->GetDescription() : cached_description_;
}

FeaturePromoResult ForwardingFeaturePromoPrecondition::CheckPrecondition(
    ui::UnownedTypedDataCollection& data) const {
  return source_ ? source_->CheckPrecondition(data)
                 : FeaturePromoResult::kError;
}

void ForwardingFeaturePromoPrecondition::Invalidate() {
  source_ = nullptr;
}

FeaturePromoPreconditionList::FeaturePromoPreconditionList(
    FeaturePromoPreconditionList&&) noexcept = default;
FeaturePromoPreconditionList& FeaturePromoPreconditionList::operator=(
    FeaturePromoPreconditionList&&) noexcept = default;
FeaturePromoPreconditionList::~FeaturePromoPreconditionList() = default;

FeaturePromoPreconditionList::CheckResult
FeaturePromoPreconditionList::CheckPreconditions(
    ui::UnownedTypedDataCollection& computed_data) const {
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
    ui::OwnedTypedDataCollection& to_add_to) {
  for (auto& precondition : preconditions_) {
    precondition->ExtractCachedData(to_add_to);
  }
}

}  // namespace user_education
