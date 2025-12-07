// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/tab_group_provider.h"

#include <algorithm>

#include "base/i18n/case_conversion.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#if BUILDFLAG(IS_ANDROID)
#include "components/browser_ui/util/android/url_constants.h"
#endif
#include "components/omnibox/browser/autocomplete_enums.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_classification.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/in_memory_url_index_types.h"
#include "components/omnibox/browser/keyword_provider.h"
#include "components/omnibox/browser/match_compare.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/scoring_functor.h"
#include "components/omnibox/browser/tab_matcher.h"
#include "components/query_parser/query_parser.h"
#include "components/saved_tab_groups/public/saved_tab_group_tab.h"
#include "components/search_engines/template_url.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/url_formatter.h"
#include "content/public/common/url_constants.h"
#include "third_party/omnibox_proto/groups.pb.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace {
std::pair<int, std::u16string> Score(
    const AutocompleteInput& input,
    const query_parser::QueryNodeVector& input_query_nodes,
    const tab_groups::SavedTabGroup& group) {
  TRACE_EVENT_BEGIN0("omnibox", "TabGroupProvider::Score");

  std::u16string matching_url;
  // TODO(crbug.com/435705148): Remove this check when unnamed tab groups are
  // supported as they may be surfaced through a matching URL with no title.
  if (group.title().empty()) {
    return {0, matching_url};
  }

  // Extract query words from the title.
  const std::u16string lower_title = base::i18n::ToLower(group.title());
  query_parser::QueryWordVector title_words;
  query_parser::QueryParser::ExtractQueryWords(lower_title, &title_words);

  // Every input term must be included in either (or both) the title or URL.
  query_parser::Snippet::MatchPositions title_matches;
  query_parser::Snippet::MatchPositions url_matches;
  if (!std::ranges::all_of(input_query_nodes, [&](const auto& query_node) {
        // Search for a matching URL and early return once one is found but do
        // not consider chrome prefixed tabs during matching. The URL provided
        // by saved_tab will include user queries for search result pages.
        bool has_url_match;
        for (auto& saved_tab : group.saved_tabs()) {
#if BUILDFLAG(IS_ANDROID)
          if (saved_tab.url().SchemeIs(browser_ui::kChromeUINativeScheme) ||
              saved_tab.url().SchemeIs(content::kChromeUIScheme)) {
            continue;
          }
#endif

          const std::u16string lower_url =
              base::i18n::ToLower(url_formatter::FormatUrl(
                  saved_tab.url(),
                  AutocompleteMatch::GetFormatTypes(
                      /*preserve_scheme=*/false,
                      /*preserve_subdomain=*/false),
                  base::UnescapeRule::SPACES, nullptr, nullptr, nullptr));

          query_parser::QueryWordVector url_words;
          query_parser::QueryParser::ExtractQueryWords(lower_url, &url_words);

          has_url_match = query_node->HasMatchIn(url_words, &url_matches);
          if (has_url_match) {
            matching_url = lower_url;
            break;
          }
        }
        // Using local vars so to not short circuit adding URL matches when
        // title matches are found.
        const bool has_title_match =
            query_node->HasMatchIn(title_words, &title_matches);
        return has_title_match || has_url_match;
      })) {
    return {0, matching_url};
  }

  // Max score is based on these suggestions sharing a group with open tab
  // matches.
  const int kMaxScore = 1000;
  const double title_factor =
      for_each(title_matches.begin(), title_matches.end(),
               ScoringFunctor(lower_title.length()))
          .ScoringFactor();
  const double url_factor = for_each(url_matches.begin(), url_matches.end(),
                                     ScoringFunctor(matching_url.length()))
                                .ScoringFactor();
  const double normalized_factors =
      std::min((title_factor + url_factor) / (lower_title.length() + 10), 1.0);
  TRACE_EVENT_END0("omnibox", "TabGroupProvider::Score");

  return {normalized_factors * kMaxScore, matching_url};
}

}  // namespace

