// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NETWORK_TIME_NETWORK_TIME_TRACKER_H_
#define COMPONENTS_NETWORK_TIME_NETWORK_TIME_TRACKER_H_

#include <stdint.h>
#include <memory>

#include "base/feature_list.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "url/gurl.h"

class PrefRegistrySimple;
class PrefService;

namespace base {
class TickClock;
}  // namespace base

namespace client_update_protocol {
class Ecdsa;
}  // namespace client_update_protocol

namespace network {
class SimpleURLLoader;
class SharedURLLoaderFactory;
}  // namespace network

namespace network_time {

// Clock resolution is platform dependent.
#if defined(OS_WIN)
const int64_t kTicksResolutionMs = base::Time::kMinLowResolutionThresholdMs;
#else
const int64_t kTicksResolutionMs = 1;  // Assume 1ms for non-windows platforms.
#endif

// Variations Service feature that enables network time service querying.
extern const base::Feature kNetworkTimeServiceQuerying;

// A class that receives network time updates and can provide the network time
// for a corresponding local time. This class is not thread safe.
class NetworkTimeTracker {
 public:
  // Describes the result of a GetNetworkTime() call, describing whether
  // network time was available and if not, why not.
  enum NetworkTimeResult {
    // Network time is available.
    NETWORK_TIME_AVAILABLE,
    // A time has been retrieved from the network in the past, but
    // network time is no longer available because the tracker fell out
    // of sync due to, for example, a suspend/resume.
    NETWORK_TIME_SYNC_LOST,
    // Network time is unavailable because the tracker has not yet
    // attempted to retrieve a time from the network.
    NETWORK_TIME_NO_SYNC_ATTEMPT,
    // Network time is unavailable because the tracker has not yet
    // successfully retrieved a time from the network (at least one
    // attempt has been made but all have failed).
    NETWORK_TIME_NO_SUCCESSFUL_SYNC,
    // Network time is unavailable because the tracker has not yet
    // attempted to retrieve a time from the network, but the first
    // attempt is currently pending.
    NETWORK_TIME_FIRST_SYNC_PENDING,
    // Network time is unavailable because the tracker has made failed
    // attempts to retrieve a time from the network, but an attempt is
    // currently pending.
    NETWORK_TIME_SUBSEQUENT_SYNC_PENDING,
  };

