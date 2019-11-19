// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_COMMON_CAST_URL_LOADER_THROTTLE_H_
#define CHROMECAST_COMMON_CAST_URL_LOADER_THROTTLE_H_

#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "net/http/http_request_headers.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"

namespace chromecast {

class CastURLLoaderThrottle : public blink::URLLoaderThrottle {
 public:
  // An interface for CastURLLoaderThrottle to modify the resource request,
  // possibly also defer the request (by returning net::IO_PENDING) in some
  // scenarios where blocking operations are needed.
  class Delegate {
   public:
    virtual int WillStartResourceRequest(
        network::ResourceRequest* request,
        const std::string& session_id,
        base::OnceCallback<void(int,
                                net::HttpRequestHeaders,
                                net::HttpRequestHeaders)> callback) = 0;

   protected:
    virtual ~Delegate() = default;
  };

  CastURLLoaderThrottle(Delegate* delegate, const std::string& session_id);
  ~CastURLLoaderThrottle() override;

 private:
  // blink::URLLoaderThrottle implementation:
  void DetachFromCurrentSequence() override;
  void WillStartRequest(network::ResourceRequest* request,
                        bool* defer) override;
  bool makes_unsafe_redirect() override;

  void ResumeRequest(int error,
                     net::HttpRequestHeaders headers,
                     net::HttpRequestHeaders cors_exempt_headers);

  bool deferred_ = false;
  Delegate* const settings_delegate_;
  const std::string session_id_;

  base::WeakPtr<CastURLLoaderThrottle> weak_this_;
  base::WeakPtrFactory<CastURLLoaderThrottle> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(CastURLLoaderThrottle);
};

}  // namespace chromecast

#endif  // CHROMECAST_COMMON_CAST_URL_LOADER_THROTTLE_H_
