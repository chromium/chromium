// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/common/cast_url_loader_throttle.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/check.h"
#include "base/notreached.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/resource_request.h"

namespace chromecast {

CastURLLoaderThrottle::CastURLLoaderThrottle(scoped_refptr<Delegate> delegate,
                                             const std::string& session_id)
    : settings_delegate_(std::move(delegate)),
      session_id_(session_id),
      weak_factory_(this) {
  weak_this_ = weak_factory_.GetWeakPtr();
}

CastURLLoaderThrottle::~CastURLLoaderThrottle() = default;

void CastURLLoaderThrottle::DetachFromCurrentSequence() {}

void CastURLLoaderThrottle::WillStartRequest(
    network::ResourceRequest* request,
    bool* defer) {
  // Although unlikely, but there might be some weird edge case where `delegate`
  // passed in the constructor is nullptr.
  if (!settings_delegate_) {
    return;
  }
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
