// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/service/variations_service.h"

#include <stddef.h>
#include <stdint.h>

#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/base_switches.h"
#include "base/bind.h"
#include "base/build_time.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "base/task/post_task.h"
#include "base/task_runner_util.h"
#include "base/timer/elapsed_timer.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "base/version.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "components/encrypted_messages/encrypted_message.pb.h"
#include "components/encrypted_messages/message_encrypter.h"
#include "components/metrics/clean_exit_beacon.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/network_time/network_time_tracker.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/variations/pref_names.h"
#include "components/variations/proto/variations_seed.pb.h"
#include "components/variations/seed_response.h"
#include "components/variations/variations_seed_processor.h"
#include "components/variations/variations_seed_simulator.h"
#include "components/variations/variations_switches.h"
#include "components/variations/variations_url_constants.h"
#include "components/version_info/version_info.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/network_change_notifier.h"
#include "net/base/url_util.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "ui/base/device_form_factor.h"
#include "url/gurl.h"

#if defined(OS_ANDROID)
#include "components/variations/android/variations_seed_bridge.h"
#endif  // OS_ANDROID

namespace variations {

const base::Feature kHttpRetryFeature{"VariationsHttpRetry",
                                      base::FEATURE_ENABLED_BY_DEFAULT};

namespace {

// Constants used for encrypting the if-none-match header if we are retrieving a
// seed over http.
const char kEncryptedMessageLabel[] = "chrome variations";

// TODO(crbug.com/792239): Change this key to a unique VariationsService one,
// once the matching private key is changed server side.
// Key is used to encrypt headers in seed retrieval requests that happen over
// HTTP connections (when retrying after an unsuccessful HTTPS retrieval
// attempt).
const uint8_t kServerPublicKey[] = {
    0x51, 0xcc, 0x52, 0x67, 0x42, 0x47, 0x3b, 0x10, 0xe8, 0x63, 0x18,
    0x3c, 0x61, 0xa7, 0x96, 0x76, 0x86, 0x91, 0x40, 0x71, 0x39, 0x5f,
    0x31, 0x1a, 0x39, 0x5b, 0x76, 0xb1, 0x6b, 0x3d, 0x6a, 0x2b};

const uint32_t kServerPublicKeyVersion = 1;

// For the HTTP date headers, the resolution of the server time is 1 second.
const base::TimeDelta kServerTimeResolution = base::TimeDelta::FromSeconds(1);

// Whether the VariationsService should fetch the seed for testing.
bool g_should_fetch_for_testing = false;

// Returns a string that will be used for the value of the 'osname' URL param
// to the variations server.
std::string GetPlatformString() {
#if defined(OS_WIN)
  return "win";
#elif defined(OS_IOS)
  return "ios";
#elif defined(OS_MACOSX)
  return "mac";
#elif defined(OS_CHROMEOS)
  return "chromeos";
#elif defined(OS_ANDROID)
  return "android";
#elif defined(OS_FUCHSIA)
  return "fuchsia";
#elif defined(OS_LINUX) || defined(OS_BSD) || defined(OS_SOLARIS)
  // Default BSD and SOLARIS to Linux to not break those builds, although these
  // platforms are not officially supported by Chrome.
  return "linux";
#else
#error Unknown platform
#endif
}

// Gets the restrict parameter from either the passed override, the client or
// |policy_pref_service|.
std::string GetRestrictParameterValue(const std::string& restrict_mode_override,
                                      VariationsServiceClient* client,
                                      PrefService* policy_pref_service) {
  if (!restrict_mode_override.empty())
    return restrict_mode_override;

  std::string parameter;
  if (client->OverridesRestrictParameter(&parameter) || !policy_pref_service)
    return parameter;

  return policy_pref_service->GetString(prefs::kVariationsRestrictParameter);
}

// Reported to UMA, keep in sync with enums.xml and don't renumber entries.
enum ResourceRequestsAllowedState {
  RESOURCE_REQUESTS_ALLOWED,
  RESOURCE_REQUESTS_NOT_ALLOWED,
  RESOURCE_REQUESTS_ALLOWED_NOTIFIED,
  RESOURCE_REQUESTS_NOT_ALLOWED_EULA_NOT_ACCEPTED,
  RESOURCE_REQUESTS_NOT_ALLOWED_NETWORK_DOWN,
  RESOURCE_REQUESTS_NOT_ALLOWED_COMMAND_LINE_DISABLED,
  RESOURCE_REQUESTS_NOT_ALLOWED_NETWORK_STATE_NOT_INITIALIZED,
  RESOURCE_REQUESTS_ALLOWED_ENUM_SIZE,
};

// Records UMA histogram with the current resource requests allowed state.
void RecordRequestsAllowedHistogram(ResourceRequestsAllowedState state) {
  UMA_HISTOGRAM_ENUMERATION("Variations.ResourceRequestsAllowed", state,
                            RESOURCE_REQUESTS_ALLOWED_ENUM_SIZE);
}

enum VariationsSeedExpiry {
  VARIATIONS_SEED_EXPIRY_NOT_EXPIRED,
  VARIATIONS_SEED_EXPIRY_FETCH_TIME_MISSING,
  VARIATIONS_SEED_EXPIRY_EXPIRED,
  VARIATIONS_SEED_EXPIRY_ENUM_SIZE,
};

// Converts ResourceRequestAllowedNotifier::State to the corresponding
// ResourceRequestsAllowedState value.
ResourceRequestsAllowedState ResourceRequestStateToHistogramValue(
    web_resource::ResourceRequestAllowedNotifier::State state) {
  using web_resource::ResourceRequestAllowedNotifier;
  switch (state) {
    case ResourceRequestAllowedNotifier::DISALLOWED_EULA_NOT_ACCEPTED:
      return RESOURCE_REQUESTS_NOT_ALLOWED_EULA_NOT_ACCEPTED;
    case ResourceRequestAllowedNotifier::DISALLOWED_NETWORK_DOWN:
      return RESOURCE_REQUESTS_NOT_ALLOWED_NETWORK_DOWN;
    case ResourceRequestAllowedNotifier::DISALLOWED_COMMAND_LINE_DISABLED:
      return RESOURCE_REQUESTS_NOT_ALLOWED_COMMAND_LINE_DISABLED;
    case ResourceRequestAllowedNotifier::
        DISALLOWED_NETWORK_STATE_NOT_INITIALIZED:
      return RESOURCE_REQUESTS_NOT_ALLOWED_NETWORK_STATE_NOT_INITIALIZED;
    case ResourceRequestAllowedNotifier::ALLOWED:
      return RESOURCE_REQUESTS_ALLOWED;
  }
  NOTREACHED();
  return RESOURCE_REQUESTS_NOT_ALLOWED;
}

// Returns the header value for |name| from |headers| or an empty string if not
// set.
std::string GetHeaderValue(const net::HttpResponseHeaders* headers,
                           const base::StringPiece& name) {
  std::string value;
  headers->EnumerateHeader(nullptr, name, &value);
  return value;
}

// Returns the list of values for |name| from |headers|. If the header in not
// set, return an empty list.
std::vector<std::string> GetHeaderValuesList(
    const net::HttpResponseHeaders* headers,
    const base::StringPiece& name) {
  std::vector<std::string> values;
  size_t iter = 0;
  std::string value;
  while (headers->EnumerateHeader(&iter, name, &value)) {
    values.push_back(value);
  }
  return values;
}

// Looks for delta and gzip compression instance manipulation flags set by the
// server in |headers|. Checks the order of flags and presence of unknown
// instance manipulations. If successful, |is_delta_compressed| and
// |is_gzip_compressed| contain compression flags and true is returned.
bool GetInstanceManipulations(const net::HttpResponseHeaders* headers,
                              bool* is_delta_compressed,
                              bool* is_gzip_compressed) {
  std::vector<std::string> ims = GetHeaderValuesList(headers, "IM");
  const auto delta_im = std::find(ims.begin(), ims.end(), "x-bm");
  const auto gzip_im = std::find(ims.begin(), ims.end(), "gzip");
  *is_delta_compressed = delta_im != ims.end();
  *is_gzip_compressed = gzip_im != ims.end();

  // The IM field should not have anything but x-bm and gzip.
  size_t im_count = (*is_delta_compressed ? 1 : 0) +
      (*is_gzip_compressed ? 1 : 0);
  if (im_count != ims.size()) {
    DVLOG(1) << "Unrecognized instance manipulations in "
             << base::JoinString(ims, ",")
             << "; only x-bm and gzip are supported";
    return false;
  }

  // The IM field defines order in which instance manipulations were applied.
  // The client requests and supports gzip-compressed delta-compressed seeds,
  // but not vice versa.
  if (*is_delta_compressed && *is_gzip_compressed && delta_im > gzip_im) {
    DVLOG(1) << "Unsupported instance manipulations order: "
             << "requested x-bm,gzip but received gzip,x-bm";
    return false;
  }

  return true;
}

// Variations seed fetching is only enabled in official Chrome builds, if a URL
// is specified on the command line, and for testing.
bool IsFetchingEnabled() {
#if !BUILDFLAG(GOOGLE_CHROME_BRANDING)
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kVariationsServerURL) &&
      !g_should_fetch_for_testing) {
    DVLOG(1)
        << "Not performing repeated fetching in unofficial build without --"
        << switches::kVariationsServerURL << " specified.";
    return false;
  }
#endif
  return true;
}

