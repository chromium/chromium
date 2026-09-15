// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CONNECTORS_CORE_PROVISIONING_DOMAIN_REFRESH_HELPER_H_
#define COMPONENTS_ENTERPRISE_CONNECTORS_CORE_PROVISIONING_DOMAIN_REFRESH_HELPER_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "components/enterprise/buildflags/buildflags.h"
#include "components/enterprise/connectors/connectors_internals.mojom.h"

#if BUILDFLAG(ENTERPRISE_PROXY)
#include "components/enterprise/net/core/enterprise_proxy_service.h"
#endif  // BUILDFLAG(ENTERPRISE_PROXY)

namespace enterprise_connectors {

#if BUILDFLAG(ENTERPRISE_PROXY)

// Helper class that manages force-refreshing Provisioning Domain configs on an
// `EnterpriseProxyService` and resolving pending callbacks when the refresh
// completes or times out.
class ProvisioningDomainRefreshHelper
    : public enterprise_net::EnterpriseProxyService::Observer {
 public:
  using RefreshCallback = connectors_internals::mojom::PageHandler::
      RefreshProvisioningDomainConfigsCallback;

  ProvisioningDomainRefreshHelper();
  ProvisioningDomainRefreshHelper(const ProvisioningDomainRefreshHelper&) =
      delete;
  ProvisioningDomainRefreshHelper& operator=(
      const ProvisioningDomainRefreshHelper&) = delete;
  ~ProvisioningDomainRefreshHelper() override;

  // Starts a refresh of Provisioning Domain configs on `proxy_service` if not
  // already in progress, and runs `callback` when the refresh completes or
  // times out after 15 seconds.
  void RefreshConfigs(enterprise_net::EnterpriseProxyService* proxy_service,
                      RefreshCallback callback);

 private:
  // enterprise_net::EnterpriseProxyService::Observer:
  void OnDynamicProxyConfigsStatusChanged() override;
  void OnEnterpriseProxyServiceDestroyed() override;

  void OnPvdRefreshTimeout();
  void ResolvePendingRefreshCallbacks();

  raw_ptr<enterprise_net::EnterpriseProxyService> proxy_service_ = nullptr;
  base::ScopedObservation<enterprise_net::EnterpriseProxyService,
                          enterprise_net::EnterpriseProxyService::Observer>
      proxy_service_observation_{this};
  std::vector<RefreshCallback> refresh_pvd_callbacks_;
  base::OneShotTimer pvd_refresh_timeout_timer_;
  base::WeakPtrFactory<ProvisioningDomainRefreshHelper> weak_ptr_factory_{this};
};

#endif  // BUILDFLAG(ENTERPRISE_PROXY)

}  // namespace enterprise_connectors

#endif  // COMPONENTS_ENTERPRISE_CONNECTORS_CORE_PROVISIONING_DOMAIN_REFRESH_HELPER_H_

