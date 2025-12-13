// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <utility>

#include "base/values.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/common/extensions/manifest_tests/chrome_manifest_test.h"
#include "extensions/buildflags/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {
namespace {

using ValidAppManifestTest = ChromeManifestTest;

TEST_F(ValidAppManifestTest, ValidApp) {
  scoped_refptr<Extension> extension(LoadAndExpectSuccess("valid_app.json"));
  URLPatternSet expected_patterns;
  AddPattern(&expected_patterns, "http://www.google.com/mail/*");
  AddPattern(&expected_patterns, "http://www.google.com/foobar/*");
  EXPECT_EQ(expected_patterns, extension->web_extent());
  EXPECT_EQ(apps::LaunchContainer::kLaunchContainerTab,
            AppLaunchInfo::GetLaunchContainer(extension.get()));
  EXPECT_EQ(GURL("http://www.google.com/mail/"),
            AppLaunchInfo::GetLaunchWebURL(extension.get()));
}

TEST_F(ValidAppManifestTest, AllowUnrecognizedPermissions) {
  std::string error;
  std::optional<base::Value::Dict> manifest =
      LoadManifest("valid_app.json", &error);
  ASSERT_TRUE(manifest);
  base::Value::List* permissions = manifest->FindList("permissions");
  ASSERT_TRUE(permissions);
  permissions->Append("not-a-valid-permission");
  LoadAndExpectSuccess(ManifestData(std::move(*manifest), ""));
}

}  // namespace
}  // namespace extensions
