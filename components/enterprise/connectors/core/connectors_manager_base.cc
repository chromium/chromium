// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/connectors_manager_base.h"

#include "base/feature_list.h"
#include "components/enterprise/connectors/core/connectors_prefs.h"
#include "components/enterprise/connectors/core/features.h"
#include "components/enterprise/connectors/core/network_request_service_settings.h"

namespace enterprise_connectors {

ConnectorsManagerBase::ConnectorsManagerBase(
    PrefService* pref_service,
    const ServiceProviderConfig* config,
    bool observe_prefs)
    : service_provider_config_(config) {
  if (observe_prefs) {
    StartObservingPrefs(pref_service);
  }
}

ConnectorsManagerBase::~ConnectorsManagerBase() = default;

bool ConnectorsManagerBase::CanGetAnalysisSettings(
    AnalysisConnector connector) {
  if (!IsAnalysisConnectorEnabled(connector)) {
    return false;
  }

  if (analysis_connector_settings_.count(connector) == 0) {
    CacheAnalysisConnectorPolicy(connector);
  }

  // If the connector is still not in memory, it means the pref is set to an
  // empty list or that it is not a list.
  return analysis_connector_settings_.count(connector) != 0;
}

std::optional<AnalysisSettings> ConnectorsManagerBase::GetAnalysisSettings(
    const GURL& url,
    AnalysisConnector connector) {
  if (!CanGetAnalysisSettings(connector)) {
    return std::nullopt;
  }

  // While multiple services can be set by the connector policies, only the
  // first one is considered for now.
  return analysis_connector_settings_[connector][0]->GetAnalysisSettings(
      url, GetDataRegion(connector));
}

#if !BUILDFLAG(IS_IOS)
std::optional<AnalysisSettings>
ConnectorsManagerBase::GetNetworkRequestAnalysisSettings(
    const GURL& tab_url,
    const GURL& request_url) {
  if (!base::FeatureList::IsEnabled(kEnableAuditOnlyNetworkRequestConnector) ||
      !CanGetAnalysisSettings(AnalysisConnector::NETWORK_REQUEST)) {
    return std::nullopt;
  }

  // Only `BlockUntilVerdict::kNoBlock` settings are currently supported,
  // so we just need to aggregate the tags from matching sub-settings in the
  // policy array. We skip entries that don't match this pattern as it likely
  // indicates a future version of the policy that supports blocking requests.
  std::optional<AnalysisSettings> final_settings;
  for (const auto& entry :
       analysis_connector_settings_[AnalysisConnector::NETWORK_REQUEST]) {
    auto sub_settings = entry->GetNetworkRequestAnalysisSettings(
        tab_url, request_url,
        GetDataRegion(AnalysisConnector::NETWORK_REQUEST));
    if (sub_settings &&
        sub_settings->block_until_verdict == BlockUntilVerdict::kNoBlock) {
      if (final_settings) {
        // Tags are currently the only thing that might vary from each entry in
        // the list, so it's the only `AnalysisSettings` field we need to merge.
        final_settings->tags.insert(sub_settings->tags.begin(),
                                    sub_settings->tags.end());
      } else {
        final_settings = std::move(sub_settings);
      }
    }
  }
  return final_settings;
}
#endif  // !BUILDFLAG(IS_IOS)

bool ConnectorsManagerBase::IsAnalysisConnectorEnabled(
    AnalysisConnector connector) const {
  if (analysis_connector_settings_.count(connector) == 0 &&
      prefs()->HasPrefPath(AnalysisConnectorPref(connector))) {
    CacheAnalysisConnectorPolicy(connector);
  }

  return analysis_connector_settings_.count(connector);
}

bool ConnectorsManagerBase::DelayUntilVerdict(AnalysisConnector connector) {
  if (IsAnalysisConnectorEnabled(connector)) {
    if (analysis_connector_settings_.count(connector) == 0) {
      CacheAnalysisConnectorPolicy(connector);
    }

    if (analysis_connector_settings_.count(connector) &&
        !analysis_connector_settings_.at(connector).empty()) {
      return analysis_connector_settings_.at(connector)
          .at(0)
          ->ShouldBlockUntilVerdict();
    }
  }
  return false;
}

std::optional<std::u16string> ConnectorsManagerBase::GetCustomMessage(
    AnalysisConnector connector,
    const std::string& tag) {
  if (IsAnalysisConnectorEnabled(connector)) {
    if (analysis_connector_settings_.count(connector) == 0) {
      CacheAnalysisConnectorPolicy(connector);
    }

    if (analysis_connector_settings_.count(connector) &&
        !analysis_connector_settings_.at(connector).empty()) {
      return analysis_connector_settings_.at(connector).at(0)->GetCustomMessage(
          tag);
    }
  }
  return std::nullopt;
}

std::optional<GURL> ConnectorsManagerBase::GetLearnMoreUrl(
    AnalysisConnector connector,
    const std::string& tag) {
  if (IsAnalysisConnectorEnabled(connector)) {
    if (analysis_connector_settings_.count(connector) == 0) {
      CacheAnalysisConnectorPolicy(connector);
    }

    if (analysis_connector_settings_.count(connector) &&
        !analysis_connector_settings_.at(connector).empty()) {
      return analysis_connector_settings_.at(connector).at(0)->GetLearnMoreUrl(
          tag);
    }
  }
  return std::nullopt;
}

bool ConnectorsManagerBase::GetBypassJustificationRequired(
    AnalysisConnector connector,
    const std::string& tag) {
  if (IsAnalysisConnectorEnabled(connector)) {
    if (analysis_connector_settings_.count(connector) == 0) {
      CacheAnalysisConnectorPolicy(connector);
    }

    if (analysis_connector_settings_.count(connector) &&
        !analysis_connector_settings_.at(connector).empty()) {
      return analysis_connector_settings_.at(connector)
          .at(0)
          ->GetBypassJustificationRequired(tag);
    }
  }
  return false;
}

std::vector<std::string> ConnectorsManagerBase::GetAnalysisServiceProviderNames(
    AnalysisConnector connector) {
  if (IsAnalysisConnectorEnabled(connector)) {
    if (analysis_connector_settings_.count(connector) == 0) {
      CacheAnalysisConnectorPolicy(connector);
    }

    if (analysis_connector_settings_.count(connector) &&
        !analysis_connector_settings_.at(connector).empty()) {
      // There can only be one provider right now, but the system is designed to
      // support multiples, so return a vector.
      return {analysis_connector_settings_.at(connector)
                  .at(0)
                  ->service_provider_name()};
    }
  }

  return {};
}

std::vector<const AnalysisConfig*>
ConnectorsManagerBase::GetAnalysisServiceConfigs(AnalysisConnector connector) {
  if (IsAnalysisConnectorEnabled(connector)) {
    if (analysis_connector_settings_.count(connector) == 0) {
      CacheAnalysisConnectorPolicy(connector);
    }

    if (analysis_connector_settings_.count(connector) &&
        !analysis_connector_settings_.at(connector).empty()) {
      // There can only be one provider right now, but the system is designed to
      // support multiples, so return a vector.
      return {analysis_connector_settings_.at(connector)
                  .at(0)
                  ->GetAnalysisConfig()};
    }
  }

  return {};
}

bool ConnectorsManagerBase::IsReportingConnectorEnabled() const {
  if (!reporting_connector_settings_.empty()) {
    return true;
  }

  const char* pref = kOnSecurityEventPref;
  return pref && prefs()->HasPrefPath(pref);
}

std::optional<ReportingSettings> ConnectorsManagerBase::GetReportingSettings() {
  if (!IsReportingConnectorEnabled()) {
    return std::nullopt;
  }

  if (reporting_connector_settings_.empty()) {
    CacheReportingConnectorPolicy();
  }

  // If the connector is still not in memory, it means the pref is set to an
  // empty list or that it is not a list.
  if (reporting_connector_settings_.empty()) {
    return std::nullopt;
  }

  // While multiple services can be set by the connector policies, only the
  // first one is considered for now.
  return reporting_connector_settings_[0].GetReportingSettings();
}

void ConnectorsManagerBase::OnReportingPrefChanged() {
  CacheReportingConnectorPolicy();
  if (!telemetry_observer_callback_.is_null()) {
    telemetry_observer_callback_.Run();
  }
}

std::vector<std::string>
ConnectorsManagerBase::GetReportingServiceProviderNames() {
  if (!IsReportingConnectorEnabled()) {
    return {};
  }

  if (reporting_connector_settings_.empty()) {
    CacheReportingConnectorPolicy();
  }

  if (!reporting_connector_settings_.empty()) {
    // There can only be one provider right now, but the system is designed to
    // support multiples, so return a vector.
    return {reporting_connector_settings_.at(0).service_provider_name()};
  }

  return {};
}

void ConnectorsManagerBase::CacheReportingConnectorPolicy() {
  reporting_connector_settings_.clear();

  // Connectors with non-existing policies should not reach this code.
  const char* pref = kOnSecurityEventPref;
  DCHECK(pref);

  const base::ListValue& policy_value = prefs()->GetList(pref);
  for (const base::Value& service_settings : policy_value) {
    reporting_connector_settings_.emplace_back(service_settings,
                                               *service_provider_config_);
  }
}

std::unique_ptr<AnalysisServiceSettingsBase>
ConnectorsManagerBase::MakeAnalysisServiceSettings(
    const base::Value& settings_value,
    const ServiceProviderConfig& service_provider_config) const {
  return std::make_unique<AnalysisServiceSettingsBase>(settings_value,
                                                       service_provider_config);
}

void ConnectorsManagerBase::CacheAnalysisConnectorPolicy(
    AnalysisConnector connector) const {
  analysis_connector_settings_.erase(connector);

  // Connectors with non-existing policies should not reach this code.
  const char* pref = AnalysisConnectorPref(connector);
  DCHECK(pref);

  const base::ListValue& policy_value = prefs()->GetList(pref);
  for (const base::Value& service_settings : policy_value) {
    if (connector == AnalysisConnector::NETWORK_REQUEST) {
#if BUILDFLAG(IS_IOS)
      // The "OnNetworkRequestEnterpriseConnector" policy is not supported on
      // iOS so this code should not be reachable.
      NOTREACHED();
#else
      analysis_connector_settings_[connector].push_back(
          std::make_unique<NetworkRequestServiceSettings>(
              service_settings, *service_provider_config_));
#endif
    } else {
      analysis_connector_settings_[connector].push_back(
          MakeAnalysisServiceSettings(service_settings,
                                      *service_provider_config_));
    }
  }
}

void ConnectorsManagerBase::OnAnalysisPrefChanged(AnalysisConnector connector) {
  CacheAnalysisConnectorPolicy(connector);
}

void ConnectorsManagerBase::StartObservingPrefs(PrefService* pref_service) {
  pref_change_registrar_.Init(pref_service);
  StartObservingAnalysisPref(AnalysisConnector::FILE_DOWNLOADED);
#if !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)
  StartObservingAnalysisPref(AnalysisConnector::FILE_ATTACHED);
  StartObservingAnalysisPref(AnalysisConnector::BULK_DATA_ENTRY);
  StartObservingAnalysisPref(AnalysisConnector::PRINT);
  StartObservingAnalysisPref(AnalysisConnector::DATA_COPIED);
#endif

#if BUILDFLAG(IS_CHROMEOS)
  StartObservingAnalysisPref(AnalysisConnector::FILE_TRANSFER);
#endif

#if !BUILDFLAG(IS_IOS)
  if (base::FeatureList::IsEnabled(kEnableAuditOnlyNetworkRequestConnector)) {
    StartObservingAnalysisPref(AnalysisConnector::NETWORK_REQUEST);
  }
#endif

  StartObservingReportingPref();
}

void ConnectorsManagerBase::StartObservingReportingPref() {
  const char* pref = kOnSecurityEventPref;
  DCHECK(pref);
  if (!pref_change_registrar_.IsObserved(pref)) {
    pref_change_registrar_.Add(
        pref,
        base::BindRepeating(&ConnectorsManagerBase::OnReportingPrefChanged,
                            base::Unretained(this)));
  }
}

void ConnectorsManagerBase::StartObservingAnalysisPref(
    AnalysisConnector connector) {
  const char* pref = AnalysisConnectorPref(connector);
  DCHECK(pref);
  if (!pref_change_registrar_.IsObserved(pref)) {
    pref_change_registrar_.Add(
        pref, base::BindRepeating(&ConnectorsManagerBase::OnAnalysisPrefChanged,
                                  base::Unretained(this), connector));
  }
}

void ConnectorsManagerBase::SetTelemetryObserverCallback(
    base::RepeatingCallback<void()> callback) {
  telemetry_observer_callback_ = callback;
}

const std::vector<ReportingServiceSettings>&
ConnectorsManagerBase::GetReportingConnectorsSettingsForTesting() const {
  return reporting_connector_settings_;
}

const ConnectorsManagerBase::AnalysisConnectorsSettings&
ConnectorsManagerBase::GetAnalysisConnectorsSettingsForTesting() const {
  return analysis_connector_settings_;
}

const base::RepeatingCallback<void()>
ConnectorsManagerBase::GetTelemetryObserverCallbackForTesting() const {
  return telemetry_observer_callback_;
}

}  // namespace enterprise_connectors
