// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/custom_handlers/protocol_handler_throttle.h"

#include <string_view>

#include "components/custom_handlers/protocol_handler_registry.h"
#include "services/network/public/cpp/resource_request.h"

namespace custom_handlers {

ProtocolHandlerThrottle::ProtocolHandlerThrottle(
    ProtocolHandlerRegistry& protocol_handler_registry)
    : protocol_handler_registry_(protocol_handler_registry.GetWeakPtr()) {}

ProtocolHandlerThrottle::~ProtocolHandlerThrottle() = default;

void ProtocolHandlerThrottle::WillStartRequest(
    network::ResourceRequest* request,
    bool* defer) {
  TranslateUrl(request->url);
}

void ProtocolHandlerThrottle::WillRedirectRequest(
    net::RedirectInfo* redirect_info,
    const network::mojom::URLResponseHead& response_head,
    bool* defer,
    std::vector<std::string>* to_be_removed_headers,
    net::HttpRequestHeaders* modified_headers,
    net::HttpRequestHeaders* modified_cors_exempt_headers) {
  TranslateUrl(redirect_info->new_url);
}

void ProtocolHandlerThrottle::TranslateUrl(GURL& url) {
  if (protocol_handler_registry_ &&
      protocol_handler_registry_->IsHandledProtocol(url.scheme())) {
    GURL translated_url = protocol_handler_registry_->Translate(url);
    if (!translated_url.is_empty()) {
      url = translated_url;
    }
  }
}

}  // namespace custom_handlers
