// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_COMMON_PAGE_VISIT_FINAL_STATUS_H_
#define COMPONENTS_PAGE_LOAD_METRICS_COMMON_PAGE_VISIT_FINAL_STATUS_H_

namespace page_load_metrics {

// This enum represents the status of a page visit: abort, non-abort, or
// neither. A page is of type NEVER_FOREGROUND if it was never in the
// foreground. A page is of type ABORT if it was in the foreground at some
// point but did not reach FCP. A page is of type REACHED_FCP if it was in the
// foreground at some point and reached FCP. These values are persisted to logs.
// Entries should not be renumbered and numeric values should never be reused.
// For any additions, also update the corresponding enum in enums.xml.
enum class PageVisitFinalStatus {
  kNeverForegrounded = 0,
  kAborted = 1,
  kReachedFCP = 2,
  kMaxValue = kReachedFCP,
};

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_COMMON_PAGE_VISIT_FINAL_STATUS_H_
