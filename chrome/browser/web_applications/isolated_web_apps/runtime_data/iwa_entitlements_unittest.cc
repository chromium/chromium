// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/runtime_data/iwa_entitlements.h"

#include <string>

#include "build/build_config.h"
#include "chrome/browser/web_applications/isolated_web_apps/runtime_data/chrome_iwa_runtime_data_provider.h"
#include "services/network/public/cpp/permissions_policy/permissions_policy_features_generated.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {

using AllowlistData = IwaAccessControl::UserInstallAllowlistItemData;

TEST(IwaEntitlementsTest, FeatureToEntitlementMapping) {
  EXPECT_EQ(GetEntitlementForFeature("direct-sockets"),
            AllowlistData::DIRECT_SOCKETS);
  EXPECT_EQ(GetEntitlementForFeature("direct-sockets-private"),
            AllowlistData::DIRECT_SOCKETS_PRIVATE);
  EXPECT_EQ(GetEntitlementForFeature("smart-card"), AllowlistData::SMART_CARD);
  EXPECT_EQ(GetEntitlementForFeature("web-printing"),
            AllowlistData::WEB_PRINTING);
  EXPECT_EQ(GetEntitlementForFeature("usb-unrestricted"),
            AllowlistData::UNRESTRICTED_WEBUSB);
  EXPECT_EQ(GetEntitlementForFeature("controlled-frame"),
            AllowlistData::CONTROLLED_FRAME);
  EXPECT_EQ(GetEntitlementForFeature("sub-apps"), AllowlistData::SUB_APPS);

  EXPECT_FALSE(GetEntitlementForFeature("all-screens-capture").has_value());
  EXPECT_FALSE(GetEntitlementForFeature("geolocation").has_value());
  EXPECT_FALSE(GetEntitlementForFeature("camera").has_value());
  EXPECT_FALSE(GetEntitlementForFeature("microphone").has_value());
  EXPECT_FALSE(GetEntitlementForFeature("unknown-feature").has_value());
}

TEST(IwaEntitlementsTest, IsFeatureGuardedByIsolatedContext) {
  EXPECT_TRUE(network::IsPermissionsPolicyFeatureGuardedByIsolatedContext(
      "direct-sockets"));
  EXPECT_TRUE(network::IsPermissionsPolicyFeatureGuardedByIsolatedContext(
      "direct-sockets-private"));
  EXPECT_TRUE(network::IsPermissionsPolicyFeatureGuardedByIsolatedContext(
      "direct-sockets-multicast"));
  EXPECT_TRUE(network::IsPermissionsPolicyFeatureGuardedByIsolatedContext(
      "smart-card"));
  EXPECT_TRUE(network::IsPermissionsPolicyFeatureGuardedByIsolatedContext(
      "web-printing"));
  EXPECT_TRUE(network::IsPermissionsPolicyFeatureGuardedByIsolatedContext(
      "usb-unrestricted"));
  EXPECT_TRUE(network::IsPermissionsPolicyFeatureGuardedByIsolatedContext(
      "controlled-frame"));
  EXPECT_TRUE(
      network::IsPermissionsPolicyFeatureGuardedByIsolatedContext("sub-apps"));
  EXPECT_TRUE(network::IsPermissionsPolicyFeatureGuardedByIsolatedContext(
      "all-screens-capture"));

  EXPECT_FALSE(network::IsPermissionsPolicyFeatureGuardedByIsolatedContext(
      "geolocation"));
  EXPECT_FALSE(
      network::IsPermissionsPolicyFeatureGuardedByIsolatedContext("camera"));
  EXPECT_FALSE(network::IsPermissionsPolicyFeatureGuardedByIsolatedContext(
      "microphone"));
  EXPECT_FALSE(network::IsPermissionsPolicyFeatureGuardedByIsolatedContext(
      "unknown-feature"));
}

}  // namespace web_app
