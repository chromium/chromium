// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/experiences/isolated_web_app/isolated_web_app_api_allowlist.h"

#include <algorithm>
#include <string_view>
#include <vector>

#include "base/containers/fixed_flat_set.h"
#include "base/no_destructor.h"
#include "components/webapps/isolated_web_apps/scheme.h"
#include "url/origin.h"

namespace ash {

namespace {

// A set of bundle IDs allowed to access the CrOS IWA API.
static constexpr auto kIwaAllowlist = base::MakeFixedFlatSet<std::string_view>({
    "ckg65ilaae42o6wd3uj4xfwznhba7pz2p6kojga5c27hkwq5f66qaaic",
    "ov6jyjge37xqptadeo5e2n6ayoxdjqw62pgm4afavkqbnqfpesraaaic",
});

std::vector<std::string_view>& GetTestAllowlist() {
  static base::NoDestructor<std::vector<std::string_view>> allowlist;
  return *allowlist;
}

}  // namespace

bool CanOriginAccessCrosIwaApi(const url::Origin& origin) {
  if (origin.scheme() != webapps::kIsolatedAppScheme) {
    return false;
  }

  const auto& test_allowlist = GetTestAllowlist();
  if (!test_allowlist.empty()) {
    return std::ranges::contains(test_allowlist, origin.host());
  }

  return kIwaAllowlist.contains(origin.host());
}

base::AutoReset<std::vector<std::string_view>>
SetAllowlistedCrosIwaApiOriginsForTesting(
    std::vector<std::string_view> origins) {  // IN-TEST
  return {&GetTestAllowlist(), std::move(origins)};
}

}  // namespace ash
