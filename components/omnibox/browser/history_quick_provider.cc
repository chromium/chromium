// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/history_quick_provider.h"

#include <stddef.h>

#include <algorithm>
#include <vector>

#include "base/i18n/break_iterator.h"
#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "base/trace_event/trace_event.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/history/core/browser/history_database.h"
#include "components/history/core/browser/history_service.h"
#include "components/omnibox/browser/autocomplete_match_classification.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/omnibox/browser/history_url_provider.h"
#include "components/omnibox/browser/in_memory_url_index.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/url_prefix.h"
#include "components/prefs/pref_service.h"
#include "components/url_formatter/url_formatter.h"
#include "net/base/escape.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "third_party/metrics_proto/omnibox_input_type.pb.h"
#include "url/third_party/mozilla/url_parse.h"
#include "url/url_util.h"

bool HistoryQuickProvider::disabled_ = false;

HistoryQuickProvider::HistoryQuickProvider(AutocompleteProviderClient* client)
    : HistoryProvider(AutocompleteProvider::TYPE_HISTORY_QUICK, client),
      in_memory_url_index_(client->GetInMemoryURLIndex()) {
}

void HistoryQuickProvider::Start(const AutocompleteInput& input,
                                 bool minimal_changes) {
  TRACE_EVENT0("omnibox", "HistoryQuickProvider::Start");
  matches_.clear();
  if (disabled_ || input.from_omnibox_focus())
    return;

  // Don't bother with INVALID.
  if ((input.type() == metrics::OmniboxInputType::EMPTY))
    return;

  autocomplete_input_ = input;

  // TODO(pkasting): We should just block here until this loads.  Any time
  // someone unloads the history backend, we'll get inconsistent inline
  // autocomplete behavior here.
  if (in_memory_url_index_) {
    DoAutocomplete();
  }
}

size_t HistoryQuickProvider::EstimateMemoryUsage() const {
  size_t res = HistoryProvider::EstimateMemoryUsage();

  res += base::trace_event::EstimateMemoryUsage(autocomplete_input_);

  return res;
}

HistoryQuickProvider::~HistoryQuickProvider() {
}

void HistoryQuickProvider::DoAutocomplete() {
  // Get the matching URLs from the DB.
  ScoredHistoryMatches matches = in_memory_url_index_->HistoryItemsForTerms(
      autocomplete_input_.text(), autocomplete_input_.cursor_position(),
      provider_max_matches_);
  if (matches.empty())
    return;

  // Loop over every result and add it to matches_.  In the process,
  // guarantee that scores are decreasing.  |max_match_score| keeps
  // track of the highest score we can assign to any later results we
  // see.
  int max_match_score = FindMaxMatchScore(matches);
  for (ScoredHistoryMatches::const_iterator match_iter = matches.begin();
       match_iter != matches.end(); ++match_iter) {
    const ScoredHistoryMatch& history_match(*match_iter);
    // Set max_match_score to the score we'll assign this result.
    max_match_score = std::min(max_match_score, history_match.raw_score);
    matches_.push_back(QuickMatchToACMatch(history_match, max_match_score));
    // Mark this max_match_score as being used.
    max_match_score--;
  }
}

