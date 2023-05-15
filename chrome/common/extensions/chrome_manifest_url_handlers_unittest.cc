// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/scoped_temp_dir.h"
#include "base/values.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/file_util.h"
#include "extensions/common/manifest_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

TEST(ChromeURLOverridesHandlerTest, TestFileMissing) {
  auto manifest = base::Value::Dict()
                      .Set("name", "ntp override")
                      .Set("version", "0.1")
                      .Set("manifest_version", 2)
                      .Set("description", "description")
                      .Set("chrome_url_overrides",
                           base::Value::Dict().Set("newtab", "newtab.html"));
  std::string error;
  std::vector<InstallWarning> warnings;
  base::ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  scoped_refptr<Extension> extension =
      Extension::Create(dir.GetPath(), mojom::ManifestLocation::kInternal,
                        manifest, Extension::NO_FLAGS, std::string(), &error);
  ASSERT_TRUE(extension);
  EXPECT_FALSE(
      file_util::ValidateExtension(extension.get(), &error, &warnings));
  EXPECT_EQ(ErrorUtils::FormatErrorMessage(manifest_errors::kFileNotFound,
                                           "newtab.html"),
            error);
}

}  // namespace extensions
