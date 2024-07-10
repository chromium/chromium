// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/privacy_sandbox/related_website_sets/related_website_sets_handler.h"

#include "content/public/browser/web_ui.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

RelatedWebsiteSetsHandler::RelatedWebsiteSetsHandler(
    mojo::PendingReceiver<
        related_website_sets::mojom::RelatedWebsiteSetsPageHandler> receiver)
    : receiver_(this, std::move(receiver)) {}

RelatedWebsiteSetsHandler::~RelatedWebsiteSetsHandler() = default;
