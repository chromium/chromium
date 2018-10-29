// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_network_request_interceptor.h"

#include "net/base/net_errors.h"

namespace chromecast {

CastNetworkRequestInterceptor::CastNetworkRequestInterceptor() = default;

CastNetworkRequestInterceptor::~CastNetworkRequestInterceptor() = default;

bool CastNetworkRequestInterceptor::IsWhiteListed(
    const GURL& /* gurl */,
    const std::string& /* session_id */,
    int /* render_process_id */,
    int /* render_frame_id */,
    bool /* for_device_auth */) const {
  return false;
}

void CastNetworkRequestInterceptor::Initialize() {}

bool CastNetworkRequestInterceptor::IsInitialized() {
  return true;
}

int CastNetworkRequestInterceptor::OnBeforeURLRequest(
    net::URLRequest* /* request */,
    const std::string& /* session_id */,
    int /* render_process_id */,
    int /* render_frame_id */,
    net::CompletionOnceCallback /* callback */,
    GURL* /* new_url */) {
  return net::OK;
}

int CastNetworkRequestInterceptor::OnBeforeStartTransaction(
    net::URLRequest* /* request */,
    net::CompletionOnceCallback /* callback */,
    net::HttpRequestHeaders* headers) {
  return net::OK;
}

void CastNetworkRequestInterceptor::OnURLRequestDestroyed(
    net::URLRequest* /* request */) {}

}  // namespace chromecast
