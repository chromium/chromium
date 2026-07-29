// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/net/core/enterprise_proxy_service.h"

#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "components/enterprise/browser/identifiers/profile_id_service.h"
#include "components/enterprise/net/core/enterprise_network_auth_service.h"
#include "components/enterprise/net/core/features.h"
#include "components/enterprise/net/core/prefs.h"
#include "components/enterprise/net/core/proxy_provisioning_domain_manager.h"
#include "components/enterprise/net/core/utils.h"
#include "components/prefs/pref_service.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace enterprise_net {
EnterpriseProxyService::EnterpriseProxyService(
    PrefService* pref_service,
    EnterpriseNetworkAuthService* auth_service,
    GetURLLoaderFactoryCallback url_loader_factory_callback,
    enterprise::ProfileIdService* profile_id_service)
    : pref_service_(pref_service),
      auth_service_(auth_service),
      url_loader_factory_callback_(std::move(url_loader_factory_callback)),
      profile_id_service_(profile_id_service) {
  CHECK(pref_service_);
  CHECK(auth_service_);
  CHECK(url_loader_factory_callback_);
  pref_change_registrar_.Init(pref_service_);
  pref_change_registrar_.Add(
      kProxyProvisioningDomains,
      base::BindRepeating(&EnterpriseProxyService::OnPolicyPrefChanged,
                          base::Unretained(this)));
  OnPolicyPrefChanged();
}

EnterpriseProxyService::~EnterpriseProxyService() = default;

void EnterpriseProxyService::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void EnterpriseProxyService::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

std::vector<ProvisioningDomainProxyConfig>
EnterpriseProxyService::GetProvisioningDomainConfigs() const {
  std::vector<ProvisioningDomainProxyConfig> configs;
  configs.reserve(provisioning_domain_managers_.size());
  for (const auto& manager : provisioning_domain_managers_) {
    configs.push_back(manager->fetched_config());
  }
  return configs;
}

bool EnterpriseProxyService::IsRefreshInProgress() const {
  return !refreshing_managers_.empty();
}

std::optional<ProvisioningDomainProxyConfig::ProxyEndpoint>
EnterpriseProxyService::FindMatchingProxyEndpoint(
    const GURL& destination_url,
    const net::ProxyChain& proxy_chain) const {
  for (const auto& domain_manager : provisioning_domain_managers_) {
    const auto* endpoint = enterprise_net::FindMatchingProxyEndpoint(
        domain_manager->fetched_config(), destination_url, proxy_chain);
    if (endpoint) {
      return *endpoint;
    }
  }
  return std::nullopt;
}

net::ProxyConfig::DynamicRoutingConfig
EnterpriseProxyService::GetDynamicRoutingConfig() const {
  net::ProxyConfig::DynamicRoutingConfig merged_config;
  for (const auto& domain_manager : provisioning_domain_managers_) {
    net::ProxyConfig::DynamicRoutingConfig domain_config =
        domain_manager->fetched_config().ToDynamicRoutingConfig();
    merged_config.routing_rules.insert(
        merged_config.routing_rules.end(),
        std::make_move_iterator(domain_config.routing_rules.begin()),
        std::make_move_iterator(domain_config.routing_rules.end()));
  }
  return merged_config;
}

void EnterpriseProxyService::Shutdown() {
  pref_change_registrar_.RemoveAll();
  for (auto& manager : provisioning_domain_managers_) {
    manager->RemoveObserver(this);
  }
  provisioning_domain_managers_.clear();
  refreshing_managers_.clear();
  observers_.Clear();
}

void EnterpriseProxyService::OnProvisioningDomainStateChanged(
    ProxyProvisioningDomainManager* domain_manager) {
  const size_t old_count = refreshing_managers_.size();

  if (domain_manager->is_refresh_in_progress()) {
    refreshing_managers_.insert(domain_manager);
  } else {
    refreshing_managers_.erase(domain_manager);
  }

  const size_t new_count = refreshing_managers_.size();

  if (old_count == 0 && new_count > 0) {
    observers_.Notify(&Observer::OnDynamicProxyConfigsUpdateInProgress);
    return;
  }

  if (old_count > 0 && new_count == 0) {
    const std::vector<ProvisioningDomainProxyConfig> configs =
        GetProvisioningDomainConfigs();
    observers_.Notify(&Observer::OnAllDynamicProxyConfigsResolved, configs);
  }
}

void EnterpriseProxyService::ForceRefreshAllConfigs() {
  for (auto& manager : provisioning_domain_managers_) {
    manager->ForceRefresh();
  }
}

base::DictValue EnterpriseProxyService::GetDebugInfo() const {
  base::DictValue dict;
  dict.Set("is_refresh_in_progress", IsRefreshInProgress());
  dict.Set("refreshing_configs_count",
           static_cast<int>(refreshing_managers_.size()));

  base::ListValue domains_list;
  for (const auto& manager : provisioning_domain_managers_) {
    domains_list.Append(manager->GetDebugInfo());
  }
  dict.Set("domains", std::move(domains_list));
  return dict;
}

void EnterpriseProxyService::OnPolicyPrefChanged() {
  const base::ListValue& policy_domains =
      pref_service_->GetList(kProxyProvisioningDomains);

  RecreateProvisioningDomainManagers(policy_domains);
}

void EnterpriseProxyService::RecreateProvisioningDomainManagers(
    const base::ListValue& policy_domains) {
  provisioning_domain_managers_.clear();
  refreshing_managers_.clear();

  for (const auto& domain_val : policy_domains) {
    auto manager = std::make_unique<ProxyProvisioningDomainManager>(
        domain_val, auth_service_, url_loader_factory_callback_);
    manager->AddObserver(this);
    OnProvisioningDomainStateChanged(manager.get());
    provisioning_domain_managers_.push_back(std::move(manager));
  }
}

}  // namespace enterprise_net
