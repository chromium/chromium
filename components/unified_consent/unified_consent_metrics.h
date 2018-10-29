// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UNIFIED_CONSENT_UNIFIED_CONSENT_METRICS_H_
#define COMPONENTS_UNIFIED_CONSENT_UNIFIED_CONSENT_METRICS_H_

#include "components/unified_consent/unified_consent_service_client.h"

namespace unified_consent {

namespace metrics {

// Histogram enum: UnifiedConsentBumpAction.
enum class UnifiedConsentBumpAction : int {
  kUnifiedConsentBumpActionDefaultOptIn = 0,
  kUnifiedConsentBumpActionMoreOptionsOptIn,
  kUnifiedConsentBumpActionMoreOptionsReviewSettings,
  kUnifiedConsentBumpActionMoreOptionsNoChanges,
  kUnifiedConsentBumpActionMoreOptionsMax,
};

// Histogram enum: UnifiedConsentRevokeReason.
enum class UnifiedConsentRevokeReason : int {
  kUserSignedOut = 0,
  kServiceWasDisabled,
  kCustomPassphrase,
  kUserDisabledSettingsToggle,
  kMaxValue = kUserDisabledSettingsToggle
};

// Used in histograms. Do not change existing values, append new values at the
// end.
enum class ConsentBumpSuppressReason {
  // There is no suppress reason. The consent bump was shown.
  kNone,
  // The user wasn't signed in during the migration.
  kNotSignedIn,
  // The user wasn't syncing everything during the migration.
  kSyncEverythingOff,
  // The user didn't have all on-by-default privacy settings enabled during
  // migration.
  kPrivacySettingOff,
  kSettingsOptIn,
  // The user was eligible for seeing the consent bump, but then signed out.
  kUserSignedOut,
  kSyncPaused,
  // The user was eligible for seeing the consent bump, but turned an individual
  // sync data type off.
  kUserTurnedSyncDatatypeOff,
  // The user was eligible for seeing the consent bump, but turned an
  // on-by-default privacy setting off.
  kUserTurnedPrivacySettingOff,
  // The user has a custom passphrase tied to their sync account.
  kCustomPassphrase,

  kMaxValue = kCustomPassphrase
};

// Google services that can be toggled in user settings.
// Used in histograms. Do not change existing values, append new values at the
// end.
enum class SettingsHistogramValue {
  kNone = 0,
  kUnifiedConsentGiven = 1,
  kUserEvents = 2,
  kUrlKeyedAnonymizedDataCollection = 3,
  kSafeBrowsingExtendedReporting = 4,
  kSpellCheck = 5,

  kMaxValue = kSpellCheck
};

// Records histogram action for the unified consent bump.
void RecordConsentBumpMetric(UnifiedConsentBumpAction action);

// Records whether the user is eligible for the consent bump. This method should
// be called at startup.
void RecordConsentBumpEligibility(bool eligible);

// Records the reason why the unified consent was revoked.
void RecordUnifiedConsentRevoked(UnifiedConsentRevokeReason reason);

// Records a sample in the kSyncAndGoogleServicesSettingsHistogram. Wrapped in a
// function to avoid code size issues caused by histogram macros.
void RecordSettingsHistogramSample(SettingsHistogramValue value);

// Checks if a pref is enabled and if so, records a sample in the
// kSyncAndGoogleServicesSettingsHistogram. Returns true if a sample was
// recorded.
bool RecordSettingsHistogramFromPref(const char* pref_name,
                                     PrefService* pref_service,
                                     SettingsHistogramValue value);

// Checks if a service is enabled and if so, records a sample in the
// kSyncAndGoogleServicesSettingsHistogram. Returns true if a sample was
// recorded.
bool RecordSettingsHistogramFromService(
    UnifiedConsentServiceClient* client,
    UnifiedConsentServiceClient::Service service,
    SettingsHistogramValue value);

}  // namespace metrics

}  // namespace unified_consent

#endif  // COMPONENTS_UNIFIED_CONSENT_UNIFIED_CONSENT_METRICS_H_
