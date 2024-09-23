// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/app/chrome_main_delegate.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/digital_credentials/digital_identity_provider_desktop.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/digital_credentials/digital_identity_safety_interstitial_controller_desktop.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/digital_identity_provider.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest-spi.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/widget/widget_observer.h"

namespace {

// Callback for when a views::Widget is shown. Sets `was_dialog_shown` if the
// shown views::Widget has `expected_dialog_title`.
void OnDialogShown(base::RepeatingClosure dialog_shown_callback,
                   std::u16string expected_dialog_title,
                   views::Widget* widget) {
  if (widget->widget_delegate()->GetWindowTitle() == expected_dialog_title) {
    dialog_shown_callback.Run();
  }
}

// content::DigitalIdentityProvider which:
// - always succeeds
// - offers method to wait till DigitalIdentityProvider::Request() is invoked.
class TestDigitalIdentityProvider final
    : public DigitalIdentityProviderDesktop {
 public:
  explicit TestDigitalIdentityProvider(
      base::OnceClosure credential_request_observer)
      : credential_request_observer_(std::move(credential_request_observer)) {}
  ~TestDigitalIdentityProvider() override = default;

  void Request(content::WebContents* web_contents,
               const url::Origin& origin,
               base::Value request,
               DigitalIdentityCallback callback) override {
    did_request_credential_ = true;

    base::OnceClosure observer = std::move(credential_request_observer_);
    // Calling the callback might destroy `this`.
    std::move(callback).Run("token");
    if (observer) {
      std::move(observer).Run();
    }
  }

  base::WeakPtr<TestDigitalIdentityProvider> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  bool did_request_credential_ = false;
  base::OnceClosure credential_request_observer_;
  base::WeakPtrFactory<TestDigitalIdentityProvider> weak_ptr_factory_{this};
};

// ChromeContentBrowserClient which returns custom
// content::DigitalIdentityProvider.
class TestBrowserClient : public ChromeContentBrowserClient {
 public:
  explicit TestBrowserClient(base::OnceClosure credential_request_observer)
      : credential_request_observer_(std::move(credential_request_observer)) {}
  ~TestBrowserClient() override = default;

  std::unique_ptr<content::DigitalIdentityProvider>
  CreateDigitalIdentityProvider() override {
    return std::make_unique<TestDigitalIdentityProvider>(
        std::move(credential_request_observer_));
  }

 private:
  base::OnceClosure credential_request_observer_;
  std::unique_ptr<content::DigitalIdentityProvider> provider_;
};

}  // anonymous namespace

// Tests for DigitalIdentitySafetyInterstitialControllerDesktop.
//
// A bunch of logic for showing the interstitial is shared between the desktop
// and Android implementations. For the sake of not duplicating integration
// tests for shared code for both desktop and Android, integration tests for
// shared logic are intentionally Android-only.
class DigitalIdentitySafetyInterstitialIntegrationTest
    : public InProcessBrowserTest {
 public:
  DigitalIdentitySafetyInterstitialIntegrationTest() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kWebIdentityDigitalCredentials);
  }
  ~DigitalIdentitySafetyInterstitialIntegrationTest() override = default;

  void SetUpOnMainThread() override {
    client_ = std::make_unique<TestBrowserClient>(
        base::BindOnce(&DigitalIdentitySafetyInterstitialIntegrationTest::
                           OnDigitalCredentialRequested,
                       weak_ptr_factory_.GetWeakPtr()));
    original_client_ = content::SetBrowserClientForTesting(client_.get());

    InProcessBrowserTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    content::SetBrowserClientForTesting(original_client_.get());
  }

  // Waits till DigitalIdentityProvider requests credential.
  void WaitTillRequestCredential() {
    if (did_request_credential_) {
      return;
    }

    base::RunLoop run_loop;
    credential_request_observer_ = run_loop.QuitClosure();
    run_loop.Run();
  }

 protected:
  void OnDigitalCredentialRequested() {
    did_request_credential_ = true;

    if (credential_request_observer_) {
      std::move(credential_request_observer_).Run();
    }
  }

  std::unique_ptr<TestBrowserClient> client_;
  raw_ptr<content::ContentBrowserClient> original_client_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  bool did_request_credential_ = false;
  base::OnceClosure credential_request_observer_;

  base::WeakPtrFactory<DigitalIdentitySafetyInterstitialIntegrationTest>
      weak_ptr_factory_{this};
};

/**
 * Test that an interstitial is shown if the page requests more than just the
 * age.
 */
IN_PROC_BROWSER_TEST_F(DigitalIdentitySafetyInterstitialIntegrationTest,
                       InterstitialShownMoreThanJustAge) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  std::u16string kExpectedDialogTitle = l10n_util::GetStringUTF16(
      IDS_WEB_DIGITAL_CREDENTIALS_INTERSTITIAL_DIALOG_TITLE);

  base::RunLoop run_loop;

  auto dialog_observer = std::make_unique<views::AnyWidgetObserver>(
      views::test::AnyWidgetTestPasskey());
  dialog_observer->set_shown_callback(base::BindRepeating(
      &OnDialogShown, run_loop.QuitClosure(), kExpectedDialogTitle));

  GURL url = ui_test_utils::GetTestUrl(
      base::FilePath(),
      base::FilePath(FILE_PATH_LITERAL("digital_credentials.html")));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  ASSERT_TRUE(content::ExecJs(
      web_contents,
      "document.getElementById('request_age_and_name_button').click();"));

  run_loop.Run();
}

/**
 * Test that an interstitial is not shown if the page only requests the age.
 */
IN_PROC_BROWSER_TEST_F(DigitalIdentitySafetyInterstitialIntegrationTest,
                       InterstitialNotShown) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  std::u16string kExpectedDialogTitle = l10n_util::GetStringUTF16(
      IDS_WEB_DIGITAL_CREDENTIALS_INTERSTITIAL_DIALOG_TITLE);

  auto dialog_observer = std::make_unique<views::AnyWidgetObserver>(
      views::test::AnyWidgetTestPasskey());
  base::RunLoop run_loop;
  bool was_dialog_shown = false;

  auto dialog_shown_action = [](bool& was_dialog_shown) {
    was_dialog_shown = true;
  };

  dialog_observer->set_shown_callback(base::BindRepeating(
      &OnDialogShown,
      base::BindRepeating(dialog_shown_action, std::ref(was_dialog_shown)),
      kExpectedDialogTitle));

  GURL url = ui_test_utils::GetTestUrl(
      base::FilePath(),
      base::FilePath(FILE_PATH_LITERAL("digital_credentials.html")));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  ASSERT_TRUE(content::ExecJs(
      web_contents,
      "document.getElementById('request_age_only_button').click();"));

  WaitTillRequestCredential();

  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(was_dialog_shown);
}
