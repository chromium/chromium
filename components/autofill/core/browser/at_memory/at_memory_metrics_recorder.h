// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AT_MEMORY_AT_MEMORY_METRICS_RECORDER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AT_MEMORY_AT_MEMORY_METRICS_RECORDER_H_

#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/common/aliases.h"
#include "url/gurl.h"

namespace optimization_guide {
class ModelQualityLogEntry;
class ModelQualityLogsUploaderService;
}  // namespace optimization_guide

namespace autofill {

// Encapsulates the state and logging logic for the @memory search funnel.
// This class tracks the progression of a user's interaction with the @memory
// suggestions, from the initial display to the submission of a query.
class AtMemoryMetricsRecorder {
 public:
  AtMemoryMetricsRecorder(
      optimization_guide::ModelQualityLogsUploaderService* uploader_service,
      GURL url,
      std::u16string_view title);
  AtMemoryMetricsRecorder(const AtMemoryMetricsRecorder&) = delete;
  AtMemoryMetricsRecorder& operator=(const AtMemoryMetricsRecorder&) = delete;
  ~AtMemoryMetricsRecorder();

  // Records that the popup UI was successfully displayed to the user.
  // This emits the "PopupDisplayed" metric. This method is idempotent; only
  // the first call per session will record metrics, and subsequent calls
  // with potentially different trigger sources are ignored. This is consistent
  // with the popup lifecycle, where a change in trigger mechanism would
  // typically result in the popup being hidden and a new session starting.
  void OnPopupShown(AutofillSuggestionTriggerSource trigger_source);

  // Records that a search query was submitted during this session.
  void OnQuerySubmitted(std::u16string query);

  // Records that a suggestion was accepted during this session.
  void OnSuggestionAccepted();

  // Records that the suggestion was successfully filled.
  void MarkFilled();

  // Records the start time of the asynchronous PII fetching process.
  void OnFetchPiiStarted();

  // Records the completion of the asynchronous PII fetching process.
  void OnFetchPiiCompleted();

 private:
  // The trigger source of the popup. It is `std::nullopt` until `OnPopupShown`
  // is called, serving as a signal that the popup was shown.
  std::optional<AutofillMetrics::AtMemoryTriggerSource> source_;
  bool query_submitted_ = false;
  // The pending log entry to be uploaded to MQLS for the query.
  std::unique_ptr<optimization_guide::ModelQualityLogEntry> pending_log_entry_;
  bool suggestion_accepted_ = false;
  bool was_filled_ = false;
  // The start time of the asynchronous fetch/unmask process.
  std::optional<base::TimeTicks> fetch_pii_start_time_;
  // The duration of the successful asynchronous fetch/unmask process.
  std::optional<base::TimeDelta> fetch_pii_duration_;

  // The URL of the primary page the user triggered the @memory search on.
  GURL url_;
  // The title of the primary page the user triggered the @memory search on.
  std::u16string title_;

  // The uploader service used to log metrics to MQLS. Not owned. Guaranteed to
  // outlive `this`.
  raw_ptr<optimization_guide::ModelQualityLogsUploaderService>
      uploader_service_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AT_MEMORY_AT_MEMORY_METRICS_RECORDER_H_
