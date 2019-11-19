// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"

#include <algorithm>
#include <map>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_features.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_server.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "components/variations/variations_associated_data.h"
#include "net/base/host_port_pair.h"
#include "net/base/proxy_server.h"
#include "net/http/http_status_code.h"
#include "url/url_constants.h"

#if defined(OS_ANDROID)
#include "base/system/sys_info.h"
#endif

namespace {

const char kEnabled[] = "Enabled";
const char kControl[] = "Control";
const char kDisabled[] = "Disabled";
const char kDefaultSecureProxyCheckUrl[] = "http://check.googlezip.net/connect";
const char kDefaultWarmupUrl[] = "http://check.googlezip.net/e2e_probe";

const char kQuicFieldTrial[] = "DataReductionProxyUseQuic";

const char kLitePageFieldTrial[] = "DataCompressionProxyLoFi";

// Default URL for retrieving the Data Reduction Proxy configuration.
const char kClientConfigURL[] =
    "https://datasaver.googleapis.com/v1/clientConfigs";

// Default URL for sending pageload metrics.
const char kPingbackURL[] =
    "https://datasaver.googleapis.com/v1/metrics:recordPageloadMetrics";

// LitePage black list version.
const char kLitePageBlackListVersion[] = "lite-page-blacklist-version";

const char kExperimentsOption[] = "exp";

bool IsIncludedInFieldTrial(const std::string& name) {
  return base::StartsWith(base::FieldTrialList::FindFullName(name), kEnabled,
                          base::CompareCase::SENSITIVE);
}

// Returns the variation value for |parameter_name|. If the value is
// unavailable, |default_value| is returned.
std::string GetStringValueForVariationParamWithDefaultValue(
    const std::map<std::string, std::string>& variation_params,
    const std::string& parameter_name,
    const std::string& default_value) {
  const auto it = variation_params.find(parameter_name);
  if (it == variation_params.end())
    return default_value;
  return it->second;
}

bool CanShowAndroidLowMemoryDevicePromo() {
#if defined(OS_ANDROID)
  return base::SysInfo::IsLowEndDevice() &&
         base::FeatureList::IsEnabled(
             data_reduction_proxy::features::
                 kDataReductionProxyLowMemoryDevicePromo);
#endif
  return false;
}

}  // namespace

namespace data_reduction_proxy {
namespace params {

bool IsIncludedInPromoFieldTrial() {
  if (IsIncludedInFieldTrial("DataCompressionProxyPromoVisibility"))
    return true;

  return CanShowAndroidLowMemoryDevicePromo();
}

bool IsIncludedInFREPromoFieldTrial() {
  if (IsIncludedInFieldTrial("DataReductionProxyFREPromo"))
    return true;

  return CanShowAndroidLowMemoryDevicePromo();
}

bool IsIncludedInHoldbackFieldTrial() {
  // For now, DRP can be disabled using either the field trial or the feature.
  // New server configs should use the feature capability.
  // TODO(tbansal): Remove the field trial code.
  return base::FeatureList::IsEnabled(
             data_reduction_proxy::features::kDataReductionProxyHoldback) ||
         IsIncludedInFieldTrial("DataCompressionProxyHoldback");
}

std::string HoldbackFieldTrialGroup() {
  return base::FieldTrialList::FindFullName("DataCompressionProxyHoldback");
}

bool ForceEnableClientConfigServiceForAllDataSaverUsers() {
  // Client config is enabled for all data users that are not in the
  // kDataReductionProxyHoldback. Users that have kDataReductionProxyHoldback
  // enabled have config service client enabled only if
  // |force_enable_config_service_fetches| is set to true.
  return GetFieldTrialParamByFeatureAsBool(
      data_reduction_proxy::features::kDataReductionProxyHoldback,
      "force_enable_config_service_fetches", false);
}

bool FetchWarmupProbeURLEnabled() {
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableDataReductionProxyWarmupURLFetch);
}

GURL GetWarmupURL() {
  std::map<std::string, std::string> params;
  variations::GetVariationParams(GetQuicFieldTrialName(), &params);
  return GURL(GetStringValueForVariationParamWithDefaultValue(
      params, "warmup_url", kDefaultWarmupUrl));
}

bool IsWarmupURLFetchCallbackEnabled() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          data_reduction_proxy::switches::
              kDisableDataReductionProxyWarmupURLFetchCallback)) {
    return false;
  }

  return true;
}

bool IsWarmupURL(const GURL& url) {
  GURL warmup_url = params::GetWarmupURL();
  return url.host() == warmup_url.host() && url.path() == warmup_url.path();
}

bool IsWhitelistedHttpResponseCodeForProbes(int http_response_code) {
  // 200 and 404 are always whitelisted.
  if (http_response_code == net::HTTP_OK ||
      http_response_code == net::HTTP_NOT_FOUND) {
    return true;
  }

  // Check if there is an additional whitelisted HTTP response code provided via
  // the field trial params.
  std::map<std::string, std::string> params;
  variations::GetVariationParams(GetQuicFieldTrialName(), &params);
  const std::string value = GetStringValueForVariationParamWithDefaultValue(
      params, "whitelisted_probe_http_response_code", "");
  int response_code;
  if (!base::StringToInt(value, &response_code))
    return false;
  return response_code == http_response_code;
}

