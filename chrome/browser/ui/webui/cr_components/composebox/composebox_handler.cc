// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/cr_components/composebox/composebox_handler.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/webui/cr_components/searchbox/contextual_searchbox_handler.h"
#include "chrome/browser/ui/webui/cr_components/searchbox/searchbox_utils.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_web_ui_controller.h"
#include "components/contextual_search/contextual_search_types.h"
#include "components/lens/lens_url_utils.h"
#include "components/metrics/metrics_provider.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "content/public/browser/page_navigator.h"
#include "net/base/url_util.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "third_party/omnibox_proto/chrome_aim_entry_point.pb.h"
#include "ui/base/models/menu_model.h"
#include "ui/base/window_open_disposition.h"

namespace {

class ComposeboxOmniboxClient final : public ContextualOmniboxClient {
 public:
  ComposeboxOmniboxClient(Profile* profile,
                          content::WebContents* web_contents,
                          ComposeboxHandler* composebox_handler);

  ~ComposeboxOmniboxClient() override;

  // OmniboxClient:
  metrics::OmniboxEventProto::PageClassification GetPageClassification(
      bool is_prefetch) const override;
  std::optional<lens::ContextualInputData> GetContextualInputData()
      const override;

  void OnAutocompleteAccept(
      const GURL& destination_url,
      TemplateURLRef::PostContent* post_content,
      WindowOpenDisposition disposition,
      ui::PageTransition transition,
      AutocompleteMatchType::Type match_type,
      base::TimeTicks match_selection_timestamp,
      bool destination_url_entered_without_scheme,
      bool destination_url_entered_with_http_scheme,
      const std::u16string& text,
      const AutocompleteMatch& match,
      const AutocompleteMatch& alternative_nav_match) override;

 private:
  raw_ptr<ComposeboxHandler> composebox_handler_;
};

ComposeboxOmniboxClient::ComposeboxOmniboxClient(
    Profile* profile,
    content::WebContents* web_contents,
    ComposeboxHandler* composebox_handler)
    : ContextualOmniboxClient(profile, web_contents),
      composebox_handler_(composebox_handler) {}

ComposeboxOmniboxClient::~ComposeboxOmniboxClient() = default;

metrics::OmniboxEventProto::PageClassification
ComposeboxOmniboxClient::GetPageClassification(bool is_prefetch) const {
  return metrics::OmniboxEventProto::NTP_COMPOSEBOX;
}

std::optional<lens::ContextualInputData>
ComposeboxOmniboxClient::GetContextualInputData() const {
  if (composebox_handler_) {
    return composebox_handler_->context_input_data();
  }
  return std::nullopt;
}

void ComposeboxOmniboxClient::OnAutocompleteAccept(
    const GURL& destination_url,
    TemplateURLRef::PostContent* post_content,
    WindowOpenDisposition disposition,
    ui::PageTransition transition,
    AutocompleteMatchType::Type match_type,
    base::TimeTicks match_selection_timestamp,
    bool destination_url_entered_without_scheme,
    bool destination_url_entered_with_http_scheme,
    const std::u16string& text,
    const AutocompleteMatch& match,
    const AutocompleteMatch& alternative_nav_match) {
  const std::map<std::string, std::string>& additional_params =
      lens::GetParametersMapWithoutQuery(destination_url);

  std::string query_text;
  net::GetValueForKeyInQuery(destination_url, "q", &query_text);
  composebox_handler_->SubmitQuery(
      query_text, disposition,
      PageClassificationToAimEntryPoint(
          GetPageClassification(/*is_prefetch=*/false)),
      additional_params);
}

}  // namespace

ComposeboxHandler::ComposeboxHandler(
    mojo::PendingReceiver<composebox::mojom::PageHandler> pending_handler,
    mojo::PendingRemote<composebox::mojom::Page> pending_page,
    mojo::PendingReceiver<searchbox::mojom::PageHandler>
        pending_searchbox_handler,
    Profile* profile,
    content::WebContents* web_contents)
    : ComposeboxHandler(
          std::move(pending_handler),
          std::move(pending_page),
          std::move(pending_searchbox_handler),
          profile,
          web_contents,
          std::make_unique<OmniboxController>(
              std::make_unique<ComposeboxOmniboxClient>(profile,
                                                        web_contents,
                                                        this))) {}

