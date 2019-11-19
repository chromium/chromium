// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_service_client.h"

#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/json/json_writer.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/system/sys_info.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_mutable_config_values.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_request_options.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_service.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_util.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_features.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_server.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "components/data_reduction_proxy/proto/client_config.pb.h"
#include "components/data_use_measurement/core/data_use_user_data.h"
#include "components/previews/core/previews_experiments.h"
#include "components/variations/net/variations_http_headers.h"
#include "net/base/host_port_pair.h"
#include "net/base/load_flags.h"
#include "net/base/load_timing_info.h"
#include "net/base/net_errors.h"
#include "net/base/proxy_server.h"
#include "net/http/http_network_session.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

#if defined(OS_ANDROID)
#include "net/android/network_library.h"
#endif

namespace data_reduction_proxy {

namespace {

// Key of the UMA DataReductionProxy.ConfigService.FetchResponseCode histogram.
const char kUMAConfigServiceFetchResponseCode[] =
    "DataReductionProxy.ConfigService.FetchResponseCode";

// Key of the UMA
// DataReductionProxy.ConfigService.FetchFailedAttemptsBeforeSuccess histogram.
const char kUMAConfigServiceFetchFailedAttemptsBeforeSuccess[] =
    "DataReductionProxy.ConfigService.FetchFailedAttemptsBeforeSuccess";

// Key of the UMA DataReductionProxy.ConfigService.FetchLatency histogram.
const char kUMAConfigServiceFetchLatency[] =
    "DataReductionProxy.ConfigService.FetchLatency";

// Key of the UMA DataReductionProxy.ConfigService.AuthExpired histogram.
const char kUMAConfigServiceAuthExpired[] =
    "DataReductionProxy.ConfigService.AuthExpired";

#if defined(OS_ANDROID)
// Maximum duration  to wait before fetching the config, while the application
// is in background.
const uint32_t kMaxBackgroundFetchIntervalSeconds = 6 * 60 * 60;  // 6 hours.
#endif

// This is the default backoff policy used to communicate with the Data
// Reduction Proxy configuration service. The policy gets overwritten based on
// kDataReductionProxyAggressiveConfigFetch.
const net::BackoffEntry::Policy kDefaultBackoffPolicy = {
    0,                // num_errors_to_ignore
    10 * 1000,        // initial_delay_ms
    3,                // multiply_factor
    0.25,             // jitter_factor,
    128 * 60 * 1000,  // maximum_backoff_ms
    -1,               // entry_lifetime_ms
    true,             // always_use_initial_delay
};

// Extracts the list of Data Reduction Proxy servers to use for HTTP requests.
std::vector<DataReductionProxyServer> GetProxiesForHTTP(
    const data_reduction_proxy::ProxyConfig& proxy_config) {
  std::vector<DataReductionProxyServer> proxies;
  for (const auto& server : proxy_config.http_proxy_servers()) {
    if (server.scheme() != ProxyServer_ProxyScheme_UNSPECIFIED) {
      proxies.push_back(DataReductionProxyServer(net::ProxyServer(
          protobuf_parser::SchemeFromProxyScheme(server.scheme()),
          net::HostPortPair(server.host(), server.port()),
          /* HTTPS proxies are marked as trusted. */
          server.scheme() == ProxyServer_ProxyScheme_HTTPS)));
    }
  }

  return proxies;
}

void RecordAuthExpiredHistogram(bool auth_expired) {
  UMA_HISTOGRAM_BOOLEAN(kUMAConfigServiceAuthExpired, auth_expired);
}

// Records whether the session key used in the request matches the current
// sesssion key.
void RecordAuthExpiredSessionKey(bool matches) {
  // This enum must remain synchronized with the
  // DataReductionProxyConfigServiceAuthExpiredSessionKey enum in
  // metrics/histograms/histograms.xml.
  enum AuthExpiredSessionKey {
    AUTH_EXPIRED_SESSION_KEY_MISMATCH = 0,
    AUTH_EXPIRED_SESSION_KEY_MATCH = 1,
    AUTH_EXPIRED_SESSION_KEY_BOUNDARY = 2
  };

  AuthExpiredSessionKey state = matches ? AUTH_EXPIRED_SESSION_KEY_MATCH
                                        : AUTH_EXPIRED_SESSION_KEY_MISMATCH;

  UMA_HISTOGRAM_ENUMERATION(
      "DataReductionProxy.ConfigService.AuthExpiredSessionKey", state,
      AUTH_EXPIRED_SESSION_KEY_BOUNDARY);
}

}  // namespace

net::BackoffEntry::Policy GetBackoffPolicy() {
  net::BackoffEntry::Policy policy = kDefaultBackoffPolicy;
  if (base::FeatureList::IsEnabled(
          features::kDataReductionProxyAggressiveConfigFetch)) {
    // Disabling always_use_initial_delay allows no backoffs until
    // num_errors_to_ignore failures have occurred.
    policy.num_errors_to_ignore = 2;
    policy.always_use_initial_delay = false;
  }
  return policy;
}

DataReductionProxyConfigServiceClient::DataReductionProxyConfigServiceClient(
    const net::BackoffEntry::Policy& backoff_policy,
    DataReductionProxyRequestOptions* request_options,
    DataReductionProxyMutableConfigValues* config_values,
    DataReductionProxyConfig* config,
    DataReductionProxyService* service,
    network::NetworkConnectionTracker* network_connection_tracker,
    ConfigStorer config_storer)
    : request_options_(request_options),
      config_values_(config_values),
      config_(config),
      service_(service),
      network_connection_tracker_(network_connection_tracker),
      config_storer_(config_storer),
      backoff_policy_(backoff_policy),
      backoff_entry_(&backoff_policy_),
      config_service_url_(util::AddApiKeyToUrl(params::GetConfigServiceURL())),
      enabled_(false),
      remote_config_applied_(false),
#if defined(OS_ANDROID)
      foreground_fetch_pending_(false),
#endif
      previous_request_failed_authentication_(false),
      failed_attempts_before_success_(0),
      fetch_in_progress_(false),
      client_config_override_used_(false) {
  DCHECK(request_options);
  DCHECK(config_values);
  DCHECK(config);
  DCHECK(service);
  DCHECK(config_service_url_.is_valid());
  DCHECK(!params::IsIncludedInHoldbackFieldTrial() ||
         previews::params::IsLitePageServerPreviewsEnabled() ||
         params::ForceEnableClientConfigServiceForAllDataSaverUsers());

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  client_config_override_ = command_line.GetSwitchValueASCII(
      switches::kDataReductionProxyServerClientConfig);

  // Constructed on the UI thread, but should be checked on the IO thread.
  thread_checker_.DetachFromThread();
}

DataReductionProxyConfigServiceClient::
    ~DataReductionProxyConfigServiceClient() {
  network_connection_tracker_->RemoveNetworkConnectionObserver(this);
}

base::TimeDelta
DataReductionProxyConfigServiceClient::CalculateNextConfigRefreshTime(
    bool fetch_succeeded,
    const base::TimeDelta& config_expiration_delta,
    const base::TimeDelta& backoff_delay) {
  DCHECK(backoff_delay >= base::TimeDelta());

#if defined(OS_ANDROID)
  foreground_fetch_pending_ = false;
  if (!fetch_succeeded &&
      !base::FeatureList::IsEnabled(
          features::kDataReductionProxyAggressiveConfigFetch) &&
      IsApplicationStateBackground()) {
    // If Chromium is in background, then fetch the config when Chromium comes
    // to foreground or after max of |kMaxBackgroundFetchIntervalSeconds| and
    // |backoff_delay|.
    foreground_fetch_pending_ = true;
    return std::max(backoff_delay, base::TimeDelta::FromSeconds(
                                       kMaxBackgroundFetchIntervalSeconds));
  }
#endif

  if (fetch_succeeded) {
    return std::max(backoff_delay, config_expiration_delta);
  }

  return backoff_delay;
}

void DataReductionProxyConfigServiceClient::Initialize(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  DCHECK(url_loader_factory);
#if defined(OS_ANDROID)
  // It is okay to use Unretained here because |app_status_listener| would be
  // destroyed before |this|.
  app_status_listener_ =
      base::android::ApplicationStatusListener::New(base::BindRepeating(
          &DataReductionProxyConfigServiceClient::OnApplicationStateChange,
          base::Unretained(this)));
#endif
  url_loader_factory_ = std::move(url_loader_factory);
  network_connection_tracker_->AddNetworkConnectionObserver(this);
}

void DataReductionProxyConfigServiceClient::SetEnabled(bool enabled) {
  DCHECK(thread_checker_.CalledOnValidThread());
  enabled_ = enabled;
}

void DataReductionProxyConfigServiceClient::InvalidateAndRetrieveNewConfig() {
  DCHECK(thread_checker_.CalledOnValidThread());

  InvalidateConfig();
  DCHECK(config_->GetProxiesForHttp().empty());

  if (fetch_in_progress_) {
    // If a client config fetch is already in progress, then do not start
    // another fetch since starting a new fetch will cause extra data
    // usage, and also cancel the ongoing fetch.
    return;
  }

  RetrieveConfig();
}

void DataReductionProxyConfigServiceClient::RetrieveConfig() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!enabled_)
    return;

  if (!client_config_override_.empty()) {
    // Return fast if the override has already been attempted.
    if (client_config_override_used_) {
      return;
    }
    // Set this flag so that we only attempt to apply the given config once. If
    // there are parse errors, the DCHECKs will catch them in a debug build.
    client_config_override_used_ = true;

    std::string override_config;
    bool b64_decode_ok =
        base::Base64Decode(client_config_override_, &override_config);
    LOG_IF(DFATAL, !b64_decode_ok)
        << "The given ClientConfig is not valid base64";

    ClientConfig config;
    bool was_valid_config = config.ParseFromString(override_config);
    LOG_IF(DFATAL, !was_valid_config) << "The given ClientConfig was invalid.";
    if (was_valid_config)
      ParseAndApplyProxyConfig(config);
    return;
  }

  config_fetch_start_time_ = base::TimeTicks::Now();

  RetrieveRemoteConfig();
}

