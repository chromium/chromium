// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_INTERCEPTOR_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_INTERCEPTOR_H_

#include <memory>

#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers.h"
#include "net/url_request/url_request_interceptor.h"

namespace data_reduction_proxy {
class DataReductionProxyBypassProtocol;
class DataReductionProxyBypassStats;
class DataReductionProxyConfig;
class DataReductionProxyConfigServiceClient;

// Used to intercept responses that contain explicit and implicit signals
// to bypass the Data Reduction Proxy. If the proxy should be bypassed,
// the interceptor returns a new URLRequestHTTPJob that fetches the resource
// without use of the proxy.
class DataReductionProxyInterceptor : public net::URLRequestInterceptor {
 public:
  // Constructs the interceptor. |config|, |config_service_client|, and |stats|
  //  must outlive |this|. |stats| and |config_service_client|
  // may be NULL.
  DataReductionProxyInterceptor(
      DataReductionProxyConfig* config,
      DataReductionProxyConfigServiceClient* config_service_client,
      DataReductionProxyBypassStats* stats);

  // Destroys the interceptor.
  ~DataReductionProxyInterceptor() override;

  // Overrides from net::URLRequestInterceptor:
  net::URLRequestJob* MaybeInterceptRequest(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override;

  // Returns a new URLRequestHTTPJob if the redirect indicates that the Data
  // Reduction Proxy should be bypassed. See |MaybeInterceptResponseOrRedirect|
  // for more details.
  net::URLRequestJob* MaybeInterceptRedirect(
      net::URLRequest* request, net::NetworkDelegate* network_delegate,
      const GURL& location) const override;

  // Returns a new URLRequestHTTPJob if the response indicates that the Data
  // Reduction Proxy should be bypassed. See |MaybeInterceptResponseOrRedirect|
  // for more details.
  net::URLRequestJob* MaybeInterceptResponse(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override;

 private:
  // Returns a new URLRequestHTTPJob if the response or redirect indicates that
  // the data reduction proxy should be bypassed according to the rules in
  // |bypass_protocol_|. Returns NULL otherwise. If a job is returned, the
  // interceptor's URLRequestInterceptingJobFactory will restart the request.
  net::URLRequestJob* MaybeInterceptResponseOrRedirect(
      net::URLRequest* request, net::NetworkDelegate* network_delegate) const;

  // Must outlive |this| if non-NULL.
  DataReductionProxyBypassStats* bypass_stats_;

  // Must outlive |this| if non-NULL.
  DataReductionProxyConfigServiceClient* config_service_client_;

  // Object responsible for identifying cases when a response should cause the
  // data reduction proxy to be bypassed, and for triggering proxy bypasses in
  // these cases.
  std::unique_ptr<DataReductionProxyBypassProtocol> bypass_protocol_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(DataReductionProxyInterceptor);
};

}  // namespace data_reduction_proxy
#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_INTERCEPTOR_H_
