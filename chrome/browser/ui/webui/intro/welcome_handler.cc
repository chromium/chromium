// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/intro/welcome_handler.h"

#include <optional>
#include <utility>

#include "base/check.h"

WelcomeHandler::WelcomeHandler(
    base::OnceClosure callback,
    mojo::PendingReceiver<intro::mojom::WelcomePageHandler> receiver)
    : callback_(std::move(callback)), receiver_(this, std::move(receiver)) {
  CHECK(callback_);
}

WelcomeHandler::~WelcomeHandler() = default;

void WelcomeHandler::Continue(std::optional<bool> is_uma_opt_in,
                              std::optional<bool> is_default_browser) {
  // TODO(crbug.com/542895787): Handle UMA opt-in and default browser state
  // choices.
  if (callback_) {
    std::move(callback_).Run();
  }
}
