// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/signin/process_dice_header_delegate_impl.h"
#include "chrome/browser/signin/signin_browser_test_base.h"
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
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/sync/base/features.h"
#include "components/sync/test/test_sync_service.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_base.h"
#include "content/public/test/test_navigation_observer.h"
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
}  // namespace

class SyncSettingsInteractiveTest
    : public SigninBrowserTestBaseT<
          WebUiInteractiveTestMixin<InteractiveBrowserTest>> {
 public:
  SyncSettingsInteractiveTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{syncer::kReplaceSyncPromosWithSignInPromos},
        /*disabled_features=*/{});
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
      )js", title_regexp.data());
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

  auto ClickButton(ui::ElementIdentifier parent_element_id,
                   DeepQuery button_query) {
    return Steps(
        ExecuteJsAt(parent_element_id, button_query, "e => e.click()"));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// TODO(crbug.com/407795729): Fix and re-enable.
IN_PROC_BROWSER_TEST_F(SyncSettingsInteractiveTest,
                       DISABLED_PressingSignOutButtonsSignsOutUser) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFirstTabContents);

  const DeepQuery turn_off_button_query = {"settings-ui",
                                           "settings-main",
                                           "settings-people-page-index",
                                           "settings-people-page",
                                           "settings-sync-account-control",
                                           "cr-button#signout-button"};

  const DeepQuery drop_down_query = {"settings-ui",
                                     "settings-main",
                                     "settings-people-page-index",
                                     "settings-people-page",
                                     "settings-sync-account-control",
                                     "cr-icon-button#dropdown-arrow"};

  std::unique_ptr<content::TestNavigationObserver> observer;
  auto url = GURL(chrome::kChromeUISignoutConfirmationURL);
  observer = std::make_unique<content::TestNavigationObserver>(url);
  observer->StartWatchingNewWebContents();

  RunTestSequence(
      Do([&]() {
        identity_test_env()->MakePrimaryAccountAvailable(
            "kTestEmail@email.com", signin::ConsentLevel::kSignin);
      }),
      InstrumentTab(kFirstTabContents),
      NavigateWebContents(kFirstTabContents, GURL(chrome::GetSettingsUrl(
                                                 chrome::kSyncSetupSubPage))),
      ExecuteJsAt(kFirstTabContents, drop_down_query,
                  "e => e.visibility === \"hidden\""),
      ClickButton(kFirstTabContents, turn_off_button_query));

  if (observer.get()) {
    observer->Wait();
    auto* signin_view_controller =
        browser()->GetFeatures().signin_view_controller();
    CHECK(signin_view_controller->ShowsModalDialog());

    auto* signout_ui = SignoutConfirmationUI::GetForTesting(
        signin_view_controller->GetModalDialogWebContentsForTesting());
    ASSERT_TRUE(signout_ui);
    signout_ui->AcceptDialogForTesting();
  }

  ASSERT_FALSE(identity_manager()->HasPrimaryAccountWithRefreshToken(
      signin::ConsentLevel::kSignin));
}

// Tests that a signed in user sees the History Sync Optin dialog after
// signing-in from the settings menu.
IN_PROC_BROWSER_TEST_F(SyncSettingsInteractiveTest,
                       ShowHistorySyncOptinDialogFromSettingsSignin) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTabId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kDiceSignInTabId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kHistorySyncOptinDialogContentsId);
  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ui::test::PollingStateObserver<bool>,
                                      kTabCountState);
  const DeepQuery kSignInButton = {"settings-ui",
                                   "settings-main",
                                   "settings-people-page-index",
                                   "settings-account-page",
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
      ClickButton(kTabId, kSignInButton), WaitForState(kTabCountState, true),
      StopObservingState(kTabCountState),
      InstrumentTab(kDiceSignInTabId, 1, browser()), Do([&]() {
        CoreAccountInfo account_info =
            identity_test_env()->MakeAccountAvailable(kTestEmail);
        // TODO(crbug.com/419203245): Investigate why using the more suitable
        // `GetSignInTabWithAccessPoint` returns null.
        content::WebContents* contents =
            browser()->tab_strip_model()->GetWebContentsAt(1);
        // Mock processing the ENABLE_SYNC signal from Gaia.
        std::unique_ptr<ProcessDiceHeaderDelegateImpl>
            process_dice_header_delegate_impl =
                ProcessDiceHeaderDelegateImpl::Create(contents);
        process_dice_header_delegate_impl->EnableSync(account_info);
      }),
      WaitForShow(SigninViewController::kHistorySyncOptinViewId),
      InstrumentNonTabWebView(kHistorySyncOptinDialogContentsId,
                              SigninViewController::kHistorySyncOptinViewId),
      WaitForStateChange(kHistorySyncOptinDialogContentsId,
                         UiElementHasAppeared(kHistoryOptinAcceptButton)),
      WaitForStateChange(kHistorySyncOptinDialogContentsId,
                         UiElementHasAppeared(kHistoryOptinRejectButton)));
  // TODO(crbug.com/419203245): Add metrics checks once they are implemented.
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
                                       "settings-account-page",
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
      ClickButton(kTabId, kContinueAsButton),
      WaitForShow(SigninViewController::kHistorySyncOptinViewId),
      InstrumentNonTabWebView(kHistorySyncOptinDialogContentsId,
                              SigninViewController::kHistorySyncOptinViewId),
      WaitForStateChange(kHistorySyncOptinDialogContentsId,
                         UiElementHasAppeared(kHistoryOptinAcceptButton)),
      WaitForStateChange(kHistorySyncOptinDialogContentsId,
                         UiElementHasAppeared(kHistoryOptinRejectButton)));
}
