// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/notreached.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/reauth_result.h"
#include "chrome/browser/signin/signin_features.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/signin_reauth_view_controller.h"
#include "chrome/browser/ui/signin_view_controller.h"
#include "chrome/browser/ui/webui/signin/login_ui_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/gaia_switches.h"
#include "net/base/escape.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/embedded_test_server/request_handler_util.h"

using ::testing::ElementsAre;

namespace {

const char kReauthUserActionHistogramName[] =
    "Signin.TransactionalReauthUserAction";
const char kReauthUserActionToFillPasswordHistogramName[] =
    "Signin.TransactionalReauthUserAction.ToFillPassword";
const char kReauthGaiaNavigationDurationFromReauthStartHistogramName[] =
    "Signin.TransactionalReauthGaiaNavigationDuration.FromReauthStart";
const char kReauthGaiaNavigationDurationFromConfirmClickHistogramName[] =
    "Signin.TransactionalReauthGaiaNavigationDuration.FromConfirmClick";

const base::TimeDelta kReauthDialogTimeout = base::TimeDelta::FromSeconds(30);
const char kReauthDonePath[] = "/embedded/xreauth/chrome?done";
const char kReauthUnexpectedResponsePath[] =
    "/embedded/xreauth/chrome?unexpected";
const char kReauthPath[] = "/embedded/xreauth/chrome";
const char kChallengePath[] = "/challenge";
constexpr char kTransactionalReauthResultToFillPasswordHistogram[] =
    "Signin.TransactionalReauthResult.ToFillPassword";
constexpr char kTransactionalReauthResultHistogram[] =
    "Signin.TransactionalReauthResult";

std::unique_ptr<net::test_server::BasicHttpResponse> CreateRedirectResponse(
    const GURL& redirect_url) {
  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_TEMPORARY_REDIRECT);
  http_response->AddCustomHeader("Location", redirect_url.spec());
  http_response->AddCustomHeader("Access-Control-Allow-Origin", "*");
  return http_response;
}

std::unique_ptr<net::test_server::BasicHttpResponse> CreateEmptyResponse(
    net::HttpStatusCode code) {
  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(code);
  return http_response;
}

std::unique_ptr<net::test_server::HttpResponse> HandleReauthURL(
    const GURL& base_url,
    const net::test_server::HttpRequest& request) {
  if (!net::test_server::ShouldHandle(request, kReauthPath)) {
    return nullptr;
  }

  GURL request_url = request.GetURL();
  std::string parameter =
      net::UnescapeBinaryURLComponent(request_url.query_piece());

  if (parameter.empty()) {
    // Parameterless request redirects to the fake challenge page.
    return CreateRedirectResponse(base_url.Resolve(kChallengePath));
  }

  if (parameter == "done") {
    // On success, the reauth returns HTTP_NO_CONTENT response.
    return CreateEmptyResponse(net::HTTP_NO_CONTENT);
  }

  if (parameter == "unexpected") {
    // Returns a response that isn't expected by Chrome.
    return CreateEmptyResponse(net::HTTP_NOT_IMPLEMENTED);
  }

  NOTREACHED();
  return nullptr;
}

class ReauthTestObserver : SigninReauthViewController::Observer {
 public:
  explicit ReauthTestObserver(SigninReauthViewController* controller)
      : controller_(controller) {
    controller_->AddObserver(this);
  }

  void WaitUntilGaiaReauthPageIsShown() { run_loop_.Run(); }

  void OnGaiaReauthPageShown() override {
    controller_->RemoveObserver(this);
    run_loop_.Quit();
  }

 private:
  SigninReauthViewController* controller_;
  base::RunLoop run_loop_;
};

base::Bucket OnceUserAction(SigninReauthViewController::UserAction action) {
  return base::Bucket(static_cast<int>(action), 1);
}

}  // namespace

// Browser tests for SigninReauthViewController.
class SigninReauthViewControllerBrowserTest : public InProcessBrowserTest {
 public:
  void SetUp() override {
    ASSERT_TRUE(https_server()->InitializeAndListen());
    InProcessBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // HTTPS server only serves a valid cert for localhost, so this is needed to
    // load pages from other hosts without an interstitial.
    command_line->AppendSwitch("ignore-certificate-errors");
    command_line->AppendSwitchASCII(switches::kGaiaUrl, base_url().spec());
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");

    https_server()->AddDefaultHandlers(GetChromeTestDataDir());
    https_server()->RegisterRequestHandler(
        base::BindRepeating(&HandleReauthURL, base_url()));
    reauth_challenge_response_ =
        std::make_unique<net::test_server::ControllableHttpResponse>(
            https_server(), kChallengePath);
    https_server()->StartAcceptingConnections();

    account_id_ = signin::SetUnconsentedPrimaryAccount(identity_manager(),
                                                       "alice@gmail.com")
                      .account_id;

    reauth_result_loop_ = std::make_unique<base::RunLoop>();
    InProcessBrowserTest::SetUpOnMainThread();
  }

