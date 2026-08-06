// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_NET_CORE_ENTERPRISE_NETWORK_AUTH_SERVICE_H_
#define COMPONENTS_ENTERPRISE_NET_CORE_ENTERPRISE_NETWORK_AUTH_SERVICE_H_

#include <memory>
#include <optional>
#include <string>

#include "base/containers/id_map.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "components/enterprise/net/core/types.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/public/base/oauth_consumer_id.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/http/http_request_headers.h"

namespace signin {
struct AccessTokenInfo;
class AccountManagedStatusFinder;
class IdentityManager;
class PrimaryAccountAccessTokenFetcher;
}  // namespace signin

class PrefService;

namespace enterprise {
class ProfileIdService;
}  // namespace enterprise

namespace enterprise_net {

// LINT.IfChange(EnterpriseNetworkTokenFetchError)
// Result error states for access token fetching.
enum class TokenFetchError {
  // Reserved for histograms.
  // kNone = 0
  kNoPrimaryAccount = 1,
  kUnmanagedUser = 2,
  kUnsupportedScope = 3,
  kInvalidCredentials = 4,
  kTransientError = 5,
  kAuthError = 6,
  kCanceled = 7,
  kMaxValue = kCanceled,
};
// LINT.ThenChange(//tools/metrics/histograms/enums.xml:EnterpriseNetworkTokenFetchError)

inline constexpr TokenFetchError kNoErrorForMetrics =
    static_cast<TokenFetchError>(0);

using AccessTokenResult = base::expected<std::string, TokenFetchError>;

// KeyedService responsible for providing authentication for Enterprise Network
// features.
class EnterpriseNetworkAuthService : public KeyedService {
 public:
  using AccessTokenCallback =
      base::OnceCallback<void(AccessTokenResult result)>;

  EnterpriseNetworkAuthService(
      signin::IdentityManager* identity_manager,
      PrefService* pref_service,
      enterprise::ProfileIdService* profile_id_service);
  EnterpriseNetworkAuthService(const EnterpriseNetworkAuthService&) = delete;
  EnterpriseNetworkAuthService& operator=(const EnterpriseNetworkAuthService&) =
      delete;
  ~EnterpriseNetworkAuthService() override;

  // KeyedService:
  void Shutdown() override;

  // Asynchronously requests an access token for the given auth `scope`.
  // Calls `callback` with the token string on success, or a TokenFetchError on
  // failure.
  virtual void FetchAccessToken(AuthScope scope, AccessTokenCallback callback);

  // Resolves extra headers specified by `extra_headers` using identity and user
  // preferences. Supported variable placeholders: `${profile_id}` and
  // `${accept_language}`. Unsupported `kVariable` headers are dropped.
  virtual net::HttpRequestHeaders ResolveExtraHeaders(
      const std::vector<ProxyExtraHeader>& extra_headers) const;

  // Returns the number of currently active in-flight token fetch requests.
  size_t GetPendingTokenFetchCountForTesting() const {
    return access_token_fetchers_.size() + pending_status_checks_.size();
  }

  // Struct used for tracking in-flight account managed status checks, which
  // can be either synchronous or asynchronous.
  struct PendingManagedStatusCheck {
    PendingManagedStatusCheck();
    PendingManagedStatusCheck(
        std::unique_ptr<signin::AccountManagedStatusFinder> finder,
        signin::OAuthConsumerId consumer_id,
        AccessTokenCallback callback);
    PendingManagedStatusCheck(PendingManagedStatusCheck&&) noexcept;
    PendingManagedStatusCheck& operator=(PendingManagedStatusCheck&&) noexcept;
    ~PendingManagedStatusCheck();

    std::unique_ptr<signin::AccountManagedStatusFinder> finder;
    signin::OAuthConsumerId consumer_id;
    AccessTokenCallback callback;
  };

  struct PendingTokenFetch {
    PendingTokenFetch();
    PendingTokenFetch(
        std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher> fetcher,
        AccessTokenCallback callback);
    PendingTokenFetch(PendingTokenFetch&&) noexcept;
    PendingTokenFetch& operator=(PendingTokenFetch&&) noexcept;
    ~PendingTokenFetch();

    std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher> fetcher;
    AccessTokenCallback callback;
  };

  // Cancels all pending access token requests and responds to their
  // callbacks with TokenFetchError::kCanceled.
  void ClearPendingTokenFetches();

 protected:
  // Protected constructor for test doubles (e.g.
  // MockEnterpriseNetworkAuthService).
  EnterpriseNetworkAuthService();

 private:
  void StartAccessTokenFetch(signin::OAuthConsumerId consumer_id,
                             AccessTokenCallback callback);

  void OnManagedStatusChecked(int check_id);

  void OnAccessTokenFetched(int fetch_id,
                            GoogleServiceAuthError error,
                            signin::AccessTokenInfo access_token_info);

  const raw_ptr<signin::IdentityManager> identity_manager_;
  const raw_ptr<PrefService> pref_service_;
  const raw_ptr<enterprise::ProfileIdService> profile_id_service_;

  // In-flight account managed status checks.
  base::IDMap<std::unique_ptr<PendingManagedStatusCheck>>
      pending_status_checks_;

  // In-flight access token fetchers.
  base::IDMap<std::unique_ptr<PendingTokenFetch>> access_token_fetchers_;

  base::WeakPtrFactory<EnterpriseNetworkAuthService> weak_factory_{this};
};

}  // namespace enterprise_net

#endif  // COMPONENTS_ENTERPRISE_NET_CORE_ENTERPRISE_NETWORK_AUTH_SERVICE_H_
