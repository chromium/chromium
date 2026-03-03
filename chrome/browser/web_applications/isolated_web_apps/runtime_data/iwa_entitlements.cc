// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/runtime_data/iwa_entitlements.h"

#include "base/containers/fixed_flat_map.h"
#include "base/containers/map_util.h"
#include "base/containers/to_value_list.h"
#include "base/types/optional_util.h"
#include "base/values.h"
#include "build/build_config.h"
#include "services/network/public/cpp/permissions_policy/permissions_policy_features_generated.h"

namespace web_app {

IwaEntitlementsSet::IwaEntitlementsSet() = default;
IwaEntitlementsSet::~IwaEntitlementsSet() = default;
IwaEntitlementsSet::IwaEntitlementsSet(const IwaEntitlementsSet&) = default;
IwaEntitlementsSet& IwaEntitlementsSet::operator=(const IwaEntitlementsSet&) =
    default;

base::Value IwaEntitlementsSet::AsDebugValue() const {
  return base::Value(
      base::DictValue()
          .Set("version_range", base::DictValue()
                                    .Set("begin", version_range.begin())
                                    .Set("end", version_range.end()))
          .Set("entitlements",
               base::ToValueList(entitlements, [](const auto& entitlement) {
                 return IwaAccessControl::UserInstallAllowlistItemData::
                     Entitlement_Name(entitlement);
               })));
}

bool IwaEntitlementsSet::operator==(const IwaEntitlementsSet& other) const {
  return version_range.begin() == other.version_range.begin() &&
         version_range.end() == other.version_range.end() &&
         entitlements == other.entitlements;
}

std::optional<IwaEntitlement> GetEntitlementForFeature(
    const std::string& feature_name) {
  static constexpr auto kEntitlementMap = base::MakeFixedFlatMap<
      std::string_view, IwaEntitlement>({
      {"controlled-frame",
       IwaAccessControl::UserInstallAllowlistItemData::CONTROLLED_FRAME},
      {"direct-sockets",
       IwaAccessControl::UserInstallAllowlistItemData::DIRECT_SOCKETS},
      {"direct-sockets-multicast",
       IwaAccessControl::UserInstallAllowlistItemData::
           DIRECT_SOCKETS_MULTICAST},
      {"direct-sockets-private",
       IwaAccessControl::UserInstallAllowlistItemData::DIRECT_SOCKETS_PRIVATE},
      {"smart-card",
       IwaAccessControl::UserInstallAllowlistItemData::SMART_CARD},
      {"sub-apps", IwaAccessControl::UserInstallAllowlistItemData::SUB_APPS},
      {"usb-unrestricted",
       IwaAccessControl::UserInstallAllowlistItemData::UNRESTRICTED_WEBUSB},
      {"web-printing",
       IwaAccessControl::UserInstallAllowlistItemData::WEB_PRINTING},
  });

  return base::OptionalFromPtr(base::FindOrNull(kEntitlementMap, feature_name));
}

}  // namespace web_app
