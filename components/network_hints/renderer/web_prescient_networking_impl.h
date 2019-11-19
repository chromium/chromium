// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NETWORK_HINTS_RENDERER_WEB_PRESCIENT_NETWORKING_IMPL_H_
#define COMPONENTS_NETWORK_HINTS_RENDERER_WEB_PRESCIENT_NETWORKING_IMPL_H_

#include "base/macros.h"
#include "components/network_hints/common/network_hints.mojom.h"
#include "components/network_hints/renderer/renderer_dns_prefetch.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/platform/web_prescient_networking.h"

namespace network_hints {

// The main entry point from blink for sending DNS prefetch requests to the
// network stack.
class WebPrescientNetworkingImpl : public blink::WebPrescientNetworking {
 public:
  WebPrescientNetworkingImpl();
  ~WebPrescientNetworkingImpl() override;

  // blink::WebPrescientNetworking methods:
  void PrefetchDNS(const blink::WebString& hostname) override;
  void Preconnect(blink::WebLocalFrame* web_local_frame,
                  const blink::WebURL& url,
                  const bool allow_credentials) override;

 private:
  mojo::Remote<mojom::NetworkHintsHandler> handler_;
  RendererDnsPrefetch dns_prefetch_;

  DISALLOW_COPY_AND_ASSIGN(WebPrescientNetworkingImpl);
};

}  // namespace network_hints

#endif  // COMPONENTS_NETWORK_HINTS_RENDERER_WEB_PRESCIENT_NETWORKING_IMPL_H_
