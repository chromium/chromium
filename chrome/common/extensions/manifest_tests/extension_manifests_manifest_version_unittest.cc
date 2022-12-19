// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "chrome/common/extensions/manifest_tests/chrome_manifest_test.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/manifest_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using extensions::Extension;

namespace errors = extensions::manifest_errors;

TEST_F(ChromeManifestTest, ManifestVersionError) {
  base::Value::Dict mv_missing;
  mv_missing.Set("name", "Miles");
  mv_missing.Set("version", "0.55");

  base::Value::Dict mv0 = mv_missing.Clone();
  mv0.Set("manifest_version", 0);

  base::Value::Dict mv1 = mv_missing.Clone();
  mv1.Set("manifest_version", 1);

  base::Value::Dict mv2 = mv_missing.Clone();
  mv2.Set("manifest_version", 2);

  base::Value::Dict mv3 = mv_missing.Clone();
  mv3.Set("manifest_version", 3);

  base::Value::Dict mv4 = mv_missing.Clone();
  mv4.Set("manifest_version", 4);

  base::Value::Dict mv_string = mv_missing.Clone();
  mv_string.Set("manifest_version", "2");

  struct {
    const char* test_name;
    bool require_modern_manifest_version;
    base::Value::Dict manifest;
    std::string expected_error;
  } test_data[] = {
      {"require_modern_with_default", true, mv_missing.Clone(),
       errors::kInvalidManifestVersionMissingKey},
      {"require_modern_with_invalid_version", true, mv0.Clone(),
       errors::kInvalidManifestVersionUnsupported},
      {"require_modern_with_old_version", true, mv1.Clone(),
       errors::kInvalidManifestVersionUnsupported},
      {"require_modern_with_v2", true, mv2.Clone(), ""},
      {"require_modern_with_v3", true, mv3.Clone(), ""},
      {"require_modern_with_future_version", true, mv4.Clone(), ""},
      {"require_modern_with_string", true, mv_string.Clone(),
       errors::kInvalidManifestVersionUnsupported},
      {"dont_require_modern_with_default", false, mv_missing.Clone(),
       errors::kInvalidManifestVersionMissingKey},
      {"dont_require_modern_with_invalid_version", false, mv0.Clone(),
       errors::kInvalidManifestVersionUnsupported},
      {"dont_require_modern_with_old_version", false, mv1.Clone(),
       errors::kInvalidManifestVersionUnsupported},
      {"dont_require_modern_with_v2", false, mv2.Clone(), ""},
      {"dont_require_modern_with_v3", false, mv3.Clone(), ""},
      {"dont_require_modern_with_future_version", false, mv4.Clone(), ""},
      {"dont_require_modern_with_string", false, mv_string.Clone(),
       errors::kInvalidManifestVersionUnsupported},
  };

  for (auto& entry : test_data) {
    int create_flags = Extension::NO_FLAGS;
    if (entry.require_modern_manifest_version)
      create_flags |= Extension::REQUIRE_MODERN_MANIFEST_VERSION;
    if (!entry.expected_error.empty()) {
      LoadAndExpectError(
          ManifestData(std::move(entry.manifest), entry.test_name),
          extensions::ErrorUtils::FormatErrorMessage(
              entry.expected_error, "either 2 or 3", "extensions"),
          extensions::mojom::ManifestLocation::kUnpacked, create_flags);
    } else {
      LoadAndExpectSuccess(
          ManifestData(std::move(entry.manifest), entry.test_name),
          extensions::mojom::ManifestLocation::kUnpacked, create_flags);
    }
  }
}
