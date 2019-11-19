// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_COMMON_DATA_REDUCTION_PROXY_FEATURES_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_COMMON_DATA_REDUCTION_PROXY_FEATURES_H_

#include "base/feature_list.h"

namespace data_reduction_proxy {
namespace features {

extern const base::Feature kDataReductionProxyDecidesTransform;
extern const base::Feature kDataReductionProxyLowMemoryDevicePromo;
extern const base::Feature kDogfood;
extern const base::Feature kDataReductionProxyHoldback;
extern const base::Feature kDataReductionProxyEnabledWithNetworkService;
extern const base::Feature kDataSaverUseOnDeviceSafeBrowsing;
extern const base::Feature kDataReductionProxyBlockOnBadGatewayResponse;
extern const base::Feature kDataReductionProxyPopulatePreviewsPageIDToPingback;
extern const base::Feature kDataReductionProxyDisableProxyFailedWarmup;
extern const base::Feature kDataReductionProxyServerExperiments;
extern const base::Feature kDataReductionProxyAggressiveConfigFetch;

}  // namespace features
}  // namespace data_reduction_proxy
#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_COMMON_DATA_REDUCTION_PROXY_FEATURES_H_
