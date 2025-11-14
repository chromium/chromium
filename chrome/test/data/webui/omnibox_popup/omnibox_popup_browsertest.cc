// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "build/config/coverage/buildflags.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "components/omnibox/common/omnibox_features.h"
#include "content/public/test/browser_test.h"

class OmniboxPopupTest : public WebUIMochaBrowserTest {
 protected:
  OmniboxPopupTest() {
    set_test_loader_host(chrome::kChromeUIOmniboxPopupHost);
    scoped_feature_list_.InitWithFeatures({omnibox::kWebUIOmniboxPopup},
                                          {omnibox::kWebUIOmniboxFullPopup});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

#if BUILDFLAG(USE_JAVASCRIPT_COVERAGE)
// TODO(crbug.com/40284073): Test fails with JS coverage turned on.
#define MAYBE_App DISABLED_App
#else
#define MAYBE_App App
#endif
IN_PROC_BROWSER_TEST_F(OmniboxPopupTest, MAYBE_App) {
  RunTest("omnibox_popup/app_test.js", "mocha.run();");
}

class OmniboxPopupFullTest : public WebUIMochaBrowserTest {
 protected:
  OmniboxPopupFullTest() {
    set_test_loader_host(chrome::kChromeUIOmniboxPopupHost);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      omnibox::kWebUIOmniboxFullPopup};
};

#if BUILDFLAG(USE_JAVASCRIPT_COVERAGE)
// TODO(crbug.com/40284073): Test fails with JS coverage turned on.
#define MAYBE_App DISABLED_App
#else
#define MAYBE_App App
#endif
IN_PROC_BROWSER_TEST_F(OmniboxPopupFullTest, MAYBE_App) {
  RunTest("omnibox_popup/full_app_test.js", "mocha.run();");
}

class OmniboxPopupAimTest : public WebUIMochaBrowserTest {
 protected:
  OmniboxPopupAimTest() {
    set_test_loader_host(chrome::kChromeUIOmniboxPopupHost);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      omnibox::internal::kWebUIOmniboxAimPopup};
};

#if BUILDFLAG(USE_JAVASCRIPT_COVERAGE)
// TODO(crbug.com/40284073): Test fails with JS coverage turned on.
#define MAYBE_App DISABLED_App
#else
#define MAYBE_App App
#endif
IN_PROC_BROWSER_TEST_F(OmniboxPopupAimTest, MAYBE_App) {
  RunTest("omnibox_popup/aim_app_test.js", "mocha.run();");
}
