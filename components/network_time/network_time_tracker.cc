// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/network_time/network_time_tracker.h"

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/i18n/time_formatting.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/metrics/field_trial_params.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/client_update_protocol/ecdsa.h"
#include "components/network_time/network_time_pref_names.h"
#include "components/network_time/time_tracker/time_tracker.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

// Time updates happen in two ways. First, other components may call
// UpdateNetworkTime() if they happen to obtain the time securely. This will
// likely be deprecated in favor of the second way, which is scheduled time
// queries issued by NetworkTimeTracker itself.
//
// On startup, the clock state may be read from a pref. (This, too, may be
// deprecated.) After that, the time is checked every |kCheckTimeInterval|. A
// "check" means the possibility, but not the certainty, of a time query. A time
// query may be issued at random, or if the network time is believed to have
// become inaccurate.
//
// After issuing a query, the next check will not happen until
// |kBackoffInterval|. This delay is doubled in the event of an error.

namespace network_time {

// Network time queries are enabled on all desktop platforms except ChromeOS,
// which uses tlsdated to set the system time.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_IOS)
BASE_FEATURE(kNetworkTimeServiceQuerying,
             "NetworkTimeServiceQuerying",
             base::FEATURE_DISABLED_BY_DEFAULT);
#else
BASE_FEATURE(kNetworkTimeServiceQuerying,
             "NetworkTimeServiceQuerying",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

namespace {

// Duration between time checks. The value should be greater than zero. Note
// that a "check" is not necessarily a network time query!
constexpr base::FeatureParam<base::TimeDelta> kCheckTimeInterval{
    &kNetworkTimeServiceQuerying, "CheckTimeInterval", base::Seconds(360)};

// Minimum number of minutes between time queries.
constexpr base::FeatureParam<base::TimeDelta> kBackoffInterval{
    &kNetworkTimeServiceQuerying, "BackoffInterval", base::Hours(1)};

// Probability that a check will randomly result in a query. Checks are made
// every |kCheckTimeInterval|. The default values are chosen with the goal of a
// high probability that a query will be issued every 24 hours. The value should
// fall between 0.0 and 1.0 (inclusive).
constexpr base::FeatureParam<double> kRandomQueryProbability{
    &kNetworkTimeServiceQuerying, "RandomQueryProbability", .012};

// The |kFetchBehavior| parameter can have three values:
//
// - "background-only": Time queries will be issued in the background as
//   needed (when the clock loses sync), but on-demand time queries will
//   not be issued (i.e. StartTimeFetch() will not start time queries.)
//
// - "on-demand-only": Time queries will not be issued except when
//   StartTimeFetch() is called. This is the default value.
//
// - "background-and-on-demand": Time queries will be issued both in the
//   background as needed and also on-demand.
constexpr base::FeatureParam<NetworkTimeTracker::FetchBehavior>::Option
    kFetchBehaviorOptions[] = {
        {NetworkTimeTracker::FETCHES_IN_BACKGROUND_ONLY, "background-only"},
        {NetworkTimeTracker::FETCHES_ON_DEMAND_ONLY, "on-demand-only"},
        {NetworkTimeTracker::FETCHES_IN_BACKGROUND_AND_ON_DEMAND,
         "background-and-on-demand"},
};
constexpr base::FeatureParam<NetworkTimeTracker::FetchBehavior> kFetchBehavior{
    &kNetworkTimeServiceQuerying, "FetchBehavior",
    NetworkTimeTracker::FETCHES_ON_DEMAND_ONLY, &kFetchBehaviorOptions};

// Number of time measurements performed in a given network time calculation.
const uint32_t kNumTimeMeasurements = 7;

// Maximum time lapse before deserialized data are considered stale.
const uint32_t kSerializedDataMaxAgeDays = 7;

// Name of a pref that stores the wall clock time, via
// |InMillisecondsFSinceUnixEpoch|.
const char kPrefTime[] = "local";

// Name of a pref that stores the tick clock time, via |ToInternalValue|.
const char kPrefTicks[] = "ticks";

// Name of a pref that stores the time uncertainty, via |ToInternalValue|.
const char kPrefUncertainty[] = "uncertainty";

// Name of a pref that stores the network time via
// |InMillisecondsFSinceUnixEpoch|.
const char kPrefNetworkTime[] = "network";

// Time server's maximum allowable clock skew, in seconds.  (This is a property
// of the time server that we happen to know.  It's unlikely that it would ever
// be that badly wrong, but all the same it's included here to document the very
// rough nature of the time service provided by this class.)
const uint32_t kTimeServerMaxSkewSeconds = 10;

const char kTimeServiceURL[] = "http://clients2.google.com/time/1/current";

// This is an ECDSA prime256v1 named-curve key.
const int kKeyVersion = 8;
const uint8_t kKeyPubBytes[] = {
    0x30, 0x59, 0x30, 0x13, 0x06, 0x07, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x02,
    0x01, 0x06, 0x08, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03, 0x01, 0x07, 0x03,
    0x42, 0x00, 0x04, 0x62, 0x54, 0x7B, 0x74, 0x30, 0xD7, 0x1A, 0x9C, 0x73,
    0x88, 0xC8, 0xEE, 0x9B, 0x27, 0x57, 0xCA, 0x2C, 0xCA, 0x93, 0xBF, 0xEA,
    0x1B, 0xD1, 0x07, 0x58, 0xBB, 0xFF, 0x83, 0x70, 0x30, 0xD0, 0x3C, 0xC7,
    0x7B, 0x40, 0x60, 0x8D, 0x3E, 0x11, 0x4E, 0x0C, 0x97, 0x16, 0xBF, 0xA7,
    0x31, 0xAC, 0x29, 0xBC, 0x27, 0x13, 0x69, 0xB8, 0x4D, 0x2B, 0x67, 0x1C,
    0x90, 0x4C, 0x44, 0x50, 0x6E, 0xD1, 0xE1};

std::string GetServerProof(
    scoped_refptr<net::HttpResponseHeaders> response_headers) {
  std::string proof;
  return response_headers->EnumerateHeader(nullptr, "x-cup-server-proof",
                                           &proof)
             ? proof
             : std::string();
}

}  // namespace

// static
void NetworkTimeTracker::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kNetworkTimeMapping);
  registry->RegisterBooleanPref(prefs::kNetworkTimeQueriesEnabled, true);
}

