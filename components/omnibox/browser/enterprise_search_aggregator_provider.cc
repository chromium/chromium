// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "enterprise_search_aggregator_provider.h"

#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/common/omnibox_feature_configs.h"

EnterpriseSearchAggregatorProvider::EnterpriseSearchAggregatorProvider()
    : AutocompleteProvider(
          AutocompleteProvider::TYPE_ENTERPRISE_SEARCH_AGGREGATOR) {}

EnterpriseSearchAggregatorProvider::~EnterpriseSearchAggregatorProvider() =
    default;

void EnterpriseSearchAggregatorProvider::Start(const AutocompleteInput& input,
                                               bool minimal_changes) {
  if (!omnibox_feature_configs::SearchAggregatorProvider::Get()
           .AreMockEnginesValid()) {
    return;
  }

  matches_.clear();
  AutocompleteMatch match{this, 1000, false,
                          AutocompleteMatchType::FEATURED_ENTERPRISE_SEARCH};
  match.destination_url = GURL{"https://google.com"};
  match.contents = u"This is a FEATURED_ENTERPRISE_SEARCH match";
  match.description = u"This is a FEATURED_ENTERPRISE_SEARCH match";
  matches_.push_back(match);
}

void EnterpriseSearchAggregatorProvider::Stop(bool clear_cached_results,
                                              bool due_to_user_inactivity) {
  done_ = true;
}
