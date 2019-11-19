// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_COMMON_DATA_REDUCTION_PROXY_PARAMS_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_COMMON_DATA_REDUCTION_PROXY_PARAMS_H_

#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_config_values.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_type_info.h"
#include "url/gurl.h"

namespace net {
class ProxyServer;
}

namespace data_reduction_proxy {

class DataReductionProxyServer;

// The data_reduction_proxy::params namespace is a collection of methods to
// determine the operating parameters of the Data Reduction Proxy as specified
// by field trials and command line switches.
namespace params {

// Returns true if this client is part of a field trial that should display
// a promotion for the data reduction proxy.
bool IsIncludedInPromoFieldTrial();

// Returns true if this client is part of a field trial that should display
// a FRE promotion for the data reduction proxy.
bool IsIncludedInFREPromoFieldTrial();

// Returns true if this client is part of a field trial that runs a holdback
// experiment. A holdback experiment is one in which a fraction of browser
// instances will not be configured to use the data reduction proxy even if
// users have enabled it to be used. The UI will not indicate that a holdback
// is in effect.
bool IsIncludedInHoldbackFieldTrial();

// The name of the Holdback experiment group, this can return an empty string if
// not included in a group.
std::string HoldbackFieldTrialGroup();

// Returns true if DRP config service should always be fetched even if DRP
// holdback is enabled.
bool ForceEnableClientConfigServiceForAllDataSaverUsers();

// Returns true if this client has the command line switch to enable forced
// pageload metrics pingbacks on every page load.
bool IsForcePingbackEnabledViaFlags();

// Returns true if this client has the command line switch to show
// interstitials for data reduction proxy bypasses.
bool WarnIfNoDataReductionProxy();

// Returns true if this client is part of a field trial that sets the origin
// proxy server as quic://proxy.googlezip.net.
bool IsIncludedInQuicFieldTrial();

const char* GetQuicFieldTrialName();

// If the Data Reduction Proxy is used for a page load, the URL for the
// Data Reduction Proxy Pageload Metrics service.
GURL GetPingbackURL();

// If the Data Reduction Proxy config client is being used, the URL for the
// Data Reduction Proxy config service.
GURL GetConfigServiceURL();

// The current LitePage experiment blacklist version.
int LitePageVersion();

// Retrieves the int stored in |param_name| from the field trial group
// |group|. If the value is not present, cannot be parsed, or is less than
// |min_value|, returns |default_value|.
int GetFieldTrialParameterAsInteger(const std::string& group,
                                    const std::string& param_name,
                                    int default_value,
                                    int min_value);

// Returns true if the list of Data Reduction Proxies to use for HTTP requests
// has been overridden on the command line, and if so, returns the override
// proxy list in |override_proxies_for_http|.
bool GetOverrideProxiesForHttpFromCommandLine(
    std::vector<DataReductionProxyServer>* override_proxies_for_http);

// Returns the server experiments option name. This name is used in the request
// headers to the data saver proxy. This name is also used to set the experiment
// name using finch trial.
std::string GetDataSaverServerExperimentsOptionName();

// Returns the server experiment. This name is used in the request
// headers to the data saver proxy. Returned value may be empty indicating no
// experiment is enabled.
std::string GetDataSaverServerExperiments();

// Returns the URL to check to decide if the secure proxy origin should be
// used.
GURL GetSecureProxyCheckURL();

// Returns true if fetching of the warmup URL is enabled.
bool FetchWarmupProbeURLEnabled();

// Returns the warmup URL.
GURL GetWarmupURL();

// Returns true if the warmup URL fetcher should callback into DRP to report the
// result of the warmup fetch.
bool IsWarmupURLFetchCallbackEnabled();

// Returns true if |url| is the warmup url.
bool IsWarmupURL(const GURL& url);

// Returns true if the |http_response_code| is in the whitelist of HTTP response
// codes that are considered as successful for fetching the warmup probe URL.
// If this method returns false, then the probe should be considered as
// unsuccessful.
bool IsWhitelistedHttpResponseCodeForProbes(int http_response_code);

// Returns whether network service is enabled and data reduction proxy should be
// used.
bool IsEnabledWithNetworkService();

// Returns the experiment parameter name to discard the cached result for canary
// check probe.
const char* GetDiscardCanaryCheckResultParam();

// Helper function to locate |proxy_server| in |proxies| if it exists. This
// function is exposed publicly so that DataReductionProxyParams can use it.
base::Optional<DataReductionProxyTypeInfo> FindConfiguredProxyInVector(
    const std::vector<DataReductionProxyServer>& proxies,
    const net::ProxyServer& proxy_server);

}  // namespace params

// Provides initialization parameters. Proxy origins, and the secure proxy
// check url are are taken from flags if available and from preprocessor
// constants otherwise. The DataReductionProxySettings class and others use this
// class to determine the necessary DNS names to configure use of the Data
// Reduction Proxy.
class DataReductionProxyParams : public DataReductionProxyConfigValues {
 public:
  // Constructs configuration parameters. A standard configuration has a primary
  // proxy, and a fallback proxy for HTTP traffic.
  DataReductionProxyParams();

  // Updates |proxies_for_http_|.
  void SetProxiesForHttpForTesting(
      const std::vector<DataReductionProxyServer>& proxies_for_http);

  ~DataReductionProxyParams() override;

  const std::vector<DataReductionProxyServer>& proxies_for_http()
      const override;

  // Finds the first proxy in |proxies_for_http()| that matches |proxy_server|
  // if any exist.
  base::Optional<DataReductionProxyTypeInfo> FindConfiguredDataReductionProxy(
      const net::ProxyServer& proxy_server) const override;
  net::ProxyList GetAllConfiguredProxies() const override;

 private:
  std::vector<DataReductionProxyServer> proxies_for_http_;

  DISALLOW_COPY_AND_ASSIGN(DataReductionProxyParams);
};

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_COMMON_DATA_REDUCTION_PROXY_PARAMS_H_
