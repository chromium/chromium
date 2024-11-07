// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/feature_promo/feature_promo_precondition.h"

#include "base/check.h"
#include "components/user_education/common/feature_promo/feature_promo_result.h"

namespace user_education {

FeaturePromoPreconditionBase::FeaturePromoPreconditionBase(
    Identifier identifier,
    FeaturePromoResult::Failure failure,
    std::string description)
    : identifier_(identifier),
      failure_(failure),
      description_(std::move(description)) {}

FeaturePromoPreconditionBase::~FeaturePromoPreconditionBase() = default;

FeaturePromoPreconditionBase::Identifier
FeaturePromoPreconditionBase::GetIdentifier() const {
  return identifier_;
}

FeaturePromoResult::Failure FeaturePromoPreconditionBase::GetFailure() const {
  return failure_;
}

const std::string& FeaturePromoPreconditionBase::GetDescription() const {
  return description_;
}

CachingFeaturePromoPrecondition::CachingFeaturePromoPrecondition(
    Identifier identifier,
    FeaturePromoResult::Failure failure,
    std::string description,
    bool initial_state)
    : FeaturePromoPreconditionBase(identifier, failure, std::move(description)),
      is_allowed_(initial_state) {}

CachingFeaturePromoPrecondition::~CachingFeaturePromoPrecondition() = default;

bool CachingFeaturePromoPrecondition::IsAllowed() const {
  return is_allowed_;
}

CallbackFeaturePromoPrecondition::CallbackFeaturePromoPrecondition(
    Identifier identifier,
    FeaturePromoResult::Failure failure,
    std::string description,
    base::RepeatingCallback<bool()> is_allowed_callback)
    : FeaturePromoPreconditionBase(identifier, failure, std::move(description)),
      is_allowed_callback_(std::move(is_allowed_callback)) {
  CHECK(!is_allowed_callback_.is_null());
}

CallbackFeaturePromoPrecondition::~CallbackFeaturePromoPrecondition() = default;

bool CallbackFeaturePromoPrecondition::IsAllowed() const {
  return is_allowed_callback_.Run();
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

FeaturePromoResult::Failure ForwardingFeaturePromoPrecondition::GetFailure()
    const {
  return source_->GetFailure();
}

const std::string& ForwardingFeaturePromoPrecondition::GetDescription() const {
  return source_->GetDescription();
}

bool ForwardingFeaturePromoPrecondition::IsAllowed() const {
  return source_->IsAllowed();
}

FeaturePromoPreconditionList::FeaturePromoPreconditionList(
    FeaturePromoPreconditionList&&) noexcept = default;
FeaturePromoPreconditionList& FeaturePromoPreconditionList::operator=(
    FeaturePromoPreconditionList&&) noexcept = default;
FeaturePromoPreconditionList::~FeaturePromoPreconditionList() = default;

FeaturePromoPreconditionList::CheckResult
FeaturePromoPreconditionList::CheckPreconditions() const {
  for (const auto& precondition : preconditions_) {
    if (!precondition->IsAllowed()) {
      return CheckResult(precondition->GetFailure(),
                         precondition->GetIdentifier());
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

}  // namespace user_education
