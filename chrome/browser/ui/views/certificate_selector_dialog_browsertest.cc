// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/certificate_selector.h"

#include <memory>
#include <string>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/test/browser_test.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/client_cert_identity_test_util.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/label.h"

class TestCertificateSelector : public chrome::CertificateSelector {
 public:
  TestCertificateSelector(net::ClientCertIdentityList identities,
                          content::WebContents* web_contents)
      : chrome::CertificateSelector(std::move(identities), web_contents) {
    std::unique_ptr<views::Label> label =
        std::make_unique<views::Label>(l10n_util::GetStringFUTF16(
            IDS_CLIENT_CERT_DIALOG_TEXT, u"example.com"));
    label->SetMultiLine(true);
    label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    label->SizeToFit(chrome::CertificateSelector::kTableViewWidth);
    InitWithText(std::move(label));
  }

  TestCertificateSelector(const TestCertificateSelector&) = delete;
  TestCertificateSelector& operator=(const TestCertificateSelector&) = delete;

  // chrome::CertificateSelector:
  void AcceptCertificate(
      std::unique_ptr<net::ClientCertIdentity> identity) override {}
};

class CertificateSelectorDialogTest : public DialogBrowserTest {
 public:
  CertificateSelectorDialogTest() {}

  CertificateSelectorDialogTest(const CertificateSelectorDialogTest&) = delete;
  CertificateSelectorDialogTest& operator=(
      const CertificateSelectorDialogTest&) = delete;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    cert_1_ =
        net::ImportCertFromFile(net::GetTestCertsDirectory(), "client_1.pem");
    cert_2_ =
        net::ImportCertFromFile(net::GetTestCertsDirectory(), "client_2.pem");

    auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
    TestCertificateSelector* dialog = new TestCertificateSelector(
        net::FakeClientCertIdentityListFromCertificateList({cert_1_, cert_2_}),
        web_contents);
    dialog->Show();
  }

 private:
  scoped_refptr<net::X509Certificate> cert_1_;
  scoped_refptr<net::X509Certificate> cert_2_;
};

// Invokes a dialog that allows the user select a certificate.
IN_PROC_BROWSER_TEST_F(CertificateSelectorDialogTest, InvokeUi_default) {
  ShowAndVerifyUi();
}
