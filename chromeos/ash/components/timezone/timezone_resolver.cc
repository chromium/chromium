// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/timezone/timezone_resolver.h"

#include <math.h>
#include <stdint.h>

#include <algorithm>
#include <memory>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/power_monitor/power_monitor.h"
#include "base/power_monitor/power_observer.h"
#include "base/rand_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/geolocation/geoposition.h"
#include "chromeos/ash/components/geolocation/simple_geolocation_provider.h"
#include "chromeos/ash/components/timezone/timezone_provider.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"

namespace ash {

namespace {

class TZRequest;

// Total timezone resolving process timeout.
const unsigned int kRefreshTimeZoneTimeoutSeconds = 60;

// Initial delay (for the first request).
const double kInitialRefreshIntervalSec = 3.0;

// Timezone refresh happens at least once each this interval.
const double kMaximumRefreshIntervalSec = 6.0 * 3600;  // 6 hours

// Delay between refresh attempts depends on current number of requests and
// this constant.
// [interval = kInitialRefreshIntervalMS * (2 ^
//                 (kRefreshIntervalRequestsCountMultiplier * requests_count))]
// in seconds.
// request_number interval (seconds)
// 1 3    (initial, requests_count = 0)
// 2 24   (requests_count = 1)
// 3 1536 (requests_count = 2)
// 4 12288 (requests_count = 3)
// 5+ 21600 (maximum)
const unsigned int kRefreshIntervalRequestsCountMultiplier = 3;

// We should limit request rate on browser start to prevent server overload
// on permanent browser crash.
// If too little time has passed since previous request, initialize
// |requests_count_| with |kRefreshTimeZoneInitialRequestCountOnRateLimit|.
const double kRefreshTimeZoneMinimumDelayOnRestartSec =
    10 * 60.0;  // 10 minutes
const unsigned int kRefreshTimeZoneInitialRequestCountOnRateLimit = 2;

int MaxRequestsCountForInterval(const double interval_seconds) {
  return log2(interval_seconds / kInitialRefreshIntervalSec) /
         kRefreshIntervalRequestsCountMultiplier;
}

int IntervalForNextRequest(const int requests) {
  const base::TimeDelta initial_interval =
      base::Seconds(kInitialRefreshIntervalSec);
  return static_cast<int>(initial_interval.InSecondsF() *
                          (2 << (static_cast<unsigned>(requests) *
                                 kRefreshIntervalRequestsCountMultiplier)));
}

}  // anonymous namespace

const char TimeZoneResolver::kLastTimeZoneRefreshTime[] =
    "timezone_resolver.last_update_time";

// This class periodically refreshes location and timezone.
// It should be destroyed to stop refresh.
class TimeZoneResolver::TimeZoneResolverImpl
    : public base::PowerSuspendObserver {
 public:
  explicit TimeZoneResolverImpl(const TimeZoneResolver* resolver);

  TimeZoneResolverImpl(const TimeZoneResolverImpl&) = delete;
  TimeZoneResolverImpl& operator=(const TimeZoneResolverImpl&) = delete;

  ~TimeZoneResolverImpl() override;

  // This is called once after the object is created.
  void Start();

  // PowerSuspendObserver implementation.
  void OnResume() override;

  // (Re)Starts timer.
  void ScheduleRequest();

  // Creates new TZRequest.
  void CreateNewRequest();

  // Called by TZRequest.
  SimpleGeolocationProvider* geolocation_provider() {
    return resolver_->geolocation_provider_;
  }
  TimeZoneProvider* timezone_provider() { return &timezone_provider_; }

  // Update requests count and last request time.
  void RecordAttempt();

  // This is called by TZRequest. Destroys active request and starts a new one.
  void RequestIsFinished();

  void ApplyTimeZone(const TimeZoneResponseData* timezone);

  TimeZoneResolver::DelayNetworkCallClosure delay_network_call() const {
    return resolver_->delay_network_call();
  }

  base::WeakPtr<TimeZoneResolver::TimeZoneResolverImpl> AsWeakPtr();

  bool ShouldSendWiFiGeolocationData();
  bool ShouldSendCellularGeolocationData();

 private:
  raw_ptr<const TimeZoneResolver> resolver_;

  // Helper to check timezone detection policy against expected value
  bool CheckTimezoneManagementSetting(int expected_policy_value);

  // Returns delay to next timezone update request
  base::TimeDelta CalculateNextInterval();

  TimeZoneProvider timezone_provider_;

  base::OneShotTimer refresh_timer_;

  // Total number of request attempts.
  int requests_count_;

  // This is not NULL when update is in progress.
  std::unique_ptr<TZRequest> request_;

  base::WeakPtrFactory<TimeZoneResolver::TimeZoneResolverImpl>
      weak_ptr_factory_{this};
};

namespace {

// This class implements a single timezone refresh attempt.
class TZRequest {
 public:
  explicit TZRequest(TimeZoneResolver::TimeZoneResolverImpl* resolver)
      : resolver_(resolver) {}

  TZRequest(const TZRequest&) = delete;
  TZRequest& operator=(const TZRequest&) = delete;

