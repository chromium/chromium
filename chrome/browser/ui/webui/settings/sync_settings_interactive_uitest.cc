// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/signin/process_dice_header_delegate_impl.h"
#include "chrome/browser/signin/signin_browser_test_base.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/signin/signin_view_controller.h"
#include "chrome/browser/ui/webui/signin/signout_confirmation/signout_confirmation_ui.h"
#include "chrome/browser/ui/webui/test_support/webui_interactive_test_mixin.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/interaction/webcontents_interaction_test_util.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/primary_account_change_event.h"
#include "components/signin/public/identity_manager/test_identity_manager_observer.h"
#include "components/sync/base/features.h"
#include "components/sync/test/test_sync_service.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_base.h"
#include "google_apis/gaia/gaia_switches.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "ui/base/interaction/state_observer.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

namespace {
const char kTestEmail[] = "kTestEmail@email.com";
const InteractiveBrowserTest::DeepQuery kHistoryOptinAcceptButton = {
    "history-sync-optin-app", "#acceptButton"};
const InteractiveBrowserTest::DeepQuery kHistoryOptinRejectButton = {
    "history-sync-optin-app", "#rejectButton"};

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSignoutDialogWebContentsId);

std::unique_ptr<net::test_server::HttpResponse> HandleSigninPageResponse(
    const net::test_server::HttpRequest& request) {
  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_content_type("text/html");
  http_response->set_content("Gaia signin page: " + request.relative_url);
  return http_response;
}

}  // namespace

class SyncSettingsInteractiveTest
    : public SigninBrowserTestBaseT<
          WebUiInteractiveTestMixin<InteractiveBrowserTest>> {
 public:
  SyncSettingsInteractiveTest()
      : gaia_signin_page_test_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    SigninBrowserTestBaseT<WebUiInteractiveTestMixin<InteractiveBrowserTest>>::
        SetUpCommandLine(command_line);
    ASSERT_TRUE(gaia_signin_page_test_server_.InitializeAndListen());
    const GURL& base_url = gaia_signin_page_test_server_.base_url();

    // Placeholder response to Gaia urls, so that the signin page can load with
    // no error.
    command_line->AppendSwitchASCII(switches::kGaiaUrl, base_url.spec());
    gaia_signin_page_test_server_.RegisterDefaultHandler(
        base::BindRepeating(&net::test_server::HandlePrefixedRequest,
                            signin::GetChromeSyncURLForDice({}).GetPath(),
                            base::BindRepeating(HandleSigninPageResponse)));
  }

  // Checks if a page title matches the given regexp in ecma script dialect.
  StateChange PageWithMatchingTitle(std::string_view title_regexp) {
    DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kStateChange);
    StateChange state_change;
    state_change.type = StateChange::Type::kConditionTrue;
    state_change.event = kStateChange;
    state_change.test_function = base::StringPrintf(
        R"js(
          () => /%s/.test(document.title)
        )js",
        title_regexp.data());
    state_change.continue_across_navigation = true;
    return state_change;
  }

  StateChange UiElementHasAppeared(DeepQuery element_selector) {
    DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kStateChange);
    StateChange state_change;
    state_change.type = StateChange::Type::kExistsAndConditionTrue;
    state_change.where = element_selector;
    state_change.event = kStateChange;
    state_change.test_function = "(el) => { return el.hidden == false; }";
    return state_change;
  }

  net::EmbeddedTestServer* embedded_test_server() {
    return &gaia_signin_page_test_server_;
  }

 private:
  base::test::ScopedFeatureList feature_list_{
      syncer::kReplaceSyncPromosWithSignInPromos};
  net::EmbeddedTestServer gaia_signin_page_test_server_;
};

IN_PROC_BROWSER_TEST_F(SyncSettingsInteractiveTest,
                       PressingSignOutButtonsSignsOutUser) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFirstTabContents);

  const DeepQuery turn_off_button_query = {"settings-ui",
                                           "settings-main",
                                           "settings-people-page-index",
                                           "settings-account-page",
                                           "settings-sync-account-control",
                                           "cr-button#signout-button"};

  const DeepQuery drop_down_query = {"settings-ui",
                                     "settings-main",
                                     "settings-people-page-index",
                                     "settings-account-page",
                                     "settings-sync-account-control",
                                     "cr-icon-button#dropdown-arrow"};

  signin::MakeAccountAvailable(identity_manager(),
                               identity_test_env()
                                   ->CreateAccountAvailabilityOptionsBuilder()
                                   .AsPrimary(signin::ConsentLevel::kSignin)
                                   .WithCookie()
                                   .Build("kTestEmail@gmail.com"));

  signin::TestIdentityManagerObserver primary_account_observer(
      identity_manager());
  base::test::TestFuture<signin::PrimaryAccountChangeEvent>
      primary_account_changed_future;
  primary_account_observer.SetOnPrimaryAccountChangedCallback(
      primary_account_changed_future.GetCallback());

  RunTestSequence(
      InstrumentTab(kFirstTabContents),
      NavigateWebContents(kFirstTabContents,
                          GURL(chrome::kChromeUIAccountSettingsURL)),
      EnsureNotVisible(kFirstTabContents, drop_down_query),
      ClickElement(kFirstTabContents, turn_off_button_query),
      WaitForShow(
          SigninViewController::kSignoutConfirmationDialogViewElementId),
      InstrumentNonTabWebView(
          kSignoutDialogWebContentsId,
          SigninViewController::kSignoutConfirmationDialogViewElementId),
      ClickElement(kSignoutDialogWebContentsId,
                   {"signout-confirmation-app", "cr-button#acceptButton"}));

  EXPECT_TRUE(primary_account_changed_future.Wait());
  EXPECT_FALSE(identity_manager()->HasPrimaryAccountWithRefreshToken(
      signin::ConsentLevel::kSignin));
}

