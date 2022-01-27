// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_COMMON_DATA_REDUCTION_PROXY_PREF_NAMES_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_COMMON_DATA_REDUCTION_PROXY_PREF_NAMES_H_

namespace data_reduction_proxy {
namespace prefs {

// Alphabetical list of preference names specific to the data_reduction_proxy
// component. Keep alphabetized, and document each in the .cc file.

extern const char kDataSaverEnabled[];
extern const char kDataReductionProxyWasEnabledBefore[];
extern const char kDataReductionProxyLastEnabledTime[];

}  // namespace prefs
}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_COMMON_DATA_REDUCTION_PROXY_PREF_NAMES_H_
