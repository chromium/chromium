// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"

namespace data_reduction_proxy {
namespace switches {

// Enable the data reduction proxy.
const char kEnableDataReductionProxy[] = "enable-spdy-proxy-auth";

// Enables a 1 MB savings promo for the data reduction proxy.
const char kEnableDataReductionProxySavingsPromo[] =
    "enable-data-reduction-proxy-savings-promo";

// Override the one-time InfoBar to not needed to be shown before triggering
// https image compression for the page load.
const char kOverrideHttpsImageCompressionInfobar[] =
    "override-https-image-compression-infobar";

}  // namespace switches
}  // namespace data_reduction_proxy
