// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/searchbox/searchbox_handler.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/metrics_reporter/metrics_reporter.h"
#include "components/omnibox/browser/omnibox_controller.h"

SearchboxHandler::SearchboxHandler(
    mojo::PendingReceiver<searchbox::mojom::PageHandler> pending_page_handler,
    Profile* profile,
    content::WebContents* web_contents,
    MetricsReporter* metrics_reporter)
    : profile_(profile),
      web_contents_(web_contents),
      metrics_reporter_(metrics_reporter),
      page_set_(false),
      page_handler_(this, std::move(pending_page_handler)) {}

SearchboxHandler::~SearchboxHandler() {
  // Avoids dangling pointer warning when `controller_` is not owned.
  controller_ = nullptr;
}
