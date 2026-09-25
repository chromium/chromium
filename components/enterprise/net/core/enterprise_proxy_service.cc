// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/net/core/enterprise_proxy_service.h"

#include <set>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/enterprise/browser/identifiers/profile_id_service.h"
#include "components/enterprise/net/core/enterprise_network_auth_service.h"
#include "components/enterprise/net/core/features.h"
#include "components/enterprise/net/core/prefs.h"
#include "components/enterprise/net/core/proxy_provisioning_domain_manager.h"
#include "components/enterprise/net/core/utils.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "net/base/auth.h"
#include "net/log/net_log.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_source_type.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

namespace enterprise_net {

namespace {

constexpr std::string_view kDisguisedErrorCodes[] = {
    "403", "500", "502", "503", "504",
};

std::string_view DecisionToString(
    EnterpriseProxyService::ProxyAuthChallengeMatch::Decision decision) {
  using Decision = EnterpriseProxyService::ProxyAuthChallengeMatch::Decision;
  switch (decision) {
    case Decision::kNotApplicable:
      return "not_applicable";
    case Decision::kNoCredentialsNeeded:
      return "no_credentials_needed";
    case Decision::kDisguisedError:
      return "disguised_error";
    case Decision::kNeedsCredentials:
      return "needs_credentials";
  }
}

std::string_view OutcomeToString(
    EnterpriseProxyService::CredentialFetchOutcome outcome) {
  using Outcome = EnterpriseProxyService::CredentialFetchOutcome;
  switch (outcome) {
    case Outcome::kSuccess:
      return "token_acquired";
    case Outcome::kFailure:
      return "failed";
    case Outcome::kSignInRequired:
      return "sign_in_required";
  }
}

std::string_view TokenFetchErrorToString(TokenFetchError error) {
  switch (error) {
    case TokenFetchError::kNoPrimaryAccount:
      return "no_primary_account";
    case TokenFetchError::kUnmanagedUser:
      return "unmanaged_user";
    case TokenFetchError::kUnsupportedScope:
      return "unsupported_scope";
    case TokenFetchError::kInvalidCredentials:
      return "invalid_credentials";
    case TokenFetchError::kTransientError:
      return "transient_error";
    case TokenFetchError::kAuthError:
      return "auth_error";
    case TokenFetchError::kCanceled:
      return "canceled";
    case TokenFetchError::kInapplicableServer:
      return "inapplicable_server";
  }
}

// Checks whether `realm` header value represents a disguised proxy error.
// Currently, only supported error codes are 403, 500, 502, 503, 504.
bool IsDisguisedErrorRealm(std::string_view realm) {
  for (std::string_view code : kDisguisedErrorCodes) {
    if (realm == code) {
      return true;
    }
  }
  return false;
}

const base::DictValue* FindMatchingCachedConfig(
    const base::Value& policy_val,
    const base::DictValue* cached_configs_dict) {
  if (!cached_configs_dict || !policy_val.is_dict()) {
    return nullptr;
  }
  std::optional<ProvisioningDomainConfig> policy =
      ParseProxyProvisioningDomainPolicy(policy_val.GetDict());
  if (!policy.has_value()) {
    return nullptr;
  }

  std::string policy_hash = ComputePolicyHash(*policy);
  if (policy_hash.empty()) {
    return nullptr;
  }
  return cached_configs_dict->FindDict(policy_hash);
}

// Emits the NetLog event that closes out a challenge. Only terminal states do
// this: `kNeedsCredentials` is still in flight, and the fetch closes it later.
void RecordResolved(std::string_view decision,
                    const std::optional<net::AuthCredentials>& credentials,
                    const net::NetLogWithSource& challenge_net_log,
                    std::optional<std::string_view> failure_reason) {
  challenge_net_log.AddEvent(
      net::NetLogEventType::ENTERPRISE_PROXY_AUTH_CHALLENGE_RESOLVED, [&] {
        base::DictValue dict;
        dict.Set("decision", decision);
        dict.Set("has_credentials", credentials.has_value());
        if (failure_reason.has_value()) {
          dict.Set("failure_reason", *failure_reason);
        }
        return dict;
      });
}

// Records the classification stage. Every decision is recorded, including
// `kNeedsCredentials`, so this histogram is the denominator for the fetch
// histogram below and the two together read as a funnel.
void RecordDecision(
    EnterpriseProxyService::ProxyAuthChallengeMatch::Decision decision) {
  base::UmaHistogramEnumeration(
      "Enterprise.SecureGateway.ProxyAuthChallengeDecision", decision);
}

// Records the fetch stage and closes the NetLog source, then runs `callback`.
// Recording happens first so the event is emitted even if the callback tears
// down state.
void RecordOutcomeAndRun(
    EnterpriseProxyService::ProxyAuthChallengeCallback callback,
    EnterpriseProxyService::CredentialFetchOutcome outcome,
    const std::optional<net::AuthCredentials>& credentials,
    const net::NetLogWithSource& challenge_net_log,
    std::optional<std::string_view> failure_reason = std::nullopt) {
  base::UmaHistogramEnumeration(
      "Enterprise.SecureGateway.ProxyAuthCredentialFetchOutcome", outcome);
  RecordResolved(OutcomeToString(outcome), credentials, challenge_net_log,
                 failure_reason);
  std::move(callback).Run(outcome, credentials, challenge_net_log);
}

}  // namespace

EnterpriseProxyService::ProxyAuthChallengeMatch::ProxyAuthChallengeMatch() =
    default;
EnterpriseProxyService::ProxyAuthChallengeMatch::ProxyAuthChallengeMatch(
    const ProxyAuthChallengeMatch&) = default;
EnterpriseProxyService::ProxyAuthChallengeMatch&
EnterpriseProxyService::ProxyAuthChallengeMatch::operator=(
    const ProxyAuthChallengeMatch&) = default;
EnterpriseProxyService::ProxyAuthChallengeMatch::ProxyAuthChallengeMatch(
    ProxyAuthChallengeMatch&&) = default;
EnterpriseProxyService::ProxyAuthChallengeMatch&
EnterpriseProxyService::ProxyAuthChallengeMatch::operator=(
    ProxyAuthChallengeMatch&&) = default;
EnterpriseProxyService::ProxyAuthChallengeMatch::~ProxyAuthChallengeMatch() =
    default;

struct EnterpriseProxyService::PendingAuthRequest {
  struct CallbackWithNetLog {
    ProxyAuthChallengeCallback callback;
    net::NetLogWithSource net_log;
  };

