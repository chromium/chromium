// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ORIGIN_GATING_CORE_DECISION_H_
#define COMPONENTS_ORIGIN_GATING_CORE_DECISION_H_

namespace origin_gating {

// The result of a single predicate check.
enum class Decision {
  // The predicate neither explicitly allowed nor explicitly blocked the event.
  kNoDecision,
  // The predicate explicitly allowed the event.
  kAllowed,
  // The predicate explicitly blocked the event.
  kBlocked,
};

}  // namespace origin_gating

#endif  // COMPONENTS_ORIGIN_GATING_CORE_DECISION_H_
