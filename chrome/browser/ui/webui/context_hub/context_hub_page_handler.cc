// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/context_hub/context_hub_page_handler.h"

#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_contents.h"

ContextHubPageHandler::ContextHubPageHandler(
    mojo::PendingReceiver<browser::context_hub::mojom::PageHandler> receiver,
    Profile* profile,
    content::WebContents* web_contents)
    : receiver_(this, std::move(receiver)),
      profile_(profile),
      web_contents_(web_contents) {}

ContextHubPageHandler::~ContextHubPageHandler() = default;
