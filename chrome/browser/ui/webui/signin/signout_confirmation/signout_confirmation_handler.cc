// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/signout_confirmation/signout_confirmation_handler.h"

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "chrome/browser/ui/browser.h"

SignoutConfirmationHandler::SignoutConfirmationHandler(
    mojo::PendingReceiver<signout_confirmation::mojom::PageHandler> receiver,
    mojo::PendingRemote<signout_confirmation::mojom::Page> page,
    Browser* browser,
    ChromeSignoutConfirmationPromptVariant variant,
    base::OnceCallback<void(ChromeSignoutConfirmationChoice)> callback)
    : browser_(browser->AsWeakPtr()),
      completion_callback_(std::move(callback)),
      receiver_(this, std::move(receiver)),
      page_(std::move(page)) {}

SignoutConfirmationHandler::~SignoutConfirmationHandler() = default;