  void ShowReauthPrompt() {
    abort_handle_ = browser()->signin_view_controller()->ShowReauthPrompt(
        account_id_, signin_metrics::ReauthAccessPoint::kAutofillDropdown,
        base::BindOnce(&SigninReauthViewControllerBrowserTest::OnReauthResult,
                       base::Unretained(this)));
  }

  // This method must be called only after the reauth dialog has been opened.
  void RedirectGaiaChallengeTo(const GURL& redirect_url) {
    reauth_challenge_response_->WaitForRequest();
    auto redirect_response = CreateRedirectResponse(redirect_url);
    reauth_challenge_response_->Send(redirect_response->ToResponseString());
    reauth_challenge_response_->Done();
  }

  void OnReauthResult(signin::ReauthResult reauth_result) {
    reauth_result_ = reauth_result;
    reauth_result_loop_->Quit();
  }

  base::Optional<signin::ReauthResult> WaitForReauthResult() {
    reauth_result_loop_->Run();
    return reauth_result_;
  }

  // The test cannot depend on Views implementation so it simulates clicking on
  // the close button through calling the close event.
  void SimulateCloseButtonClick() {
    signin_reauth_view_controller()->OnModalSigninClosed();
  }

  void ResetAbortHandle() { abort_handle_.reset(); }

  net::EmbeddedTestServer* https_server() { return &https_server_; }

  GURL base_url() { return https_server()->base_url(); }

  signin::IdentityManager* identity_manager() {
    return IdentityManagerFactory::GetForProfile(browser()->profile());
  }

  SigninReauthViewController* signin_reauth_view_controller() {
    SigninViewController* signin_view_controller =
        browser()->signin_view_controller();
    DCHECK(signin_view_controller->ShowsModalDialog());
    return static_cast<SigninReauthViewController*>(
        signin_view_controller->GetModalDialogDelegateForTesting());
  }

  base::HistogramTester* histogram_tester() { return &histogram_tester_; }

 private:
  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
  base::HistogramTester histogram_tester_;
  std::unique_ptr<net::test_server::ControllableHttpResponse>
      reauth_challenge_response_;
  CoreAccountId account_id_;
  std::unique_ptr<SigninViewController::ReauthAbortHandle> abort_handle_;

  std::unique_ptr<base::RunLoop> reauth_result_loop_;
  base::Optional<signin::ReauthResult> reauth_result_;
};

// Tests that the abort handle cancels an ongoing reauth flow.
IN_PROC_BROWSER_TEST_F(SigninReauthViewControllerBrowserTest,
                       AbortReauthDialog_AbortHandle) {
  ShowReauthPrompt();
  ResetAbortHandle();
  EXPECT_EQ(WaitForReauthResult(), signin::ReauthResult::kCancelled);
}

// Tests canceling the reauth dialog through CloseModalSignin().
IN_PROC_BROWSER_TEST_F(SigninReauthViewControllerBrowserTest,
                       AbortReauthDialog_CloseModalSignin) {
  ShowReauthPrompt();
  browser()->signin_view_controller()->CloseModalSignin();
  EXPECT_EQ(WaitForReauthResult(), signin::ReauthResult::kCancelled);
}

// Tests closing the reauth dialog by closing a hosting tab.
IN_PROC_BROWSER_TEST_F(SigninReauthViewControllerBrowserTest,
                       AbortReauthDialog_CloseHostingTab) {
  ShowReauthPrompt();
  auto* tab_strip_model = browser()->tab_strip_model();
  tab_strip_model->CloseWebContentsAt(tab_strip_model->active_index(),
                                      TabStripModel::CLOSE_USER_GESTURE);
  EXPECT_EQ(WaitForReauthResult(), signin::ReauthResult::kDismissedByUser);
  histogram_tester()->ExpectUniqueSample(
      kReauthUserActionHistogramName,
      SigninReauthViewController::UserAction::kCloseConfirmationDialog, 1);
}

