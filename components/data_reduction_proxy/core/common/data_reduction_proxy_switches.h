// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_COMMON_DATA_REDUCTION_PROXY_SWITCHES_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_COMMON_DATA_REDUCTION_PROXY_SWITCHES_H_

namespace data_reduction_proxy {
namespace switches {

// All switches in alphabetical order. The switches should be documented
// alongside the definition of their values in the .cc file.

extern const char kDataReductionPingbackURL[];
extern const char kDataReductionProxy[];
extern const char kDataReductionProxyConfigURL[];
extern const char kDataReductionProxyExperiment[];
extern const char kDataReductionProxyFallback[];
extern const char kDataReductionProxyHttpProxies[];
extern const char kDataReductionProxyKey[];
extern const char kDataReductionProxySecureProxyCheckURL[];
extern const char kDataReductionProxyServerExperimentsDisabled[];
extern const char kDataReductionProxyServerAlternative1[];
extern const char kDataReductionProxyServerAlternative2[];
extern const char kDataReductionProxyServerAlternative3[];
extern const char kDataReductionProxyServerAlternative4[];
extern const char kDataReductionProxyServerAlternative5[];
extern const char kDataReductionProxyServerAlternative6[];
extern const char kDataReductionProxyServerAlternative7[];
extern const char kDataReductionProxyServerAlternative8[];
extern const char kDataReductionProxyServerAlternative9[];
extern const char kDataReductionProxyServerAlternative10[];
extern const char kDataReductionProxyServerClientConfig[];
extern const char kEnableDataReductionProxy[];
extern const char kEnableDataReductionProxyBypassWarning[];
extern const char kEnableDataReductionProxyForcePingback[];
extern const char kEnableDataReductionProxySavingsPromo[];
extern const char kDisableDataReductionProxyWarmupURLFetch[];
extern const char kDisableDataReductionProxyWarmupURLFetchCallback[];

}  // namespace switches
}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_COMMON_DATA_REDUCTION_PROXY_SWITCHES_H_
