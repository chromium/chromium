// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ORIGIN_GATING_CORE_TYPES_H_
#define COMPONENTS_ORIGIN_GATING_CORE_TYPES_H_

#include <optional>

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

// Struct wrapping the final gating verdict and its resolution metadata.
struct GatingDecision {
  bool is_allowed = false;
  DecisionSource source;
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