NetworkTimeTracker::NetworkTimeTracker(
    std::unique_ptr<base::Clock> clock,
    std::unique_ptr<const base::TickClock> tick_clock,
    PrefService* pref_service,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    std::optional<FetchBehavior> fetch_behavior)
    : server_url_(kTimeServiceURL),
      max_response_size_(1024),
      backoff_(kBackoffInterval.Get()),
      url_loader_factory_(std::move(url_loader_factory)),
      clock_(std::move(clock)),
      tick_clock_(std::move(tick_clock)),
      pref_service_(pref_service),
      time_query_completed_(false),
      fetch_behavior_(fetch_behavior) {
  const base::Value::Dict& time_mapping =
      pref_service_->GetDict(prefs::kNetworkTimeMapping);
  std::optional<double> time_js = time_mapping.FindDouble(kPrefTime);
  std::optional<double> ticks_js = time_mapping.FindDouble(kPrefTicks);
  std::optional<double> uncertainty_js =
      time_mapping.FindDouble(kPrefUncertainty);
  std::optional<double> network_time_js =
      time_mapping.FindDouble(kPrefNetworkTime);
  if (time_js && ticks_js && uncertainty_js && network_time_js) {
    base::Time time_at_last_measurement =
        base::Time::FromMillisecondsSinceUnixEpoch(*time_js);
    base::TimeTicks ticks_at_last_measurement =
        base::TimeTicks::FromInternalValue(static_cast<int64_t>(*ticks_js));
    base::TimeDelta network_time_uncertainty =
        base::TimeDelta::FromInternalValue(
            static_cast<int64_t>(*uncertainty_js));
    base::Time network_time_at_last_measurement =
        base::Time::FromMillisecondsSinceUnixEpoch(*network_time_js);
    base::Time now = clock_->Now();
    if (ticks_at_last_measurement > tick_clock_->NowTicks() ||
        time_at_last_measurement > now ||
        now - time_at_last_measurement >
            base::Days(kSerializedDataMaxAgeDays)) {
      // Drop saved mapping if either clock has run backward, or the data are
      // too old.
      pref_service_->ClearPref(prefs::kNetworkTimeMapping);
    } else {
      tracker_.emplace(time_at_last_measurement, ticks_at_last_measurement,
                       network_time_at_last_measurement,
                       network_time_uncertainty);
    }
  }

  std::string_view public_key = {reinterpret_cast<const char*>(kKeyPubBytes),
                                 sizeof(kKeyPubBytes)};
  query_signer_ =
      client_update_protocol::Ecdsa::Create(kKeyVersion, public_key);

  QueueCheckTime(base::Seconds(0));
}

