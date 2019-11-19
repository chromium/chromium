// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/common/data_reduction_proxy_features.h"

#include "build/build_config.h"

namespace data_reduction_proxy {
namespace features {

// Enables a new version of the data reduction proxy protocol where the server
// decides if a server-generated preview should be served. The previous
// version required the client to make this decision. The new protocol relies
// on updates primarily to the Chrome-Proxy-Accept-Transform header.
const base::Feature kDataReductionProxyDecidesTransform{
    "DataReductionProxyDecidesTransform",
#if defined(OS_ANDROID)
    base::FEATURE_ENABLED_BY_DEFAULT
#else   // !defined(OS_ANDROID)
    base::FEATURE_DISABLED_BY_DEFAULT
#endif  // defined(OS_ANDROID)
};

// Enables the data saver promo for low memory Android devices.
const base::Feature kDataReductionProxyLowMemoryDevicePromo{
    "DataReductionProxyLowMemoryDevicePromo",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Enabled for Chrome dogfooders.
const base::Feature kDogfood{"DataReductionProxyDogfood",
                             base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, the usage of data reduction proxy is disabled for HTTP URLs.
// Does not affect the state of save-data header or other
// features that may depend on data saver being enabled.
const base::Feature kDataReductionProxyHoldback{
    "DataReductionProxyHoldback", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables data reduction proxy when network service is enabled.
const base::Feature kDataReductionProxyEnabledWithNetworkService{
  "DataReductionProxyEnabledWithNetworkService",
      base::FEATURE_ENABLED_BY_DEFAULT
};

// Enables block action of all proxies when 502 is received with no
// Chrome-Proxy header. The block duration is configurable via field trial with
// a default of one second.
const base::Feature kDataReductionProxyBlockOnBadGatewayResponse{
    "DataReductionProxyBlockOnBadGatewayResponse",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Enables populating the previews page ID from NavigationUIData to the
// pingbacks.
const base::Feature kDataReductionProxyPopulatePreviewsPageIDToPingback{
    "DataReductionProxyPopulatePreviewsPageIDToPingback",
    base::FEATURE_ENABLED_BY_DEFAULT};

// Enables not allowing proxies that fail warmup url fetch, to custom proxy
// config updates when network service is enabled.
const base::Feature kDataReductionProxyDisableProxyFailedWarmup{
    "DataReductionProxyDisableProxyFailedWarmup",
    base::FEATURE_ENABLED_BY_DEFAULT};

// Enables server experiments run jointly with Chrome. The experiment
// id should be specified using the finch parameter
// params::GetDataSaverServerExperimentsOptionName().
const base::Feature kDataReductionProxyServerExperiments{
    "DataReductionProxyServerExperiments", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables fetching the client config aggressively by tuning the backoff params
// and by not deferring fetches while Chrome is in background.
const base::Feature kDataReductionProxyAggressiveConfigFetch{
    "DataReductionProxyAggressiveConfigFetch",
    base::FEATURE_ENABLED_BY_DEFAULT};

}  // namespace features
}  // namespace data_reduction_proxy
