// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "chrome/browser/certificate_viewer.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/certificate_viewer_webui.h"
#include "chrome/browser/ui/webui/constrained_web_dialog_ui.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/cert/x509_util.h"
#include "net/test/test_certificate_data.h"

// Test framework for
// chrome/test/data/webui/certificate_viewer_dialog_browsertest.js.
class CertificateViewerUITest : public WebUIMochaBrowserTest {
 protected:
  CertificateViewerUITest() {
    // Set a loadable URL so that loading the initial tab doesn't fail.
    // Certificate viewer tests run in a constrained dialog containing the
    // certificate viewer WebContents that is on top of this initial tab.
    set_test_loader_host(chrome::kChromeUIDefaultHost);
  }

  virtual std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> GetCerts() {
    std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> certs;
    certs.push_back(
        net::x509_util::CreateCryptoBuffer(base::make_span(google_der)));
    return certs;
  }

  void RunTestCase(const std::string& testCase) {
    std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> certs = GetCerts();
    ASSERT_TRUE(certs.back());

    content::WebContents* dialog_contents =
        ShowCertificateViewer(std::move(certs));
    ASSERT_TRUE(dialog_contents);

    ASSERT_TRUE(RunTestOnWebContents(dialog_contents,
        "certificate_viewer_dialog/certificate_viewer_dialog_test.js",
        base::StringPrintf("runMochaTest('CertificateViewer', '%s');",
                           testCase.c_str()),
        /*skip_test_loader=*/ true));
  }

 private:
  content::WebContents* ShowCertificateViewer(
      std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> certs) {
    if (!browser() || !browser()->window()) {
      return nullptr;
    }

    CertificateViewerDialog* dialog = CertificateViewerDialog::ShowConstrained(
        std::move(certs), /*cert_nicknames=*/{},
        browser()->tab_strip_model()->GetActiveWebContents(),
        browser()->window()->GetNativeWindow());

    content::WebContents* webui_webcontents =
        dialog->delegate_->GetWebContents();
    EXPECT_TRUE(content::WaitForLoadStop(webui_webcontents));
    webui_webcontents->GetPrimaryMainFrame()->SetWebUIProperty(
        "expectedUrl", chrome::kChromeUICertificateViewerURL);
    return webui_webcontents;
  }
};

IN_PROC_BROWSER_TEST_F(CertificateViewerUITest, DialogURL) {
  RunTestCase("DialogURL");
}

IN_PROC_BROWSER_TEST_F(CertificateViewerUITest, CommonName) {
  RunTestCase("CommonName");
}

IN_PROC_BROWSER_TEST_F(CertificateViewerUITest, Details) {
  RunTestCase("Details");
}

class CertificateViewerUIInvalidCertTest : public CertificateViewerUITest {
 protected:
  std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> GetCerts() override {
    const uint8_t kInvalid[] = {42, 42, 42, 42, 42};
    std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> certs;
    certs.push_back(
        net::x509_util::CreateCryptoBuffer(base::make_span(kInvalid)));
    return certs;
  }
};

IN_PROC_BROWSER_TEST_F(CertificateViewerUIInvalidCertTest, InvalidCert) {
  RunTestCase("InvalidCert");
}