std::unique_ptr<SeedResponse> MaybeImportFirstRunSeed(
    PrefService* local_state) {
#if defined(OS_ANDROID)
  if (!local_state->HasPrefPath(prefs::kVariationsSeedSignature)) {
    DVLOG(1) << "Importing first run seed from Java preferences.";
    return android::GetVariationsFirstRunSeed();
  }
#endif
  return nullptr;
}

// Called when the VariationsSeedStore first stores a seed.
void OnInitialSeedStored() {
#if defined(OS_ANDROID)
  android::MarkVariationsSeedAsStored();
  android::ClearJavaFirstRunPrefs();
#endif
}

}  // namespace

VariationsService::VariationsService(
    std::unique_ptr<VariationsServiceClient> client,
    std::unique_ptr<web_resource::ResourceRequestAllowedNotifier> notifier,
    PrefService* local_state,
    metrics::MetricsStateManager* state_manager,
    const UIStringOverrider& ui_string_overrider)
    : client_(std::move(client)),
      local_state_(local_state),
      state_manager_(state_manager),
      policy_pref_service_(local_state),
      initial_request_completed_(false),
      disable_deltas_for_next_request_(false),
      resource_request_allowed_notifier_(std::move(notifier)),
      request_count_(0),
      safe_seed_manager_(state_manager->clean_exit_beacon()->exited_cleanly(),
                         local_state),
      field_trial_creator_(local_state,
                           client_.get(),
                           std::make_unique<VariationsSeedStore>(
                               local_state,
                               MaybeImportFirstRunSeed(local_state),
                               base::BindOnce(&OnInitialSeedStored)),
                           ui_string_overrider),
      last_request_was_http_retry_(false) {
  DCHECK(client_);
  DCHECK(resource_request_allowed_notifier_);
}

