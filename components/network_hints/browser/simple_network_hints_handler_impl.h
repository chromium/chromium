// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NETWORK_HINTS_BROWSER_SIMPLE_NETWORK_HINTS_HANDLER_IMPL_H_
#define COMPONENTS_NETWORK_HINTS_BROWSER_SIMPLE_NETWORK_HINTS_HANDLER_IMPL_H_

#include "components/network_hints/common/network_hints.mojom.h"

namespace content {
class RenderFrameHost;
}

namespace network_hints {

// Simple browser-side handler for DNS prefetch requests.
// Each renderer process requires its own filter.
class SimpleNetworkHintsHandlerImpl : public mojom::NetworkHintsHandler {
 public:
  SimpleNetworkHintsHandlerImpl(int render_process_id, int render_frame_id);

  SimpleNetworkHintsHandlerImpl(const SimpleNetworkHintsHandlerImpl&) = delete;
  SimpleNetworkHintsHandlerImpl& operator=(
      const SimpleNetworkHintsHandlerImpl&) = delete;

  ~SimpleNetworkHintsHandlerImpl() override;

  static void Create(
      content::RenderFrameHost* frame_host,
      mojo::PendingReceiver<mojom::NetworkHintsHandler> receiver);

  // mojom::NetworkHintsHandler methods:
  void PrefetchDNS(const std::vector<url::SchemeHostPort>& urls) override;
  void Preconnect(const url::SchemeHostPort& url,
                  bool allow_credentials) override;

 private:
  const int render_process_id_;
  const int render_frame_id_;
};

}  // namespace network_hints

#endif  // COMPONENTS_NETWORK_HINTS_BROWSER_SIMPLE_NETWORK_HINTS_HANDLER_IMPL_H_
