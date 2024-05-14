// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/signin/signin_reauth_view_controller.h"

#include <memory>
#include <optional>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/strings/escape.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/reauth_result.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/sync/test/integration/encryption_helper.h"
#include "chrome/browser/trusted_vault/trusted_vault_encryption_keys_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/signin/signin_view_controller.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/webui/signin/login_ui_test_utils.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/base/features.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/gaia_switches.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_switches.h"

using ::testing::ElementsAre;

namespace {

const char kReauthUserActionHistogramName[] =
    "Signin.TransactionalReauthUserAction";
const char kReauthUserActionToFillPasswordHistogramName[] =
    "Signin.TransactionalReauthUserAction.ToFillPassword";

const base::TimeDelta kReauthDialogTimeout = base::Seconds(30);
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

std::unique_ptr<net::test_server::BasicHttpResponse> CreateNonEmptyResponse(
    net::HttpStatusCode code) {
  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(code);
  http_response->set_content("<html>");
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
      base::UnescapeBinaryURLComponent(request_url.query_piece());

  if (parameter.empty()) {
    // Parameterless request redirects to the fake challenge page.
    return CreateRedirectResponse(base_url.Resolve(kChallengePath));
  }

  if (parameter == "done") {
    // On success, the reauth returns HTTP_NO_CONTENT response.
    return CreateEmptyResponse(net::HTTP_NO_CONTENT);
  }

  if (parameter == "unexpected") {
    // Returns a response that isn't expected by Chrome. Note that we shouldn't
    // return an empty response here because that will result in an error page
    // being committed for the navigation.
    return CreateNonEmptyResponse(net::HTTP_NOT_IMPLEMENTED);
  }

  NOTREACHED_IN_MIGRATION();
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
  raw_ptr<SigninReauthViewController, AcrossTasksDanglingUntriaged> controller_;
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

    account_id_ =
        signin::SetPrimaryAccount(identity_manager(), "alice@gmail.com",
                                  signin::ConsentLevel::kSignin)
            .account_id;

    reauth_result_loop_ = std::make_unique<base::RunLoop>();
    InProcessBrowserTest::SetUpOnMainThread();
  }

