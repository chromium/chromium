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
#include "components/legion/client_impl.h"
#include "components/legion/connection_factory_impl.h"
#include "components/legion/features.h"
#include "components/legion/phosphor/token_manager.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "url/gurl.h"

namespace legion {

// static
std::unique_ptr<Client> Client::CreateWithApiKey(
    const GURL& url,
    network::mojom::NetworkContext* network_context) {
  CHECK(base::FeatureList::IsEnabled(kLegion));

  auto connection_factory_impl =
      std::make_unique<ApiKeyConnectionFactoryImpl>(url, network_context);

  // Raw `new` is used here because the constructor is private.
  return base::WrapUnique(new ClientImpl(std::move(connection_factory_impl)));
}

// static
std::unique_ptr<Client> Client::CreateWithToken(
    const GURL& url,
    network::mojom::NetworkContext* network_context,
    phosphor::TokenManager* token_manager) {
  CHECK(base::FeatureList::IsEnabled(kLegion));

  auto connection_factory_impl = std::make_unique<TokenConnectionFactoryImpl>(
      url, network_context, token_manager);

  // Raw `new` is used here because the constructor is private.
  return base::WrapUnique(new ClientImpl(std::move(connection_factory_impl)));
}

// static
GURL Client::FormatUrl(const std::string& url) {
  return GURL(base::StrCat({"wss://", url}));
}

// static
GURL Client::FormatUrl(const std::string& url, const std::string& api_key) {
  return GURL(base::StrCat({"wss://", url, "?key=", api_key}));
}

}  // namespace legion
