// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/webauthn_metrics_util.h"

#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

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
constexpr std::string_view
    kCachedOpportunisticallyRetrievedKeyEventTypeHistogram =
        "WebAuthentication.GPM.CachedOpportunisticallyRetrievedKeyEvent";

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

webauthn::metrics::AmbientUkmDisplayOutcome ToUkmDisplayOutcome(
    webauthn::metrics::AmbientDisplayOutcome outcome) {
  switch (outcome) {
    case webauthn::metrics::AmbientDisplayOutcome::kShown:
      return webauthn::metrics::AmbientUkmDisplayOutcome::kShown;
    case webauthn::metrics::AmbientDisplayOutcome::kNoCredentials:
      return webauthn::metrics::AmbientUkmDisplayOutcome::kNoCredentials;
    case webauthn::metrics::AmbientDisplayOutcome::kNotPrimaryPage:
    case webauthn::metrics::AmbientDisplayOutcome::kNoPermissionRequestManager:
    case webauthn::metrics::AmbientDisplayOutcome::kAnotherRequestPending:
      return webauthn::metrics::AmbientUkmDisplayOutcome::kNotShownOther;
  }
}

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

void RecordGPMCachedOpportunisticallyRetrievedKeyEvent(
    webauthn::metrics::
        WebAuthenticationGPMCachedOpportunisticallyRetrievedKeyEvent event) {
  base::UmaHistogramEnumeration(
      kCachedOpportunisticallyRetrievedKeyEventTypeHistogram, event);
}

void RecordAmbientDisplayOutcome(ukm::SourceId source_id,
                                 AmbientDisplayOutcome outcome) {
  base::UmaHistogramEnumeration("WebAuthentication.Ambient.DisplayOutcome",
                                outcome);
  if (source_id != ukm::kInvalidSourceId) {
    ukm::builders::WebAuthn_AmbientSignIn(source_id)
        .SetDisplayOutcome(static_cast<int64_t>(ToUkmDisplayOutcome(outcome)))
        .Record(ukm::UkmRecorder::Get());
  }
}

void RecordAmbientInitialUiMode(ukm::SourceId source_id,
                                AmbientInitialUiMode mode) {
  base::UmaHistogramEnumeration("WebAuthentication.Ambient.InitialUiMode",
                                mode);
  if (source_id != ukm::kInvalidSourceId) {
    ukm::builders::WebAuthn_AmbientSignIn(source_id)
        .SetInitialUiMode(static_cast<int64_t>(mode))
        .Record(ukm::UkmRecorder::Get());
  }
}

void RecordAmbientBubbleCredentialCount(size_t credential_count) {
  base::UmaHistogramCounts100("WebAuthentication.Ambient.BubbleCredentialCount",
                              base::saturated_cast<int>(credential_count));
}

void RecordAmbientBubbleResolution(bool bubble_initially_shown,
                                   AmbientBubbleResolution resolution) {
  base::UmaHistogramEnumeration(
      bubble_initially_shown
          ? "WebAuthentication.Ambient.BubbleResolution.InitiallyShown"
          : "WebAuthentication.Ambient.BubbleResolution.OpenedFromChip",
      resolution);
}

void RecordAmbientCredentialSelectionMethod(
    AmbientCredentialSelectionMethod method) {
  base::UmaHistogramEnumeration(
      "WebAuthentication.Ambient.CredentialSelectionMethod", method);
}

void RecordAmbientChipOnlyOutcome(ukm::SourceId source_id,
                                  AmbientChipOnlyOutcome outcome) {
  base::UmaHistogramEnumeration("WebAuthentication.Ambient.ChipOnlyOutcome",
                                outcome);
  if (source_id != ukm::kInvalidSourceId) {
    ukm::builders::WebAuthn_AmbientSignIn(source_id)
        .SetChipOnlyOutcome(static_cast<int64_t>(outcome))
        .Record(ukm::UkmRecorder::Get());
  }
}

void RecordAmbientUkmBubbleOutcome(ukm::SourceId source_id,
                                   AmbientUkmBubbleOutcome outcome) {
  if (source_id != ukm::kInvalidSourceId) {
    ukm::builders::WebAuthn_AmbientSignIn(source_id)
        .SetBubbleOutcome(static_cast<int64_t>(outcome))
        .Record(ukm::UkmRecorder::Get());
  }
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
