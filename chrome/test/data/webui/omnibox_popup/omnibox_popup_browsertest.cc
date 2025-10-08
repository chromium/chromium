// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "build/config/coverage/buildflags.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "components/omnibox/common/omnibox_features.h"
#include "content/public/test/browser_test.h"

class OmniboxPopupBrowserTest : public WebUIMochaBrowserTest {
 protected:
  OmniboxPopupBrowserTest() {
    set_test_loader_host(chrome::kChromeUIOmniboxPopupHost);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      omnibox::kWebUIOmniboxPopup};
};

#if BUILDFLAG(USE_JAVASCRIPT_COVERAGE)
// TODO(crbug.com/40284073): Test fails with JS coverage turned on.
#define MAYBE_App DISABLED_App
#else
#define MAYBE_App App
#endif
typedef OmniboxPopupBrowserTest OmniboxPopupTest;
IN_PROC_BROWSER_TEST_F(OmniboxPopupTest, MAYBE_App) {
  RunTest("omnibox_popup/app_test.js", "mocha.run();");
}
