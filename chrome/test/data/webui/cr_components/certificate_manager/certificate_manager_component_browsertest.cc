// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_features.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"
#include "crypto/crypto_buildflags.h"

class CrComponentsCertManagerV2Test : public WebUIMochaBrowserTest {
 protected:
  CrComponentsCertManagerV2Test() {
    set_test_loader_host(chrome::kChromeUICertificateManagerHost);
  }
};

#if BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(CrComponentsCertManagerV2Test,
                       CertificateManagerProvisioning) {
  RunTest(
      "cr_components/certificate_manager/"
      "certificate_manager_provisioning_test.js",
      "mocha.run()");
}
#endif  // BUILDFLAG(IS_CHROMEOS)

IN_PROC_BROWSER_TEST_F(CrComponentsCertManagerV2Test, CertificateManagerV2) {
  RunTest("cr_components/certificate_manager/certificate_manager_v2_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsCertManagerV2Test, CertificateListV2) {
  RunTest("cr_components/certificate_manager/certificate_list_v2_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsCertManagerV2Test, CertificateEntryV2) {
  RunTest("cr_components/certificate_manager/certificate_entry_v2_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsCertManagerV2Test, CertificateSubpageV2) {
  RunTest("cr_components/certificate_manager/certificate_subpage_v2_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsCertManagerV2Test,
                       CertificatePasswordDialog) {
  RunTest(
      "cr_components/certificate_manager/certificate_password_dialog_test.js",
      "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsCertManagerV2Test, LocalCertsV2) {
  RunTest("cr_components/certificate_manager/local_certs_section_v2_test.js",
          "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(CrComponentsCertManagerV2Test, NavigationV2) {
  RunTest("cr_components/certificate_manager/navigation_v2_test.js",
          "mocha.run()");
}

