// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/history_fuzzy_provider.h"

#include <vector>

#include "base/check.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "base/trace_event/trace_event.h"
#include "components/omnibox/browser/autocomplete_match_classification.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/search_engines/omnibox_focus_type.h"

HistoryFuzzyProvider::HistoryFuzzyProvider(AutocompleteProviderClient* client)
    : HistoryProvider(AutocompleteProvider::TYPE_HISTORY_FUZZY, client) {}

void HistoryFuzzyProvider::Start(const AutocompleteInput& input,
                                 bool minimal_changes) {
  TRACE_EVENT0("omnibox", "HistoryFuzzyProvider::Start");
  matches_.clear();
  if (input.focus_type() != OmniboxFocusType::DEFAULT ||
      input.type() == metrics::OmniboxInputType::EMPTY) {
    return;
  }

  autocomplete_input_ = input;

  DoAutocomplete();
}

size_t HistoryFuzzyProvider::EstimateMemoryUsage() const {
  size_t res = HistoryProvider::EstimateMemoryUsage();
  res += base::trace_event::EstimateMemoryUsage(autocomplete_input_);
  return res;
}

HistoryFuzzyProvider::~HistoryFuzzyProvider() = default;

void HistoryFuzzyProvider::DoAutocomplete() {
  AutocompleteMatch match(this, 10000000, false,
                          AutocompleteMatchType::HISTORY_URL);
  match.contents = u"fuzzyurlhere.org";
  match.contents_class.push_back({0, 0});
  matches_.push_back(std::move(match));
}
