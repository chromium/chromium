// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AT_MEMORY_AT_MEMORY_FUNNEL_METRICS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AT_MEMORY_AT_MEMORY_FUNNEL_METRICS_H_

#include <optional>

#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/common/aliases.h"

namespace autofill {

// Encapsulates the state and logging logic for the @memory search funnel.
// This class tracks the progression of a user's interaction with the @memory
// suggestions, from the initial display to the submission of a query.
class AtMemoryFunnelMetrics {
 public:
  AtMemoryFunnelMetrics();
  AtMemoryFunnelMetrics(const AtMemoryFunnelMetrics&) = delete;
  AtMemoryFunnelMetrics& operator=(const AtMemoryFunnelMetrics&) = delete;
  virtual ~AtMemoryFunnelMetrics();

  // Records that the popup UI was successfully displayed to the user.
  // This emits the "PopupDisplayed" metric. This method is idempotent; only
  // the first call per session will record metrics, and subsequent calls
  // with potentially different trigger sources are ignored. This is consistent
  // with the popup lifecycle, where a change in trigger mechanism would
  // typically result in the popup being hidden and a new session starting.
  // Virtual for testing.
  virtual void OnPopupShown(AutofillSuggestionTriggerSource trigger_source);

  // Records that at least one search query was submitted during this session.
  // Virtual for testing.
  virtual void OnQuerySubmitted();

  // Records that a suggestion was accepted during this session.
  // Virtual for testing.
  virtual void OnSuggestionAccepted();

  // Records the final state of the funnel when the UI is hidden.
  // Emits summary metrics (like QuerySubmitted) only if the popup was shown.
  // Virtual for testing.
  virtual void OnPopupHidden();

 private:
  // The trigger source of the popup. It is `std::nullopt` until `OnPopupShown`
  // is called, serving as a signal that the popup was shown.
  std::optional<AutofillMetrics::AtMemoryTriggerSource> source_;
  bool query_submitted_ = false;
  bool suggestion_accepted_ = false;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AT_MEMORY_AT_MEMORY_FUNNEL_METRICS_H_
