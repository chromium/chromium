// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/ssl_client_certificate_selector_mac.h"

#import <SecurityInterface/SFChooseIdentityPanel.h>

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ssl/ssl_client_auth_metrics.h"
#include "chrome/browser/ssl/ssl_client_certificate_selector.h"
#include "chrome/browser/ssl/ssl_client_certificate_selector_test.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/client_certificate_delegate.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_utils.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/client_cert_identity_mac.h"
#include "net/ssl/ssl_private_key_test_util.h"
#include "net/test/cert_test_util.h"
#include "net/test/keychain_test_util_mac.h"
#include "net/test/test_data_directory.h"
#import "testing/gtest_mac.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "ui/base/cocoa/window_size_constants.h"

using web_modal::WebContentsModalDialogManager;

namespace {

struct TestClientCertificateDelegateResults {
  bool destroyed = false;
  bool continue_with_certificate_called = false;
  scoped_refptr<net::X509Certificate> cert;
  scoped_refptr<net::SSLPrivateKey> key;
};

class TestClientCertificateDelegate
    : public content::ClientCertificateDelegate {
 public:
  // Creates a ClientCertificateDelegate that sets |*destroyed| to true on
  // destruction.
  explicit TestClientCertificateDelegate(
      TestClientCertificateDelegateResults* results)
      : results_(results) {}

  ~TestClientCertificateDelegate() override { results_->destroyed = true; }

  // content::ClientCertificateDelegate.
  void ContinueWithCertificate(scoped_refptr<net::X509Certificate> cert,
                               scoped_refptr<net::SSLPrivateKey> key) override {
    EXPECT_FALSE(results_->continue_with_certificate_called);
    results_->cert = cert;
    results_->key = key;
    results_->continue_with_certificate_called = true;
    // TODO(mattm): Add a test of selecting the 2nd certificate (if possible).
  }

 private:
  TestClientCertificateDelegateResults* results_;

  DISALLOW_COPY_AND_ASSIGN(TestClientCertificateDelegate);
};

}  // namespace

class SSLClientCertificateSelectorMacTest
    : public SSLClientCertificateSelectorTestBase {
 public:
  ~SSLClientCertificateSelectorMacTest() override;

  // InProcessBrowserTest:
  void SetUpInProcessBrowserTestFixture() override;

  net::ClientCertIdentityList GetTestCertificateList();

 protected:
  scoped_refptr<net::X509Certificate> client_cert1_;
  scoped_refptr<net::X509Certificate> client_cert2_;
  std::string pkcs8_key1_;
  std::string pkcs8_key2_;
  net::ScopedTestKeychain scoped_keychain_;
  base::ScopedCFTypeRef<SecIdentityRef> sec_identity1_;
  base::ScopedCFTypeRef<SecIdentityRef> sec_identity2_;
};

SSLClientCertificateSelectorMacTest::~SSLClientCertificateSelectorMacTest() =
    default;

void SSLClientCertificateSelectorMacTest::SetUpInProcessBrowserTestFixture() {
  SSLClientCertificateSelectorTestBase::SetUpInProcessBrowserTestFixture();

  base::FilePath certs_dir = net::GetTestCertsDirectory();

  client_cert1_ = net::ImportCertFromFile(certs_dir, "client_1.pem");
  ASSERT_TRUE(client_cert1_);
  client_cert2_ = net::ImportCertFromFile(certs_dir, "client_2.pem");
  ASSERT_TRUE(client_cert2_);

  ASSERT_TRUE(base::ReadFileToString(certs_dir.AppendASCII("client_1.pk8"),
                                     &pkcs8_key1_));
  ASSERT_TRUE(base::ReadFileToString(certs_dir.AppendASCII("client_2.pk8"),
                                     &pkcs8_key2_));

  ASSERT_TRUE(scoped_keychain_.Initialize());

  sec_identity1_ = net::ImportCertAndKeyToKeychain(
      client_cert1_.get(), pkcs8_key1_, scoped_keychain_.keychain());
  ASSERT_TRUE(sec_identity1_);
  sec_identity2_ = net::ImportCertAndKeyToKeychain(
      client_cert2_.get(), pkcs8_key2_, scoped_keychain_.keychain());
  ASSERT_TRUE(sec_identity2_);
}

net::ClientCertIdentityList
SSLClientCertificateSelectorMacTest::GetTestCertificateList() {
  net::ClientCertIdentityList client_cert_list;
  client_cert_list.push_back(std::make_unique<net::ClientCertIdentityMac>(
      client_cert1_, base::ScopedCFTypeRef<SecIdentityRef>(sec_identity1_)));
  client_cert_list.push_back(std::make_unique<net::ClientCertIdentityMac>(
      client_cert2_, base::ScopedCFTypeRef<SecIdentityRef>(sec_identity2_)));
  return client_cert_list;
}

