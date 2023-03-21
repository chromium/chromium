// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NETWORK_HINTS_RENDERER_WEB_PRESCIENT_NETWORKING_IMPL_H_
#define COMPONENTS_NETWORK_HINTS_RENDERER_WEB_PRESCIENT_NETWORKING_IMPL_H_

#include "components/network_hints/common/network_hints.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/platform/web_prescient_networking.h"

namespace content {
class RenderFrame;
}

namespace network_hints {

// The main entry point from blink for sending DNS prefetch requests to the
// network stack.
class WebPrescientNetworkingImpl : public blink::WebPrescientNetworking {
 public:
  explicit WebPrescientNetworkingImpl(content::RenderFrame* render_frame);

  WebPrescientNetworkingImpl(const WebPrescientNetworkingImpl&) = delete;
  WebPrescientNetworkingImpl& operator=(const WebPrescientNetworkingImpl&) =
      delete;

  ~WebPrescientNetworkingImpl() override;

  // blink::WebPrescientNetworking methods:
  void PrefetchDNS(const blink::WebURL& url) override;
  void Preconnect(const blink::WebURL& url, bool allow_credentials) override;

 private:
  mojo::Remote<mojom::NetworkHintsHandler> handler_;
};

}  // namespace network_hints

#endif  // COMPONENTS_NETWORK_HINTS_RENDERER_WEB_PRESCIENT_NETWORKING_IMPL_H_
