// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_MOST_VISITED_SITES_PROVIDER_H_
#define COMPONENTS_OMNIBOX_BROWSER_MOST_VISITED_SITES_PROVIDER_H_

#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/timer/elapsed_timer.h"
#include "components/history/core/browser/history_types.h"
#include "components/omnibox/browser/autocomplete_enums.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/autocomplete_provider_debouncer.h"
#include "components/omnibox/browser/autocomplete_provider_listener.h"
#include "ui/base/device_form_factor.h"

using QueryMostVisitedURLsCallback =
    base::OnceCallback<void(history::MostVisitedURLList)>;

// Autocomplete provider serving Most Visited Sites in zero-prefix context.
// Serves most frequently visited URLs in a form of either individual- or
// aggregate suggestions.
class MostVisitedSitesProvider : public AutocompleteProvider {
 public:
  MostVisitedSitesProvider(AutocompleteProviderClient* client,
                           AutocompleteProviderListener* listener);

  // AutocompleteProvider:
  void StartPrefetch(const AutocompleteInput& input) override;
  void Start(const AutocompleteInput& input, bool minimal_changes) override;
  void Stop(AutocompleteStopReason stop_reason) override;
  void DeleteMatch(const AutocompleteMatch& match) override;
  void DeleteMatchElement(const AutocompleteMatch& match,
                          size_t element) override;

  // Whether zero-prefix suggestions are allowed in the given context.
  // Invoked early, confirms all the external conditions for Most Visited Sites
  // suggestions are met.
  static bool AllowMostVisitedSitesSuggestions(
      const AutocompleteProviderClient* client,
      const AutocompleteInput& input);

  // Returns the number of results to request from history.
  size_t GetRequestedResultSize(const AutocompleteInput& input) const;

  // Returns list of cached sites.
  history::MostVisitedURLList GetCachedSitesForTesting() const;

 private:
  FRIEND_TEST_ALL_PREFIXES(MostVisitedSitesProviderTest, NoSRPCoverage);
  FRIEND_TEST_ALL_PREFIXES(MostVisitedSitesProviderTest,
                           DesktopProviderDoesNotAllowChromeSites);
  FRIEND_TEST_ALL_PREFIXES(MostVisitedSitesProviderTest, BlocklistedURLs);
  FRIEND_TEST_ALL_PREFIXES(MostVisitedSitesProviderTest, DuplicateSuggestions);
  FRIEND_TEST_ALL_PREFIXES(MostVisitedSitesProviderTest, DedupingOpenTabs);

  ~MostVisitedSitesProvider() override;

  // When the TopSites service serves the most visited URLs, this function
  // converts those urls to AutocompleteMatches and adds them to |matches_|.
  void OnMostVisitedUrlsAvailable(AutocompleteInput input,
                                  const history::MostVisitedURLList& urls);

  // When the HistoryService serves the most visited URLs, this function
  // converts those urls to AutocompleteMatches and adds them to |matches_|.
  // Unlike `OnMostVisitedUrlsAvailable` which gets called through a request
  // to TopSites, this callback is invoked when HistoryService is queried
  // directly in the provider.
  void OnMostVisitedUrlsFromHistoryServiceAvailable(
      AutocompleteInput input,
      base::ElapsedTimer query_timer,
      history::MostVisitedURLList sites);

  void BlockURL(const GURL& site_url);

  // Calls HistoryService's QueryMostVisitedURLs().
  // Called in `StartPrefetch()` and in `Start()` by the debouncer.
  void RequestSitesFromHistoryService(const AutocompleteInput& input);

  // Updates the list of cached sites.
  void UpdateCachedSites(history::MostVisitedURLList sites);

  // Task tracker for querying the most visited URLs from HistoryService.
  base::CancelableTaskTracker cancelable_task_tracker_;

  // Debouncer used to throttle the frequency of calls to HistoryService's
  // `QueryMostVisitedURLs()`.
  std::unique_ptr<AutocompleteProviderDebouncer> debouncer_;

  // `cached_sites_` stores both the prefetched sites as well as sites returned
  // returned from subsequent queries to the history service when prefetching
  // is enabled.
  history::MostVisitedURLList cached_sites_;

  const ui::DeviceFormFactor device_form_factor_;
  const raw_ptr<AutocompleteProviderClient, DanglingUntriaged> client_;
  // Note: used to cancel requests - not a general purpose WeakPtr factory.
  base::WeakPtrFactory<MostVisitedSitesProvider> request_weak_ptr_factory_{
      this};
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_MOST_VISITED_SITES_PROVIDER_H_
