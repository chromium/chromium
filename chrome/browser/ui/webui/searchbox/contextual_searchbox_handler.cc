// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/searchbox/contextual_searchbox_handler.h"

ContextualSearchboxHandler::ContextualSearchboxHandler(
    mojo::PendingReceiver<searchbox::mojom::PageHandler> pending_page_handler,
    Profile* profile,
    content::WebContents* web_contents,
    MetricsReporter* metrics_reporter,
    OmniboxController* omnibox_controller)
    : SearchboxHandler(std::move(pending_page_handler),
                       profile,
                       web_contents,
                       metrics_reporter) {}

ContextualSearchboxHandler::~ContextualSearchboxHandler() = default;
