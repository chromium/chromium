// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_CAST_NETWORK_DELEGATE_H_
#define CHROMECAST_BROWSER_CAST_NETWORK_DELEGATE_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "net/base/network_delegate_impl.h"

namespace chromecast {

class CastNetworkRequestInterceptor;

namespace shell {

class CastNetworkDelegate : public net::NetworkDelegateImpl {
 public:
  static std::unique_ptr<CastNetworkDelegate> Create();

  explicit CastNetworkDelegate(std::unique_ptr<CastNetworkRequestInterceptor>
                                   network_request_interceptor);
  ~CastNetworkDelegate() override;

  void Initialize();

  bool IsWhitelisted(const GURL& gurl,
                     const std::string& session_id,
                     int render_process_id,
                     int render_frame_id,
                     bool for_device_auth) const;

 private:
  // net::NetworkDelegate implementation:
  int OnBeforeURLRequest(net::URLRequest* request,
                         net::CompletionOnceCallback callback,
                         GURL* new_url) override;
  int OnBeforeStartTransaction(net::URLRequest* request,
                               net::CompletionOnceCallback callback,
                               net::HttpRequestHeaders* headers) override;
  void OnURLRequestDestroyed(net::URLRequest* request) override;

  const std::unique_ptr<CastNetworkRequestInterceptor>
      network_request_interceptor_;

  DISALLOW_COPY_AND_ASSIGN(CastNetworkDelegate);
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_CAST_NETWORK_DELEGATE_H_
