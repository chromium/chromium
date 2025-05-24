// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/compose/compose_enabling.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "components/compose/core/browser/compose_features.h"
#include "content/public/test/browser_test.h"

class ComposeTest : public WebUIMochaBrowserTest,
                    public testing::WithParamInterface<bool> {
 public:
  static std::string DescribeParams(
      const testing::TestParamInfo<ParamType>& info) {
    return info.param ? "InputModesUI" : "LegacyUI";
  }

 protected:
  ComposeTest() {
    set_test_loader_host(chrome::kChromeUIUntrustedComposeHost);
    set_test_loader_scheme(content::kChromeUIUntrustedScheme);
    scoped_compose_enabled_ = ComposeEnabling::ScopedEnableComposeForTesting();
    scoped_feature_list_.InitWithFeatureStates(
        {{compose::features::kEnableCompose, true},
         {compose::features::kComposeUpfrontInputModes, GetParam()}});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  ComposeEnabling::ScopedOverride scoped_compose_enabled_;
};

INSTANTIATE_TEST_SUITE_P(/*no prefix*/,
                         ComposeTest,
                         ::testing::Bool(),
                         ComposeTest::DescribeParams);

IN_PROC_BROWSER_TEST_P(ComposeTest, App) {
  RunTest("compose/compose_app_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_P(ComposeTest, Textarea) {
  RunTest("compose/compose_textarea_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_P(ComposeTest, Animator) {
  RunTest("compose/compose_animator_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_P(ComposeTest, WordStreamer) {
  RunTest("compose/word_streamer_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_P(ComposeTest, ResultText) {
  RunTest("compose/result_text_test.js", "mocha.run()");
}
