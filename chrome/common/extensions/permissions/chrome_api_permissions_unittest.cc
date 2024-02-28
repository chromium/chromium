// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/contains.h"
#include "extensions/common/features/feature_provider.h"
#include "extensions/common/mojom/api_permission_id.mojom.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permissions_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

TEST(ChromeAPIPermissionsUnitTest, AllApiPermissionsHaveACorrespondingFeature) {
  APIPermissionSet all_api_permissions =
      PermissionsInfo::GetInstance()->GetAllForTest();
  ASSERT_FALSE(all_api_permissions.empty());

  // Sanity check that the returned API permissions include both those from
  // the //chrome layer and the //extensions layer. This is important because
  // the dependency in this test on the //chrome layer is subtle (the fact that
  // this runs as part of `unit_tests` means the chrome-layer permissions are
  // added as part of the environment setup).
  // `downloads` is a chrome-layer permission.
  // `storage` is an extensions-layer permission.
  ASSERT_EQ(1u, all_api_permissions.count(mojom::APIPermissionID::kDownloads));
  ASSERT_EQ(1u, all_api_permissions.count(mojom::APIPermissionID::kStorage));

  const FeatureProvider* permission_features =
      FeatureProvider::GetPermissionFeatures();
  // Iterate over every API permission and ensure each has an entry in
  // _permission_features.json.
  for (const auto* permission : all_api_permissions) {
    // Internal-only permissions (which are generally used as implementation
    // details) do not need to have a separate features entry.
    if (permission->info()->is_internal()) {
      continue;
    }

    const Feature* feature =
        permission_features->GetFeature(permission->name());
    EXPECT_TRUE(feature)
        << "Missing _permission_features.json entry for permission '"
        << permission->name()
        << "'. Add a new entry in _permission_features.json to fix this.";
  }
}

}  // namespace extensions
