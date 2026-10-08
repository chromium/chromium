// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ORIGIN_GATING_CORE_DECISION_SOURCE_H_
#define COMPONENTS_ORIGIN_GATING_CORE_DECISION_SOURCE_H_

#include <string>

namespace origin_gating {

// The source of any positive/negative decision.
enum class DecisionSource {
  // Predicate that allows if the origins in question are same-origin with each
  // other. Not supported for `GateableEvent::kPageAction` events.
  kAllowSameOrigin,
  // Predicate that allows if the destination is a localhost URL with an http or
  // https scheme.
  kAllowHttpLocalhost,
  // Predicate that allows if the destination is about:blank.
  kAllowAboutBlank,
  // Predicate that allows if the user has already confirmed the origin in
  // question.
  kCacheWithUserConfirmation,
  // Predicate that allows if the origin is already present in the cache and the
  // delegate does not require user confirmation for that origin.
  kCacheWithoutUserConfirmation,
  // Evaluates the destination against an enterprise policy allow/blocklist. The
  // delegate provides the embedder-specific logic via
  // `Delegate::EvaluateEnterprisePolicy`.
  kEnterprisePolicy,
  // Predicate that blocks if the destination's host is an IP address, unless
  // it is a loopback/localhost IP address.
  kForbidNonLocalhostIpAddress,
  // Predicate that blocks if the destination's scheme is not https, unless the
  // destination is localhost.
  kRequireHttpsOrLocalhost,
  // Predicate that blocks if the destination's scheme is neither https nor
  // http.
  kRequireHttpsOrHttp,
  // Predicate that evaluates the destination against the task policy
  // configuration and blocks if blocked, otherwise returns kNoDecision.
  kBlockByTaskPolicyConfig,
  // Predicate that evaluates the destination against the task policy
  // configuration and allows if allowed, otherwise returns kNoDecision.
  kAllowByTaskPolicyConfig,
  // No decision was reached before the OriginGating framework ran out of
  // predicates to run.
  kNoVerdict,
};

std::string DecisionSourceToString(DecisionSource source);

}  // namespace origin_gating

#endif  // COMPONENTS_ORIGIN_GATING_CORE_DECISION_SOURCE_H_
