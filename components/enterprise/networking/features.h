// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_NETWORKING_FEATURES_H_
#define COMPONENTS_ENTERPRISE_NETWORKING_FEATURES_H_

#include "base/feature_list.h"

namespace enterprise {

// Controls whether the proxy authentication service is enabled.
BASE_DECLARE_FEATURE(kEnableProxyAuthenticationService);

// Return true if the proxy authentication service is enabled.
bool IsProxyAuthenticationEnabled();

}  // namespace enterprise

#endif  // COMPONENTS_ENTERPRISE_NETWORKING_FEATURES_H_
