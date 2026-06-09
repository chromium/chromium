// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MULTISTEP_FILTER_CORE_LOGGING_LOG_ENTRY_H_
#define COMPONENTS_MULTISTEP_FILTER_CORE_LOGGING_LOG_ENTRY_H_

#include <string>
#include <string_view>

#include "base/time/time.h"
#include "base/values.h"

namespace multistep_filter {

// Represents the types of events that can be logged by the Multistep Filter
// system.
enum class LogEventType {
  kNavigationStarted,
  kUrlEligibilityCheck,
  kAnnotationExtractionStarted,
  kAnnotationsExtracted,
  kSuggestionGenerationStarted,
  kNoSupportedTasks,
  kNoRelevantAnnotations,
  kServerRequestSent,
  kServerResponseReceived,
  kSuggestionGenerated,
  kSuggestionSuppressed,
  kSuggestionCleared,
  kSuggestionShown,
  kSuggestionAccepted,
  kSuggestionDismissed,
  kSuggestionIgnored,
  kServerRequestFailed,
  kServerResponseMalformed,
};

// Represents a single log entry for the Multistep Filter feature.
// LINT.IfChange(LogEntry)
struct LogEntry {
  // The time when the logged event occurred.
  base::Time timestamp = base::Time::Now();
  // A unique identifier for the navigation this event belongs to.
  int64_t navigation_id;
  // The type of event that occurred.
  LogEventType event_type = LogEventType::kNavigationStarted;
  // The effective Top-Level Domain plus one (eTLD+1) of the page where the
  // event occurred.
  std::string source_etld_plus_1;
  // Additional key-value details associated with the event.
  base::DictValue details;

  LogEntry(int64_t navigation_id,
           LogEventType type,
           std::string_view source_etld_plus_1);
  LogEntry(LogEntry&& other) noexcept;
  LogEntry& operator=(LogEntry&& other) noexcept;
  ~LogEntry();

  LogEntry(const LogEntry&) = delete;
  LogEntry& operator=(const LogEntry&) = delete;

  LogEntry Clone() const;

  // Converts the entry to a dictionary for Mojo/IPC communication.
  base::Value ToValue() const;

 private:
  // Internal constructor used for cloning.
  LogEntry(base::Time time,
           int64_t navigation_id,
           LogEventType type,
           std::string_view source_etld_plus_1);
};
// LINT.ThenChange(//chrome/browser/ui/webui/multistep_filter_internals/multistep_filter_internals.mojom:LogEntry)

}  // namespace multistep_filter

#endif  // COMPONENTS_MULTISTEP_FILTER_CORE_LOGGING_LOG_ENTRY_H_
