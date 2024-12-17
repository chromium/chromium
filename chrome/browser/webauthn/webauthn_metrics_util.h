// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_WEBAUTHN_METRICS_UTIL_H_
#define CHROME_BROWSER_WEBAUTHN_WEBAUTHN_METRICS_UTIL_H_

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
}  // namespace webauthn::metrics

void ReportConditionalUiPasskeyCount(int passkey_count);
void RecordGPMMakeCredentialEvent(
    webauthn::metrics::GPMMakeCredentialEvents event);
void RecordGPMGetAssertionEvent(webauthn::metrics::GPMGetAssertionEvents event);
void RecordOnboardingEvent(webauthn::metrics::OnboardingEvents event);

#endif  // CHROME_BROWSER_WEBAUTHN_WEBAUTHN_METRICS_UTIL_H_
