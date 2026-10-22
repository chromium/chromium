// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/recently_closed_tabs_provider.h"

#include <algorithm>
#include <set>
#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/i18n/case_conversion.h"
#include "base/strings/escape.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_classification.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/autocomplete_provider_listener.h"
#include "components/omnibox/browser/page_classification_functions.h"
#include "components/omnibox/browser/scoring_functor.h"
#include "components/omnibox/browser/suggestion_group_util.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/query_parser/query_parser.h"
#include "components/sessions/core/serialized_navigation_entry.h"
#include "components/sessions/core/tab_restore_types.h"
#include "components/url_formatter/url_formatter.h"
#include "content/public/common/url_constants.h"
#include "third_party/omnibox_proto/groups.pb.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/browser_ui/util/android/url_constants.h"
#endif

namespace {

// LINT.IfChange(TabSearchRecentlyClosedMaxMatches)
// The maximum number of recently closed tab matches, one per section slot.
constexpr size_t kMaxMatches = 5;
// LINT.ThenChange(//components/omnibox/browser/autocomplete_grouper_sections.cc:TabSearchRecentlyClosedZPSSectionSlots,
//   //components/omnibox/browser/autocomplete_grouper_sections.cc:TabSearchRecentlyClosedNonZPSSectionSlots
// )

// The maximum relevance of a typed match, below most open tab matches.
// TODO(crbug.com/575623966): A typed match may be folded into a history
// duplicate with the same URL, and so shown under History instead, since
// history can win the deduplication tiebreaker on relevance.
constexpr int kMaxTypedRelevance = 1000;

bool ShouldSkipUrl(const GURL& url) {
  if (!url.is_valid()) {
    return true;
  }
#if BUILDFLAG(IS_ANDROID)
  if (url.SchemeIs(browser_ui::kChromeUINativeScheme) ||
      url.SchemeIs(content::kChromeUIScheme)) {
    return true;
  }
#endif
  return false;
}

// Returns 0 unless every input term is found in the title or URL. Simplified
// from `OpenTabProvider`'s scoring.
int ScoreTypedMatch(const query_parser::QueryNodeVector& input_query_nodes,
                    const std::u16string& title,
                    const GURL& url) {
  const std::u16string lower_title = base::i18n::ToLower(title);
  query_parser::QueryWordVector title_words;
  query_parser::QueryParser::ExtractQueryWords(lower_title, &title_words);

  const std::u16string lower_url =
      base::i18n::ToLower(base::UTF8ToUTF16(url.spec()));
  query_parser::QueryWordVector url_words;
  query_parser::QueryParser::ExtractQueryWords(lower_url, &url_words);

  query_parser::Snippet::MatchPositions title_matches;
  query_parser::Snippet::MatchPositions url_matches;
  if (!std::ranges::all_of(input_query_nodes, [&](const auto& query_node) {
        // Avoid short-circuiting so URL matches are collected alongside title
        // matches.
        const bool has_title_match =
            query_node->HasMatchIn(title_words, &title_matches);
        const bool has_url_match =
            query_node->HasMatchIn(url_words, &url_matches);
        return has_title_match || has_url_match;
      })) {
    return 0;
  }

  const double title_factor =
      std::for_each(title_matches.begin(), title_matches.end(),
                    ScoringFunctor(lower_title.length()))
          .ScoringFactor();
  const double url_factor =
      std::for_each(url_matches.begin(), url_matches.end(),
                    ScoringFunctor(lower_url.length()))
          .ScoringFactor();
  const double normalized_factors =
      std::min((title_factor + url_factor) / (lower_title.length() + 10), 1.0);
  return normalized_factors * kMaxTypedRelevance;
}

}  // namespace

RecentlyClosedTabsProvider::RecentlyClosedTabsProvider(
    AutocompleteProviderClient* client,
    AutocompleteProviderListener* listener)
    : AutocompleteProvider(AutocompleteProvider::Type::kRecentlyClosedTabs),
      client_(client) {
  AddListener(listener);
}

RecentlyClosedTabsProvider::~RecentlyClosedTabsProvider() = default;

void RecentlyClosedTabsProvider::Start(const AutocompleteInput& input,
                                       bool minimal_changes) {
  // Currently only enabled for the Android Tab Search overlay.
  if (omnibox::IsAndroidTabSearch(input.current_page_classification())) {
    if (base::FeatureList::IsEnabled(omnibox::kAndroidTabSearchRecentTabs)) {
      StartWithTabRestoreService(input);
    } else {
      Stop(AutocompleteStopReason::kClobbered);
    }
    return;
  }

#if BUILDFLAG(IS_ANDROID)
  // This provider exists for every Android omnibox when the feature is
  // enabled, so don't fall through to the desktop placeholder.
  matches_.clear();
#else
  // TODO(crbug.com/574631471): Remove this placeholder or replace it with real
  // results.
  if (minimal_changes) {
    return;
  }

  matches_.clear();
  AutocompleteMatch match{this, 2000, false,
                          omnibox::AutocompleteMatchType::kHistoryUrl};
  match.destination_url = GURL{"https://google.com"};
  match.contents = u"";
  match.description = u"";
  // Zero suggest results should always omit protocols and never appear bold.
  auto format_types = AutocompleteMatch::GetFormatTypes(false, false);
  match.contents = url_formatter::FormatUrl(match.destination_url, format_types,
                                            base::UnescapeRule::SPACES, nullptr,
                                            nullptr, nullptr);
  match.description_class = ClassifyTermMatches({}, match.description.length(),
                                                ACMatchClassification::NONE,
                                                ACMatchClassification::NONE);
  match.contents_class = ClassifyTermMatches({}, match.contents.length(),
                                             ACMatchClassification::NONE,
                                             ACMatchClassification::URL);
  matches_.push_back(match);
#endif  // BUILDFLAG(IS_ANDROID)
}

