// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/privacy_sandbox/private_state_tokens/private_state_tokens_handler.h"

#include <iostream>

#include "base/functional/bind.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/trust_tokens.mojom.h"
#include "url/gurl.h"

using private_state_tokens::mojom::IssuerTokenCount;

namespace {

void SendTrustTokens(
    PrivateStateTokensHandler::GetIssuerTokenCountsCallback callback,
    std::vector<::network::mojom::StoredTrustTokensForIssuerPtr> tokens) {
  std::vector<private_state_tokens::mojom::IssuerTokenCountPtr> result;
  result.reserve(tokens.size());
  for (auto const& token : tokens) {
    result.emplace_back(
        IssuerTokenCount::New(token->issuer.Serialize(), token->count));
  }
  std::move(callback).Run(std::move(result));
}

}  // namespace

network::mojom::NetworkContext* PrivateStateTokensHandler::GetNetworkContext() {
  return web_ui_->GetWebContents()
      ->GetBrowserContext()
      ->GetDefaultStoragePartition()
      ->GetNetworkContext();
}

void PrivateStateTokensHandler::GetIssuerTokenCounts(
    GetIssuerTokenCountsCallback callback) {
  GetNetworkContext()->GetStoredTrustTokenCounts(
      base::BindOnce(&SendTrustTokens, std::move(callback)));
  // TODO(crbug.com/348590926): refactor this method to structure data in a
  // form that matches the test data in
  // https://source.chromium.org/chromium/chromium/src/+/main:chrome/test/data/webui/privacy_sandbox/internals/private_state_tokens/test_data.ts
}

PrivateStateTokensHandler::PrivateStateTokensHandler(
    content::WebUI* web_ui,
    mojo::PendingReceiver<
        private_state_tokens::mojom::PrivateStateTokensPageHandler> receiver)
    : web_ui_(web_ui), receiver_(this, std::move(receiver)) {}

PrivateStateTokensHandler::~PrivateStateTokensHandler() = default;
