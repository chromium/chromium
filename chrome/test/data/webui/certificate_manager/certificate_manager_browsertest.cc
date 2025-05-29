// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"

class CertficateManagerTest : public WebUIMochaBrowserTest {
 protected:
  CertficateManagerTest() {
    set_test_loader_host(chrome::kChromeUICertificateManagerHost);
  }
};

IN_PROC_BROWSER_TEST_F(CertficateManagerTest, Load) {
  RunTestWithoutTestLoader("certificate_manager/certificate_manager_test.js",
                           "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CertficateManagerTest, CertificateManager) {
  RunTest("certificate_manager/certificate_manager_v2_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CertficateManagerTest, CertificateList) {
  RunTest("certificate_manager/certificate_list_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CertficateManagerTest, CertificateEntry) {
  RunTest("certificate_manager/certificate_entry_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CertficateManagerTest, CertificateSubpage) {
  RunTest("certificate_manager/certificate_subpage_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CertficateManagerTest, CertificatePasswordDialog) {
  RunTest("certificate_manager/certificate_password_dialog_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CertficateManagerTest, LocalCerts) {
  RunTest("certificate_manager/local_certs_section_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CertficateManagerTest, Navigation) {
  RunTest("certificate_manager/navigation_test.js", "mocha.run()");
}

#if BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(CertficateManagerTest, CertificateManagerProvisioning) {
  RunTest("certificate_manager/certificate_manager_provisioning_test.js",
          "mocha.run()");
}
#endif  // BUILDFLAG(IS_CHROMEOS)
