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
#include "url/gurl.h"

namespace enterprise_net {

// KeyedService responsible for handling proxy errors and 407 Proxy
// Authentication challenges for managed Provisioning Domain dynamic routes.
class EnterpriseProxyErrorService : public KeyedService {
 public:
  explicit EnterpriseProxyErrorService(
      EnterpriseProxyService* enterprise_proxy_service);
  EnterpriseProxyErrorService(const EnterpriseProxyErrorService&) = delete;
  EnterpriseProxyErrorService& operator=(const EnterpriseProxyErrorService&) =
      delete;
  ~EnterpriseProxyErrorService() override;

  // Records a disguised proxy error for the specified navigation ID.
  void RecordDisguisedError(int64_t navigation_id,
                            EnterpriseProxyErrorData error_data);

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

  // Intercepts a 407 Proxy Authentication Required challenge.
  // Returns true if this challenge is handled by EnterpriseProxyErrorService
  // (either canceled due to a disguised error or credentials fetched).
  // Returns false if the challenge is not applicable to dynamic routes.
  bool InterceptProxyAuthChallenge(
      const net::AuthChallengeInfo& auth_info,
      const GURL& destination_url,
      const scoped_refptr<net::HttpResponseHeaders>& response_headers,
      int64_t navigation_id,
      base::OnceCallback<void(const std::optional<net::AuthCredentials>&)>
          callback);

 private:
  void RecordErrorCodeHistogram(int error_code) const;

  void MaybeRecordErrorForNavigation(
      int64_t navigation_id,
      const GURL& destination_url,
      const GURL& proxy_url,
      int error_code,
      EnterpriseProxyErrorData::ErrorCategory category);

  void OnProxyAuthChallengeResult(
      bool* handled_flag,
      int64_t navigation_id,
      const GURL& destination_url,
      const GURL& proxy_url,
      int error_code,
      base::OnceCallback<void(const std::optional<net::AuthCredentials>&)>
          coord_callback,
      EnterpriseProxyService::ProxyAuthChallengeResult result,
      const std::optional<net::AuthCredentials>& credentials);

  raw_ptr<EnterpriseProxyService> enterprise_proxy_service_ = nullptr;
  base::flat_map<int64_t, EnterpriseProxyErrorData> disguised_errors_;
  base::WeakPtrFactory<EnterpriseProxyErrorService> weak_ptr_factory_{this};
};

}  // namespace enterprise_net

#endif  // COMPONENTS_ENTERPRISE_NET_CORE_ENTERPRISE_PROXY_ERROR_SERVICE_H_
