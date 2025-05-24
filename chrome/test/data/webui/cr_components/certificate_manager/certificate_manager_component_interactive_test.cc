// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"

typedef WebUIMochaFocusTest CrComponentsFocusTest;

class CrComponentsCertManagerV2FocusTest : public WebUIMochaBrowserTest {
 protected:
  CrComponentsCertManagerV2FocusTest() {
    set_test_loader_host(chrome::kChromeUICertificateManagerHost);
  }
};
IN_PROC_BROWSER_TEST_F(CrComponentsCertManagerV2FocusTest,
                       CertificateManagerV2) {
  RunTest(
      "cr_components/certificate_manager/certificate_manager_v2_focus_test.js",
      "mocha.run()");
}
