// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/signin/signin_browser_test_base.h"
#include "chrome/browser/sync/sync_service_factory.h"
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
#include "components/sync/test/test_sync_service.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_base.h"
#include "content/public/test/test_navigation_observer.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

class SyncSettingsInteractiveTest
    : public SigninBrowserTestBaseT<
          WebUiInteractiveTestMixin<InteractiveBrowserTest>> {
 public:
  SyncSettingsInteractiveTest() = default;

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
    state_change.type = StateChange::Type::kExists;
    state_change.where = element_selector;
    state_change.event = kStateChange;
    return state_change;
  }

  auto ClickButton(ui::ElementIdentifier parent_element_id,
                   DeepQuery button_query) {
    return Steps(
        ExecuteJsAt(parent_element_id, button_query, "e => e.click()"));
  }

 private:
  base::test::ScopedFeatureList feature_list_{
      switches::kEnableHistorySyncOptin};
};

// TODO(crbug.com/407795729): Fix and re-enable.
IN_PROC_BROWSER_TEST_F(SyncSettingsInteractiveTest,
                       DISABLED_PressingSignOutButtonsSignsOutUser) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFirstTabContents);

  const DeepQuery turn_off_button_query = {"settings-ui",
                                           "settings-main",
                                           "settings-basic-page",
                                           "settings-people-page",
                                           "settings-sync-account-control",
                                           "cr-button#signout-button"};

  const DeepQuery drop_down_query = {"settings-ui",
                                     "settings-main",
                                     "settings-basic-page",
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
    auto* signin_view_controller = browser()->signin_view_controller();
    CHECK(signin_view_controller->ShowsModalDialog());

    auto* signout_ui = SignoutConfirmationUI::GetForTesting(
        signin_view_controller->GetModalDialogWebContentsForTesting());
    ASSERT_TRUE(signout_ui);
    signout_ui->AcceptDialogForTesting();
  }

  ASSERT_FALSE(identity_manager()->HasPrimaryAccountWithRefreshToken(
      signin::ConsentLevel::kSignin));
}

// Tests that a signed in user that does not sync the history data type,
// can open the History Sync Optin dialog from the settings menu.
// Approving the dialog enables the history syncing preference.
IN_PROC_BROWSER_TEST_F(SyncSettingsInteractiveTest,
                       ShowAndAcceptHistorySyncOptinDialogFromSettings) {
  identity_test_env()->MakePrimaryAccountAvailable(
      "kTestEmail@email.com", signin::ConsentLevel::kSignin);
  ASSERT_FALSE(SyncServiceFactory::GetForProfile(browser()->profile())
                   ->GetUserSettings()
                   ->GetSelectedTypes()
                   .Has(syncer::UserSelectableType::kHistory));

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTabId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kHistorySyncOptinDialogContentsId);
  const DeepQuery kTurnHistorySyncOn = {"settings-ui",
                                        "settings-main",
                                        "settings-basic-page",
                                        "settings-people-page",
                                        "settings-sync-account-control",
                                        "cr-button#sync-button"};
  const DeepQuery kHistoryOptinAcceptButton = {"history-sync-optin-app",
                                               "#acceptButton"};
  const DeepQuery kHistoryOptinRejectButton = {"history-sync-optin-app",
                                               "#rejectButton"};
  const GURL kSyncSettingsUrl = GURL("chrome://settings/syncSetup");

  RunTestSequence(
      InstrumentTab(kTabId, 0, browser()),
      NavigateWebContents(kTabId, kSyncSettingsUrl),
      WaitForStateChange(kTabId, PageWithMatchingTitle("Settings")),
      WaitForStateChange(kTabId, UiElementHasAppeared(kTurnHistorySyncOn)),
      ClickButton(kTabId, kTurnHistorySyncOn),
      WaitForShow(SigninViewController::kHistorySyncOptinViewId),
      InstrumentNonTabWebView(kHistorySyncOptinDialogContentsId,
                              SigninViewController::kHistorySyncOptinViewId),
      WaitForStateChange(kHistorySyncOptinDialogContentsId,
                         UiElementHasAppeared(kHistoryOptinAcceptButton)),
      WaitForStateChange(kHistorySyncOptinDialogContentsId,
                         UiElementHasAppeared(kHistoryOptinRejectButton)),
      ClickButton(kHistorySyncOptinDialogContentsId, kHistoryOptinAcceptButton),
      WaitForHide(SigninViewController::kHistorySyncOptinViewId));

  EXPECT_TRUE(SyncServiceFactory::GetForProfile(browser()->profile())
                  ->GetUserSettings()
                  ->GetSelectedTypes()
                  .Has(syncer::UserSelectableType::kHistory));
  // TODO(crbug.com/419203245): Add metrics checks once they are implemented.
}