NetworkTimeTracker::~NetworkTimeTracker() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void NetworkTimeTracker::UpdateNetworkTime(base::Time network_time,
                                           base::TimeDelta resolution,
                                           base::TimeDelta latency,
                                           base::TimeTicks post_time) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << "Network time updating to "
           << base::UTF16ToUTF8(
                  base::TimeFormatFriendlyDateAndTime(network_time));
  // Update network time on every request to limit dependency on ticks lag.
  // TODO(mad): Find a heuristic to avoid augmenting the
  // network_time_uncertainty_ too much by a particularly long latency.
  // Maybe only update when the the new time either improves in accuracy or
  // drifts too far from |network_time_at_last_measurement_|.
  base::Time network_time_at_last_measurement = network_time;

  // Calculate the delay since the network time was received.
  base::TimeTicks now_ticks = tick_clock_->NowTicks();
  base::TimeDelta task_delay = now_ticks - post_time;
  DCHECK_GE(task_delay.InMilliseconds(), 0);
  DCHECK_GE(latency.InMilliseconds(), 0);
  // Estimate that the time was set midway through the latency time.
  base::TimeDelta offset = task_delay + latency / 2;
  base::TimeTicks ticks_at_last_measurement = now_ticks - offset;
  base::Time time_at_last_measurement = clock_->Now() - offset;

  // Can't assume a better time than the resolution of the given time and the
  // ticks measurements involved, each with their own uncertainty.  1 & 2 are
  // the ones used to compute the latency, 3 is the Now() from when this task
  // was posted, 4 and 5 are the Now() and NowTicks() above, and 6 and 7 will be
  // the Now() and NowTicks() in GetNetworkTime().
  base::TimeDelta network_time_uncertainty =
      resolution + latency +
      kNumTimeMeasurements * base::Milliseconds(kTicksResolutionMs);

  tracker_.emplace(time_at_last_measurement, ticks_at_last_measurement,
                   network_time_at_last_measurement, network_time_uncertainty);

  base::Value::Dict time_mapping;
  time_mapping.Set(kPrefTime,
                   time_at_last_measurement.InMillisecondsFSinceUnixEpoch());
  time_mapping.Set(
      kPrefTicks,
      static_cast<double>(ticks_at_last_measurement.ToInternalValue()));
  time_mapping.Set(
      kPrefUncertainty,
      static_cast<double>(network_time_uncertainty.ToInternalValue()));
  time_mapping.Set(
      kPrefNetworkTime,
      network_time_at_last_measurement.InMillisecondsFSinceUnixEpoch());
  pref_service_->Set(prefs::kNetworkTimeMapping,
                     base::Value(std::move(time_mapping)));

  NotifyObservers();
}

