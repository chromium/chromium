// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_NET_CORE_ENTERPRISE_PROXY_ERROR_SERVICE_H_
#define COMPONENTS_ENTERPRISE_NET_CORE_ENTERPRISE_PROXY_ERROR_SERVICE_H_

#include <stdint.h>

#include <optional>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "components/enterprise/net/core/enterprise_proxy_error_data.h"
#include "components/enterprise/net/core/enterprise_proxy_service.h"
#include "components/keyed_service/core/keyed_service.h"
#include "net/base/auth.h"
#include "net/http/http_response_headers.h"
#include "net/log/net_log_with_source.h"
#include "url/gurl.h"

namespace enterprise_net {

// KeyedService responsible for handling proxy errors and 407 Proxy
// Authentication challenges for managed Provisioning Domain dynamic routes.
class EnterpriseProxyErrorService : public KeyedService {
 public:
  using ProxyAuthCredentialsCallback =
      base::OnceCallback<void(const std::optional<net::AuthCredentials>&)>;

  // A 407 challenge that this service has committed to handling.
  //
  // Move-only, and only this service can mint one, so an interception can be
  // resolved at most once.
  class PendingInterception {
   public:
    PendingInterception(PendingInterception&&);
    PendingInterception& operator=(PendingInterception&&);
    PendingInterception(const PendingInterception&) = delete;
    PendingInterception& operator=(const PendingInterception&) = delete;
    ~PendingInterception();

   private:
    friend class EnterpriseProxyErrorService;

    PendingInterception(EnterpriseProxyService::ProxyAuthChallengeMatch match,
                        int64_t navigation_id,
                        GURL destination_url,
                        GURL proxy_url,
                        int error_code);

    EnterpriseProxyService::ProxyAuthChallengeMatch match_;
    int64_t navigation_id_;
    GURL destination_url_;
    GURL proxy_url_;
    int error_code_;
  };

  explicit EnterpriseProxyErrorService(
      EnterpriseProxyService* enterprise_proxy_service);
  EnterpriseProxyErrorService(const EnterpriseProxyErrorService&) = delete;
  EnterpriseProxyErrorService& operator=(const EnterpriseProxyErrorService&) =
      delete;
  ~EnterpriseProxyErrorService() override;

  // Records a disguised proxy error for the specified navigation ID.
  void RecordDisguisedError(int64_t navigation_id,
                            EnterpriseProxyErrorData error_data,
                            const net::NetLogWithSource& net_log);

  // Retrieves and removes the recorded disguised proxy error for the navigation
  // ID.
  std::optional<EnterpriseProxyErrorData> TakeDisguisedError(
      int64_t navigation_id);

  // Removes any recorded disguised proxy error for the navigation ID.
  void RemoveDisguisedError(int64_t navigation_id);

  // Populates template parameters for the enterprise proxy alternative error
  // page (destination URL, proxy URL, and disguised error code).
  base::DictValue GetErrorPageParams(
      const EnterpriseProxyErrorData& error_data) const;

  // Evaluates a 407 Proxy Authentication Required challenge.
  //
  // Returns std::nullopt if the challenge is not applicable to managed dynamic
  // routes, in which case the caller should fall back to its default auth
  // handling.
  //
  // Otherwise returns a handle that the caller must pass to
  // `ResolveProxyAuthChallenge()` to resolve the challenge.
  [[nodiscard]] std::optional<PendingInterception> EvaluateProxyAuthChallenge(
      const net::AuthChallengeInfo& auth_info,
      const GURL& destination_url,
      const scoped_refptr<net::HttpResponseHeaders>& response_headers,
      int64_t navigation_id);

  // Resolves an interception, running `callback` exactly once -- immediately
  // for the fail-closed decisions, or later once an access token has been
  // fetched.
  //
  // `callback` still runs if the underlying EnterpriseProxyService is shut down
  // or destroyed mid-fetch. It is dropped only if *this* service is destroyed
  // first, which KeyedService teardown ordering prevents; see
  // OnProxyAuthCredentialsFetched().
  //
  // A null `callback` is a no-op: `interception` is discarded.
  void ResolveProxyAuthChallenge(PendingInterception interception,
                                 ProxyAuthCredentialsCallback callback);

 private:
  void RecordErrorCodeHistogram(int error_code) const;

  void MaybeRecordErrorForNavigation(
      int64_t navigation_id,
      const GURL& destination_url,
      const GURL& proxy_url,
      int error_code,
      EnterpriseProxyErrorData::ErrorCategory category,
      const net::NetLogWithSource& net_log);

  // Continuation for the asynchronous credential fetch.
  void OnProxyAuthCredentialsFetched(
      int64_t navigation_id,
      const GURL& destination_url,
      const GURL& proxy_url,
      int error_code,
      ProxyAuthCredentialsCallback callback,
      EnterpriseProxyService::CredentialFetchOutcome outcome,
      const std::optional<net::AuthCredentials>& credentials,
      const net::NetLogWithSource& net_log);

  raw_ptr<EnterpriseProxyService> enterprise_proxy_service_ = nullptr;
  base::flat_map<int64_t, EnterpriseProxyErrorData> disguised_errors_;
  base::WeakPtrFactory<EnterpriseProxyErrorService> weak_ptr_factory_{this};
};

}  // namespace enterprise_net

#endif  // COMPONENTS_ENTERPRISE_NET_CORE_ENTERPRISE_PROXY_ERROR_SERVICE_H_
