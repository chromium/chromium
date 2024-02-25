// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/builtin_provider.h"

#include <stddef.h>

#include <string>

#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_classification.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/url_formatter/url_fixer.h"
#include "third_party/metrics_proto/omnibox_focus_type.pb.h"
#include "third_party/metrics_proto/omnibox_input_type.pb.h"
#include "url/url_constants.h"

const int BuiltinProvider::kRelevance = 860;

BuiltinProvider::BuiltinProvider(AutocompleteProviderClient* client)
    : AutocompleteProvider(AutocompleteProvider::TYPE_BUILTIN),
      client_(client) {
  builtins_ = client_->GetBuiltinURLs();
  template_url_service_ = client->GetTemplateURLService();
}

void BuiltinProvider::Start(const AutocompleteInput& input,
                            bool minimal_changes) {
  matches_.clear();
  if (input.IsZeroSuggest() ||
      (input.type() == metrics::OmniboxInputType::EMPTY)) {
    return;
  }

  if (input.type() != metrics::OmniboxInputType::QUERY) {
    DoBuiltinAutocompletion(input.text());
    UpdateRelevanceScores(input);
  }
}

BuiltinProvider::~BuiltinProvider() = default;

void BuiltinProvider::DoBuiltinAutocompletion(const std::u16string& text) {
  const size_t kAboutSchemeLength = strlen(url::kAboutScheme);
  const std::u16string kAbout =
      base::StrCat({url::kAboutScheme16, url::kStandardSchemeSeparator16});
  const std::u16string embedderAbout = base::StrCat(
      {base::UTF8ToUTF16(client_->GetEmbedderRepresentationOfAboutScheme()),
       url::kStandardSchemeSeparator16});

  const int kUrl = ACMatchClassification::URL;
  const int kMatch = kUrl | ACMatchClassification::MATCH;

  bool starting_about = base::StartsWith(embedderAbout, text,
                                         base::CompareCase::INSENSITIVE_ASCII);
  if (starting_about ||
      base::StartsWith(kAbout, text, base::CompareCase::INSENSITIVE_ASCII)) {
    // Highlight the input portion matching |embedderAbout|; or if the user
    // has input "about:" (with optional slashes), highlight the whole
    // |embedderAbout|.
    TermMatches style_matches;
    if (starting_about)
      style_matches.emplace_back(0, 0, text.length());
    else if (text.length() > kAboutSchemeLength)
      style_matches.emplace_back(0, 0, embedderAbout.length());
    ACMatchClassifications styles =
        ClassifyTermMatches(style_matches, std::string::npos, kMatch, kUrl);
    // Include some common builtin URLs as the user types the scheme.
    for (std::u16string url : client_->GetBuiltinsToProvideAsUserTypes())
      AddBuiltinMatch(url, std::u16string(), styles);

  } else {
    // Match input about: or |embedderAbout| URL input against builtin URLs.
    GURL url = url_formatter::FixupURL(base::UTF16ToUTF8(text), std::string());
    const bool text_ends_with_slash =
        base::EndsWith(text, u"/", base::CompareCase::SENSITIVE);
    // BuiltinProvider doesn't know how to suggest valid ?query or #fragment
    // extensions to builtin URLs.
    if (url.SchemeIs(client_->GetEmbedderRepresentationOfAboutScheme()) &&
        url.has_host() && !url.has_query() && !url.has_ref()) {
      // Suggest about:blank for substrings, taking URL fixup into account.
      // Chrome does not support trailing slashes or paths for about:blank.
      const std::u16string blank_host = u"blank";
      const std::u16string host = base::UTF8ToUTF16(url.host());
      if (base::StartsWith(text, url::kAboutScheme16,
                           base::CompareCase::INSENSITIVE_ASCII) &&
          base::StartsWith(blank_host, host,
                           base::CompareCase::INSENSITIVE_ASCII) &&
          (url.path().length() <= 1) && !text_ends_with_slash) {
        std::u16string match(url::kAboutBlankURL16);
        const size_t corrected_length = kAboutSchemeLength + 1 + host.length();
        TermMatches style_matches = {{0, 0, corrected_length}};
        ACMatchClassifications styles =
            ClassifyTermMatches(style_matches, match.length(), kMatch, kUrl);
        AddBuiltinMatch(match, match.substr(corrected_length), styles);
      }

      // Include the path for sub-pages (e.g. "chrome://settings/browser").
      std::u16string host_and_path = base::UTF8ToUTF16(url.host() + url.path());
      base::TrimString(host_and_path, u"/", &host_and_path);
      size_t match_length = embedderAbout.length() + host_and_path.length();
      for (Builtins::const_iterator i(builtins_.begin());
           (i != builtins_.end()) && (matches_.size() < provider_max_matches_);
           ++i) {
        if (base::StartsWith(*i, host_and_path,
                             base::CompareCase::INSENSITIVE_ASCII)) {
          std::u16string match_string = embedderAbout + *i;
          TermMatches style_matches = {{0, 0, match_length}};
          ACMatchClassifications styles = ClassifyTermMatches(
              style_matches, match_string.length(), kMatch, kUrl);
          // FixupURL() may have dropped a trailing slash on the user's input.
          // Ensure that in that case, we don't inline autocomplete unless the
          // autocompletion restores the slash.  This prevents us from e.g.
          // trying to add a 'y' to an input like "chrome://histor/".
          std::u16string inline_autocompletion(
              match_string.substr(match_length));
          if (text_ends_with_slash &&
              !base::StartsWith(match_string.substr(match_length), u"/",
                                base::CompareCase::INSENSITIVE_ASCII))
            inline_autocompletion = std::u16string();
          AddBuiltinMatch(match_string, inline_autocompletion, styles);
        }
      }
    }
  }
}

