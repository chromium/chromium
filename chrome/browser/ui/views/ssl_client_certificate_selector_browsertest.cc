// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/ssl_client_auth_requestor_mock.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/ssl_client_certificate_selector.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/client_certificate_delegate.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/request_priority.h"
#include "net/cert/x509_certificate.h"
#include "net/http/http_transaction_factory.h"
#include "net/ssl/client_cert_identity_test_util.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/test/widget_activation_waiter.h"
#include "ui/views/test/widget_test.h"

using ::testing::Mock;
using ::testing::StrictMock;
// We don't have a way to do end-to-end SSL client auth testing, so this test
// creates a certificate selector_ manually with a mocked
// SSLClientAuthHandler.

class SSLClientCertificateSelectorTest : public InProcessBrowserTest {
 public:
  SSLClientCertificateSelectorTest() = default;

  void SetUpInProcessBrowserTestFixture() override {
    base::FilePath certs_dir = net::GetTestCertsDirectory();

    cert_identity_1_ = net::FakeClientCertIdentity::CreateFromCertAndKeyFiles(
        certs_dir, "client_1.pem", "client_1.pk8");
    ASSERT_TRUE(cert_identity_1_);
    cert_identity_2_ = net::FakeClientCertIdentity::CreateFromCertAndKeyFiles(
        certs_dir, "client_2.pem", "client_2.pk8");
    ASSERT_TRUE(cert_identity_2_);

    cert_request_info_ = base::MakeRefCounted<net::SSLCertRequestInfo>();
    cert_request_info_->host_and_port = net::HostPortPair("foo", 123);
  }

  void SetUpOnMainThread() override {
    auth_requestor_ =
        new StrictMock<SSLClientAuthRequestorMock>(cert_request_info_);

    EXPECT_TRUE(content::WaitForLoadStop(
        browser()->tab_strip_model()->GetActiveWebContents()));
    net::ClientCertIdentityList cert_identity_list;
    cert_identity_list.push_back(cert_identity_1_->Copy());
    cert_identity_list.push_back(cert_identity_2_->Copy());
    selector_ = new SSLClientCertificateSelector(
        browser()->tab_strip_model()->GetActiveWebContents(),
        auth_requestor_->cert_request_info_, std::move(cert_identity_list),
        auth_requestor_->CreateDelegate());
    selector_->Init();
    selector_->Show();

    ASSERT_TRUE(selector_->GetSelectedCert());
    EXPECT_EQ(cert_identity_1_->certificate(),
              selector_->GetSelectedCert()->certificate());
  }

  // Have to release our reference to the auth handler during the test to allow
  // it to be destroyed while the Browser still exists.
  void TearDownOnMainThread() override {
    auth_requestor_.reset();
  }

 protected:
  std::unique_ptr<net::FakeClientCertIdentity> cert_identity_1_;
  std::unique_ptr<net::FakeClientCertIdentity> cert_identity_2_;
  scoped_refptr<net::SSLCertRequestInfo> cert_request_info_;
  scoped_refptr<StrictMock<SSLClientAuthRequestorMock>> auth_requestor_;
  // The selector will be deleted when a cert is selected or the tab is closed.
  raw_ptr<SSLClientCertificateSelector, AcrossTasksDanglingUntriaged>
      selector_ = nullptr;
};

