// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CONNECTORS_CORE_NETWORK_REQUEST_SERVICE_SETTINGS_H_
#define COMPONENTS_ENTERPRISE_CONNECTORS_CORE_NETWORK_REQUEST_SERVICE_SETTINGS_H_

#include <memory>
#include <optional>

#include "base/values.h"
#include "components/enterprise/connectors/core/analysis_service_settings_base.h"
#include "components/enterprise/connectors/core/analysis_settings.h"
#include "components/enterprise/connectors/core/common.h"
#include "components/enterprise/connectors/core/service_provider_config.h"
#include "url/gurl.h"

namespace enterprise_connectors {

// Settings for the "OnNetworkRequestEnterpriseConnector" policy.
class NetworkRequestServiceSettings : public AnalysisServiceSettingsBase {
 public:
  NetworkRequestServiceSettings(
      const base::Value& settings_value,
      const ServiceProviderConfig& service_provider_config);
  NetworkRequestServiceSettings(const NetworkRequestServiceSettings&) = delete;
  NetworkRequestServiceSettings(NetworkRequestServiceSettings&&);
  NetworkRequestServiceSettings& operator=(
      const NetworkRequestServiceSettings&) = delete;
  NetworkRequestServiceSettings& operator=(NetworkRequestServiceSettings&&);
  ~NetworkRequestServiceSettings() override;

  // AnalysisServiceSettingsBase:
  std::optional<AnalysisSettings> GetAnalysisSettings(
      const GURL& url,
      DataRegion data_region) const override;
  std::optional<AnalysisSettings> GetNetworkRequestAnalysisSettings(
      const GURL& tab_url,
      const GURL& request_url,
      DataRegion data_region) const override;

 private:
  std::vector<std::string> audit_request_domains_;
  std::vector<std::string> audit_tab_domains_;
};

}  // namespace enterprise_connectors

#endif  // COMPONENTS_ENTERPRISE_CONNECTORS_CORE_NETWORK_REQUEST_SERVICE_SETTINGS_H_
