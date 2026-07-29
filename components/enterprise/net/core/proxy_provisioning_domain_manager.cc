// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/net/core/proxy_provisioning_domain_manager.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "components/enterprise/net/core/enterprise_network_auth_service.h"
#include "components/enterprise/net/core/provisioning_domain_fetcher.h"
#include "components/enterprise/net/core/utils.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace enterprise_net {

namespace {

bool IsTransientHttpError(int net_error, std::optional<int> response_code) {
  // Network / Socket errors
  if (net_error != net::OK) {
    // Permanent network/client errors: certificate failures, browser blocks,
    // invalid URLs, unknown URL scheme.
    if (net::IsCertificateError(net_error) ||
        net::IsClientCertificateError(net_error) ||
        net::IsRequestBlockedError(net_error) ||
        net_error == net::ERR_INVALID_URL ||
        net_error == net::ERR_UNKNOWN_URL_SCHEME) {
      return false;
    }

    // Other net errors are considered to be transient.
    return true;
  }

  // HTTP Status Codes
  if (response_code.has_value()) {
    int code = *response_code;
    // 5xx errors are transient.
    if (code / 100 == 5) {
      return true;
    }
    // 408 and 429 are transient.
    if (code == net::HTTP_TOO_MANY_REQUESTS ||
        code == net::HTTP_REQUEST_TIMEOUT) {
      return true;
    }
    return false;
  }

  return true;
}

bool IsTransientError(const ProvisioningDomainFetchError& error) {
  switch (error.status) {
    case ProvisioningDomainFetchResultStatus::kInvalidUrl:
    case ProvisioningDomainFetchResultStatus::kParseError:
      return false;

    case ProvisioningDomainFetchResultStatus::kTokenFetchError:
      return error.token_fetch_error == TokenFetchError::kTransientError;

    case ProvisioningDomainFetchResultStatus::kHttpError:
      return IsTransientHttpError(error.net_error, error.response_code);

    case ProvisioningDomainFetchResultStatus::kSuccess:
      return false;
  }
}

}  // namespace

ProxyProvisioningDomainManager::ProxyProvisioningDomainManager(
    const ProvisioningDomainConfig& policy,
    EnterpriseNetworkAuthService* auth_service,
    GetURLLoaderFactoryCallback url_loader_factory_callback)
    : policy_(policy),
      auth_service_(auth_service),
      url_loader_factory_callback_(std::move(url_loader_factory_callback)) {
  CHECK(auth_service_);
  CHECK(url_loader_factory_callback_);
  fetched_config_.pvd_id = policy_.pvd_id;
  fetched_config_.state = ProvisioningDomainProxyConfig::State::kRefreshNeeded;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&ProxyProvisioningDomainManager::StartRefreshInternal,
                     weak_factory_.GetWeakPtr()));
}

ProxyProvisioningDomainManager::~ProxyProvisioningDomainManager() = default;

void ProxyProvisioningDomainManager::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ProxyProvisioningDomainManager::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ProxyProvisioningDomainManager::ForceRefresh() {
  if (is_refresh_in_progress()) {
    CancelRefresh();
  }
  StartRefreshInternal();
}

void ProxyProvisioningDomainManager::CancelRefresh() {
  weak_factory_.InvalidateWeakPtrs();
  fetcher_.reset();
}

base::DictValue ProxyProvisioningDomainManager::GetDebugInfo() const {
  base::DictValue dict;
  dict.Set("policy", ProvisioningDomainConfigToDict(policy_));
  dict.Set("fetched_config",
           ProvisioningDomainProxyConfigToDict(fetched_config_));
  return dict;
}

void ProxyProvisioningDomainManager::Refresh() {
  if (is_refresh_in_progress()) {
    return;
  }
  StartRefreshInternal();
}

void ProxyProvisioningDomainManager::StartRefreshInternal() {
  if (!url_loader_factory_) {
    url_loader_factory_ = url_loader_factory_callback_.Run();
  }

  if (!url_loader_factory_) {
    fetched_config_.state =
        ProvisioningDomainProxyConfig::State::kFailedTransient;
    NotifyIfStateChanged();
    return;
  }
  fetcher_ = std::make_unique<ProvisioningDomainFetcher>(policy_, auth_service_,
                                                         url_loader_factory_);
  fetched_config_.state = ProvisioningDomainProxyConfig::State::kFetching;
  NotifyIfStateChanged();

  fetcher_->Start(
      base::BindOnce(&ProxyProvisioningDomainManager::OnRefreshComplete,
                     weak_factory_.GetWeakPtr()));
}

void ProxyProvisioningDomainManager::OnRefreshComplete(
    ProvisioningDomainFetchResult result) {
  fetcher_.reset();

  if (result.has_value()) {
    fetched_config_ = std::move(*result);
    fetched_config_.state = ProvisioningDomainProxyConfig::State::kValid;
  } else {
    // Preserve existing routes on failure.
    fetched_config_.state =
        IsTransientError(result.error())
            ? ProvisioningDomainProxyConfig::State::kFailedTransient
            : ProvisioningDomainProxyConfig::State::kFailedPermanent;
  }

  NotifyIfStateChanged();
}

void ProxyProvisioningDomainManager::NotifyIfStateChanged() {
  if (last_notified_state_.has_value() &&
      *last_notified_state_ == fetched_config_.state) {
    return;
  }
  last_notified_state_ = fetched_config_.state;

  for (auto& observer : observers_) {
    observer.OnProvisioningDomainStateChanged(this);
  }
}

}  // namespace enterprise_net
