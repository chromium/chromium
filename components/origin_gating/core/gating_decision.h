// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ORIGIN_GATING_CORE_GATING_DECISION_H_
#define COMPONENTS_ORIGIN_GATING_CORE_GATING_DECISION_H_

#include <memory>

#include "base/functional/callback.h"
#include "components/origin_gating/core/decision_attribution.h"

namespace origin_gating {

// The final gating verdict and its resolution metadata.
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

#endif  // COMPONENTS_ORIGIN_GATING_CORE_GATING_DECISION_H_