int HistoryQuickProvider::FindMaxMatchScore(
    const ScoredHistoryMatches& matches) {
  // Figure out if HistoryURL provider has a URL-what-you-typed match
  // that ought to go first and what its score will be.
  bool will_have_url_what_you_typed_match_first = false;
  int url_what_you_typed_match_score = -1;  // undefined
  // These are necessary (but not sufficient) conditions for the omnibox
  // input to be a URL-what-you-typed match.  The username test checks that
  // either the username does not exist (a regular URL such as http://site/)
  // or, if the username exists (http://user@site/), there must be either
  // a password or a port.  Together these exclude pure username@site
  // inputs because these are likely to be an e-mail address.  HistoryURL
  // provider won't promote the URL-what-you-typed match to first
  // for these inputs.
  const bool can_have_url_what_you_typed_match_first =
      (autocomplete_input_.type() != metrics::OmniboxInputType::QUERY) &&
      (!autocomplete_input_.parts().username.is_nonempty() ||
       autocomplete_input_.parts().password.is_nonempty() ||
       autocomplete_input_.parts().path.is_nonempty());
  if (can_have_url_what_you_typed_match_first) {
    history::HistoryService* const history_service =
        client()->GetHistoryService();
    // We expect HistoryService to be available.  In case it's not,
    // (e.g., due to Profile corruption) we let HistoryQuick provider
    // completions (which may be available because it's a different
    // data structure) compete with the URL-what-you-typed match as
    // normal.
    if (history_service) {
      history::URLDatabase* url_db = history_service->InMemoryDatabase();
      // url_db can be NULL if it hasn't finished initializing (or
      // failed to to initialize).  In this case, we let HistoryQuick
      // provider completions compete with the URL-what-you-typed
      // match as normal.
      if (url_db) {
        const std::string host(base::UTF16ToUTF8(
            autocomplete_input_.text().substr(
                autocomplete_input_.parts().host.begin,
                autocomplete_input_.parts().host.len)));
        // We want to put the URL-what-you-typed match first if either
        // * the user visited the URL before (intranet or internet).
        // * it's a URL on a host that user visited before and this
        //   is the root path of the host.  (If the user types some
        //   of a path--more than a simple "/"--we let autocomplete compete
        //   normally with the URL-what-you-typed match.)
        // TODO(mpearson): Remove this hacky code and simply score URL-what-
        // you-typed in some sane way relative to possible completions:
        // URL-what-you-typed should get some sort of a boost relative
        // to completions, but completions should naturally win if
        // they're a lot more popular.  In this process, if the input
        // is a bare intranet hostname that has been visited before, we
        // may want to enforce that the only completions that can outscore
        // the URL-what-you-typed match are on the same host (i.e., aren't
        // from a longer internet hostname for which the omnibox input is
        // a prefix).
        if (url_db->GetRowForURL(autocomplete_input_.canonicalized_url(),
                                 nullptr) != 0) {
          // We visited this URL before.
          will_have_url_what_you_typed_match_first = true;
          // HistoryURLProvider gives visited what-you-typed URLs a high score.
          url_what_you_typed_match_score =
              HistoryURLProvider::kScoreForBestInlineableResult;
        } else if (url_db->IsTypedHost(host, /*scheme=*/nullptr) &&
                   (!autocomplete_input_.parts().path.is_nonempty() ||
                    ((autocomplete_input_.parts().path.len == 1) &&
                     (autocomplete_input_
                          .text()[autocomplete_input_.parts().path.begin] ==
                      '/'))) &&
                   !autocomplete_input_.parts().query.is_nonempty() &&
                   !autocomplete_input_.parts().ref.is_nonempty()) {
          // Not visited, but we've seen the host before.
          will_have_url_what_you_typed_match_first = true;
          if (net::registry_controlled_domains::HostHasRegistryControlledDomain(
                  host,
                  net::registry_controlled_domains::EXCLUDE_UNKNOWN_REGISTRIES,
                  net::registry_controlled_domains::
                      EXCLUDE_PRIVATE_REGISTRIES)) {
            // Known internet host.
            url_what_you_typed_match_score =
                HistoryURLProvider::kScoreForWhatYouTypedResult;
          } else {
            // An intranet host.
            url_what_you_typed_match_score =
                HistoryURLProvider::kScoreForUnvisitedIntranetResult;
          }
        }
      }
    }
  }
  // Return a |max_match_score| that is the raw score for the first match, but
  // reduce it if we think there will be a URL-what-you-typed match.  (We want
  // URL-what-you-typed matches for visited URLs to beat out any longer URLs, no
  // matter how frequently they're visited.)  The strength of this reduction
  // depends on the likely score for the URL-what-you-typed result.
  int max_match_score = matches.begin()->raw_score;
  if (will_have_url_what_you_typed_match_first) {
    max_match_score = std::min(max_match_score,
        url_what_you_typed_match_score - 1);
  }
  return max_match_score;
}

