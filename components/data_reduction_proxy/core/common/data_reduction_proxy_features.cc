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

// Reports estimated data savings due to save-data request header and JS API, as
// savings provided by DataSaver.
const base::Feature kReportSaveDataSavings{"ReportSaveDataSavings",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace features
}  // namespace data_reduction_proxy
