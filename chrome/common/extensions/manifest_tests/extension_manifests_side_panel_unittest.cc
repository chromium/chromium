// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/extensions/api/side_panel/side_panel_info.h"
#include "chrome/common/extensions/manifest_tests/chrome_manifest_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

class SidePanelManifestTest : public ChromeManifestTest {
 protected:
  ManifestData GetManifestData(const std::string& side_panel,
                               int manifest_version = 3) {
    constexpr char kManifestStub[] =
        R"({
        "name": "Test",
        "version": "1.0",
        "manifest_version": %d,
        "side_panel": %s
      })";
    return ManifestData::FromJSON(base::StringPrintf(
        kManifestStub, manifest_version, side_panel.c_str()));
  }
};

// Test presence of side_panel key in manifest.json.
TEST_F(SidePanelManifestTest, All) {
  // Succeed when side_panel.path is defined.
  {
    scoped_refptr<Extension> extension(LoadAndExpectSuccess(
        GetManifestData(R"({"default_path": "panel.html"})")));
    EXPECT_TRUE(SidePanelInfo::HasSidePanel(extension.get()));
  }

  // Error when side_panel.default_path type doesn't match.
  {
    std::string error =
        "Error at key 'side_panel.default_path'. Type is invalid. Expected "
        "string, found dictionary.";
    LoadAndExpectError(GetManifestData(R"({"default_path": {}})"), error);
  }

  // Error when side_panel type doesn't match.
  {
    std::string error =
        "Error at key 'side_panel'. Type is invalid. Expected dictionary, found"
        " string.";
    LoadAndExpectError(GetManifestData(R"("")"), error);
  }
}

class SidePanelExtensionsTest : public testing::Test {
 public:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

 protected:
  // Empty filepath doesn't exist test coverage.
  // TODO(crbug.com/41317803): Continue removing std::string error and
  // replacing with std::u16string.
  scoped_refptr<Extension> CreateExtension(const base::Value::Dict& manifest,
                                           std::string* error) {
    base::Value::Dict manifest_base;
    manifest_base.Set("name", "test");
    manifest_base.Set("version", "1.0");
    manifest_base.Set("manifest_version", 3);
    manifest_base.Merge(manifest.Clone());
    std::u16string utf16_error;
    scoped_refptr<Extension> extension = Extension::Create(
        temp_dir_.GetPath(), mojom::ManifestLocation::kUnpacked, manifest_base,
        Extension::NO_FLAGS, "", &utf16_error);
    *error = base::UTF16ToUTF8(utf16_error);
    return extension;
  }

 private:
  base::ScopedTempDir temp_dir_;
};

// Error loading extension when filepath is invalid.
TEST_F(SidePanelExtensionsTest, ValidateFileInvalid) {
  static constexpr struct {
    const char* relative_path;
  } test_cases[] = {
      {""},
      {"?"},
      {"dir/"},
      {"https://example.com"},
  };

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(test_case.relative_path);

    base::Value::Dict side_panel;
    side_panel.Set("default_path", test_case.relative_path);
    base::Value::Dict manifest;
    manifest.Set("side_panel", base::Value(std::move(side_panel)));

    std::string error;
    auto extension = CreateExtension(manifest, &error);
    ASSERT_FALSE(extension);
    ASSERT_EQ(base::UTF16ToUTF8(
                  manifest_errors::kSidePanelManifestDefaultPathInvalid),
              error);
  }
}

// Error loading extension when filepath doesn't exist.
TEST_F(SidePanelExtensionsTest, ValidateFileDoesntExist) {
  base::Value::Dict side_panel;
  side_panel.Set("default_path", "does_not_exist.html");
  base::Value::Dict manifest;
  manifest.Set("side_panel", base::Value(std::move(side_panel)));

  std::string error;
  auto extension = CreateExtension(manifest, &error);
  ASSERT_TRUE(extension);

  std::vector<InstallWarning> warnings;
  ManifestHandler::ValidateExtension(extension.get(), &error, &warnings);
  ASSERT_EQ(manifest_errors::kSidePanelManifestDefaultPathDoesNotExist, error);
}

}  // namespace extensions
