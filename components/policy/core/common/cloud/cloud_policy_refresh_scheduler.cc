// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/cloud_policy_refresh_scheduler.h"

#include <algorithm>
#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/metrics/histogram_macros.h"
#include "base/sequenced_task_runner.h"
#include "build/build_config.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"

namespace policy {

#if defined(OS_ANDROID) || defined(OS_IOS)

const int64_t CloudPolicyRefreshScheduler::kDefaultRefreshDelayMs =
    24 * 60 * 60 * 1000;  // 1 day.
const int64_t CloudPolicyRefreshScheduler::kUnmanagedRefreshDelayMs =
    24 * 60 * 60 * 1000;  // 1 day.
// Delay for periodic refreshes when the invalidations service is available,
// in milliseconds.
// TODO(joaodasilva): increase this value once we're confident that the
// invalidations channel works as expected.
const int64_t CloudPolicyRefreshScheduler::kWithInvalidationsRefreshDelayMs =
    24 * 60 * 60 * 1000;  // 1 day.
const int64_t CloudPolicyRefreshScheduler::kInitialErrorRetryDelayMs =
    5 * 60 * 1000;  // 5 minutes.
const int64_t CloudPolicyRefreshScheduler::kRefreshDelayMinMs =
    30 * 60 * 1000;  // 30 minutes.
const int64_t CloudPolicyRefreshScheduler::kRefreshDelayMaxMs =
    7 * 24 * 60 * 60 * 1000;  // 1 week.

#else

const int64_t CloudPolicyRefreshScheduler::kDefaultRefreshDelayMs =
    3 * 60 * 60 * 1000;  // 3 hours.
const int64_t CloudPolicyRefreshScheduler::kUnmanagedRefreshDelayMs =
    24 * 60 * 60 * 1000;  // 1 day.
// Delay for periodic refreshes when the invalidations service is available,
// in milliseconds.
const int64_t CloudPolicyRefreshScheduler::kWithInvalidationsRefreshDelayMs =
    24 * 60 * 60 * 1000;  // 1 day.
const int64_t CloudPolicyRefreshScheduler::kInitialErrorRetryDelayMs =
    5 * 60 * 1000;  // 5 minutes.
const int64_t CloudPolicyRefreshScheduler::kRefreshDelayMinMs =
    30 * 60 * 1000;  // 30 minutes.
const int64_t CloudPolicyRefreshScheduler::kRefreshDelayMaxMs =
    24 * 60 * 60 * 1000;  // 1 day.

#endif

CloudPolicyRefreshScheduler::CloudPolicyRefreshScheduler(
    CloudPolicyClient* client,
    CloudPolicyStore* store,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    network::NetworkConnectionTrackerGetter network_connection_tracker_getter)
    : client_(client),
      store_(store),
      task_runner_(task_runner),
      network_connection_tracker_(network_connection_tracker_getter.Run()),
      error_retry_delay_ms_(kInitialErrorRetryDelayMs),
      refresh_delay_ms_(kDefaultRefreshDelayMs),
      invalidations_available_(false),
      creation_time_(base::Time::NowFromSystemTime()),
      weak_factory_(this) {
  client_->AddObserver(this);
  store_->AddObserver(this);
  network_connection_tracker_->AddNetworkConnectionObserver(this);

  UpdateLastRefreshFromPolicy();
  ScheduleRefresh();
}

CloudPolicyRefreshScheduler::~CloudPolicyRefreshScheduler() {
  store_->RemoveObserver(this);
  client_->RemoveObserver(this);
  if (network_connection_tracker_)
    network_connection_tracker_->RemoveNetworkConnectionObserver(this);
}

void CloudPolicyRefreshScheduler::SetDesiredRefreshDelay(
    int64_t refresh_delay) {
  refresh_delay_ms_ = std::min(std::max(refresh_delay, kRefreshDelayMinMs),
                               kRefreshDelayMaxMs);
  ScheduleRefresh();
}

int64_t CloudPolicyRefreshScheduler::GetActualRefreshDelay() const {
  // Returns the refresh delay, possibly modified/lengthened due to the presence
  // of invalidations (we don't have to poll as often if we have policy
  // invalidations because policy invalidations provide for timely refreshes.
  if (invalidations_available_) {
    // If policy invalidations are available then periodic updates are done at
    // a much lower rate; otherwise use the |refresh_delay_ms_| value.
    return std::max(refresh_delay_ms_, kWithInvalidationsRefreshDelayMs);
  } else {
    return refresh_delay_ms_;
  }
}

void CloudPolicyRefreshScheduler::RefreshSoon() {
  // If the client isn't registered, there is nothing to do.
  if (!client_->is_registered())
    return;

  is_scheduled_for_soon_ = true;
  RefreshAfter(0);
}

void CloudPolicyRefreshScheduler::SetInvalidationServiceAvailability(
    bool is_available) {
  if (!creation_time_.is_null()) {
    base::TimeDelta elapsed = base::Time::NowFromSystemTime() - creation_time_;
    UMA_HISTOGRAM_MEDIUM_TIMES("Enterprise.PolicyInvalidationsStartupTime",
                               elapsed);
    creation_time_ = base::Time();
  }

  if (is_available == invalidations_available_) {
    // No change in state.
    return;
  }

  invalidations_available_ = is_available;

  // Schedule a refresh since the refresh delay has been updated.
  ScheduleRefresh();
}

void CloudPolicyRefreshScheduler::OnPolicyFetched(CloudPolicyClient* client) {
  error_retry_delay_ms_ = kInitialErrorRetryDelayMs;

  // Schedule the next refresh.
  UpdateLastRefresh();
  ScheduleRefresh();
}

void CloudPolicyRefreshScheduler::OnRegistrationStateChanged(
    CloudPolicyClient* client) {
  error_retry_delay_ms_ = kInitialErrorRetryDelayMs;

  // The client might have registered, so trigger an immediate refresh.
  RefreshSoon();
}

void CloudPolicyRefreshScheduler::OnClientError(CloudPolicyClient* client) {
  // Save the status for below.
  DeviceManagementStatus status = client_->status();

  // Schedule an error retry if applicable.
  UpdateLastRefresh();
  ScheduleRefresh();

  // Update the retry delay.
  if (client->is_registered() &&
      (status == DM_STATUS_REQUEST_FAILED ||
       status == DM_STATUS_TEMPORARY_UNAVAILABLE)) {
    error_retry_delay_ms_ = std::min(error_retry_delay_ms_ * 2,
                                     refresh_delay_ms_);
  } else {
    error_retry_delay_ms_ = kInitialErrorRetryDelayMs;
  }
}

void CloudPolicyRefreshScheduler::OnStoreLoaded(CloudPolicyStore* store) {
  UpdateLastRefreshFromPolicy();

  // Re-schedule the next refresh in case the is_managed bit changed.
  ScheduleRefresh();
}

void CloudPolicyRefreshScheduler::OnStoreError(CloudPolicyStore* store) {
  // If |store_| fails, the is_managed bit that it provides may become stale.
  // The best guess in that situation is to assume is_managed didn't change and
  // continue using the stale information. Thus, no specific response to a store
  // error is required. NB: Changes to is_managed fire OnStoreLoaded().
}

void CloudPolicyRefreshScheduler::OnConnectionChanged(
    network::mojom::ConnectionType type) {
  if (type == network::mojom::ConnectionType::CONNECTION_NONE)
    return;

  if (client_->status() == DM_STATUS_REQUEST_FAILED) {
    RefreshSoon();
    return;
  }

  // If this is triggered by the device wake-up event, the scheduled refresh
  // that is in the queue is off because it's based on TimeTicks. Check when the
  // the next refresh should happen based on system time, and if this provides
  // shorter delay then re-schedule the next refresh. It has to happen sooner,
  // according to delay based on system time. If we have no information about
  // the last refresh based on system time, there's nothing we can do in
  // applying the above logic.
  if (last_refresh_.is_null() || !client_->is_registered())
    return;

  const base::TimeDelta refresh_delay =
      base::TimeDelta::FromMilliseconds(GetActualRefreshDelay());
  const base::TimeDelta system_delta =
      std::max(last_refresh_ + refresh_delay - base::Time::NowFromSystemTime(),
               base::TimeDelta());
  const base::TimeDelta ticks_delta =
      last_refresh_ticks_ + refresh_delay - base::TimeTicks::Now();
  if (ticks_delta > system_delta)
    RefreshAfter(system_delta.InMilliseconds());
}

void CloudPolicyRefreshScheduler::set_last_refresh_for_testing(
    base::Time last_refresh) {
  last_refresh_ = last_refresh;
}

void CloudPolicyRefreshScheduler::UpdateLastRefreshFromPolicy() {
  if (!last_refresh_.is_null())
    return;

  // If the client has already fetched policy, assume that happened recently. If
  // that assumption ever breaks, the proper thing to do probably is to move the
  // |last_refresh_| bookkeeping to CloudPolicyClient.
  if (!client_->responses().empty()) {
    UpdateLastRefresh();
    return;
  }

#if defined(OS_ANDROID) || defined(OS_IOS)
  // Refreshing on mobile platforms:
  // - if no user is signed-in then the |client_| is never registered and
  //   nothing happens here.
  // - if the user is signed-in but isn't enterprise then the |client_| is
  //   never registered and nothing happens here.
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
  if (store_->has_policy() && store_->policy()->has_timestamp()) {
    last_refresh_ = base::Time::FromJavaTime(store_->policy()->timestamp());
    last_refresh_ticks_ = base::TimeTicks::Now() +
                          (last_refresh_ - base::Time::NowFromSystemTime());
  }
#else
  // If there is a cached non-managed response, make sure to only re-query the
  // server after kUnmanagedRefreshDelayMs. NB: For existing policy, an
  // immediate refresh is intentional.
  if (store_->has_policy() && store_->policy()->has_timestamp() &&
      !store_->is_managed()) {
    last_refresh_ = base::Time::FromJavaTime(store_->policy()->timestamp());
    last_refresh_ticks_ = base::TimeTicks::Now() +
                          (last_refresh_ - base::Time::NowFromSystemTime());
  }
#endif
}

void CloudPolicyRefreshScheduler::ScheduleRefresh() {
  // If the client isn't registered, there is nothing to do.
  if (!client_->is_registered()) {
    CancelRefresh();
    return;
  }

  // Ignore the refresh request if there's a request scheduled for soon.
  if (is_scheduled_for_soon_) {
    DCHECK(!refresh_callback_.IsCancelled());
    return;
  }

  // If there is a registration, go by the client's status. That will tell us
  // what the appropriate refresh delay should be.
  switch (client_->status()) {
    case DM_STATUS_SUCCESS:
      if (store_->is_managed())
        RefreshAfter(GetActualRefreshDelay());
      else
        RefreshAfter(kUnmanagedRefreshDelayMs);
      return;
    case DM_STATUS_SERVICE_ACTIVATION_PENDING:
    case DM_STATUS_SERVICE_POLICY_NOT_FOUND:
      RefreshAfter(GetActualRefreshDelay());
      return;
    case DM_STATUS_REQUEST_FAILED:
    case DM_STATUS_TEMPORARY_UNAVAILABLE:
    case DM_STATUS_CANNOT_SIGN_REQUEST:
      RefreshAfter(error_retry_delay_ms_);
      return;
    case DM_STATUS_REQUEST_INVALID:
    case DM_STATUS_HTTP_STATUS_ERROR:
    case DM_STATUS_RESPONSE_DECODING_ERROR:
    case DM_STATUS_SERVICE_MANAGEMENT_NOT_SUPPORTED:
      RefreshAfter(kUnmanagedRefreshDelayMs);
      return;
    case DM_STATUS_SERVICE_MANAGEMENT_TOKEN_INVALID:
    case DM_STATUS_SERVICE_DEVICE_NOT_FOUND:
    case DM_STATUS_SERVICE_INVALID_SERIAL_NUMBER:
    case DM_STATUS_SERVICE_DEVICE_ID_CONFLICT:
    case DM_STATUS_SERVICE_MISSING_LICENSES:
    case DM_STATUS_SERVICE_DEPROVISIONED:
    case DM_STATUS_SERVICE_DOMAIN_MISMATCH:
    case DM_STATUS_SERVICE_CONSUMER_ACCOUNT_WITH_PACKAGED_LICENSE:
      // Need a re-registration, no use in retrying.
      CancelRefresh();
      return;
    case DM_STATUS_SERVICE_ARC_DISABLED:
      // This doesn't occur during policy refresh, don't change the schedule.
      return;
  }

  NOTREACHED() << "Invalid client status " << client_->status();
  RefreshAfter(kUnmanagedRefreshDelayMs);
}

void CloudPolicyRefreshScheduler::PerformRefresh() {
  CancelRefresh();

  if (client_->is_registered()) {
    // Update |last_refresh_| so another fetch isn't triggered inadvertently.
    UpdateLastRefresh();

    // The result of this operation will be reported through a callback, at
    // which point the next refresh will be scheduled.
    client_->FetchPolicy();
    return;
  }

  // This should never happen, as the registration change should have been
  // handled via OnRegistrationStateChanged().
  NOTREACHED();
}

void CloudPolicyRefreshScheduler::RefreshAfter(int delta_ms) {
  const base::TimeDelta delta(base::TimeDelta::FromMilliseconds(delta_ms));

  // Schedule the callback, calculating the delay based on both, system time
  // and TimeTicks, whatever comes up to become earlier update. This is done to
  // make sure the refresh is not delayed too much when the system time moved
  // backward after the last refresh.
  const base::TimeDelta system_delay =
      std::max((last_refresh_ + delta) - base::Time::NowFromSystemTime(),
               base::TimeDelta());
  const base::TimeDelta time_ticks_delay =
      std::max((last_refresh_ticks_ + delta) - base::TimeTicks::Now(),
               base::TimeDelta());
  const base::TimeDelta delay = std::min(system_delay, time_ticks_delay);
  refresh_callback_.Reset(
      base::Bind(&CloudPolicyRefreshScheduler::PerformRefresh,
                 base::Unretained(this)));
  task_runner_->PostDelayedTask(FROM_HERE, refresh_callback_.callback(), delay);
}

void CloudPolicyRefreshScheduler::CancelRefresh() {
  refresh_callback_.Cancel();
  is_scheduled_for_soon_ = false;
}

void CloudPolicyRefreshScheduler::UpdateLastRefresh() {
  last_refresh_ = base::Time::NowFromSystemTime();
  last_refresh_ticks_ = base::TimeTicks::Now();
}

}  // namespace policy
