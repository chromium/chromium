// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_NET_CORE_ENTERPRISE_PROXY_ERROR_SERVICE_H_
#define COMPONENTS_ENTERPRISE_NET_CORE_ENTERPRISE_PROXY_ERROR_SERVICE_H_

#include <optional>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
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

  // Intercepts a 407 Proxy Authentication Required challenge.
  // Returns true if this challenge is handled by EnterpriseProxyErrorService
  // (either canceled due to a disguised error or credentials fetched).
  // Returns false if the challenge is not applicable to dynamic routes.
  bool InterceptProxyAuthChallenge(
      const net::AuthChallengeInfo& auth_info,
      const GURL& destination_url,
      const scoped_refptr<net::HttpResponseHeaders>& response_headers,
      base::OnceCallback<void(const std::optional<net::AuthCredentials>&)>
          callback);

 private:
  void AttachDisguisedErrorData();

  void OnProxyAuthChallengeResult(
      bool* handled_flag,
      base::OnceCallback<void(const std::optional<net::AuthCredentials>&)>
          coord_callback,
      EnterpriseProxyService::ProxyAuthChallengeResult result,
      const std::optional<net::AuthCredentials>& credentials);

  raw_ptr<EnterpriseProxyService> enterprise_proxy_service_ = nullptr;
  base::WeakPtrFactory<EnterpriseProxyErrorService> weak_ptr_factory_{this};
};

}  // namespace enterprise_net

#endif  // COMPONENTS_ENTERPRISE_NET_CORE_ENTERPRISE_PROXY_ERROR_SERVICE_H_
