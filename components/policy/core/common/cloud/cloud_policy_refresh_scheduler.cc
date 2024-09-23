// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/cloud_policy_refresh_scheduler.h"

#include <algorithm>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/time/default_tick_clock.h"
#include "base/time/tick_clock.h"
#include "build/build_config.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_service.h"
#include "components/policy/core/common/cloud/user_cloud_policy_store.h"

namespace policy {

namespace {

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
BASE_FEATURE(kRetryWithKeyReset,
             "RetryWithKeyReset",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

base::Clock* clock_for_testing_ = nullptr;

const base::Clock* GetClock() {
  if (clock_for_testing_)
    return clock_for_testing_;
  return base::DefaultClock::GetInstance();
}

base::TickClock* tick_clock_for_testing_ = nullptr;

const base::TickClock* GetTickClock() {
  if (tick_clock_for_testing_)
    return tick_clock_for_testing_;
  return base::DefaultTickClock::GetInstance();
}

}  // namespace

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)

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
const int64_t CloudPolicyRefreshScheduler::kRandomSaltDelayMaxValueMs =
    5 * 60 * 1000;  // 5 minutes.

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
const int64_t CloudPolicyRefreshScheduler::kRandomSaltDelayMaxValueMs =
    5 * 60 * 1000;  // 5 minutes.

#endif

CloudPolicyRefreshScheduler::CloudPolicyRefreshScheduler(
    CloudPolicyClient* client,
    CloudPolicyStore* store,
    CloudPolicyService* service,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    network::NetworkConnectionTrackerGetter network_connection_tracker_getter)
    : client_(client),
      store_(store),
      service_(service),
      task_runner_(task_runner),
      network_connection_tracker_(network_connection_tracker_getter.Run()),
      error_retry_delay_ms_(kInitialErrorRetryDelayMs),
      refresh_delay_ms_(kDefaultRefreshDelayMs),
      refresh_delay_salt_ms_(static_cast<int64_t>(
          base::RandGenerator(kRandomSaltDelayMaxValueMs))),
      invalidations_available_(false) {
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
  for (auto& observer : observers_)
    observer.OnRefreshSchedulerDestruction(this);
}

void CloudPolicyRefreshScheduler::SetDesiredRefreshDelay(
    int64_t refresh_delay) {
  refresh_delay_ms_ =
      std::clamp(refresh_delay, kRefreshDelayMinMs, kRefreshDelayMaxMs);
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

void CloudPolicyRefreshScheduler::RefreshSoon(PolicyFetchReason reason) {
  // If the client isn't registered, there is nothing to do.
  if (!client_->is_registered())
    return;

  is_scheduled_for_soon_ = true;
  RefreshAfter(0, reason);
}

void CloudPolicyRefreshScheduler::SetInvalidationServiceAvailability(
    bool is_available) {
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
  if (!client->is_registered()) {
    CancelRefresh();
    return;
  }

  // The client has registered, so trigger an immediate refresh.
  error_retry_delay_ms_ = kInitialErrorRetryDelayMs;
  RefreshSoon(PolicyFetchReason::kRegistrationChanged);
}

void CloudPolicyRefreshScheduler::OnClientError(CloudPolicyClient* client) {
  // Save the status for below.
  DeviceManagementStatus status = client_->last_dm_status();

  // Schedule an error retry if applicable.
  UpdateLastRefresh();
  ScheduleRefresh();

  // Update the retry delay.
  if (client->is_registered() && (status == DM_STATUS_REQUEST_FAILED ||
                                  status == DM_STATUS_TEMPORARY_UNAVAILABLE)) {
    error_retry_delay_ms_ =
        std::min(error_retry_delay_ms_ * 2, refresh_delay_ms_);
  } else {
    error_retry_delay_ms_ = kInitialErrorRetryDelayMs;
  }
}

void CloudPolicyRefreshScheduler::OnStoreLoaded(CloudPolicyStore* store) {
  // Load successfully, reset flag in case we failed again.
  has_retried_with_key_reset_ = false;

  UpdateLastRefreshFromPolicy();

  // Re-schedule the next refresh in case the is_managed bit changed.
  ScheduleRefresh();
}

void CloudPolicyRefreshScheduler::OnStoreError(CloudPolicyStore* store) {
  // If |store_| fails, the is_managed bit that it provides may become stale.
  // The best guess in that situation is to assume is_managed didn't change and
  // continue using the stale information. Thus, no specific response to a store
  // error is required. NB: Changes to is_managed fire OnStoreLoaded().

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  // Client is registered means we have successfully get policy key once. However,
  // a following policy fetch request is failed because we can't verified
  // signature. Delete the policy key so that we can get it again with next
  // policy fetch response.
  if (base::FeatureList::IsEnabled(kRetryWithKeyReset) &&
      client_->is_registered() &&
      store->status() == CloudPolicyStore::STATUS_VALIDATION_ERROR &&
      store->validation_status() ==
          CloudPolicyValidatorBase::VALIDATION_BAD_SIGNATURE &&
      !has_retried_with_key_reset_) {
    has_retried_with_key_reset_ = true;
    static_cast<DesktopCloudPolicyStore*>(store)->ResetPolicyKey();
    client_->clear_public_key_version();
    RefreshSoon(PolicyFetchReason::kRetry);
  }
#endif
}

void CloudPolicyRefreshScheduler::OnConnectionChanged(
    network::mojom::ConnectionType type) {
  if (type == network::mojom::ConnectionType::CONNECTION_NONE)
    return;

  if (client_->last_dm_status() == DM_STATUS_REQUEST_FAILED) {
    RefreshSoon(PolicyFetchReason::kRetryAfterStatusRequestFailed);
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
      base::Milliseconds(GetActualRefreshDelay());
  const base::TimeDelta system_delta = std::max(
      last_refresh_ + refresh_delay - GetClock()->Now(), base::TimeDelta());
  const base::TimeDelta ticks_delta =
      last_refresh_ticks_ + refresh_delay - GetTickClock()->NowTicks();
  if (ticks_delta > system_delta)
    RefreshAfter(system_delta.InMilliseconds(), PolicyFetchReason::kScheduled);
}

void CloudPolicyRefreshScheduler::UpdateLastRefreshFromPolicy() {
  if (!last_refresh_.is_null())
    return;

  // If the client has already fetched policy, assume that happened recently. If
  // that assumption ever breaks, the proper thing to do probably is to move the
  // |last_refresh_| bookkeeping to CloudPolicyClient.
  if (!client_->last_policy_fetch_responses().empty()) {
    UpdateLastRefresh();
    return;
  }

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  // On mobile platforms the client is only registered for enterprise users.
  constexpr bool should_update = true;
#else
  // Only delay refresh for a cached non-managed response.
  const bool should_update = !store_->is_managed();
#endif

  if (store_->has_policy() && store_->policy()->has_timestamp() &&
      should_update) {
    last_refresh_ = base::Time::FromMillisecondsSinceUnixEpoch(
        store_->policy()->timestamp());
    last_refresh_ticks_ =
        GetTickClock()->NowTicks() + (last_refresh_ - GetClock()->Now());
  }
}

void CloudPolicyRefreshScheduler::ScheduleRefresh() {
  // If the client isn't registered, there is nothing to do.
  if (!client_->is_registered()) {
    CancelRefresh();
    return;
  }

  // Ignore the refresh request if there's a request scheduled for soon.
  if (is_scheduled_for_soon_) {
    DCHECK(refresh_weak_factory_.HasWeakPtrs());
    return;
  }

  // If there is a registration, go by the client's status. That will tell us
  // what the appropriate refresh delay should be.
  switch (client_->last_dm_status()) {
    case DM_STATUS_SUCCESS:
      if (store_->is_managed())
        RefreshAfter(GetActualRefreshDelay(), PolicyFetchReason::kScheduled);
      else
        RefreshAfter(kUnmanagedRefreshDelayMs, PolicyFetchReason::kScheduled);
      return;

      // Try again after `GetActualRefreshDelay()`:
    case DM_STATUS_SERVICE_ACTIVATION_PENDING:
      return RefreshAfter(
          GetActualRefreshDelay(),
          PolicyFetchReason::kRetryAfterStatusServiceActivationPending);
    case DM_STATUS_SERVICE_POLICY_NOT_FOUND:
      return RefreshAfter(
          GetActualRefreshDelay(),
          PolicyFetchReason::kRetryAfterStatusServicePolicyNotFound);
    case DM_STATUS_SERVICE_TOO_MANY_REQUESTS:
      return RefreshAfter(
          GetActualRefreshDelay(),
          PolicyFetchReason::kRetryAfterStatusServiceTooManyRequests);

      // Try again after `error_retry_delay_ms_`
    case DM_STATUS_REQUEST_FAILED:
      return RefreshAfter(error_retry_delay_ms_,
                          PolicyFetchReason::kRetryAfterStatusRequestFailed);
    case DM_STATUS_TEMPORARY_UNAVAILABLE:
      return RefreshAfter(
          error_retry_delay_ms_,
          PolicyFetchReason::kRetryAfterStatusTemporaryUnavailable);
    case DM_STATUS_CANNOT_SIGN_REQUEST:
      return RefreshAfter(
          error_retry_delay_ms_,
          PolicyFetchReason::kRetryAfterStatusCannotSignRequest);

      // Try again after `kUnmanagedRefreshDelay.
    case DM_STATUS_REQUEST_INVALID:
      return RefreshAfter(kUnmanagedRefreshDelayMs,
                          PolicyFetchReason::kRetryAfterStatusRequestInvalid);
    case DM_STATUS_HTTP_STATUS_ERROR:
      return RefreshAfter(kUnmanagedRefreshDelayMs,
                          PolicyFetchReason::kRetryAfterStatusHttpStatusError);
    case DM_STATUS_RESPONSE_DECODING_ERROR:
      return RefreshAfter(
          kUnmanagedRefreshDelayMs,
          PolicyFetchReason::kRetryAfterStatusResponseDecodingError);
    case DM_STATUS_SERVICE_MANAGEMENT_NOT_SUPPORTED:
      return RefreshAfter(
          kUnmanagedRefreshDelayMs,
          PolicyFetchReason::kRetryAfterStatusServiceManagementNotSupported);
    case DM_STATUS_REQUEST_TOO_LARGE:
      return RefreshAfter(kUnmanagedRefreshDelayMs,
                          PolicyFetchReason::kRetryAfterStatusRequestTooLarge);

      // No retry
    case DM_STATUS_SERVICE_MANAGEMENT_TOKEN_INVALID:
    case DM_STATUS_SERVICE_DEVICE_NOT_FOUND:
    case DM_STATUS_SERVICE_INVALID_SERIAL_NUMBER:
    case DM_STATUS_SERVICE_DEVICE_ID_CONFLICT:
    case DM_STATUS_SERVICE_MISSING_LICENSES:
    case DM_STATUS_SERVICE_DEPROVISIONED:
    case DM_STATUS_SERVICE_DOMAIN_MISMATCH:
    case DM_STATUS_SERVICE_DEVICE_NEEDS_RESET:
    case DM_STATUS_SERVICE_CONSUMER_ACCOUNT_WITH_PACKAGED_LICENSE:
    case DM_STATUS_SERVICE_ENTERPRISE_ACCOUNT_IS_NOT_ELIGIBLE_TO_ENROLL:
    case DM_STATUS_SERVICE_ENTERPRISE_TOS_HAS_NOT_BEEN_ACCEPTED:
    case DM_STATUS_SERVICE_ILLEGAL_ACCOUNT_FOR_PACKAGED_EDU_LICENSE:
    case DM_STATUS_SERVICE_INVALID_PACKAGED_DEVICE_FOR_KIOSK:
      // Need a re-registration, no use in retrying.
      CancelRefresh();
      return;
    case DM_STATUS_SERVICE_ARC_DISABLED:
      // This doesn't occur during policy refresh, don't change the schedule.
      return;
  }

  NOTREACHED_IN_MIGRATION()
      << "Invalid client status " << client_->last_dm_status();
  RefreshAfter(kUnmanagedRefreshDelayMs, PolicyFetchReason::kUnspecified);
}

void CloudPolicyRefreshScheduler::PerformRefresh(PolicyFetchReason reason) {
  CancelRefresh();

  if (client_->is_registered()) {
    // Update |last_refresh_| so another fetch isn't triggered inadvertently.
    UpdateLastRefresh();

    // The result of this operation will be reported through OnPolicyFetched()
    // and OnPolicyRefreshed() callbacks. Next refresh will be scheduled in
    // OnPolicyFetched().
    service_->RefreshPolicy(
        base::BindOnce(&CloudPolicyRefreshScheduler::OnPolicyRefreshed,
                       weak_factory_.GetWeakPtr()),
        reason);
    return;
  }

  // This should never happen, as the registration change should have been
  // handled via OnRegistrationStateChanged().
  NOTREACHED_IN_MIGRATION();
}

void CloudPolicyRefreshScheduler::RefreshAfter(int delta_ms,
                                               PolicyFetchReason reason) {
  const base::TimeDelta delta(base::Milliseconds(delta_ms));

  // Schedule the callback, calculating the delay based on both, system time
  // and TimeTicks, whatever comes up to become earlier update. This is done to
  // make sure the refresh is not delayed too much when the system time moved
  // backward after the last refresh.
  const base::TimeDelta system_delay =
      std::max((last_refresh_ + delta) - GetClock()->Now(), base::TimeDelta());
  const base::TimeDelta time_ticks_delay =
      std::max((last_refresh_ticks_ + delta) - GetTickClock()->NowTicks(),
               base::TimeDelta());
  base::TimeDelta delay = std::min(system_delay, time_ticks_delay);

  // Unless requesting an immediate refresh, add a delay to the scheduled policy
  // refresh in order to spread out server load.
  if (!delay.is_zero())
    delay += base::Milliseconds(refresh_delay_salt_ms_);

  refresh_weak_factory_.InvalidateWeakPtrs();
  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&CloudPolicyRefreshScheduler::PerformRefresh,
                     refresh_weak_factory_.GetWeakPtr(), reason),
      delay);
}

void CloudPolicyRefreshScheduler::CancelRefresh() {
  refresh_weak_factory_.InvalidateWeakPtrs();
  is_scheduled_for_soon_ = false;
}

void CloudPolicyRefreshScheduler::UpdateLastRefresh() {
  last_refresh_ = GetClock()->Now();
  last_refresh_ticks_ = GetTickClock()->NowTicks();
  for (auto& observer : observers_)
    observer.OnFetchAttempt(this);
}

void CloudPolicyRefreshScheduler::OnPolicyRefreshed(bool success) {
  // Next policy fetch is scheduled in OnPolicyFetched() callback.
  VLOG(1) << "Scheduled policy refresh "
          << (success ? "successful" : "unsuccessful");
}

// static
base::ScopedClosureRunner CloudPolicyRefreshScheduler::OverrideClockForTesting(
    base::Clock* clock_for_testing) {
  CHECK(!clock_for_testing_);
  clock_for_testing_ = clock_for_testing;
  return base::ScopedClosureRunner(
      base::BindOnce([]() { clock_for_testing_ = nullptr; }));
}

// static
base::ScopedClosureRunner
CloudPolicyRefreshScheduler::OverrideTickClockForTesting(
    base::TickClock* tick_clock_for_testing) {
  CHECK(!tick_clock_for_testing_);
  tick_clock_for_testing_ = tick_clock_for_testing;
  return base::ScopedClosureRunner(
      base::BindOnce([]() { tick_clock_for_testing_ = nullptr; }));
}

void CloudPolicyRefreshScheduler::AddObserver(
    CloudPolicyRefreshSchedulerObserver* observer) {
  observers_.AddObserver(observer);
}

void CloudPolicyRefreshScheduler::RemoveObserver(
    CloudPolicyRefreshSchedulerObserver* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace policy
