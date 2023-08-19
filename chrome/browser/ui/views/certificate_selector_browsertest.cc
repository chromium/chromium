// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/certificate_selector.h"

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/client_cert_identity_test_util.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/table_model.h"
#include "ui/views/controls/label.h"

namespace {

class TestCertificateSelector : public chrome::CertificateSelector {
 public:
  TestCertificateSelector(net::ClientCertIdentityList certificates,
                          content::WebContents* web_contents)
      : CertificateSelector(std::move(certificates), web_contents) {}

  TestCertificateSelector(const TestCertificateSelector&) = delete;
  TestCertificateSelector& operator=(const TestCertificateSelector&) = delete;

  ~TestCertificateSelector() override {
    if (!on_destroy_.is_null())
      std::move(on_destroy_).Run();
  }

  void Init() {
    InitWithText(std::make_unique<views::Label>(u"some arbitrary text"));
  }

  void AcceptCertificate(
      std::unique_ptr<net::ClientCertIdentity> identity) override {
    if (accepted_)
      *accepted_ = true;
  }

  bool Cancel() override {
    if (canceled_)
      *canceled_ = true;
    return CertificateSelector::Cancel();
  }

  void TrackState(bool* accepted, bool* canceled) {
    accepted_ = accepted;
    canceled_ = canceled;
  }

  using chrome::CertificateSelector::table_model_for_testing;

  void set_on_destroy(base::OnceClosure on_destroy) {
    on_destroy_ = std::move(on_destroy);
  }

 private:
  raw_ptr<bool> accepted_ = nullptr;
  raw_ptr<bool> canceled_ = nullptr;
  base::OnceClosure on_destroy_;
};

class CertificateSelectorTest : public InProcessBrowserTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    client_1_ =
        net::ImportCertFromFile(net::GetTestCertsDirectory(), "client_1.pem");
    ASSERT_TRUE(client_1_);

    client_2_ =
        net::ImportCertFromFile(net::GetTestCertsDirectory(), "client_2.pem");
    ASSERT_TRUE(client_2_);
  }

  void SetUpOnMainThread() override {
    ASSERT_TRUE(content::WaitForLoadStop(
        browser()->tab_strip_model()->GetActiveWebContents()));

    selector_ = new TestCertificateSelector(
        net::FakeClientCertIdentityListFromCertificateList(
            {client_1_, client_2_}),
        browser()->tab_strip_model()->GetActiveWebContents());
    selector_->Init();
    selector_->Show();
  }

 protected:
  scoped_refptr<net::X509Certificate> client_1_;
  scoped_refptr<net::X509Certificate> client_2_;

  // The selector will be owned by the Views hierarchy and will at latest be
  // deleted during the browser shutdown.
  raw_ptr<TestCertificateSelector, AcrossTasksDanglingUntriaged> selector_ =
      nullptr;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(CertificateSelectorTest, GetRowText) {
  ui::TableModel* model = selector_->table_model_for_testing();
  EXPECT_EQ(u"Client Cert A",
            model->GetText(0, IDS_CERT_SELECTOR_SUBJECT_COLUMN));
  EXPECT_EQ(u"B CA", model->GetText(0, IDS_CERT_SELECTOR_ISSUER_COLUMN));
  EXPECT_EQ(std::u16string(),
            model->GetText(0, IDS_CERT_SELECTOR_PROVIDER_COLUMN));
  EXPECT_EQ(u"1000", model->GetText(0, IDS_CERT_SELECTOR_SERIAL_COLUMN));

  EXPECT_EQ(u"Client Cert D",
            model->GetText(1, IDS_CERT_SELECTOR_SUBJECT_COLUMN));
  EXPECT_EQ(u"E CA", model->GetText(1, IDS_CERT_SELECTOR_ISSUER_COLUMN));
  EXPECT_EQ(std::u16string(),
            model->GetText(1, IDS_CERT_SELECTOR_PROVIDER_COLUMN));
  EXPECT_EQ(u"1002", model->GetText(1, IDS_CERT_SELECTOR_SERIAL_COLUMN));
}

IN_PROC_BROWSER_TEST_F(CertificateSelectorTest, GetSelectedCert) {
  ASSERT_TRUE(selector_->GetSelectedCert());
  EXPECT_EQ(client_1_.get(), selector_->GetSelectedCert()->certificate());
  EXPECT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_DOWN, false,
                                              false, false, false));
  ASSERT_TRUE(selector_->GetSelectedCert());
  EXPECT_EQ(client_2_.get(), selector_->GetSelectedCert()->certificate());
  EXPECT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_UP, false,
                                              false, false, false));
  ASSERT_TRUE(selector_->GetSelectedCert());
  EXPECT_EQ(client_1_.get(), selector_->GetSelectedCert()->certificate());
}

IN_PROC_BROWSER_TEST_F(CertificateSelectorTest, DoubleClick) {
  bool accepted = false;
  bool canceled = false;
  selector_->TrackState(&accepted, &canceled);

  base::RunLoop loop;
  selector_->set_on_destroy(loop.QuitClosure());

  // Simulate double clicking on an entry in the certificate list.
  selector_->OnDoubleClick();

  // Wait for the dialog to be closed and destroyed.
  loop.Run();

  // Closing the dialog through a double click must call only the Accept()
  // function and not Cancel().
  EXPECT_TRUE(accepted);
  EXPECT_FALSE(canceled);
}
