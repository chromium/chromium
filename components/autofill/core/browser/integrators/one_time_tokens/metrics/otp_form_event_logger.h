// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_METRICS_OTP_FORM_EVENT_LOGGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_METRICS_OTP_FORM_EVENT_LOGGER_H_

#include <string_view>

#include "base/containers/span.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/form_types.h"
#include "components/autofill/core/browser/metrics/form_events/form_event_logger_base.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/suggestions/suggestion_util.h"
#include "components/autofill/core/common/dense_set.h"
#include "components/autofill/core/common/signatures.h"

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
  void OnDestroyed() override;

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
      const FormStructure& form,
      AutocompleteUnrecognizedBehavior ac_unrecognized_behavior) const override;

  // Also record the key metrics as `OneTimePassword.GmailEligibleClients` for
  // users who are eligible for Gmail OTPs.
  void RecordFillingReadiness(LogBuffer& logs) const override;
  void RecordFillingAcceptance(LogBuffer& logs) const override;
  void RecordFillingCorrectness(LogBuffer& logs) const override;
  void RecordFillingAssistance(LogBuffer& logs) const override;

 private:
  // Returns whether the user is eligible for Gmail OTPs: Gmail OTP suggestions
  // are enabled, the user opted into them, a Gmail OTP backend exists, and the
  // user is signed in with a Gmail account. This does not guarantee that the
  // user gets Gmail OTPs, e.g. their Gmail settings may still block them.
  bool IsEligibleForGmailOtps() const;

  // Records
  // `Autofill.KeyMetrics.{metric}.OneTimePassword.GmailEligibleClients` if the
  // user is eligible for Gmail OTPs.
  void RecordGmailKeyMetric(std::string_view metric, bool value) const;

  // Tracks the number of OTPs that have been made available for filling during
  // the lifetime of a form.
  bool otp_for_filling_existed_ = false;

  // Set in `OnDestroyed()`, right before the metrics are recorded.
  bool is_eligible_for_gmail_otps_ = false;
};

}  // namespace autofill::autofill_metrics
#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_ONE_TIME_TOKENS_METRICS_OTP_FORM_EVENT_LOGGER_H_
