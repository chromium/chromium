// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/test_support/glic_test_environment.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"

class GlicSelectionOverlayWebUIBrowserTest : public WebUIMochaBrowserTest {
 protected:
  GlicSelectionOverlayWebUIBrowserTest() {
    set_test_loader_scheme(content::kChromeUIUntrustedScheme);
    set_test_loader_host(chrome::kChromeUIGlicUntrustedHost);
  }

 private:
  glic::GlicTestEnvironment glic_test_env_;
};

IN_PROC_BROWSER_TEST_F(GlicSelectionOverlayWebUIBrowserTest,
                       InlineFulfillmentHost) {
  RunTest("glic/selection_overlay/inline_fulfillment_host_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(GlicSelectionOverlayWebUIBrowserTest,
                       ExplainFulfillment) {
  RunTest("glic/selection_overlay/explain_fulfillment_test.js", "mocha.run()");
}
