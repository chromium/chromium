// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/manifest_tests/chrome_manifest_test.h"

#include "testing/gtest/include/gtest/gtest.h"

TEST_F(ChromeManifestTest, PortsInPermissions) {
  // Loading as a user would shoud not trigger an error.
  LoadAndExpectSuccess("ports_in_permissions.json");
}
