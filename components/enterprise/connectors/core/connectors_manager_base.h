// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CONNECTORS_MANAGER_BASE_H_
#define COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CONNECTORS_MANAGER_BASE_H_

#include <optional>

#include "components/enterprise/connectors/core/reporting_service_settings.h"
#include "components/enterprise/connectors/core/service_provider_config.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"

namespace enterprise_connectors {

// Base class that manages access to Reporting Connector policies for the given
// preferences. The class is responsible for caching the Connector policies,
// validates them against approved service providers and provides a simple
// interface to them.
class ConnectorsManagerBase {
 public:
  ConnectorsManagerBase(PrefService* pref_service,
                        const ServiceProviderConfig* config,
                        bool observe_prefs = true);

  virtual ~ConnectorsManagerBase();

  // Validates which settings should be applied to a reporting event
  // against cached policies. Cache the policy value the first time this is
  // called for every different connector.
  std::optional<ReportingSettings> GetReportingSettings();

  // Checks if the corresponding connector is enabled.
  bool IsReportingConnectorEnabled() const;

  std::vector<std::string> GetReportingServiceProviderNames();

  // Public testing function.
  const std::vector<ReportingServiceSettings>&
  GetReportingConnectorsSettingsForTesting() const;

 protected:
  // Sets up |pref_change_registrar_|. Used by the constructor and
  // SetUpForTesting.
  virtual void StartObservingPrefs(PrefService* pref_service);
  void StartObservingPref();

  const PrefService* prefs() const { return pref_change_registrar_.prefs(); }

  // Cached values of available service providers. This information validates
  // the Connector policies have a valid provider.
  raw_ptr<const ServiceProviderConfig> service_provider_config_;

  std::vector<ReportingServiceSettings> reporting_connector_settings_;

  // Used to track changes of connector policies and propagate them in
  // |connector_settings_|.
  PrefChangeRegistrar pref_change_registrar_;

  // Used to report changes of reporting connector policy.
  base::RepeatingCallback<void()> telemetry_observer_callback_;

 private:
  // Read and cache the policy corresponding to the reporting connector.
  void CacheReportingConnectorPolicy();

  // Re-cache reporting connector policy.
  void OnPrefChanged();
};

}  // namespace enterprise_connectors
#endif  // COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CONNECTORS_MANAGER_BASE_H_
