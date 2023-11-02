// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/manifest_tests/chrome_manifest_test.h"
#include "extensions/common/manifest_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

class FileSystemProviderHandlerTest : public ChromeManifestTest {};

TEST_F(FileSystemProviderHandlerTest, Valid) {
  RunTestcase(Testcase("filesystemprovider_valid.json"), EXPECT_TYPE_SUCCESS);
}

TEST_F(FileSystemProviderHandlerTest, Invalid_MissingCapabilities) {
  RunTestcase(
      Testcase("filesystemprovider_missing_capabilities.json",
               manifest_errors::kInvalidFileSystemProviderMissingCapabilities),
      EXPECT_TYPE_ERROR);
}

TEST_F(FileSystemProviderHandlerTest, Invalid_MissingPermission) {
  RunTestcase(
      Testcase("filesystemprovider_missing_permission.json",
               manifest_errors::kInvalidFileSystemProviderMissingPermission),
      EXPECT_TYPE_WARNING);
}

}  // namespace extensions
