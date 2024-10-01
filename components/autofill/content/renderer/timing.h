// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_RENDERER_TIMING_H_
#define COMPONENTS_AUTOFILL_CONTENT_RENDERER_TIMING_H_

#include "base/memory/raw_ref.h"
#include "base/memory/stack_allocated.h"
#include "base/time/time.h"
#include "components/autofill/core/common/is_required.h"

namespace autofill {

// We measure the performance of some functions of interest per caller. The
// caller identifies itself to the function with this struct.
struct CallTimerState {
  enum class CallSite {
    kApplyFieldsAction,
    kBatchSelectOptionChange,
    kDidChangeScrollOffsetImpl,
    kExtractForm,
    kFocusedElementChanged,
    kFocusedElementChangedDeprecated,
    kGetFormDataFromUnownedInputElements,
    kGetFormDataFromWebForm,
    kGetSubmittedForm,
    kHandleCaretMovedInFormField,
    kJavaScriptChangedValue,
    kNotifyPasswordManagerAboutClearedForm,
    kOnFormSubmitted,
    kOnProvisionallySaveForm,
    kOnTextFieldDidChange,
    kQueryAutofillSuggestions,
    kShowSuggestionPopup,
    kUpdateFormCache,
    kUpdateLastInteractedElement,
  };
  CallSite call_site = internal::IsRequired();
  base::TimeTicks last_autofill_agent_reset = internal::IsRequired();
  base::TimeTicks last_dom_content_loaded = internal::IsRequired();
};

// Emits UMA metrics once it goes out of scope. It emits two types of metrics:
// - The duration of the object's scope.
// - The interval from the the last AutofillAgent::Reset() and the last
//   DOMContentLoad event (as per `state`) until the end of the timer's scope.
//
// These metrics are recorded in microseconds. They are only recorded if the
// machine's timer has a high resolution.
//
// We use this instead of SCOPED_UMA_HISTOGRAM_TIMER_MICROS() because we need to
// determine the metric name dynamically.
class ScopedCallTimer final {
  STACK_ALLOCATED();

 public:
  [[nodiscard]] explicit ScopedCallTimer(const char* name,
                                         const CallTimerState state);

  ScopedCallTimer(const ScopedCallTimer&) = delete;
  ScopedCallTimer& operator=(const ScopedCallTimer&) = delete;

  ~ScopedCallTimer();

 private:
  const CallTimerState state_;
  const char* const name_ = nullptr;
  const base::TimeTicks before_ = base::TimeTicks::Now();
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_RENDERER_TIMING_H_
