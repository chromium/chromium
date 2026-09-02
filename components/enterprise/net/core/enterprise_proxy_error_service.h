// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_NET_CORE_ENTERPRISE_PROXY_ERROR_SERVICE_H_
#define COMPONENTS_ENTERPRISE_NET_CORE_ENTERPRISE_PROXY_ERROR_SERVICE_H_

#include <memory>
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

class EnterpriseProxyErrorData;
class EnterpriseProxyService;

// KeyedService responsible for handling proxy errors and 407 Proxy
// Authentication challenges for managed Provisioning Domain dynamic routes.
class EnterpriseProxyErrorService : public KeyedService {
 public:
  // Delegate interface for platform-specific error marking and display.
  // Non-iOS platforms implement this using content::NavigationHandle, while
  // iOS platforms can implement this using WebKit / WKWebView classes.
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // TODO(crbug.com/543017119): Remove once disguised error page uses
    // NavigationID correlation instead of Delegate.
    virtual const EnterpriseProxyErrorData* GetDisguisedErrorData() const;
    virtual void AttachDisguisedErrorData(
        const EnterpriseProxyErrorData& error_data);
    // Called when proxy authentication requires the user to sign in or re-auth.
    virtual void OnSignInRequired(const GURL& destination_url);
  };

  explicit EnterpriseProxyErrorService(
      EnterpriseProxyService* enterprise_proxy_service);
  EnterpriseProxyErrorService(const EnterpriseProxyErrorService&) = delete;
  EnterpriseProxyErrorService& operator=(const EnterpriseProxyErrorService&) =
      delete;
  ~EnterpriseProxyErrorService() override;

  // Generates placeholder HTML for the special error page displaying
  // destination URL, proxy URL, and disguised error code.
  std::string GetErrorPageHTML(Delegate* delegate) const;

  // Intercepts a 407 Proxy Authentication Required challenge.
  // Returns true if this challenge is handled by EnterpriseProxyErrorService
  // (either canceled due to a disguised error or credentials fetched).
  // Returns false if the challenge is not applicable to dynamic routes.
  bool InterceptProxyAuthChallenge(
      const net::AuthChallengeInfo& auth_info,
      const GURL& destination_url,
      const scoped_refptr<net::HttpResponseHeaders>& response_headers,
      std::unique_ptr<Delegate> delegate,
      base::OnceCallback<void(const std::optional<net::AuthCredentials>&)>
          callback);

 private:
  std::string GetErrorPageHTML(
      const EnterpriseProxyErrorData& error_data) const;
  void OnProxyAuthChallengeResult(
      bool* handled_flag,
      std::unique_ptr<Delegate> delegate,
      const GURL& destination_url,
      const GURL& proxy_url,
      int error_code,
      base::OnceCallback<void(const std::optional<net::AuthCredentials>&)>
          coord_callback,
      EnterpriseProxyService::ProxyAuthChallengeResult result,
      const std::optional<net::AuthCredentials>& credentials);

  raw_ptr<EnterpriseProxyService> enterprise_proxy_service_ = nullptr;
  base::WeakPtrFactory<EnterpriseProxyErrorService> weak_ptr_factory_{this};
};

}  // namespace enterprise_net

#endif  // COMPONENTS_ENTERPRISE_NET_CORE_ENTERPRISE_PROXY_ERROR_SERVICE_H_
