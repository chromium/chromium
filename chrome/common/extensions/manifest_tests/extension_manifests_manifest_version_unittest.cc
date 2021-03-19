// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "chrome/common/extensions/manifest_tests/chrome_manifest_test.h"
#include "extensions/common/manifest_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using extensions::Extension;

namespace errors = extensions::manifest_errors;

TEST_F(ChromeManifestTest, ManifestVersionError) {
  base::Value manifest1(base::Value::Type::DICTIONARY);
  manifest1.SetStringKey("name", "Miles");
  manifest1.SetStringKey("version", "0.55");

  base::Value manifest2 = manifest1.Clone();
  manifest2.SetIntKey("manifest_version", 1);

  base::Value manifest3 = manifest1.Clone();
  manifest3.SetIntKey("manifest_version", 2);

  struct {
    const char* test_name;
    bool require_modern_manifest_version;
    base::Value manifest;
    bool expect_error;
  } test_data[] = {
      {"require_modern_with_default", true, manifest1.Clone(), true},
      {"require_modern_with_v1", true, manifest2.Clone(), true},
      {"require_modern_with_v2", true, manifest3.Clone(), false},
      {"dont_require_modern_with_default", false, manifest1.Clone(), true},
      {"dont_require_modern_with_v1", false, manifest2.Clone(), true},
      {"dont_require_modern_with_v2", false, manifest3.Clone(), false},
  };

  for (auto& entry : test_data) {
    int create_flags = Extension::NO_FLAGS;
    if (entry.require_modern_manifest_version)
      create_flags |= Extension::REQUIRE_MODERN_MANIFEST_VERSION;
    if (entry.expect_error) {
      LoadAndExpectError(
          ManifestData(std::move(entry.manifest), entry.test_name),
          errors::kInvalidManifestVersionOld,
          extensions::mojom::ManifestLocation::kUnpacked, create_flags);
    } else {
      LoadAndExpectSuccess(
          ManifestData(std::move(entry.manifest), entry.test_name),
          extensions::mojom::ManifestLocation::kUnpacked, create_flags);
    }
  }
}
