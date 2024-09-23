// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_HINTS_FETCHER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_HINTS_FETCHER_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "url/gurl.h"

class OptimizationGuideLogger;
class PrefService;

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

namespace optimization_guide {

// Callback to inform the caller that the remote hints have been fetched and
// to pass back the fetched hints response from the remote Optimization Guide
// Service.
using HintsFetchedCallback = base::OnceCallback<void(
    std::optional<std::unique_ptr<proto::GetHintsResponse>>)>;

// A class to handle requests for optimization hints from a remote Optimization
// Guide Service.
//
// This class fetches new hints from the remote Optimization Guide Service.
class HintsFetcher {
 public:
  HintsFetcher(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const GURL& optimization_guide_service_url,
      PrefService* pref_service,
      OptimizationGuideLogger* optimization_guide_logger);

  HintsFetcher(const HintsFetcher&) = delete;
  HintsFetcher& operator=(const HintsFetcher&) = delete;

  virtual ~HintsFetcher();

  // Requests hints from the Optimization Guide Service if a request for them is
  // not already in progress. Returns whether a new request was issued.
  // |hints_fetched_callback| is run once when the outcome of this request is
  // determined (whether a request was actually sent or not). Virtualized for
  // testing. Hints fetcher may fetch hints for only a subset of the provided
  // |hosts|. A host may be skipped when too many are requested, or when it
  // already has a result cached, unless |skip_cache| is specified. |hosts|
  // should be an ordered list in descending order of probability that the hints
  // are needed for that host. Only supported |urls| will be included in the
  // fetch. |urls| is an ordered list in descending order of probability that a
  // hint will be needed for the URL. The supplied optimization types will be
  // included in the request, if empty no fetch will be made.
  virtual bool FetchOptimizationGuideServiceHints(
      const std::vector<std::string>& hosts,
      const std::vector<GURL>& urls,
      const base::flat_set<optimization_guide::proto::OptimizationType>&
          optimization_types,
      optimization_guide::proto::RequestContext request_context,
      const std::string& locale,
      const std::string& access_token,
      bool skip_cache,
      HintsFetchedCallback hints_fetched_callback,
      std::optional<proto::RequestContextMetadata> request_context_metadata);

  // Set |time_clock_| for testing.
  void SetTimeClockForTesting(const base::Clock* time_clock);

  // Clear all the hosts and expiration times from the
  // HintsFetcherHostsSuccessfullyFetched dictionary pref.
  static void ClearHostsSuccessfullyFetched(PrefService* pref_service);

  // Clear the given host from the HintsFetcherHostsSuccessfullyFetched
  // dictionary pref.
  static void ClearSingleFetchedHost(PrefService* pref_service,
                                     const std::string& host);

  // Adds a fetched host at the given time. Used only for testing.
  static void AddFetchedHostForTesting(PrefService* pref_service,
                                       const std::string& host,
                                       base::Time time);

  // Return whether the host was covered by a hints fetch and any returned hints
  // would not have expired.
  static bool WasHostCoveredByFetch(PrefService* pref_service,
                                    const std::string& host);
  static bool WasHostCoveredByFetch(PrefService* pref_service,
                                    const std::string& host,
                                    const base::Clock* clock);

 private:
  // URL loader completion callback.
  void OnURLLoadComplete(bool skip_cache,
                         std::unique_ptr<std::string> response_body);

  // Handles the response from the remote Optimization Guide Service.
  // |response| is the response body, |status| is the
  // |net::Error| of the response, and response_code is the HTTP
  // response code (if available).
  void HandleResponse(const std::string& response,
                      int status,
                      int response_code,
                      bool skip_cache);

  // Stores the hosts in |hosts_fetched_| in the
  // HintsFetcherHostsSuccessfullyFetched dictionary pref. The value stored for
  // each host is the time that the hints fetched for each host will expire.
  // |hosts_fetched_| is cleared once the hosts are stored
  // in the pref.
  void UpdateHostsSuccessfullyFetched(base::TimeDelta valid_duration);

  // Returns the subset of URLs from |urls| for which the URL is considered
  // valid and can be included in a hints fetch.
  std::vector<GURL> GetSizeLimitedURLsForFetching(
      const std::vector<GURL>& urls) const;

  // Returns the subset of hosts from |hosts| for which the hints should be
  // refreshed. The count of returned hosts is limited to
  // features::MaxHostsForOptimizationGuideServiceHintsFetch().
  std::vector<std::string> GetSizeLimitedHostsDueForHintsRefresh(
      const std::vector<std::string>& hosts,
      bool skip_cache) const;

  // Used to hold the callback while the SimpleURLLoader performs the request
  // asynchronously.
  HintsFetchedCallback hints_fetched_callback_;

  // The URL for the remote Optimization Guide Service.
  const GURL optimization_guide_service_url_;

  // The API key used to call the remote Optimization Guide Service when no
  // access token is present.
  const std::string optimization_guide_service_api_key_;

  // Holds the |URLLoader| for an active hints request.
  std::unique_ptr<network::SimpleURLLoader> active_url_loader_;

  // Context of the fetch request. Opaque field that's returned back in the
  // callback and is also included in the requests to the hints server.
  optimization_guide::proto::RequestContext request_context_;

  // A reference to the PrefService for this profile. Not owned.
  raw_ptr<PrefService> pref_service_ = nullptr;

  // Holds the hosts being requested by the hints fetcher.
  std::vector<std::string> hosts_fetched_;

  // Clock used for recording time that the hints fetch occurred.
  raw_ptr<const base::Clock> time_clock_;

  // Used for creating an |active_url_loader_| when needed for request hints.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // The start time of the current hints fetch, used to determine the latency in
  // retrieving hints from the remote Optimization Guide Service.
  base::TimeTicks hints_fetch_start_time_;

  // Owned by OptimizationGuideKeyedService and outlives |this|.
  raw_ptr<OptimizationGuideLogger> optimization_guide_logger_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_HINTS_FETCHER_H_