IN_PROC_BROWSER_TEST_F(SSLClientCertificateSelectorMacTest, Basic) {
  base::HistogramTester histograms;
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  WebContentsModalDialogManager* web_contents_modal_dialog_manager =
      WebContentsModalDialogManager::FromWebContents(web_contents);
  EXPECT_FALSE(web_contents_modal_dialog_manager->IsDialogActive());

  TestClientCertificateDelegateResults results;
  base::OnceClosure cancellation_callback =
      chrome::ShowSSLClientCertificateSelector(
          web_contents, auth_requestor_->cert_request_info_.get(),
          GetTestCertificateList(),
          std::make_unique<TestClientCertificateDelegate>(&results));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(web_contents_modal_dialog_manager->IsDialogActive());

  WebContentsModalDialogManager::TestApi test_api(
      web_contents_modal_dialog_manager);
  test_api.CloseAllDialogs();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(web_contents_modal_dialog_manager->IsDialogActive());

  histograms.ExpectUniqueSample(kClientCertSelectHistogramName,
                                ClientCertSelectionResult::kUserCloseTab, 1);

  EXPECT_TRUE(results.destroyed);
  EXPECT_FALSE(results.continue_with_certificate_called);
}

IN_PROC_BROWSER_TEST_F(SSLClientCertificateSelectorMacTest,
                       CancellationCallback) {
  base::HistogramTester histograms;
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  WebContentsModalDialogManager* web_contents_modal_dialog_manager =
      WebContentsModalDialogManager::FromWebContents(web_contents);
  EXPECT_FALSE(web_contents_modal_dialog_manager->IsDialogActive());

  TestClientCertificateDelegateResults results;
  base::OnceClosure cancellation_callback =
      chrome::ShowSSLClientCertificateSelector(
          web_contents, auth_requestor_->cert_request_info_.get(),
          GetTestCertificateList(),
          std::make_unique<TestClientCertificateDelegate>(&results));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(web_contents_modal_dialog_manager->IsDialogActive());

  std::move(cancellation_callback).Run();

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(web_contents_modal_dialog_manager->IsDialogActive());

  // The user did not close the tab, so there should be zero samples reported
  // for ClientCertSelectionResult::kUserCloseTab.

  // The TestClientCertificateDelegate will not be freed (yet) because no
  // SSLClientAuthObserver methods have been invoked. The SSLClientAuthObserver
  // owns the ClientCertificateDelegate in a unique_ptr, so it will be freed the
  // SSLClientAuthObserver is destroyed.
  EXPECT_FALSE(results.destroyed);
  EXPECT_FALSE(results.continue_with_certificate_called);
}

IN_PROC_BROWSER_TEST_F(SSLClientCertificateSelectorMacTest, Cancel) {
  base::HistogramTester histograms;
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  WebContentsModalDialogManager* web_contents_modal_dialog_manager =
      WebContentsModalDialogManager::FromWebContents(web_contents);
  EXPECT_FALSE(web_contents_modal_dialog_manager->IsDialogActive());

  TestClientCertificateDelegateResults results;
  chrome::OkAndCancelableForTesting* ok_and_cancelable =
      chrome::ShowSSLClientCertificateSelectorMacForTesting(
          web_contents, auth_requestor_->cert_request_info_.get(),
          GetTestCertificateList(),
          std::make_unique<TestClientCertificateDelegate>(&results),
          base::DoNothing());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(web_contents_modal_dialog_manager->IsDialogActive());

  ok_and_cancelable->ClickCancelButton();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(web_contents_modal_dialog_manager->IsDialogActive());

  histograms.ExpectUniqueSample(kClientCertSelectHistogramName,
                                ClientCertSelectionResult::kUserCancel, 1);

  // ContinueWithCertificate(nullptr, nullptr) should have been called.
  EXPECT_TRUE(results.destroyed);
  EXPECT_TRUE(results.continue_with_certificate_called);
  EXPECT_FALSE(results.cert);
  EXPECT_FALSE(results.key);
}