bool NetworkTimeTracker::AreTimeFetchesEnabled() const {
  return base::FeatureList::IsEnabled(kNetworkTimeServiceQuerying);
}

NetworkTimeTracker::FetchBehavior NetworkTimeTracker::GetFetchBehavior() const {
  return fetch_behavior_.value_or(kFetchBehavior.Get());
}

void NetworkTimeTracker::SetTimeServerURLForTesting(const GURL& url) {
  server_url_ = url;
}

GURL NetworkTimeTracker::GetTimeServerURLForTesting() const {
  return server_url_;
}

void NetworkTimeTracker::SetMaxResponseSizeForTesting(size_t limit) {
  max_response_size_ = limit;
}

void NetworkTimeTracker::SetPublicKeyForTesting(std::string_view key) {
  query_signer_ = client_update_protocol::Ecdsa::Create(kKeyVersion, key);
}

bool NetworkTimeTracker::QueryTimeServiceForTesting() {
  CheckTime();
  return time_fetcher_ != nullptr;
}

void NetworkTimeTracker::WaitForFetch() {
  base::RunLoop run_loop;
  fetch_completion_callbacks_.push_back(run_loop.QuitClosure());
  run_loop.Run();
}

void NetworkTimeTracker::AddObserver(NetworkTimeObserver* obs) {
  observers_.AddObserver(obs);
}

void NetworkTimeTracker::RemoveObserver(NetworkTimeObserver* obs) {
  observers_.RemoveObserver(obs);
}

bool NetworkTimeTracker::GetTrackerState(
    TimeTracker::TimeTrackerState* state) const {
  base::Time unused;
  auto res = GetNetworkTime(&unused, nullptr);
  if (res != NETWORK_TIME_AVAILABLE) {
    return false;
  }
  *state = tracker_->GetStateAtCreation();
  return true;
}

void NetworkTimeTracker::WaitForFetchForTesting(uint32_t nonce) {
  query_signer_->OverrideNonceForTesting(kKeyVersion, nonce);  // IN-TEST
  WaitForFetch();
}

void NetworkTimeTracker::OverrideNonceForTesting(uint32_t nonce) {
  query_signer_->OverrideNonceForTesting(kKeyVersion, nonce);
}

base::TimeDelta NetworkTimeTracker::GetTimerDelayForTesting() const {
  DCHECK(timer_.IsRunning());
  return timer_.GetCurrentDelay();
}

void NetworkTimeTracker::ClearNetworkTimeForTesting() {
  tracker_ = std::nullopt;
}

NetworkTimeTracker::NetworkTimeResult NetworkTimeTracker::GetNetworkTime(
    base::Time* network_time,
    base::TimeDelta* uncertainty) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(network_time);
  if (!tracker_.has_value()) {
    if (time_query_completed_) {
      // Time query attempts have been made in the past and failed.
      if (time_fetcher_) {
        // A fetch (not the first attempt) is in progress.
        return NETWORK_TIME_SUBSEQUENT_SYNC_PENDING;
      }
      return NETWORK_TIME_NO_SUCCESSFUL_SYNC;
    }
    // No time queries have happened yet.
    if (time_fetcher_) {
      return NETWORK_TIME_FIRST_SYNC_PENDING;
    }
    return NETWORK_TIME_NO_SYNC_ATTEMPT;
  }

  if (!tracker_->GetTime(clock_->Now(), tick_clock_->NowTicks(), network_time,
                         uncertainty)) {
    return NETWORK_TIME_SYNC_LOST;
  }
  return NETWORK_TIME_AVAILABLE;
}

