// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/scoped_temp_dir.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/file_util.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/value_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

TEST(ChromeURLOverridesHandlerTest, TestFileMissing) {
  DictionaryBuilder manifest;
  manifest.Set("name", "ntp override");
  manifest.Set("version", "0.1");
  manifest.Set("manifest_version", 2);
  manifest.Set("description", "description");
  manifest.Set("chrome_url_overrides",
               DictionaryBuilder().Set("newtab", "newtab.html").Build());
  base::Value::Dict manifest_value = manifest.BuildDict();
  std::string error;
  std::vector<InstallWarning> warnings;
  base::ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  scoped_refptr<Extension> extension = Extension::Create(
      dir.GetPath(), mojom::ManifestLocation::kInternal, manifest_value,
      Extension::NO_FLAGS, std::string(), &error);
  ASSERT_TRUE(extension);
  EXPECT_FALSE(
      file_util::ValidateExtension(extension.get(), &error, &warnings));
  EXPECT_EQ(ErrorUtils::FormatErrorMessage(manifest_errors::kFileNotFound,
                                           "newtab.html"),
            error);
}

}  // namespace extensions
