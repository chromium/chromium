// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/at_memory/at_memory_funnel_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"

namespace autofill {

AtMemoryFunnelMetrics::AtMemoryFunnelMetrics() = default;

AtMemoryFunnelMetrics::~AtMemoryFunnelMetrics() = default;

void AtMemoryFunnelMetrics::OnPopupShown(
    AutofillSuggestionTriggerSource trigger_source) {
  if (source_.has_value()) {
    return;
  }

  switch (trigger_source) {
    case AutofillSuggestionTriggerSource::kAtMemory:
      source_ = AutofillMetrics::AtMemoryTriggerSource::kTypedTrigger;
      break;
    case AutofillSuggestionTriggerSource::kAtMemoryContextMenu:
      source_ = AutofillMetrics::AtMemoryTriggerSource::kContextMenu;
      break;
    case AutofillSuggestionTriggerSource::kUnspecified:
    case AutofillSuggestionTriggerSource::kFormControlElementClicked:
    case AutofillSuggestionTriggerSource::kTextareaFocusedWithoutClick:
    case AutofillSuggestionTriggerSource::kContentEditableClicked:
    case AutofillSuggestionTriggerSource::kTextFieldValueChanged:
    case AutofillSuggestionTriggerSource::kTextFieldDidReceiveKeyDown:
    case AutofillSuggestionTriggerSource::kOpenTextDataListChooser:
    case AutofillSuggestionTriggerSource::kPasswordManager:
    case AutofillSuggestionTriggerSource::kiOS:
    case AutofillSuggestionTriggerSource::kManualFallbackPasswords:
    case AutofillSuggestionTriggerSource::kManualFallbackPlusAddresses:
    case AutofillSuggestionTriggerSource::kComposeDialogLostFocus:
    case AutofillSuggestionTriggerSource::kComposeDelayedProactiveNudge:
    case AutofillSuggestionTriggerSource::kPasswordManagerProcessedFocusedField:
    case AutofillSuggestionTriggerSource::kPlusAddressUpdatedInBrowserProcess:
    case AutofillSuggestionTriggerSource::kProactivePasswordRecovery:
    case AutofillSuggestionTriggerSource::kGlic:
      // This class should only be used for @memory searches.
      NOTREACHED();
  }

  // `source_` is set only when the popup is successfully displayed. This
  // serves as a signal that the user has actually seen the suggestions.
  base::UmaHistogramEnumeration("Autofill.AtMemory.Funnel.PopupDisplayed",
                                *source_);
}

void AtMemoryFunnelMetrics::OnQuerySubmitted() {
  query_submitted_ = true;
}

void AtMemoryFunnelMetrics::OnSuggestionAccepted() {
  suggestion_accepted_ = true;
}

void AtMemoryFunnelMetrics::OnPopupHidden() {
  // Only log summary metrics if the popup was successfully shown.
  // This avoids polluting the "No Query Submitted" data with cases where the
  // popup was hidden immediately after initialization (e.g., due to focus
  // loss) before the user could see or interact with it.
  if (source_.has_value()) {
    base::UmaHistogramBoolean("Autofill.AtMemory.Funnel.QuerySubmitted",
                              query_submitted_);
    base::UmaHistogramBoolean("Autofill.AtMemory.Funnel.SuggestionAccepted",
                              suggestion_accepted_);
  }
}

}  // namespace autofill
