// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/network_hints/renderer/web_prescient_networking_impl.h"

#include "base/logging.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"

namespace network_hints {
namespace {

void ForwardToHandler(mojo::Remote<mojom::NetworkHintsHandler>* handler,
                      const std::vector<std::string>& names) {
  handler->get()->PrefetchDNS(names);
}

}  // namespace

WebPrescientNetworkingImpl::WebPrescientNetworkingImpl()
    : dns_prefetch_(
          base::BindRepeating(&ForwardToHandler, base::Unretained(&handler_))) {
  content::RenderThread::Get()->BindHostReceiver(
      handler_.BindNewPipeAndPassReceiver());
}

WebPrescientNetworkingImpl::~WebPrescientNetworkingImpl() {}

void WebPrescientNetworkingImpl::PrefetchDNS(const blink::WebString& hostname) {
  DVLOG(2) << "Prefetch DNS: " << hostname.Utf8();
  if (hostname.IsEmpty())
    return;

  std::string hostname_utf8 = hostname.Utf8();
  dns_prefetch_.Resolve(hostname_utf8.data(), hostname_utf8.length());
}

void WebPrescientNetworkingImpl::Preconnect(
    blink::WebLocalFrame* web_local_frame,
    const blink::WebURL& url,
    bool allow_credentials) {
  DVLOG(2) << "Preconnect: " << url.GetString().Utf8();
  if (!url.IsValid() || !web_local_frame)
    return;

  handler_->Preconnect(
      content::RenderFrame::FromWebFrame(web_local_frame)->GetRoutingID(), url,
      allow_credentials);
}

}  // namespace network_hints
