// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "components/compose/core/browser/compose_features.h"
#include "content/public/test/browser_test.h"

class ComposeTest : public WebUIMochaBrowserTest {
 protected:
  ComposeTest() { set_test_loader_host(chrome::kChromeUIComposeHost); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      compose::features::kEnableCompose};
};

IN_PROC_BROWSER_TEST_F(ComposeTest, App) {
  RunTest("compose/compose_app_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ComposeTest, Textarea) {
  RunTest("compose/compose_textarea_test.js", "mocha.run()");
}
