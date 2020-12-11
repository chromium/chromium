// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_HINTS_FETCHER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_HINTS_FETCHER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "url/gurl.h"

class PrefService;

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

namespace optimization_guide {

// Status of a request to fetch hints.
// This enum must remain synchronized with the enum
// |OptimizationGuideHintsFetcherRequestStatus| in
// tools/metrics/histograms/enums.xml.
enum class HintsFetcherRequestStatus {
  // No fetch status known. Used in testing.
  kUnknown,
  // Fetch request was sent and a response received.
  kSuccess,
  // Fetch request was sent but no response received.
  kResponseError,
  // Fetch request not sent because of offline network status.
  kNetworkOffline,
  // Fetch request not sent because fetcher was busy with another request.
  kFetcherBusy,
  // Fetch request not sent because the host and URL lists were empty.
  kNoHostsOrURLsToFetch,
  // Fetch request not sent because no supported optimization types were
  // provided.
  kNoSupportedOptimizationTypes,

  // Insert new values before this line.
  kMaxValue = kNoSupportedOptimizationTypes
};

// Callback to inform the caller that the remote hints have been fetched and
// to pass back the fetched hints response from the remote Optimization Guide
// Service.
using HintsFetchedCallback = base::OnceCallback<void(
    base::Optional<std::unique_ptr<proto::GetHintsResponse>>)>;

// A class to handle requests for optimization hints from a remote Optimization
// Guide Service.
//
// This class fetches new hints from the remote Optimization Guide Service.
// Owner must ensure that |hint_cache| remains alive for the lifetime of
// |HintsFetcher|.
class HintsFetcher {
 public:
  HintsFetcher(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const GURL& optimization_guide_service_url,
      PrefService* pref_service);
  virtual ~HintsFetcher();

  // Requests hints from the Optimization Guide Service if a request for them is
  // not already in progress. Returns whether a new request was issued.
  // |hints_fetched_callback| is run once when the outcome of this request is
  // determined (whether a request was actually sent or not). Virtualized for
  // testing. Hints fetcher may fetch hints for only a subset of the provided
  // |hosts|. |hosts| should be an ordered list in descending order of
  // probability that the hints are needed for that host. Only supported |urls|
  // will be included in the fetch. |urls| is an ordered list in descending
  // order of probability that a hint will be needed for the URL. The supplied
  // optimization types will be included in the request, if empty no fetch will
  // be made.
  virtual bool FetchOptimizationGuideServiceHints(
      const std::vector<std::string>& hosts,
      const std::vector<GURL>& urls,
      const base::flat_set<optimization_guide::proto::OptimizationType>&
          optimization_types,
      optimization_guide::proto::RequestContext request_context,
      HintsFetchedCallback hints_fetched_callback);

  // Set |time_clock_| for testing.
  void SetTimeClockForTesting(const base::Clock* time_clock);

  // Clear all the hosts and expiration times from the
  // HintsFetcherHostsSuccessfullyFetched dictionary pref.
  static void ClearHostsSuccessfullyFetched(PrefService* pref_service);

  // Return whether the host was covered by a hints fetch and any returned hints
  // would not have expired.
  static bool WasHostCoveredByFetch(PrefService* pref_service,
                                    const std::string& host);
  static bool WasHostCoveredByFetch(PrefService* pref_service,
                                    const std::string& host,
                                    const base::Clock* clock);

 private:
  // URL loader completion callback.
  void OnURLLoadComplete(std::unique_ptr<std::string> response_body);

  // Handles the response from the remote Optimization Guide Service.
  // |response| is the response body, |status| is the
  // |net::Error| of the response, and response_code is the HTTP
  // response code (if available).
  void HandleResponse(const std::string& response,
                      int status,
                      int response_code);

  // Stores the hosts in |hosts_fetched_| in the
  // HintsFetcherHostsSuccessfullyFetched dictionary pref. The value stored for
  // each host is the time that the hints fetched for each host will expire.
  // |hosts_fetched_| is cleared once the hosts are stored
  // in the pref.
  void UpdateHostsSuccessfullyFetched(base::TimeDelta valid_duration);

  // Returns the subset of hosts from |hosts| for which the hints should be
  // refreshed. The count of returned hosts is limited to
  // features::MaxHostsForOptimizationGuideServiceHintsFetch().
  std::vector<std::string> GetSizeLimitedHostsDueForHintsRefresh(
      const std::vector<std::string>& hosts) const;

  // Used to hold the callback while the SimpleURLLoader performs the request
  // asynchronously.
  HintsFetchedCallback hints_fetched_callback_;

  // The URL for the remote Optimization Guide Service.
  const GURL optimization_guide_service_url_;

  // Holds the |URLLoader| for an active hints request.
  std::unique_ptr<network::SimpleURLLoader> active_url_loader_;

  // Context of the fetch request. Opaque field that's returned back in the
  // callback and is also included in the requests to the hints server.
  optimization_guide::proto::RequestContext request_context_;

  // A reference to the PrefService for this profile. Not owned.
  PrefService* pref_service_ = nullptr;

  // Holds the hosts being requested by the hints fetcher.
  std::vector<std::string> hosts_fetched_;

  // Clock used for recording time that the hints fetch occurred.
  const base::Clock* time_clock_;

  // Used for creating an |active_url_loader_| when needed for request hints.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // The start time of the current hints fetch, used to determine the latency in
  // retrieving hints from the remote Optimization Guide Service.
  base::TimeTicks hints_fetch_start_time_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(HintsFetcher);
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_HINTS_FETCHER_H_