class SSLClientCertificateSelectorMultiTabTest
    : public SSLClientCertificateSelectorTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    SSLClientCertificateSelectorTest::SetUpInProcessBrowserTestFixture();

    cert_request_info_1_ = base::MakeRefCounted<net::SSLCertRequestInfo>();
    cert_request_info_1_->host_and_port = net::HostPortPair("bar", 123);

    cert_request_info_2_ = base::MakeRefCounted<net::SSLCertRequestInfo>();
    cert_request_info_2_->host_and_port = net::HostPortPair("bar", 123);
  }

  void SetUpOnMainThread() override {
    SSLClientCertificateSelectorTest::SetUpOnMainThread();

    auth_requestor_1_ =
        new StrictMock<SSLClientAuthRequestorMock>(cert_request_info_1_);
    auth_requestor_2_ =
        new StrictMock<SSLClientAuthRequestorMock>(cert_request_info_2_);

    ASSERT_TRUE(
        AddTabAtIndex(1, GURL("about:blank"), ui::PAGE_TRANSITION_LINK));
    ASSERT_TRUE(
        AddTabAtIndex(2, GURL("about:blank"), ui::PAGE_TRANSITION_LINK));
    ASSERT_TRUE(nullptr != browser()->tab_strip_model()->GetWebContentsAt(0));
    ASSERT_TRUE(nullptr != browser()->tab_strip_model()->GetWebContentsAt(1));
    ASSERT_TRUE(nullptr != browser()->tab_strip_model()->GetWebContentsAt(2));
    EXPECT_TRUE(content::WaitForLoadStop(
        browser()->tab_strip_model()->GetWebContentsAt(1)));
    EXPECT_TRUE(content::WaitForLoadStop(
        browser()->tab_strip_model()->GetWebContentsAt(2)));

    net::ClientCertIdentityList cert_identity_list_1;
    cert_identity_list_1.push_back(cert_identity_1_->Copy());
    cert_identity_list_1.push_back(cert_identity_2_->Copy());
    selector_1_ = new SSLClientCertificateSelector(
        browser()->tab_strip_model()->GetWebContentsAt(1),
        auth_requestor_1_->cert_request_info_, std::move(cert_identity_list_1),
        auth_requestor_1_->CreateDelegate());
    selector_1_->Init();
    selector_1_->Show();

    net::ClientCertIdentityList cert_identity_list_2;
    cert_identity_list_2.push_back(cert_identity_1_->Copy());
    cert_identity_list_2.push_back(cert_identity_2_->Copy());
    selector_2_ = new SSLClientCertificateSelector(
        browser()->tab_strip_model()->GetWebContentsAt(2),
        auth_requestor_2_->cert_request_info_, std::move(cert_identity_list_2),
        auth_requestor_2_->CreateDelegate());
    selector_2_->Init();
    selector_2_->Show();

    EXPECT_EQ(2, browser()->tab_strip_model()->active_index());
    ASSERT_TRUE(selector_1_->GetSelectedCert());
    EXPECT_EQ(cert_identity_1_->certificate(),
              selector_1_->GetSelectedCert()->certificate());
    ASSERT_TRUE(selector_2_->GetSelectedCert());
    EXPECT_EQ(cert_identity_1_->certificate(),
              selector_2_->GetSelectedCert()->certificate());
  }

  void TearDownOnMainThread() override {
    auth_requestor_2_.reset();
    auth_requestor_1_.reset();
    SSLClientCertificateSelectorTest::TearDownOnMainThread();
  }

 protected:
  scoped_refptr<net::SSLCertRequestInfo> cert_request_info_1_;
  scoped_refptr<net::SSLCertRequestInfo> cert_request_info_2_;
  scoped_refptr<StrictMock<SSLClientAuthRequestorMock>> auth_requestor_1_;
  scoped_refptr<StrictMock<SSLClientAuthRequestorMock>> auth_requestor_2_;
  raw_ptr<SSLClientCertificateSelector, AcrossTasksDanglingUntriaged>
      selector_1_;
  raw_ptr<SSLClientCertificateSelector, AcrossTasksDanglingUntriaged>
      selector_2_;
};

