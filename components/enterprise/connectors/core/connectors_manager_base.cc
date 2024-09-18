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

bool ConnectorsManagerBase::IsReportingConnectorEnabled(
    ReportingConnector connector) const {
  if (reporting_connector_settings_.count(connector) == 1) {
    return true;
  }

  const char* pref = kOnSecurityEventPref;
  return pref && prefs()->HasPrefPath(pref);
}

std::optional<ReportingSettings> ConnectorsManagerBase::GetReportingSettings(
    ReportingConnector connector) {
  if (!IsReportingConnectorEnabled(connector)) {
    return std::nullopt;
  }

  if (reporting_connector_settings_.count(connector) == 0) {
    CacheReportingConnectorPolicy(connector);
  }

  // If the connector is still not in memory, it means the pref is set to an
  // empty list or that it is not a list.
  if (reporting_connector_settings_.count(connector) == 0) {
    return std::nullopt;
  }

  // While multiple services can be set by the connector policies, only the
  // first one is considered for now.
  return reporting_connector_settings_[connector][0].GetReportingSettings();
}

void ConnectorsManagerBase::OnPrefChanged(ReportingConnector connector) {
  CacheReportingConnectorPolicy(connector);
  if (!telemetry_observer_callback_.is_null()) {
    telemetry_observer_callback_.Run();
  }
}

std::vector<std::string>
ConnectorsManagerBase::GetReportingServiceProviderNames(
    ReportingConnector connector) {
  if (!IsReportingConnectorEnabled(connector)) {
    return {};
  }

  if (reporting_connector_settings_.count(connector) == 0) {
    CacheReportingConnectorPolicy(connector);
  }

  if (reporting_connector_settings_.count(connector) &&
      !reporting_connector_settings_.at(connector).empty()) {
    // There can only be one provider right now, but the system is designed to
    // support multiples, so return a vector.
    return {reporting_connector_settings_.at(connector)
                .at(0)
                .service_provider_name()};
  }

  return {};
}

void ConnectorsManagerBase::CacheReportingConnectorPolicy(
    ReportingConnector connector) {
  reporting_connector_settings_.erase(connector);

  // Connectors with non-existing policies should not reach this code.
  const char* pref = kOnSecurityEventPref;
  DCHECK(pref);

  const base::Value::List& policy_value = prefs()->GetList(pref);
  for (const base::Value& service_settings : policy_value) {
    reporting_connector_settings_[connector].emplace_back(
        service_settings, *service_provider_config_);
  }
}

void ConnectorsManagerBase::StartObservingPrefs(PrefService* pref_service) {
  pref_change_registrar_.Init(pref_service);
  StartObservingPref(ReportingConnector::SECURITY_EVENT);
}

void ConnectorsManagerBase::StartObservingPref(ReportingConnector connector) {
  const char* pref = kOnSecurityEventPref;
  DCHECK(pref);
  if (!pref_change_registrar_.IsObserved(pref)) {
    pref_change_registrar_.Add(
        pref,
        base::BindRepeating(
            static_cast<void (ConnectorsManagerBase::*)(ReportingConnector)>(
                &ConnectorsManagerBase::OnPrefChanged),
            base::Unretained(this), connector));
  }
}

const ConnectorsManagerBase::ReportingConnectorsSettings&
ConnectorsManagerBase::GetReportingConnectorsSettingsForTesting() const {
  return reporting_connector_settings_;
}

}  // namespace enterprise_connectors
