// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/net/core/proxy_provisioning_domain_manager.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/values.h"
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

ProvisioningDomainProxyConfig::State ClassifyFetchError(
    const ProvisioningDomainFetchError& error) {
  switch (error.status) {
    case ProvisioningDomainFetchResultStatus::kInvalidUrl:
      return ProvisioningDomainProxyConfig::State::kFailedPermanent;

    // Invalid response, we should treat this as a blocked error in case the
    // server side response is corrected, or in the special case of captive
    // portal.
    case ProvisioningDomainFetchResultStatus::kParseError:
      return ProvisioningDomainProxyConfig::State::kFailedBlocked;

    case ProvisioningDomainFetchResultStatus::kTokenFetchError:
      CHECK(error.token_fetch_error.has_value());
      switch (*error.token_fetch_error) {
        case TokenFetchError::kTransientError:
          return ProvisioningDomainProxyConfig::State::kFailedTransient;
        case TokenFetchError::kNoPrimaryAccount:
        case TokenFetchError::kUnmanagedUser:
        case TokenFetchError::kInvalidCredentials:
        case TokenFetchError::kAuthError:
        case TokenFetchError::kCanceled:
          return ProvisioningDomainProxyConfig::State::kFailedBlocked;
        case TokenFetchError::kUnsupportedScope:
          return ProvisioningDomainProxyConfig::State::kFailedPermanent;
      }

    case ProvisioningDomainFetchResultStatus::kHttpError:
      if (IsTransientHttpError(error.net_error, error.response_code)) {
        return ProvisioningDomainProxyConfig::State::kFailedTransient;
      }
      return ProvisioningDomainProxyConfig::State::kFailedPermanent;

    case ProvisioningDomainFetchResultStatus::kSuccess:
      NOTREACHED();
  }
}

ProvisioningDomainConfig ParsePolicyFromValue(const base::Value& policy_val) {
  const base::DictValue* dict = policy_val.GetIfDict();
  ProvisioningDomainConfig fallback_policy;
  if (!dict) {
    return fallback_policy;
  }

  std::optional<ProvisioningDomainConfig> parsed_policy =
      ParseProxyProvisioningDomainPolicy(*dict);
  if (parsed_policy.has_value()) {
    return std::move(*parsed_policy);
  }

  // Fallback: If parsing fails, try to retrieve pvd_id if available.
  const std::string* pvd_id = dict->FindString("pvd_id");
  if (pvd_id) {
    fallback_policy.pvd_id = *pvd_id;
  }
  return fallback_policy;
}

}  // namespace

ProxyProvisioningDomainManager::ProxyProvisioningDomainManager(
    const base::Value& policy_val,
    const base::DictValue* cached_config_dict,
    EnterpriseNetworkAuthService* auth_service,
    GetURLLoaderFactoryCallback url_loader_factory_callback)
    : policy_(ParsePolicyFromValue(policy_val)),
      auth_service_(auth_service),
      url_loader_factory_callback_(std::move(url_loader_factory_callback)) {
  CHECK(auth_service_);
  CHECK(url_loader_factory_callback_);

  const base::DictValue* dict = policy_val.GetIfDict();
  if (!dict || !ParseProxyProvisioningDomainPolicy(*dict).has_value()) {
    fetched_config_.pvd_id = policy_.pvd_id;
    fetched_config_.state =
        ProvisioningDomainProxyConfig::State::kFailedPermanent;
    return;
  }

  fetched_config_.pvd_id = policy_.pvd_id;
  fetched_config_.state = ProvisioningDomainProxyConfig::State::kRefreshNeeded;
  if (cached_config_dict) {
    const base::DictValue* fetched_dict =
        cached_config_dict->FindDict("fetched_config");
    if (fetched_dict) {
      std::optional<ProvisioningDomainProxyConfig> parsed =
          ParseProvisioningDomainConfig(*fetched_dict);
      if (parsed.has_value()) {
        fetched_config_ = std::move(*parsed);
        fetched_config_.state =
            ProvisioningDomainProxyConfig::State::kRefreshNeeded;
      }
    }
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&ProxyProvisioningDomainManager::StartRefreshInternal,
                     weak_factory_.GetWeakPtr(), /*force=*/false));
}

ProxyProvisioningDomainManager::~ProxyProvisioningDomainManager() = default;

void ProxyProvisioningDomainManager::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ProxyProvisioningDomainManager::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ProxyProvisioningDomainManager::ForceRefresh() {
  if (state() == ProvisioningDomainProxyConfig::State::kFailedPermanent) {
    return;
  }
  if (is_refresh_in_progress()) {
    CancelRefresh();
  }
  StartRefreshInternal(/*force=*/true);
}

void ProxyProvisioningDomainManager::CancelRefresh() {
  weak_factory_.InvalidateWeakPtrs();
  fetcher_.reset();
}

base::DictValue ProxyProvisioningDomainManager::ToDict() const {
  base::DictValue dict;
  dict.Set("policy_hash", ComputePolicyHash(policy_));
  dict.Set("policy", ProvisioningDomainConfigToDict(policy_));
  dict.Set("fetched_config",
           ProvisioningDomainProxyConfigToDict(fetched_config_));
  return dict;
}

void ProxyProvisioningDomainManager::Refresh() {
  if (is_refresh_in_progress()) {
    return;
  }
  StartRefreshInternal(/*force=*/false);
}

void ProxyProvisioningDomainManager::StartRefreshInternal(bool force) {
  if (state() == ProvisioningDomainProxyConfig::State::kFailedPermanent) {
    return;
  }
  if (!force &&
      state() == ProvisioningDomainProxyConfig::State::kFailedBlocked) {
    return;
  }
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
    ProvisioningDomainProxyConfig::State state =
        ClassifyFetchError(result.error());
    if (state == ProvisioningDomainProxyConfig::State::kFailedTransient) {
      // Preserve existing routes for momentary network/server blips.
      fetched_config_.state = state;
    } else {
      // Flush active routes so re-authentication and IdP traffic connect
      // directly (kFailedBlocked) or mark permanently failed
      // (kFailedPermanent).
      fetched_config_ = ProvisioningDomainProxyConfig();
      fetched_config_.pvd_id = policy_.pvd_id;
      fetched_config_.state = state;
    }
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
