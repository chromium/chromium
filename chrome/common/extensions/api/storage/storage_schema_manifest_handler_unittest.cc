// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/api/storage/storage_schema_manifest_handler.h"

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/values.h"
#include "components/version_info/version_info.h"
#include "extensions/common/extension.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/common/file_util.h"
#include "extensions/common/manifest.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

class StorageSchemaManifestHandlerTest : public testing::Test {
 public:
  StorageSchemaManifestHandlerTest()
      : scoped_channel_(version_info::Channel::DEV) {}

  ~StorageSchemaManifestHandlerTest() override {}

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    manifest_.Set("name", "test");
    manifest_.Set("version", "1.2.3.4");
    manifest_.Set("manifest_version", 2);
  }

  scoped_refptr<Extension> CreateExtension(const std::string& schema) {
    std::string error;
    scoped_refptr<Extension> extension = Extension::Create(
        temp_dir_.GetPath(), mojom::ManifestLocation::kUnpacked, manifest_,
        Extension::NO_FLAGS, "", &error);
    if (!extension.get())
      return nullptr;
    base::FilePath schema_path = temp_dir_.GetPath().AppendASCII("schema.json");
    if (schema.empty()) {
      base::DeleteFile(schema_path);
    } else {
      if (!base::WriteFile(schema_path, schema)) {
        return nullptr;
      }
    }
    return extension;
  }

  testing::AssertionResult Validates(const std::string& schema) {
    scoped_refptr<Extension> extension = CreateExtension(schema);
    if (!extension.get())
      return testing::AssertionFailure() << "Failed to create test extension";
    std::string error;
    std::vector<InstallWarning> warnings;
    if (file_util::ValidateExtension(extension.get(), &error, &warnings))
      return testing::AssertionSuccess();
    return testing::AssertionFailure() << error;
  }

  base::ScopedTempDir temp_dir_;
  ScopedCurrentChannel scoped_channel_;
  base::Value::Dict manifest_;
};

TEST_F(StorageSchemaManifestHandlerTest, Parse) {
  scoped_refptr<Extension> extension = CreateExtension("");
  ASSERT_TRUE(extension.get());

  // Not a string.
  manifest_.SetByDottedPath("storage.managed_schema", 123);
  extension = CreateExtension("");
  EXPECT_FALSE(extension.get());

  // All good now.
  manifest_.SetByDottedPath("storage.managed_schema", "schema.json");
  extension = CreateExtension("");
  ASSERT_TRUE(extension.get());
}

TEST_F(StorageSchemaManifestHandlerTest, Validate) {
  base::Value::List permissions;
  permissions.Append("storage");
  manifest_.Set("permissions", std::move(permissions));

  // Absolute path.
  manifest_.SetByDottedPath("storage.managed_schema", "/etc/passwd");
  EXPECT_FALSE(Validates(""));

  // Path with ..
  manifest_.SetByDottedPath("storage.managed_schema",
                            "../../../../../etc/passwd");
  EXPECT_FALSE(Validates(""));

  // Does not exist.
  manifest_.SetByDottedPath("storage.managed_schema", "not-there");
  EXPECT_FALSE(Validates(""));

  // Invalid JSON.
  manifest_.SetByDottedPath("storage.managed_schema", "schema.json");
  EXPECT_FALSE(Validates("-invalid-"));

  // No version.
  EXPECT_FALSE(Validates("{}"));

  // Invalid version.
  EXPECT_FALSE(Validates(
      "{"
      "  \"$schema\": \"http://json-schema.org/draft-42/schema#\""
      "}"));

  // Missing type.
  EXPECT_FALSE(Validates(
      "{"
      "  \"$schema\": \"http://json-schema.org/draft-03/schema#\""
      "}"));

  // Invalid type.
  EXPECT_FALSE(Validates(
      "{"
      "  \"$schema\": \"http://json-schema.org/draft-03/schema#\","
      "  \"type\": \"string\""
      "}"));

  // "additionalProperties" not supported at top level.
  EXPECT_FALSE(Validates(
      "{"
      "  \"$schema\": \"http://json-schema.org/draft-03/schema#\","
      "  \"type\": \"object\","
      "  \"additionalProperties\": {}"
      "}"));

  // All good now.
  EXPECT_TRUE(Validates(
      "{"
      "  \"$schema\": \"http://json-schema.org/draft-03/schema#\","
      "  \"type\": \"object\""
      "}"));
}

}  // namespace extensions
