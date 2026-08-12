// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_INTRO_WELCOME_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_INTRO_WELCOME_HANDLER_H_

#include <optional>

#include "base/functional/callback.h"
#include "chrome/browser/ui/webui/intro/welcome.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

class WelcomeHandler : public intro::mojom::WelcomePageHandler {
 public:
  WelcomeHandler(
      base::OnceClosure callback,
      mojo::PendingReceiver<intro::mojom::WelcomePageHandler> receiver);

  WelcomeHandler(const WelcomeHandler&) = delete;
  WelcomeHandler& operator=(const WelcomeHandler&) = delete;

  ~WelcomeHandler() override;

  // intro::mojom::WelcomePageHandler:
  void Continue(std::optional<bool> is_uma_opt_in,
                std::optional<bool> is_default_browser) override;

 private:
  base::OnceClosure callback_;
  mojo::Receiver<intro::mojom::WelcomePageHandler> receiver_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_INTRO_WELCOME_HANDLER_H_

