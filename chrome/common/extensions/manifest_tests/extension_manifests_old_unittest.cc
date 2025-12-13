// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/manifest_tests/chrome_manifest_test.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/permissions_data.h"
#include "testing/gtest/include/gtest/gtest.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {
namespace {

// Tests that the old permission name "unlimited_storage" still works for
// backwards compatibility (we renamed it to "unlimitedStorage").
TEST_F(ChromeManifestTest, OldUnlimitedStoragePermission) {
  scoped_refptr<Extension> extension = LoadAndExpectSuccess(
      "old_unlimited_storage.json", mojom::ManifestLocation::kInternal,
      Extension::NO_FLAGS);
  EXPECT_TRUE(extension->permissions_data()->HasAPIPermission(
      mojom::APIPermissionID::kUnlimitedStorage));
}

}  // namespace
}  // namespace extensions