bool NetworkTimeTracker::StartTimeFetch(base::OnceClosure closure) {
  DCHECK(thread_checker_.CalledOnValidThread());
  FetchBehavior behavior = GetFetchBehavior();
  if (behavior != FETCHES_ON_DEMAND_ONLY &&
      behavior != FETCHES_IN_BACKGROUND_AND_ON_DEMAND) {
    return false;
  }

  // Enqueue the callback before calling CheckTime(), so that if
  // CheckTime() completes synchronously, the callback gets called.
  fetch_completion_callbacks_.push_back(std::move(closure));

  // If a time query is already in progress, do not start another one.
  if (time_fetcher_) {
    return true;
  }

  // Cancel any fetches that are scheduled for the future, and try to
  // start one now.
  timer_.Stop();
  CheckTime();

  // CheckTime() does not necessarily start a fetch; for example, time
  // queries might be disabled or network time might already be
  // available.
  if (!time_fetcher_) {
    // If no query is in progress, no callbacks need to be called.
    fetch_completion_callbacks_.clear();
    return false;
  }
  return true;
}

void NetworkTimeTracker::CheckTime() {
  DCHECK(thread_checker_.CalledOnValidThread());

  base::TimeDelta interval = kCheckTimeInterval.Get();
  if (interval.is_negative()) {
    interval = kCheckTimeInterval.default_value;
  }

  // If NetworkTimeTracker is waking up after a backoff, this will reset the
  // timer to its default faster frequency.
  QueueCheckTime(interval);

  if (!ShouldIssueTimeQuery()) {
    return;
  }

  std::string query_string;
  query_signer_->SignRequest("", &query_string);
  GURL::Replacements replacements;
  replacements.SetQueryStr(query_string);
  GURL url = server_url_.ReplaceComponents(replacements);

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("network_time_component", R"(
        semantics {
          sender: "Network Time Component"
          description:
            "Sends a request to a Google server to retrieve the current "
            "timestamp."
          trigger:
            "A request can be sent to retrieve the current time when the user "
            "encounters an SSL date error, or in the background if Chromium "
            "determines that it doesn't have an accurate timestamp."
          data: "None"
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting: "This feature cannot be disabled by settings."
          chrome_policy {
            BrowserNetworkTimeQueriesEnabled {
                BrowserNetworkTimeQueriesEnabled: false
            }
          }
        })");
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = std::move(url);
  // Not expecting any cookies, but just in case.
  resource_request->load_flags =
      net::LOAD_BYPASS_CACHE | net::LOAD_DISABLE_CACHE;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->enable_load_timing = true;
  // This cancels any outstanding fetch.
  time_fetcher_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                   traffic_annotation);
  time_fetcher_->SetAllowHttpErrorResults(true);
  time_fetcher_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&NetworkTimeTracker::OnURLLoaderComplete,
                     base::Unretained(this)),
      max_response_size_);

  timer_.Stop();  // Restarted in OnURLLoaderComplete().
}

bool NetworkTimeTracker::UpdateTimeFromResponse(
    std::unique_ptr<std::string> response_body) {
  int response_code = 0;
  if (time_fetcher_->ResponseInfo() && time_fetcher_->ResponseInfo()->headers) {
    response_code = time_fetcher_->ResponseInfo()->headers->response_code();
  }
  if (response_code != 200 || !response_body) {
    time_query_completed_ = true;
    DVLOG(1) << "fetch failed code=" << response_code;
    return false;
  }

  std::string_view response(*response_body);

  DCHECK(query_signer_);
  if (!query_signer_->ValidateResponse(
          response, GetServerProof(time_fetcher_->ResponseInfo()->headers))) {
    DVLOG(1) << "invalid signature";
    return false;
  }
  response.remove_prefix(5);  // Skips leading )]}'\n
  std::optional<base::Value> value = base::JSONReader::Read(response);
  if (!value) {
    DVLOG(1) << "bad JSON";
    return false;
  }
  if (!value->is_dict()) {
    DVLOG(1) << "not a dictionary";
    return false;
  }
  std::optional<double> current_time_millis =
      value->GetDict().FindDouble("current_time_millis");
  if (!current_time_millis) {
    DVLOG(1) << "no current_time_millis";
    return false;
  }

  // There is a "server_nonce" key here too, but it serves no purpose other than
  // to make the server's response unpredictable.
  base::Time current_time =
      base::Time::FromMillisecondsSinceUnixEpoch(*current_time_millis);
  base::TimeDelta resolution =
      base::Milliseconds(1) + base::Seconds(kTimeServerMaxSkewSeconds);

  // Record histograms for the latency of the time query and the time delta
  // between time fetches.
  base::TimeDelta latency =
      time_fetcher_->ResponseInfo()->load_timing.receive_headers_start -
      time_fetcher_->ResponseInfo()->load_timing.send_end;

  last_fetched_time_ = current_time;

  UpdateNetworkTime(current_time, resolution, latency, tick_clock_->NowTicks());
  return true;
}

