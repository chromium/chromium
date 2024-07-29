// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PRIVACY_SANDBOX_PRIVATE_STATE_TOKENS_PRIVATE_STATE_TOKENS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_PRIVACY_SANDBOX_PRIVATE_STATE_TOKENS_PRIVATE_STATE_TOKENS_HANDLER_H_

#include "chrome/browser/ui/webui/privacy_sandbox/private_state_tokens/private_state_tokens.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"

class PrivateStateTokensHandler
    : public private_state_tokens::mojom::PrivateStateTokensPageHandler {
 public:
  explicit PrivateStateTokensHandler(
      mojo::PendingReceiver<
          private_state_tokens::mojom::PrivateStateTokensPageHandler> receiver);

  ~PrivateStateTokensHandler() override;

  PrivateStateTokensHandler(const PrivateStateTokensHandler&) = delete;
  PrivateStateTokensHandler& operator=(const PrivateStateTokensHandler&) =
      delete;

 private:
  mojo::Receiver<private_state_tokens::mojom::PrivateStateTokensPageHandler>
      receiver_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_PRIVACY_SANDBOX_PRIVATE_STATE_TOKENS_PRIVATE_STATE_TOKENS_HANDLER_H_