VariationsService::~VariationsService() {
}

void VariationsService::PerformPreMainMessageLoopStartup() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(field_trial_creator_.IsOverrideResourceMapEmpty());

  InitResourceRequestedAllowedNotifier();

// Android instead calls OnAppEnterForeground() which then calls
// StartRepeatedVariationsSeedFetch(). This is too early to do it on Android
// because at this point the |restrict_mode_| hasn't been set yet. See also
// the CHECK in SetRestrictMode().
#if !defined(OS_ANDROID)
  if (!IsFetchingEnabled())
    return;

  StartRepeatedVariationsSeedFetch();
#endif  // !defined(OS_ANDROID)
}

std::string VariationsService::LoadPermanentConsistencyCountry(
    const base::Version& version,
    const std::string& latest_country) {
  return field_trial_creator_.LoadPermanentConsistencyCountry(version,
                                                              latest_country);
}

bool VariationsService::EncryptString(const std::string& plaintext,
                                      std::string* encrypted) {
  encrypted_messages::EncryptedMessage encrypted_message;
  if (!encrypted_messages::EncryptSerializedMessage(
          kServerPublicKey, kServerPublicKeyVersion, kEncryptedMessageLabel,
          plaintext, &encrypted_message) ||
      !encrypted_message.SerializeToString(encrypted)) {
    return false;
  }
  return true;
}

void VariationsService::AddObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observer_list_.AddObserver(observer);
}

void VariationsService::RemoveObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observer_list_.RemoveObserver(observer);
}

void VariationsService::OnAppEnterForeground() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!IsFetchingEnabled())
    return;

  // On mobile platforms, initialize the fetch scheduler when we receive the
  // first app foreground notification.
  if (!request_scheduler_)
    StartRepeatedVariationsSeedFetch();
  request_scheduler_->OnAppEnterForeground();
}

void VariationsService::SetRestrictMode(const std::string& restrict_mode) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // This should be called before the server URL has been computed. Note: This
  // uses a CHECK because this is relevant for the behavior in release official
  // builds that talk to the variations server - which don't enable DCHECKs.
  CHECK(variations_server_url_.is_empty());
  restrict_mode_ = restrict_mode;
}