  ~TZRequest();

  // Starts request after specified delay.
  void Start();

  // Called from SimpleGeolocationProvider when location is resolved.
  void OnLocationResolved(const Geoposition& position,
                          bool server_error,
                          const base::TimeDelta elapsed);

  // TimeZoneRequest::TimeZoneResponseCallback implementation.
  void OnTimezoneResolved(std::unique_ptr<TimeZoneResponseData> timezone,
                          bool server_error);

  base::WeakPtr<TZRequest> AsWeakPtr();

 private:
  // This is called by network detector when network is available.
  void StartRequestOnNetworkAvailable();

  const raw_ptr<TimeZoneResolver::TimeZoneResolverImpl> resolver_;

  base::WeakPtrFactory<TZRequest> weak_ptr_factory_{this};
};

TZRequest::~TZRequest() = default;

void TZRequest::StartRequestOnNetworkAvailable() {
  resolver_->RecordAttempt();
  resolver_->geolocation_provider()->RequestGeolocation(
      base::Seconds(kRefreshTimeZoneTimeoutSeconds),
      resolver_->ShouldSendWiFiGeolocationData(),
      resolver_->ShouldSendCellularGeolocationData(),
      base::BindOnce(&TZRequest::OnLocationResolved, AsWeakPtr()),
      SimpleGeolocationProvider::ClientId::kTimezoneResolver);
}

void TZRequest::Start() {
  // Call to `ash::DelayNetworkCall`.
  resolver_->delay_network_call().Run(
      base::BindOnce(&TZRequest::StartRequestOnNetworkAvailable, AsWeakPtr()));
}

void TZRequest::OnLocationResolved(const Geoposition& position,
                                   bool server_error,
                                   const base::TimeDelta elapsed) {
  absl::Cleanup on_request_finished = [this] {
    resolver_->RequestIsFinished();
  };

  // Ignore invalid position.
  if (!position.Valid())
    return;

  const base::TimeDelta timeout = base::Seconds(kRefreshTimeZoneTimeoutSeconds);

  if (elapsed >= timeout) {
    VLOG(1) << "Refresh TimeZone: got location after timeout ("
            << elapsed.InSecondsF() << " seconds elapsed). Ignored.";
    return;
  }

  resolver_->timezone_provider()->RequestTimezone(
      position, timeout - elapsed,
      base::BindOnce(&TZRequest::OnTimezoneResolved, AsWeakPtr()));

  // `OnTimezoneResolved` is responsible for calling `RequestIsFinished()` now.
  std::move(on_request_finished).Cancel();
}

void TZRequest::OnTimezoneResolved(
    std::unique_ptr<TimeZoneResponseData> timezone,
    bool server_error) {
  absl::Cleanup on_request_finished = [this] {
    resolver_->RequestIsFinished();
  };

  DCHECK(timezone);
  VLOG(1) << "Refreshed local timezone={" << timezone->ToStringForDebug()
          << "}.";

  if (timezone->status != TimeZoneResponseData::OK) {
    VLOG(1) << "Refresh TimeZone: failed to resolve timezone.";
    return;
  }

  resolver_->ApplyTimeZone(timezone.get());
}

base::WeakPtr<TZRequest> TZRequest::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // anonymous namespace

// ------------------------------------------------------------------------
// TimeZoneResolver::TimeZoneResolverImpl implementation.

TimeZoneResolver::TimeZoneResolverImpl::TimeZoneResolverImpl(
    const TimeZoneResolver* resolver)
    : resolver_(resolver),

