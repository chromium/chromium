// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/legion/client.h"

#include <memory>
#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/strings/strcat.h"
#include "components/legion/attestation_handler_impl.h"
#include "components/legion/client_impl.h"
#include "components/legion/features.h"
#include "components/legion/secure_channel_impl.h"
#include "components/legion/secure_session_async_impl.h"
#include "components/legion/websocket_client.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "url/gurl.h"

namespace legion {

// static
std::unique_ptr<Client> Client::Create(
    network::mojom::NetworkContext* network_context) {
  return CreateWithUrl(FormatUrl(kLegionUrl.Get(), kLegionApiKey.Get()),
                       network_context);
}

// static
std::unique_ptr<Client> Client::CreateWithUrl(
    const GURL& url,
    network::mojom::NetworkContext* network_context) {
  CHECK(base::FeatureList::IsEnabled(kLegion));

  auto factory = base::BindRepeating(
      [](const GURL& url, network::mojom::NetworkContext* context)
          -> std::unique_ptr<SecureChannel> {
        auto transport = std::make_unique<WebSocketClient>(
            url,
            base::BindRepeating(
                [](network::mojom::NetworkContext* context) { return context; },
                base::Unretained(context)));
        auto secure_session = std::make_unique<SecureSessionAsyncImpl>();
        auto attestation_handler = std::make_unique<AttestationHandlerImpl>();

        return std::make_unique<SecureChannelImpl>(
            std::move(transport), std::move(secure_session),
            std::move(attestation_handler));
      },
      url, base::Unretained(network_context));

  // Raw `new` is used here because the constructor is private.
  return base::WrapUnique(new ClientImpl(std::move(factory)));
}

// static
GURL Client::FormatUrl(const std::string& url, const std::string& api_key) {
  return GURL(base::StrCat({"wss://", url, "?key=", api_key}));
}

}  // namespace legion