AutocompleteMatch HistoryQuickProvider::QuickMatchToACMatch(
    const ScoredHistoryMatch& history_match,
    int score) {
  const history::URLRow& info = history_match.url_info;
  AutocompleteMatch match(
      this, score, !!info.visit_count(),
      history_match.url_matches.empty() ?
          AutocompleteMatchType::HISTORY_TITLE :
          AutocompleteMatchType::HISTORY_URL);
  match.typed_count = info.typed_count();
  match.destination_url = info.url();
  DCHECK(match.destination_url.is_valid());

  // The inline_autocomplete_offset should be adjusted based on the formatting
  // applied to |fill_into_edit|.
  size_t inline_autocomplete_offset = URLPrefix::GetInlineAutocompleteOffset(
      autocomplete_input_.text(), FixupUserInput(autocomplete_input_).second,
      false, base::UTF8ToUTF16(info.url().spec()));
  auto fill_into_edit_format_types = url_formatter::kFormatUrlOmitDefaults;
  if (history_match.match_in_scheme)
    fill_into_edit_format_types &= ~url_formatter::kFormatUrlOmitHTTP;
  match.fill_into_edit =
      AutocompleteInput::FormattedStringWithEquivalentMeaning(
          info.url(),
          url_formatter::FormatUrl(info.url(), fill_into_edit_format_types,
                                   net::UnescapeRule::SPACES, nullptr, nullptr,
                                   &inline_autocomplete_offset),
          client()->GetSchemeClassifier(), &inline_autocomplete_offset);

  // HistoryQuick classification diverges from relevance scoring. Specifically,
  // 1) All occurrences of the input contribute to relevance; e.g. for the input
  // 'pre', the suggestion 'pre prefix' will be scored higher than 'pre suffix'.
  // For classification though, if the input is a prefix of the suggestion text,
  // only the prefix will be bolded; e.g. the 1st suggestion will display '[pre]
  // prefix' as opposed to '[pre] [pre]fix'. This divergence allows consistency
  // with other providers' and google.com's bolding.
  // 2) Mid-word occurrences of the input within the suggestion URL contribute
  // to relevance; e.g. for the input 'mail', the suggestion 'mail - gmail.com'
  // will be scored higher than 'mail - outlook.live.com'. Mid-word matches only
  // in the domain affect scoring. For classification though, mid-word matches
  // are not bolded; e.g. the 1st suggestion will display '[mail] - gmail.com'.
  // 3) User input is not broken on symbols for relevance calculations; e.g. for
  // the input '#yolo', the suggestion 'how-to-yolo - yolo.com/#yolo' would be
  // scored the same as 'how-to-tie-a-tie - yolo.com/#yolo/tie'. For
  // classification though, user input is broken on symbols; e.g. the 1st
  // suggestion will display 'how-to-[yolo] - [yolo].com/#[yolo]'.

  match.contents = url_formatter::FormatUrl(
      info.url(),
      AutocompleteMatch::GetFormatTypes(
          autocomplete_input_.parts().scheme.len > 0 ||
              history_match.match_in_scheme,
          history_match.match_in_subdomain),
      net::UnescapeRule::SPACES, nullptr, nullptr, nullptr);
  auto contents_terms =
      FindTermMatches(autocomplete_input_.text(), match.contents);
  match.contents_class = ClassifyTermMatches(
      contents_terms, match.contents.size(),
      ACMatchClassification::MATCH | ACMatchClassification::URL,
      ACMatchClassification::URL);

  match.description = info.title();
  auto description_terms =
      FindTermMatches(autocomplete_input_.text(), match.description);
  match.description_class = ClassifyTermMatches(
      description_terms, match.description.size(), ACMatchClassification::MATCH,
      ACMatchClassification::NONE);

  // Set |inline_autocompletion| and |allowed_to_be_default_match| if possible.
  if (inline_autocomplete_offset != base::string16::npos) {
    match.inline_autocompletion =
        match.fill_into_edit.substr(inline_autocomplete_offset);
    match.allowed_to_be_default_match =
        AutocompleteMatch::AllowedToBeDefault(autocomplete_input_, match);
  } else {
    auto title = match.description + base::UTF8ToUTF16(" - ") + match.contents;
    match.TryAutocompleteWithTitle(title, autocomplete_input_);
  }

  match.RecordAdditionalInfo("typed count", info.typed_count());
  match.RecordAdditionalInfo("visit count", info.visit_count());
  match.RecordAdditionalInfo("last visit", info.last_visit());

  return match;
}
