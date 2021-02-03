// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"

namespace data_reduction_proxy {
namespace switches {

// The origin of the data reduction proxy.
const char kDataReductionProxy[]         = "spdy-proxy-auth-origin";

// The name of a Data Reduction Proxy experiment to run. These experiments are
// defined by the proxy server. Use --force-fieldtrials for Data Reduction
// Proxy field trials.
const char kDataReductionProxyExperiment[] = "data-reduction-proxy-experiment";

// The Chrome-Proxy "exp" directive value used by data reduction proxy to
// receive an alternative back end implementation.
const char kDataReductionProxyServerAlternative1[] = "alt1";
const char kDataReductionProxyServerAlternative2[] = "alt2";
const char kDataReductionProxyServerAlternative3[] = "alt3";
const char kDataReductionProxyServerAlternative4[] = "alt4";
const char kDataReductionProxyServerAlternative5[] = "alt5";
const char kDataReductionProxyServerAlternative6[] = "alt6";
const char kDataReductionProxyServerAlternative7[] = "alt7";
const char kDataReductionProxyServerAlternative8[] = "alt8";
const char kDataReductionProxyServerAlternative9[] = "alt9";
const char kDataReductionProxyServerAlternative10[] = "alt10";

// A test key for data reduction proxy authentication.
const char kDataReductionProxyKey[] = "spdy-proxy-auth-value";

// Disables server experiments that may be enabled through field trial.
const char kDataReductionProxyServerExperimentsDisabled[] =
    "data-reduction-proxy-server-experiments-disabled";

// Enable the data reduction proxy.
const char kEnableDataReductionProxy[] = "enable-spdy-proxy-auth";

// Enable the data reduction proxy bypass warning.
const char kEnableDataReductionProxyBypassWarning[] =
    "enable-data-reduction-proxy-bypass-warning";

// Enables a 1 MB savings promo for the data reduction proxy.
const char kEnableDataReductionProxySavingsPromo[] =
    "enable-data-reduction-proxy-savings-promo";

// Uses the encoded ClientConfig instead of fetching one from the config server.
// This value is always used, regardless of error or expiration. The value
// should be a base64 encoded binary protobuf.
const char kDataReductionProxyServerClientConfig[] =
    "data-reduction-proxy-client-config";

// Override the one-time InfoBar to not needed to be shown before triggering
// https image compression for the page load.
const char kOverrideHttpsImageCompressionInfobar[] =
    "override-https-image-compression-infobar";

}  // namespace switches
}  // namespace data_reduction_proxy
