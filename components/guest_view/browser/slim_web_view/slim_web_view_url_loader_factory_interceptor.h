// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GUEST_VIEW_BROWSER_SLIM_WEB_VIEW_SLIM_WEB_VIEW_URL_LOADER_FACTORY_INTERCEPTOR_H_
#define COMPONENTS_GUEST_VIEW_BROWSER_SLIM_WEB_VIEW_SLIM_WEB_VIEW_URL_LOADER_FACTORY_INTERCEPTOR_H_

#include "mojo/public/cpp/bindings/pending_remote.h"

namespace content {
class RenderFrameHost;
}

namespace network {
class URLLoaderFactoryBuilder;

namespace mojom {
class TrustedURLLoaderHeaderClient;
}
}  // namespace network

namespace guest_view {

// Inserts a proxy URLLoaderFactory into the factory builder, if the frame has a
// SlimWebViewGuest.
// This proxy implements the following features:
// - Blocks requests that do not match the allowed origins of the guest.
// - Intercepts requests that match the `before_send_headers_params` of the
// SlimWebViewGuest to inject additional headers.
void MaybeInterceptURLLoaderFactoryForSlimWebView(
    content::RenderFrameHost* render_frame_host,
    network::URLLoaderFactoryBuilder& factory_builder,
    mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>*
        header_client);

}  // namespace guest_view

#endif  // COMPONENTS_GUEST_VIEW_BROWSER_SLIM_WEB_VIEW_SLIM_WEB_VIEW_URL_LOADER_FACTORY_INTERCEPTOR_H_
