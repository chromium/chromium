// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CONNECTORS_CORE_ENTERPRISE_INTERSTITIAL_UTIL_H_
#define COMPONENTS_ENTERPRISE_CONNECTORS_CORE_ENTERPRISE_INTERSTITIAL_UTIL_H_

#include <string>
#include <vector>

#include "components/security_interstitials/core/unsafe_resource.h"

// This namespace contains shared functions for enterprise interstitials
// security pages.
namespace enterprise_connectors {

// Returns the custom message specified by admin in RTLookup response.
std::u16string GetUrlFilteringCustomMessage(
    const std::vector<security_interstitials::UnsafeResource>&
        unsafe_resources);

}  // namespace enterprise_connectors

#endif  // COMPONENTS_ENTERPRISE_CONNECTORS_CORE_ENTERPRISE_INTERSTITIAL_UTIL_H_
