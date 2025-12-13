// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_METRICS_OTP_FORM_EVENT_LOGGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_METRICS_OTP_FORM_EVENT_LOGGER_H_

#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/metrics/form_events/form_event_logger_base.h"
#include "components/autofill/core/browser/metrics/form_events/form_events.h"

namespace autofill::autofill_metrics {

class OtpFormEventLogger : public FormEventLoggerBase {
 public:
  explicit OtpFormEventLogger(BrowserAutofillManager* owner);

  ~OtpFormEventLogger() override;

  // Implementation of FormEventLoggerBase:
  void OnDidShowSuggestions(const FormStructure& form,
                            const AutofillField& field,
                            base::TimeTicks form_parsed_timestamp,
                            bool off_the_record,
                            base::span<const Suggestion> suggestions) override;

  void LogUkmInteractedWithForm(FormSignature form_signature) override;

  void OnDidFillOtpSuggestion(const FormStructure& form,
                              const AutofillField& field);

  // Call this method when an OTP becomes available for suggestions.
  void OnOtpAvailable();

  bool HasLoggedDataToFillAvailableForTesting() {
    return HasLoggedDataToFillAvailable();
  }

 protected:
  void RecordParseForm() override;
  void RecordShowSuggestions() override;

  // Used to track KeyMetrics.Readiness.
  bool HasLoggedDataToFillAvailable() const override;

  DenseSet<FormTypeNameForLogging> GetSupportedFormTypeNamesForLogging()
      const override;
  DenseSet<FormTypeNameForLogging> GetFormTypesForLogging(
      const FormStructure& form) const override;

 private:
  // Tracks the number of OTPs that have been made available for filling during
  // the lifetime of a form.
  bool otp_for_filling_existed_ = false;
};

}  // namespace autofill::autofill_metrics
#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_METRICS_OTP_FORM_EVENT_LOGGER_H_
