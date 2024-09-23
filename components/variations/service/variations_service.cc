// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/service/variations_service.h"

#include <stddef.h>
#include <stdint.h>

#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/observer_list.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "base/version.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/encrypted_messages/encrypted_message.pb.h"
#include "components/encrypted_messages/message_encrypter.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/network_time/network_time_tracker.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/variations/field_trial_internals_utils.h"
#include "components/variations/pref_names.h"
#include "components/variations/proto/variations_seed.pb.h"
#include "components/variations/seed_response.h"
#include "components/variations/service/limited_entropy_synthetic_trial.h"
#include "components/variations/synthetic_trial_registry.h"
#include "components/variations/variations_safe_seed_store_local_state.h"
#include "components/variations/variations_seed_simulator.h"
#include "components/variations/variations_switches.h"
#include "components/variations/variations_url_constants.h"
#include "components/version_info/channel.h"
#include "components/version_info/version_info.h"
#include "net/base/net_errors.h"
#include "net/base/url_util.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

namespace variations {
namespace {

// Constants used for encrypting the if-none-match header if we are retrieving a
// seed over http.
const char kEncryptedMessageLabel[] = "chrome variations";

// TODO(crbug.com/41359527): Change this key to a unique VariationsService one,
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
const uint32_t kServerTimeResolutionInSeconds = 1;

// Whether the VariationsService should fetch the seed for testing.
bool g_should_fetch_for_testing = false;

// Returns a string that will be used for the value of the 'osname' URL param
// to the variations server.
std::string GetPlatformString() {
#if BUILDFLAG(IS_WIN)
  return "win";
#elif BUILDFLAG(IS_IOS)
  return "ios";
#elif BUILDFLAG(IS_MAC)
  return "mac";
#elif BUILDFLAG(IS_CHROMEOS_ASH)
  return "chromeos";
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  return "chromeos_lacros";
#elif BUILDFLAG(IS_ANDROID)
  return "android";
#elif BUILDFLAG(IS_FUCHSIA)
  return "fuchsia";
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_BSD) || BUILDFLAG(IS_SOLARIS)
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
  NOTREACHED_IN_MIGRATION();
  return RESOURCE_REQUESTS_NOT_ALLOWED;
}

// Returns the header value for |name| from |headers| or an empty string if not
// set.
std::string GetHeaderValue(const net::HttpResponseHeaders* headers,
                           std::string_view name) {
  std::string value;
  headers->EnumerateHeader(nullptr, name, &value);
  return value;
}

// Returns the list of values for |name| from |headers|. If the header in not
// set, return an empty list.
std::vector<std::string> GetHeaderValuesList(
    const net::HttpResponseHeaders* headers,
    std::string_view name) {
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
  const auto delta_im = base::ranges::find(ims, "x-bm");
  const auto gzip_im = base::ranges::find(ims, "gzip");
  *is_delta_compressed = delta_im != ims.end();
  *is_gzip_compressed = gzip_im != ims.end();

  // The IM field should not have anything but x-bm and gzip.
  size_t im_count =
      (*is_delta_compressed ? 1 : 0) + (*is_gzip_compressed ? 1 : 0);
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

// Returns the already downloaded first run seed, and clear the seed from the
// native-side prefs. At this point, the seed has already been fetched from the
// native seed storage, so it's no longer needed there. This is done regardless
// if we fail or succeed below - since if we succeed, we're good to go and if we
// fail, we probably don't want to keep around the bad content anyway.
std::unique_ptr<SeedResponse> MaybeImportFirstRunSeed(
    VariationsServiceClient* client,
    PrefService* local_state) {
  if (!local_state->HasPrefPath(prefs::kVariationsSeedSignature)) {
    DVLOG(1) << "Importing first run seed from native preferences.";
    return client->TakeSeedFromNativeVariationsSeedStore();
  }
  return nullptr;
}

}  // namespace

#if BUILDFLAG(IS_CHROMEOS_ASH)
// This is a utility which syncs the policy-managed value of
// |prefs::kDeviceVariationsRestrictionsByPolicy| into
// |prefs::kVariationsRestrictionsByPolicy|.
// TODO(crbug.com/40121933): Remove this workaround and implement a better long
// term solution.
class DeviceVariationsRestrictionByPolicyApplicator {
 public:
  DeviceVariationsRestrictionByPolicyApplicator(
      PrefService* policy_pref_service)
      : policy_pref_service_(policy_pref_service) {
    DCHECK(policy_pref_service_);
    const PrefService::PrefInitializationStatus prefs_init_status =
        policy_pref_service_->GetAllPrefStoresInitializationStatus();
    if (prefs_init_status == PrefService::INITIALIZATION_STATUS_WAITING) {
      policy_pref_service_->AddPrefInitObserver(
          base::BindOnce(&DeviceVariationsRestrictionByPolicyApplicator::
                             OnPolicyPrefServiceInitialized,
                         weak_ptr_factory_.GetWeakPtr()));
      return;
    }
    OnPolicyPrefServiceInitialized(prefs_init_status ==
                                   PrefService::INITIALIZATION_STATUS_SUCCESS);
  }