void NetworkTimeTracker::OnURLLoaderComplete(
    std::unique_ptr<std::string> response_body) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(time_fetcher_);

  time_query_completed_ = true;

  // After completion of a query, whether succeeded or failed, go to sleep
  // for a long time.
  if (!UpdateTimeFromResponse(
          std::move(response_body))) {  // On error, back off.
    if (backoff_ < base::Days(2)) {
      backoff_ *= 2;
    }
  } else {
    backoff_ = kBackoffInterval.Get();
  }
  QueueCheckTime(backoff_);
  time_fetcher_.reset();

  // Clear |fetch_completion_callbacks_| before running any of them,
  // because a callback could call StartTimeFetch() to enqueue another
  // callback.
  std::vector<base::OnceClosure> callbacks =
      std::move(fetch_completion_callbacks_);
  fetch_completion_callbacks_.clear();
  for (auto& callback : callbacks) {
    std::move(callback).Run();
  }
}

void NetworkTimeTracker::QueueCheckTime(base::TimeDelta delay) {
  DCHECK_GE(delay, base::TimeDelta()) << "delay must be non-negative";
  // Check if the user is opted in to background time fetches.
  FetchBehavior behavior = GetFetchBehavior();
  if (behavior == FETCHES_IN_BACKGROUND_ONLY ||
      behavior == FETCHES_IN_BACKGROUND_AND_ON_DEMAND) {
    timer_.Start(FROM_HERE, delay,
                 base::BindRepeating(&NetworkTimeTracker::CheckTime,
                                     base::Unretained(this)));
  }
}

bool NetworkTimeTracker::ShouldIssueTimeQuery() {
  // Do not query the time service if the feature is not enabled.
  if (!AreTimeFetchesEnabled()) {
    return false;
  }

  // Do not query the time service if queries are disabled by policy.
  if (!pref_service_->GetBoolean(prefs::kNetworkTimeQueriesEnabled)) {
    return false;
  }

  // If GetNetworkTime() does not return NETWORK_TIME_AVAILABLE,
  // synchronization has been lost and a query is needed.
  base::Time network_time;
  if (GetNetworkTime(&network_time, nullptr) != NETWORK_TIME_AVAILABLE) {
    return true;
  }

  // Otherwise, make the decision at random.
  double probability = kRandomQueryProbability.Get();
  if (probability < 0.0 || probability > 1.0) {
    probability = kRandomQueryProbability.default_value;
  }

  return base::RandDouble() < probability;
}

void NetworkTimeTracker::NotifyObservers() {
  // Don't notify if the current state is not NETWORK_TIME_AVAILABLE.
  base::Time unused;
  auto res = GetNetworkTime(&unused, nullptr);
  if (res != NETWORK_TIME_AVAILABLE) {
    return;
  }
  TimeTracker::TimeTrackerState state = tracker_->GetStateAtCreation();
  for (NetworkTimeObserver& obs : observers_) {
    obs.OnNetworkTimeChanged(state);
  }
}

}  // namespace network_time
