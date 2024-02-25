// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/compose/compose_enabling.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "components/compose/core/browser/compose_features.h"
#include "content/public/test/browser_test.h"

class ComposeFocusTest : public WebUIMochaFocusTest {
 protected:
  ComposeFocusTest() {
    set_test_loader_host(chrome::kChromeUIUntrustedComposeHost);
    set_test_loader_scheme(content::kChromeUIUntrustedScheme);
    scoped_compose_enabled_ = ComposeEnabling::ScopedEnableComposeForTesting();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      compose::features::kEnableCompose};
  ComposeEnabling::ScopedOverride scoped_compose_enabled_;
};

IN_PROC_BROWSER_TEST_F(ComposeFocusTest, App) {
  RunTest("compose/compose_app_focus_test.js", "mocha.run()");
}
