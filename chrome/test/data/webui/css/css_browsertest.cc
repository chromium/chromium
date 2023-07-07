// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"

#if !BUILDFLAG(IS_LINUX)
#include "ui/base/ui_base_features.h"
#endif

using TextDefaultsTest = WebUIMochaBrowserTest;
IN_PROC_BROWSER_TEST_F(TextDefaultsTest, All) {
  RunTest("css/text_defaults_test.js", "runMochaSuite('TextDefaults')");
}

#if !BUILDFLAG(IS_LINUX)
class TextDefaultsNoSystemFontTest : public WebUIMochaBrowserTest {
 protected:
  TextDefaultsNoSystemFontTest() {
    scoped_feature_list_.InitAndDisableFeature(features::kWebUiSystemFont);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(TextDefaultsNoSystemFontTest, All) {
  RunTest("css/text_defaults_test.js",
          "runMochaSuite('TextDefaultsNoSystemFont')");
}
#endif

using ColorProviderCSSColorsTest = WebUIMochaBrowserTest;

IN_PROC_BROWSER_TEST_F(ColorProviderCSSColorsTest, All) {
  RunTest("css/color_provider_css_colors_test.js", "mocha.run()");
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(ColorProviderCSSColorsTest, ChromeOS) {
  RunTest("css/color_provider_css_colors_test_chromeos.js", "mocha.run()");
}
#endif