  ~DeviceVariationsRestrictionByPolicyApplicator() = default;

  DeviceVariationsRestrictionByPolicyApplicator(
      const DeviceVariationsRestrictionByPolicyApplicator& other) = delete;
  DeviceVariationsRestrictionByPolicyApplicator& operator=(
      const DeviceVariationsRestrictionByPolicyApplicator& other) = delete;

 private:
  void OnPolicyPrefServiceInitialized(bool successful) {
    // If PrefService initialization was not successful, another component will
    // display an error message to the user.
    if (!successful)
      return;

    pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
    pref_change_registrar_->Init(policy_pref_service_);
    pref_change_registrar_->Add(
        prefs::kDeviceVariationsRestrictionsByPolicy,
        base::BindRepeating(&DeviceVariationsRestrictionByPolicyApplicator::
                                OnDevicePolicyChange,
                            weak_ptr_factory_.GetWeakPtr()));
    // Also process the initial value.
    OnDevicePolicyChange();
  }

  // Observes the changes in prefs::kDeviceVariationsRestrictionsByPolicy,
  // and saves and retrieve its local state value, then sets
  // prefs::kVariationsRestrictParameter with that new value. That's to
  // reflect the changes of chromeos policy into the user policy.
  // TODO(crbug.com/40121933): Remove that workaround, and make a better long
  // term solution.
  void OnDevicePolicyChange() {
    const std::string& device_policy =
        prefs::kDeviceVariationsRestrictionsByPolicy;
    const std::string& user_policy = prefs::kVariationsRestrictionsByPolicy;

    if (policy_pref_service_->IsManagedPreference(device_policy)) {
      const int device_value = policy_pref_service_->GetInteger(device_policy);
      policy_pref_service_->SetInteger(user_policy, device_value);
    } else {
      policy_pref_service_->ClearPref(user_policy);
    }
  }

  const raw_ptr<PrefService> policy_pref_service_;

  // Watch the changes of the variations prefs.
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  base::WeakPtrFactory<DeviceVariationsRestrictionByPolicyApplicator>
      weak_ptr_factory_{this};
};
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

VariationsService::VariationsService(
    std::unique_ptr<VariationsServiceClient> client,
    std::unique_ptr<web_resource::ResourceRequestAllowedNotifier> notifier,
    PrefService* local_state,
    metrics::MetricsStateManager* state_manager,
    const UIStringOverrider& ui_string_overrider,
    SyntheticTrialRegistry* synthetic_trial_registry)
    : client_(std::move(client)),
      local_state_(local_state),
      synthetic_trial_registry_(synthetic_trial_registry),
      state_manager_(state_manager),
      limited_entropy_synthetic_trial_(
          local_state,
          client_.get()->GetChannelForVariations()),
      policy_pref_service_(local_state),
      resource_request_allowed_notifier_(std::move(notifier)),
      safe_seed_manager_(local_state),
      field_trial_creator_(
          client_.get(),
          std::make_unique<VariationsSeedStore>(
              local_state,
              MaybeImportFirstRunSeed(client_.get(), local_state),
              /*signature_verification_enabled=*/true,
              std::make_unique<VariationsSafeSeedStoreLocalState>(local_state)),
          ui_string_overrider,
          &limited_entropy_synthetic_trial_) {
  DCHECK(client_);
  DCHECK(resource_request_allowed_notifier_);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  device_variations_restrictions_by_policy_applicator_ =
      std::make_unique<DeviceVariationsRestrictionByPolicyApplicator>(
          policy_pref_service_);
#endif
}

VariationsService::~VariationsService() = default;

void VariationsService::PerformPreMainMessageLoopStartup() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(field_trial_creator_.IsOverrideResourceMapEmpty());

