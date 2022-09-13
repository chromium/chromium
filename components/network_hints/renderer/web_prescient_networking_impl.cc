// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/network_hints/renderer/web_prescient_networking_impl.h"

#include "base/logging.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"

namespace network_hints {
namespace {

void ForwardToHandler(mojo::Remote<mojom::NetworkHintsHandler>* handler,
                      const std::vector<std::string>& names) {
  handler->get()->PrefetchDNS(names);
}

}  // namespace

WebPrescientNetworkingImpl::WebPrescientNetworkingImpl(
    content::RenderFrame* render_frame)
    : dns_prefetch_(
          base::BindRepeating(&ForwardToHandler, base::Unretained(&handler_))) {
  render_frame->GetBrowserInterfaceBroker()->GetInterface(
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
    const blink::WebURL& url,
    bool allow_credentials) {
  DVLOG(2) << "Preconnect: " << url.GetString().Utf8();
  if (!url.IsValid())
    return;

  handler_->Preconnect(url, allow_credentials);
}

}  // namespace network_hints
