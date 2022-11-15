// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CUSTOM_HANDLERS_PROTOCOL_HANDLER_THROTTLE_H_
#define COMPONENTS_CUSTOM_HANDLERS_PROTOCOL_HANDLER_THROTTLE_H_

#include "base/memory/ref_counted.h"
#include "url/gurl.h"

#include "third_party/blink/public/common/loader/url_loader_throttle.h"

namespace custom_handlers {

class ProtocolHandlerRegistry;

class ProtocolHandlerThrottle : public blink::URLLoaderThrottle {
 public:
  explicit ProtocolHandlerThrottle(ProtocolHandlerRegistry&);
  ~ProtocolHandlerThrottle() override;

  void WillStartRequest(network::ResourceRequest* request,
                        bool* defer) override;

  void WillRedirectRequest(
      net::RedirectInfo* redirect_info,
      const network::mojom::URLResponseHead& response_head,
      bool* defer,
      std::vector<std::string>* to_be_removed_headers,
      net::HttpRequestHeaders* modified_headers,
      net::HttpRequestHeaders* modified_cors_exempt_headers) override;

 private:
  // If the @url has a custom protocol handler, the argument will return the
  // translated url.
  void TranslateUrl(GURL& url);
  // The ProtocolHandlerRegistry instance is a KeyedService which ownership is
  // managed by the BrowserContext. BrowserContext can be destroyed before this
  // throttle.
  base::WeakPtr<ProtocolHandlerRegistry> protocol_handler_registry_;
};

}  // namespace custom_handlers

#endif  // COMPONENTS_CUSTOM_HANDLERS_PROTOCOL_HANDLER_THROTTLE_H_