// Tests closing the reauth confirmation dialog through by clicking on the close
// button (the X).
IN_PROC_BROWSER_TEST_F(SigninReauthViewControllerBrowserTest,
                       CloseReauthConfirmationDialog) {
  ShowReauthPrompt();
  SimulateCloseButtonClick();
  EXPECT_EQ(WaitForReauthResult(), signin::ReauthResult::kDismissedByUser);
  histogram_tester()->ExpectUniqueSample(
      kReauthUserActionHistogramName,
      SigninReauthViewController::UserAction::kCloseConfirmationDialog, 1);
}

// Tests closing the Gaia reauth dialog through by clicking on the close button
// (the X).
IN_PROC_BROWSER_TEST_F(SigninReauthViewControllerBrowserTest,
                       CloseGaiaReauthDialog) {
  ShowReauthPrompt();
  RedirectGaiaChallengeTo(https_server()->GetURL("/title1.html"));

  ReauthTestObserver reauth_observer(signin_reauth_view_controller());
  ASSERT_TRUE(login_ui_test_utils::ConfirmReauthConfirmationDialog(
      browser(), kReauthDialogTimeout));
  reauth_observer.WaitUntilGaiaReauthPageIsShown();

  SimulateCloseButtonClick();
  EXPECT_EQ(WaitForReauthResult(), signin::ReauthResult::kDismissedByUser);
  EXPECT_THAT(
      histogram_tester()->GetAllSamples(kReauthUserActionHistogramName),
      ElementsAre(
          OnceUserAction(
              SigninReauthViewController::UserAction::kClickNextButton),
          OnceUserAction(
              SigninReauthViewController::UserAction::kCloseGaiaReauthDialog)));
}

// Tests clicking on the cancel button in the reauth dialog.
IN_PROC_BROWSER_TEST_F(SigninReauthViewControllerBrowserTest,
                       CancelReauthDialog) {
  ShowReauthPrompt();
  RedirectGaiaChallengeTo(https_server()->GetURL(kReauthDonePath));
  ASSERT_TRUE(login_ui_test_utils::CancelReauthConfirmationDialog(
      browser(), kReauthDialogTimeout));
  EXPECT_EQ(WaitForReauthResult(), signin::ReauthResult::kDismissedByUser);
  histogram_tester()->ExpectUniqueSample(
      kReauthUserActionHistogramName,
      SigninReauthViewController::UserAction::kClickCancelButton, 1);
}

// Tests the error page being displayed in case Gaia page failed to load.
IN_PROC_BROWSER_TEST_F(SigninReauthViewControllerBrowserTest,
                       GaiaChallengeLoadFailed) {
  ShowReauthPrompt();

  // Make the Gaia page fail to load.
  const GURL target_url = https_server()->GetURL("/close-socket");
  content::TestNavigationObserver target_content_observer(target_url);
  target_content_observer.WatchExistingWebContents();
  RedirectGaiaChallengeTo(target_url);
  target_content_observer.Wait();

  EXPECT_TRUE(browser()->signin_view_controller()->ShowsModalDialog());
  EXPECT_FALSE(target_content_observer.last_navigation_succeeded());

  // Now confirm the pre-reauth confirmation dialog, and wait for the Gaia page
  // (an error page in this case) to show up.
  ReauthTestObserver reauth_observer(signin_reauth_view_controller());
  ASSERT_TRUE(login_ui_test_utils::ConfirmReauthConfirmationDialog(
      browser(), kReauthDialogTimeout));
  reauth_observer.WaitUntilGaiaReauthPageIsShown();

  // Close the modal dialog and check that |kLoadFailed| is returned as the
  // result.
  SimulateCloseButtonClick();
  EXPECT_EQ(WaitForReauthResult(), signin::ReauthResult::kLoadFailed);
  EXPECT_THAT(
      histogram_tester()->GetAllSamples(kReauthUserActionHistogramName),
      ElementsAre(
          OnceUserAction(
              SigninReauthViewController::UserAction::kClickNextButton),
          OnceUserAction(
              SigninReauthViewController::UserAction::kCloseGaiaReauthDialog)));
}

