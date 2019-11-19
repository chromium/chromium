// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/common/extensions/manifest_tests/chrome_manifest_test.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/options_page_info.h"
#include "testing/gtest/include/gtest/gtest.h"

using extensions::FeatureSwitch;
using extensions::OptionsPageInfo;

namespace {

class OptionsPageManifestTest : public ChromeManifestTest {
 protected:
  // Tests how the options_ui manifest key affects the open-in-tab and
  // chromes-style behaviour.
  testing::AssertionResult TestOptionsUIChromeStyleAndOpenInTab() {
    // Explicitly specifying true in the manifest for options_ui.chrome_style
    // and options_ui.open_in_tab sets them both to true.
    scoped_refptr<extensions::Extension> extension =
        LoadAndExpectSuccess("options_ui_flags_true.json");
    EXPECT_TRUE(OptionsPageInfo::ShouldUseChromeStyle(extension.get()));
    EXPECT_TRUE(OptionsPageInfo::ShouldOpenInTab(extension.get()));

    // Explicitly specifying false in the manifest for options_ui.chrome_style
    // and options_ui.open_in_tab sets them both to false.
    extension = LoadAndExpectSuccess("options_ui_flags_false.json");
    EXPECT_FALSE(OptionsPageInfo::ShouldUseChromeStyle(extension.get()));
    EXPECT_FALSE(OptionsPageInfo::ShouldOpenInTab(extension.get()));

    // Specifying an options_ui key but neither options_ui.chrome_style nor
    // options_ui.open_in_tab uses the default values: false for open-in-tab,
    // false for use-chrome-style.
    extension = LoadAndExpectSuccess("options_ui_page_basic.json");
    EXPECT_FALSE(OptionsPageInfo::ShouldUseChromeStyle(extension.get()));
    EXPECT_FALSE(OptionsPageInfo::ShouldOpenInTab(extension.get()));

    // This extension has both options_page and options_ui specified. The
    // options_ui key should take precedence.
    extension = LoadAndExpectSuccess("options_ui_page_with_legacy_page.json");
    EXPECT_FALSE(OptionsPageInfo::ShouldUseChromeStyle(extension.get()));
    EXPECT_FALSE(OptionsPageInfo::ShouldOpenInTab(extension.get()));

    return testing::AssertionSuccess();
  }

  // Tests how the options_page manifest key affects the open-in-tab and
  // chromes-style behaviour.
  testing::AssertionResult TestOptionsPageChromeStyleAndOpenInTab(
      bool expect_open_in_tab) {
    scoped_refptr<extensions::Extension> extension =
        LoadAndExpectSuccess("init_valid_options.json");
    EXPECT_FALSE(OptionsPageInfo::ShouldUseChromeStyle(extension.get()));
    if (expect_open_in_tab) {
      EXPECT_TRUE(OptionsPageInfo::ShouldOpenInTab(extension.get()));
    } else {
      EXPECT_FALSE(OptionsPageInfo::ShouldOpenInTab(extension.get()));
    }
    return testing::AssertionSuccess();
  }
};

TEST_F(OptionsPageManifestTest, OptionsPageInApps) {
  // Allow options page with absolute URL in hosted apps.
  scoped_refptr<extensions::Extension> extension =
      LoadAndExpectSuccess("hosted_app_absolute_options.json");
  EXPECT_EQ("http://example.com/options.html",
            OptionsPageInfo::GetOptionsPage(extension.get()).spec());

  extension = LoadAndExpectSuccess("platform_app_with_options_page.json");
  EXPECT_TRUE(!OptionsPageInfo::HasOptionsPage(extension.get()));

  Testcase testcases[] = {
      // Forbid options page with relative URL in hosted apps.
      Testcase("hosted_app_relative_options.json",
               extensions::manifest_errors::kInvalidOptionsPageInHostedApp),

      // Forbid options page with non-(http|https) scheme in hosted app.
      Testcase("hosted_app_file_options.json",
               extensions::manifest_errors::kInvalidOptionsPageInHostedApp),

      // Forbid absolute URL for options page in packaged apps.
      Testcase(
          "packaged_app_absolute_options.json",
          extensions::manifest_errors::kInvalidOptionsPageExpectUrlInPackage)};
  RunTestcases(testcases, base::size(testcases), EXPECT_TYPE_ERROR);
}

// Tests for the options_ui.page manifest field.
TEST_F(OptionsPageManifestTest, OptionsUIPage) {
  FeatureSwitch::ScopedOverride enable_flag(
      FeatureSwitch::embedded_extension_options(), true);

  scoped_refptr<extensions::Extension> extension =
      LoadAndExpectSuccess("options_ui_page_basic.json");
  EXPECT_EQ(base::StringPrintf("chrome-extension://%s/options.html",
                               extension->id().c_str()),
            OptionsPageInfo::GetOptionsPage(extension.get()).spec());

  extension = LoadAndExpectSuccess("options_ui_page_with_legacy_page.json");
  EXPECT_EQ(base::StringPrintf("chrome-extension://%s/newoptions.html",
                               extension->id().c_str()),
            OptionsPageInfo::GetOptionsPage(extension.get()).spec());

  Testcase testcases[] = {Testcase("options_ui_page_bad_url.json",
                                   "'page': expected page, got null")};
  RunTestcases(testcases, base::size(testcases), EXPECT_TYPE_WARNING);
}

// Runs TestOptionsUIChromeStyleAndOpenInTab with and without the
// embedded-extension-options flag. The results should always be the same.
TEST_F(OptionsPageManifestTest, OptionsUIChromeStyleAndOpenInTab) {
  ASSERT_FALSE(FeatureSwitch::embedded_extension_options()->IsEnabled());
  EXPECT_TRUE(TestOptionsUIChromeStyleAndOpenInTab());
  {
    FeatureSwitch::ScopedOverride enable_flag(
        FeatureSwitch::embedded_extension_options(), true);
    EXPECT_TRUE(TestOptionsUIChromeStyleAndOpenInTab());
  }
}

// Runs TestOptionsPageChromeStyleAndOpenInTab with and without the
// embedded-extension-options flag. The default value of open-in-tab differs
// depending on the flag's value.
TEST_F(OptionsPageManifestTest, OptionsPageChromeStyleAndOpenInTab) {
  ASSERT_FALSE(FeatureSwitch::embedded_extension_options()->IsEnabled());
  EXPECT_TRUE(TestOptionsPageChromeStyleAndOpenInTab(true));
  {
    FeatureSwitch::ScopedOverride enable_flag(
        FeatureSwitch::embedded_extension_options(), true);
    EXPECT_TRUE(TestOptionsPageChromeStyleAndOpenInTab(false));
  }
}

}  // namespace
