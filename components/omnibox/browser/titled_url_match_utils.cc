// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/titled_url_match_utils.h"

#include <vector>

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/bookmarks/browser/titled_url_node.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/history_provider.h"
#include "components/omnibox/browser/url_prefix.h"
#include "components/url_formatter/url_formatter.h"

namespace {

// Converts |positions| into ACMatchClassifications and returns the
// classifications. |text_length| is used to determine the need to add an
// 'unhighlighted' classification span so the tail of the source string
// properly highlighted.
ACMatchClassifications ClassificationsFromMatchPositions(
    const bookmarks::TitledUrlMatch::MatchPositions& positions,
    size_t text_length,
    bool is_url) {
  ACMatchClassification::Style url_style =
      is_url ? ACMatchClassification::URL : ACMatchClassification::NONE;
  ACMatchClassifications classifications;
  if (positions.empty()) {
    if (text_length > 0) {
      classifications.push_back(ACMatchClassification(0, url_style));
    }
    return classifications;
  }

  for (auto i = positions.begin(); i != positions.end(); ++i) {
    AutocompleteMatch::ACMatchClassifications new_class;
    AutocompleteMatch::ClassifyLocationInString(
        i->first, i->second - i->first, text_length, url_style, &new_class);
    classifications =
        AutocompleteMatch::MergeClassifications(classifications, new_class);
  }
  return classifications;
}

}  // namespace

namespace bookmarks {

AutocompleteMatch TitledUrlMatchToAutocompleteMatch(
    const TitledUrlMatch& titled_url_match,
    AutocompleteMatchType::Type type,
    int relevance,
    AutocompleteProvider* provider,
    const AutocompleteSchemeClassifier& scheme_classifier,
    const AutocompleteInput& input,
    const base::string16& fixed_up_input_text) {
  const GURL& url = titled_url_match.node->GetTitledUrlNodeUrl();
  base::string16 title = titled_url_match.node->GetTitledUrlNodeTitle();

  // The AutocompleteMatch we construct is non-deletable because the only way to
  // support this would be to delete the underlying object that created the
  // titled_url_match. E.g., for the bookmark provider this would mean deleting
  // the underlying bookmark, which is unlikely to be what the user intends.
  AutocompleteMatch match(provider, relevance, false, type);
  TitledUrlMatch::MatchPositions new_title_match_positions =
      titled_url_match.title_match_positions;
  CorrectTitleAndMatchPositions(&title, &new_title_match_positions);
  const base::string16& url_utf16 = base::UTF8ToUTF16(url.spec());
  match.destination_url = url;

  bool match_in_scheme = false;
  bool match_in_subdomain = false;
  bool match_after_host = false;
  AutocompleteMatch::GetMatchComponents(
      url, titled_url_match.url_match_positions, &match_in_scheme,
      &match_in_subdomain, &match_after_host);
  auto format_types = AutocompleteMatch::GetFormatTypes(
      input.parts().scheme.len > 0 || match_in_scheme, match_in_subdomain,
      match_after_host);

  std::vector<size_t> offsets = TitledUrlMatch::OffsetsFromMatchPositions(
      titled_url_match.url_match_positions);
  match.contents = url_formatter::FormatUrlWithOffsets(
      url, format_types, net::UnescapeRule::SPACES, nullptr, nullptr, &offsets);
  TitledUrlMatch::MatchPositions new_url_match_positions =
      TitledUrlMatch::ReplaceOffsetsInMatchPositions(
          titled_url_match.url_match_positions, offsets);
  match.contents_class = ClassificationsFromMatchPositions(
      new_url_match_positions, match.contents.size(), true);

  // The inline_autocomplete_offset should be adjusted based on the formatting
  // applied to |fill_into_edit|.
  size_t inline_autocomplete_offset = URLPrefix::GetInlineAutocompleteOffset(
      input.text(), fixed_up_input_text, false, url_utf16);
  auto fill_into_edit_format_types = url_formatter::kFormatUrlOmitDefaults;
  if (match_in_scheme)
    fill_into_edit_format_types &= ~url_formatter::kFormatUrlOmitHTTP;
  match.fill_into_edit =
      AutocompleteInput::FormattedStringWithEquivalentMeaning(
          url,
          url_formatter::FormatUrl(url, fill_into_edit_format_types,
                                   net::UnescapeRule::SPACES, nullptr, nullptr,
                                   &inline_autocomplete_offset),
          scheme_classifier, &inline_autocomplete_offset);
  if (inline_autocomplete_offset != base::string16::npos) {
    match.inline_autocompletion =
        match.fill_into_edit.substr(inline_autocomplete_offset);
    match.allowed_to_be_default_match =
        match.inline_autocompletion.empty() ||
        !HistoryProvider::PreventInlineAutocomplete(input);
  }
  match.description = title;
  match.description_class = ClassificationsFromMatchPositions(
      titled_url_match.title_match_positions, match.description.size(), false);

  return match;
}

void CorrectTitleAndMatchPositions(
    base::string16* title,
    TitledUrlMatch::MatchPositions* title_match_positions) {
  size_t leading_whitespace_chars = title->length();
  base::TrimWhitespace(*title, base::TRIM_LEADING, title);
  leading_whitespace_chars -= title->length();
  if (leading_whitespace_chars == 0)
    return;
  for (auto it = title_match_positions->begin();
       it != title_match_positions->end(); ++it) {
    it->first -= leading_whitespace_chars;
    it->second -= leading_whitespace_chars;
  }
}

}  // namespace bookmarks
