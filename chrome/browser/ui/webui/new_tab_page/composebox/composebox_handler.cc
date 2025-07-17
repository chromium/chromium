// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_page/composebox/composebox_handler.h"

#include "content/public/browser/page_navigator.h"

ComposeboxHandler::ComposeboxHandler(
    mojo::PendingReceiver<composebox::mojom::ComposeboxPageHandler> handler,
    std::unique_ptr<ComposeboxQueryController> query_controller,
    content::WebContents* web_contents)
    : handler_(this, std::move(handler)),
      query_controller_(std::move(query_controller)),
      web_contents_(web_contents) {}

ComposeboxHandler::~ComposeboxHandler() = default;

void ComposeboxHandler::NotifySessionStarted() {
  query_controller_->NotifySessionStarted();
}

void ComposeboxHandler::NotifySessionAbandoned() {
  query_controller_->NotifySessionAbandoned();
}

void ComposeboxHandler::SubmitQuery(const std::string& query_text,
                                    uint8_t mouse_button,
                                    bool alt_key,
                                    bool ctrl_key,
                                    bool meta_key,
                                    bool shift_key) {
  const WindowOpenDisposition disposition = ui::DispositionFromClick(
      /*middle_button=*/mouse_button == 1, alt_key, ctrl_key, meta_key,
      shift_key);
  OpenUrl(query_controller_->CreateAimUrl(query_text), disposition);
}

void ComposeboxHandler::OpenUrl(GURL url,
                                const WindowOpenDisposition disposition) {
  content::OpenURLParams params(url, content::Referrer(), disposition,
                                ui::PAGE_TRANSITION_LINK, false);
  web_contents_->OpenURL(params, base::DoNothing());
}
