// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/zero_suggest_verbatim_match_provider.h"

#include "base/feature_list.h"
#include "base/strings/escape.h"
#include "components/omnibox/browser/autocomplete_match_classification.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/autocomplete_provider_listener.h"
#include "components/omnibox/browser/verbatim_match.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/url_formatter/url_formatter.h"

namespace {
// The relevance score for verbatim match.
// Must outrank the QueryTiles relevance score.
const int kVerbatimMatchRelevanceScore = 1600;

// Returns whether specific context is eligible for a verbatim match.
// Only offer verbatim match on a site visit and SRP (no NTP etc).
bool IsVerbatimMatchEligible(
    metrics::OmniboxEventProto::PageClassification context) {
  // Only offer verbatim match on a site visit and SRP (no NTP etc).
  return context == metrics::OmniboxEventProto::
                        SEARCH_RESULT_PAGE_DOING_SEARCH_TERM_REPLACEMENT ||
         context == metrics::OmniboxEventProto::
                        SEARCH_RESULT_PAGE_NO_SEARCH_TERM_REPLACEMENT ||
         context == metrics::OmniboxEventProto::ANDROID_SEARCH_WIDGET ||
         context == metrics::OmniboxEventProto::ANDROID_SHORTCUTS_WIDGET ||
         context == metrics::OmniboxEventProto::OTHER;
}

}  // namespace

ZeroSuggestVerbatimMatchProvider::ZeroSuggestVerbatimMatchProvider(
    AutocompleteProviderClient* client)
    : AutocompleteProvider(TYPE_VERBATIM_MATCH), client_(client) {}

ZeroSuggestVerbatimMatchProvider::~ZeroSuggestVerbatimMatchProvider() = default;

void ZeroSuggestVerbatimMatchProvider::Start(const AutocompleteInput& input,
                                             bool minimal_changes) {
  Stop(true, false);
  if (!IsVerbatimMatchEligible(input.current_page_classification()))
    return;

  // Only offer verbatim match after the user just focused the Omnibox,
  // or if the input field is empty.
  if (input.focus_type() == metrics::OmniboxFocusType::INTERACTION_DEFAULT ||
      input.focus_type() == metrics::OmniboxFocusType::INTERACTION_CLOBBER)
    return;

  // For consistency with other zero-prefix providers.
  const auto& page_url = input.current_url();
  if (input.type() != metrics::OmniboxInputType::EMPTY &&
      !(page_url.is_valid() &&
        ((page_url.scheme() == url::kHttpScheme) ||
         (page_url.scheme() == url::kHttpsScheme) ||
         (page_url.scheme() == url::kAboutScheme) ||
         (page_url.scheme() ==
          client_->GetEmbedderRepresentationOfAboutScheme())))) {
    return;
  }

  AutocompleteInput verbatim_input = input;
  verbatim_input.set_prevent_inline_autocomplete(true);
  verbatim_input.set_allow_exact_keyword_match(false);

  AutocompleteMatch match =
      VerbatimMatchForURL(this, client_, verbatim_input, page_url,
                          input.current_title(), kVerbatimMatchRelevanceScore);
  // Make sure the URL is formatted the same was as most visited sites.
  auto format_types = AutocompleteMatch::GetFormatTypes(false, false);
  match.contents = url_formatter::FormatUrl(page_url, format_types,
                                            base::UnescapeRule::SPACES, nullptr,
                                            nullptr, nullptr);

  TermMatches term_matches;
  if (input.text().length() > 0) {
    term_matches = {{0, 0, input.text().length()}};
  }

  match.contents_class = ClassifyTermMatches(
      term_matches, match.contents.size(),
      ACMatchClassification::MATCH | ACMatchClassification::URL,
      ACMatchClassification::URL);

  // In the case of native pages, the classifier may replace the URL with an
  // empty content, resulting with a verbatim match that does not point
  // anywhere.
  if (!match.destination_url.is_valid())
    return;

  match.provider = this;
  matches_.push_back(match);
}
