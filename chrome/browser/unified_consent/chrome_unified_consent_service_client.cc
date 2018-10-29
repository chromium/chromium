// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/unified_consent/chrome_unified_consent_service_client.h"

#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/metrics/metrics_reporting_state.h"
#include "chrome/browser/net/prediction_options.h"
#include "chrome/common/pref_names.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/common/safe_browsing_prefs.h"
#include "components/spellcheck/browser/pref_names.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/metrics/uma_utils.h"
#endif

ChromeUnifiedConsentServiceClient::ChromeUnifiedConsentServiceClient(
    PrefService* pref_service)
    : pref_service_(pref_service) {
  DCHECK(pref_service_);
  ObserveServicePrefChange(Service::kAlternateErrorPages,
                           prefs::kAlternateErrorPagesEnabled, pref_service_);
  ObserveServicePrefChange(Service::kMetricsReporting,
                           metrics::prefs::kMetricsReportingEnabled,
                           g_browser_process->local_state());
  ObserveServicePrefChange(Service::kNetworkPrediction,
                           prefs::kNetworkPredictionOptions, pref_service_);
  ObserveServicePrefChange(Service::kSafeBrowsing, prefs::kSafeBrowsingEnabled,
                           pref_service_);
  ObserveServicePrefChange(Service::kSafeBrowsingExtendedReporting,
                           prefs::kSafeBrowsingScoutReportingEnabled,
                           pref_service_);
  ObserveServicePrefChange(Service::kSearchSuggest,
                           prefs::kSearchSuggestEnabled, pref_service_);
  ObserveServicePrefChange(Service::kSpellCheck,
                           spellcheck::prefs::kSpellCheckUseSpellingService,
                           pref_service_);
}

ChromeUnifiedConsentServiceClient::~ChromeUnifiedConsentServiceClient() {}

ChromeUnifiedConsentServiceClient::ServiceState
ChromeUnifiedConsentServiceClient::GetServiceState(Service service) {
  bool enabled;
  switch (service) {
    case Service::kAlternateErrorPages:
      enabled = pref_service_->GetBoolean(prefs::kAlternateErrorPagesEnabled);
      break;
    case Service::kMetricsReporting:
      if (!g_browser_process->metrics_service())
        return ServiceState::kNotSupported;
      // Uploads are disabled for non-official builds, but UnifiedConsentService
      // only cares whether the user has manually disabled metrics reporting.
      enabled = g_browser_process->local_state()->GetBoolean(
          metrics::prefs::kMetricsReportingEnabled);
      break;
    case Service::kNetworkPrediction:
      enabled = pref_service_->GetInteger(prefs::kNetworkPredictionOptions) ==
                chrome_browser_net::NETWORK_PREDICTION_DEFAULT;
      break;
    case Service::kSafeBrowsing:
      enabled = pref_service_->GetBoolean(prefs::kSafeBrowsingEnabled);
      break;
    case Service::kSafeBrowsingExtendedReporting:
      enabled = safe_browsing::IsExtendedReportingEnabled(*pref_service_);
      break;
    case Service::kSearchSuggest:
      enabled = pref_service_->GetBoolean(prefs::kSearchSuggestEnabled);
      break;
    case Service::kSpellCheck:
      enabled = pref_service_->GetBoolean(
          spellcheck::prefs::kSpellCheckUseSpellingService);
      break;
  }
  return enabled ? ServiceState::kEnabled : ServiceState::kDisabled;
}

void ChromeUnifiedConsentServiceClient::SetServiceEnabled(Service service,
                                                          bool enabled) {
  switch (service) {
    case Service::kAlternateErrorPages:
      pref_service_->SetBoolean(prefs::kAlternateErrorPagesEnabled, enabled);
      break;
    case Service::kMetricsReporting:
#if defined(OS_ANDROID)
      // TODO(https://crbug.com/880936): Move inside ChangeMetricsReportingState
      chrome::android::SetUsageAndCrashReporting(enabled);
#else
      ChangeMetricsReportingState(enabled);
#endif
      break;
    case Service::kNetworkPrediction:
      pref_service_->SetInteger(
          prefs::kNetworkPredictionOptions,
          enabled ? chrome_browser_net::NETWORK_PREDICTION_DEFAULT
                  : chrome_browser_net::NETWORK_PREDICTION_NEVER);
      break;
    case Service::kSafeBrowsing:
      pref_service_->SetBoolean(prefs::kSafeBrowsingEnabled, enabled);
      break;
    case Service::kSafeBrowsingExtendedReporting:
      safe_browsing::SetExtendedReportingPref(pref_service_, enabled);
      break;
    case Service::kSearchSuggest:
      pref_service_->SetBoolean(prefs::kSearchSuggestEnabled, enabled);
      break;
    case Service::kSpellCheck:
      pref_service_->SetBoolean(
          spellcheck::prefs::kSpellCheckUseSpellingService, enabled);
      break;
  }
}
