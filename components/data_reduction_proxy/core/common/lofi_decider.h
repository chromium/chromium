// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_LOFI_DECIDER_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_LOFI_DECIDER_H_

#include "base/macros.h"

namespace net {
class HttpRequestHeaders;
class URLRequest;
}

namespace data_reduction_proxy {

// Interface to determine if a request should be made for a low fidelity version
// of the resource.
class LoFiDecider {
 public:
  virtual ~LoFiDecider() {}

  // Adds a previews-specific directive to the Chrome-Proxy-Accept-Transform
  // header if needed.
  virtual void MaybeSetAcceptTransformHeader(
      const net::URLRequest& request,
      net::HttpRequestHeaders* headers) const = 0;

  // Unconditionally removes the Chrome-Proxy-Accept-Transform header from
  // |headers.|
  virtual void RemoveAcceptTransformHeader(
      net::HttpRequestHeaders* headers) const = 0;

  // Returns whether the request was a client-side Lo-Fi image request.
  virtual bool IsClientLoFiImageRequest(
      const net::URLRequest& request) const = 0;

  // Returns true if the request is for a client-side Lo-Fi image that is being
  // automatically reloaded because of a decoding error.
  virtual bool IsClientLoFiAutoReloadRequest(
      const net::URLRequest& request) const = 0;
};

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_LOFI_DECIDER_H_