bool DataReductionProxyConfigServiceClient::RemoteConfigApplied() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return remote_config_applied_;
}

void DataReductionProxyConfigServiceClient::ApplySerializedConfig(
    const std::string& config_value) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (RemoteConfigApplied())
    return;

  if (!client_config_override_.empty())
    return;

  std::string decoded_config;
  if (base::Base64Decode(config_value, &decoded_config)) {
    ClientConfig config;
    if (config.ParseFromString(decoded_config))
      ParseAndApplyProxyConfig(config);
  }
}

bool DataReductionProxyConfigServiceClient::ShouldRetryDueToAuthFailure(
    const net::HttpRequestHeaders& request_headers,
    const net::HttpResponseHeaders* response_headers,
    const net::ProxyServer& proxy_server,
    const net::LoadTimingInfo& load_timing_info) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(response_headers);

  if (!config_->FindConfiguredDataReductionProxy(proxy_server))
    return false;

  if (response_headers->response_code() !=
      net::HTTP_PROXY_AUTHENTICATION_REQUIRED) {
    previous_request_failed_authentication_ = false;
    return false;
  }

  // If the session key used in the request is different from the current
  // session key, then the current session key does not need to be
  // invalidated.
  base::Optional<std::string> session_key =
      request_options_->GetSessionKeyFromRequestHeaders(request_headers);
  if ((session_key.has_value() ? session_key.value() : std::string()) !=
      request_options_->GetSecureSession()) {
    RecordAuthExpiredSessionKey(false);
    return true;
  }
  RecordAuthExpiredSessionKey(true);

  // The default backoff logic is to increment the failure count (and
  // increase the backoff time) with each response failure to the remote
  // config service, and to decrement the failure count (and decrease the
  // backoff time) with each response success. In the case where the
  // config service returns a success response (decrementing the failure
  // count) but the session key is continually invalid (as a response from
  // the Data Reduction Proxy and not the config service), the previous
  // response should be considered a failure in order to ensure the backoff
  // time continues to increase.
  if (previous_request_failed_authentication_)
    GetBackoffEntry()->InformOfRequest(false);

  // Record that a request resulted in an authentication failure.
  RecordAuthExpiredHistogram(true);
  previous_request_failed_authentication_ = true;
  InvalidateConfig();
  DCHECK(config_->GetProxiesForHttp().empty());

  if (fetch_in_progress_) {
    // If a client config fetch is already in progress, then do not start
    // another fetch since starting a new fetch will cause extra data
    // usage, and also cancel the ongoing fetch.
    return true;
  }

  RetrieveConfig();

  if (!load_timing_info.send_start.is_null() &&
      !load_timing_info.request_start.is_null() &&
      !network_connection_tracker_->IsOffline() &&
      last_ip_address_change_ < load_timing_info.request_start) {
    // Record only if there was no change in the IP address since the
    // request started.
    UMA_HISTOGRAM_TIMES(
        "DataReductionProxy.ConfigService.AuthFailure.LatencyPenalty",
        base::TimeTicks::Now() - load_timing_info.request_start);
  }

  return true;
}