// Tests clicking on the confirm button in the reauth dialog. Reauth completes
// before the confirmation.
IN_PROC_BROWSER_TEST_F(SigninReauthViewControllerBrowserTest,
                       ConfirmReauthDialog) {
  ShowReauthPrompt();
  RedirectGaiaChallengeTo(https_server()->GetURL(kReauthDonePath));
  ASSERT_TRUE(login_ui_test_utils::ConfirmReauthConfirmationDialog(
      browser(), kReauthDialogTimeout));
  EXPECT_EQ(WaitForReauthResult(), signin::ReauthResult::kSuccess);
  histogram_tester()->ExpectUniqueSample(
      kReauthUserActionHistogramName,
      SigninReauthViewController::UserAction::kClickConfirmButton, 1);
  histogram_tester()->ExpectUniqueSample(
      kReauthUserActionToFillPasswordHistogramName,
      SigninReauthViewController::UserAction::kClickConfirmButton, 1);
  histogram_tester()->ExpectTotalCount(
      kReauthGaiaNavigationDurationFromReauthStartHistogramName, 1);
  histogram_tester()->ExpectTotalCount(
      kReauthGaiaNavigationDurationFromConfirmClickHistogramName, 1);
}

// Tests completing the Gaia reauth challenge in a dialog.
IN_PROC_BROWSER_TEST_F(SigninReauthViewControllerBrowserTest,
                       CompleteReauthInDialog) {
  // The URL contains a link that navigates to the reauth success URL.
  const std::string target_path = net::test_server::GetFilePathWithReplacements(
      "/signin/link_with_replacements.html",
      {{"REPLACE_WITH_URL", https_server()->GetURL(kReauthDonePath).spec()}});
  const GURL target_url = https_server()->GetURL(target_path);

  content::TestNavigationObserver target_content_observer(target_url);
  target_content_observer.StartWatchingNewWebContents();
  ShowReauthPrompt();
  RedirectGaiaChallengeTo(target_url);

  ReauthTestObserver reauth_observer(signin_reauth_view_controller());
  ASSERT_TRUE(login_ui_test_utils::ConfirmReauthConfirmationDialog(
      browser(), kReauthDialogTimeout));
  reauth_observer.WaitUntilGaiaReauthPageIsShown();
  target_content_observer.Wait();

  content::WebContents* target_contents =
      signin_reauth_view_controller()->GetWebContents();
  ASSERT_TRUE(content::ExecuteScript(
      target_contents, "document.getElementsByTagName('a')[0].click();"));
  EXPECT_EQ(WaitForReauthResult(), signin::ReauthResult::kSuccess);
  EXPECT_THAT(
      histogram_tester()->GetAllSamples(kReauthUserActionHistogramName),
      ElementsAre(
          OnceUserAction(
              SigninReauthViewController::UserAction::kClickNextButton),
          OnceUserAction(
              SigninReauthViewController::UserAction::kPassGaiaReauth)));
}

// Tests that links from the Gaia page are opened in a new tab.
IN_PROC_BROWSER_TEST_F(SigninReauthViewControllerBrowserTest,
                       OpenLinksInNewTab) {
  content::WebContents* original_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  const GURL target_url = https_server()->GetURL("/link_with_target.html");
  content::TestNavigationObserver target_content_observer(target_url);
  target_content_observer.StartWatchingNewWebContents();
  ShowReauthPrompt();
  RedirectGaiaChallengeTo(target_url);

  ReauthTestObserver reauth_observer(signin_reauth_view_controller());
  ASSERT_TRUE(login_ui_test_utils::ConfirmReauthConfirmationDialog(
      browser(), kReauthDialogTimeout));
  reauth_observer.WaitUntilGaiaReauthPageIsShown();
  target_content_observer.Wait();

  content::WebContents* dialog_contents =
      signin_reauth_view_controller()->GetWebContents();
  content::TestNavigationObserver new_tab_observer(nullptr);
  new_tab_observer.StartWatchingNewWebContents();
  ASSERT_TRUE(content::ExecuteScript(
      dialog_contents, "document.getElementsByTagName('a')[0].click();"));
  new_tab_observer.Wait();

  content::WebContents* new_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_NE(new_contents, original_contents);
  EXPECT_NE(new_contents, dialog_contents);
  EXPECT_EQ(new_contents->GetURL(), https_server()->GetURL("/title1.html"));
}

