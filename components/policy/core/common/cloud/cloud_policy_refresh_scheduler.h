// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_CLOUD_CLOUD_POLICY_REFRESH_SCHEDULER_H_
#define COMPONENTS_POLICY_CORE_COMMON_CLOUD_CLOUD_POLICY_REFRESH_SCHEDULER_H_

#include <stdint.h>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/clock.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_refresh_scheduler_observer.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/policy_export.h"
#include "services/network/public/cpp/network_connection_tracker.h"

namespace base {
class SequencedTaskRunner;
}

namespace policy {

class CloudPolicyService;

// Observes CloudPolicyClient and CloudPolicyStore to trigger periodic policy
// fetches and issue retries on error conditions.
//
// Refreshing non-managed responses:
// - If there is a cached non-managed response, make sure to only re-query the
//   server after kUnmanagedRefreshDelayMs.
//  - NB: For existing policy, an immediate refresh is intentional.
//
// Refreshing on mobile platforms:
// - if no user is signed-in then the |client_| is never registered.
// - if the user is signed-in but isn't enterprise then the |client_| is
//   never registered.
// - if the user is signed-in but isn't registered for policy yet then the
//   |client_| isn't registered either; the UserPolicySigninService will try
//   to register, and OnRegistrationStateChanged() will be invoked later.
// - if the client is signed-in and has policy then its timestamp is used to
//   determine when to perform the next fetch, which will be once the cached
//   version is considered "old enough".
//
// If there is an old policy cache then a fetch will be performed "soon"; if
// that fetch fails then a retry is attempted after a delay, with exponential
// backoff. If those fetches keep failing then the cached timestamp is *not*
// updated, and another fetch (and subsequent retries) will be attempted
// again on the next startup.
//
// But if the cached policy is considered fresh enough then we try to avoid
// fetching again on startup; the Android logic differs from the desktop in
// this aspect.
class POLICY_EXPORT CloudPolicyRefreshScheduler
    : public CloudPolicyClient::Observer,
      public CloudPolicyStore::Observer,
      public network::NetworkConnectionTracker::NetworkConnectionObserver {
 public:
  // Refresh constants.
  static const int64_t kDefaultRefreshDelayMs;
  static const int64_t kUnmanagedRefreshDelayMs;
  static const int64_t kWithInvalidationsRefreshDelayMs;
  static const int64_t kInitialErrorRetryDelayMs;
  static const int64_t kRandomSaltDelayMaxValueMs;

  // Refresh delay bounds.
  static const int64_t kRefreshDelayMinMs;
  static const int64_t kRefreshDelayMaxMs;

  // |client|, |store| and |service| pointers must stay valid throughout the
  // lifetime of CloudPolicyRefreshScheduler.
  CloudPolicyRefreshScheduler(
      CloudPolicyClient* client,
      CloudPolicyStore* store,
      CloudPolicyService* service,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner,
      network::NetworkConnectionTrackerGetter
          network_connection_tracker_getter);
  CloudPolicyRefreshScheduler(const CloudPolicyRefreshScheduler&) = delete;
  CloudPolicyRefreshScheduler& operator=(const CloudPolicyRefreshScheduler&) =
      delete;
  ~CloudPolicyRefreshScheduler() override;

  base::Time last_refresh() const { return last_refresh_; }

  // Sets the refresh delay to |refresh_delay| (actual refresh delay may vary
  // due to min/max clamping, changes to delay due to invalidations, etc).
  void SetDesiredRefreshDelay(int64_t refresh_delay);

  // Returns the current fixed refresh delay (can vary depending on whether
  // invalidations are available or not).
  int64_t GetActualRefreshDelay() const;

  // For testing: get the value randomly assigned to refresh_delay_salt_ms_.
  int64_t GetSaltDelayForTesting() const { return refresh_delay_salt_ms_; }

  // Schedules a refresh to be performed immediately if the `client_` is
  // registered. Otherwise, this is a no-op.
  //
  // The |reason| parameter will be used to tag the request to DMServer. This
  // will allow for more targeted monitoring and alerting.
  void RefreshSoon(PolicyFetchReason reason);

  // The refresh scheduler starts by assuming that invalidations are not
  // available. This call can be used to signal whether the invalidations
  // service is available or not, and can be called multiple times.
  // When the invalidations service is available then the refresh rate is much
  // lower.
  void SetInvalidationServiceAvailability(bool is_available);

  // Whether the invalidations service is available and receiving notifications
  // of policy updates.
  bool invalidations_available() const { return invalidations_available_; }

  // CloudPolicyClient::Observer:
  void OnPolicyFetched(CloudPolicyClient* client) override;
  void OnRegistrationStateChanged(CloudPolicyClient* client) override;
  void OnClientError(CloudPolicyClient* client) override;

  // CloudPolicyStore::Observer:
  void OnStoreLoaded(CloudPolicyStore* store) override;
  void OnStoreError(CloudPolicyStore* store) override;

  // network::NetworkConnectionTracker::NetworkConnectionObserver:
  // Triggered also when the device wakes up.
  void OnConnectionChanged(network::mojom::ConnectionType type) override;

  // Overrides clock or tick clock in tests. Returned closure removes the
  // override when destroyed.
  static base::ScopedClosureRunner OverrideClockForTesting(
      base::Clock* clock_for_testing);
  static base::ScopedClosureRunner OverrideTickClockForTesting(
      base::TickClock* tick_clock_for_testing);

  // Registers an observer to be notified.
  void AddObserver(CloudPolicyRefreshSchedulerObserver* observer);

  // Removes the specified observer.
  void RemoveObserver(CloudPolicyRefreshSchedulerObserver* observer);

 private:
  // Initializes |last_refresh_| to the policy timestamp from |store_| in case
  // there is policy present that indicates this client is not managed. This
  // results in policy fetches only to occur after the entire unmanaged refresh
  // delay expires, even over restarts. For managed clients, we want to trigger
  // a refresh on every restart.
  void UpdateLastRefreshFromPolicy();

  // Evaluates when the next refresh is pending and updates the callback to
  // execute that refresh at the appropriate time.
  void ScheduleRefresh();

  // Triggers a policy refresh.
  void PerformRefresh(PolicyFetchReason reason);

  // Schedules a policy refresh to happen no later than |delta_ms| +
  // |refresh_delay_salt_ms_| msecs after |last_refresh_| or
  // |last_refresh_ticks_| whichever is sooner.
  void RefreshAfter(int delta_ms, PolicyFetchReason reason);

  // Cancels the scheduled policy refresh.
  void CancelRefresh();

  // Sets the |last_refresh_| and |last_refresh_ticks_| to current time.
  void UpdateLastRefresh();

  // Called when policy was refreshed after refresh request.
  // It is different than OnPolicyFetched(), which will be called every time
  // policy was fetched by the |client_|, does not matter where it was
  // requested.
  void OnPolicyRefreshed(bool success);

  raw_ptr<CloudPolicyClient> client_;
  raw_ptr<CloudPolicyStore> store_;
  raw_ptr<CloudPolicyService> service_;

  // For scheduling delayed tasks.
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // For listening for network connection changes.
  raw_ptr<network::NetworkConnectionTracker> network_connection_tracker_;

  // Whether the refresh is scheduled for soon (using |RefreshSoon|).
  bool is_scheduled_for_soon_ = false;

  // The last time a policy fetch was attempted or completed.
  base::Time last_refresh_;

  // The same |last_refresh_|, but based on TimeTicks. This allows to schedule
  // the refresh times based on both, system time and TimeTicks, providing a
  // protection against changes in system time.
  base::TimeTicks last_refresh_ticks_;

  // Error retry delay in milliseconds.
  int64_t error_retry_delay_ms_;

  // The refresh delay.
  int64_t refresh_delay_ms_;

  // A randomly-generated (between 0 and |kRandomSaltDelayMaxValueMs|) delay
  // added to all non-immediately scheduled refresh requests.
  const int64_t refresh_delay_salt_ms_;

  // Whether the invalidations service is available and receiving notifications
  // of policy updates.
  bool invalidations_available_;

  // Whether we have retried with key reset or not.
  bool has_retried_with_key_reset_ = false;

  base::ObserverList<CloudPolicyRefreshSchedulerObserver, true> observers_;

  // WeakPtrFactory used to schedule refresh tasks.
  base::WeakPtrFactory<CloudPolicyRefreshScheduler> refresh_weak_factory_{this};

  // General purpose WeakPtrFactory.
  base::WeakPtrFactory<CloudPolicyRefreshScheduler> weak_factory_{this};
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_CLOUD_CLOUD_POLICY_REFRESH_SCHEDULER_H_
