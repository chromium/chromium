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
#endif

OpenTabProvider::OpenTabProvider(AutocompleteProviderClient* client)
    : AutocompleteProvider(AutocompleteProvider::TYPE_OPEN_TAB),
      client_(client) {}

OpenTabProvider::~OpenTabProvider() = default;

void OpenTabProvider::Start(const AutocompleteInput& input,
                            bool minimal_changes) {
  matches_.clear();
  if (input.focus_type() != OmniboxFocusType::DEFAULT) {
    return;
  }

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  constexpr size_t kMinQueryLength = 2;
  if (input.text().length() < kMinQueryLength) {
    // Exit early if the query is too short. This is to mitigate a short query
    // matching a large volume of results with low confidence.
    return;
  }

  std::vector<std::u16string> query_terms;
  for (const auto& term :
       base::SplitString(base::i18n::ToLower(input.text()), u" ",
                         base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    query_terms.push_back(term);
  }

  // Perform basic substring matching on the query terms.
  // TODO(crbug.com/1293702): This currently filters via exact substring matches
  // and does not perform scoring yet.
  for (auto* web_contents : client_->GetTabMatcher().GetOpenTabs()) {
    const GURL& url = web_contents->GetLastCommittedURL();
    if (!url.is_valid()) {
      continue;
    }

    const std::u16string title = base::i18n::ToLower(web_contents->GetTitle());
    for (const auto& query_term : query_terms) {
      if (title.find(query_term) != std::string::npos) {
        matches_.push_back(
            CreateOpenTabMatch(input.text(), web_contents->GetTitle(), url));
        break;
      }
    }
  }
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
}

AutocompleteMatch OpenTabProvider::CreateOpenTabMatch(
    const std::u16string& input_text,
    const std::u16string& title,
    const GURL& url) {
  DCHECK(url.is_valid());

  // TODO(crbug.com/1293702): This uses a placeholder relevance. The scoring
  // will need to be revisited as results may be truncated later.
  AutocompleteMatch match(this, /*relevance=*/1000,
                          /*deletable=*/false, AutocompleteMatchType::OPEN_TAB);
  match.destination_url = url;

  match.contents = base::UTF8ToUTF16(url.spec());
  auto contents_terms = FindTermMatches(input_text, match.contents);
  match.contents_class = ClassifyTermMatches(
      contents_terms, match.contents.size(),
      ACMatchClassification::MATCH | ACMatchClassification::URL,
      ACMatchClassification::URL);

  match.description = title;
  auto description_terms = FindTermMatches(input_text, match.description);
  match.description_class = ClassifyTermMatches(
      description_terms, match.description.size(), ACMatchClassification::MATCH,
      ACMatchClassification::NONE);

  return match;
}
