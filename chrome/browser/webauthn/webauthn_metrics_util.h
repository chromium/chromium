// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_WEBAUTHN_METRICS_UTIL_H_
#define CHROME_BROWSER_WEBAUTHN_WEBAUTHN_METRICS_UTIL_H_

#include <cstddef>

#include "services/metrics/public/cpp/ukm_source_id.h"

namespace webauthn::metrics {
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class GPMMakeCredentialEvents {
  // LINT.IfChange
  kStarted = 0,
  kSuccess = 1,
  kFailure = 2,
  kMaxValue = kFailure,
  // LINT.ThenChange(//tools/metrics/histograms/metadata/webauthn/enums.xml)
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class GPMGetAssertionEvents {
  // LINT.IfChange
  kStarted = 0,
  kSuccess = 1,
  kFailure = 2,
  kMaxValue = kFailure,
  // LINT.ThenChange(//tools/metrics/histograms/metadata/webauthn/enums.xml)
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class OnboardingEvents {
  // LINT.IfChange
  kStarted = 0,
  kSucceeded = 1,
  kCreateGpmPasskeySheetCancelled = 2,
  kCreateGpmPasskeySheetSaveAnotherWaySelected = 3,
  kAuthenticatorGpmPinSheetCancelled = 4,
  kFailure = 5,
  kMaxValue = kFailure,
  // LINT.ThenChange(//tools/metrics/histograms/metadata/webauthn/enums.xml)
};

// Enum for the WebAuthentication.StoreKeysFlowType histogram.
enum class WebAuthenticationGPMRecoveryEvent {
  // LINT.IfChange(WebAuthenticationGPMRecoveryEvent)
  kStoreKeysFromExplicitFlowStarted = 0,
  kStoreKeysFromExplicitFlowSucceeded = 1,
  kStoreKeysFromOpportunisticFlowStarted = 2,
  kStoreKeysFromOpportunisticFlowSucceeded = 3,
  kStoreKeysFromOpportunisticFlowIgnoredRedundant = 4,
  kStoreKeysFromOpportunisticFlowFailed = 6,
  kStoreKeysFromOpportunisticFlowCachedKeysBecauseAccountDoesNotMatch = 7,
  kMaxValue =
      kStoreKeysFromOpportunisticFlowCachedKeysBecauseAccountDoesNotMatch,
  // LINT.ThenChange(//tools/metrics/histograms/metadata/webauthn/enums.xml:WebAuthenticationGPMRecoveryEvent)
};

// Enum for the WebAuthentication.StoreKeysFlowType histogram.
enum class WebAuthenticationGPMCachedOpportunisticallyRetrievedKeyEvent {
  // LINT.IfChange(WebAuthenticationGPMCachedOpportunisticallyRetrievedKeyEvent)
  kStoreKeysFromOpportunisticFlowCachedKeysRemovedAfterTimeout = 0,
  kStoreKeysFromOpportunisticFlowCachedKeysStoringAfterSignIn = 1,
  kStoreKeysFromOpportunisticFlowCachedKeysHaveBeenOverwritten = 2,
  kMaxValue = kStoreKeysFromOpportunisticFlowCachedKeysHaveBeenOverwritten,
  // LINT.ThenChange(//tools/metrics/histograms/metadata/webauthn/enums.xml:WebAuthenticationGPMCachedOpportunisticallyRetrievedKeyEvent)
};

void RecordGPMRecoveryEvent(WebAuthenticationGPMRecoveryEvent event);
void RecordGPMCachedOpportunisticallyRetrievedKeyEvent(
    webauthn::metrics::
        WebAuthenticationGPMCachedOpportunisticallyRetrievedKeyEvent event);

void RecordCombinedSelectorShown(int credential_count);
void RecordCombinedSelectorAccept(int credential_count, bool default_selected);
void RecordCombinedSelectorCancelButtonClicked();

// Enum for the WebAuthentication.Ambient.DisplayOutcome histogram.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class AmbientDisplayOutcome {
  // LINT.IfChange(AmbientDisplayOutcome)
  kShown = 0,
  kNoCredentials = 1,
  kNotPrimaryPage = 2,
  kNoPermissionRequestManager = 3,
  kAnotherRequestPending = 4,
  kMaxValue = kAnotherRequestPending,
  // LINT.ThenChange(//tools/metrics/histograms/metadata/webauthn/enums.xml:AmbientDisplayOutcome)
};

// Enum for the WebAuthentication.Ambient.InitialUiMode histogram.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class AmbientInitialUiMode {
  // LINT.IfChange(AmbientInitialUiMode)
  kBubble = 0,
  kChipOnlyRequestedBySite = 1,
  kChipOnlyCooldownActive = 2,
  kMaxValue = kChipOnlyCooldownActive,
  // LINT.ThenChange(//tools/metrics/histograms/metadata/webauthn/enums.xml:AmbientInitialUiMode)
};

// Enum for the WebAuthentication.Ambient.BubbleResolution.* histograms.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class AmbientBubbleResolution {
  // LINT.IfChange(AmbientBubbleResolution)
  kCredentialSelected = 0,
  kDismissed = 1,
  kIgnored = 2,
  kMaxValue = kIgnored,
  // LINT.ThenChange(//tools/metrics/histograms/metadata/webauthn/enums.xml:AmbientBubbleResolution)
};