GURL VariationsService::GetVariationsServerURL(HttpOptions http_options) {
  const bool secure = http_options == USE_HTTPS;
  const std::string restrict_mode = GetRestrictParameterValue(
      restrict_mode_, client_.get(), policy_pref_service_);

  // If there's a restrict mode, we don't want to fall back to HTTP to avoid
  // toggling restrict mode state.
  if (!secure && !restrict_mode.empty())
    return GURL();

  std::string server_url_string(
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          secure ? switches::kVariationsServerURL
                 : switches::kVariationsInsecureServerURL));
  if (server_url_string.empty())
    server_url_string = secure ? kDefaultServerUrl : kDefaultInsecureServerUrl;
  GURL server_url = GURL(server_url_string);
  if (!restrict_mode.empty()) {
    DCHECK(secure);
    server_url = net::AppendOrReplaceQueryParameter(server_url, "restrict",
                                                    restrict_mode);
  }
  server_url = net::AppendOrReplaceQueryParameter(server_url, "osname",
                                                  GetPlatformString());

  // Add channel to the request URL.
  version_info::Channel channel = client_->GetChannelForVariations();
  if (channel != version_info::Channel::UNKNOWN) {
    server_url = net::AppendOrReplaceQueryParameter(
        server_url, "channel", version_info::GetChannelString(channel));
  }

  // Add milestone to the request URL.
  std::string version = version_info::GetVersionNumber();
  std::vector<std::string> version_parts = base::SplitString(
      version, ".", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (version_parts.size() > 0) {
    server_url = net::AppendOrReplaceQueryParameter(server_url, "milestone",
                                                    version_parts[0]);
  }

  DCHECK(server_url.is_valid());
  return server_url;
}

void VariationsService::EnsureLocaleEquals(const std::string& locale) {
#if defined(OS_CHROMEOS)
  // Chrome OS may switch language on the fly.
  DCHECK_EQ(locale, field_trial_creator_.application_locale());
#else

#if defined(OS_ANDROID)
  // TODO(asvitkine): Speculative early return to silence CHECK failures on
  // Android, see crbug.com/912320.
  if (locale.empty())
    return;
#endif

  // Uses a CHECK rather than a DCHECK to ensure that issues are caught since
  // problems in this area may only appear in the wild due to official builds
  // and end user machines.
  CHECK_EQ(locale, field_trial_creator_.application_locale());
#endif
}

// static
std::string VariationsService::GetDefaultVariationsServerURLForTesting() {
  return kDefaultServerUrl;
}

// static
void VariationsService::RegisterPrefs(PrefRegistrySimple* registry) {
  SafeSeedManager::RegisterPrefs(registry);
  VariationsSeedStore::RegisterPrefs(registry);

  // This preference will only be written by the policy service, which will fill
  // it according to a value stored in the User Policy.
  registry->RegisterStringPref(prefs::kVariationsRestrictParameter,
                               std::string());
  // This preference is used to override the variations country code which is
  // consistent across different chrome version.
  registry->RegisterStringPref(prefs::kVariationsPermanentOverriddenCountry,
                               std::string());
  // This preference keeps track of the country code used to filter
  // permanent-consistency studies.
  registry->RegisterListPref(prefs::kVariationsPermanentConsistencyCountry);
}

// static
void VariationsService::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  // This preference will only be written by the policy service, which will fill
  // it according to a value stored in the User Policy.
  registry->RegisterStringPref(prefs::kVariationsRestrictParameter,
                               std::string());
}

// static
std::unique_ptr<VariationsService> VariationsService::Create(
    std::unique_ptr<VariationsServiceClient> client,
    PrefService* local_state,
    metrics::MetricsStateManager* state_manager,
    const char* disable_network_switch,
    const UIStringOverrider& ui_string_overrider,
    web_resource::ResourceRequestAllowedNotifier::NetworkConnectionTrackerGetter
        network_connection_tracker_getter) {
  std::unique_ptr<VariationsService> result;
  result.reset(new VariationsService(
      std::move(client),
      std::make_unique<web_resource::ResourceRequestAllowedNotifier>(
          local_state, disable_network_switch,
          std::move(network_connection_tracker_getter)),
      local_state, state_manager, ui_string_overrider));
  return result;
}

// static
void VariationsService::EnableFetchForTesting() {
  g_should_fetch_for_testing = true;
}

void VariationsService::DoActualFetch() {
  DoFetchFromURL(variations_server_url_, false);
}

