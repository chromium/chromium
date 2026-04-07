// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/test_connectors_service.h"

#include "components/enterprise/connectors/core/analysis_service_settings_base.h"
#include "components/enterprise/connectors/core/connectors_manager_base.h"
#include "components/enterprise/connectors/core/connectors_prefs.h"
#include "components/enterprise/connectors/core/service_provider_config.h"

namespace enterprise_connectors {

namespace {

class AnalysisServiceSettings : public AnalysisServiceSettingsBase {
 public:
  AnalysisServiceSettings(const base::Value& settings_value,
                          const ServiceProviderConfig& service_provider_config)
      : AnalysisServiceSettingsBase(settings_value, service_provider_config) {}
};

class ConnectorsManager : public ConnectorsManagerBase {
 public:
  using ConnectorsManagerBase::ConnectorsManagerBase;

  void CacheAnalysisConnectorPolicy(
      AnalysisConnector connector) const override {
    analysis_connector_settings_.erase(connector);

    // Connectors with non-existing policies should not reach this code.
    const char* pref = AnalysisConnectorPref(connector);
    DCHECK(pref);

    const base::ListValue& policy_value = prefs()->GetList(pref);
    for (const base::Value& service_settings : policy_value) {
      analysis_connector_settings_[connector].push_back(
          std::make_unique<AnalysisServiceSettings>(service_settings,
                                                    *service_provider_config_));
    }
  }

  DataRegion GetDataRegion(AnalysisConnector connector) const override {
    return DataRegion::NO_PREFERENCE;
  }
};

}  // namespace

TestConnectorsService::TestConnectorsService(TestingPrefServiceSimple* prefs)
    : ConnectorsServiceBase(
          std::make_unique<ConnectorsManager>(prefs,
                                              GetServiceProviderConfig())),
      prefs_(prefs) {
  RegisterProfilePrefs(prefs_->registry());
}

TestConnectorsService::~TestConnectorsService() = default;

void TestConnectorsService::set_machine_dm_token(const char* dm_token) {
  machine_dm_token_ =
      ConnectorsServiceBase::DmToken(dm_token, policy::POLICY_SCOPE_MACHINE);
}

void TestConnectorsService::set_profile_dm_token(const char* dm_token) {
  profile_dm_token_ =
      ConnectorsServiceBase::DmToken(dm_token, policy::POLICY_SCOPE_USER);
}

void TestConnectorsService::set_connectors_enabled(bool enabled) {
  connectors_enabled_ = enabled;
}

std::optional<ConnectorsServiceBase::DmToken> TestConnectorsService::GetDmToken(
    const char* scope_pref) const {
  switch (prefs_->GetInteger(scope_pref)) {
    case policy::POLICY_SCOPE_MACHINE:
      return machine_dm_token_;
    case policy::POLICY_SCOPE_USER:
      return profile_dm_token_;
    default:
      NOTREACHED();
  }
}

std::optional<std::string> TestConnectorsService::GetBrowserDmToken() const {
  if (machine_dm_token_.has_value()) {
    return machine_dm_token_.value().value;
  }
  return std::nullopt;
}

std::unique_ptr<ClientMetadata> TestConnectorsService::BuildClientMetadata(
    bool is_cloud) {
  return nullptr;
}

bool TestConnectorsService::ConnectorsEnabled() const {
  return connectors_enabled_;
}

PrefService* TestConnectorsService::GetPrefs() {
  return prefs_;
}
const PrefService* TestConnectorsService::GetPrefs() const {
  return prefs_;
}

policy::CloudPolicyManager*
TestConnectorsService::GetManagedUserCloudPolicyManager() const {
  NOTREACHED();
}

}  // namespace enterprise_connectors