// Enum for the WebAuthentication.Ambient.CredentialSelectionMethod histogram.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class AmbientCredentialSelectionMethod {
  // LINT.IfChange(AmbientCredentialSelectionMethod)
  kSignInButtonClicked = 0,
  kExpandedDefaultCredentialClicked = 1,
  kExpandedNonDefaultCredentialClicked = 2,
  kMaxValue = kExpandedNonDefaultCredentialClicked,
  // LINT.ThenChange(//tools/metrics/histograms/metadata/webauthn/enums.xml:AmbientCredentialSelectionMethod)
};

// Enum for the WebAuthentication.Ambient.ChipOnlyOutcome histogram.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class AmbientChipOnlyOutcome {
  // LINT.IfChange(AmbientChipOnlyOutcome)
  kBubbleActivated = 0,
  kIgnored = 1,
  kMaxValue = kIgnored,
  // LINT.ThenChange(//tools/metrics/histograms/metadata/webauthn/enums.xml:AmbientChipOnlyOutcome)
};

// Enum for the WebAuthn.AmbientSignIn.DisplayOutcome UKM metric.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class AmbientUkmDisplayOutcome {
  // LINT.IfChange(AmbientUkmDisplayOutcome)
  kShown = 0,
  kNoCredentials = 1,
  kNotShownOther = 2,
  kMaxValue = kNotShownOther,
  // LINT.ThenChange(//tools/metrics/histograms/metadata/webauthn/enums.xml:AmbientUkmDisplayOutcome)
};

// Enum for the WebAuthn.AmbientSignIn.BubbleOutcome UKM metric.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class AmbientUkmBubbleOutcome {
  // LINT.IfChange(AmbientUkmBubbleOutcome)
  kDismissed = 0,
  kSelectedDefaultInUnexpandedBubble = 1,
  kSelectedInExpandedBubble = 2,
  kDestroyedWithoutInteraction = 3,
  kMaxValue = kDestroyedWithoutInteraction,
  // LINT.ThenChange(//tools/metrics/histograms/metadata/webauthn/enums.xml:AmbientUkmBubbleOutcome)
};

void RecordAmbientDisplayOutcome(ukm::SourceId source_id,
                                 AmbientDisplayOutcome outcome);
void RecordAmbientInitialUiMode(ukm::SourceId source_id,
                                AmbientInitialUiMode mode);
void RecordAmbientBubbleCredentialCount(size_t credential_count);
void RecordAmbientBubbleResolution(bool bubble_initially_shown,
                                   AmbientBubbleResolution resolution);
void RecordAmbientCredentialSelectionMethod(
    AmbientCredentialSelectionMethod method);
void RecordAmbientChipOnlyOutcome(ukm::SourceId source_id,
                                  AmbientChipOnlyOutcome outcome);
void RecordAmbientUkmBubbleOutcome(ukm::SourceId source_id,
                                   AmbientUkmBubbleOutcome outcome);

}  // namespace webauthn::metrics

void ReportConditionalUiPasskeyCount(int passkey_count);
void RecordGPMMakeCredentialEvent(
    webauthn::metrics::GPMMakeCredentialEvents event);
void RecordGPMGetAssertionEvent(webauthn::metrics::GPMGetAssertionEvents event);
void RecordOnboardingEvent(webauthn::metrics::OnboardingEvents event);

#endif  // CHROME_BROWSER_WEBAUTHN_WEBAUTHN_METRICS_UTIL_H_
