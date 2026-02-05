// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "build/config/coverage/buildflags.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "components/omnibox/browser/aim_eligibility_service_features.h"
#include "components/omnibox/common/omnibox_features.h"
#include "content/public/test/browser_test.h"

class OmniboxPopupTest : public WebUIMochaBrowserTest {
 protected:
  OmniboxPopupTest() {
    set_test_loader_host(chrome::kChromeUIOmniboxPopupHost);
    scoped_feature_list_.InitWithFeatures(
        {omnibox::kWebUIOmniboxPopup},
        {omnibox::kWebUIOmniboxFullPopup, omnibox::kAimUsePecApi});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(OmniboxPopupTest, App) {
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

IN_PROC_BROWSER_TEST_F(OmniboxPopupFullTest, App) {
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

IN_PROC_BROWSER_TEST_F(OmniboxPopupAimTest, App) {
  RunTest("omnibox_popup/aim_app_test.js", "mocha.run();");
}
