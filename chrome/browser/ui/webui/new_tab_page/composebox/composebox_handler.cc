// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_page/composebox/composebox_handler.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/notreached.h"
#include "base/time/time.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/webui/new_tab_page/composebox/composebox_omnibox_client.h"
#include "content/public/browser/page_navigator.h"

using composebox::SessionState;

ComposeboxHandler::ComposeboxHandler(
    mojo::PendingReceiver<composebox::mojom::PageHandler> pending_handler,
    mojo::PendingRemote<composebox::mojom::Page> pending_page,
    mojo::PendingReceiver<searchbox::mojom::PageHandler>
        pending_searchbox_handler,
    std::unique_ptr<ContextualSessionService::SessionHandle>
        contextual_session_handle,
    std::unique_ptr<ContextualSessionService::SessionHandle>
        secondary_contextual_session_handle,
    std::unique_ptr<ComposeboxMetricsRecorder> composebox_metrics_recorder,
    Profile* profile,
    content::WebContents* web_contents)
    : ContextualSearchboxHandler(
          std::move(pending_searchbox_handler),
          profile,
          web_contents,
          std::move(composebox_metrics_recorder),
          std::make_unique<OmniboxController>(
              /*view=*/nullptr,
              std::make_unique<composebox::ComposeboxOmniboxClient>(
                  profile,
                  web_contents,
                  this,
                  std::move(secondary_contextual_session_handle))),
          std::move(contextual_session_handle)),
      web_contents_(web_contents),
      page_{std::move(pending_page)},
      handler_(this, std::move(pending_handler)) {
  autocomplete_controller_observation_.Observe(autocomplete_controller());
}

ComposeboxHandler::~ComposeboxHandler() = default;

void ComposeboxHandler::SetDeepSearchMode(bool enabled) {
  deep_search_mode_enabled_ = enabled;
}

void ComposeboxHandler::SetCreateImageMode(bool enabled) {
  create_image_mode_enabled_ = enabled;
}

void ComposeboxHandler::FocusChanged(bool focused) {
  // Unimplemented. Currently the composebox session is tied to when it is
  // connected/disconnected from the DOM, so this is not needed.
}

void ComposeboxHandler::HandleLensButtonClick() {
  // Ignore, intentionally unimplemented for NTP.
}

void ComposeboxHandler::ExecuteAction(uint8_t line,
                                      uint8_t action_index,
                                      const GURL& url,
                                      base::TimeTicks match_selection_timestamp,
                                      uint8_t mouse_button,
                                      bool alt_key,
                                      bool ctrl_key,
                                      bool meta_key,
                                      bool shift_key) {
  NOTREACHED();
}

void ComposeboxHandler::OnThumbnailRemoved() {
  NOTREACHED();
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
  SubmitQuery(query_text, disposition, /*additional_params=*/{});
}

void ComposeboxHandler::SubmitQuery(
    const std::string& query_text,
    WindowOpenDisposition disposition,
    std::map<std::string, std::string> additional_params) {
  if (deep_search_mode_enabled_) {
    additional_params["dr"] = "1";
  }

  if (create_image_mode_enabled_) {
    additional_params["imgn"] = "1";
  }

  ComputeAndOpenQueryUrl(query_text, disposition, std::move(additional_params));
}