class SSLClientCertificateSelectorMultiProfileTest
    : public SSLClientCertificateSelectorTest {
 public:
  SSLClientCertificateSelectorMultiProfileTest() = default;

  SSLClientCertificateSelectorMultiProfileTest(
      const SSLClientCertificateSelectorMultiProfileTest&) = delete;
  SSLClientCertificateSelectorMultiProfileTest& operator=(
      const SSLClientCertificateSelectorMultiProfileTest&) = delete;

  ~SSLClientCertificateSelectorMultiProfileTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    SSLClientCertificateSelectorTest::SetUpInProcessBrowserTestFixture();

    cert_request_info_1_ = base::MakeRefCounted<net::SSLCertRequestInfo>();
    cert_request_info_1_->host_and_port = net::HostPortPair("foo", 123);
  }

  void SetUpOnMainThread() override {
    SSLClientCertificateSelectorTest::SetUpOnMainThread();

    browser_1_ = CreateIncognitoBrowser();

    gfx::NativeWindow window = browser_1_->window()->GetNativeWindow();
    views::Widget* widget = views::Widget::GetWidgetForNativeWindow(window);
    ASSERT_NE(nullptr, widget);
    views::test::WaitForWidgetActive(widget, true);

    auth_requestor_1_ =
        new StrictMock<SSLClientAuthRequestorMock>(cert_request_info_1_);

    net::ClientCertIdentityList cert_identity_list;
    cert_identity_list.push_back(cert_identity_1_->Copy());
    cert_identity_list.push_back(cert_identity_2_->Copy());
    selector_1_ = new SSLClientCertificateSelector(
        browser_1_->tab_strip_model()->GetActiveWebContents(),
        auth_requestor_1_->cert_request_info_, std::move(cert_identity_list),
        auth_requestor_1_->CreateDelegate());
    selector_1_->Init();
    selector_1_->Show();

    ASSERT_TRUE(selector_1_->GetSelectedCert());
    EXPECT_EQ(cert_identity_1_->certificate(),
              selector_1_->GetSelectedCert()->certificate());
  }

  void TearDownOnMainThread() override {
    auth_requestor_1_.reset();
    SSLClientCertificateSelectorTest::TearDownOnMainThread();
  }

 protected:
  raw_ptr<Browser, AcrossTasksDanglingUntriaged> browser_1_;
  scoped_refptr<net::SSLCertRequestInfo> cert_request_info_1_;
  scoped_refptr<StrictMock<SSLClientAuthRequestorMock> > auth_requestor_1_;
  raw_ptr<SSLClientCertificateSelector, AcrossTasksDanglingUntriaged>
      selector_1_;
};

IN_PROC_BROWSER_TEST_F(SSLClientCertificateSelectorTest, SelectNone) {
  EXPECT_CALL(*auth_requestor_, CancelCertificateSelection());

  // Let the mock get checked on destruction.
}

IN_PROC_BROWSER_TEST_F(SSLClientCertificateSelectorTest, Escape) {
  EXPECT_CALL(*auth_requestor_, CertificateSelected(nullptr, nullptr));

  EXPECT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_ESCAPE, false, false, false, false));
  auth_requestor_->WaitForCompletion();

  Mock::VerifyAndClear(auth_requestor_.get());
}

IN_PROC_BROWSER_TEST_F(SSLClientCertificateSelectorTest, SelectDefault) {
  EXPECT_CALL(*auth_requestor_,
              CertificateSelected(cert_identity_1_->certificate(),
                                  cert_identity_1_->ssl_private_key()));

  EXPECT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_RETURN, false, false, false, false));
  auth_requestor_->WaitForCompletion();

  Mock::VerifyAndClear(auth_requestor_.get());
}

IN_PROC_BROWSER_TEST_F(SSLClientCertificateSelectorTest, CloseTab) {
  EXPECT_CALL(*auth_requestor_, CancelCertificateSelection());

  browser()->tab_strip_model()->CloseAllTabs();
  auth_requestor_->WaitForCompletion();

  Mock::VerifyAndClear(auth_requestor_.get());
}

