// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/api/side_panel/side_panel_info.h"

#include "base/strings/stringprintf.h"
#include "base/test/values_test_util.h"
#include "chrome/common/extensions/manifest_tests/chrome_manifest_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/common/manifest.h"
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
    base::Value manifest_value = base::test::ParseJson(base::StringPrintf(
        kManifestStub, manifest_version, side_panel.c_str()));
    EXPECT_EQ(base::Value::Type::DICTIONARY, manifest_value.type());
    return ManifestData(std::move(manifest_value), "test");
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
  scoped_refptr<Extension> CreateExtension(const base::Value::Dict& manifest) {
    base::Value::Dict manifest_base;
    manifest_base.Set("name", "test");
    manifest_base.Set("version", "1.0");
    manifest_base.Set("manifest_version", 3);
    manifest_base.Merge(manifest.Clone());
    std::string error;
    scoped_refptr<Extension> extension = Extension::Create(
        temp_dir_.GetPath(), mojom::ManifestLocation::kUnpacked, manifest_base,
        Extension::NO_FLAGS, "", &error);
    if (!extension.get())
      return nullptr;
    return extension;
  }

 private:
  base::ScopedTempDir temp_dir_;
};

#if defined(OFFICIAL_BUILD)
#define MAYBE_FileDoesntExist DISABLED_FileDoesntExist
#else
#define MAYBE_FileDoesntExist FileDoesntExist
#endif  // defined(OFFICIAL_BUILD)
// Error loading extension when filepath doesn't exist or is empty.
TEST_F(SidePanelExtensionsTest, MAYBE_FileDoesntExist) {
  for (const auto* default_path : {"", "error"}) {
    std::string error;
    std::vector<InstallWarning> warnings;
    base::Value::Dict manifest;
    base::Value::Dict side_panel;
    side_panel.Set("default_path", default_path);
    manifest.Set("side_panel", base::Value(std::move(side_panel)));
    auto extension = CreateExtension(manifest);
    ManifestHandler::ValidateExtension(extension.get(), &error, &warnings);
    ASSERT_EQ("Side panel file path must exist.", error);
  }
}

}  // namespace extensions