  InitResourceRequestedAllowedNotifier();

// Android instead calls OnAppEnterForeground() which then calls
// StartRepeatedVariationsSeedFetch(). This is too early to do it on Android
// because at this point the |restrict_mode_| hasn't been set yet. See also
// the CHECK in SetRestrictMode().
#if !BUILDFLAG(IS_ANDROID)
  if (!IsFetchingEnabled())
    return;

  StartRepeatedVariationsSeedFetch();
#endif  // !BUILDFLAG(IS_ANDROID)
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

bool VariationsService::IsLikelyDogfoodClient() const {
  // The param is typically only set for dogfood clients, though in principle it
  // could be set in other rare contexts as well.
  const std::string restrict_mode = GetRestrictParameterValue(
      restrict_mode_, client_.get(), policy_pref_service_);
  return !restrict_mode.empty();
}

void VariationsService::SetIsLikelyDogfoodClientForTesting(
    bool is_dogfood_client) {
  // Any non-empty value for the `restrict_mode_` is treated as a dogfood client
  // (see above).
  if (is_dogfood_client) {
    restrict_mode_ = "nonempty";
  } else {
    restrict_mode_ = std::string();
  }
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
  server_url = net::AppendOrReplaceQueryParameter(
      server_url, "osname",
      osname_server_param_override_.empty() ? GetPlatformString()
                                            : osname_server_param_override_);

  // Add channel to the request URL.
  version_info::Channel channel = client_->GetChannelForVariations();
  if (channel != version_info::Channel::UNKNOWN) {
    server_url = net::AppendOrReplaceQueryParameter(
        server_url, "channel", version_info::GetChannelString(channel));
  }

  // Add milestone to the request URL.
  const std::string milestone = version_info::GetMajorVersionNumber();
  if (!milestone.empty()) {
    server_url =
        net::AppendOrReplaceQueryParameter(server_url, "milestone", milestone);
  }

  DCHECK(server_url.is_valid());
  return server_url;
}

void VariationsService::EnsureLocaleEquals(const std::string& locale) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Chrome OS may switch language on the fly.
  return;
#else

#if BUILDFLAG(IS_ANDROID)
  // TODO(asvitkine): Speculative early return to silence CHECK failures on
  // Android, see crbug.com/912320.
  if (locale.empty())
    return;
#endif

  // Uses a CHECK rather than a DCHECK to ensure that issues are caught since
  // problems in this area may only appear in the wild due to official builds
  // and end user machines.
  if (locale != field_trial_creator_.application_locale()) {
    // TODO(crbug.com/41430274): Report the two values in crash keys.
    static auto* lhs_key = base::debug::AllocateCrashKeyString(
        "mismatched_locale_lhs", base::debug::CrashKeySize::Size256);
    static auto* rhs_key = base::debug::AllocateCrashKeyString(
        "mismatched_locale_rhs", base::debug::CrashKeySize::Size256);
    base::debug::ScopedCrashKeyString scoped_lhs(lhs_key, locale);
    base::debug::ScopedCrashKeyString scoped_rhs(
        rhs_key, field_trial_creator_.application_locale());
    CHECK_EQ(locale, field_trial_creator_.application_locale());
  }
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
  LimitedEntropySyntheticTrial::RegisterPrefs(registry);
  RegisterFieldTrialInternalsPrefs(*registry);

