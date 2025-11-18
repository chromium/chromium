// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/connectors_manager_base.h"

#include "components/enterprise/connectors/core/connectors_prefs.h"

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

std::optional<AnalysisSettings> ConnectorsManagerBase::GetAnalysisSettings(
    const GURL& url,
    AnalysisConnector connector) {
  if (!IsAnalysisConnectorEnabled(connector)) {
    return std::nullopt;
  }

  if (analysis_connector_settings_.count(connector) == 0) {
    CacheAnalysisConnectorPolicy(connector);
  }

  // If the connector is still not in memory, it means the pref is set to an
  // empty list or that it is not a list.
  if (analysis_connector_settings_.count(connector) == 0) {
    return std::nullopt;
  }

  // While multiple services can be set by the connector policies, only the
  // first one is considered for now.
  return analysis_connector_settings_[connector][0]->GetAnalysisSettings(
      url, GetDataRegion(connector));
}

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

void ConnectorsManagerBase::OnPrefChanged() {
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

  const base::Value::List& policy_value = prefs()->GetList(pref);
  for (const base::Value& service_settings : policy_value) {
    reporting_connector_settings_.emplace_back(service_settings,
                                               *service_provider_config_);
  }
}

void ConnectorsManagerBase::StartObservingPrefs(PrefService* pref_service) {
  pref_change_registrar_.Init(pref_service);
  StartObservingPref();
}

void ConnectorsManagerBase::StartObservingPref() {
  const char* pref = kOnSecurityEventPref;
  DCHECK(pref);
  if (!pref_change_registrar_.IsObserved(pref)) {
    pref_change_registrar_.Add(
        pref, base::BindRepeating(&ConnectorsManagerBase::OnPrefChanged,
                                  base::Unretained(this)));
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
