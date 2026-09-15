// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/provisioning_domain_refresh_helper.h"

#if BUILDFLAG(ENTERPRISE_PROXY)

#include <utility>

#include "base/time/time.h"
#include "components/enterprise/connectors/core/connectors_internals_utils.h"

namespace enterprise_connectors {

ProvisioningDomainRefreshHelper::ProvisioningDomainRefreshHelper() = default;

ProvisioningDomainRefreshHelper::~ProvisioningDomainRefreshHelper() = default;

void ProvisioningDomainRefreshHelper::RefreshConfigs(
    enterprise_net::EnterpriseProxyService* proxy_service,
    RefreshCallback callback) {
  if (!proxy_service) {
    std::move(callback).Run(utils::GetProvisioningDomainState(nullptr));
    return;
  }

  if (proxy_service_ != proxy_service) {
    proxy_service_observation_.Reset();
    proxy_service_ = proxy_service;
  }

  if (!proxy_service_observation_.IsObserving()) {
    proxy_service_observation_.Observe(proxy_service_);
  }

  refresh_pvd_callbacks_.push_back(std::move(callback));

  if (!proxy_service_->IsRefreshInProgress()) {
    proxy_service_->ForceRefreshAllConfigs();
  }

  // `ForceRefreshAllConfigs()` may finish synchronously or be a no-op (e.g. if
  // there are no configs or all configs are permanently failed). If no refresh
  // is running, resolve callbacks immediately rather than waiting for timeout.
  if (!proxy_service_->IsRefreshInProgress()) {
    ResolvePendingRefreshCallbacks();
    return;
  }

  if (!pvd_refresh_timeout_timer_.IsRunning()) {
    pvd_refresh_timeout_timer_.Start(
        FROM_HERE, base::Seconds(15),
        base::BindOnce(&ProvisioningDomainRefreshHelper::OnPvdRefreshTimeout,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void ProvisioningDomainRefreshHelper::ResolvePendingRefreshCallbacks() {
  pvd_refresh_timeout_timer_.Stop();
  if (refresh_pvd_callbacks_.empty()) {
    return;
  }

  auto callbacks = std::move(refresh_pvd_callbacks_);
  auto state = utils::GetProvisioningDomainState(proxy_service_);
  for (auto& callback : callbacks) {
    std::move(callback).Run(state.Clone());
  }
}

void ProvisioningDomainRefreshHelper::OnDynamicProxyConfigsStatusChanged() {
  if (proxy_service_ && proxy_service_->IsRefreshInProgress()) {
    return;
  }

  ResolvePendingRefreshCallbacks();
}

void ProvisioningDomainRefreshHelper::OnEnterpriseProxyServiceDestroyed() {
  proxy_service_observation_.Reset();
  proxy_service_ = nullptr;
  ResolvePendingRefreshCallbacks();
}

void ProvisioningDomainRefreshHelper::OnPvdRefreshTimeout() {
  ResolvePendingRefreshCallbacks();
}

}  // namespace enterprise_connectors

#endif  // BUILDFLAG(ENTERPRISE_PROXY)

