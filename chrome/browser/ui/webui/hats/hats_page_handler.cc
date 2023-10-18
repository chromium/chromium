// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/hats/hats_page_handler.h"

#include <string>

#include "google_apis/google_api_keys.h"

HatsPageHandler::HatsPageHandler(
    mojo::PendingReceiver<hats::mojom::PageHandler> receiver,
    mojo::PendingRemote<hats::mojom::Page> page)
    : receiver_(this, std::move(receiver)), page_(std::move(page)) {}

HatsPageHandler::~HatsPageHandler() = default;

// Triggered by getApiKey() call in TS; sends a response back to the renderer.
void HatsPageHandler::GetApiKey(GetApiKeyCallback callback) {
  std::move(callback).Run(google_apis::GetHatsAPIKey());
}
