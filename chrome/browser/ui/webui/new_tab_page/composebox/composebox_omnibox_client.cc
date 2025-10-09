// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_page/composebox/composebox_omnibox_client.h"

#include "base/strings/utf_string_conversions.h"
#include "components/lens/lens_url_utils.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/composebox/composebox_query_controller.h"
#include "components/omnibox/composebox/contextual_session_service.h"
#include "net/base/url_util.h"
#include "ui/webui/resources/cr_components/composebox/composebox.mojom.h"

namespace composebox {

ComposeboxOmniboxClient::ComposeboxOmniboxClient(
    Profile* profile,
    content::WebContents* web_contents,
    BaseComposeboxHandler* composebox_handler,
    std::unique_ptr<ContextualSessionService::SessionHandle>
        contextual_session_handle)
    : SearchboxOmniboxClient(profile, web_contents),
      composebox_handler_(composebox_handler),
      contextual_session_handle_(std::move(contextual_session_handle)) {}

ComposeboxOmniboxClient::~ComposeboxOmniboxClient() = default;

metrics::OmniboxEventProto::PageClassification
ComposeboxOmniboxClient::GetPageClassification(bool is_prefetch) const {
  // TODO(crbug.com/441808425): This page classification should be passed in
  // from the embedder so that it can be customized. Currently, Lens is logging
  // as NTP_COMPOSEBOX, but it should be its own page classification.
  return metrics::OmniboxEventProto::NTP_COMPOSEBOX;
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
  composebox_handler_->SubmitQuery(query_text, disposition, additional_params);
}

std::optional<lens::proto::LensOverlaySuggestInputs>
ComposeboxOmniboxClient::GetLensOverlaySuggestInputs() const {
  if (!contextual_session_handle_) {
    return std::nullopt;
  }

  auto* query_controller = contextual_session_handle_->GetController();
  if (!query_controller) {
    return std::nullopt;
  }

  const auto& suggest_inputs = query_controller->suggest_inputs();
  if (suggest_inputs.has_encoded_request_id()) {
    return suggest_inputs;
  }

  return std::nullopt;
}
}  // namespace composebox