  // Describes the behavior of fetches to the network time service.
  enum FetchBehavior {
    // Only used in case of an unrecognize Finch experiment parameter.
    FETCH_BEHAVIOR_UNKNOWN,
    // Time queries will be issued in the background as needed.
    FETCHES_IN_BACKGROUND_ONLY,
    // Time queries will not be issued except when StartTimeFetch() is called.
    FETCHES_ON_DEMAND_ONLY,
    // Time queries will be issued both in the background as needed and also
    // on-demand.
    FETCHES_IN_BACKGROUND_AND_ON_DEMAND,
  };

  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Constructor.  Arguments may be stubbed out for tests. |url_loader_factory|
  // must be non-null unless the kNetworkTimeServiceQuerying is disabled.
  // Otherwise, time is available only if |UpdateNetworkTime| is called.
  NetworkTimeTracker(
      std::unique_ptr<base::Clock> clock,
      std::unique_ptr<const base::TickClock> tick_clock,
      PrefService* pref_service,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~NetworkTimeTracker();

  // Sets |network_time| to an estimate of the true time.  Returns
  // NETWORK_TIME_AVAILABLE if time is available. If |uncertainty| is
  // non-NULL, it will be set to an estimate of the error range.
  //
  // If network time is unavailable, this method returns
  // NETWORK_TIME_SYNC_LOST or NETWORK_TIME_NO_SYNC to indicate the
  // reason.
  //
  // Network time may be available on startup if deserialized from a pref.
  // Failing that, a call to |UpdateNetworkTime| is required to make time
  // available to callers of |GetNetworkTime|.  Subsequently, network time may
  // become unavailable if |NetworkTimeTracker| has reason to believe it is no
  // longer accurate.  Consumers should even be prepared to handle the case
  // where calls to |GetNetworkTime| never once succeeds.
  NetworkTimeResult GetNetworkTime(base::Time* network_time,
                                   base::TimeDelta* uncertainty) const;

  // Starts a network time query if network time isn't already available
  // and if there isn't already a time query in progress. If a new query
  // is started or if there is one already in progress, |callback| will
  // run when the query completes.
  //
  // Returns true if a time query is started or was already in progress,
  // and false otherwise. For example, this method may return false if
  // time queries are disabled or if network time is already available.
  bool StartTimeFetch(base::OnceClosure callback);

  // Calculates corresponding time ticks according to the given parameters.
  // The provided |network_time| is precise at the given |resolution| and
  // represent the time between now and up to |latency| + (now - |post_time|)
  // ago.
  void UpdateNetworkTime(base::Time network_time,
                         base::TimeDelta resolution,
                         base::TimeDelta latency,
                         base::TimeTicks post_time);

  bool AreTimeFetchesEnabled() const;
  FetchBehavior GetFetchBehavior() const;

  void SetMaxResponseSizeForTesting(size_t limit);

  void SetPublicKeyForTesting(const base::StringPiece& key);

  void SetTimeServerURLForTesting(const GURL& url);

  GURL GetTimeServerURLForTesting() const;

  bool QueryTimeServiceForTesting();

  void WaitForFetchForTesting(uint32_t nonce);

  void OverrideNonceForTesting(uint32_t nonce);

  base::TimeDelta GetTimerDelayForTesting() const;

 private:
  // Checks whether a network time query should be issued, and issues one if so.
  // Upon response, execution resumes in |OnURLFetchComplete|.
  void CheckTime();

  // Updates network time from a time server response, returning true
  // if successful.
  bool UpdateTimeFromResponse(std::unique_ptr<std::string> response_body);

  // Called to process responses from the secure time service.
  void OnURLLoaderComplete(std::unique_ptr<std::string> response_body);

  // Sets the next time query to be run at the specified time.
  void QueueCheckTime(base::TimeDelta delay);

  // Returns true if there's sufficient reason to suspect that
  // NetworkTimeTracker does not know what time it is.  This returns true
  // unconditionally every once in a long while, just to be on the safe side.
  bool ShouldIssueTimeQuery();

  // State variables for internally-managed secure time service queries.
  GURL server_url_;
  size_t max_response_size_;
  base::TimeDelta backoff_;
  // Timer that runs CheckTime().  All backoff and delay is implemented by
  // changing the delay of this timer, with the result that CheckTime() may
  // assume that if it runs, it is eligible to issue a time query.
  base::RepeatingTimer timer_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::unique_ptr<network::SimpleURLLoader> time_fetcher_;
  base::TimeTicks fetch_started_;
  std::unique_ptr<client_update_protocol::Ecdsa> query_signer_;

  // The |Clock| and |TickClock| are used to sanity-check one another, allowing
  // the NetworkTimeTracker to notice e.g. suspend/resume events and clock
  // resets.
  std::unique_ptr<base::Clock> clock_;
  std::unique_ptr<const base::TickClock> tick_clock_;

  PrefService* pref_service_;

  // Network time based on last call to UpdateNetworkTime().
  mutable base::Time network_time_at_last_measurement_;

  // The estimated local times that correspond with |network_time_|. Assumes
  // the actual network time measurement was performed midway through the
  // latency time.  See UpdateNetworkTime(...) implementation for details.  The
  // tick clock is the one actually used to return values to callers, but both
  // clocks must agree to within some tolerance.
  base::Time time_at_last_measurement_;
  base::TimeTicks ticks_at_last_measurement_;

  // Uncertainty of |network_time_| based on added inaccuracies/resolution.  See
  // UpdateNetworkTime(...) implementation for details.
  base::TimeDelta network_time_uncertainty_;

  // True if any time query has completed (but not necessarily succeeded) in
  // this NetworkTimeTracker's lifetime.
  bool time_query_completed_;

  // The time that was received from the last network time fetch made by
  // CheckTime(). Unlike |network_time_at_least_measurement_|, this time
  // is not updated when UpdateNetworkTime() is called. Used for UMA
  // metrics.
  base::Time last_fetched_time_;

  // Callbacks to run when the in-progress time fetch completes.
  std::vector<base::OnceClosure> fetch_completion_callbacks_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(NetworkTimeTracker);
};

}  // namespace network_time

#endif  // COMPONENTS_NETWORK_TIME_NETWORK_TIME_TRACKER_H_
