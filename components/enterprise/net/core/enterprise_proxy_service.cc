// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/net/core/enterprise_proxy_service.h"

#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "components/enterprise/net/core/prefs.h"
#include "components/prefs/pref_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace enterprise_net {

// The nested fetcher class that encapsulates the logic to fetch the
// ProvisioningDomainProxyConfig from the corresponding web server endpoint.
class EnterpriseProxyService::ProvisioningDomainFetcher {};

EnterpriseProxyService::ProxyProvisioningDomain::ProxyProvisioningDomain() =
    default;
EnterpriseProxyService::ProxyProvisioningDomain::ProxyProvisioningDomain(
    ProxyProvisioningDomain&&) noexcept = default;
EnterpriseProxyService::ProxyProvisioningDomain&
EnterpriseProxyService::ProxyProvisioningDomain::operator=(
    ProxyProvisioningDomain&&) noexcept = default;
EnterpriseProxyService::ProxyProvisioningDomain::~ProxyProvisioningDomain() =
    default;

EnterpriseProxyService::EnterpriseProxyService(
    PrefService* pref_service,
    signin::IdentityManager* identity_manager,
    GetURLLoaderFactoryCallback url_loader_factory_callback,
    enterprise::ProfileIdService* profile_id_service)
    : pref_service_(pref_service),
      identity_manager_(identity_manager),
      url_loader_factory_callback_(std::move(url_loader_factory_callback)),
      profile_id_service_(profile_id_service) {
  pref_change_registrar_.Init(pref_service_);
  pref_change_registrar_.Add(
      kProxyProvisioningDomains,
      base::BindRepeating(&EnterpriseProxyService::OnPolicyPrefChanged,
                          base::Unretained(this)));
  // Initial parsing
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
  return {};
}

bool EnterpriseProxyService::IsFetchInProgress() const {
  return in_progress_fetches_ > 0;
}

void EnterpriseProxyService::Shutdown() {
  pref_change_registrar_.RemoveAll();
  observers_.Clear();
}

void EnterpriseProxyService::OnPolicyPrefChanged() {
  const base::ListValue& policy_domains =
      pref_service_->GetList(kProxyProvisioningDomains);
  RebuildProvisioningDomains(policy_domains);
}

void EnterpriseProxyService::OnFetchComplete(
    EnterpriseProxyService::ProvisioningDomainFetcher* fetcher,
    std::optional<ProvisioningDomainProxyConfig> parsed_config) {}

void EnterpriseProxyService::NotifyAndUpdateCachedConfigs() {}

void EnterpriseProxyService::RebuildProvisioningDomains(
    const base::ListValue& policy_domains) {}

}  // namespace enterprise_net
