// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "components/omnibox/common/omnibox_features.h"
#include "content/public/test/browser_test.h"

class OmniboxEverywhereWebUITest : public WebUIMochaBrowserTest {
 protected:
  OmniboxEverywhereWebUITest() {
    set_test_loader_host(chrome::kChromeUIOmniboxEverywhereHost);
    scoped_feature_list_.InitAndEnableFeature(omnibox::kOmniboxEverywhere);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(OmniboxEverywhereWebUITest, TabSelectionAndRestoration) {
  RunTest("omnibox_everywhere/omnibox_test.js", "mocha.run();");
}
