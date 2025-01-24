// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_ENTERPRISE_SEARCH_AGGREGATOR_PROVIDER_H_
#define COMPONENTS_OMNIBOX_BROWSER_ENTERPRISE_SEARCH_AGGREGATOR_PROVIDER_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/autocomplete_provider_debouncer.h"

namespace network {
class SimpleURLLoader;
}

class AutocompleteProviderClient;
class AutocompleteProviderDebouncer;
class AutocompleteInput;

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

  // Determines whether the profile/session/window meet the feature
  // prerequisites.
  bool IsProviderAllowed(const AutocompleteInput& input);

  // Called by `debouncer_`, queued when `Start()` is called.
  void Run();

  // Callback for when the loader is available with a valid token. Takes
  // ownership of the loader.
  void RequestStarted(std::unique_ptr<network::SimpleURLLoader> loader);

  // Called when the network request for suggestions has completed.
  void RequestCompleted(const network::SimpleURLLoader* source,
                        const int response_code,
                        std::unique_ptr<std::string> response_body);

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

  // Used to ensure that we don't send multiple requests in quick succession.
  std::unique_ptr<AutocompleteProviderDebouncer> debouncer_;

  // Saved when starting a new autocomplete request so that it can be retrieved
  // when responses return asynchronously.
  AutocompleteInput input_;

  // Loader used to retrieve results.
  std::unique_ptr<network::SimpleURLLoader> loader_;

  base::WeakPtrFactory<EnterpriseSearchAggregatorProvider> weak_ptr_factory_{
      this};
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_ENTERPRISE_SEARCH_AGGREGATOR_PROVIDER_H_
