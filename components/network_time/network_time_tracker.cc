// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/network_time/network_time_tracker.h"

#include <memory>
#include <stdint.h>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/i18n/time_formatting.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros_local.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/client_update_protocol/ecdsa.h"
#include "components/network_time/network_time_pref_names.h"
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
#include "third_party/abseil-cpp/absl/types/optional.h"

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

// Amount of divergence allowed between wall clock and tick clock.
const uint32_t kClockDivergenceSeconds = 60;

// Maximum time lapse before deserialized data are considered stale.
const uint32_t kSerializedDataMaxAgeDays = 7;

// Name of a pref that stores the wall clock time, via |ToJsTime|.
const char kPrefTime[] = "local";

// Name of a pref that stores the tick clock time, via |ToInternalValue|.
const char kPrefTicks[] = "ticks";

// Name of a pref that stores the time uncertainty, via |ToInternalValue|.
const char kPrefUncertainty[] = "uncertainty";

// Name of a pref that stores the network time via |ToJsTime|.
const char kPrefNetworkTime[] = "network";

// Time server's maximum allowable clock skew, in seconds.  (This is a property
// of the time server that we happen to know.  It's unlikely that it would ever
// be that badly wrong, but all the same it's included here to document the very
// rough nature of the time service provided by this class.)
const uint32_t kTimeServerMaxSkewSeconds = 10;

const char kTimeServiceURL[] = "http://clients2.google.com/time/1/current";

// This is an ECDSA prime256v1 named-curve key.
const int kKeyVersion = 7;
const uint8_t kKeyPubBytes[] = {
    0x30, 0x59, 0x30, 0x13, 0x06, 0x07, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x02,
    0x01, 0x06, 0x08, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03, 0x01, 0x07, 0x03,
    0x42, 0x00, 0x04, 0x9F, 0xB4, 0x82, 0x7E, 0xAE, 0x02, 0xA2, 0xF2, 0x9C,
    0x32, 0x8E, 0xF8, 0x00, 0xFC, 0x75, 0x45, 0xCF, 0x45, 0x36, 0x01, 0x71,
    0x93, 0x57, 0x54, 0x1C, 0xA7, 0xC5, 0x09, 0xDA, 0xB1, 0xBC, 0x36, 0xB1,
    0x44, 0x1C, 0x2E, 0x12, 0x58, 0x2F, 0xE2, 0x27, 0x40, 0x40, 0x42, 0xEE,
    0x95, 0x7A, 0xAC, 0xE4, 0x33, 0xAC, 0xAA, 0x09, 0x6F, 0x5C, 0x0F, 0x94,
    0xA7, 0xB4, 0xB5, 0xE2, 0x6B, 0xB6, 0xC4};

// Number of samples to be used for the computation of clock drift.
constexpr base::FeatureParam<NetworkTimeTracker::ClockDriftSamples>::Option
    kClockDriftSamplesOptions[] = {
        {NetworkTimeTracker::ClockDriftSamples::NO_SAMPLES, "0"},
        {NetworkTimeTracker::ClockDriftSamples::TWO_SAMPLES, "2"}};
constexpr base::FeatureParam<NetworkTimeTracker::ClockDriftSamples>
    kClockDriftSamples{&kNetworkTimeServiceQuerying, "ClockDriftSamples",
                       NetworkTimeTracker::ClockDriftSamples::NO_SAMPLES,
                       &kClockDriftSamplesOptions};

// Distance between the clock drift samples.
constexpr base::FeatureParam<base::TimeDelta> kClockDriftSamplesDistance{
    &kNetworkTimeServiceQuerying, "ClockDriftSamplesDistance",
    base::Seconds(2)};

std::string GetServerProof(
    scoped_refptr<net::HttpResponseHeaders> response_headers) {
  std::string proof;
  return response_headers->EnumerateHeader(nullptr, "x-cup-server-proof",
                                           &proof)
             ? proof
             : std::string();
}

