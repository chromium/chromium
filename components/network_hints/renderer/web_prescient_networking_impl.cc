// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/network_hints/renderer/web_prescient_networking_impl.h"

#include "base/logging.h"
#include "content/public/renderer/render_frame.h"
#include "services/network/public/cpp/features.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"

namespace network_hints {
namespace {

void ForwardToHandler(mojo::Remote<mojom::NetworkHintsHandler>* handler,
                      const std::vector<std::string>& names) {
  std::vector<GURL> urls;
  for (const auto& name : names) {
    urls.emplace_back("http://" + name);
  }
  handler->get()->PrefetchDNS(urls);
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

void WebPrescientNetworkingImpl::PrefetchDNS(const blink::WebURL& url) {
  DVLOG(2) << "Prefetch DNS: " << url.GetString().Utf8();
  GURL gurl(url);
  if (!gurl.is_valid() || !gurl.has_host()) {
    return;
  }

  if (base::FeatureList::IsEnabled(network::features::kPrefetchDNSWithURL)) {
    std::vector<GURL> urls;
    urls.push_back(std::move(gurl));
    handler_->PrefetchDNS(urls);
    // TODO(jam): If this launches remove DnsQueue and RendererDnsPrefetch
    // which are no longer needed. They were from a feature which existed
    // at launch but not anymore that prefetched DNS for every link on a page.
  } else {
    auto host_piece = gurl.host_piece();
    dns_prefetch_.Resolve(host_piece.data(), host_piece.length());
  }
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
