// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/unified_consent/unified_consent_metrics.h"

#include "base/metrics/histogram_macros.h"
#include "components/prefs/pref_service.h"

namespace {

// Histogram name for the consent bump action.
const char kConsentBumpActionMetricName[] = "UnifiedConsent.ConsentBump.Action";

// Histogram recorded at startup to log which Google services are enabled.
const char kSyncAndGoogleServicesSettingsHistogram[] =
    "UnifiedConsent.SyncAndGoogleServicesSettings";

}  // namespace

namespace unified_consent {

namespace metrics {

void RecordConsentBumpMetric(UnifiedConsentBumpAction action) {
  UMA_HISTOGRAM_ENUMERATION(
      kConsentBumpActionMetricName, action,
      UnifiedConsentBumpAction::kUnifiedConsentBumpActionMoreOptionsMax);
}

void RecordConsentBumpEligibility(bool eligible) {
  UMA_HISTOGRAM_BOOLEAN("UnifiedConsent.ConsentBump.EligibleAtStartup",
                        eligible);
}

void RecordUnifiedConsentRevoked(UnifiedConsentRevokeReason reason) {
  UMA_HISTOGRAM_ENUMERATION("UnifiedConsent.RevokeReason", reason);
}

void RecordSettingsHistogramSample(SettingsHistogramValue value) {
  UMA_HISTOGRAM_ENUMERATION(kSyncAndGoogleServicesSettingsHistogram, value);
}

bool RecordSettingsHistogramFromPref(const char* pref_name,
                                     PrefService* pref_service,
                                     SettingsHistogramValue value) {
  if (!pref_service->GetBoolean(pref_name))
    return false;
  RecordSettingsHistogramSample(value);
  return true;
}

bool RecordSettingsHistogramFromService(
    UnifiedConsentServiceClient* client,
    UnifiedConsentServiceClient::Service service,
    SettingsHistogramValue value) {
  if (client->GetServiceState(service) !=
      UnifiedConsentServiceClient::ServiceState::kEnabled) {
    return false;
  }

  RecordSettingsHistogramSample(value);
  return true;
}

}  // namespace metrics

}  // namespace unified_consent