      timezone_provider_(resolver->shared_url_loader_factory(),
                         DefaultTimezoneProviderURL()),
      requests_count_(0) {
  DCHECK(!resolver_->apply_timezone().is_null());
  DCHECK(!resolver_->delay_network_call().is_null());

  base::PowerMonitor::GetInstance()->AddPowerSuspendObserver(this);

  const int64_t last_refresh_at_us =
      resolver_->local_state()->GetInt64(kLastTimeZoneRefreshTime);
  const base::Time last_refresh_at = base::Time::FromDeltaSinceWindowsEpoch(
      base::Microseconds(last_refresh_at_us));
  const base::Time next_refresh_not_before =
      last_refresh_at + base::Seconds(kRefreshTimeZoneMinimumDelayOnRestartSec);
  if (next_refresh_not_before > base::Time::Now()) {
    requests_count_ = kRefreshTimeZoneInitialRequestCountOnRateLimit;
    VLOG(1) << "TimeZoneResolverImpl(): initialize requests_count_="
            << requests_count_ << " because of rate limit.";
  }
}

TimeZoneResolver::TimeZoneResolverImpl::~TimeZoneResolverImpl() {
  base::PowerMonitor::GetInstance()->RemovePowerSuspendObserver(this);
}

void TimeZoneResolver::TimeZoneResolverImpl::Start() {
  // Start() is usually called twice:
  // - On device boot.
  // - On user session start.
  if (request_ || refresh_timer_.IsRunning())
    return;

  ScheduleRequest();
}

// Returns delay to next timezone update request
base::TimeDelta
TimeZoneResolver::TimeZoneResolverImpl::CalculateNextInterval() {
  // This is initial request, which should be served immediately.
  if (requests_count_ == 0) {
    return base::Seconds(kInitialRefreshIntervalSec);
  }

  // See comment to kRefreshIntervalRequestsCountMultiplier.
  if (requests_count_ >=
      MaxRequestsCountForInterval(kMaximumRefreshIntervalSec)) {
    return base::Seconds(kMaximumRefreshIntervalSec);
  }

  const int base_interval = IntervalForNextRequest(requests_count_);
  DCHECK_LE(base_interval, kMaximumRefreshIntervalSec);

  // Add jitter to level request rate.
  const base::TimeDelta interval(
      base::Seconds(base::RandDouble() * 2 * base_interval));
  VLOG(1) << "TimeZoneResolverImpl::CalculateNextInterval(): interval="
          << interval.InSecondsF();
  return interval;
}

void TimeZoneResolver::TimeZoneResolverImpl::OnResume() {
  requests_count_ = 0;
  // Refresh timezone immediately.
  request_.reset();
  ScheduleRequest();
}

void TimeZoneResolver::TimeZoneResolverImpl::ScheduleRequest() {
  if (request_)
    return;

  // base::OneShotTimer
  base::TimeDelta interval = CalculateNextInterval();
  refresh_timer_.Stop();
  refresh_timer_.Start(
      FROM_HERE, interval,
      base::BindOnce(&TimeZoneResolver::TimeZoneResolverImpl::CreateNewRequest,
                     AsWeakPtr()));
}

void TimeZoneResolver::TimeZoneResolverImpl::CreateNewRequest() {
  if (request_)
    return;

  refresh_timer_.Stop();

  request_ = std::make_unique<TZRequest>(this);
  request_->Start();
}

void TimeZoneResolver::TimeZoneResolverImpl::RecordAttempt() {
  resolver_->local_state()->SetInt64(
      kLastTimeZoneRefreshTime,
      base::Time::Now().ToDeltaSinceWindowsEpoch().InMicroseconds());
  ++requests_count_;
}

void TimeZoneResolver::TimeZoneResolverImpl::RequestIsFinished() {
  request_.reset();
  ScheduleRequest();
}

void TimeZoneResolver::TimeZoneResolverImpl::ApplyTimeZone(
    const TimeZoneResponseData* timezone) {
  resolver_->apply_timezone().Run(timezone);
}

bool TimeZoneResolver::TimeZoneResolverImpl::ShouldSendWiFiGeolocationData() {
  return resolver_->ShouldSendWiFiGeolocationData();
}

bool TimeZoneResolver::TimeZoneResolverImpl::
    ShouldSendCellularGeolocationData() {
  return resolver_->ShouldSendCellularGeolocationData();
}

base::WeakPtr<TimeZoneResolver::TimeZoneResolverImpl>
TimeZoneResolver::TimeZoneResolverImpl::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

// ------------------------------------------------------------------------
// TimeZoneResolver implementation

TimeZoneResolver::TimeZoneResolver(
    Delegate* delegate,
    SimpleGeolocationProvider* geolocation_provider,
    scoped_refptr<network::SharedURLLoaderFactory> factory,
    const ApplyTimeZoneCallback& apply_timezone,
    const DelayNetworkCallClosure& delay_network_call,
    PrefService* local_state)
    : delegate_(delegate),
      geolocation_provider_(geolocation_provider),
      shared_url_loader_factory_(std::move(factory)),
      apply_timezone_(apply_timezone),
      delay_network_call_(delay_network_call),
      local_state_(local_state) {
  DCHECK(!apply_timezone.is_null());
  DCHECK(delegate_);
}

TimeZoneResolver::~TimeZoneResolver() {
  Stop();
}

void TimeZoneResolver::Start() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!implementation_) {
    implementation_ = std::make_unique<TimeZoneResolverImpl>(this);
    implementation_->Start();
    is_running_ = true;
  }
}

void TimeZoneResolver::Stop() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  implementation_.reset();
  is_running_ = false;
}

bool TimeZoneResolver::IsRunning() {
  return is_running_;
}

// static
int TimeZoneResolver::MaxRequestsCountForIntervalForTesting(
    const double interval_seconds) {
  return MaxRequestsCountForInterval(interval_seconds);
}

// static
int TimeZoneResolver::IntervalForNextRequestForTesting(const int requests) {
  return IntervalForNextRequest(requests);
}

// static
void TimeZoneResolver::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterInt64Pref(kLastTimeZoneRefreshTime, 0);
}

bool TimeZoneResolver::ShouldSendWiFiGeolocationData() const {
  return delegate_->ShouldSendWiFiGeolocationData();
}

bool TimeZoneResolver::ShouldSendCellularGeolocationData() const {
  return delegate_->ShouldSendCellularGeolocationData();
}

scoped_refptr<network::SharedURLLoaderFactory>
TimeZoneResolver::shared_url_loader_factory() const {
  return shared_url_loader_factory_;
}

}  // namespace ash
