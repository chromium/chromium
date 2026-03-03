// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_RUNTIME_DATA_IWA_ENTITLEMENTS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_RUNTIME_DATA_IWA_ENTITLEMENTS_H_

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/values.h"
#include "chrome/browser/web_applications/isolated_web_apps/key_distribution/proto/key_distribution.pb.h"

namespace web_app {

using IwaEntitlement =
    IwaAccessControl::UserInstallAllowlistItemData::Entitlement;
using IwaVersionRange =
    IwaAccessControl::UserInstallAllowlistItemData::VersionRange;

struct IwaEntitlementsSet {
  IwaEntitlementsSet();
  ~IwaEntitlementsSet();
  IwaEntitlementsSet(const IwaEntitlementsSet&);
  IwaEntitlementsSet& operator=(const IwaEntitlementsSet&);

  IwaVersionRange version_range;
  std::vector<IwaEntitlement> entitlements;

  base::Value AsDebugValue() const;
  bool operator==(const IwaEntitlementsSet&) const;
};

// Maps a permissions policy feature to the required entitlement for
// user-installed Isolated Web Apps. If the feature is not restricted by
// entitlements, returns std::nullopt.
//
// If the feature is `std::nullopt` it doesn't mean that the feature is allowed
// by default. It just means that it's not controlled by a specific entitlement.
// If it's `std::nullopt` and
// `network::IsPermissionsPolicyFeatureGuardedByIsolatedContext` returns true,
// then the feature is disallowed for user-installed IWAs.
std::optional<IwaEntitlement> GetEntitlementForFeature(
    const std::string& feature_name);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_RUNTIME_DATA_IWA_ENTITLEMENTS_H_
