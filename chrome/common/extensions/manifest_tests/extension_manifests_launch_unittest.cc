// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/common/extensions/manifest_tests/chrome_manifest_test.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

namespace errors = manifest_errors;
namespace keys = manifest_keys;

using AppLaunchManifestTest = ChromeManifestTest;

TEST_F(AppLaunchManifestTest, AppLaunchContainer) {
  scoped_refptr<Extension> extension = LoadAndExpectSuccess("launch_tab.json");
  EXPECT_EQ(apps::LaunchContainer::kLaunchContainerTab,
            AppLaunchInfo::GetLaunchContainer(extension.get()));

  extension = LoadAndExpectSuccess("launch_panel.json");
  EXPECT_EQ(apps::LaunchContainer::kLaunchContainerPanelDeprecated,
            AppLaunchInfo::GetLaunchContainer(extension.get()));
  EXPECT_EQ(1u, extension->web_extent().size());
  URLPattern pattern(Extension::kValidWebExtentSchemes, "*://www.google.com/*");
  EXPECT_TRUE(extension->web_extent().ContainsPattern(pattern));

  extension = LoadAndExpectSuccess("launch_default.json");
  EXPECT_EQ(apps::LaunchContainer::kLaunchContainerTab,
            AppLaunchInfo::GetLaunchContainer(extension.get()));

  extension = LoadAndExpectSuccess("launch_width.json");
  EXPECT_EQ(640, AppLaunchInfo::GetLaunchWidth(extension.get()));

  extension = LoadAndExpectSuccess("launch_height.json");
  EXPECT_EQ(480, AppLaunchInfo::GetLaunchHeight(extension.get()));

  const Testcase testcases[] = {
      Testcase("launch_window.json", errors::kInvalidLaunchContainer),
      Testcase("launch_container_invalid_type.json",
               errors::kInvalidLaunchContainer),
      Testcase("launch_container_invalid_value.json",
               errors::kInvalidLaunchContainer),
      Testcase("launch_container_without_launch_url.json",
               errors::kLaunchURLRequired),
      Testcase("launch_width_invalid.json",
               ErrorUtils::FormatErrorMessage(
                   errors::kInvalidLaunchValueContainer, keys::kLaunchWidth)),
      Testcase("launch_width_negative.json",
               ErrorUtils::FormatErrorMessage(errors::kInvalidLaunchValue,
                                              keys::kLaunchWidth)),
      Testcase("launch_height_invalid.json",
               ErrorUtils::FormatErrorMessage(
                   errors::kInvalidLaunchValueContainer, keys::kLaunchHeight)),
      Testcase("launch_height_negative.json",
               ErrorUtils::FormatErrorMessage(errors::kInvalidLaunchValue,
                                              keys::kLaunchHeight))};
  RunTestcases(testcases, ExpectType::kError);
}

TEST_F(AppLaunchManifestTest, AppLaunchURL) {
  const Testcase testcases[] = {
      Testcase("launch_path_and_url.json",
               errors::kLaunchPathAndURLAreExclusive),
      Testcase("launch_path_and_extent.json",
               errors::kLaunchPathAndExtentAreExclusive),
      Testcase("launch_path_invalid_type.json",
               ErrorUtils::FormatErrorMessage(errors::kInvalidLaunchValue,
                                              keys::kLaunchLocalPath)),
      Testcase("launch_path_invalid_value.json",
               ErrorUtils::FormatErrorMessage(errors::kInvalidLaunchValue,
                                              keys::kLaunchLocalPath)),
      Testcase("launch_path_invalid_localized.json",
               ErrorUtils::FormatErrorMessage(errors::kInvalidLaunchValue,
                                              keys::kLaunchLocalPath)),
      Testcase("launch_url_invalid_type_1.json",
               ErrorUtils::FormatErrorMessage(errors::kInvalidLaunchValue,
                                              keys::kLaunchWebURL)),
      Testcase("launch_url_invalid_type_2.json",
               ErrorUtils::FormatErrorMessage(errors::kInvalidLaunchValue,
                                              keys::kLaunchWebURL)),
      Testcase("launch_url_invalid_type_3.json",
               ErrorUtils::FormatErrorMessage(errors::kInvalidLaunchValue,
                                              keys::kLaunchWebURL)),
      Testcase("launch_url_invalid_localized.json",
               ErrorUtils::FormatErrorMessage(errors::kInvalidLaunchValue,
                                              keys::kLaunchWebURL))};
  RunTestcases(testcases, ExpectType::kError);

  scoped_refptr<Extension> extension =
      LoadAndExpectSuccess("launch_local_path.json");
  EXPECT_EQ(extension->url().spec() + "launch.html",
            AppLaunchInfo::GetFullLaunchURL(extension.get()).spec());

  extension = LoadAndExpectSuccess("launch_local_path_localized.json");
  EXPECT_EQ(extension->url().spec() + "launch.html",
            AppLaunchInfo::GetFullLaunchURL(extension.get()).spec());

  LoadAndExpectError("launch_web_url_relative.json",
                     ErrorUtils::FormatErrorMessage(
                         errors::kInvalidLaunchValue,
                         keys::kLaunchWebURL));

  extension = LoadAndExpectSuccess("launch_web_url_absolute.json");
  EXPECT_EQ(GURL("http://www.google.com/launch.html"),
            AppLaunchInfo::GetFullLaunchURL(extension.get()));
  EXPECT_EQ(1u, extension->web_extent().size());
  URLPattern pattern(Extension::kValidWebExtentSchemes, "*://www.google.com/*");
  EXPECT_TRUE(extension->web_extent().ContainsPattern(pattern));

  extension = LoadAndExpectSuccess("launch_web_url_localized.json");
  EXPECT_EQ(GURL("http://www.google.com/launch.html"),
            AppLaunchInfo::GetFullLaunchURL(extension.get()));
  EXPECT_EQ(1u, extension->web_extent().size());
  EXPECT_TRUE(extension->web_extent().ContainsPattern(pattern));
}

// If Chrome App has a valid URL in "app.urls" manifest key, then
// "app.launch.web_url" does not impact Extension::web_extent().
// This test ensures that "app.urls" is parsed before "app.launch.web_url",
// so "app.urls" gets into Extension::web_extent() before call to
// AppLaunchInfo::Parse() which calls AppLaunchInfo::LoadLaunchURL()
// which calls Extension::web_extent().is_empty().
TEST_F(AppLaunchManifestTest, AppURLsAndAppLaunchWebURL) {
  static constexpr char kManifest[] = R"({
         "name": "No extra WebExtent",
         "manifest_version": 2,
         "version": "1",
         "app": {
           "urls": [
             "https://www.gmail.com/"
           ],
           "launch": {
             "web_url": "https://www.google.com/launch.html"
           }
         }
       })";
  scoped_refptr<Extension> extension =
      LoadAndExpectSuccess(ManifestData::FromJSON(kManifest));
  EXPECT_EQ(GURL("https://www.google.com/launch.html"),
            AppLaunchInfo::GetFullLaunchURL(extension.get()));
  EXPECT_EQ(1u, extension->web_extent().size());
  EXPECT_TRUE(extension->web_extent().ContainsPattern(URLPattern(
      Extension::kValidWebExtentSchemes, "https://www.gmail.com/*")));
}

}  // namespace extensions
