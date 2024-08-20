// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"

typedef WebUIMochaFocusTest CrComponentsFocusTest;

#if BUILDFLAG(CHROME_ROOT_STORE_CERT_MANAGEMENT_UI)
class CrComponentsCertManagerV2FocusTest : public WebUIMochaBrowserTest {
 protected:
  CrComponentsCertManagerV2FocusTest() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kEnableCertManagementUIV2);
    set_test_loader_host(chrome::kChromeUICertificateManagerHost);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};
IN_PROC_BROWSER_TEST_F(CrComponentsCertManagerV2FocusTest,
                       CertificateManagerV2) {
  RunTest(
      "cr_components/certificate_manager/certificate_manager_v2_focus_test.js",
      "mocha.run()");
}
#endif  // BUILDFLAG(CHROME_ROOT_STORE_CERT_MANAGEMENT_UI)
