// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/network_hints/renderer/web_prescient_networking_impl.h"

#include "base/logging.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"

namespace network_hints {

WebPrescientNetworkingImpl::WebPrescientNetworkingImpl(
    content::RenderFrame* render_frame) {
  render_frame->GetBrowserInterfaceBroker().GetInterface(
      handler_.BindNewPipeAndPassReceiver());
}

WebPrescientNetworkingImpl::~WebPrescientNetworkingImpl() = default;

void WebPrescientNetworkingImpl::PrefetchDNS(const blink::WebURL& url) {
  DVLOG(2) << "Prefetch DNS: " << url.GetString().Utf8();
  GURL gurl(url);
  if (!gurl.is_valid() || !gurl.has_host()) {
    return;
  }
  url::SchemeHostPort scheme_host_pair(gurl);

  std::vector<url::SchemeHostPort> urls;
  urls.push_back(std::move(scheme_host_pair));
  handler_->PrefetchDNS(urls);
}

void WebPrescientNetworkingImpl::Preconnect(
    const blink::WebURL& url,
    bool allow_credentials) {
  DVLOG(2) << "Preconnect: " << url.GetString().Utf8();
  if (!url.IsValid())
    return;

  GURL gurl(url);
  url::SchemeHostPort scheme_host_pair(gurl);
  handler_->Preconnect(scheme_host_pair, allow_credentials);
}

}  // namespace network_hints
