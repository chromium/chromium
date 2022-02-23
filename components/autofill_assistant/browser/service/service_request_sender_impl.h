// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SERVICE_SERVICE_REQUEST_SENDER_IMPL_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SERVICE_SERVICE_REQUEST_SENDER_IMPL_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill_assistant/browser/service/access_token_fetcher.h"
#include "components/autofill_assistant/browser/service/cup_factory.h"
#include "components/autofill_assistant/browser/service/service_request_sender.h"
#include "components/autofill_assistant/browser/service/simple_url_loader_factory.h"
#include "content/public/browser/browser_context.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace autofill_assistant {

class ServiceRequestSenderImpl : public ServiceRequestSender {
 public:
  // Constructor. |access_token_fetcher| is optional if |auth_enabled| is false.
  // Pointers to |context| and, if provided, |access_token_fetcher| must remain
  // valid during the lifetime of this instance.
  // If |disable_auth_if_no_access_token| is true, authentication will
  // automatically be disabled in case fetching the access token fails.
  ServiceRequestSenderImpl(
      content::BrowserContext* context,
      AccessTokenFetcher* access_token_fetcher,
      std::unique_ptr<cup::CUPFactory> cup_factory,
      std::unique_ptr<SimpleURLLoaderFactory> loader_factory,
      const std::string& api_key,
      bool auth_enabled,
      bool disable_auth_if_no_access_token);
  ~ServiceRequestSenderImpl() override;
  ServiceRequestSenderImpl(const ServiceRequestSenderImpl&) = delete;
  ServiceRequestSenderImpl& operator=(const ServiceRequestSenderImpl&) = delete;

  // Sends |request_body| to |url|. Depending on configuration, the request
  // will be authenticated either with an Oauth access token or the api key. The
  // |rpc_type| will be used to decide whether to use CUP verification. Returns
  // the http status code and the response itself. If the returned http headers
  // could not be parsed, the http code will be 0.
  //
  // When an auth-request first fails with a 401, the access token is
  // invalidated and fetched again. If the request fails again, the request
  // is considered failed and the callback is invoked.
  void SendRequest(const GURL& url,
                   const std::string& request_body,
                   ResponseCallback callback,
                   RpcType rpc_type) override;

 private:
  // Unlike |ServiceRequestSenderImpl::SendRequest|, assumes that any necessary
  // CUP signing and validation is already done or accounted for in the
  // |callback|.
  void InternalSendRequest(const GURL& url,
                           const std::string& request_body,
                           ResponseCallback callback);

  void SendRequestAuth(const GURL& url,
                       const std::string& request_body,
                       const std::string& access_token,
                       ResponseCallback callback);

  void RetryIfUnauthorized(const GURL& url,
                           const std::string& access_token,
                           const std::string& request_body,
                           ResponseCallback callback,
                           int http_status,
                           const std::string& response);

  void OnFetchAccessToken(GURL url,
                          std::string request_body,
                          ResponseCallback callback,
                          bool access_token_fetched,
                          const std::string& access_token);

  raw_ptr<content::BrowserContext> context_ = nullptr;
  raw_ptr<AccessTokenFetcher> access_token_fetcher_ = nullptr;
  std::unique_ptr<cup::CUPFactory> cup_factory_;
  std::unique_ptr<SimpleURLLoaderFactory> loader_factory_;

  // API key to add to the URL of unauthenticated requests.
  std::string api_key_;

  // Whether requests should be authenticated.
  bool auth_enabled_ = true;

  bool disable_auth_if_no_access_token_ = true;
  bool retried_with_fresh_access_token_ = false;
  base::WeakPtrFactory<ServiceRequestSenderImpl> weak_ptr_factory_{this};
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SERVICE_SERVICE_REQUEST_SENDER_IMPL_H_
