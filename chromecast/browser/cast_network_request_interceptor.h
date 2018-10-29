// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_CAST_NETWORK_REQUEST_INTERCEPTOR_H_
#define CHROMECAST_BROWSER_CAST_NETWORK_REQUEST_INTERCEPTOR_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/sequence_checker.h"
#include "net/base/completion_once_callback.h"

class GURL;

namespace net {
class HttpRequestHeaders;
class URLRequest;
}  // namespace net

namespace chromecast {

// Used to intercept the network request and modify the headers.
class CastNetworkRequestInterceptor {
 public:
  static std::unique_ptr<CastNetworkRequestInterceptor> Create();

  CastNetworkRequestInterceptor();
  virtual ~CastNetworkRequestInterceptor();

  // TODO(juke): Remove render_process_id.
  virtual bool IsWhiteListed(const GURL& gurl,
                             const std::string& session_id,
                             int render_process_id,
                             int render_frame_id,
                             bool for_device_auth) const;
  virtual void Initialize();
  virtual bool IsInitialized();

  virtual int OnBeforeURLRequest(net::URLRequest* request,
                                 const std::string& session_id,
                                 int render_process_id,
                                 int render_frame_id,
                                 net::CompletionOnceCallback callback,
                                 GURL* new_url);

  virtual int OnBeforeStartTransaction(net::URLRequest* request,
                                       net::CompletionOnceCallback callback,
                                       net::HttpRequestHeaders* headers);

  virtual void OnURLRequestDestroyed(net::URLRequest* request);

 private:
  DISALLOW_COPY_AND_ASSIGN(CastNetworkRequestInterceptor);
};
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_CAST_NETWORK_REQUEST_INTERCEPTOR_H_