  PendingAuthRequest(
      ProxyAuthChallengeCallback callback,
      net::NetLogWithSource net_log,
      const ProvisioningDomainProxyConfig::ProxyEndpoint& proxy_endpoint)
      : proxy_endpoint(proxy_endpoint) {
    callbacks.push_back({std::move(callback), std::move(net_log)});
  }

  // Guarantees that every callback this request owns runs exactly once. A
  // request that is resolved normally has already moved its callbacks out, so
  // this only fires for requests destroyed with a fetch still in flight (i.e.
  // Shutdown(), or destruction of the service without Shutdown()).
  ~PendingAuthRequest() {
    for (auto& [callback, net_log] : callbacks) {
      if (!callback) {
        continue;
      }
      RecordOutcomeAndRun(std::move(callback), CredentialFetchOutcome::kFailure,
                          std::nullopt, net_log, destruction_failure_reason);
    }
  }

  std::vector<CallbackWithNetLog> callbacks;
  ProvisioningDomainProxyConfig::ProxyEndpoint proxy_endpoint;

  // NetLog `failure_reason` reported if this request is destroyed before it
  // resolves. Overridden by Shutdown() to distinguish orderly teardown.
  std::string_view destruction_failure_reason = "request_destroyed";
};

EnterpriseProxyService::EnterpriseProxyService() = default;

EnterpriseProxyService::EnterpriseProxyService(
    PrefService* pref_service,
    EnterpriseNetworkAuthService* auth_service,
    GetURLLoaderFactoryCallback url_loader_factory_callback,
    enterprise::ProfileIdService* profile_id_service,
    net::NetLog* net_log)
    : pref_service_(pref_service),
      auth_service_(auth_service),
      url_loader_factory_callback_(std::move(url_loader_factory_callback)),
      profile_id_service_(profile_id_service),
      net_log_(net::NetLogWithSource::Make(
          net_log,
          net::NetLogSourceType::ENTERPRISE_PROXY_SERVICE)) {
  CHECK(pref_service_);
  CHECK(auth_service_);
  CHECK(url_loader_factory_callback_);
  net::NetworkChangeNotifier::AddNetworkChangeObserver(this);
  auth_service_observation_.Observe(auth_service_);
  pref_change_registrar_.Init(pref_service_);
  pref_change_registrar_.Add(
      kProxyProvisioningDomains,
      base::BindRepeating(&EnterpriseProxyService::OnPolicyPrefChanged,
                          base::Unretained(this)));
  OnPolicyPrefChanged();
}

EnterpriseProxyService::~EnterpriseProxyService() {
  net::NetworkChangeNotifier::RemoveNetworkChangeObserver(this);
}

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
    if (domain_manager->state() ==
        ProvisioningDomainProxyConfig::State::kFailedPermanent) {
      continue;
    }
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
  merged_config.is_update_in_progress = IsRefreshInProgress();
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

EnterpriseProxyService::ProxyAuthChallengeMatch
EnterpriseProxyService::ClassifyProxyAuthChallenge(
    const net::AuthChallengeInfo& auth_info,
    const GURL& destination_url,
    const scoped_refptr<net::HttpResponseHeaders>& response_headers) {
  using Decision = ProxyAuthChallengeMatch::Decision;

  ProxyAuthChallengeMatch match;
  match.net_log = net::NetLogWithSource::Make(
      net_log_.net_log(), net::NetLogSourceType::ENTERPRISE_PROXY_SERVICE);
  match.net_log.AddEvent(
      net::NetLogEventType::ENTERPRISE_PROXY_AUTH_CHALLENGE_RECEIVED, [&] {
        return base::DictValue()
            .Set("proxy_url", auth_info.challenger.Serialize())
            .Set("destination_url", destination_url.possibly_invalid_spec())
            .Set("auth_scheme", auth_info.scheme)
            .Set("realm", auth_info.realm)
            .Set("is_proxy", auth_info.is_proxy);
      });

  // Records a terminal decision on `match`. `kNeedsCredentials` records its
  // histogram sample separately below, but must not emit RESOLVED: the
  // challenge is still in flight until FetchProxyAuthCredentials() closes it.
  auto terminal = [&match](Decision decision,
                           std::optional<std::string_view> failure_reason =
                               std::nullopt) {
    match.decision = decision;
    RecordDecision(decision);
    RecordResolved(DecisionToString(decision), std::nullopt, match.net_log,
                   failure_reason);
    return std::move(match);
  };

  if (!auth_info.is_proxy) {
    return terminal(Decision::kNotApplicable);
  }

  net::ProxyChain proxy_chain = net::ProxyChain::FromSchemeHostAndPort(
      net::ProxyServer::SCHEME_HTTPS, auth_info.challenger.host(),
      auth_info.challenger.port());

  std::optional<ProvisioningDomainProxyConfig::ProxyEndpoint> matched_proxy =
      FindMatchingProxyEndpoint(destination_url, proxy_chain);

  if (!matched_proxy.has_value()) {
    return terminal(Decision::kNotApplicable);
  }

  // A matching route exists, so the challenge is ours, but the route gives us
  // no usable way to authenticate. Both sub-cases below are reported as
  // `kNoCredentialsNeeded` and fail closed identically; they differ only in the
  // NetLog reason, because they point at different misconfigurations.
  if (!matched_proxy->auth.has_value()) {
    // The PvD response advertised no auth block for this proxy at all, yet the
    // proxy issued a challenge. The proxy and its own PvD config disagree.
    return terminal(Decision::kNoCredentialsNeeded,
                    "no_auth_config_advertised");
  }

  if (matched_proxy->auth->type != AuthType::kProfileBearerToken) {
    // An auth block exists but does not ask for a bearer token, so there is
    // nothing for us to fetch.
    //
    // TODO(crbug.com/542666426): This cannot currently distinguish an explicit
    // "none" from a type string this client failed to recognize, because
    // ParseAuthType() maps every unrecognized value to AuthType::kNone. Telling
    // a deliberate no-auth route apart from a policy typo requires preserving
    // the parse failure in ProxyAuthConfig first.
    return terminal(Decision::kNoCredentialsNeeded,
                    "auth_type_not_bearer_token");
  }

  if (GetForcedDisguisedErrorCode().has_value() ||
      IsDisguisedErrorRealm(auth_info.realm)) {
    return terminal(Decision::kDisguisedError);
  }

  match.decision = Decision::kNeedsCredentials;
  RecordDecision(Decision::kNeedsCredentials);
  match.endpoint = std::move(matched_proxy);
  return match;
}

void EnterpriseProxyService::FetchProxyAuthCredentials(
    ProxyAuthChallengeMatch match,
    ProxyAuthChallengeCallback callback) {
  CHECK(match.decision == ProxyAuthChallengeMatch::Decision::kNeedsCredentials);
  CHECK(match.endpoint.has_value());

  // With no callback there is nobody to deliver a result to, so there is no
  // work worth starting. This leaves `match` exactly as if the caller had
  // dropped it: one missing histogram sample and an unterminated NetLog source,
  // which ClassifyProxyAuthChallenge() already documents as safe.
  if (!callback) {
    return;
  }

  // Simulate the identity layer reporting an unusable account. This sits here,
  // after the null-callback check, rather than in ClassifyProxyAuthChallenge(),
  // so that a forced sign-in travels the same path as a real one: there is no
  // way to know before the fetch that an account is missing or rejected, so a
  // synchronous verdict would exercise code that production never reaches.
  if (ShouldForceSignInRequired()) {
    RecordOutcomeAndRun(std::move(callback),
                        CredentialFetchOutcome::kSignInRequired, std::nullopt,
                        match.net_log, "forced_sign_in_required");
    return;
  }

  const auto& challenger = match.endpoint->proxy_chain.First().host_port_pair();

  // Deduplicate concurrent auth requests for the same challenger host/port.
  for (const auto& req : pending_auth_requests_) {
    const auto& host_port =
        req->proxy_endpoint.proxy_chain.First().host_port_pair();
    if (host_port.host() == challenger.host() &&
        host_port.port() == challenger.port()) {
      req->callbacks.push_back({std::move(callback), match.net_log});
      return;
    }
  }

  auto request = std::make_unique<PendingAuthRequest>(
      std::move(callback), match.net_log, *match.endpoint);
  PendingAuthRequest* request_ptr = request.get();
  pending_auth_requests_.push_back(std::move(request));

  GURL proxy_url(base::StrCat({"https://", challenger.ToString()}));

  auth_service_->FetchAccessToken(
      match.endpoint->auth->scope, proxy_url,
      base::BindOnce(&EnterpriseProxyService::OnProxyAuthTokenFetched,
                     weak_ptr_factory_.GetWeakPtr(), request_ptr));
}

void EnterpriseProxyService::Shutdown() {
  // Destroying the requests flushes their callbacks with
  // `CredentialFetchOutcome::kFailure`; tag them first so NetLog distinguishes
  // an orderly shutdown from other teardown.
  for (auto& request : pending_auth_requests_) {
    request->destruction_failure_reason = "service_shutdown";
  }
  pending_auth_requests_.clear();

  // No pending request can be resolved after this point, so drop any
  // outstanding token-fetch callbacks rather than letting them run against a
  // shut-down service with a stale PendingAuthRequest pointer.
  weak_ptr_factory_.InvalidateWeakPtrs();

  pref_change_registrar_.RemoveAll();
  if (!refreshing_managers_.empty()) {
    // Close any open network pause event if refreshes were in progress.
    net_log_.EndEvent(net::NetLogEventType::ENTERPRISE_PROXY_NETWORK_PAUSE);
  }
  refreshing_managers_.clear();
  provisioning_domain_observations_.RemoveAllObservations();
  provisioning_domain_managers_.clear();
  auth_service_observation_.Reset();
  observers_.Notify(&Observer::OnEnterpriseProxyServiceDestroyed);
  observers_.Clear();
}

void EnterpriseProxyService::OnProvisioningDomainStateChanged(
    ProxyProvisioningDomainManager* domain_manager) {
  const size_t old_count = refreshing_managers_.size();

  if (domain_manager->is_refresh_in_progress()) {
    refreshing_managers_.insert(domain_manager);
  } else {
    refreshing_managers_.erase(domain_manager);
    // Do not overwrite previously cached valid configurations with transient
    // or blocked failure states.
    if (domain_manager->state() !=
            ProvisioningDomainProxyConfig::State::kFailedTransient &&
        domain_manager->state() !=
            ProvisioningDomainProxyConfig::State::kFailedBlocked) {
      std::string policy_hash = ComputePolicyHash(domain_manager->policy());
      if (!policy_hash.empty()) {
        ScopedDictPrefUpdate update(pref_service_,
                                    kProvisioningDomainProxyConfigs);
        update->Set(policy_hash, domain_manager->ToDict());
      }
    }
  }

  const size_t new_count = refreshing_managers_.size();

  if (old_count == 0 && new_count > 0) {
    net_log_.BeginEvent(net::NetLogEventType::ENTERPRISE_PROXY_NETWORK_PAUSE);
    observers_.Notify(&Observer::OnDynamicProxyConfigsStatusChanged);
  } else if (old_count > 0 && new_count == 0) {
    net_log_.EndEvent(net::NetLogEventType::ENTERPRISE_PROXY_NETWORK_PAUSE);
    observers_.Notify(&Observer::OnDynamicProxyConfigsStatusChanged);
  }
}

void EnterpriseProxyService::OnNetworkChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  if (type != net::NetworkChangeNotifier::CONNECTION_NONE) {
    ForceRefreshAllConfigs();
  }
}