// Tests that the authentication flow that goes outside of the reauth host is
// shown in a new tab.
IN_PROC_BROWSER_TEST_F(SigninReauthViewControllerBrowserTest,
                       CompleteSAMLInNewTab) {
  content::WebContents* original_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // The URL contains a link that navigates to the reauth success URL.
  const std::string target_path = net::test_server::GetFilePathWithReplacements(
      "/signin/link_with_replacements.html",
      {{"REPLACE_WITH_URL", https_server()->GetURL(kReauthDonePath).spec()}});
  const GURL target_url =
      https_server()->GetURL("3p-identity-provider.com", target_path);

  content::TestNavigationObserver target_content_observer(target_url);
  target_content_observer.StartWatchingNewWebContents();
  ShowReauthPrompt();
  RedirectGaiaChallengeTo(target_url);

  ui_test_utils::TabAddedWaiter tab_added_waiter(browser());
  ReauthTestObserver reauth_observer(signin_reauth_view_controller());
  ASSERT_TRUE(login_ui_test_utils::ConfirmReauthConfirmationDialog(
      browser(), kReauthDialogTimeout));
  reauth_observer.WaitUntilGaiaReauthPageIsShown();
  tab_added_waiter.Wait();
  target_content_observer.Wait();

  content::WebContents* target_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_NE(target_contents, original_contents);
  EXPECT_EQ(target_contents, signin_reauth_view_controller()->GetWebContents());
  EXPECT_EQ(target_contents->GetURL(), target_url);

  ASSERT_TRUE(content::ExecuteScript(
      target_contents, "document.getElementsByTagName('a')[0].click();"));
  EXPECT_EQ(WaitForReauthResult(), signin::ReauthResult::kSuccess);
  EXPECT_THAT(
      histogram_tester()->GetAllSamples(kReauthUserActionHistogramName),
      ElementsAre(
          OnceUserAction(
              SigninReauthViewController::UserAction::kClickNextButton),
          OnceUserAction(
              SigninReauthViewController::UserAction::kPassGaiaReauth)));
}

// Tests that closing of the SAML tab aborts the reauth flow.
IN_PROC_BROWSER_TEST_F(SigninReauthViewControllerBrowserTest, CloseSAMLTab) {
  const GURL target_url =
      https_server()->GetURL("3p-identity-provider.com", "/title1.html");
  ShowReauthPrompt();
  RedirectGaiaChallengeTo(target_url);

  ui_test_utils::TabAddedWaiter tab_added_waiter(browser());
  ASSERT_TRUE(login_ui_test_utils::ConfirmReauthConfirmationDialog(
      browser(), kReauthDialogTimeout));
  tab_added_waiter.Wait();

  auto* tab_strip_model = browser()->tab_strip_model();
  EXPECT_EQ(tab_strip_model->GetActiveWebContents()->GetURL(), target_url);
  tab_strip_model->CloseWebContentsAt(tab_strip_model->active_index(),
                                      TabStripModel::CLOSE_USER_GESTURE);
  EXPECT_EQ(WaitForReauthResult(), signin::ReauthResult::kDismissedByUser);
  EXPECT_THAT(
      histogram_tester()->GetAllSamples(kReauthUserActionHistogramName),
      ElementsAre(
          OnceUserAction(
              SigninReauthViewController::UserAction::kClickNextButton),
          OnceUserAction(
              SigninReauthViewController::UserAction::kCloseGaiaReauthTab)));
}

// Tests verifying that reauth results are recorded.
IN_PROC_BROWSER_TEST_F(SigninReauthViewControllerBrowserTest,
                       RecordsReauthResultsMetrics) {
  base::HistogramTester histograms;

  ShowReauthPrompt();
  RedirectGaiaChallengeTo(https_server()->GetURL(kReauthDonePath));
  ASSERT_TRUE(login_ui_test_utils::ConfirmReauthConfirmationDialog(
      browser(), kReauthDialogTimeout));
  EXPECT_EQ(WaitForReauthResult(), signin::ReauthResult::kSuccess);

  histograms.ExpectUniqueSample(
      kTransactionalReauthResultToFillPasswordHistogram,
      signin::ReauthResult::kSuccess, 1);
  histograms.ExpectTotalCount(kTransactionalReauthResultToFillPasswordHistogram,
                              1);
  histograms.ExpectTotalCount(kTransactionalReauthResultHistogram, 1);
}

// Tests an unexpected response from Gaia.
IN_PROC_BROWSER_TEST_F(SigninReauthViewControllerBrowserTest,
                       GaiaChallengeUnexpectedResponse) {
  ShowReauthPrompt();
  RedirectGaiaChallengeTo(
      https_server()->GetURL(kReauthUnexpectedResponsePath));
  ASSERT_TRUE(login_ui_test_utils::ConfirmReauthConfirmationDialog(
      browser(), kReauthDialogTimeout));
  EXPECT_EQ(WaitForReauthResult(), signin::ReauthResult::kUnexpectedResponse);
}