  registry->RegisterIntegerPref(
      prefs::kDeviceVariationsRestrictionsByPolicy,
      static_cast<int>(RestrictionPolicy::NO_RESTRICTIONS));
  registry->RegisterDictionaryPref(
      prefs::kVariationsGoogleGroups,
      static_cast<int>(RestrictionPolicy::NO_RESTRICTIONS));
  // This preference keeps track of the country code used to filter
  // permanent-consistency studies.
  registry->RegisterListPref(prefs::kVariationsPermanentConsistencyCountry);
  // This preference is used to override the variations country code which is
  // consistent across different chrome version.
  registry->RegisterStringPref(prefs::kVariationsPermanentOverriddenCountry,
                               std::string());
  // This preference keeps track of ChromeVariations enum policy which
  // allows the admin to restrict the set of variations applied.
  registry->RegisterIntegerPref(
      prefs::kVariationsRestrictionsByPolicy,
      static_cast<int>(RestrictionPolicy::NO_RESTRICTIONS));
  // This preference will only be written by the policy service, which will fill
  // it according to a value stored in the User Policy.
  registry->RegisterStringPref(prefs::kVariationsRestrictParameter,
                               std::string());
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
        network_connection_tracker_getter,
    SyntheticTrialRegistry* synthetic_trial_registry) {
  return base::WrapUnique(new VariationsService(
      std::move(client),
      std::make_unique<web_resource::ResourceRequestAllowedNotifier>(
          local_state, disable_network_switch,
          std::move(network_connection_tracker_getter)),
      local_state, state_manager, ui_string_overrider,
      synthetic_trial_registry));
}

// static
void VariationsService::EnableFetchForTesting() {
  g_should_fetch_for_testing = true;
}

void VariationsService::DoActualFetch() {
  DoFetchFromURL(variations_server_url_, false);
}

const std::string& VariationsService::GetLatestSerialNumber() {
  return field_trial_creator_.seed_store()->GetLatestSerialNumber();
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
            "The ChromeVariations policy prevents Variations from applying, "
            "but Google Chrome still downloads Variations from the server "
            "periodically. This way, the downloaded Variations apply "
            "immediately on restart if you unset the ChromeVariations policy."
        })");
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = url;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  std::string serial_number = GetLatestSerialNumber();
  if (!serial_number.empty()) {
    // Get the seed only if its serial number doesn't match what we have.
    // If the fetch is an HTTP retry, encrypt the If-None-Match header.
    if (is_http_retry) {
      if (!EncryptString(serial_number, &serial_number)) {
        return false;
      }
      serial_number = base::Base64Encode(serial_number);
    }
    resource_request->headers.SetHeader("If-None-Match", serial_number);
  }
  const bool enable_deltas =
      !serial_number.empty() && !delta_error_since_last_success_;
  // Tell the server that delta-compressed and gzipped seeds are supported.
  const char* supported_im = enable_deltas ? "x-bm,gzip" : "gzip";
  resource_request->headers.SetHeader("A-IM", supported_im);

  pending_seed_request_ = network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation);
  // Ensure our callback is called even with "304 Not Modified" responses.
  pending_seed_request_->SetAllowHttpErrorResults(true);
  pending_seed_request_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      client_->GetURLLoaderFactory().get(),
      base::BindOnce(&VariationsService::OnSimpleLoaderComplete,
                     weak_ptr_factory_.GetWeakPtr()));

  const base::TimeTicks now = base::TimeTicks::Now();
  base::TimeDelta time_since_last_fetch;
  // Record a time delta of 0 (default value) if there was no previous fetch.
  if (!last_request_started_time_.is_null())
    time_since_last_fetch = now - last_request_started_time_;
  UMA_HISTOGRAM_CUSTOM_COUNTS("Variations.TimeSinceLastFetchAttempt",
                              time_since_last_fetch.InMinutes(), 1,
                              base::Days(7).InMinutes(), 50);
  ++request_count_;
  last_request_started_time_ = now;
  delta_error_since_last_success_ = false;
  return true;
}

void VariationsService::StoreSeed(std::string seed_data,
                                  std::string seed_signature,
                                  std::string country_code,
                                  base::Time date_fetched,
                                  bool is_delta_compressed,
                                  bool is_gzip_compressed) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::OnceCallback<void(bool, VariationsSeed)> done_callback =
      base::BindOnce(&VariationsService::OnSeedStoreResult,
                     weak_ptr_factory_.GetWeakPtr(), is_delta_compressed);
  field_trial_creator_.seed_store()->StoreSeedData(
      std::move(seed_data), std::move(seed_signature), std::move(country_code),
      date_fetched, is_delta_compressed, is_gzip_compressed,
      std::move(done_callback));
}

