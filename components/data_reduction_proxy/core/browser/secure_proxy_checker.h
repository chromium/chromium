// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_SECURE_PROXY_CHECKER_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_SECURE_PROXY_CHECKER_H_

#include <string>
#include <utility>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"

namespace net {
struct RedirectInfo;
}  // namespace net

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

namespace data_reduction_proxy {

typedef base::RepeatingCallback<void(const std::string&, int, int)>
    SecureProxyCheckerCallback;

// Checks if the secure proxy is allowed by the carrier by sending a probe.
class SecureProxyChecker {
 public:
  explicit SecureProxyChecker(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  virtual ~SecureProxyChecker();

  void CheckIfSecureProxyIsAllowed(SecureProxyCheckerCallback fetcher_callback);

 private:
  void OnURLLoadComplete(std::unique_ptr<std::string> response_body);
  void OnURLLoadCompleteOrRedirect(const std::string& response,
                                   int net_error,
                                   int response_code);

  void OnURLLoaderRedirect(const net::RedirectInfo& redirect_info,
                           const network::mojom::URLResponseHead& response_head,
                           std::vector<std::string>* to_be_removed_headers);

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // The URLLoader being used for the secure proxy check.
  std::unique_ptr<network::SimpleURLLoader> url_loader_;
  SecureProxyCheckerCallback fetcher_callback_;

  // Used to determine the latency in performing the Data Reduction Proxy secure
  // proxy check.
  base::Time secure_proxy_check_start_time_;

  DISALLOW_COPY_AND_ASSIGN(SecureProxyChecker);
};

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_SECURE_PROXY_CHECKER_H_
