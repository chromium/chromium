// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/webauthn_metrics_util.h"

#include "base/metrics/histogram_functions.h"

namespace {

// Maximum bucket for reporting number of passkeys present for a given
// Conditional UI request.
constexpr int kPasskeyCountMax = 10;
constexpr std::string_view kCombinedSelectorActionHistogram =
    "WebAuthentication.GetCredentials.Immediate.CombinedSelectorActions";
constexpr std::string_view kCombinedSelectorCredentialCountHistogram =
    "WebAuthentication.GetCredentials.Immediate.CredentialCount";
constexpr std::string_view kRecoveryEventTypeHistogram =
    "WebAuthentication.GPM.RecoveryEvent";

// LINT.IfChange
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class CombinedSelectorAction {
  kAcceptWithSingleCredential = 0,
  kAcceptWithDefaultCredential = 1,
  kAcceptWithNonDefaultCredential = 2,
  kCancelButtonClicked = 3,
  kMaxValue = kCancelButtonClicked,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/webauthn/enums.xml)

}  // namespace

namespace webauthn::metrics {

void RecordCombinedSelectorShown(int credential_count) {
  base::UmaHistogramCounts100(kCombinedSelectorCredentialCountHistogram,
                              credential_count);
}

void RecordCombinedSelectorAccept(int credential_count, bool default_selected) {
  if (credential_count == 1) {
    base::UmaHistogramEnumeration(
        kCombinedSelectorActionHistogram,
        CombinedSelectorAction::kAcceptWithSingleCredential);
    return;
  }
  base::UmaHistogramEnumeration(
      kCombinedSelectorActionHistogram,
      default_selected
          ? CombinedSelectorAction::kAcceptWithDefaultCredential
          : CombinedSelectorAction::kAcceptWithNonDefaultCredential);
}

void RecordCombinedSelectorCancelButtonClicked() {
  base::UmaHistogramEnumeration(kCombinedSelectorActionHistogram,
                                CombinedSelectorAction::kCancelButtonClicked);
}

void RecordGPMRecoveryEvent(
    webauthn::metrics::WebAuthenticationGPMRecoveryEvent event) {
  base::UmaHistogramEnumeration(kRecoveryEventTypeHistogram, event);
}

}  // namespace webauthn::metrics

void ReportConditionalUiPasskeyCount(int passkey_count) {
  base::UmaHistogramExactLinear("WebAuthentication.ConditionalUiPasskeyCount",
                                passkey_count, kPasskeyCountMax);
}

void RecordGPMMakeCredentialEvent(
    webauthn::metrics::GPMMakeCredentialEvents event) {
  base::UmaHistogramEnumeration("WebAuthentication.GPM.MakeCredential", event);
}

void RecordGPMGetAssertionEvent(
    webauthn::metrics::GPMGetAssertionEvents event) {
  base::UmaHistogramEnumeration("WebAuthentication.GPM.GetAssertion", event);
}

void RecordOnboardingEvent(webauthn::metrics::OnboardingEvents event) {
  base::UmaHistogramEnumeration("WebAuthentication.OnboardingEvents", event);
}
