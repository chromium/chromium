// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/open_tab_provider.h"

#include "base/i18n/case_conversion.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_classification.h"
#include "components/omnibox/browser/in_memory_url_index_types.h"
#include "components/omnibox/browser/keyword_provider.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/scoring_functor.h"
#include "components/omnibox/browser/tab_matcher.h"
#include "components/query_parser/query_parser.h"
#include "components/search_engines/template_url.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/url_formatter.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
#include "content/public/browser/web_contents.h"
#endif

namespace {

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
int Score(const query_parser::QueryNodeVector& input_query_nodes,
          const std::u16string& title,
          const GURL& url) {
  // TODO(crbug/1287313): The bookmark provider also uses on `query_parser` and
  //  `ScoringFunctor` to compute its scores. However, it uses normalized match
  //  titles. (see `Normalize()` in
  //  components/bookmarks/browser/titled_url_index.cc) IDK its purpose, but we
  //  should either verify it's unnecessary here, or do likewise here.

  // Extract query words from the title.
  const std::u16string lower_title = base::i18n::ToLower(title);
  query_parser::QueryWordVector title_words;
  query_parser::QueryParser::ExtractQueryWords(lower_title, &title_words);

  // Extract query words from the URL.
  const std::u16string lower_url =
      base::i18n::ToLower(base::UTF8ToUTF16(url.spec()));
  query_parser::QueryWordVector url_words;
  query_parser::QueryParser::ExtractQueryWords(lower_url, &url_words);

  // Every input term must be included in either (or both) the title or URL.
  query_parser::Snippet::MatchPositions title_matches;
  query_parser::Snippet::MatchPositions url_matches;
  if (!base::ranges::all_of(input_query_nodes, [&](const auto& query_node) {
        // Using local vars so to not short circuit adding URL matches when
        // title matches are found.
        const bool has_title_match =
            query_node->HasMatchIn(title_words, &title_matches);
        const bool has_url_match =
            query_node->HasMatchIn(url_words, &url_matches);
        return has_title_match || has_url_match;
      })) {
    return 0;
  }

  // Score the matches following a simplified variation of
  // `BookmarkProvider::CalculateBookmarkMatchRelevance()`. `kMaxScore` is
  // chosen based on a reasonable upper bound for title & URL length of 100. The
  // exact scores don't matter, but using a scale proportional the max title
  // length squared will avoid suggestions with different float scores being
  // rounded to the same integer score.
  const int kMaxScore = 100 * 100;
  const double title_factor =
      for_each(title_matches.begin(), title_matches.end(),
               ScoringFunctor(lower_title.length()))
          .ScoringFactor();
  const double url_factor = for_each(url_matches.begin(), url_matches.end(),
                                     ScoringFunctor(lower_url.length()))
                                .ScoringFactor();
  const double normalized_factors =
      std::min((title_factor + url_factor) / (lower_title.length() + 10), 1.0);
  return normalized_factors * kMaxScore;
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

}  // namespace

OpenTabProvider::OpenTabProvider(AutocompleteProviderClient* client)
    : AutocompleteProvider(AutocompleteProvider::TYPE_OPEN_TAB),
      client_(client) {}

OpenTabProvider::~OpenTabProvider() = default;

void OpenTabProvider::Start(const AutocompleteInput& input,
                            bool minimal_changes) {
  matches_.clear();
  if (input.focus_type() != metrics::OmniboxFocusType::INTERACTION_DEFAULT ||
      input.text().empty()) {
    return;
  }

  // Remove the keyword from input if we're in keyword mode for a starter pack
  // engine.
  const auto [adjusted_input, template_url] =
      KeywordProvider::AdjustInputForStarterPackEngines(
          input, client_->GetTemplateURLService());
  if (adjusted_input.text().empty()) {
    return;
  }

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  // Preprocess the query into query nodes.
  const auto adjusted_input_text = std::u16string(
      base::TrimWhitespace(base::i18n::ToLower(adjusted_input.text()),
                           base::TrimPositions::TRIM_ALL));
  query_parser::QueryNodeVector input_query_nodes;
  query_parser::QueryParser::ParseQueryNodes(
      adjusted_input_text,
      query_parser::MatchingAlgorithm::ALWAYS_PREFIX_SEARCH,
      &input_query_nodes);

  // Perform basic substring matching on the query terms.
  for (auto* web_contents : client_->GetTabMatcher().GetOpenTabs()) {
    const GURL& url = web_contents->GetLastCommittedURL();
    if (!url.is_valid()) {
      continue;
    }
    int score = Score(input_query_nodes, web_contents->GetTitle(),
                      web_contents->GetLastCommittedURL());
    if (score > 0) {
      matches_.push_back(CreateOpenTabMatch(
          adjusted_input, web_contents->GetTitle(), url, score, template_url));
    }
  }

  // If there were no open tab results found, and we're in keyword mode,
  // generate a NULL_RESULT_MESSAGE suggestion to keep the user in keyword mode
  // and display a no results message.
  if (OmniboxFieldTrial::IsSiteSearchStarterPackEnabled() &&
      InKeywordMode(adjusted_input) && matches_.empty()) {
    matches_.push_back(
        CreateNullResultMessageMatch(adjusted_input, template_url));
  }
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
}

AutocompleteMatch OpenTabProvider::CreateOpenTabMatch(
    const AutocompleteInput& input,
    const std::u16string& title,
    const GURL& url,
    int score,
    const TemplateURL* template_url) {
  DCHECK(url.is_valid());

  AutocompleteMatch match(this, score, /*deletable=*/false,
                          AutocompleteMatchType::OPEN_TAB);

  match.destination_url = url;
  match.fill_into_edit = base::UTF8ToUTF16(url.spec());

  // Setting this ensures that the result deduplication doesn't prioritize
  // another default match over an open tab result.
  match.allowed_to_be_default_match = true;

  // If the input was in the @tabs keyword scope, set the `keyword` and
  // `transition` appropriately to avoid popping the user out of keyword mode.
  if (template_url) {
    match.keyword = template_url->keyword();
    match.transition = ui::PAGE_TRANSITION_KEYWORD;
  }

  // For display in the suggestion UI, elide all optional parts. The user has
  // already opened these URLs so there's no need for them to make nuanced
  // judgements about the URL (for example, whether the scheme is http:// or
  // https:// or whether there's a "www" subdomain).
  match.contents = url_formatter::FormatUrl(
      url,
      AutocompleteMatch::GetFormatTypes(/*preserve_scheme=*/false,
                                        /*preserve_subdomain=*/false),
      base::UnescapeRule::SPACES, nullptr, nullptr, nullptr);

  auto contents_terms = FindTermMatches(input.text(), match.contents);
  match.contents_class = ClassifyTermMatches(
      contents_terms, match.contents.size(),
      ACMatchClassification::MATCH | ACMatchClassification::URL,
      ACMatchClassification::URL);

  match.description = title;
  auto description_terms = FindTermMatches(input.text(), match.description);
  match.description_class = ClassifyTermMatches(
      description_terms, match.description.size(), ACMatchClassification::MATCH,
      ACMatchClassification::NONE);

  if (InKeywordMode(input)) {
    match.from_keyword = true;
  }

  return match;
}

AutocompleteMatch OpenTabProvider::CreateNullResultMessageMatch(
    const AutocompleteInput& input,
    const TemplateURL* template_url) {
  DCHECK(template_url);

  // This value doesn't really matter as this suggestion is only displayed when
  // no other suggestions were found. Use an arbitrary constant.
  constexpr int kRelevanceScore = 1000;
  AutocompleteMatch match(this, kRelevanceScore, /*deletable=*/false,
                          AutocompleteMatchType::NULL_RESULT_MESSAGE);

  // These fields are filled in to enable the Keyword UI when only this
  // suggestion is available.
  match.fill_into_edit = input.text();
  match.allowed_to_be_default_match = true;
  match.keyword = template_url->keyword();
  match.transition = ui::PAGE_TRANSITION_KEYWORD;

  // Use this suggestion's contents field to display a message to the user that
  // there were no matching results found.
  match.contents =
      l10n_util::GetStringUTF16(IDS_OMNIBOX_TAB_SEARCH_NO_RESULTS_FOUND);
  match.contents_class.push_back(
      ACMatchClassification(0, ACMatchClassification::NONE));
  match.from_keyword = true;

  return match;
}