IN_PROC_BROWSER_TEST_F(SSLClientCertificateSelectorMacTest, Accept) {
  base::HistogramTester histograms;
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  WebContentsModalDialogManager* web_contents_modal_dialog_manager =
      WebContentsModalDialogManager::FromWebContents(web_contents);
  EXPECT_FALSE(web_contents_modal_dialog_manager->IsDialogActive());

  TestClientCertificateDelegateResults results;
  chrome::OkAndCancelableForTesting* ok_and_cancelable =
      chrome::ShowSSLClientCertificateSelectorMacForTesting(
          web_contents, auth_requestor_->cert_request_info_.get(),
          GetTestCertificateList(),
          std::make_unique<TestClientCertificateDelegate>(&results),
          base::DoNothing());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(web_contents_modal_dialog_manager->IsDialogActive());

  ok_and_cancelable->ClickOkButton();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(web_contents_modal_dialog_manager->IsDialogActive());

  // The first cert in the list should have been selected.
  EXPECT_TRUE(results.destroyed);
  EXPECT_TRUE(results.continue_with_certificate_called);
  EXPECT_EQ(client_cert1_, results.cert);
  ASSERT_TRUE(results.key);

  histograms.ExpectUniqueSample(kClientCertSelectHistogramName,
                                ClientCertSelectionResult::kUserSelect, 1);

  // The test keys are RSA keys.
  EXPECT_EQ(net::SSLPrivateKey::DefaultAlgorithmPreferences(EVP_PKEY_RSA, true),
            results.key->GetAlgorithmPreferences());
  TestSSLPrivateKeyMatches(results.key.get(), pkcs8_key1_);
}

// Test that switching to another tab correctly hides the sheet.
IN_PROC_BROWSER_TEST_F(SSLClientCertificateSelectorMacTest, HideShow) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  WebContentsModalDialogManager* web_contents_modal_dialog_manager =
      WebContentsModalDialogManager::FromWebContents(web_contents);
  EXPECT_FALSE(web_contents_modal_dialog_manager->IsDialogActive());

  // Account for any child windows that might be present before the certificate
  // viewer is open.
  NSWindow* window =
      web_contents->GetTopLevelNativeWindow().GetNativeNSWindow();
  NSUInteger num_child_windows = [[window childWindows] count];

  TestClientCertificateDelegateResults results;
  base::OnceClosure cancellation_callback =
      chrome::ShowSSLClientCertificateSelector(
          web_contents, auth_requestor_->cert_request_info_.get(),
          GetTestCertificateList(),
          std::make_unique<TestClientCertificateDelegate>(&results));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(web_contents_modal_dialog_manager->IsDialogActive());

  EXPECT_EQ(num_child_windows + 1, [[window childWindows] count]);
  // Assume the last child is the overlay window that was added.
  NSWindow* overlay_window = [[window childWindows] lastObject];

  NSWindow* sheet_window = [overlay_window attachedSheet];
  EXPECT_EQ(1.0, [sheet_window alphaValue]);

  // Switch to another tab and verify that the sheet is hidden. Interaction with
  // the tab underneath should not be blocked.
  AddBlankTabAndShow(browser());
  EXPECT_EQ(0.0, [sheet_window alphaValue]);
  EXPECT_TRUE([overlay_window ignoresMouseEvents]);

  // Switch back and verify that the sheet is shown. Interaction with the tab
  // underneath should be blocked while the sheet is showing.
  chrome::SelectNumberedTab(browser(), 0);
  EXPECT_EQ(1.0, [sheet_window alphaValue]);
  EXPECT_FALSE([overlay_window ignoresMouseEvents]);

  EXPECT_FALSE(results.destroyed);
  EXPECT_FALSE(results.continue_with_certificate_called);

  // Close the tab. Delegate should be destroyed without continuing.
  chrome::CloseTab(browser());
  EXPECT_TRUE(results.destroyed);
  EXPECT_FALSE(results.continue_with_certificate_called);
}

// Test that we can't trigger the crash from https://crbug.com/653093
IN_PROC_BROWSER_TEST_F(SSLClientCertificateSelectorMacTest,
                       WorkaroundTableViewCrash) {
  base::RunLoop run_loop;

  @autoreleasepool {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    chrome::OkAndCancelableForTesting* ok_and_cancelable =
        chrome::ShowSSLClientCertificateSelectorMacForTesting(
            web_contents, auth_requestor_->cert_request_info_.get(),
            GetTestCertificateList(), nullptr, run_loop.QuitClosure());
    base::RunLoop().RunUntilIdle();

    ok_and_cancelable->ClickOkButton();
    base::RunLoop().RunUntilIdle();
  }

  // Wait until the deallocation lambda fires.
  run_loop.Run();

  // Without the workaround, this will crash on just about anything 10.11+.
  [[NSNotificationCenter defaultCenter]
      postNotificationName:NSPreferredScrollerStyleDidChangeNotification
                    object:nil
                  userInfo:@{
                    @"NSScrollerStyle" : @(NSScrollerStyleLegacy)
                  }];
  [[NSNotificationCenter defaultCenter]
      postNotificationName:NSPreferredScrollerStyleDidChangeNotification
                    object:nil
                  userInfo:@{
                    @"NSScrollerStyle" : @(NSScrollerStyleOverlay)
                  }];
}
