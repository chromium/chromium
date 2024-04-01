// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/nearby/common/scheduling/nearby_scheduler_base.h"

#include <algorithm>
#include <sstream>
#include <utility>

#include "base/i18n/time_formatting.h"
#include "base/json/values_util.h"
#include "base/numerics/clamped_math.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/clock.h"
#include "components/cross_device/logging/logging.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/network_service_instance.h"

namespace {

constexpr base::TimeDelta kZeroTimeDelta = base::Seconds(0);
constexpr base::TimeDelta kBaseRetryDelay = base::Seconds(5);
constexpr base::TimeDelta kMaxRetryDelay = base::Hours(1);

const char kLastAttemptTimeKeyName[] = "a";
const char kLastSuccessTimeKeyName[] = "s";
const char kNumConsecutiveFailuresKeyName[] = "f";
const char kHasPendingImmediateRequestKeyName[] = "p";
const char kIsWaitingForResultKeyName[] = "w";

}  // namespace

namespace ash::nearby {

NearbySchedulerBase::NearbySchedulerBase(bool retry_failures,
                                         bool require_connectivity,
                                         const std::string& pref_name,
                                         PrefService* pref_service,
                                         OnRequestCallback callback,
                                         Feature logging_feature,
                                         const base::Clock* clock)
    : NearbyScheduler(std::move(callback)),
      retry_failures_(retry_failures),
      require_connectivity_(require_connectivity),
      pref_name_(pref_name),
      pref_service_(pref_service),
      logging_feature_(logging_feature),
      clock_(clock) {
  DCHECK(pref_service_);

  InitializePersistedRequest();

  if (require_connectivity_) {
    content::GetNetworkConnectionTracker()->AddNetworkConnectionObserver(this);
  }
}

NearbySchedulerBase::~NearbySchedulerBase() {
  if (require_connectivity_) {
    content::GetNetworkConnectionTracker()->RemoveNetworkConnectionObserver(
        this);
  }
}

void NearbySchedulerBase::MakeImmediateRequest() {
  timer_.Stop();
  SetHasPendingImmediateRequest(true);
  Reschedule();
}

void NearbySchedulerBase::HandleResult(bool success) {
  base::Time now = clock_->Now();
  SetLastAttemptTime(now);

  CD_LOG(VERBOSE, logging_feature_)
      << "NearbyScheduler \"" << pref_name_ << "\" latest attempt "
      << (success ? "succeeded" : "failed");

  if (success) {
    SetLastSuccessTime(now);
    SetNumConsecutiveFailures(0);
  } else {
    SetNumConsecutiveFailures(base::ClampAdd(GetNumConsecutiveFailures(), 1));
  }

  SetIsWaitingForResult(false);
  Reschedule();
  PrintSchedulerState();
}

void NearbySchedulerBase::Reschedule() {
  if (!is_running()) {
    return;
  }

  timer_.Stop();

  std::optional<base::TimeDelta> delay = GetTimeUntilNextRequest();
  if (!delay) {
    return;
  }

  timer_.Start(FROM_HERE, *delay,
               base::BindOnce(&NearbySchedulerBase::OnTimerFired,
                              base::Unretained(this)));
}

std::optional<base::Time> NearbySchedulerBase::GetLastSuccessTime() const {
  return base::ValueToTime(
      pref_service_->GetDict(pref_name_).Find(kLastSuccessTimeKeyName));
}

std::optional<base::TimeDelta> NearbySchedulerBase::GetTimeUntilNextRequest()
    const {
  if (!is_running() || IsWaitingForResult()) {
    return std::nullopt;
  }

  if (HasPendingImmediateRequest()) {
    return kZeroTimeDelta;
  }

  base::Time now = clock_->Now();

  // Recover from failures using exponential backoff strategy if necessary.
  std::optional<base::TimeDelta> time_until_retry = TimeUntilRetry(now);
  if (time_until_retry) {
    return time_until_retry;
  }

  // Schedule the periodic request if applicable.
  return TimeUntilRecurringRequest(now);
}

bool NearbySchedulerBase::IsWaitingForResult() const {
  return pref_service_->GetDict(pref_name_)
      .FindBool(kIsWaitingForResultKeyName)
      .value_or(false);
}

size_t NearbySchedulerBase::GetNumConsecutiveFailures() const {
  const std::string* str = pref_service_->GetDict(pref_name_)
                               .FindString(kNumConsecutiveFailuresKeyName);
  if (!str) {
    return 0;
  }

  size_t num_failures = 0;
  if (!base::StringToSizeT(*str, &num_failures)) {
    return 0;
  }

  return num_failures;
}

void NearbySchedulerBase::OnStart() {
  Reschedule();
  CD_LOG(VERBOSE, logging_feature_)
      << "Starting NearbyScheduler \"" << pref_name_ << "\"";
  PrintSchedulerState();
}

void NearbySchedulerBase::OnStop() {
  timer_.Stop();
}

void NearbySchedulerBase::OnConnectionChanged(
    network::mojom::ConnectionType type) {
  if (content::GetNetworkConnectionTracker()->IsOffline()) {
    return;
  }

  Reschedule();
}

std::optional<base::Time> NearbySchedulerBase::GetLastAttemptTime() const {
  return base::ValueToTime(
      pref_service_->GetDict(pref_name_).Find(kLastAttemptTimeKeyName));
}

bool NearbySchedulerBase::HasPendingImmediateRequest() const {
  return pref_service_->GetDict(pref_name_)
      .FindBool(kHasPendingImmediateRequestKeyName)
      .value_or(false);
}

void NearbySchedulerBase::SetLastAttemptTime(base::Time last_attempt_time) {
  ScopedDictPrefUpdate(pref_service_, pref_name_)
      ->Set(kLastAttemptTimeKeyName, base::TimeToValue(last_attempt_time));
}

void NearbySchedulerBase::SetLastSuccessTime(base::Time last_success_time) {
  ScopedDictPrefUpdate(pref_service_, pref_name_)
      ->Set(kLastSuccessTimeKeyName, base::TimeToValue(last_success_time));
}

void NearbySchedulerBase::SetNumConsecutiveFailures(size_t num_failures) {
  ScopedDictPrefUpdate(pref_service_, pref_name_)
      ->Set(kNumConsecutiveFailuresKeyName, base::NumberToString(num_failures));
}

void NearbySchedulerBase::SetHasPendingImmediateRequest(
    bool has_pending_immediate_request) {
  ScopedDictPrefUpdate(pref_service_, pref_name_)
      ->Set(kHasPendingImmediateRequestKeyName, has_pending_immediate_request);
}

void NearbySchedulerBase::SetIsWaitingForResult(bool is_waiting_for_result) {
  ScopedDictPrefUpdate(pref_service_, pref_name_)
      ->Set(kIsWaitingForResultKeyName, is_waiting_for_result);
}

void NearbySchedulerBase::InitializePersistedRequest() {
  if (IsWaitingForResult()) {
    SetHasPendingImmediateRequest(true);
    SetIsWaitingForResult(false);
  }
}

std::optional<base::TimeDelta> NearbySchedulerBase::TimeUntilRetry(
    base::Time now) const {
  if (!retry_failures_) {
    return std::nullopt;
  }

  size_t num_failures = GetNumConsecutiveFailures();
  if (num_failures == 0) {
    return std::nullopt;
  }

  // The exponential back off is
  //
  //   base * 2^(num_failures - 1)
  //
  // up to a fixed maximum retry delay.
  base::TimeDelta delay =
      std::min(kMaxRetryDelay, kBaseRetryDelay * (1 << (num_failures - 1)));

  base::TimeDelta time_elapsed_since_last_attempt = now - *GetLastAttemptTime();

  return std::max(kZeroTimeDelta, delay - time_elapsed_since_last_attempt);
}

void NearbySchedulerBase::OnTimerFired() {
  DCHECK(is_running());
  if (require_connectivity_ &&
      content::GetNetworkConnectionTracker()->IsOffline()) {
    return;
  }

  SetIsWaitingForResult(true);
  SetHasPendingImmediateRequest(false);
  NotifyOfRequest();
}

void NearbySchedulerBase::PrintSchedulerState() const {
  std::optional<base::Time> last_attempt_time = GetLastAttemptTime();
  std::optional<base::Time> last_success_time = GetLastSuccessTime();
  std::optional<base::TimeDelta> time_until_next_request =
      GetTimeUntilNextRequest();

  std::stringstream ss;
  ss << "State of NearbyScheduler scheduler \"" << pref_name_
     << "\":" << "\n  Last attempt time: ";
  if (last_attempt_time) {
    ss << base::TimeFormatShortDateAndTimeWithTimeZone(*last_attempt_time);
  } else {
    ss << "Never";
  }

  ss << "\n  Last success time: ";
  if (last_success_time) {
    ss << base::TimeFormatShortDateAndTimeWithTimeZone(*last_success_time);
  } else {
    ss << "Never";
  }

  ss << "\n  Time until next request: ";
  if (time_until_next_request) {
    std::u16string next_request_delay;
    bool success = base::TimeDurationFormatWithSeconds(
        *time_until_next_request,
        base::DurationFormatWidth::DURATION_WIDTH_NARROW, &next_request_delay);
    if (success) {
      ss << next_request_delay;
    }
  } else {
    ss << "Never";
  }

  ss << "\n  Is waiting for result? " << (IsWaitingForResult() ? "Yes" : "No");
  ss << "\n  Pending immediate request? "
     << (HasPendingImmediateRequest() ? "Yes" : "No");
  ss << "\n  Num consecutive failures: " << GetNumConsecutiveFailures();

  CD_LOG(VERBOSE, logging_feature_) << ss.str();
}

}  // namespace ash::nearby
