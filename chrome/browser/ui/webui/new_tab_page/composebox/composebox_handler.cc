// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_page/composebox/composebox_handler.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/webui/new_tab_page/composebox/composebox_omnibox_client.h"
#include "chrome/browser/ui/webui/searchbox/contextual_searchbox_handler.h"
#include "content/public/browser/page_navigator.h"

using composebox::SessionState;

ComposeboxHandler::ComposeboxHandler(
    mojo::PendingReceiver<composebox::mojom::PageHandler> pending_handler,
    mojo::PendingRemote<composebox::mojom::Page> pending_page,
    mojo::PendingReceiver<searchbox::mojom::PageHandler>
        pending_searchbox_handler,
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
                  this))),
      web_contents_(web_contents),
      page_{std::move(pending_page)},
      handler_(this, std::move(pending_handler)) {
  autocomplete_controller_observation_.Observe(autocomplete_controller());
}

ComposeboxHandler::~ComposeboxHandler() = default;

omnibox::ChromeAimToolsAndModels ComposeboxHandler::GetAimToolMode() {
  return aim_tool_mode_;
}

// TODO(crbug.com/450894455): Clean up how we set the tool mode. Create a enum
// on the WebUI side that can set this.
void ComposeboxHandler::SetDeepSearchMode(bool enabled) {
  if (enabled) {
    aim_tool_mode_ = omnibox::ChromeAimToolsAndModels::TOOL_MODE_DEEP_SEARCH;
    base::UmaHistogramEnumeration("NewTabPage.Composebox.Tools.DeepSearch",
                                  AimToolState::kEnabled);
  } else {
    aim_tool_mode_ = omnibox::ChromeAimToolsAndModels::TOOL_MODE_UNSPECIFIED;
    base::UmaHistogramEnumeration("NewTabPage.Composebox.Tools.DeepSearch",
                                  AimToolState::kDisabled);
  }
}

void ComposeboxHandler::SetCreateImageMode(bool enabled, bool image_present) {
  if (enabled) {
    // Only log if not already in some form of create image mode so this metric
    // does not get double counted.
    if (aim_tool_mode_ ==
        omnibox::ChromeAimToolsAndModels::TOOL_MODE_UNSPECIFIED) {
      base::UmaHistogramEnumeration("NewTabPage.Composebox.Tools.CreateImage",
                                    AimToolState::kEnabled);
    }
    // Server uses different `azm` param to make IMAGE_GEN requests when an
    // image is present.
    if (image_present) {
      aim_tool_mode_ =
          omnibox::ChromeAimToolsAndModels::TOOL_MODE_IMAGE_GEN_UPLOAD;
    } else {
      aim_tool_mode_ = omnibox::ChromeAimToolsAndModels::TOOL_MODE_IMAGE_GEN;
    }
  } else {
    aim_tool_mode_ = omnibox::ChromeAimToolsAndModels::TOOL_MODE_UNSPECIFIED;
    base::UmaHistogramEnumeration("NewTabPage.Composebox.Tools.CreateImage",
                                  AimToolState::kDisabled);
  }
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

void ComposeboxHandler::ClearFiles() {
  ContextualSearchboxHandler::ClearFiles();
  // Reset the AIM tool mode to not include file upload if it currently does.
  if (aim_tool_mode_ ==
      omnibox::ChromeAimToolsAndModels::TOOL_MODE_IMAGE_GEN_UPLOAD) {
    aim_tool_mode_ = omnibox::ChromeAimToolsAndModels::TOOL_MODE_IMAGE_GEN;
  }
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
  switch (aim_tool_mode_) {
    case omnibox::ChromeAimToolsAndModels::TOOL_MODE_DEEP_SEARCH:
      additional_params["dr"] = "1";
      base::UmaHistogramEnumeration(
          "NewTabPage.Composebox.Tools.SubmissionType",
          SubmissionType::kDeepSearch);
      break;
    case omnibox::ChromeAimToolsAndModels::TOOL_MODE_IMAGE_GEN:
    case omnibox::ChromeAimToolsAndModels::TOOL_MODE_IMAGE_GEN_UPLOAD:
      additional_params["imgn"] = "1";
      base::UmaHistogramEnumeration(
          "NewTabPage.Composebox.Tools.SubmissionType",
          SubmissionType::kCreateImages);
      break;
    default:
      base::UmaHistogramEnumeration(
          "NewTabPage.Composebox.Tools.SubmissionType",
          SubmissionType::kDefault);
      break;
  }

  ComputeAndOpenQueryUrl(query_text, disposition, std::move(additional_params));
}