bool IsForcePingbackEnabledViaFlags() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      data_reduction_proxy::switches::kEnableDataReductionProxyForcePingback);
}

bool WarnIfNoDataReductionProxy() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      data_reduction_proxy::switches::kEnableDataReductionProxyBypassWarning);
}

bool IsIncludedInQuicFieldTrial() {
  if (base::StartsWith(base::FieldTrialList::FindFullName(kQuicFieldTrial),
                       kControl, base::CompareCase::SENSITIVE)) {
    return false;
  }
  if (base::StartsWith(base::FieldTrialList::FindFullName(kQuicFieldTrial),
                       kDisabled, base::CompareCase::SENSITIVE)) {
    return false;
  }
  // QUIC is enabled by default.
  return true;
}

const char* GetQuicFieldTrialName() {
  return kQuicFieldTrial;
}

GURL GetConfigServiceURL() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  std::string url;
  if (command_line->HasSwitch(switches::kDataReductionProxyConfigURL)) {
    url = command_line->GetSwitchValueASCII(
        switches::kDataReductionProxyConfigURL);
  }

  if (url.empty())
    return GURL(kClientConfigURL);

  GURL result(url);
  if (result.is_valid())
    return result;

  LOG(WARNING) << "The following client config URL specified at the "
               << "command-line or variation is invalid: " << url;
  return GURL(kClientConfigURL);
}

GURL GetPingbackURL() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  std::string url;
  if (command_line->HasSwitch(switches::kDataReductionPingbackURL)) {
    url =
        command_line->GetSwitchValueASCII(switches::kDataReductionPingbackURL);
  }

  if (url.empty())
    return GURL(kPingbackURL);

  GURL result(url);
  if (result.is_valid())
    return result;

  LOG(WARNING) << "The following page load metrics URL specified at the "
               << "command-line or variation is invalid: " << url;
  return GURL(kPingbackURL);
}

int LitePageVersion() {
  return GetFieldTrialParameterAsInteger(kLitePageFieldTrial,
                                         kLitePageBlackListVersion, 0, 0);
}

int GetFieldTrialParameterAsInteger(const std::string& group,
                                    const std::string& param_name,
                                    int default_value,
                                    int min_value) {
  DCHECK(default_value >= min_value);
  std::string param_value =
      variations::GetVariationParamValue(group, param_name);
  int value;
  if (param_value.empty() || !base::StringToInt(param_value, &value) ||
      value < min_value) {
    return default_value;
  }

  return value;
}

bool GetOverrideProxiesForHttpFromCommandLine(
    std::vector<DataReductionProxyServer>* override_proxies_for_http) {
  DCHECK(override_proxies_for_http);
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDataReductionProxyHttpProxies)) {
    // It is illegal to use |kDataReductionProxy| or
    // |kDataReductionProxyFallback| with |kDataReductionProxyHttpProxies|.
    DCHECK(base::CommandLine::ForCurrentProcess()
               ->GetSwitchValueASCII(switches::kDataReductionProxy)
               .empty());
    DCHECK(base::CommandLine::ForCurrentProcess()
               ->GetSwitchValueASCII(switches::kDataReductionProxyFallback)
               .empty());
    override_proxies_for_http->clear();

    std::string proxy_overrides =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kDataReductionProxyHttpProxies);

    for (const auto& proxy_override :
         base::SplitStringPiece(proxy_overrides, ";", base::TRIM_WHITESPACE,
                                base::SPLIT_WANT_NONEMPTY)) {
      net::ProxyServer proxy_server = net::ProxyServer::FromURI(
          proxy_override, net::ProxyServer::SCHEME_HTTP);
      DCHECK(proxy_server.is_valid());
      DCHECK(!proxy_server.is_direct());

      override_proxies_for_http->push_back(
          DataReductionProxyServer(std::move(proxy_server)));
    }

    return true;
  }

  std::string origin =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kDataReductionProxy);

  std::string fallback_origin =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kDataReductionProxyFallback);

  if (origin.empty() && fallback_origin.empty())
    return false;

  override_proxies_for_http->clear();

  if (!origin.empty()) {
    net::ProxyServer primary_proxy =
        net::ProxyServer::FromURI(origin, net::ProxyServer::SCHEME_HTTP);
    DCHECK(primary_proxy.is_valid());
    DCHECK(!primary_proxy.is_direct());
    override_proxies_for_http->push_back(
        DataReductionProxyServer(std::move(primary_proxy)));
  }
  if (!fallback_origin.empty()) {
    net::ProxyServer fallback_proxy = net::ProxyServer::FromURI(
        fallback_origin, net::ProxyServer::SCHEME_HTTP);
    DCHECK(fallback_proxy.is_valid());
    DCHECK(!fallback_proxy.is_direct());
    override_proxies_for_http->push_back(
        DataReductionProxyServer(std::move(fallback_proxy)));
  }

  return true;
}

