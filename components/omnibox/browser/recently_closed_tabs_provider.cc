// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/recently_closed_tabs_provider.h"

#include <string>

#include "base/strings/escape.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_classification.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/autocomplete_provider_listener.h"
#include "components/url_formatter/url_formatter.h"
#include "url/gurl.h"

RecentlyClosedTabsProvider::RecentlyClosedTabsProvider(
    AutocompleteProviderClient* client,
    AutocompleteProviderListener* listener)
    : AutocompleteProvider(AutocompleteProvider::TYPE_RECENTLY_CLOSED_TABS),
      client_(client) {
  AddListener(listener);
}

RecentlyClosedTabsProvider::~RecentlyClosedTabsProvider() = default;

void RecentlyClosedTabsProvider::Start(const AutocompleteInput& input,
                                       bool minimal_changes) {
  if (minimal_changes) {
    return;
  }

  matches_.clear();
  AutocompleteMatch match{this, 2000, false,
                          AutocompleteMatchType::HISTORY_URL};
  match.destination_url = GURL{"https://google.com"};
  match.contents = u"";
  match.description = u"";
  // Zero suggest results should always omit protocols and never appear bold.
  auto format_types = AutocompleteMatch::GetFormatTypes(false, false);
  match.contents = url_formatter::FormatUrl(match.destination_url, format_types,
                                            base::UnescapeRule::SPACES, nullptr,
                                            nullptr, nullptr);
  match.description_class = ClassifyTermMatches({}, match.description.length(),
                                                0, ACMatchClassification::NONE);
  match.contents_class = ClassifyTermMatches({}, match.contents.length(), 0,
                                             ACMatchClassification::URL);
  matches_.push_back(match);
}
