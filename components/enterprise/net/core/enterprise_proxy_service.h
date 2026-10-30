// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_NET_CORE_ENTERPRISE_PROXY_SERVICE_H_
#define COMPONENTS_ENTERPRISE_NET_CORE_ENTERPRISE_PROXY_SERVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "base/values.h"
#include "components/enterprise/net/core/enterprise_network_auth_service.h"
#include "components/enterprise/net/core/proxy_provisioning_domain_manager.h"
#include "components/enterprise/net/core/types.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "net/base/auth.h"
#include "net/base/network_change_notifier.h"
#include "net/http/http_response_headers.h"
#include "net/log/net_log_with_source.h"
#include "url/gurl.h"

class PrefService;

namespace net {
class NetLog;
}  // namespace net

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace enterprise {
class ProfileIdService;
}  // namespace enterprise

namespace enterprise_net {

// State-machine service responsible for managing multiple
// ProxyProvisioningDomainManager, each of which maintain up-to-date proxy
// configurations fetched from their configured Provisioning Domain (PvD).
class EnterpriseProxyService
    : public KeyedService,
      public ProxyProvisioningDomainManager::Observer,
      public net::NetworkChangeNotifier::NetworkChangeObserver,
      public EnterpriseNetworkAuthService::Observer {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called when dynamic proxy route status changes (e.g. refresh starts or
    // completes, or dynamic routing rules update).
    virtual void OnDynamicProxyConfigsStatusChanged() = 0;

    // Called during Shutdown(), used by non-keyed service classes (e.g.
    // PrefProxyConfigTrackerImpl) to maintain lifetime and reset their
    // ScopedObservation before the service is destroyed.
    virtual void OnEnterpriseProxyServiceDestroyed() {}
  };

  using GetURLLoaderFactoryCallback =
      base::RepeatingCallback<scoped_refptr<network::SharedURLLoaderFactory>()>;

  // Outcome of the asynchronous credential fetch. Exists only for a
  // `kNeedsCredentials` match; every other decision is terminal on its own and
  // never produces one of these.
  //
  // LINT.IfChange(CredentialFetchOutcome)
  enum class CredentialFetchOutcome {
    // A token was acquired and credentials are attached.
    kSuccess = 0,
    // The token fetch failed for a reason the user cannot act on.
    kFailure = 1,
    // The token fetch failed because there is no primary account, or the
    // account's credentials are invalid. Distinct from `kFailure` because the
    // user can fix it by signing in.
    kSignInRequired = 2,
    kMaxValue = kSignInRequired,
  };
  // LINT.ThenChange(//tools/metrics/histograms/enums.xml:EnterpriseProxyAuthCredentialFetchOutcome)

  using ProxyAuthChallengeCallback =
      base::OnceCallback<void(CredentialFetchOutcome,
                              const std::optional<net::AuthCredentials>&,
                              const net::NetLogWithSource&)>;

  // Outcome of synchronously classifying a 407 Proxy Authentication challenge
  // against the managed dynamic routes. Produced by
  // `ClassifyProxyAuthChallenge()`.
  struct ProxyAuthChallengeMatch {
    // LINT.IfChange(ProxyAuthChallengeDecision)
    enum class Decision {
      // No applicable rule for the destination URL & proxy pair. The challenge
      // is none of this service's business and the caller should fall back to
      // its default auth handling.
      kNotApplicable = 0,
      // A matching rule explicitly specifies no auth or non-bearer auth.
      kNoCredentialsNeeded = 1,
      // The response contains a disguised error from the proxy.
      kDisguisedError = 2,
      // A bearer token is required. This is the only non-terminal decision:
      // the caller must hand the match to `FetchProxyAuthCredentials()`, which
      // produces the `CredentialFetchOutcome`. Note that a missing or rejected
      // account is not detectable here, and surfaces as
      // `CredentialFetchOutcome::kSignInRequired` instead.
      kNeedsCredentials = 3,
      kMaxValue = kNeedsCredentials,
    };
    // LINT.ThenChange(//tools/metrics/histograms/enums.xml:EnterpriseProxyAuthChallengeDecision)

    ProxyAuthChallengeMatch();
    ProxyAuthChallengeMatch(const ProxyAuthChallengeMatch&);
    ProxyAuthChallengeMatch& operator=(const ProxyAuthChallengeMatch&);
    ProxyAuthChallengeMatch(ProxyAuthChallengeMatch&&);
    ProxyAuthChallengeMatch& operator=(ProxyAuthChallengeMatch&&);
    ~ProxyAuthChallengeMatch();

    Decision decision = Decision::kNotApplicable;

    // Set if and only if `decision` is `kNeedsCredentials`.
    std::optional<ProvisioningDomainProxyConfig::ProxyEndpoint> endpoint;

    // NetLog source scoped to this challenge. Carried through to
    // `FetchProxyAuthCredentials()` so that the RECEIVED and RESOLVED events
    // share a source ID.
    net::NetLogWithSource net_log;
  };

  EnterpriseProxyService(
      PrefService* pref_service,
      EnterpriseNetworkAuthService* auth_service,
      GetURLLoaderFactoryCallback url_loader_factory_callback,
      enterprise::ProfileIdService* profile_id_service = nullptr,
      net::NetLog* net_log = nullptr);

  EnterpriseProxyService(const EnterpriseProxyService&) = delete;
  EnterpriseProxyService& operator=(const EnterpriseProxyService&) = delete;

  ~EnterpriseProxyService() override;

  virtual void AddObserver(Observer* observer);
  virtual void RemoveObserver(Observer* observer);

  // Returns the list of current PvD configs with their states.
  std::vector<ProvisioningDomainProxyConfig> GetProvisioningDomainConfigs()
      const;

  // Returns true if there is at least one background refresh currently running.
  virtual bool IsRefreshInProgress() const;

  // Looks up the first matching `ProxyEndpoint` across all active Provisioning
  // Domain configs for a given `destination_url` and `proxy_chain`.
  // Returns std::nullopt if no matching endpoint is found.
  std::optional<ProvisioningDomainProxyConfig::ProxyEndpoint>
  FindMatchingProxyEndpoint(const GURL& destination_url,
                            const net::ProxyChain& proxy_chain) const;

  // Returns the `net::ProxyConfig::DynamicRoutingConfig` concatenated
  // from all valid active Provisioning Domain configs, with ordering strictly
  // preserved and all browser-side PvD metadata removed.
  virtual net::ProxyConfig::DynamicRoutingConfig GetDynamicRoutingConfig()
      const;

  // Returns a dictionary containing detailed debugging information for all
  // managed Provisioning Domains and active fetch states.
  virtual base::DictValue GetDebugInfo() const;

  // Classifies a 407 Proxy Authentication challenge against the managed
  // dynamic routes.
  //
  // Emits the ENTERPRISE_PROXY_AUTH_CHALLENGE_RECEIVED NetLog event, and for
  // every terminal decision also emits ENTERPRISE_PROXY_AUTH_CHALLENGE_RESOLVED
  // and the result histogram. `kNeedsCredentials` is the sole non-terminal
  // decision; callers that receive it must pass the match to
  // `FetchProxyAuthCredentials()`, which records the terminal result. Dropping
  // a `kNeedsCredentials` match on the floor loses one histogram sample and
  // leaves an unterminated NetLog source, but is otherwise safe.
  //
  // Note that in-flight auth requests will not adjust for any config changes
  // that occur after classification is finished.
  [[nodiscard]] virtual ProxyAuthChallengeMatch ClassifyProxyAuthChallenge(
      const net::AuthChallengeInfo& auth_info,
      const GURL& destination_url,
      const scoped_refptr<net::HttpResponseHeaders>& response_headers);

  // Fetches credentials for a `kNeedsCredentials` `match`. Takes ownership of
  // `callback` and guarantees it runs exactly once, including if this service
  // is shut down or destroyed while the fetch is in flight (in which case the
  // outcome is `CredentialFetchOutcome::kFailure`).
  //
  // A null `callback` is a no-op: no fetch is started and `match` is discarded.
  virtual void FetchProxyAuthCredentials(ProxyAuthChallengeMatch match,
                                         ProxyAuthChallengeCallback callback);

 protected:
  // Protected constructor for test doubles (e.g. MockEnterpriseProxyService).
  EnterpriseProxyService();

 public:
  // KeyedService:
  void Shutdown() override;

  // ProxyProvisioningDomainManager::Observer:
  void OnProvisioningDomainStateChanged(
      ProxyProvisioningDomainManager* domain_manager) override;

  // net::NetworkChangeNotifier::NetworkChangeObserver:
  void OnNetworkChanged(
      net::NetworkChangeNotifier::ConnectionType type) override;

  // EnterpriseNetworkAuthService::Observer:
  void OnAccountStateChanged() override;

  // Forces a new fetch for all managed Provisioning Domains.
  virtual void ForceRefreshAllConfigs();

 private:
  friend class EnterpriseProxyServiceTest;

  void OnPolicyPrefChanged();

  // Recreates the managed `ProxyProvisioningDomainManager` instances from the
  // "ProxyProvisioningDomains" policy list. Clears all current managers and
  // creates new ones for each policy entry.
  // TODO(crbug.com/526587734): Incorporate preference caching to restore cached
  // Provisioning Domain configs on startup and preserve/move cached configs for
  // existing domain managers across policy updates (even if their index
  // changes).
  void RecreateProvisioningDomainManagers(
      const base::ListValue& policy_domains);

  // Represents an in-flight OAuth token fetch for a proxy authentication
  // challenge, grouping callbacks for duplicate requests to the same proxy.
  struct PendingAuthRequest;

  // Resolves variable placeholders (e.g. `${profile_id}`, `${accept_language}`)
  // in the proxy endpoint extra headers and formats them as URL-escaped query
  // parameters to be used as the Basic Auth username.
  std::string BuildBasicAuthUsername(
      const std::vector<ProxyExtraHeader>& proxy_headers) const;

  void OnProxyAuthTokenFetched(PendingAuthRequest* request,
                               AccessTokenResult token_result);

  const raw_ptr<PrefService> pref_service_;
  const raw_ptr<EnterpriseNetworkAuthService> auth_service_;

  // Callback used to obtain a SharedURLLoaderFactory lazily when creating
  // ProvisioningDomainManagers. A callback is used instead of a static
  // SharedURLLoaderFactory pointer because Profile StoragePartition
  // initialization may not be ready at KeyedService creation time.
  GetURLLoaderFactoryCallback url_loader_factory_callback_;

  const raw_ptr<enterprise::ProfileIdService> profile_id_service_;

  PrefChangeRegistrar pref_change_registrar_;
  base::ObserverList<Observer> observers_;

  // In-memory list of provisioning domain state machines matching policy.
  std::vector<std::unique_ptr<ProxyProvisioningDomainManager>>
      provisioning_domain_managers_;

  // Automatically manages observation of domain manager instances.
  base::ScopedMultiSourceObservation<ProxyProvisioningDomainManager,
                                     ProxyProvisioningDomainManager::Observer>
      provisioning_domain_observations_{this};

  base::ScopedObservation<EnterpriseNetworkAuthService,
                          EnterpriseNetworkAuthService::Observer>
      auth_service_observation_{this};

  // Set of managers currently executing a background fetch.
  base::flat_set<raw_ptr<ProxyProvisioningDomainManager>> refreshing_managers_;

  // List of pending proxy auth requests.
  std::vector<std::unique_ptr<PendingAuthRequest>> pending_auth_requests_;

  net::NetLogWithSource net_log_;

  base::WeakPtrFactory<EnterpriseProxyService> weak_ptr_factory_{this};
};

}  // namespace enterprise_net

#endif  // COMPONENTS_ENTERPRISE_NET_CORE_ENTERPRISE_PROXY_SERVICE_H_
