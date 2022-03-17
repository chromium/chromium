// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/open_tab_provider.h"

#include "base/i18n/case_conversion.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "components/omnibox/browser/autocomplete_match_classification.h"
#include "components/omnibox/browser/tab_matcher.h"

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
#include "content/public/browser/web_contents.h"

namespace {

constexpr size_t kMaxMatches = 5u;

}  // namespace

#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

OpenTabProvider::OpenTabProvider(AutocompleteProviderClient* client)
    : AutocompleteProvider(AutocompleteProvider::TYPE_OPEN_TAB),
      client_(client),
      in_memory_url_index_(client->GetInMemoryURLIndex()) {}

OpenTabProvider::~OpenTabProvider() = default;

void OpenTabProvider::Start(const AutocompleteInput& input,
                            bool minimal_changes) {
  matches_.clear();
  if (input.focus_type() != OmniboxFocusType::DEFAULT ||
      !in_memory_url_index_) {
    return;
  }

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  // TODO(crbug.com/1293702): Open tab search is currently implemented using
  // the history model to score open tabs. This is an interim implementation,
  // which is intended to be replaced with a scoring mechanism using only the
  // TabMatcher itself.

  base::flat_set<GURL> open_urls;
  for (auto* web_contents : client_->GetTabMatcher().GetOpenTabs()) {
    open_urls.insert(web_contents->GetLastCommittedURL());
  }
  if (open_urls.empty()) {
    return;
  }

  ScoredHistoryMatches matches = in_memory_url_index_->HistoryItemsForTerms(
      input.text(), input.cursor_position(), kMaxMatches);
  if (matches.empty()) {
    return;
  }

  for (ScoredHistoryMatches::const_iterator match_iter = matches.begin();
       match_iter != matches.end(); ++match_iter) {
    const ScoredHistoryMatch& history_match(*match_iter);
    const GURL& url = history_match.url_info.url();
    const auto it = open_urls.find(url);
    if (it != open_urls.end() && url.is_valid()) {
      matches_.push_back(CreateOpenTabMatch(input.text(), history_match));
    }
  }
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
}

AutocompleteMatch OpenTabProvider::CreateOpenTabMatch(
    const std::u16string& input_text,
    const ScoredHistoryMatch& history_match) {
  AutocompleteMatch match(this, history_match.raw_score,
                          /*deletable=*/false, AutocompleteMatchType::OPEN_TAB);

  const GURL& url = history_match.url_info.url();
  DCHECK(url.is_valid());
  match.destination_url = url;

  // Setting this ensures that result deduplication doesn't prioritize a history
  // over an open tab result.
  match.allowed_to_be_default_match = true;

  match.contents = base::UTF8ToUTF16(url.spec());
  auto contents_terms = FindTermMatches(input_text, match.contents);
  match.contents_class = ClassifyTermMatches(
      contents_terms, match.contents.size(),
      ACMatchClassification::MATCH | ACMatchClassification::URL,
      ACMatchClassification::URL);

  match.description = history_match.url_info.title();
  auto description_terms = FindTermMatches(input_text, match.description);
  match.description_class = ClassifyTermMatches(
      description_terms, match.description.size(), ACMatchClassification::MATCH,
      ACMatchClassification::NONE);

  return match;
}
