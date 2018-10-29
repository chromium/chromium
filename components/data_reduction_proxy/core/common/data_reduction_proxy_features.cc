// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/common/data_reduction_proxy_features.h"

#include "build/build_config.h"

namespace data_reduction_proxy {
namespace features {

// Enables the Data Reduction Proxy menu item in the main menu on Android.
const base::Feature kDataReductionMainMenu{"DataReductionProxyMainMenu",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

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

// Enables experiment that makes connection to the data saver proxy more robust.
const base::Feature kDataReductionProxyRobustConnection{
    "DataReductionProxyRobustConnection", base::FEATURE_ENABLED_BY_DEFAULT};

// Enabled for Chrome dogfooders.
const base::Feature kDogfood{"DataReductionProxyDogfood",
                             base::FEATURE_DISABLED_BY_DEFAULT};

// Enables recording of the site-breakdown metrics using the page load metrics
// harness, and disables the observer for data use ascriber.
const base::Feature kDataSaverSiteBreakdownUsingPageLoadMetrics{
    "DataSaverSiteBreakdownUsingPageLoadMetrics",
    base::FEATURE_ENABLED_BY_DEFAULT};

// If enabled, "br" is not added to the accept-encoding header. This effectively
// disables the use of Brotli on the connection from Chrome to secure
// HTTPS data saver proxies.
const base::Feature kDataReductionProxyBrotliHoldback{
    "DataReductionProxyBrotliHoldback", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables data reduction proxy when network service is enabled.
const base::Feature kDataReductionProxyEnabledWithNetworkService{
    "DataReductionProxyEnabledWithNetworkService",
    base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace features
}  // namespace data_reduction_proxy
