// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_DATA_WEBUI_CERTIFICATE_VIEWER_UI_TEST_INL_H_
#define CHROME_TEST_DATA_WEBUI_CERTIFICATE_VIEWER_UI_TEST_INL_H_

#include "chrome/browser/certificate_viewer.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/certificate_viewer_webui.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/base/web_ui_browser_test.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/cert/x509_util.h"
#include "net/test/test_certificate_data.h"

// Test framework for
// chrome/test/data/webui/certificate_viewer_dialog_browsertest.js.
class CertificateViewerUITest : public WebUIBrowserTest {
 public:
  CertificateViewerUITest();
  ~CertificateViewerUITest() override;

 protected:
  void ShowCertificateViewerGoogleCert();
  void ShowCertificateViewerInvalidCert();
  void ShowCertificateViewer(std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> certs);
};

void CertificateViewerUITest::ShowCertificateViewerGoogleCert() {
  std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> certs;
  certs.push_back(
      net::x509_util::CreateCryptoBuffer(base::make_span(google_der)));
  ASSERT_TRUE(certs.back());

  ShowCertificateViewer(std::move(certs));
}

void CertificateViewerUITest::ShowCertificateViewerInvalidCert() {
  const uint8_t kInvalid[] = {42, 42, 42, 42, 42};
  std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> certs;
  certs.push_back(
      net::x509_util::CreateCryptoBuffer(base::make_span(kInvalid)));
  ASSERT_TRUE(certs.back());

  ShowCertificateViewer(std::move(certs));
}

void CertificateViewerUITest::ShowCertificateViewer(
    std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> certs) {
  ASSERT_TRUE(browser());
  ASSERT_TRUE(browser()->window());

  CertificateViewerDialog* dialog = CertificateViewerDialog::ShowConstrained(
      std::move(certs), /*cert_nicknames=*/{},
      browser()->tab_strip_model()->GetActiveWebContents(),
      browser()->window()->GetNativeWindow());
  content::WebContents* webui_webcontents = dialog->webui_->GetWebContents();
  EXPECT_TRUE(content::WaitForLoadStop(webui_webcontents));
  content::WebUI* webui = webui_webcontents->GetWebUI();
  webui_webcontents->GetPrimaryMainFrame()->SetWebUIProperty(
      "expectedUrl", chrome::kChromeUICertificateViewerURL);
  SetWebUIInstance(webui);
}

#endif  // CHROME_TEST_DATA_WEBUI_CERTIFICATE_VIEWER_UI_TEST_INL_H_