std::string GetDataSaverServerExperimentsOptionName() {
  return kExperimentsOption;
}

std::string GetDataSaverServerExperiments() {
  const std::string cmd_line_experiment =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          data_reduction_proxy::switches::kDataReductionProxyExperiment);

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          data_reduction_proxy::switches::
              kDataReductionProxyServerExperimentsDisabled)) {
    // Both kDataReductionProxyExperiment and
    // kDataReductionProxyServerExperimentsDisabled switches can't be set at the
    // same time.
    DCHECK(cmd_line_experiment.empty());
    return std::string();
  }

  // Experiment set using command line overrides field trial.
  if (!cmd_line_experiment.empty())
    return cmd_line_experiment;

  // First check if the feature is enabled.
  if (!base::FeatureList::IsEnabled(
          features::kDataReductionProxyServerExperiments)) {
    return std::string();
  }
  return base::GetFieldTrialParamValueByFeature(
      features::kDataReductionProxyServerExperiments, kExperimentsOption);
}


GURL GetSecureProxyCheckURL() {
  std::string secure_proxy_check_url =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kDataReductionProxySecureProxyCheckURL);
  if (secure_proxy_check_url.empty())
    secure_proxy_check_url = kDefaultSecureProxyCheckUrl;

  return GURL(secure_proxy_check_url);
}

bool IsEnabledWithNetworkService() {
  return base::FeatureList::IsEnabled(
      data_reduction_proxy::features::
          kDataReductionProxyEnabledWithNetworkService);
}

base::Optional<DataReductionProxyTypeInfo> FindConfiguredProxyInVector(
    const std::vector<DataReductionProxyServer>& proxies,
    const net::ProxyServer& proxy_server) {
  if (!proxy_server.is_valid() || proxy_server.is_direct())
    return base::nullopt;

  // Only compare the host port pair of the |proxy_server| since the proxy
  // scheme of the stored data reduction proxy may be different than the proxy
  // scheme of |proxy_server|. This may happen even when the |proxy_server| is a
  // valid data reduction proxy. As an example, the stored data reduction proxy
  // may have a proxy scheme of HTTPS while |proxy_server| may have QUIC as the
  // proxy scheme.
  const net::HostPortPair& host_port_pair = proxy_server.host_port_pair();
  auto it = std::find_if(
      proxies.begin(), proxies.end(),
      [&host_port_pair](const DataReductionProxyServer& proxy) {
        return proxy.proxy_server().host_port_pair().Equals(host_port_pair);
      });

  if (it == proxies.end())
    return base::nullopt;

  return DataReductionProxyTypeInfo(proxies,
                                    static_cast<size_t>(it - proxies.begin()));
}

}  // namespace params

DataReductionProxyParams::DataReductionProxyParams() {
  bool use_override_proxies_for_http =
      params::GetOverrideProxiesForHttpFromCommandLine(&proxies_for_http_);

  if (!use_override_proxies_for_http) {
    DCHECK(proxies_for_http_.empty());
    proxies_for_http_.push_back(
        DataReductionProxyServer(net::ProxyServer::FromURI(
            "https://proxy.googlezip.net:443", net::ProxyServer::SCHEME_HTTP)));
    proxies_for_http_.push_back(
        DataReductionProxyServer(net::ProxyServer::FromURI(
            "compress.googlezip.net:80", net::ProxyServer::SCHEME_HTTP)));
  }

  DCHECK(std::all_of(proxies_for_http_.begin(), proxies_for_http_.end(),
                     [](const DataReductionProxyServer& proxy) {
                       return proxy.proxy_server().is_valid() &&
                              !proxy.proxy_server().is_direct();
                     }));
}

DataReductionProxyParams::~DataReductionProxyParams() {}

void DataReductionProxyParams::SetProxiesForHttpForTesting(
    const std::vector<DataReductionProxyServer>& proxies_for_http) {
  DCHECK(std::all_of(proxies_for_http.begin(), proxies_for_http.end(),
                     [](const DataReductionProxyServer& proxy) {
                       return proxy.proxy_server().is_valid() &&
                              !proxy.proxy_server().is_direct();
                     }));

  proxies_for_http_ = proxies_for_http;
}

const std::vector<DataReductionProxyServer>&
DataReductionProxyParams::proxies_for_http() const {
  return proxies_for_http_;
}

base::Optional<DataReductionProxyTypeInfo>
DataReductionProxyParams::FindConfiguredDataReductionProxy(
    const net::ProxyServer& proxy_server) const {
  return params::FindConfiguredProxyInVector(proxies_for_http(), proxy_server);
}

net::ProxyList DataReductionProxyParams::GetAllConfiguredProxies() const {
  NOTREACHED();
  return net::ProxyList();
}

}  // namespace data_reduction_proxy
