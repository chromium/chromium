// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/open_tab_provider.h"

#include "base/i18n/case_conversion.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match_classification.h"
#include "components/omnibox/browser/keyword_provider.h"
#include "components/omnibox/browser/tab_matcher.h"

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
#include "content/public/browser/web_contents.h"
#endif

OpenTabProvider::OpenTabProvider(AutocompleteProviderClient* client)
    : AutocompleteProvider(AutocompleteProvider::TYPE_OPEN_TAB),
      client_(client) {}

OpenTabProvider::~OpenTabProvider() = default;

void OpenTabProvider::Start(const AutocompleteInput& input,
                            bool minimal_changes) {
  matches_.clear();
  if (input.focus_type() != OmniboxFocusType::DEFAULT || input.text().empty()) {
    return;
  }

  // Remove the keyword from input if we're in keyword mode for a starter pack
  // engine.
  AutocompleteInput adjusted_input =
      KeywordProvider::AdjustInputForStarterPackEngines(
          input, client_->GetTemplateURLService());

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  // Preprocess the query into lowercase terms.
  std::vector<std::u16string> query_terms;
  for (const std::u16string& term :
       base::SplitString(base::i18n::ToLower(adjusted_input.text()), u" ",
                         base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    query_terms.push_back(term);
  }

  // Perform basic substring matching on the query terms.
  for (auto* web_contents : client_->GetTabMatcher().GetOpenTabs()) {
    const GURL& url = web_contents->GetLastCommittedURL();
    if (!url.is_valid()) {
      continue;
    }

    const std::u16string title = base::i18n::ToLower(web_contents->GetTitle());
    for (const std::u16string& query_term : query_terms) {
      if (title.find(query_term) != std::string::npos) {
        matches_.push_back(
            CreateOpenTabMatch(adjusted_input, web_contents->GetTitle(), url));
        break;
      }
    }
  }
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
}

AutocompleteMatch OpenTabProvider::CreateOpenTabMatch(
    const AutocompleteInput& input,
    const std::u16string& title,
    const GURL& url) {
  DCHECK(url.is_valid());

  // TODO(crbug.com/1293702): All open tab results are given a default score.
  // Currently, the only user of the Open Tab Provider is the ChromeOS launcher,
  // which performs further scoring using the ChromeOS tokenized string match
  // library.
  constexpr int kOpenTabScore = 1000;
  AutocompleteMatch match(this, kOpenTabScore,
                          /*deletable=*/false, AutocompleteMatchType::OPEN_TAB);

  match.destination_url = url;

  // Setting this ensures that the result deduplication doesn't prioritize
  // another default match over an open tab result.
  match.allowed_to_be_default_match = true;

  match.contents = base::UTF8ToUTF16(url.spec());
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