void RecentlyClosedTabsProvider::Stop(AutocompleteStopReason stop_reason) {
  AutocompleteProvider::Stop(stop_reason);
  // Drop any pending load, including on inactivity, so that late results do
  // not reshuffle suggestions the user may already be interacting with.
  pending_input_.reset();
}

void RecentlyClosedTabsProvider::TabRestoreServiceLoaded(
    sessions::TabRestoreService* service) {
  // The service only loads once, so there is nothing left to observe.
  tab_restore_service_observation_.Reset();
  if (!pending_input_) {
    return;
  }

  const AutocompleteInput input = *std::exchange(pending_input_, std::nullopt);
  AddMatches(input, *service);
  done_ = true;
  NotifyListeners(!matches_.empty());
}

void RecentlyClosedTabsProvider::TabRestoreServiceDestroyed(
    sessions::TabRestoreService* service) {
  tab_restore_service_observation_.Reset();
  pending_input_.reset();
  done_ = true;
}

void RecentlyClosedTabsProvider::StartWithTabRestoreService(
    const AutocompleteInput& input) {
  Stop(AutocompleteStopReason::kClobbered);

  // Recently closed tabs are not available in incognito, matching desktop.
  if (client_->IsOffTheRecord()) {
    return;
  }

  sessions::TabRestoreService* service = client_->GetTabRestoreService();
  if (!service) {
    return;
  }

  if (!service->IsLoaded()) {
    // Wait for the load unless only synchronous matches are allowed.
    if (!input.omit_asynchronous_matches()) {
      pending_input_ = input;
      if (!tab_restore_service_observation_.IsObservingSource(service)) {
        tab_restore_service_observation_.Reset();
        tab_restore_service_observation_.Observe(service);
      }
      done_ = false;
    }
    service->LoadTabsFromLastSession();
    return;
  }

  AddMatches(input, *service);
}

void RecentlyClosedTabsProvider::AddMatches(
    const AutocompleteInput& input,
    const sessions::TabRestoreService& service) {
  const bool is_zero_suggest = input.IsZeroSuggest() || input.text().empty();

  query_parser::QueryNodeVector input_query_nodes;
  if (!is_zero_suggest) {
    query_parser::QueryParser::ParseQueryNodes(
        std::u16string(base::TrimWhitespace(base::i18n::ToLower(input.text()),
                                            base::TrimPositions::TRIM_ALL)),
        query_parser::MatchingAlgorithm::ALWAYS_PREFIX_SEARCH,
        &input_query_nodes);
  }

  // Entries are ordered from most to least recently closed.
  std::set<GURL> seen_urls;
  for (const auto& entry : service.entries()) {
    if (is_zero_suggest && matches_.size() >= kMaxMatches) {
      break;
    }
    // TODO(crbug.com/574632670): Support groups, windows and split views.
    if (entry->type != sessions::tab_restore::TAB) {
      continue;
    }

    const auto& tab = static_cast<const sessions::tab_restore::Tab&>(*entry);
    if (tab.navigations.empty()) {
      continue;
    }
    const sessions::SerializedNavigationEntry& navigation =
        tab.navigations[tab.normalized_navigation_index()];
    const GURL& url = navigation.virtual_url();
    // Keep only the most recent tab per URL so duplicates don't use up slots.
    if (ShouldSkipUrl(url) || seen_urls.contains(url)) {
      continue;
    }

    // In zero-prefix, preserve the most recently closed ordering.
    const int relevance =
        is_zero_suggest
            ? omnibox::kRecentlyClosedTabZeroSuggestRelevance -
                  static_cast<int>(matches_.size())
            : ScoreTypedMatch(input_query_nodes, navigation.title(), url);
    if (relevance <= 0) {
      continue;
    }
    seen_urls.insert(url);
    matches_.push_back(CreateRecentlyClosedTabMatch(input, navigation.title(),
                                                    url, relevance));
  }

  if (!is_zero_suggest) {
    std::ranges::stable_sort(matches_, std::ranges::greater(),
                             &AutocompleteMatch::relevance);
    if (matches_.size() > kMaxMatches) {
      matches_.resize(kMaxMatches);
    }
  }
}

AutocompleteMatch RecentlyClosedTabsProvider::CreateRecentlyClosedTabMatch(
    const AutocompleteInput& input,
    const std::u16string& title,
    const GURL& url,
    int relevance) {
  AutocompleteMatch match(this, relevance, /*deletable=*/false,
                          omnibox::AutocompleteMatchType::kRecentlyClosedTab);
  match.destination_url = url;
  match.fill_into_edit = base::UTF8ToUTF16(url.spec());
  match.allowed_to_be_default_match = true;
  match.suggestion_group_id = omnibox::GROUP_RECENTLY_CLOSED_TABS;
  // Restore the closed tab rather than switching to an open tab with the same
  // URL.
  match.has_tab_match = false;

  // Elide all optional parts of the URL, matching open tab suggestions.
  match.contents = url_formatter::FormatUrl(
      url,
      AutocompleteMatch::GetFormatTypes(/*preserve_scheme=*/false,
                                        /*preserve_subdomain=*/false),
      base::UnescapeRule::SPACES, nullptr, nullptr, nullptr);
  match.contents_class = ClassifyTermMatches(
      FindTermMatches(input.text(), match.contents), match.contents.size(),
      ACMatchClassification::MATCH | ACMatchClassification::URL,
      ACMatchClassification::URL);

  match.description = title;
  match.description_class = ClassifyTermMatches(
      FindTermMatches(input.text(), match.description),
      match.description.size(), ACMatchClassification::MATCH,
      ACMatchClassification::NONE);

  return match;
}
