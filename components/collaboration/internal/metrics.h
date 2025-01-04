// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COLLABORATION_INTERNAL_METRICS_H_
#define COMPONENTS_COLLABORATION_INTERNAL_METRICS_H_

namespace collaboration::metrics {

// Types of join events that occur in the collaboration service.
// These values are persisted to logs. Entries should not be renumbered and
// number values should never be reused.
// LINT.IfChange(CollaborationServiceJoinEvent)
enum class CollaborationServiceJoinEvent {
  kUnknown = 0,
  kStarted = 1,
  kMaxValue = kStarted
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/collaboration_service/enums.xml:CollaborationServiceJoinEvent)

void RecordJoinEvent(CollaborationServiceJoinEvent event);
}  // namespace collaboration::metrics

#endif  // COMPONENTS_COLLABORATION_INTERNAL_METRICS_H_