bool VariationsService::DoFetchFromURL(const GURL& url, bool is_http_retry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsFetchingEnabled());

  safe_seed_manager_.RecordFetchStarted();

  // Normally, there shouldn't be a |pending_seed_request_| when this fires.
  // However it's not impossible - for example if Chrome was paused (e.g. in a
  // debugger or if the machine was suspended) and OnURLFetchComplete() hasn't
  // had a chance to run yet from the previous request. In this case, don't
  // start a new request and just let the previous one finish.
  if (pending_seed_request_)
    return false;

  last_request_was_http_retry_ = is_http_retry;

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("chrome_variations_service", R"(
        semantics {
          sender: "Chrome Variations Service"
          description:
            "Retrieves the list of Google Chrome's Variations from the server, "
            "which will apply to the next Chrome session upon a restart."
          trigger:
            "Requests are made periodically while Google Chrome is running."
          data: "The operating system name."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting: "This feature cannot be disabled by settings."
          policy_exception_justification:
            "Not implemented, considered not required."
        })");
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = url;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  bool enable_deltas = false;
  std::string serial_number =
      field_trial_creator_.seed_store()->GetLatestSerialNumber();
  if (!serial_number.empty() && !disable_deltas_for_next_request_) {
    // Tell the server that delta-compressed seeds are supported.
    enable_deltas = true;
    // Get the seed only if its serial number doesn't match what we have.
    // If the fetch is an HTTP retry, encrypt the If-None-Match header.
    if (is_http_retry) {
      if (!EncryptString(serial_number, &serial_number)) {
        return false;
      }
      base::Base64Encode(serial_number, &serial_number);
    }
    resource_request->headers.SetHeader("If-None-Match", serial_number);
  }
  // Tell the server that delta-compressed and gzipped seeds are supported.
  const char* supported_im = enable_deltas ? "x-bm,gzip" : "gzip";
  resource_request->headers.SetHeader("A-IM", supported_im);

  pending_seed_request_ = network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation);
  // Ensure our callback is called even with "304 Not Modified" responses.
  pending_seed_request_->SetAllowHttpErrorResults(true);
  // Set the redirect callback so we can cancel on redirects.
  // base::Unretained is safe here since this class owns
  // |pending_seed_request_|'s lifetime.
  pending_seed_request_->SetOnRedirectCallback(base::BindRepeating(
      &VariationsService::OnSimpleLoaderRedirect, base::Unretained(this)));
  pending_seed_request_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      client_->GetURLLoaderFactory().get(),
      base::BindOnce(&VariationsService::OnSimpleLoaderComplete,
                     base::Unretained(this)));

  const base::TimeTicks now = base::TimeTicks::Now();
  base::TimeDelta time_since_last_fetch;
  // Record a time delta of 0 (default value) if there was no previous fetch.
  if (!last_request_started_time_.is_null())
    time_since_last_fetch = now - last_request_started_time_;
  UMA_HISTOGRAM_CUSTOM_COUNTS("Variations.TimeSinceLastFetchAttempt",
                              time_since_last_fetch.InMinutes(), 1,
                              base::TimeDelta::FromDays(7).InMinutes(), 50);
  UMA_HISTOGRAM_COUNTS_100("Variations.RequestCount", request_count_);
  ++request_count_;
  last_request_started_time_ = now;
  disable_deltas_for_next_request_ = false;
  return true;
}

bool VariationsService::StoreSeed(const std::string& seed_data,
                                  const std::string& seed_signature,
                                  const std::string& country_code,
                                  base::Time date_fetched,
                                  bool is_delta_compressed,
                                  bool is_gzip_compressed,
                                  bool fetched_insecurely) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::unique_ptr<VariationsSeed> seed(new VariationsSeed);
  if (!field_trial_creator_.seed_store()->StoreSeedData(
          seed_data, seed_signature, country_code, date_fetched,
          is_delta_compressed, is_gzip_compressed, fetched_insecurely,
          seed.get())) {
    return false;
  }

  RecordSuccessfulFetch();

  // Now, do simulation to determine if there are any kill-switches that were
  // activated by this seed. To do this, first get the Chrome version to do a
  // simulation with, which must be done on a background thread, and then do the
  // actual simulation on the UI thread.
  base::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      client_->GetVersionForSimulationCallback(),
      base::Bind(&VariationsService::PerformSimulationWithVersion,
                 weak_ptr_factory_.GetWeakPtr(), base::Passed(&seed)));
  return true;
}