void BuiltinProvider::UpdateRelevanceScores(const AutocompleteInput& input) {
  // Provide a relevance score for each match.
  for (size_t i = 0; i < matches_.size(); ++i) {
    matches_[i].relevance += matches_.size() - (i + 1);
  }

  // If allowing completions is okay and there's a match that's considered
  // appropriate to be the default match, mark it as such and give it a high
  // enough score to beat url-what-you-typed.
  size_t default_match_index;

  // None of the built in site URLs contain whitespaces so we can safely prevent
  // autocompletion when the input has a trailing whitespace in order to avoid
  // autocompleting e.g. 'chrome://s ettings' when the input is 'chrome://s '.
  bool input_allowed_to_have_default_match =
      !input.prevent_inline_autocomplete() &&
      (input.text().empty() || !base::IsUnicodeWhitespace(input.text().back()));
  if (input_allowed_to_have_default_match &&
      HasMatchThatShouldBeDefault(&default_match_index)) {
    matches_[default_match_index].relevance = 1250;
    matches_[default_match_index].allowed_to_be_default_match = true;
  }
}

void BuiltinProvider::AddBuiltinMatch(const std::u16string& match_string,
                                      const std::u16string& inline_completion,
                                      const ACMatchClassifications& styles) {
  AutocompleteMatch match(this, kRelevance, false,
                          AutocompleteMatchType::NAVSUGGEST);
  match.suggest_type = omnibox::TYPE_NAVIGATION;
  match.fill_into_edit = match_string;
  match.inline_autocompletion = inline_completion;
  match.destination_url = GURL(match_string);
  match.contents = match_string;
  match.contents_class = styles;
  matches_.push_back(match);
}

bool BuiltinProvider::HasMatchThatShouldBeDefault(size_t* index) const {
  if (matches_.size() == 0)
    return false;

  // If there's only one possible completion of the user's input and it's not
  // empty, it should be allowed to be the default match.
  if (matches_.size() == 1) {
    *index = 0;
    return !matches_[0].inline_autocompletion.empty();
  }

  // If there's a non-empty completion that is a prefix of all of the others,
  // it should be allowed to be the default match.
  size_t shortest = 0;
  for (size_t i = 1; i < matches_.size(); ++i) {
    if (matches_[i].contents.length() < matches_[shortest].contents.length())
      shortest = i;
  }
  if (matches_[shortest].inline_autocompletion.empty())
    return false;

  for (size_t i = 0; i < matches_.size(); ++i) {
    if (!base::StartsWith(matches_[i].contents, matches_[shortest].contents,
                          base::CompareCase::INSENSITIVE_ASCII)) {
      return false;
    }
  }
  *index = shortest;
  return true;
}
