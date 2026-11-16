// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/signin_error_handler.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/signin/signin_error_ui.h"
#include "chrome/browser/ui/webui/signin/signin_ui_error.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_web_ui.h"

namespace {
const char kSigninErrorLearnMoreUrl[] =
    "https://support.google.com/chrome/answer/1181420?";

class TestingSigninErrorHandler : public SigninErrorHandler {
 public:
  TestingSigninErrorHandler(BrowserWindowInterface* browser,
                            content::WebUI* web_ui)
      : SigninErrorHandler(browser) {
    set_web_ui(web_ui);
  }

  TestingSigninErrorHandler(const TestingSigninErrorHandler&) = delete;
  TestingSigninErrorHandler& operator=(const TestingSigninErrorHandler&) =
      delete;

  void CloseBrowserModalSigninDialog() override {
    browser_modal_dialog_did_close_ = true;
    SigninErrorHandler::CloseBrowserModalSigninDialog();
  }

  using SigninErrorHandler::HandleConfirm;
  using SigninErrorHandler::HandleInitializedWithSize;
  using SigninErrorHandler::HandleLearnMore;
  using SigninErrorHandler::HandleSwitchToExistingProfile;

  bool browser_modal_dialog_did_close() {
    return browser_modal_dialog_did_close_;
  }

 private:
  bool browser_modal_dialog_did_close_ = false;
};

class SigninErrorHandlerTest : public InProcessBrowserTest {
 public:
  SigninErrorHandlerTest()
      : web_ui_(new content::TestWebUI), handler_(nullptr) {}

  SigninErrorHandlerTest(const SigninErrorHandlerTest&) = delete;
  SigninErrorHandlerTest& operator=(const SigninErrorHandlerTest&) = delete;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), chrome::ChromeUINewTabURLAsGURL()));
    web_ui()->set_web_contents(
        browser()->GetTabStripModel()->GetActiveWebContents());
    signin_error_ui_ = std::make_unique<SigninErrorUI>(web_ui());
  }

  void TearDownOnMainThread() override {
    signin_error_ui_.reset();
    web_ui_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  void CreateHandlerInBrowser() {
    DCHECK(!handler_);
    auto handler =
        std::make_unique<TestingSigninErrorHandler>(browser(), web_ui());
    handler_ = handler.get();
    signin_error_ui_ = std::make_unique<SigninErrorUI>(web_ui());
    web_ui()->AddMessageHandler(std::move(handler));
  }

  TestingSigninErrorHandler* handler() { return handler_; }

  SigninErrorUI* signin_error_ui() { return signin_error_ui_.get(); }

  content::TestWebUI* web_ui() { return web_ui_.get(); }

 private:
  std::unique_ptr<content::TestWebUI> web_ui_;
  std::unique_ptr<SigninErrorUI> signin_error_ui_;
  raw_ptr<TestingSigninErrorHandler, DanglingUntriaged> handler_;  // Not owned.
};

IN_PROC_BROWSER_TEST_F(SigninErrorHandlerTest, InBrowserHandleLearnMore) {
  // Before the test, there is only one new tab opened.
  TabStripModel* tab_strip_model = browser()->GetTabStripModel();
  EXPECT_EQ(1, tab_strip_model->count());
  EXPECT_EQ(chrome::ChromeUINewTabURLAsGURL(),
            tab_strip_model->GetActiveWebContents()->GetVisibleURL());

  // Open learn more.
  CreateHandlerInBrowser();
  base::ListValue args;
  handler()->HandleLearnMore(args);

  // Dialog should be closed now.
  EXPECT_TRUE(handler()->browser_modal_dialog_did_close());

  // Verify that the learn more URL was opened.
  EXPECT_EQ(2, tab_strip_model->count());
  EXPECT_EQ(GURL(kSigninErrorLearnMoreUrl),
            tab_strip_model->GetActiveWebContents()->GetVisibleURL());
}

IN_PROC_BROWSER_TEST_F(SigninErrorHandlerTest,
                       InBrowserHandleLearnMoreAfterBrowserRemoved) {
  // Before the test, there is only one new tab opened.
  TabStripModel* tab_strip_model = browser()->GetTabStripModel();
  EXPECT_EQ(1, tab_strip_model->count());
  EXPECT_EQ(chrome::ChromeUINewTabURLAsGURL(),
            tab_strip_model->GetActiveWebContents()->GetVisibleURL());

  // Inform the handler that the browser was removed.
  CreateHandlerInBrowser();
  handler()->OnBrowserClosed(browser());

  // Open learn more.
  base::ListValue args;
  handler()->HandleLearnMore(args);

  // Dialog is not closed if the browser was removed.
  EXPECT_FALSE(handler()->browser_modal_dialog_did_close());

  // Verify that the learn more URL was not opened as the browser was removed.
  EXPECT_EQ(1, tab_strip_model->count());
  EXPECT_EQ(chrome::ChromeUINewTabURLAsGURL(),
            tab_strip_model->GetActiveWebContents()->GetVisibleURL());
}

IN_PROC_BROWSER_TEST_F(SigninErrorHandlerTest, InBrowserTestConfirm) {
  CreateHandlerInBrowser();
  base::ListValue args;
  handler()->HandleConfirm(args);

  // Confirm simply closes the dialog.
  EXPECT_TRUE(handler()->browser_modal_dialog_did_close());
}

// Regression test for crbug.com/582848651: Initializing `SigninErrorUI` for
// `SigninUIError::Type::kAccountAlreadyUsedByAnotherProfile` when the duplicate
// profile is signed in with an unconsented primary account (`kSignin`, neither
// syncing nor enterprise-managed) should succeed and allow switching to the
// existing profile.
IN_PROC_BROWSER_TEST_F(
    SigninErrorHandlerTest,
    InitializeWithAccountAlreadyUsedBySignedInUnmanagedProfile) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  Profile& other_profile = profiles::testing::CreateProfileSync(
      profile_manager, profile_manager->GenerateNextProfileDirectoryPath());
  signin::MakePrimaryAccountAvailable(
      IdentityManagerFactory::GetForProfile(&other_profile), "user@gmail.com",
      signin::ConsentLevel::kSignin);

  ProfileAttributesEntry* entry =
      profile_manager->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(other_profile.GetPath());
  ASSERT_NE(entry, nullptr);
  ASSERT_EQ(entry->GetSigninState(),
            SigninState::kSignedInWithUnconsentedPrimaryAccount);
  ASSERT_FALSE(entry->IsAuthenticated());
  ASSERT_FALSE(entry->CanBeManaged());

  signin_error_ui()->InitializeMessageHandlerWithBrowser(
      browser(), SigninUIError::AccountAlreadyUsedByAnotherProfile(
                     "user@gmail.com", other_profile.GetPath()));

  ui_test_utils::BrowserCreatedObserver browser_created_observer;
  web_ui()->HandleReceivedMessage("switchToExistingProfile", base::ListValue());
  BrowserWindowInterface* other_browser = browser_created_observer.Wait();
  EXPECT_EQ(other_browser->GetProfile(), &other_profile);
}

}  // namespace