void UmaHistogramCustomTimesClockSkew(const char* name,
                                      base::TimeDelta sample) {
  base::UmaHistogramCustomTimes(name, sample, base::Milliseconds(1),
                                base::Days(7), 50);
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
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : server_url_(kTimeServiceURL),
      max_response_size_(1024),
      backoff_(kBackoffInterval.Get()),
      url_loader_factory_(std::move(url_loader_factory)),
      clock_(std::move(clock)),
      tick_clock_(std::move(tick_clock)),
      pref_service_(pref_service),
      time_query_completed_(false) {
  const base::Value::Dict& time_mapping =
      pref_service_->GetDict(prefs::kNetworkTimeMapping);
  absl::optional<double> time_js = time_mapping.FindDouble(kPrefTime);
  absl::optional<double> ticks_js = time_mapping.FindDouble(kPrefTicks);
  absl::optional<double> uncertainty_js =
      time_mapping.FindDouble(kPrefUncertainty);
  absl::optional<double> network_time_js =
      time_mapping.FindDouble(kPrefNetworkTime);
  if (time_js && ticks_js && uncertainty_js && network_time_js) {
    time_at_last_measurement_ = base::Time::FromJsTime(*time_js);
    ticks_at_last_measurement_ =
        base::TimeTicks::FromInternalValue(static_cast<int64_t>(*ticks_js));
    network_time_uncertainty_ = base::TimeDelta::FromInternalValue(
        static_cast<int64_t>(*uncertainty_js));
    network_time_at_last_measurement_ =
        base::Time::FromJsTime(*network_time_js);
  }
  base::Time now = clock_->Now();
  if (ticks_at_last_measurement_ > tick_clock_->NowTicks() ||
      time_at_last_measurement_ > now ||
      now - time_at_last_measurement_ > base::Days(kSerializedDataMaxAgeDays)) {
    // Drop saved mapping if either clock has run backward, or the data are too
    // old.
    pref_service_->ClearPref(prefs::kNetworkTimeMapping);
    network_time_at_last_measurement_ = base::Time();  // Reset.
  }

  base::StringPiece public_key = {reinterpret_cast<const char*>(kKeyPubBytes),
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
  network_time_at_last_measurement_ = network_time;

  // Calculate the delay since the network time was received.
  base::TimeTicks now_ticks = tick_clock_->NowTicks();
  base::TimeDelta task_delay = now_ticks - post_time;
  DCHECK_GE(task_delay.InMilliseconds(), 0);
  DCHECK_GE(latency.InMilliseconds(), 0);
  // Estimate that the time was set midway through the latency time.
  base::TimeDelta offset = task_delay + latency / 2;
  ticks_at_last_measurement_ = now_ticks - offset;
  time_at_last_measurement_ = clock_->Now() - offset;

  // Can't assume a better time than the resolution of the given time and the
  // ticks measurements involved, each with their own uncertainty.  1 & 2 are
  // the ones used to compute the latency, 3 is the Now() from when this task
  // was posted, 4 and 5 are the Now() and NowTicks() above, and 6 and 7 will be
  // the Now() and NowTicks() in GetNetworkTime().
  network_time_uncertainty_ =
      resolution + latency +
      kNumTimeMeasurements * base::Milliseconds(kTicksResolutionMs);

  base::Value::Dict time_mapping;
  time_mapping.Set(kPrefTime, time_at_last_measurement_.ToJsTime());
  time_mapping.Set(
      kPrefTicks,
      static_cast<double>(ticks_at_last_measurement_.ToInternalValue()));
  time_mapping.Set(
      kPrefUncertainty,
      static_cast<double>(network_time_uncertainty_.ToInternalValue()));
  time_mapping.Set(kPrefNetworkTime,
                   network_time_at_last_measurement_.ToJsTime());
  pref_service_->Set(prefs::kNetworkTimeMapping,
                     base::Value(std::move(time_mapping)));
}

bool NetworkTimeTracker::AreTimeFetchesEnabled() const {
  return base::FeatureList::IsEnabled(kNetworkTimeServiceQuerying);
}

NetworkTimeTracker::FetchBehavior NetworkTimeTracker::GetFetchBehavior() const {
  return kFetchBehavior.Get();
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

void NetworkTimeTracker::SetPublicKeyForTesting(base::StringPiece key) {
  query_signer_ = client_update_protocol::Ecdsa::Create(kKeyVersion, key);
}

bool NetworkTimeTracker::QueryTimeServiceForTesting(bool on_demand) {
  CheckTime(on_demand ? CheckTimeType::ON_DEMAND : CheckTimeType::BACKGROUND);
  return time_fetcher_ != nullptr;
}

void NetworkTimeTracker::WaitForFetch() {
  base::RunLoop run_loop;
  fetch_completion_callbacks_.push_back(run_loop.QuitClosure());
  run_loop.Run();
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

NetworkTimeTracker::NetworkTimeResult NetworkTimeTracker::GetNetworkTime(
    base::Time* network_time,
    base::TimeDelta* uncertainty) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(network_time);
  if (network_time_at_last_measurement_.is_null()) {
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

  DCHECK(!ticks_at_last_measurement_.is_null());
  DCHECK(!time_at_last_measurement_.is_null());
  base::TimeDelta tick_delta =
      tick_clock_->NowTicks() - ticks_at_last_measurement_;
  base::TimeDelta time_delta = clock_->Now() - time_at_last_measurement_;
  if (time_delta.InMilliseconds() < 0) {  // Has wall clock run backward?
    DVLOG(1) << "Discarding network time due to wall clock running backward";
    network_time_at_last_measurement_ = base::Time();
    return NETWORK_TIME_SYNC_LOST;
  }
  // Now we know that both |tick_delta| and |time_delta| are positive.
  base::TimeDelta divergence = tick_delta - time_delta;
  if (divergence.magnitude() > base::Seconds(kClockDivergenceSeconds)) {
    // Most likely either the machine has suspended, or the wall clock has been
    // reset.
    DVLOG(1) << "Discarding network time due to clocks diverging";

    network_time_at_last_measurement_ = base::Time();
    return NETWORK_TIME_SYNC_LOST;
  }
  *network_time = network_time_at_last_measurement_ + tick_delta;
  if (uncertainty) {
    *uncertainty = network_time_uncertainty_ + divergence;
  }
  return NETWORK_TIME_AVAILABLE;
}

bool NetworkTimeTracker::StartTimeFetch(base::OnceClosure closure) {
  DCHECK(thread_checker_.CalledOnValidThread());
  FetchBehavior behavior = kFetchBehavior.Get();
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
  CheckTime(CheckTimeType::ON_DEMAND);

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

void NetworkTimeTracker::CheckTime(CheckTimeType check_type) {
  DCHECK(thread_checker_.CalledOnValidThread());

  base::TimeDelta interval = kCheckTimeInterval.Get();
  if (interval.is_negative()) {
    interval = kCheckTimeInterval.default_value;
  }

  // If NetworkTimeTracker is waking up after a backoff, this will reset the
  // timer to its default faster frequency.
  QueueCheckTime(interval);

  if (!ShouldIssueTimeQuery(check_type)) {
    return;
  }

  std::string query_string;
  query_signer_->SignRequest("", &query_string);
  GURL url = server_url_;
  GURL::Replacements replacements;
  replacements.SetQueryStr(query_string);
  url = url.ReplaceComponents(replacements);

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
  resource_request->url = url;
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
                     base::Unretained(this), check_type),
      max_response_size_);

  timer_.Stop();  // Restarted in OnURLLoaderComplete().
}

bool NetworkTimeTracker::UpdateTimeFromResponse(
    CheckTimeType check_type,
    std::unique_ptr<std::string> response_body) {
  int response_code = 0;
  if (time_fetcher_->ResponseInfo() && time_fetcher_->ResponseInfo()->headers)
    response_code = time_fetcher_->ResponseInfo()->headers->response_code();
  if (response_code != 200 || !response_body) {
    time_query_completed_ = true;
    DVLOG(1) << "fetch failed code=" << response_code;
    return false;
  }

  base::StringPiece response(*response_body);

  DCHECK(query_signer_);
  if (!query_signer_->ValidateResponse(
          response, GetServerProof(time_fetcher_->ResponseInfo()->headers))) {
    DVLOG(1) << "invalid signature";
    return false;
  }
  response.remove_prefix(5);  // Skips leading )]}'\n
  absl::optional<base::Value> value = base::JSONReader::Read(response);
  if (!value) {
    DVLOG(1) << "bad JSON";
    return false;
  }
  if (!value->is_dict()) {
    DVLOG(1) << "not a dictionary";
    return false;
  }
  absl::optional<double> current_time_millis =
      value->GetDict().FindDouble("current_time_millis");
  if (!current_time_millis) {
    DVLOG(1) << "no current_time_millis";
    return false;
  }

  // There is a "server_nonce" key here too, but it serves no purpose other than
  // to make the server's response unpredictable.
  base::Time current_time = base::Time::FromJsTime(*current_time_millis);
  base::TimeDelta resolution =
      base::Milliseconds(1) + base::Seconds(kTimeServerMaxSkewSeconds);

  // Record histograms for the latency of the time query and the time delta
  // between time fetches.
  base::TimeDelta latency =
      time_fetcher_->ResponseInfo()->load_timing.receive_headers_start -
      time_fetcher_->ResponseInfo()->load_timing.send_end;

  last_fetched_time_ = current_time;

  if (check_type == CheckTimeType::BACKGROUND) {
    ProcessClockHistograms(current_time, latency);
  }

  UpdateNetworkTime(current_time, resolution, latency, tick_clock_->NowTicks());
  return true;
}

void NetworkTimeTracker::ProcessClockHistograms(base::Time current_time,
                                                base::TimeDelta latency) {
  // Compute the skew by comparing the reference clock to the system clock.
  // Note that the server processed our query roughly `fetch_latency/2` units
  // of time in the past. Adjust the `current_time` accordingly.
  base::TimeDelta system_clock_skew =
      clock_->Now() - (current_time + latency / 2);
  if (clock_drift_measurement_triggered_) {
    ClockDriftSample s;
    s.latency = latency;
    s.skew = system_clock_skew;
    s.timestamp = current_time + latency / 2;
    clock_drift_samples_.push_back(s);

    // We need one more sample than the number used for the
    // computation because the middle sample is not used by the
    // central finite difference formulas.
    if (clock_drift_samples_.size() ==
        static_cast<uint8_t>(kClockDriftSamples.Get()) + 1) {
      RecordClockDriftHistograms();
      clock_drift_measurement_triggered_ = false;

      // Go to sleep for a long time after we recorded the histogram.
      QueueCheckTime(kBackoffInterval.Get());
    }
  } else {
    // We always trigger the clock drift measurements right after clock skew
    // is measured.
    RecordClockSkewHistograms(system_clock_skew, latency);
    MaybeTriggerClockDriftMeasurements();
  }
}

void NetworkTimeTracker::RecordClockSkewHistograms(
    base::TimeDelta system_clock_skew,
    base::TimeDelta fetch_latency) {
  // Explicitly record clock skew of zero in the "positive" histograms.
  if (system_clock_skew >= base::TimeDelta()) {
    UmaHistogramCustomTimesClockSkew(
        "PrivacyBudget.ClockSkew.Magnitude.Positive", system_clock_skew);
  } else if (system_clock_skew.is_negative()) {
    UmaHistogramCustomTimesClockSkew(
        "PrivacyBudget.ClockSkew.Magnitude.Negative", -system_clock_skew);
  }

  base::UmaHistogramTimes("PrivacyBudget.ClockSkew.FetchLatency",
                          fetch_latency);
  historical_latencies_.Record(fetch_latency);

  absl::optional<base::TimeDelta> latency_jitter =
      historical_latencies_.StdDeviation();
  if (latency_jitter.has_value()) {
    base::UmaHistogramTimes("PrivacyBudget.ClockSkew.FetchLatencyJitter",
                            latency_jitter.value());
  }
}

void NetworkTimeTracker::MaybeTriggerClockDriftMeasurements() {
  if (clock_drift_measurement_triggered_ ||
      static_cast<uint8_t>(kClockDriftSamples.Get()) == 0) {
    return;
  }
  clock_drift_samples_.clear();
  clock_drift_measurement_triggered_ = true;
  QueueCheckTime(kClockDriftSamplesDistance.Get());
}

// The clock drift is the time derivative of clock skew. We use the central
// finite difference method to compute the derivative using the skew samples
// we have collected. Because we use *central* finite differences, the middle
// sample does not take part in the computation.
double NetworkTimeTracker::ComputeClockDrift() {
  if (kClockDriftSamplesDistance.Get() <= base::TimeDelta(base::Seconds(0)))
    return std::numeric_limits<double>::infinity();
  CHECK(clock_drift_samples_.size() >= 2);
  return (clock_drift_samples_[2].skew - clock_drift_samples_[0].skew) /
         (clock_drift_samples_[2].timestamp -
          clock_drift_samples_[0].timestamp);
}

void NetworkTimeTracker::RecordClockDriftHistograms() {
  if (clock_drift_samples_.size() !=
      static_cast<uint8_t>(kClockDriftSamples.Get()) + 1) {
    return;
  }

  double clock_drift_microseconds = ComputeClockDrift() * 1e+6;
  if (std::isfinite(clock_drift_microseconds)) {
    if (clock_drift_microseconds >= 0) {
      base::UmaHistogramCounts100000(
          "PrivacyBudget.ClockDrift.Magnitude.Positive",
          base::ClampRound(clock_drift_microseconds));
    } else {
      base::UmaHistogramCounts100000(
          "PrivacyBudget.ClockDrift.Magnitude.Negative",
          base::ClampRound(-clock_drift_microseconds));
    }
  }

  base::UmaHistogramCounts100000(
      "PrivacyBudget.ClockDrift.FetchLatencyVariance",
      base::ClampRound(ComputeClockDriftLatencyVariance()));
}

double NetworkTimeTracker::ComputeClockDriftLatencyVariance() {
  if (static_cast<uint8_t>(kClockDriftSamples.Get()) == 0) {
    return std::numeric_limits<double>::infinity();
  }

  base::TimeDelta mean = base::Seconds(0);
  for (size_t i = 0; i < clock_drift_samples_.size(); i++) {
    // Exclude middle sample since we do not use it
    if (i != clock_drift_samples_.size() / 2) {
      mean += clock_drift_samples_[i].latency;
    }
  }
  mean /= static_cast<uint8_t>(kClockDriftSamples.Get());

  double variance = 0;
  for (size_t i = 0; i < clock_drift_samples_.size(); i++) {
    base::TimeDelta diff_from_mean = mean - clock_drift_samples_[i].latency;
    // Exclude middle sample since we do not use it
    if (i != clock_drift_samples_.size() / 2) {
      variance +=
          diff_from_mean.InMilliseconds() * diff_from_mean.InMilliseconds();
    }
  }
  variance /= static_cast<uint8_t>(kClockDriftSamples.Get());

  return variance;
}

void NetworkTimeTracker::OnURLLoaderComplete(
    CheckTimeType check_type,
    std::unique_ptr<std::string> response_body) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(time_fetcher_);

  time_query_completed_ = true;

  // After completion of a query, whether succeeded or failed, go to sleep
  // for a long time.
  if (!UpdateTimeFromResponse(
          check_type,
          std::move(response_body))) {  // On error, back off.
    clock_drift_measurement_triggered_ = false;
    if (backoff_ < base::Days(2)) {
      backoff_ *= 2;
    }
  } else {
    backoff_ = kBackoffInterval.Get();

    // If the clock skew measurements were triggered restrict the backoff to
    // their distance.
    if (clock_drift_measurement_triggered_)
      backoff_ = kClockDriftSamplesDistance.Get();
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
  FetchBehavior behavior = kFetchBehavior.Get();
  if (behavior == FETCHES_IN_BACKGROUND_ONLY ||
      behavior == FETCHES_IN_BACKGROUND_AND_ON_DEMAND) {
    timer_.Start(
        FROM_HERE, delay,
        base::BindRepeating(&NetworkTimeTracker::CheckTime,
                            base::Unretained(this), CheckTimeType::BACKGROUND));
  }
}

bool NetworkTimeTracker::ShouldIssueTimeQuery(CheckTimeType check_type) {
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

  // If we are in the process of measuring drift, we should query.
  if (clock_drift_measurement_triggered_ &&
      check_type == CheckTimeType::BACKGROUND) {
    return true;
  }

  // Otherwise, make the decision at random.
  double probability = kRandomQueryProbability.Get();
  if (probability < 0.0 || probability > 1.0) {
    probability = kRandomQueryProbability.default_value;
  }

  return base::RandDouble() < probability;
}

}  // namespace network_time
