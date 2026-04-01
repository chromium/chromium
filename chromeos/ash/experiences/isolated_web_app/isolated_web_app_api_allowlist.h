// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_EXPERIENCES_ISOLATED_WEB_APP_ISOLATED_WEB_APP_API_ALLOWLIST_H_
#define CHROMEOS_ASH_EXPERIENCES_ISOLATED_WEB_APP_ISOLATED_WEB_APP_API_ALLOWLIST_H_

#include <string_view>
#include <vector>

#include "base/auto_reset.h"
#include "base/component_export.h"
#include "url/origin.h"

namespace ash {

// Returns true if the given origin is allowed to access the CrOS IWA API.
bool CanOriginAccessCrosIwaApi(const url::Origin& origin);

// Overrides the set of allowlisted origins for testing.
// The allowlist will be reset to its default state when the returned
// AutoReset is destroyed.
[[nodiscard]] COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_ISOLATED_WEB_APP) base::
    AutoReset<std::vector<std::string_view>> SetAllowlistedCrosIwaApiOriginsForTesting(
        std::vector<std::string_view> origins);

}  // namespace ash

#endif  // CHROMEOS_ASH_EXPERIENCES_ISOLATED_WEB_APP_ISOLATED_WEB_APP_API_ALLOWLIST_H_
