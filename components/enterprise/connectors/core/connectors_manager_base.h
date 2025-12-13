// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CONNECTORS_MANAGER_BASE_H_
#define COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CONNECTORS_MANAGER_BASE_H_

#include <map>
#include <memory>
#include <optional>
#include <vector>

#include "components/enterprise/connectors/core/analysis_service_settings_base.h"
#include "components/enterprise/connectors/core/reporting_service_settings.h"
#include "components/enterprise/connectors/core/service_provider_config.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"

namespace enterprise_connectors {

class AnalysisServiceSettingsBase;

// Base class that manages access to Reporting Connector policies for the given
// preferences. The class is responsible for caching the Connector policies,
// validates them against approved service providers and provides a simple
// interface to them.
class ConnectorsManagerBase {
 public:
  using AnalysisConnectorsSettings =
      std::map<AnalysisConnector,
               std::vector<std::unique_ptr<AnalysisServiceSettingsBase>>>;

  ConnectorsManagerBase(PrefService* pref_service,
                        const ServiceProviderConfig* config,
                        bool observe_prefs = true);

  virtual ~ConnectorsManagerBase();

  // Checks if the corresponding connector is enabled.
  bool IsReportingConnectorEnabled() const;

  // Checks if the corresponding connector is enabled.
  bool IsAnalysisConnectorEnabled(AnalysisConnector connector) const;

  bool DelayUntilVerdict(AnalysisConnector connector);

  bool GetBypassJustificationRequired(AnalysisConnector connector,
                                      const std::string& tag);

  // Validates which settings should be applied to an analysis connector event
  // against cached policies. This function will prioritize new connector
  // policies over legacy ones if they are set.
  std::optional<AnalysisSettings> GetAnalysisSettings(
      const GURL& url,
      AnalysisConnector connector);

  // Validates which settings should be applied to a reporting event
  // against cached policies. Cache the policy value the first time this is
  // called for every different connector.
  std::optional<ReportingSettings> GetReportingSettings();

  std::optional<std::u16string> GetCustomMessage(AnalysisConnector connector,
                                                 const std::string& tag);

  std::optional<GURL> GetLearnMoreUrl(AnalysisConnector connector,
                                      const std::string& tag);

  std::vector<std::string> GetReportingServiceProviderNames();

  std::vector<std::string> GetAnalysisServiceProviderNames(
      AnalysisConnector connector);

  std::vector<const AnalysisConfig*> GetAnalysisServiceConfigs(
      AnalysisConnector connector);

  void SetTelemetryObserverCallback(base::RepeatingCallback<void()> callback);

  // Public testing functions.
  const std::vector<ReportingServiceSettings>&
  GetReportingConnectorsSettingsForTesting() const;

  const AnalysisConnectorsSettings& GetAnalysisConnectorsSettingsForTesting()
      const;

  const base::RepeatingCallback<void()> GetTelemetryObserverCallbackForTesting()
      const;

 protected:
  // Read and cache the policy corresponding to |connector|.
  virtual void CacheAnalysisConnectorPolicy(
      AnalysisConnector connector) const = 0;

  virtual DataRegion GetDataRegion(AnalysisConnector connector) const = 0;

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

  // Cached values of the connector policies. Updated when a connector is first
  // used or when a policy is updated.
  //
  // This member is `mutable` to enable lazy initialization of the cache within
  // `const` member functions (e.g. `IsAnalysisConnectorEnabled`). This allows
  // methods that are logically read-only to physically modify the internal
  // cache for efficiency without violating const-correctness from the caller's
  // perspective.
  mutable AnalysisConnectorsSettings analysis_connector_settings_;

 private:
  // Read and cache the policy corresponding to the reporting connector.
  void CacheReportingConnectorPolicy();

  // Re-cache reporting connector policy.
  void OnPrefChanged();
};

}  // namespace enterprise_connectors
#endif  // COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CONNECTORS_MANAGER_BASE_H_