net::BackoffEntry* DataReductionProxyConfigServiceClient::GetBackoffEntry() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return &backoff_entry_;
}

void DataReductionProxyConfigServiceClient::SetConfigRefreshTimer(
    const base::TimeDelta& delay) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(delay >= base::TimeDelta());
  config_refresh_timer_.Stop();
  config_refresh_timer_.Start(
      FROM_HERE, delay, this,
      &DataReductionProxyConfigServiceClient::RetrieveConfig);
}

base::Time DataReductionProxyConfigServiceClient::Now() {
  return base::Time::Now();
}

void DataReductionProxyConfigServiceClient::OnConnectionChanged(
    network::mojom::ConnectionType type) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (type == network::mojom::ConnectionType::CONNECTION_NONE)
    return;

  GetBackoffEntry()->Reset();
  last_ip_address_change_ = base::TimeTicks::Now();
  failed_attempts_before_success_ = 0;
  RetrieveConfig();
}

void DataReductionProxyConfigServiceClient::OnURLLoadComplete(
    std::unique_ptr<std::string> response_body) {
  DCHECK(thread_checker_.CalledOnValidThread());
  fetch_in_progress_ = false;

  int response_code = -1;
  if (url_loader_->ResponseInfo() && url_loader_->ResponseInfo()->headers) {
    response_code = url_loader_->ResponseInfo()->headers->response_code();
  }
  HandleResponse(response_body ? *response_body : "", url_loader_->NetError(),
                 response_code);
}

