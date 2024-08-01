// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/privacy_sandbox/private_state_tokens/private_state_tokens_handler.h"

#include "mojo/public/cpp/bindings/pending_receiver.h"

PrivateStateTokensHandler::PrivateStateTokensHandler(
    mojo::PendingReceiver<
        private_state_tokens::mojom::PrivateStateTokensPageHandler> receiver)
    : receiver_(this, std::move(receiver)) {}

PrivateStateTokensHandler::~PrivateStateTokensHandler() = default;
