// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_NETWORK_HEADER_INJECTION_CORE_FEATURES_H_
#define COMPONENTS_ENTERPRISE_NETWORK_HEADER_INJECTION_CORE_FEATURES_H_

#include "base/feature_list.h"

namespace enterprise_custom_headers {

// Controls the HTTP Header Injection feature. Enabled by default on desktop
// platforms, where it acts as a kill-switch. Disabled by default on Android
// until the feature is launched there.
BASE_DECLARE_FEATURE(kHttpHeadersInjection);

bool IsHttpHeaderInjectionEnabled();

}  // namespace enterprise_custom_headers

#endif  // COMPONENTS_ENTERPRISE_NETWORK_HEADER_INJECTION_CORE_FEATURES_H_
