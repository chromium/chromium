// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_BYPASS_PROTOCOL_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_BYPASS_PROTOCOL_H_

#include "base/macros.h"
#include "base/optional.h"
#include "base/threading/thread_checker.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers.h"
#include "net/base/net_errors.h"

namespace data_reduction_proxy {

class DataReductionProxyConfig;
struct DataReductionProxyTypeInfo;

// Class responsible for determining when a response should or should not cause
// the data reduction proxy to be bypassed, and to what degree. Owned by the
// DataReductionProxyInterceptor.
class DataReductionProxyBypassProtocol {
 public:
  // Enum values that can be reported for the
  // DataReductionProxy.ResponseProxyServerStatus histogram. These values must
  // be kept in sync with their counterparts in histograms.xml. Visible here for
  // testing purposes.
  enum ResponseProxyServerStatus {
    RESPONSE_PROXY_SERVER_STATUS_EMPTY = 0,
    RESPONSE_PROXY_SERVER_STATUS_DRP,
    RESPONSE_PROXY_SERVER_STATUS_NON_DRP_NO_VIA,
    RESPONSE_PROXY_SERVER_STATUS_NON_DRP_WITH_VIA,
    RESPONSE_PROXY_SERVER_STATUS_MAX
  };

  // Constructs a DataReductionProxyBypassProtocol object. |config| must be
  // non-NULL and outlive |this|.
  explicit DataReductionProxyBypassProtocol(DataReductionProxyConfig* config);

  ~DataReductionProxyBypassProtocol();

  // Decides whether to restart the request, whether to bypass proxies when
  // doing so, and whether to mark any data reduction proxies as bad based on
  // the response.
  // Returns true if the request should be retried. Sets
  // |should_bypass_proxy_and_cache| if when restarting the request it should
  // completely bypass proxies (e.g., in response to "block-once"). Fills
  // |bad_proxies| with the list of proxies to mark as bad.
  bool MaybeBypassProxyAndPrepareToRetry(
      const std::string& method,
      const std::vector<GURL>& url_chain,
      const net::HttpResponseHeaders* response_headers,
      const net::ProxyServer& proxy_server,
      net::Error net_error,
      const net::ProxyRetryInfoMap& proxy_retry_info,
      DataReductionProxyBypassType* proxy_bypass_type,
      DataReductionProxyInfo* data_reduction_proxy_info,
      std::vector<net::ProxyServer>* bad_proxies,
      bool* should_bypass_proxy_and_cache);

 private:
  // Decides whether to mark the data reduction proxy as temporarily bad and
  // put it on the proxy retry map. Returns true if the request should be
  // retried. Should be called only when the response had null response headers.
  bool HandleInvalidResponseHeadersCase(
      const std::vector<GURL>& url_chain,
      net::Error net_error,
      const base::Optional<DataReductionProxyTypeInfo>&
          data_reduction_proxy_type_info,
      DataReductionProxyInfo* data_reduction_proxy_info,
      DataReductionProxyBypassType* bypass_type) const;

  // Decides whether to mark the data reduction proxy as temporarily bad and
  // put it on the proxy retry map. Returns true if the request should be
  // retried. Should be called only when the response had non-null response
  // headers.
  bool HandleValidResponseHeadersCase(
      const std::vector<GURL>& url_chain,
      const net::HttpResponseHeaders* response_headers,
      const net::ProxyRetryInfoMap& proxy_retry_info,
      const DataReductionProxyTypeInfo& data_reduction_proxy_type_info,
      DataReductionProxyBypassType* proxy_bypass_type,
      DataReductionProxyInfo* data_reduction_proxy_info,
      DataReductionProxyBypassType* bypass_type) const;

  // Must outlive |this|.
  DataReductionProxyConfig* config_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(DataReductionProxyBypassProtocol);
};

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_BYPASS_PROTOCOL_H_
