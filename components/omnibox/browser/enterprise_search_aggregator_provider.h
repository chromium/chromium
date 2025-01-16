// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_ENTERPRISE_SEARCH_AGGREGATOR_PROVIDER_H_
#define COMPONENTS_OMNIBOX_BROWSER_ENTERPRISE_SEARCH_AGGREGATOR_PROVIDER_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_provider.h"

class AutocompleteProviderClient;

class EnterpriseSearchAggregatorProvider : public AutocompleteProvider {
 public:
  explicit EnterpriseSearchAggregatorProvider(
      AutocompleteProviderClient* client);

  // AutocompleteProvider:
  void Start(const AutocompleteInput& input, bool minimal_changes) override;
  void Stop(bool clear_cached_results, bool due_to_user_inactivity) override;

 private:
  friend class FakeEnterpriseSearchAggregatorProvider;

  ~EnterpriseSearchAggregatorProvider() override;

  // Helper to create a match.
  AutocompleteMatch CreateMatch(const AutocompleteInput& input,
                                const std::u16string& keyword,
                                bool is_navigation,
                                int relevance,
                                const std::string& url,
                                const std::u16string& title,
                                const std::u16string& additional_text);

  // Owned by AutocompleteController.
  const raw_ptr<AutocompleteProviderClient> client_;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_ENTERPRISE_SEARCH_AGGREGATOR_PROVIDER_H_
