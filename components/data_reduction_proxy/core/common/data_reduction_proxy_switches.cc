// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"

namespace data_reduction_proxy {
namespace switches {

// The origin of the data reduction proxy.
const char kDataReductionProxy[]         = "spdy-proxy-auth-origin";

// The URL from which to retrieve the Data Reduction Proxy configuration.
const char kDataReductionProxyConfigURL[] = "data-reduction-proxy-config-url";

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

// The origin of the data reduction proxy fallback.
const char kDataReductionProxyFallback[] = "spdy-proxy-auth-fallback";

// The semicolon-separated list of proxy server URIs to override the list of
// HTTP proxies returned by the Data Saver API. It is illegal to use
// |kDataReductionProxy| or |kDataReductionProxyFallback| switch in conjunction
// with |kDataReductionProxyHttpProxies|. If the URI omits a scheme, then the
// proxy server scheme defaults to HTTP, and if the port is omitted then the
// default port for that scheme is used. E.g. "http://foo.net:80",
// "http://foo.net", "foo.net:80", and "foo.net" are all equivalent.
const char kDataReductionProxyHttpProxies[] =
    "data-reduction-proxy-http-proxies";

// A test key for data reduction proxy authentication.
const char kDataReductionProxyKey[] = "spdy-proxy-auth-value";

const char kDataReductionPingbackURL[] = "data-reduction-proxy-pingback-url";

// Sets a secure proxy check URL to test before committing to using the Data
// Reduction Proxy. Note this check does not go through the Data Reduction
// Proxy.
const char kDataReductionProxySecureProxyCheckURL[] =
    "data-reduction-proxy-secure-proxy-check-url";

// Disables server experiments that may be enabled through field trial.
const char kDataReductionProxyServerExperimentsDisabled[] =
    "data-reduction-proxy-server-experiments-disabled";

// Enable the data reduction proxy.
const char kEnableDataReductionProxy[] = "enable-spdy-proxy-auth";

// Enable the data reduction proxy bypass warning.
const char kEnableDataReductionProxyBypassWarning[] =
    "enable-data-reduction-proxy-bypass-warning";

// Enables sending a pageload metrics pingback after every page load.
const char kEnableDataReductionProxyForcePingback[] =
    "enable-data-reduction-proxy-force-pingback";

// Enables a 1 MB savings promo for the data reduction proxy.
const char kEnableDataReductionProxySavingsPromo[] =
    "enable-data-reduction-proxy-savings-promo";

// Disables fetching of the warmup URL.
const char kDisableDataReductionProxyWarmupURLFetch[] =
    "disable-data-reduction-proxy-warmup-url-fetch";

// Disables the warmup URL fetcher to callback into DRP to report the result of
// the warmup fetch.
const char kDisableDataReductionProxyWarmupURLFetchCallback[] =
    "disable-data-reduction-proxy-warmup-url-fetch-callback";

// Uses the encoded ClientConfig instead of fetching one from the config server.
// This value is always used, regardless of error or expiration. The value
// should be a base64 encoded binary protobuf.
const char kDataReductionProxyServerClientConfig[] =
    "data-reduction-proxy-client-config";

}  // namespace switches
}  // namespace data_reduction_proxy