void EnterpriseProxyService::OnAccountStateChanged() {
  ForceRefreshAllConfigs();
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
    domains_list.Append(manager->ToDict());
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
  // If managers were currently refreshing, close the pause NetLog event before
  // destroying them, since manager destruction does not trigger
  // OnProvisioningDomainStateChanged.
  if (!refreshing_managers_.empty()) {
    net_log_.EndEvent(net::NetLogEventType::ENTERPRISE_PROXY_NETWORK_PAUSE);
  }
  refreshing_managers_.clear();
  provisioning_domain_observations_.RemoveAllObservations();
  provisioning_domain_managers_.clear();

  const base::DictValue& cached_configs_dict =
      pref_service_->GetDict(kProvisioningDomainProxyConfigs);

  std::set<std::string> active_policy_hashes;

  for (const auto& domain_val : policy_domains) {
    const base::DictValue* matching_cached_config =
        FindMatchingCachedConfig(domain_val, &cached_configs_dict);
    auto manager = std::make_unique<ProxyProvisioningDomainManager>(
        domain_val, matching_cached_config, auth_service_,
        url_loader_factory_callback_);
    std::string policy_hash = ComputePolicyHash(manager->policy());
    if (!policy_hash.empty()) {
      active_policy_hashes.insert(policy_hash);
    }
    provisioning_domain_observations_.AddObservation(manager.get());
    OnProvisioningDomainStateChanged(manager.get());
    provisioning_domain_managers_.push_back(std::move(manager));
  }

  // Prune cache entries for policy hashes that are no longer configured.
  ScopedDictPrefUpdate update(pref_service_, kProvisioningDomainProxyConfigs);
  std::vector<std::string> keys_to_remove;
  for (const auto [key, value] : *update) {
    if (!active_policy_hashes.contains(key)) {
      keys_to_remove.push_back(key);
    }
  }
  for (const auto& key : keys_to_remove) {
    update->Remove(key);
  }
}