void VariationsService::OnSeedStoreResult(bool is_delta_compressed,
                                          bool store_success,
                                          VariationsSeed seed) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!store_success && is_delta_compressed) {
    delta_error_since_last_success_ = true;
    // |request_scheduler_| will be null during unit tests.
    if (request_scheduler_)
      request_scheduler_->ScheduleFetchShortly();
  }

  if (store_success) {
    RecordSuccessfulFetch();

    // Now, do simulation to determine if there are any kill-switches that were
    // activated by this seed.
    PerformSimulationWithVersion(seed, client_->GetVersionForSimulation());
  }
}

void VariationsService::InitResourceRequestedAllowedNotifier() {
  // ResourceRequestAllowedNotifier does not install an observer if there is no
  // NetworkChangeNotifier, which results in never being notified of changes to
  // network status.
  resource_request_allowed_notifier_->Init(this, /*leaky=*/false,
                                           /*wait_for_eula=*/false);
}

void VariationsService::StartRepeatedVariationsSeedFetch() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Initialize Variations server URLs.
  variations_server_url_ = GetVariationsServerURL(USE_HTTPS);
  insecure_variations_server_url_ = GetVariationsServerURL(USE_HTTP);

  DCHECK(!request_scheduler_);
  request_scheduler_.reset(VariationsRequestScheduler::Create(
      base::BindRepeating(&VariationsService::FetchVariationsSeed,
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

void VariationsService::NotifyObservers(const SeedSimulationResult& result) {
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
  TRACE_EVENT0("browser", "VariationsService::OnSimpleLoaderComplete");

  const bool is_first_request = !initial_request_completed_;
  initial_request_completed_ = true;

  const base::TimeTicks now = base::TimeTicks::Now();
  if (is_first_request &&
      !local_state_->HasPrefPath(prefs::kVariationsSeedSignature)) {
    base::UmaHistogramTimes("Variations.SeedFetchTimeOnFirstRun",
                            now - last_request_started_time_);
  }

  const network::mojom::URLResponseHead* response_info =
      pending_seed_request_->ResponseInfo();
  const scoped_refptr<net::HttpResponseHeaders> headers =
      response_info ? response_info->headers : nullptr;
  const int response_code = headers ? headers->response_code() : -1;
  const int net_error = pending_seed_request_->NetError();
  const bool is_success = headers && response_body && (net_error == net::OK);

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

  std::optional<base::Time> response_date = headers->GetDateValue();
  // If the seed was fetched securely, opportunistically update the network time
  // tracker with the headers time.
  if (response_date && !last_request_was_http_retry_) {
    DCHECK(!response_date->is_null());

    const base::TimeDelta latency = now - last_request_started_time_;
    client_->GetNetworkTimeTracker()->UpdateNetworkTime(
        response_date.value(), base::Seconds(kServerTimeResolutionInSeconds),
        latency, now);
  }

  if (response_code == net::HTTP_NOT_MODIFIED) {
    RecordSuccessfulFetch();

    // Update the seed date value in local state (used for expiry check on
    // next start up), since 304 is a successful response. Note that the
    // serial number included in the request is always that of the latest
    // seed, even when running in safe mode, so it's appropriate to always
    // modify the latest seed's date.
    field_trial_creator_.seed_store()->UpdateSeedDateAndLogDayChange(
        response_date.value_or(base::Time()));
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
    ReportUnsupportedSeedFormatError();
    return;
  }

  std::string signature = GetHeaderValue(headers.get(), "X-Seed-Signature");
  std::string country_code = GetHeaderValue(headers.get(), "X-Country");
  StoreSeed(std::move(*response_body), std::move(signature),
            std::move(country_code), response_date.value_or(base::Time()),
            is_delta_compressed, is_gzip_compressed);
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
    const VariationsSeed& seed,
    const base::Version& version) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!version.IsValid())
    return;

  auto entropy_providers = state_manager_->CreateEntropyProviders(
      VariationsFieldTrialCreatorBase::
          IsLimitedEntropyRandomizationSourceEnabled(
              client()->GetChannelForVariations(),
              &limited_entropy_synthetic_trial_));

  std::unique_ptr<ClientFilterableState> client_state =
      field_trial_creator_.GetClientFilterableStateForVersion(version);
  auto result = SimulateSeedStudies(seed, *client_state, *entropy_providers);

  NotifyObservers(result);
}

bool VariationsService::CallMaybeRetryOverHTTPForTesting() {
  return MaybeRetryOverHTTP();
}

void VariationsService::RecordSuccessfulFetch() {
  field_trial_creator_.seed_store()->RecordLastFetchTime(base::Time::Now());
  safe_seed_manager_.RecordSuccessfulFetch(field_trial_creator_.seed_store());
}

std::unique_ptr<ClientFilterableState>
VariationsService::GetClientFilterableStateForVersion() {
  const base::Version current_version(version_info::GetVersionNumber());
  DCHECK(current_version.IsValid());
  return field_trial_creator_.GetClientFilterableStateForVersion(
      current_version);
}

std::string VariationsService::GetLatestCountry() const {
  return field_trial_creator_.GetLatestCountry();
}

bool VariationsService::SetUpFieldTrials(
    const std::vector<std::string>& variation_ids,
    const std::string& command_line_variation_ids,
    const std::vector<base::FeatureList::FeatureOverrideInfo>& extra_overrides,
    std::unique_ptr<base::FeatureList> feature_list,
    PlatformFieldTrials* platform_field_trials) {
  ForceTrialsAtStartup(*local_state_);

  return field_trial_creator_.SetUpFieldTrials(
      variation_ids, command_line_variation_ids, extra_overrides,
      std::move(feature_list), state_manager_, synthetic_trial_registry_,
      platform_field_trials, &safe_seed_manager_,
      /*add_entropy_source_to_variations_ids=*/true);
}

std::vector<StudyGroupNames> VariationsService::GetStudiesAvailableToForce() {
  VariationsSeed seed;
  std::string seed_data;
  std::string base64_seed_signature;
  if (!field_trial_creator_.seed_store()->LoadSeed(&seed, &seed_data,
                                                   &base64_seed_signature)) {
    return {};
  }

  // TODO(crbug.com/41492213): chrome://field-trial-internals will not support
  // studies that are constrained to a layer with LIMITED entropy mode before
  // limited entropy randomization fully lands.
  auto entropy_providers = state_manager_->CreateEntropyProviders(
      /*enable_limited_entropy_mode=*/false);
  return variations::GetStudiesAvailableToForce(
      std::move(seed), *entropy_providers,
      *GetClientFilterableStateForVersion());
}

SeedType VariationsService::GetSeedType() const {
  return field_trial_creator_.seed_type();
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

void VariationsService::OverridePlatform(
    Study::Platform platform,
    const std::string& osname_server_param_override) {
  field_trial_creator_.OverrideVariationsPlatform(platform);
  osname_server_param_override_ = osname_server_param_override;
}

std::string VariationsService::GetOverriddenPermanentCountry() const {
  return local_state_->GetString(prefs::kVariationsPermanentOverriddenCountry);
}

std::string VariationsService::GetStoredPermanentCountry() const {
  const std::string variations_overridden_country =
      GetOverriddenPermanentCountry();
  if (!variations_overridden_country.empty())
    return variations_overridden_country;

  const auto& list_value =
      local_state_->GetList(prefs::kVariationsPermanentConsistencyCountry);
  std::string stored_country;

  if (list_value.size() == 2 && list_value[1].is_string()) {
    stored_country = list_value[1].GetString();
  }

  return stored_country;
}

bool VariationsService::OverrideStoredPermanentCountry(
    const std::string& country_override) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const std::string country_override_lowercase =
      base::ToLowerASCII(country_override);
  const std::string stored_country =
      local_state_->GetString(prefs::kVariationsPermanentOverriddenCountry);

  if (stored_country == country_override_lowercase) {
    return false;
  }

  field_trial_creator_.StoreVariationsOverriddenCountry(
      country_override_lowercase);
  return true;
}

}  // namespace variations
