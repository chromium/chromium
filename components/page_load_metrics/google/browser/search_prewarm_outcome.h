// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_GOOGLE_BROWSER_SEARCH_PREWARM_OUTCOME_H_
#define COMPONENTS_PAGE_LOAD_METRICS_GOOGLE_BROWSER_SEARCH_PREWARM_OUTCOME_H_

namespace page_load_metrics {

// The outcome of a DSE search prewarm attempt.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(SearchPrewarmOutcome)
enum class SearchPrewarmOutcome {
  // Primary search navigation reused the prewarmed RenderProcessHost.
  kProcessReusedByPrimarySearch = 0,
  // Prewarm completed (became ready), but process was NOT reused by
  // any search navigation.
  kUnused = 1,
  // Cancelled by a subsequent prewarm before being used.
  kCancelledByNewPrewarm = 2,
  // Prewarm failed before becoming ready (HTTP error, timeout, limit).
  kFailedBeforeReady = 3,
  // Cancelled before becoming ready.
  kCancelledBeforeReady = 4,

  kMaxValue = kCancelledBeforeReady,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/page/enums.xml:SearchPrewarmOutcome)

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_GOOGLE_BROWSER_SEARCH_PREWARM_OUTCOME_H_
