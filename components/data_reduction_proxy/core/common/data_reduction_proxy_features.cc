// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/common/data_reduction_proxy_features.h"

#include "build/build_config.h"

namespace data_reduction_proxy {
namespace features {

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
    "DataReductionProxyHoldback", base::FEATURE_ENABLED_BY_DEFAULT};

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

// Reports estimated data savings due to save-data request header and JS API, as
// savings provided by DataSaver.
const base::Feature kReportSaveDataSavings{"ReportSaveDataSavings",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace features
}  // namespace data_reduction_proxy