std::unique_ptr<const base::FieldTrial::EntropyProvider>
VariationsService::CreateLowEntropyProvider() {
  return state_manager_->CreateLowEntropyProvider();
}

void VariationsService::InitResourceRequestedAllowedNotifier() {
  // ResourceRequestAllowedNotifier does not install an observer if there is no
  // NetworkChangeNotifier, which results in never being notified of changes to
  // network status.
  resource_request_allowed_notifier_->Init(this, false /* leaky */);
}

void VariationsService::StartRepeatedVariationsSeedFetch() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Initialize the Variations server URL.
  variations_server_url_ = GetVariationsServerURL(USE_HTTPS);

  // Initialize the fallback HTTP URL if the HTTP retry feature is enabled.
  if (base::FeatureList::IsEnabled(kHttpRetryFeature))
    insecure_variations_server_url_ = GetVariationsServerURL(USE_HTTP);

  DCHECK(!request_scheduler_);
  request_scheduler_.reset(VariationsRequestScheduler::Create(
      base::Bind(&VariationsService::FetchVariationsSeed,
                 weak_ptr_factory_.GetWeakPtr()),
      local_state_));
  // Note that the act of starting the scheduler will start the fetch, if the
  // scheduler deems appropriate.
  request_scheduler_->Start();
}

void VariationsService::FetchVariationsSeed() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const web_resource::ResourceRequestAllowedNotifier::State state =
      resource_request_allowed_notifier_->GetResourceRequestsAllowedState();
  RecordRequestsAllowedHistogram(ResourceRequestStateToHistogramValue(state));
  if (state != web_resource::ResourceRequestAllowedNotifier::ALLOWED) {
    DVLOG(1) << "Resource requests were not allowed. Waiting for notification.";
    return;
  }

  DoActualFetch();
}

void VariationsService::NotifyObservers(
    const VariationsSeedSimulator::Result& result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (result.kill_critical_group_change_count > 0) {
    for (auto& observer : observer_list_)
      observer.OnExperimentChangesDetected(Observer::CRITICAL);
  } else if (result.kill_best_effort_group_change_count > 0) {
    for (auto& observer : observer_list_)
      observer.OnExperimentChangesDetected(Observer::BEST_EFFORT);
  }
}

void VariationsService::OnSimpleLoaderComplete(
    std::unique_ptr<std::string> response_body) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  OnSimpleLoaderCompleteOrRedirect(std::move(response_body), false);
}

void VariationsService::OnSimpleLoaderRedirect(
    const net::RedirectInfo& redirect_info,
    const network::mojom::URLResponseHead& response_head,
    std::vector<std::string>* to_be_removed_headers) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  OnSimpleLoaderCompleteOrRedirect(nullptr, true);
}

