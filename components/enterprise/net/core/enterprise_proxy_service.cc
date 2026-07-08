// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/net/core/enterprise_proxy_service.h"

#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "components/enterprise/net/core/prefs.h"
#include "components/prefs/pref_service.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace enterprise_net {

namespace {

// Network traffic annotation for Provisioning Domain configuration fetches.
[[maybe_unused]] const net::NetworkTrafficAnnotationTag kPvdTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("enterprise_proxy_pvd_fetch", R"(
        semantics {
          sender: "Enterprise Proxy Provisioning Domains Service"
          description:
            "Fetches dynamic proxy configuration and routing rules from a "
            "Provisioning Domain (PvD) server configured by the "
            "enterprise administrator."
          trigger:
            "When new network configurations are needed from Provisioning "
            "Domains, including but not limited to policy value, network "
            "change, signed in account change and route expiration."
          data:
            "May include OAuth access token and custom headers (such as "
            "profile ID, preferred language) for Provisioning Domains."
          destination: OTHER
          destination_other:
            "The endpoint URL is constructed from the domains specified in the "
            "ProxyProvisioningDomains policy."
          internal {
            contacts {
              email: "chrome-enterprise-networking-core@google.com"
            }
            contacts {
              owners: "//components/enterprise/OWNERS"
            }
          }
          user_data {
            type: ACCESS_TOKEN
            type: PROFILE_DATA
          }
          last_reviewed: "2026-07-07"
        }
        policy {
          cookies_allowed: NO
          setting:
            "This feature cannot be disabled by users. It is only active when "
            "configured by an enterprise administrator via the "
            "ProxyProvisioningDomains policy."
          chrome_policy {
            ProxyProvisioningDomains {
              ProxyProvisioningDomains: "[]"
            }
          }
        })");

}  // namespace

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