ComposeboxHandler::ComposeboxHandler(
    mojo::PendingReceiver<composebox::mojom::PageHandler> pending_handler,
    mojo::PendingRemote<composebox::mojom::Page> pending_page,
    mojo::PendingReceiver<searchbox::mojom::PageHandler>
        pending_searchbox_handler,
    Profile* profile,
    content::WebContents* web_contents,
    std::unique_ptr<OmniboxController> omnibox_controller)
    : ContextualSearchboxHandler(std::move(pending_searchbox_handler),
                                 profile,
                                 web_contents,
                                 std::move(omnibox_controller)),
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
  } else {
    aim_tool_mode_ = omnibox::ChromeAimToolsAndModels::TOOL_MODE_UNSPECIFIED;
  }

  if (auto* metrics_recorder = GetMetricsRecorder()) {
    metrics_recorder->RecordToolState(
        contextual_search::SubmissionType::kDeepSearch,
        enabled ? contextual_search::AimToolState::kEnabled
                : contextual_search::AimToolState::kDisabled);
  }
}

void ComposeboxHandler::SetCreateImageMode(bool enabled, bool image_present) {
  std::optional<contextual_search::AimToolState> tool_state;
  if (enabled) {
    // Only log if not already in some form of create image mode so this metric
    // does not get double counted.
    if (aim_tool_mode_ ==
        omnibox::ChromeAimToolsAndModels::TOOL_MODE_UNSPECIFIED) {
      tool_state = contextual_search::AimToolState::kEnabled;
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
    tool_state = contextual_search::AimToolState::kDisabled;
  }

  if (!tool_state) {
    return;
  }
  if (auto* metrics_recorder = GetMetricsRecorder()) {
    metrics_recorder->RecordToolState(
        contextual_search::SubmissionType::kCreateImages, *tool_state);
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

void ComposeboxHandler::ShowContextMenu(const gfx::Point& point) {
  if (embedder_) {
    embedder_->ShowContextMenu(point, /*menu_model=*/nullptr);
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
  omnibox::ChromeAimEntryPoint aim_entry_point =
      PageClassificationToAimEntryPoint(
          omnibox_controller()->client()->GetPageClassification(
              /*is_prefetch=*/false));
  SubmitQuery(query_text, disposition, aim_entry_point,
              /*additional_params=*/{});
}

void ComposeboxHandler::SubmitQuery(
    const std::string& query_text,
    WindowOpenDisposition disposition,
    omnibox::ChromeAimEntryPoint aim_entrypoint,
    std::map<std::string, std::string> additional_params) {
  contextual_search::SubmissionType submission_type;
  switch (aim_tool_mode_) {
    case omnibox::ChromeAimToolsAndModels::TOOL_MODE_DEEP_SEARCH:
      additional_params["dr"] = "1";
      submission_type = contextual_search::SubmissionType::kDeepSearch;
      break;
    case omnibox::ChromeAimToolsAndModels::TOOL_MODE_IMAGE_GEN:
    case omnibox::ChromeAimToolsAndModels::TOOL_MODE_IMAGE_GEN_UPLOAD:
      additional_params["imgn"] = "1";
      submission_type = contextual_search::SubmissionType::kCreateImages;
      break;
    default:
      submission_type = contextual_search::SubmissionType::kDefault;
      break;
  }

  if (auto* metrics_recorder = GetMetricsRecorder()) {
    metrics_recorder->RecordToolsSubmissionType(submission_type);
  }

  ComputeAndOpenQueryUrl(query_text, disposition, aim_entrypoint,
                         std::move(additional_params));
}

void ComposeboxHandler::UpdateSuggestedTabContext(
    searchbox::mojom::TabInfoPtr tab_info) {
  SearchboxHandler::page_->UpdateAutoSuggestedTabContext(std::move(tab_info));
}
