// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/timing.h"

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"

namespace autofill {

namespace {
std::string_view CallSiteToString(CallTimerState::CallSite call_site) {
  using enum CallTimerState::CallSite;
  switch (call_site) {
    case kApplyFieldsAction:
      return "ApplyFieldsAction";
    case kBatchSelectOptionChange:
      return "BatchSelectOptionChange";
    case kDidChangeScrollOffsetImpl:
      return "DidChangeScrollOffsetImpl";
    case kExtractForm:
      return "ExtractForm";
    case kFocusedElementChanged:
      return "FocusedElementChanged";
    case kFocusedElementChangedDeprecated:
      return "FocusedElementChangedDeprecated";
    case kGetFormDataFromUnownedInputElements:
      return "GetFormDataFromUnownedInputElements";
    case kGetFormDataFromWebForm:
      return "GetFormDataFromWebForm";
    case kGetSubmittedForm:
      return "GetSubmittedForm";
    case kHandleCaretMovedInFormField:
      return "HandleCaretMovedInFormField";
    case kJavaScriptChangedValue:
      return "JavaScriptChangedValue";
    case kNotifyPasswordManagerAboutClearedForm:
      return "NotifyPasswordManagerAboutClearedForm";
    case kOnFormSubmitted:
      return "OnFormSubmitted";
    case kOnProvisionallySaveForm:
      return "OnProvisionallySaveForm";
    case kOnTextFieldDidChange:
      return "OnTextFieldDidChange";
    case kQueryAutofillSuggestions:
      return "QueryAutofillSuggestions";
    case kShowSuggestionPopup:
      return "ShowSuggestionPopup";
    case kUpdateFormCache:
      return "UpdateFormCache";
    case kUpdateLastInteractedElement:
      return "UpdateLastInteractedElement";
  }
  NOTREACHED();
}
}  // namespace

ScopedCallTimer::ScopedCallTimer(const char* name, CallTimerState state)
    : state_(state), name_(name) {}

ScopedCallTimer::~ScopedCallTimer() {
  if (!base::TimeTicks::IsHighResolution()) {
    return;
  }
  base::TimeTicks after = base::TimeTicks::Now();
  std::string_view call_site = CallSiteToString(state_.call_site);

  {
    // Emit the duration of the timer's scope.
    auto record = [this](base::TimeDelta value, std::string_view suffix) {
      static constexpr std::string_view kPrefix = "Autofill.TimingPrecise.";
      base::UmaHistogramCustomMicrosecondsTimes(
          base::StrCat({kPrefix, name_, suffix.empty() ? "" : ".", suffix}),
          value, base::Microseconds(1), base::Seconds(1), 100);
    };
    record(after - before_, "");
    record(after - before_, call_site);
  }

  {
    // Emit the interval metrics from `state_.last_*` until the end of the
    // timer's scope.
    auto record = [this, call_site](base::TimeDelta value,
                                    std::string_view suffix) {
      static constexpr std::string_view kPrefix = "Autofill.TimingInterval.";
      DCHECK(!suffix.empty());
      base::UmaHistogramCustomMicrosecondsTimes(
          base::StrCat({kPrefix, name_, ".", call_site, ".", suffix}), value,
          base::Microseconds(1), base::Seconds(10), 100);
    };
    record(after - state_.last_autofill_agent_reset, "AutofillAgentReset");
    record(after - state_.last_dom_content_loaded, "DOMContentLoaded");
  }
}

}  // namespace autofill