TabGroupProvider::TabGroupProvider(AutocompleteProviderClient* client)
    : AutocompleteProvider(AutocompleteProvider::TYPE_OPEN_TAB),
      client_(client) {}

TabGroupProvider::~TabGroupProvider() = default;

// TODO(crbug.com/412433887): Make the TabGroupProvider async.
void TabGroupProvider::Start(const AutocompleteInput& input,
                             bool minimal_changes) {
  Stop(AutocompleteStopReason::kClobbered);
  if (input.current_page_classification() !=
          ::metrics::OmniboxEventProto::ANDROID_HUB ||
      client_->IsOffTheRecord()) {
    return;
  }

  TRACE_EVENT_BEGIN0("omnibox", "TabGroupProvider::Start");
  // Remove the keyword from input if we're in keyword mode for a starter pack
  // engine.
  const auto& [adjusted_input, template_url] =
      AdjustInputForStarterPackKeyword(input, client_->GetTemplateURLService());

  // Preprocess the query into query nodes.
  const auto adjusted_input_text = std::u16string(
      base::TrimWhitespace(base::i18n::ToLower(adjusted_input.text()),
                           base::TrimPositions::TRIM_ALL));
  query_parser::QueryNodeVector input_query_nodes;
  query_parser::QueryParser::ParseQueryNodes(
      adjusted_input_text,
      query_parser::MatchingAlgorithm::ALWAYS_PREFIX_SEARCH,
      &input_query_nodes);

  for (auto& group : client_->GetTabGroupSyncService()->GetAllGroups()) {
    const auto& [score, matching_url] = Score(input, input_query_nodes, group);
    if (score > 0) {
      matches_.push_back(
          CreateTabGroupMatch(input, group, score, matching_url));
    }
  }
  TRACE_EVENT_END0("omnibox", "TabGroupProvider::Start");
}

AutocompleteMatch TabGroupProvider::CreateTabGroupMatch(
    const AutocompleteInput& input,
    const tab_groups::SavedTabGroup& group,
    int score,
    const std::u16string& matching_url) {
  AutocompleteMatch match(this, score, /*deletable=*/false,
                          AutocompleteMatchType::TAB_GROUP);
  match.contents = group.title();
  auto contents_terms = FindTermMatches(input.text(), match.contents);
  match.contents_class = ClassifyTermMatches(
      contents_terms, match.contents.size(), ACMatchClassification::MATCH,
      ACMatchClassification::NONE);
  match.matching_tab_group_uuid = group.saved_guid();
  match.suggestion_group_id = omnibox::GROUP_MOBILE_OPEN_TABS;
  match.image_dominant_color =
      base::NumberToString(static_cast<int>(group.color()));

  std::u16string url_list;
  for (auto& saved_tab : group.saved_tabs()) {
#if BUILDFLAG(IS_ANDROID)
    // Skip showing chrome-prefixed tabs.
    if (saved_tab.url().SchemeIs(browser_ui::kChromeUINativeScheme) ||
        saved_tab.url().SchemeIs(content::kChromeUIScheme)) {
      continue;
    }
#endif

    const std::u16string url = url_formatter::FormatUrl(
        saved_tab.url(),
        AutocompleteMatch::GetFormatTypes(/*preserve_scheme=*/false,
                                          /*preserve_subdomain=*/false),
        base::UnescapeRule::SPACES, nullptr, nullptr, nullptr);

    // Place the matching URL at the front of the list.
    if (!matching_url.empty() && url == matching_url) {
      url_list.insert(0, u", ");
      url_list.insert(0, url);
    } else {
      url_list.append(url);
      url_list.append(u", ");
    }
  }

  // Remove the last comma and space from the string.
  if (!url_list.empty()) {
    url_list.erase(url_list.size() - 2);
  }

  match.description = url_list;
  auto description_terms = FindTermMatches(input.text(), match.description);
  match.description_class = ClassifyTermMatches(
      description_terms, match.description.size(),
      ACMatchClassification::MATCH | ACMatchClassification::URL,
      ACMatchClassification::URL);
  match.has_tab_match = true;

  return match;
}