  void ShowReauthPrompt(
      signin_metrics::ReauthAccessPoint access_point =
          signin_metrics::ReauthAccessPoint::kAutofillDropdown) {
    abort_handle_ = browser()->signin_view_controller()->ShowReauthPrompt(
        account_id_, access_point,
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

  std::optional<signin::ReauthResult> WaitForReauthResult() {
    reauth_result_loop_->Run();
    return reauth_result_;
  }

  // The test cannot depend on Views implementation so it simulates clicking on
  // the close button through calling the close event.
  void SimulateCloseButtonClick() {
    signin_reauth_view_controller()->OnModalDialogClosed();
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
        signin_view_controller->GetModalDialogForTesting());
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
  std::optional<signin::ReauthResult> reauth_result_;
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
                                      TabCloseTypes::CLOSE_USER_GESTURE);
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
      signin_reauth_view_controller()->GetModalDialogWebContentsForTesting();
  ASSERT_TRUE(content::ExecJs(
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

// Tests the sync encryption-related Javascript APIs exercised by the Gaia
// reauth challenge.
// Regression test for crbug.com/1266415.
IN_PROC_BROWSER_TEST_F(SigninReauthViewControllerBrowserTest,
                       SetSyncEncryptionKeysDuringReauthChallenge) {
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
      signin_reauth_view_controller()->GetModalDialogWebContentsForTesting();

  TrustedVaultEncryptionKeysTabHelper* encryption_keys_tab_helper =
      TrustedVaultEncryptionKeysTabHelper::FromWebContents(target_contents);
  ASSERT_NE(encryption_keys_tab_helper, nullptr);
  EXPECT_TRUE(encryption_keys_tab_helper->HasEncryptionKeysApiForTesting(
      target_contents->GetPrimaryMainFrame()));

  // The invocation of the API, even with dummy values, should propagate until
  // TrustedVaultClient and its observers.
  TrustedVaultKeysChangedStateChecker keys_added_checker(
      SyncServiceFactory::GetAsSyncServiceImplForProfileForTesting(
          browser()->profile()));
  EXPECT_TRUE(content::ExecJs(
      target_contents,
      "chrome.setSyncEncryptionKeys(() => {}, \"\", [new ArrayBuffer()], 0);"));
  EXPECT_TRUE(keys_added_checker.Wait());
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
      signin_reauth_view_controller()->GetModalDialogWebContentsForTesting();
  content::TestNavigationObserver new_tab_observer(nullptr);
  new_tab_observer.StartWatchingNewWebContents();
  ASSERT_TRUE(content::ExecJs(
      dialog_contents, "document.getElementsByTagName('a')[0].click();"));
  new_tab_observer.Wait();

  content::WebContents* new_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_NE(new_contents, original_contents);
  EXPECT_NE(new_contents, dialog_contents);
  EXPECT_EQ(new_contents->GetLastCommittedURL(),
            https_server()->GetURL("/title1.html"));
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
  EXPECT_EQ(
      target_contents,
      signin_reauth_view_controller()->GetModalDialogWebContentsForTesting());
  EXPECT_EQ(target_contents->GetLastCommittedURL(), target_url);

  ASSERT_TRUE(content::ExecJs(
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
  EXPECT_EQ(tab_strip_model->GetActiveWebContents()->GetLastCommittedURL(),
            target_url);
  tab_strip_model->CloseWebContentsAt(tab_strip_model->active_index(),
                                      TabCloseTypes::CLOSE_USER_GESTURE);
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

// TODO(crbug.com/40284051): Remove kNoPasskeySyncing path after metadata
// syncing is enabled by default.
enum HasPasskeySyncing {
  kHasPasskeySyncing,
  kNoPasskeySyncing,
};

class SigninReauthViewControllerMessageBrowserTest
    : public SigninReauthViewControllerBrowserTest,
      public testing::WithParamInterface<HasPasskeySyncing> {
 public:
  SigninReauthViewControllerMessageBrowserTest() {
    if (GetParam() == kHasPasskeySyncing) {
      scoped_feature_list_.InitAndEnableFeature(
          syncer::kSyncWebauthnCredentials);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          syncer::kSyncWebauthnCredentials);
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(SigninReauthViewControllerMessageBrowserTest,
                       MessageIfPasswordWasSavedLocally) {
  // The AccessPoint specifies that the password was already saved locally
  // before the reauth prompt was shown.
  ShowReauthPrompt(
      signin_metrics::ReauthAccessPoint::kPasswordSaveLocallyBubble);
  content::WebContents* confirmation_dialog_contents =
      signin_reauth_view_controller()->GetModalDialogWebContentsForTesting();
  content::TestNavigationObserver navigation_observer(
      confirmation_dialog_contents);
  navigation_observer.Wait();

  std::string dialog_message =
      content::EvalJs(confirmation_dialog_contents,
                      "document.querySelector('signin-reauth-app').shadowRoot."
                      "querySelector('.message-container').innerText")
          .ExtractString();
  // The dialog message should specify that the password was already saved
  // locally.
  EXPECT_EQ(
      dialog_message,
      l10n_util::GetStringUTF8(
          GetParam() == kHasPasskeySyncing
              ? IDS_ACCOUNT_PASSWORDS_WITH_PASSKEYS_REAUTH_DESC_ALREADY_SAVED_LOCALLY
              : IDS_ACCOUNT_PASSWORDS_REAUTH_DESC_ALREADY_SAVED_LOCALLY));
}

IN_PROC_BROWSER_TEST_P(SigninReauthViewControllerMessageBrowserTest,
                       MessageIfPasswordWasNotSavedLocally) {
  // The AccessPoint specifies that the password was NOT already saved locally
  // before the reauth prompt was shown.
  ShowReauthPrompt(signin_metrics::ReauthAccessPoint::kPasswordSaveBubble);
  content::WebContents* confirmation_dialog_contents =
      signin_reauth_view_controller()->GetModalDialogWebContentsForTesting();
  content::TestNavigationObserver navigation_observer(
      confirmation_dialog_contents);
  navigation_observer.Wait();

  std::string dialog_message =
      content::EvalJs(confirmation_dialog_contents,
                      "document.querySelector('signin-reauth-app').shadowRoot."
                      "querySelector('.message-container').innerText")
          .ExtractString();
  // The dialog message should be the regular one.
  EXPECT_EQ(dialog_message,
            l10n_util::GetStringUTF8(
                GetParam() == kHasPasskeySyncing
                    ? IDS_ACCOUNT_PASSWORDS_WITH_PASSKEYS_REAUTH_DESC
                    : IDS_ACCOUNT_PASSWORDS_REAUTH_DESC));
}

INSTANTIATE_TEST_SUITE_P(/* no prefix */,
                         SigninReauthViewControllerMessageBrowserTest,
                         testing::Values(kHasPasskeySyncing,
                                         kNoPasskeySyncing));

class SigninReauthViewControllerDarkModeBrowserTest
    : public SigninReauthViewControllerBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kForceDarkMode);
    SigninReauthViewControllerBrowserTest::SetUpCommandLine(command_line);
  }
};

// Tests the light mode is enforced for the reauth confirmation dialog even if
// the dark mode is enabled.
IN_PROC_BROWSER_TEST_F(SigninReauthViewControllerDarkModeBrowserTest,
                       ConfirmationDialogDarkModeDisabled) {
  ShowReauthPrompt();
  content::WebContents* confirmation_dialog_contents =
      signin_reauth_view_controller()->GetModalDialogWebContentsForTesting();
  content::TestNavigationObserver navigation_observer(
      confirmation_dialog_contents);
  navigation_observer.WaitForNavigationFinished();

  EXPECT_EQ(content::EvalJs(
                confirmation_dialog_contents,
                "window.matchMedia('(prefers-color-scheme: dark)').matches"),
            false);
}

class SigninReauthViewControllerFencedFrameBrowserTest
    : public SigninReauthViewControllerBrowserTest {
 public:
  SigninReauthViewControllerFencedFrameBrowserTest() = default;
  ~SigninReauthViewControllerFencedFrameBrowserTest() override = default;

  content::test::FencedFrameTestHelper& fenced_frame_test_helper() {
    return fenced_frame_test_helper_;
  }

 private:
  content::test::FencedFrameTestHelper fenced_frame_test_helper_;
};

// Tests that SigninReauthViewController proceeds Reauth only with the primary
// main frame.
IN_PROC_BROWSER_TEST_F(SigninReauthViewControllerFencedFrameBrowserTest,
                       FencedFrame) {
  const GURL target_url = https_server()->GetURL("/title1.html");
  ShowReauthPrompt();
  RedirectGaiaChallengeTo(target_url);

  // Reauth page is shown along with the primary main frame navigation.
  ReauthTestObserver reauth_observer(signin_reauth_view_controller());
  ASSERT_TRUE(login_ui_test_utils::ConfirmReauthConfirmationDialog(
      browser(), kReauthDialogTimeout));
  reauth_observer.WaitUntilGaiaReauthPageIsShown();

  content::WebContents* target_contents =
      signin_reauth_view_controller()->GetModalDialogWebContentsForTesting();
  const GURL fenced_frame_url =
      https_server()->GetURL("/fenced_frames/title1.html");
  base::HistogramTester histogram_tester;
  // Creates a fenced frame inside the primary main frame.
  content::RenderFrameHost* fenced_frame =
      fenced_frame_test_helper().CreateFencedFrame(
          &target_contents->GetPrimaryPage().GetMainDocument(),
          fenced_frame_url);
  EXPECT_EQ(fenced_frame->GetLastCommittedURL(), fenced_frame_url);
  // Fenced Frame navigation doesn't have any actions for Reauth.
  histogram_tester.ExpectBucketCount(
      kReauthUserActionHistogramName,
      SigninReauthViewController::UserAction::kClickNextButton, 0);

  SimulateCloseButtonClick();
  EXPECT_EQ(WaitForReauthResult(), signin::ReauthResult::kDismissedByUser);
}
