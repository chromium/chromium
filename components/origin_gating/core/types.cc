// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/origin_gating/core/types.h"

#include <string>
#include <utility>
#include <variant>

#include "base/check.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"

namespace origin_gating {

GateableEvent::GateableEvent(NavigationRequestEvent event)
    : data_(std::move(event)) {}

GateableEvent::GateableEvent(NavigationResponseEvent event)
    : data_(std::move(event)) {}

GateableEvent::GateableEvent(PageActionEvent event) : data_(std::move(event)) {}

GateableEvent::GateableEvent(const GateableEvent&) = default;
GateableEvent& GateableEvent::operator=(const GateableEvent&) = default;
GateableEvent::GateableEvent(GateableEvent&&) = default;
GateableEvent& GateableEvent::operator=(GateableEvent&&) = default;
GateableEvent::~GateableEvent() = default;

GateableEvent::Type GateableEvent::type() const {
  return static_cast<Type>(data_.index());
}

const GURL& GateableEvent::destination() const {
  return std::visit(
      [](const auto& event) -> const GURL& { return event.destination; },
      data_);
}

const GURL* GateableEvent::source() const {
  return std::visit(
      absl::Overload{
          [](const NavigationRequestEvent& event) -> const GURL* {
            return &event.source;
          },
          [](const NavigationResponseEvent& event) -> const GURL* {
            return &event.source;
          },
          [](const PageActionEvent&) -> const GURL* { return nullptr; },
      },
      data_);
}

std::string GateableEventTypeToString(GateableEvent::Type type) {
  switch (type) {
    case GateableEvent::Type::kNavigationRequest:
      return "NavigationRequest";
    case GateableEvent::Type::kNavigationResponse:
      return "NavigationResponse";
    case GateableEvent::Type::kPageAction:
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
    case DecisionSource::kForbidNonLocalhostIpAddress:
      return "ForbidNonLocalhostIpAddress";
    case DecisionSource::kRequireHttpsOrLocalhost:
      return "RequireHttpsOrLocalhost";
    case DecisionSource::kRequireHttpsOrHttp:
      return "RequireHttpsOrHttp";
    case DecisionSource::kBlockByTaskPolicyConfig:
      return "BlockByTaskPolicyConfig";
    case DecisionSource::kAllowByTaskPolicyConfig:
      return "AllowByTaskPolicyConfig";
    case DecisionSource::kNoVerdict:
      return "NoVerdict";
  }
}

DecisionAttribution::DecisionAttribution(DecisionSource source)
    : attribution_(source) {}

DecisionAttribution::DecisionAttribution(
    const CustomPredicateAttribution& attribution)
    : attribution_(attribution) {}

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

bool DecisionAttribution::operator==(DecisionSource source) const {
  return is_source() && Source() == source;
}

}  // namespace origin_gating