IN_PROC_BROWSER_TEST_F(SSLClientCertificateSelectorMultiTabTest, EscapeTest) {
  // auth_requestor_1_ should get selected automatically by the
  // SSLClientAuthObserver when selector_2_ is accepted, since both 1 & 2 have
  // the same host:port.
  EXPECT_CALL(*auth_requestor_1_, CertificateSelected(nullptr, nullptr));
  EXPECT_CALL(*auth_requestor_2_, CertificateSelected(nullptr, nullptr));

  EXPECT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_ESCAPE, false, false, false, false));
  auth_requestor_1_->WaitForCompletion();
  auth_requestor_2_->WaitForCompletion();

  Mock::VerifyAndClear(auth_requestor_.get());
  Mock::VerifyAndClear(auth_requestor_1_.get());
  Mock::VerifyAndClear(auth_requestor_2_.get());

  // Now let the default selection for auth_requestor_ mock get checked on
  // destruction.
  EXPECT_CALL(*auth_requestor_, CancelCertificateSelection());
}

IN_PROC_BROWSER_TEST_F(SSLClientCertificateSelectorMultiTabTest, SelectSecond) {
  // auth_requestor_1_ should get selected automatically by the
  // SSLClientAuthObserver when selector_2_ is accepted, since both 1 & 2 have
  // the same host:port.
  EXPECT_CALL(*auth_requestor_1_,
              CertificateSelected(cert_identity_2_->certificate(),
                                  cert_identity_2_->ssl_private_key()));
  EXPECT_CALL(*auth_requestor_2_,
              CertificateSelected(cert_identity_2_->certificate(),
                                  cert_identity_2_->ssl_private_key()));

  EXPECT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_DOWN, false, false, false, false));

  ASSERT_TRUE(selector_->GetSelectedCert());
  EXPECT_EQ(cert_identity_1_->certificate(),
            selector_->GetSelectedCert()->certificate());
  ASSERT_TRUE(selector_1_->GetSelectedCert());
  EXPECT_EQ(cert_identity_1_->certificate(),
            selector_1_->GetSelectedCert()->certificate());
  ASSERT_TRUE(selector_2_->GetSelectedCert());
  EXPECT_EQ(cert_identity_2_->certificate(),
            selector_2_->GetSelectedCert()->certificate());

  EXPECT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_RETURN, false, false, false, false));
  auth_requestor_1_->WaitForCompletion();
  auth_requestor_2_->WaitForCompletion();

  Mock::VerifyAndClear(auth_requestor_.get());
  Mock::VerifyAndClear(auth_requestor_1_.get());
  Mock::VerifyAndClear(auth_requestor_2_.get());

  // Now let the default selection for auth_requestor_ mock get checked on
  // destruction.
  EXPECT_CALL(*auth_requestor_, CancelCertificateSelection());
}

IN_PROC_BROWSER_TEST_F(SSLClientCertificateSelectorMultiProfileTest, Escape) {
  EXPECT_CALL(*auth_requestor_1_, CertificateSelected(nullptr, nullptr));

  EXPECT_TRUE(ui_test_utils::SendKeyPressSync(
      browser_1_, ui::VKEY_ESCAPE, false, false, false, false));
  auth_requestor_1_->WaitForCompletion();

  Mock::VerifyAndClear(auth_requestor_.get());
  Mock::VerifyAndClear(auth_requestor_1_.get());

  // Now let the default selection for auth_requestor_ mock get checked on
  // destruction.
  EXPECT_CALL(*auth_requestor_, CancelCertificateSelection());
}

IN_PROC_BROWSER_TEST_F(SSLClientCertificateSelectorMultiProfileTest,
                       SelectDefault) {
  EXPECT_CALL(*auth_requestor_1_,
              CertificateSelected(cert_identity_1_->certificate(),
                                  cert_identity_1_->ssl_private_key()));

  EXPECT_TRUE(ui_test_utils::SendKeyPressSync(
      browser_1_, ui::VKEY_RETURN, false, false, false, false));
  auth_requestor_1_->WaitForCompletion();

  Mock::VerifyAndClear(auth_requestor_.get());
  Mock::VerifyAndClear(auth_requestor_1_.get());

  // Now let the default selection for auth_requestor_ mock get checked on
  // destruction.
  EXPECT_CALL(*auth_requestor_, CancelCertificateSelection());
}