void DataReductionProxyConfigServiceClient::RetrieveRemoteConfig() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!params::IsIncludedInHoldbackFieldTrial() ||
         previews::params::IsLitePageServerPreviewsEnabled() ||
         params::ForceEnableClientConfigServiceForAllDataSaverUsers());

  CreateClientConfigRequest request;
  std::string serialized_request;
#if defined(OS_ANDROID)
  request.set_telephony_network_operator(
      net::android::GetTelephonyNetworkOperator());
#endif

  data_reduction_proxy::ConfigDeviceInfo* device_info =
      request.mutable_device_info();
  device_info->set_total_device_memory_kb(
      base::SysInfo::AmountOfPhysicalMemory() / 1024);
  const std::string& session_key = request_options_->GetSecureSession();
  if (!session_key.empty())
    request.set_session_key(request_options_->GetSecureSession());
  request.set_dogfood_group(
      base::FeatureList::IsEnabled(features::kDogfood)
          ? CreateClientConfigRequest_DogfoodGroup_DOGFOOD
          : CreateClientConfigRequest_DogfoodGroup_NONDOGFOOD);
  data_reduction_proxy::VersionInfo* version_info =
      request.mutable_version_info();
  uint32_t build;
  uint32_t patch;
  util::GetChromiumBuildAndPatchAsInts(util::ChromiumVersion(), &build, &patch);
  version_info->set_client(util::GetStringForClient(service_->client()));
  version_info->set_build(build);
  version_info->set_patch(patch);
  version_info->set_channel(service_->channel());
  request.SerializeToString(&serialized_request);

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("data_reduction_proxy_config", R"(
        semantics {
          sender: "Data Reduction Proxy"
          description:
            "Requests a configuration that specifies how to connect to the "
            "data reduction proxy."
          trigger:
            "Requested when Data Saver is enabled and the browser does not "
            "have a configuration that is not older than a threshold set by "
            "the server."
          data: "None."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "Users can control Data Saver on Android via 'Data Saver' setting. "
            "Data Saver is not available on iOS, and on desktop it is enabled "
            "by insalling the Data Saver extension."
          policy_exception_justification: "Not implemented."
        })");
  fetch_in_progress_ = true;

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = config_service_url_;
  resource_request->method = "POST";
  resource_request->load_flags = net::LOAD_BYPASS_PROXY;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  // Attach variations headers.
  url_loader_ = variations::CreateSimpleURLLoaderWithVariationsHeader(
      std::move(resource_request), variations::InIncognito::kNo,
      variations::SignedIn::kNo, traffic_annotation);

  url_loader_->AttachStringForUpload(serialized_request,
                                     "application/x-protobuf");
  // |url_loader_| should not retry on 5xx errors since the server may already
  // be overloaded. Spurious 5xx errors are still retried on exponential
  // backoff. |url_loader_| should retry on network changes since the network
  // stack may receive the connection change event later than |this|.
  static const int kMaxRetries = 5;
  url_loader_->SetRetryOptions(
      kMaxRetries, network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE);
  url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&DataReductionProxyConfigServiceClient::OnURLLoadComplete,
                     base::Unretained(this)));
}

