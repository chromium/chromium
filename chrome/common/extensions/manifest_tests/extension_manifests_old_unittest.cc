// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/manifest_tests/chrome_manifest_test.h"

#include "extensions/common/extension.h"
#include "extensions/common/permissions/permissions_data.h"
#include "testing/gtest/include/gtest/gtest.h"

// Tests that the old permission name "unlimited_storage" still works for
// backwards compatibility (we renamed it to "unlimitedStorage").
TEST_F(ChromeManifestTest, OldUnlimitedStoragePermission) {
  scoped_refptr<extensions::Extension> extension =
      LoadAndExpectSuccess("old_unlimited_storage.json",
                           extensions::mojom::ManifestLocation::kInternal,
                           extensions::Extension::NO_FLAGS);
  EXPECT_TRUE(extension->permissions_data()->HasAPIPermission(
      extensions::mojom::APIPermissionID::kUnlimitedStorage));
}
