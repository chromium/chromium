// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRITICAL_ACTIONS_CORE_BROWSER_CRITICAL_ACTION_METRICS_H_
#define COMPONENTS_CRITICAL_ACTIONS_CORE_BROWSER_CRITICAL_ACTION_METRICS_H_

#include <string_view>

#include "components/critical_actions/core/browser/critical_action_types.h"

namespace critical_actions {

// Outcome of resolving a conversation anchor ID for critical action entries
// at enqueue time.
// LINT.IfChange(ConversationAnchorResolutionOutcome)
enum class ConversationAnchorResolutionOutcome {
  kAlreadyPresent = 0,
  kResolvedFromCache = 1,
  kMissingTaskId = 2,
  kCacheMiss = 3,
  kDroppedTooLong = 4,
  kMaxValue = kDroppedTooLong,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/critical_actions/enums.xml:CriticalActionConversationAnchorResolutionOutcome)

// Outcome of resolving a conversation ID for critical action entries.
// LINT.IfChange(ConversationIdResolutionOutcome)
enum class ConversationIdResolutionOutcome {
  kAlreadyPresent = 0,
  kResolvedFromCache = 1,
  kMissingTaskId = 2,
  kDeferredCacheMiss = 3,
  kMaxValue = kDeferredCacheMiss,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/critical_actions/enums.xml:CriticalActionConversationIdResolutionOutcome)

// Outcome of deferred conversation ID resolution for critical action entries.
// LINT.IfChange(ConversationIdDeferredResolutionOutcome)
enum class ConversationIdDeferredResolutionOutcome {
  kBackfilled = 0,
  kEvictedServiceShutdown = 1,
  kEvictedCapacityExceeded = 2,
  kMaxValue = kEvictedCapacityExceeded,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/critical_actions/enums.xml:CriticalActionConversationIdDeferredResolutionOutcome)

// Outcome of resolving a navigation ID to a History Visit ID for critical
// action entries.
// LINT.IfChange(VisitIdResolutionOutcome)
enum class VisitIdResolutionOutcome {
  kSuccess = 0,
  kEvictedCapacityExceeded = 1,
  kEvictedNavigatedAway = 2,
  kDroppedNoNavigationId = 3,
  kEvictedServiceShutdown = 4,
  kMaxValue = kEvictedServiceShutdown,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/critical_actions/enums.xml:CriticalActionVisitIdResolutionOutcome)

// Returns the name of `source` as used in CriticalActions.* histogram
// suffixes.
std::string_view ActionSourceToHistogramSuffix(ActionSource source);

// Records CriticalActions.VisitIdResolutionOutcome.{ActionSource}.
void RecordVisitIdResolutionOutcome(ActionSource source,
                                    VisitIdResolutionOutcome outcome);

// Records
// CriticalActions.ConversationAnchorResolutionOutcome.{ActionSource}.
void RecordConversationAnchorResolutionOutcome(
    ActionSource source,
    ConversationAnchorResolutionOutcome outcome);

// Records CriticalActions.ConversationIdResolutionOutcome.{ActionSource}.
void RecordConversationIdResolutionOutcome(
    ActionSource source,
    ConversationIdResolutionOutcome outcome);

// Records
// CriticalActions.ConversationIdDeferredResolutionOutcome.{ActionSource}.
void RecordConversationIdDeferredResolutionOutcome(
    ActionSource source,
    ConversationIdDeferredResolutionOutcome outcome);

// Records CriticalActions.EventLogged.{ActionSource}.
void RecordEventLogged(ActionSource source, ActionType action_type);

}  // namespace critical_actions

#endif  // COMPONENTS_CRITICAL_ACTIONS_CORE_BROWSER_CRITICAL_ACTION_METRICS_H_