void DataReductionProxyConfigServiceClient::InvalidateConfig() {
  DCHECK(thread_checker_.CalledOnValidThread());
  GetBackoffEntry()->InformOfRequest(false);
  config_storer_.Run(std::string());
  request_options_->Invalidate();
  config_values_->Invalidate();
  config_->OnNewClientConfigFetched();
}

void DataReductionProxyConfigServiceClient::HandleResponse(
    const std::string& config_data,
    int status,
    int response_code) {
  DCHECK(thread_checker_.CalledOnValidThread());
  ClientConfig config;
  bool succeeded = false;

  if (!network_connection_tracker_->IsOffline())
    base::UmaHistogramSparse(kUMAConfigServiceFetchResponseCode, response_code);

  if (status == net::OK && response_code == net::HTTP_OK &&
      config.ParseFromString(config_data)) {
    succeeded = ParseAndApplyProxyConfig(config);
  }

  // These are proxies listed in the config. The proxies that client eventually
  // ends up using depend on the field trials.
  std::vector<DataReductionProxyServer> proxies;
  base::TimeDelta refresh_duration;
  if (succeeded) {
    proxies = GetProxiesForHTTP(config.proxy_config());
    refresh_duration =
        protobuf_parser::DurationToTimeDelta(config.refresh_duration());

    DCHECK(!config_fetch_start_time_.is_null());
    base::TimeDelta configuration_fetch_latency =
        base::TimeTicks::Now() - config_fetch_start_time_;
    RecordAuthExpiredHistogram(false);
    UMA_HISTOGRAM_MEDIUM_TIMES(kUMAConfigServiceFetchLatency,
                               configuration_fetch_latency);
    UMA_HISTOGRAM_COUNTS_100(kUMAConfigServiceFetchFailedAttemptsBeforeSuccess,
                             failed_attempts_before_success_);
    failed_attempts_before_success_ = 0;
    std::string encoded_config;
    base::Base64Encode(config_data, &encoded_config);
    config_storer_.Run(encoded_config);

    // Record timing metrics on successful requests only.
    const network::mojom::URLResponseHead* info = url_loader_->ResponseInfo();
    base::TimeDelta http_request_rtt =
        info->response_start - info->request_start;
    UMA_HISTOGRAM_TIMES("DataReductionProxy.ConfigService.HttpRequestRTT",
                        http_request_rtt);

    if (info->load_timing.connect_timing.connect_end > base::TimeTicks() &&
        info->load_timing.connect_timing.connect_start > base::TimeTicks()) {
      base::TimeDelta connection_setup =
          info->load_timing.connect_timing.connect_end -
          info->load_timing.connect_timing.connect_start;
      UMA_HISTOGRAM_TIMES(
          "DataReductionProxy.ConfigService.ConnectionSetupTime",
          connection_setup);
    }
  } else {
    ++failed_attempts_before_success_;
  }

  GetBackoffEntry()->InformOfRequest(succeeded);
  base::TimeDelta next_config_refresh_time = CalculateNextConfigRefreshTime(
      succeeded, refresh_duration, GetBackoffEntry()->GetTimeUntilRelease());

  SetConfigRefreshTimer(next_config_refresh_time);
}

bool DataReductionProxyConfigServiceClient::ParseAndApplyProxyConfig(
    const ClientConfig& config) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!config.has_proxy_config())
    return false;

  service_->SetIgnoreLongTermBlackListRules(
      config.ignore_long_term_black_list_rules());

  // An empty proxy config is OK, and allows the server to effectively turn off
  // DataSaver if needed. See http://crbug.com/840978.
  std::vector<DataReductionProxyServer> proxies =
      GetProxiesForHTTP(config.proxy_config());

  request_options_->SetSecureSession(config.session_key());
  config_values_->UpdateValues(proxies);
  config_->OnNewClientConfigFetched();
  remote_config_applied_ = true;
  return true;
}

#if defined(OS_ANDROID)
bool DataReductionProxyConfigServiceClient::IsApplicationStateBackground()
    const {
  return base::android::ApplicationStatusListener::GetState() !=
         base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES;
}

void DataReductionProxyConfigServiceClient::OnApplicationStateChange(
    base::android::ApplicationState new_state) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (new_state == base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES &&
      foreground_fetch_pending_) {
    foreground_fetch_pending_ = false;
    RetrieveConfig();
  }
}
#endif

}  // namespace data_reduction_proxy