// Tests that a signed in user sees the History Sync Optin dialog after
// signing-in from the settings menu.
IN_PROC_BROWSER_TEST_F(SyncSettingsInteractiveTest,
                       ShowHistorySyncOptinDialogFromSettingsSignin) {
  base::HistogramTester histogram_tester;
  // Handle the Gaia signin page.
  embedded_test_server()->StartAcceptingConnections();

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTabId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kDiceSignInTabId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kHistorySyncOptinDialogContentsId);
  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ui::test::PollingStateObserver<bool>,
                                      kTabCountState);
  const DeepQuery kSignInButton = {"settings-ui",
                                   "settings-main",
                                   "settings-people-page-index",
                                   "settings-people-page",
                                   "settings-sync-account-control",
                                   "cr-button#signIn"};
  const GURL kAccountSettingsUrl = GURL(chrome::kChromeUIAccountSettingsURL);

  RunTestSequence(
      InstrumentTab(kTabId, 0, browser()),
      NavigateWebContents(kTabId, kAccountSettingsUrl),
      WaitForStateChange(kTabId, PageWithMatchingTitle("Settings")),
      WaitForStateChange(kTabId, UiElementHasAppeared(kSignInButton)),
      PollState(kTabCountState,
                [&]() { return browser()->tab_strip_model()->count() == 2; }),
      ClickElement(kTabId, kSignInButton), WaitForState(kTabCountState, true),
      StopObservingState(kTabCountState),
      InstrumentTab(kDiceSignInTabId, 1, browser()), Do([&]() {
        // Simulate adding the account from the web.
        CoreAccountInfo account_info =
            identity_test_env()->MakeAccountAvailable(kTestEmail);
        content::WebContents* signin_tab =
            signin_ui_util::GetSignInTabWithAccessPoint(
                browser(), signin_metrics::AccessPoint::kSettings);
        EXPECT_EQ(signin_tab,
                  browser()->tab_strip_model()->GetWebContentsAt(1));
        // Mock processing the ENABLE_SYNC signal from Gaia.
        std::unique_ptr<ProcessDiceHeaderDelegateImpl>
            process_dice_header_delegate_impl =
                ProcessDiceHeaderDelegateImpl::Create(signin_tab);
        process_dice_header_delegate_impl->EnableSync(account_info);
      }),
      WaitForShow(SigninViewController::kHistorySyncOptinViewId),
      InstrumentNonTabWebView(kHistorySyncOptinDialogContentsId,
                              SigninViewController::kHistorySyncOptinViewId),
      WaitForStateChange(kHistorySyncOptinDialogContentsId,
                         UiElementHasAppeared(kHistoryOptinAcceptButton)),
      WaitForStateChange(kHistorySyncOptinDialogContentsId,
                         UiElementHasAppeared(kHistoryOptinRejectButton)));

  histogram_tester.ExpectUniqueSample("Signin.HistorySyncOptIn.Started",
                                      signin_metrics::AccessPoint::kSettings,
                                      1);
  histogram_tester.ExpectTotalCount("Signin.HistorySyncOptIn.Completed", 0);
}

// Tests that a signed in user on the web can trigger and see the History
// Sync Optin dialog.
IN_PROC_BROWSER_TEST_F(
    SyncSettingsInteractiveTest,
    ShowHistorySyncOptinDialogFromSettingsInAccountAwareMode) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTabId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kHistorySyncOptinDialogContentsId);
  const DeepQuery kContinueAsButton = {"settings-ui",
                                       "settings-main",
                                       "settings-people-page-index",
                                       "settings-people-page",
                                       "settings-sync-account-control",
                                       "cr-button#account-aware"};
  const GURL kAccountSettingsUrl = GURL(chrome::kChromeUIAccountSettingsURL);

  // Sign the user on the web only.
  AccountInfo info = signin::MakeAccountAvailable(
      identity_test_env()->identity_manager(),
      signin::AccountAvailabilityOptionsBuilder(test_url_loader_factory())
          .WithCookie()
          .Build(kTestEmail));
  signin::UpdateAccountInfoForAccount(identity_test_env()->identity_manager(),
                                      info);

  RunTestSequence(
      InstrumentTab(kTabId, 0, browser()),
      NavigateWebContents(kTabId, kAccountSettingsUrl),
      WaitForStateChange(kTabId, PageWithMatchingTitle("Settings")),
      WaitForStateChange(kTabId, UiElementHasAppeared(kContinueAsButton)),
      ClickElement(kTabId, kContinueAsButton),
      WaitForShow(SigninViewController::kHistorySyncOptinViewId),
      InstrumentNonTabWebView(kHistorySyncOptinDialogContentsId,
                              SigninViewController::kHistorySyncOptinViewId),
      WaitForStateChange(kHistorySyncOptinDialogContentsId,
                         UiElementHasAppeared(kHistoryOptinAcceptButton)),
      WaitForStateChange(kHistorySyncOptinDialogContentsId,
                         UiElementHasAppeared(kHistoryOptinRejectButton)));
}
