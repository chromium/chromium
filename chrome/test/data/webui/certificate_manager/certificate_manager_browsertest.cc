// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_features.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"

typedef WebUIMochaBrowserTest CrComponentsTest;

class CertficateManagerV2Test : public WebUIMochaBrowserTest {
 protected:
  CertficateManagerV2Test() {
    set_test_loader_host(chrome::kChromeUICertificateManagerHost);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kEnableCertManagementUIV2};
};

IN_PROC_BROWSER_TEST_F(CertficateManagerV2Test, Load) {
  RunTestWithoutTestLoader("certificate_manager/certificate_manager_test.js",
                           "mocha.run()");
}
