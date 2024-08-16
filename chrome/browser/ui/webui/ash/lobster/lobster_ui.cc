// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/lobster/lobster_ui.h"

#include "chrome/browser/ash/lobster/lobster_service_provider.h"
#include "chrome/browser/profiles/profile.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace ash {

LobsterUI::LobsterUI(content::WebUI* web_ui)
    : UntrustedTopChromeWebUIController(web_ui),
      page_handler_(std::make_unique<LobsterPageHandler>(
          LobsterServiceProvider::GetForProfile(Profile::FromWebUI(web_ui))
              ->active_session())) {
  // TODO(b/348281154): Initialize WebUI container and show to the user.
}

LobsterUI::~LobsterUI() = default;

}  // namespace ash
