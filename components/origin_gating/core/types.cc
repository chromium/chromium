// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/origin_gating/core/types.h"

#include <utility>

#include "base/check.h"

namespace origin_gating {

std::string GateableEventToString(GateableEvent event) {
  switch (event) {
    case origin_gating::GateableEvent::kNavigationRequest:
      return "NavigationRequest";
    case origin_gating::GateableEvent::kNavigationResponse:
      return "NavigationResponse";
    case origin_gating::GateableEvent::kPageAction:
      return "PageAction";
  }
}

std::string DecisionSourceToString(DecisionSource source) {
  switch (source) {
    case DecisionSource::kAllowSameOrigin:
      return "AllowSameOrigin";
    case DecisionSource::kAllowHttpLocalhost:
      return "AllowHttpLocalhost";
    case DecisionSource::kAllowAboutBlank:
      return "AllowAboutBlank";
    case DecisionSource::kCacheWithUserConfirmation:
      return "CacheWithUserConfirmation";
    case DecisionSource::kCacheWithoutUserConfirmation:
      return "CacheWithoutUserConfirmation";
    case DecisionSource::kEnterprisePolicy:
      return "EnterprisePolicy";
    case DecisionSource::kForbidIpAddress:
      return "ForbidIpAddress";
    case DecisionSource::kRequireHttps:
      return "RequireHttps";
    case DecisionSource::kRequireHttpsOrHttp:
      return "RequireHttpsOrHttp";
    case DecisionSource::kActorContainerConfig:
      return "ActorContainerConfig";
    case DecisionSource::kNoVerdict:
      return "NoVerdict";
  }
}

DecisionAttribution::DecisionAttribution(DecisionSource source)
    : attribution_(source) {}

DecisionAttribution::DecisionAttribution(std::string custom_predicate_name)
    : attribution_(std::move(custom_predicate_name)) {}

DecisionAttribution::~DecisionAttribution() = default;

DecisionAttribution::DecisionAttribution(const DecisionAttribution&) = default;

DecisionAttribution& DecisionAttribution::operator=(
    const DecisionAttribution&) = default;

DecisionAttribution::DecisionAttribution(DecisionAttribution&&) = default;

DecisionAttribution& DecisionAttribution::operator=(DecisionAttribution&&) =
    default;

DecisionAttribution::Type DecisionAttribution::type() const {
  return std::holds_alternative<DecisionSource>(attribution_)
             ? Type::kDecisionSource
             : Type::kCustomPredicate;
}

DecisionSource DecisionAttribution::Source() const {
  CHECK(is_source());
  return std::get<DecisionSource>(attribution_);
}

const std::string& DecisionAttribution::CustomPredicateName() const {
  CHECK(is_custom_predicate());
  return std::get<std::string>(attribution_);
}

bool DecisionAttribution::operator==(DecisionSource source) const {
  return is_source() && Source() == source;
}

bool DecisionAttribution::operator==(std::string_view name) const {
  return is_custom_predicate() && CustomPredicateName() == name;
}

bool DecisionAttribution::operator==(const DecisionAttribution& other) const =
    default;

std::string DecisionAttribution::ToString() const {
  switch (type()) {
    case Type::kDecisionSource:
      return DecisionSourceToString(Source());
    case Type::kCustomPredicate:
      return CustomPredicateName();
  }
}

}  // namespace origin_gating
