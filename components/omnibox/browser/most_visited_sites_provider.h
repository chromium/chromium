// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_MOST_VISITED_SITES_PROVIDER_H_
#define COMPONENTS_OMNIBOX_BROWSER_MOST_VISITED_SITES_PROVIDER_H_

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/autocomplete_provider_listener.h"
#include "ui/base/device_form_factor.h"

// Autocomplete provider serving Most Visited Sites in zero-prefix context.
// Serves most frequently visited URLs in a form of either individual- or
// aggregate suggestions.
class MostVisitedSitesProvider : public AutocompleteProvider {
 public:
  MostVisitedSitesProvider(AutocompleteProviderClient* client,
                           AutocompleteProviderListener* listener);

  void Start(const AutocompleteInput& input, bool minimal_changes) override;
  void Stop(bool clear_cached_results, bool due_to_user_inactivity) override;
  void DeleteMatch(const AutocompleteMatch& match) override;
  void DeleteMatchElement(const AutocompleteMatch& match,
                          size_t element) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(MostVisitedSitesProviderTest,
                           AllowMostVisitedSitesSuggestions);
  FRIEND_TEST_ALL_PREFIXES(MostVisitedSitesProviderTest, NoSRPCoverage);

  ~MostVisitedSitesProvider() override;

  // When the TopSites service serves the most visited URLs, this function
  // converts those urls to AutocompleteMatches and adds them to |matches_|.
  void OnMostVisitedUrlsAvailable(const history::MostVisitedURLList& urls);

  // Whether zero suggest suggestions are allowed in the given context.
  // Invoked early, confirms all the external conditions for ZeroSuggest are
  // met.
  bool AllowMostVisitedSitesSuggestions(const AutocompleteInput& input) const;

  void BlockURL(const GURL& site_url);

  const ui::DeviceFormFactor device_form_factor_;
  const raw_ptr<AutocompleteProviderClient, DanglingUntriaged> client_;
  // Note: used to cancel requests - not a general purpose WeakPtr factory.
  base::WeakPtrFactory<MostVisitedSitesProvider> request_weak_ptr_factory_{
      this};
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_MOST_VISITED_SITES_PROVIDER_H_