void VariationsService::OnSimpleLoaderCompleteOrRedirect(
    std::unique_ptr<std::string> response_body,
    bool was_redirect) {
  TRACE_EVENT0("browser", "VariationsService::OnSimpleLoaderCompleteOrRedirect");
  const bool is_first_request = !initial_request_completed_;
  initial_request_completed_ = true;

  bool is_success = false;
  int net_error = net::ERR_INVALID_REDIRECT;
  scoped_refptr<net::HttpResponseHeaders> headers;

  int response_code = -1;
  // We use the final URL HTTPS/HTTP value to pass to StoreSeed, since
  // signature validation should be forced on for any HTTP fetch, including
  // redirects from HTTPS to HTTP. We default to false since we can't get this
  // value (nor will it be used) in the redirect case.
  bool final_url_was_https = false;

  // Variations seed fetches should not follow redirects, so if this request was
  // redirected, keep the default values for |net_error| and |is_success| (treat
  // it as a net::ERR_INVALID_REDIRECT), and the fetch will be cancelled when
  // pending_seed_request is reset.
  if (!was_redirect) {
    final_url_was_https =
        pending_seed_request_->GetFinalURL().SchemeIs(url::kHttpsScheme);
    const network::mojom::URLResponseHead* response_info =
        pending_seed_request_->ResponseInfo();
    if (response_info && response_info->headers) {
      headers = response_info->headers;
      response_code = headers->response_code();
    }
    net_error = pending_seed_request_->NetError();
    is_success = headers && response_body && (net_error == net::OK);
  }

  pending_seed_request_.reset();
  if (last_request_was_http_retry_) {
    base::UmaHistogramSparse("Variations.SeedFetchResponseOrErrorCode.HTTP",
                             is_success ? response_code : net_error);
  } else {
    base::UmaHistogramSparse("Variations.SeedFetchResponseOrErrorCode",
                             is_success ? response_code : net_error);
  }
  if (!is_success) {
    DVLOG(1) << "Variations server request failed with error: " << net_error
             << ": " << net::ErrorToString(net_error);
    // It's common for the very first fetch attempt to fail (e.g. the network
    // may not yet be available). In such a case, try again soon, rather than
    // waiting the full time interval.
    // |request_scheduler_| will be null during unit tests.
    if (is_first_request && request_scheduler_) {
      request_scheduler_->ScheduleFetchShortly();
      return;
    }

    if (MaybeRetryOverHTTP()) {
      // If the retry was successfully started, return immediately,
      // OnSimpleLoaderComplete will be called again when the new fetch
      // finishes.
      return;
    }
  }

  // Return if there was a failure. Note that we check both |is_success| which
  // is set above and the response code. There could be a case where there's a
  // HTTP_OK response code but |is_success| is false, for example if the fetch
  // download was interrupted after having been started.
  if (!is_success || (response_code != net::HTTP_OK &&
                      response_code != net::HTTP_NOT_MODIFIED)) {
    DVLOG(1) << "Variations server request failed: is_success=" << is_success
             << " response_code=" << response_code
             << " net_error=" << net_error;
    return;
  }
  // At this point, |headers| and |response_body| should exist.
  DCHECK(headers);
  DCHECK(response_body);

  base::Time response_date;
  if (headers->GetDateValue(&response_date)) {
    DCHECK(!response_date.is_null());

    const base::TimeTicks now = base::TimeTicks::Now();
    const base::TimeDelta latency = now - last_request_started_time_;
    client_->GetNetworkTimeTracker()->UpdateNetworkTime(
        response_date, kServerTimeResolution, latency, now);
  }

  if (response_code == net::HTTP_NOT_MODIFIED) {
    RecordSuccessfulFetch();

    // Update the seed date value in local state (used for expiry check on
    // next start up), since 304 is a successful response. Note that the
    // serial number included in the request is always that of the latest
    // seed, even when running in safe mode, so it's appropriate to always
    // modify the latest seed's date.
    field_trial_creator_.seed_store()->UpdateSeedDateAndLogDayChange(
        response_date);
    return;
  }

  // We're now handling the HTTP_OK success case.
  DCHECK_EQ(response_code, net::HTTP_OK);

  bool is_delta_compressed;
  bool is_gzip_compressed;
  if (!GetInstanceManipulations(headers.get(), &is_delta_compressed,
                                &is_gzip_compressed)) {
    // The header does not specify supported instance manipulations, unable to
    // process data. Details of errors were logged by GetInstanceManipulations.
    field_trial_creator_.seed_store()->ReportUnsupportedSeedFormatError();
    return;
  }

  const std::string signature =
      GetHeaderValue(headers.get(), "X-Seed-Signature");
  const std::string country_code = GetHeaderValue(headers.get(), "X-Country");
  const bool store_success =
      StoreSeed(*response_body, signature, country_code, response_date,
                is_delta_compressed, is_gzip_compressed, !final_url_was_https);
  if (!store_success && is_delta_compressed) {
    disable_deltas_for_next_request_ = true;
    // |request_scheduler_| will be null during unit tests.
    if (request_scheduler_)
      request_scheduler_->ScheduleFetchShortly();
  }
}

bool VariationsService::MaybeRetryOverHTTP() {
  // If the current fetch attempt was over an HTTPS connection, retry the
  // fetch immediately over an HTTP connection. We only do this if an insecure
  // variations URL is set and its scheme is HTTP.
  if (!last_request_was_http_retry_ &&
      !insecure_variations_server_url_.is_empty() &&
      insecure_variations_server_url_.SchemeIs(url::kHttpScheme)) {
    return DoFetchFromURL(insecure_variations_server_url_, true);
  }
  return false;
}

void VariationsService::OnResourceRequestsAllowed() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Note that this only attempts to fetch the seed at most once per period
  // (kSeedFetchPeriodHours). This works because
  // |resource_request_allowed_notifier_| only calls this method if an
  // attempt was made earlier that fails (which implies that the period had
  // elapsed). After a successful attempt is made, the notifier will know not
  // to call this method again until another failed attempt occurs.
  RecordRequestsAllowedHistogram(RESOURCE_REQUESTS_ALLOWED_NOTIFIED);
  DVLOG(1) << "Retrying fetch.";
  DoActualFetch();

  // This service must have created a scheduler in order for this to be called.
  DCHECK(request_scheduler_);
  request_scheduler_->Reset();
}

