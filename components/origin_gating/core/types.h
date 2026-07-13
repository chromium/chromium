// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ORIGIN_GATING_CORE_TYPES_H_
#define COMPONENTS_ORIGIN_GATING_CORE_TYPES_H_

#include <memory>
#include <string>
#include <string_view>
#include <variant>

#include "base/containers/enum_set.h"
#include "base/functional/callback.h"

namespace origin_gating {

// Enumerate the result of a single predicate check.
enum class Decision {
  // The predicate neither explicitly allowed nor explicitly blocked the event.
  kNoDecision,
  // The predicate explicitly allowed the event.
  kAllowed,
  // The predicate explicitly blocked the event.
  kBlocked,
};

// Enumerates the category of event being evaluated. A single predicate may
// apply to some events but not others (e.g. dangerous MIME type check is
// applicable only to navigation response), so callers pass the relevant event
// to ComputeGatingDecision and each predicate declares the set of events it
// applies to.
enum class GateableEvent {
  // A navigation request is starting, or is being redirected.
  kNavigationRequest,
  // A navigation response is being processed (the final, committed URL).
  kNavigationResponse,
  // An action is being performed on an existing tab/page.
  kPageAction,
};

using GateableEventSet = base::EnumSet<GateableEvent,
                                       GateableEvent::kNavigationRequest,
                                       GateableEvent::kPageAction>;

// Enumerates the source of any positive/negative decision.
enum class DecisionSource {
  // Predicate that allows if the origins in question are same-origin with each
  // other.
  kAllowSameOrigin,
  // Predicate that allows if the user has already confirmed the origin in
  // question.
  kCacheWithUserConfirmation,
  // Predicate that allows if the origin is already present in the cache and the
  // delegate does not require user confirmation for that origin.
  kCacheWithoutUserConfirmation,
  // No decision was reached before the OriginGating framework ran out of
  // predicates to run.
  kNoVerdict,
};

// Encapsulates the source of any positive/negative gating verdict.
class DecisionAttribution {
 public:
  enum class Type {
    kDecisionSource,
    kCustomPredicate,
  };

  DecisionAttribution() = delete;
  explicit DecisionAttribution(DecisionSource source);
  explicit DecisionAttribution(std::string custom_predicate_name);

  ~DecisionAttribution();
  DecisionAttribution(const DecisionAttribution&);
  DecisionAttribution& operator=(const DecisionAttribution&);
  DecisionAttribution(DecisionAttribution&&);
  DecisionAttribution& operator=(DecisionAttribution&&);

  Type type() const;

  // Returns the DecisionSource. Safe to call only when `type()` is
  // `Type::kDecisionSource`.
  DecisionSource Source() const;

  // Returns the custom predicate name. Safe to call only when `type()` is
  // `Type::kCustomPredicate`.
  const std::string& CustomPredicateName() const;

  bool operator==(DecisionSource source) const;
  bool operator==(std::string_view name) const;
  bool operator==(const DecisionAttribution& other) const;

 private:
  bool is_source() const { return type() == Type::kDecisionSource; }
  bool is_custom_predicate() const { return type() == Type::kCustomPredicate; }

  std::variant<DecisionSource, std::string> attribution_;
};

// Struct wrapping the final gating verdict and its resolution metadata.
struct GatingDecision {
  bool is_allowed = false;
  DecisionAttribution attribution;
};

// Opaque virtual base class for consumer-specific context passed along the
// gating decision chain.
class GatingDecisionContext {
 public:
  virtual ~GatingDecisionContext() = default;
};

using GatingDecisionCallback =
    base::OnceCallback<void(std::unique_ptr<GatingDecisionContext>,
                            GatingDecision)>;

}  // namespace origin_gating

#endif  // COMPONENTS_ORIGIN_GATING_CORE_TYPES_H_
