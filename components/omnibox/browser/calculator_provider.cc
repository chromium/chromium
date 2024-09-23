// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "calculator_provider.h"

#include <limits>
#include <vector>

#include "base/check.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/autocomplete_provider_listener.h"
#include "components/omnibox/browser/omnibox_feature_configs.h"
#include "components/omnibox/browser/provider_state_service.h"
#include "components/omnibox/browser/search_provider.h"

CalculatorProvider::CalculatorProvider(AutocompleteProviderClient* client,
                                       AutocompleteProviderListener* listener,
                                       SearchProvider* search_provider)
    : AutocompleteProvider(AutocompleteProvider::TYPE_CALCULATOR),
      client_(client),
      search_provider_(search_provider) {
  CHECK(search_provider_);
  AddListener(listener);
  search_provider->AddListener(this);
}

CalculatorProvider::~CalculatorProvider() = default;

void CalculatorProvider::Start(const AutocompleteInput& input,
                               bool minimal_changes) {
  if (!omnibox_feature_configs::CalcProvider::Get().enabled)
    return;

  matches_.clear();
  done_ = input.omit_asynchronous_matches();
  if (done_)
    return;

  input_ = input.text();
  grew_input_ = base::StartsWith(input.text(), last_calc_input_,
                                 base::CompareCase::SENSITIVE);
  shrunk_input_ = base::StartsWith(last_calc_input_, input.text(),
                                   base::CompareCase::SENSITIVE);

  if (since_last_calculator_suggestion_ < SIZE_MAX)
    since_last_calculator_suggestion_++;

  if (search_provider_->done()) {
    // If the search provider finished before this started (unlikely), read the
    // search suggestions and finalize the calc suggestions immediately.
    UpdateFromSearch();
  } else if (Show()) {
    // Otherwise, show the cached matches now. Then `OnProviderUpdate()` will
    // update the matches when the search provider finishes.
    AddMatches();
  }
}

void CalculatorProvider::Stop(bool clear_cached_results,
                              bool due_to_user_inactivity) {
  done_ = true;
}

void CalculatorProvider::DeleteMatch(const AutocompleteMatch& match) {
  auto it = base::ranges::find_if(Cache(), [&](const auto& cached) {
    return cached.match.destination_url == match.destination_url;
  });
  if (it != Cache().end()) {
    Cache().erase(it);
    AddMatches();
  }
}

void CalculatorProvider::OnProviderUpdate(
    bool updated_matches,
    const AutocompleteProvider* provider) {
  CHECK_EQ(provider, search_provider_);
  if (search_provider_->done() && !done_)
    UpdateFromSearch();
}

void CalculatorProvider::UpdateFromSearch() {
  CHECK(!done_);

  // Find and cache any search calculator matches.
  for (const auto& match : search_provider_->matches()) {
    if (match.type == AutocompleteMatchType::CALCULATOR)
      AddMatchToCache(match);
  }

  done_ = true;
  bool show = Show();
  if (show)
    AddMatches();
  NotifyListeners(show);
}

void CalculatorProvider::AddMatchToCache(AutocompleteMatch match) {
  // Set provider to null so the cache doesn't contain dangling pointers if this
  // provider is deleted (i.e. the window it belongs to is closed).
  match.provider = nullptr;
  match.deletable = true;
  match.allowed_to_be_default_match = false;
  match.additional_info.clear();
  match.RecordAdditionalInfo("original relevance", match.relevance);
  match.RecordAdditionalInfo("input", input_);

  // As the user types out an input, e.g. '1+22+33', replace the intermediate
  // matches to avoid showing all of: '1+2=3', '1+22=23', '1+22+3=26', &
  // '1+22+33=56'.
  if (!Cache().empty() && grew_input_ && !last_calc_input_.empty())
    Cache().pop_back();

  // Remove duplicates to avoid a repeated match reducing cache capacity.
  auto duplicate = base::ranges::find_if(Cache(), [&](const auto& cached) {
    return cached.match.contents == match.contents;
  });
  if (duplicate != Cache().end())
    Cache().erase(duplicate);

  if (Cache().size() >
      omnibox_feature_configs::CalcProvider::Get().max_matches) {
    Cache().erase(Cache().begin());
  }

  Cache().push_back({std::move(match), base::TimeTicks::Now()});
  last_calc_input_ = input_;
  since_last_calculator_suggestion_ = 0;
}

void CalculatorProvider::AddMatches() {
  // Expire old cached matches.
  const auto now = base::TimeTicks::Now();
  std::erase_if(Cache(), [&](const auto& cached) {
    return now - cached.time > base::Hours(1);
  });

  matches_.clear();
  // Score sequentially so they're ranked sequentially.
  // TODO(manukh) Consider enforcing hard grouping (e.g. search v URL).
  int relevance = omnibox_feature_configs::CalcProvider::Get().score;
  // Use copies instead of references to avoid dangling pointers. This provider
  // might be deleted before the cache (i.e. the window this provider belongs to
  // might be closed).
  for (auto [match, _] : Cache()) {
    match.relevance = relevance++;
    match.provider = this;
    matches_.push_back(match);
  }
}

bool CalculatorProvider::Show() {
  // Show matches if either:
  // a) the current input is has a calc search suggestion; e.g. '3+2'.
  if (since_last_calculator_suggestion_ == 0)
    return true;
  // b) OR one of the recent inputs had a calc search suggestion and this input
  // is similar to it. E.g. '3+2-' won't have a calc search suggestion as it's
  // incomplete. This helps keep the suggestions stable.
  return (grew_input_ || shrunk_input_) &&
         since_last_calculator_suggestion_ <=
             omnibox_feature_configs::CalcProvider::Get().num_non_calc_inputs;
}

std::vector<ProviderStateService::CachedAutocompleteMatch>&
CalculatorProvider::Cache() {
  return client_->GetProviderStateService()->calculator_provider_cache;
}