void VariationsService::PerformSimulationWithVersion(
    std::unique_ptr<VariationsSeed> seed,
    const base::Version& version) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!version.IsValid())
    return;

  const base::ElapsedTimer timer;

  std::unique_ptr<const base::FieldTrial::EntropyProvider> default_provider =
      state_manager_->CreateDefaultEntropyProvider();
  std::unique_ptr<const base::FieldTrial::EntropyProvider> low_provider =
      state_manager_->CreateLowEntropyProvider();
  VariationsSeedSimulator seed_simulator(*default_provider, *low_provider);

  std::unique_ptr<ClientFilterableState> client_state =
      field_trial_creator_.GetClientFilterableStateForVersion(version);
  const VariationsSeedSimulator::Result result =
      seed_simulator.SimulateSeedStudies(*seed, *client_state);

  UMA_HISTOGRAM_COUNTS_100("Variations.SimulateSeed.NormalChanges",
                           result.normal_group_change_count);
  UMA_HISTOGRAM_COUNTS_100("Variations.SimulateSeed.KillBestEffortChanges",
                           result.kill_best_effort_group_change_count);
  UMA_HISTOGRAM_COUNTS_100("Variations.SimulateSeed.KillCriticalChanges",
                           result.kill_critical_group_change_count);

  UMA_HISTOGRAM_TIMES("Variations.SimulateSeed.Duration", timer.Elapsed());

  NotifyObservers(result);
}

bool VariationsService::CallMaybeRetryOverHTTPForTesting() {
  return MaybeRetryOverHTTP();
}

void VariationsService::RecordSuccessfulFetch() {
  field_trial_creator_.seed_store()->RecordLastFetchTime(base::Time::Now());
  safe_seed_manager_.RecordSuccessfulFetch(field_trial_creator_.seed_store());
}

void VariationsService::GetClientFilterableStateForVersionCalledForTesting() {
  const base::Version current_version(version_info::GetVersionNumber());
  if (!current_version.IsValid())
    return;

  field_trial_creator_.GetClientFilterableStateForVersion(current_version);
}

std::string VariationsService::GetLatestCountry() const {
  return field_trial_creator_.GetLatestCountry();
}

bool VariationsService::SetupFieldTrials(
    const char* kEnableGpuBenchmarking,
    const char* kEnableFeatures,
    const char* kDisableFeatures,
    const std::set<std::string>& unforceable_field_trials,
    const std::vector<std::string>& variation_ids,
    const std::vector<base::FeatureList::FeatureOverrideInfo>& extra_overrides,
    std::unique_ptr<base::FeatureList> feature_list,
    variations::PlatformFieldTrials* platform_field_trials) {
  return field_trial_creator_.SetupFieldTrials(
      kEnableGpuBenchmarking, kEnableFeatures, kDisableFeatures,
      unforceable_field_trials, variation_ids, extra_overrides,
      CreateLowEntropyProvider(), std::move(feature_list),
      platform_field_trials, &safe_seed_manager_);
}

void VariationsService::OverrideCachedUIStrings() {
  field_trial_creator_.OverrideCachedUIStrings();
}

void VariationsService::CancelCurrentRequestForTesting() {
  pending_seed_request_.reset();
}

void VariationsService::StartRepeatedVariationsSeedFetchForTesting() {
  InitResourceRequestedAllowedNotifier();
  return StartRepeatedVariationsSeedFetch();
}

std::string VariationsService::GetOverriddenPermanentCountry() {
  return local_state_->GetString(prefs::kVariationsPermanentOverriddenCountry);
}

std::string VariationsService::GetStoredPermanentCountry() {
  const std::string variations_overridden_country =
      GetOverriddenPermanentCountry();
  if (!variations_overridden_country.empty())
    return variations_overridden_country;

  const base::ListValue* list_value =
      local_state_->GetList(prefs::kVariationsPermanentConsistencyCountry);
  std::string stored_country;

  if (list_value->GetSize() == 2) {
    list_value->GetString(1, &stored_country);
  }

  return stored_country;
}

bool VariationsService::OverrideStoredPermanentCountry(
    const std::string& country_override) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const std::string stored_country =
      local_state_->GetString(prefs::kVariationsPermanentOverriddenCountry);

  if (stored_country == country_override)
    return false;

  field_trial_creator_.StoreVariationsOverriddenCountry(country_override);
  return true;
}

}  // namespace variations
