// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/common/cast_url_loader_throttle.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/resource_response.h"

namespace chromecast {

CastURLLoaderThrottle::CastURLLoaderThrottle(
    Delegate* delegate,
    const std::string& session_id)
    : settings_delegate_(delegate),
      session_id_(session_id),
      weak_factory_(this) {
  DCHECK(settings_delegate_);
  weak_this_ = weak_factory_.GetWeakPtr();
}

CastURLLoaderThrottle::~CastURLLoaderThrottle() = default;

void CastURLLoaderThrottle::DetachFromCurrentSequence() {}

void CastURLLoaderThrottle::WillStartRequest(
    network::ResourceRequest* request,
    bool* defer) {
  int error = settings_delegate_->WillStartResourceRequest(
      request, session_id_,
      base::BindOnce(&CastURLLoaderThrottle::ResumeRequest, weak_this_));
  if (error == net::ERR_IO_PENDING) {
    deferred_ = true;
    *defer = true;
  }
}

bool CastURLLoaderThrottle::makes_unsafe_redirect() {
  // Yes, this makes cross-scheme redirects.
  return true;
}

void CastURLLoaderThrottle::ResumeRequest(
    int error,
    net::HttpRequestHeaders headers,
    net::HttpRequestHeaders cors_exempt_headers) {
  DCHECK(deferred_);
  if (error != net::OK) {
    NOTREACHED() << "Trying to resume a request with unexpected error: "
                 << error;
    return;
  }
  deferred_ = false;
  delegate_->UpdateDeferredRequestHeaders(headers, cors_exempt_headers);
  delegate_->Resume();
}

}  // namespace chromecast