std::string EnterpriseProxyService::BuildBasicAuthUsername(
    const std::vector<ProxyExtraHeader>& proxy_headers) const {
  std::string profile_id;
  if (profile_id_service_) {
    std::optional<std::string> pid = profile_id_service_->GetProfileId();
    if (pid.has_value()) {
      profile_id = *pid;
    }
  }
  std::string accept_languages;
  if (pref_service_ &&
      pref_service_->FindPreference(language::prefs::kAcceptLanguages)) {
    accept_languages =
        pref_service_->GetString(language::prefs::kAcceptLanguages);
  }

  net::HttpRequestHeaders resolved_proxy_headers =
      ResolveExtraHeadersWithValues(proxy_headers, profile_id,
                                    accept_languages);

  std::vector<std::string> query_params;
  for (const auto& [key, val] : resolved_proxy_headers.GetHeaderVector()) {
    std::string escaped_key = base::EscapeQueryParamValue(key, true);
    std::string escaped_val = base::EscapeQueryParamValue(val, true);
    query_params.push_back(escaped_key + "=" + escaped_val);
  }
  return base::JoinString(query_params, "&");
}

void EnterpriseProxyService::OnProxyAuthTokenFetched(
    PendingAuthRequest* request,
    AccessTokenResult token_result) {
  auto it =
      std::find_if(pending_auth_requests_.begin(), pending_auth_requests_.end(),
                   [request](const std::unique_ptr<PendingAuthRequest>& r) {
                     return r.get() == request;
                   });
  // A request only leaves `pending_auth_requests_` here or during teardown,
  // and teardown invalidates the weak pointer bound to this callback, so the
  // request must still be present.
  CHECK(it != pending_auth_requests_.end());
  std::unique_ptr<PendingAuthRequest> owned_request = std::move(*it);
  pending_auth_requests_.erase(it);

  if (!token_result.has_value()) {
    CredentialFetchOutcome outcome =
        (token_result.error() == TokenFetchError::kNoPrimaryAccount ||
         token_result.error() == TokenFetchError::kInvalidCredentials)
            ? CredentialFetchOutcome::kSignInRequired
            : CredentialFetchOutcome::kFailure;
    std::string_view failure_reason =
        TokenFetchErrorToString(token_result.error());
    for (auto& [cb, req_net_log] : owned_request->callbacks) {
      RecordOutcomeAndRun(std::move(cb), outcome, std::nullopt, req_net_log,
                          failure_reason);
    }
    return;
  }

  std::string access_token = std::move(*token_result);
  std::string username =
      BuildBasicAuthUsername(owned_request->proxy_endpoint.extra_headers);
  std::string password = access_token;

  net::AuthCredentials credentials(base::UTF8ToUTF16(username),
                                   base::UTF8ToUTF16(password));
  for (auto& [cb, req_net_log] : owned_request->callbacks) {
    RecordOutcomeAndRun(std::move(cb), CredentialFetchOutcome::kSuccess,
                        credentials, req_net_log);
  }
}

}  // namespace enterprise_net
