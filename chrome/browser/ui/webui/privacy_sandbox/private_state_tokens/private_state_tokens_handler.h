// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PRIVACY_SANDBOX_PRIVATE_STATE_TOKENS_PRIVATE_STATE_TOKENS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_PRIVACY_SANDBOX_PRIVATE_STATE_TOKENS_PRIVATE_STATE_TOKENS_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webui/privacy_sandbox/private_state_tokens/private_state_tokens.mojom.h"
#include "content/public/browser/web_ui.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/network/public/mojom/network_context.mojom.h"

class PrivateStateTokensHandler
    : public private_state_tokens::mojom::PrivateStateTokensPageHandler {
 public:
  explicit PrivateStateTokensHandler(
      content::WebUI* web_ui,
      mojo::PendingReceiver<
          private_state_tokens::mojom::PrivateStateTokensPageHandler> receiver);

  ~PrivateStateTokensHandler() override;

  PrivateStateTokensHandler(const PrivateStateTokensHandler&) = delete;
  PrivateStateTokensHandler& operator=(const PrivateStateTokensHandler&) =
      delete;
  void GetIssuerTokenCounts(GetIssuerTokenCountsCallback callback) override;

 private:
  network::mojom::NetworkContext* GetNetworkContext();

  raw_ptr<content::WebUI> web_ui_ = nullptr;
  mojo::Receiver<private_state_tokens::mojom::PrivateStateTokensPageHandler>
      receiver_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_PRIVACY_SANDBOX_PRIVATE_STATE_TOKENS_PRIVATE_STATE_TOKENS_HANDLER_H_
