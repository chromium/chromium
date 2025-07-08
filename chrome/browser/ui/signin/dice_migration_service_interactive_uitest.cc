// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_browser_test_base.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/signin/dice_migration_service.h"
#include "chrome/browser/ui/signin/dice_migration_service_factory.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_utils.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/interactive_test.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/window/dialog_client_view.h"

namespace {
constexpr char kTestEmail[] = "test@gmail.com";

class DiceMigrationServiceInteractiveUiTest
    : public SigninBrowserTestBaseT<InteractiveBrowserTest> {
 public:
  void SetUpOnMainThread() override {
    SigninBrowserTestBaseT<InteractiveBrowserTest>::SetUpOnMainThread();
    // Implicitly sign in.
    AccountInfo account_info = signin::MakeAccountAvailable(
        identity_manager(),
        signin::AccountAvailabilityOptionsBuilder()
            .AsPrimary(signin::ConsentLevel::kSignin)
            // `kWebSignin` is not explicit signin.
            .WithAccessPoint(signin_metrics::AccessPoint::kWebSignin)
            .Build(kTestEmail));
  }

  DiceMigrationService* GetDiceMigrationService() {
    return DiceMigrationServiceFactory::GetForProfile(GetProfile());
  }

  auto TriggerDialog() {
    return Do([&]() {
      GetDiceMigrationService()->ShowDiceMigrationOfferDialogIfUserEligible();
    });
  }

  auto PressEscape() {
    return Check([&]() {
      return ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_ESCAPE, false,
                                             false, false, false);
    });
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      switches::kOfferMigrationToDiceUsers};
};

IN_PROC_BROWSER_TEST_F(DiceMigrationServiceInteractiveUiTest,
                       CloseXButtonClosesDialog) {
  RunTestSequence(
      TriggerDialog(),

      WaitForShow(DiceMigrationService::kAcceptButtonElementId),

      // Simulate clicking the close-x button.
      Do([this]() {
        GetDiceMigrationService()->GetDialogWidgetForTesting()->CloseWithReason(
            views::Widget::ClosedReason::kCloseButtonClicked);
      }),

      WaitForHide(DiceMigrationService::kAcceptButtonElementId));

  ASSERT_FALSE(GetDiceMigrationService()->IsDialogShowing());
  ASSERT_FALSE(GetDiceMigrationService()->GetDialogWidgetForTesting());
}

IN_PROC_BROWSER_TEST_F(DiceMigrationServiceInteractiveUiTest,
                       AcceptButtonClosesDialog) {
  RunTestSequence(TriggerDialog(),

                  WaitForShow(DiceMigrationService::kAcceptButtonElementId),

                  // Press the "Got it" button.
                  PressButton(DiceMigrationService::kAcceptButtonElementId),

                  WaitForHide(DiceMigrationService::kAcceptButtonElementId));

  ASSERT_FALSE(GetDiceMigrationService()->IsDialogShowing());
  ASSERT_FALSE(GetDiceMigrationService()->GetDialogWidgetForTesting());
}

IN_PROC_BROWSER_TEST_F(DiceMigrationServiceInteractiveUiTest, EscClosesDialog) {
  RunTestSequence(
      TriggerDialog(),

      WaitForShow(DiceMigrationService::kAcceptButtonElementId),

      // Ensure the surface containing the dialog is active.
      ActivateSurface(DiceMigrationService::kAcceptButtonElementId),
      PressEscape(),

      EnsureNotPresent(DiceMigrationService::kAcceptButtonElementId));

  ASSERT_FALSE(GetDiceMigrationService()->IsDialogShowing());
  ASSERT_FALSE(GetDiceMigrationService()->GetDialogWidgetForTesting());
}

IN_PROC_BROWSER_TEST_F(DiceMigrationServiceInteractiveUiTest,
                       NavigationDoesNotCloseDialog) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTab);
  constexpr char16_t kNewUrl[] = u"chrome://version";

  RunTestSequence(TriggerDialog(),

                  WaitForShow(DiceMigrationService::kAcceptButtonElementId),

                  // Navigate to another page using the omnibox.
                  InstrumentTab(kActiveTab),
                  EnterText(kOmniboxElementId, kNewUrl),
                  Confirm(kOmniboxElementId),
                  WaitForWebContentsNavigation(kActiveTab, GURL(kNewUrl)),

                  // Ensure the dialog is still open.
                  EnsurePresent(DiceMigrationService::kAcceptButtonElementId));

  ASSERT_TRUE(GetDiceMigrationService()->IsDialogShowing());
}

IN_PROC_BROWSER_TEST_F(DiceMigrationServiceInteractiveUiTest,
                       ClickingElsewhereDoesNotCloseDialog) {
  RunTestSequence(TriggerDialog(),

                  WaitForShow(DiceMigrationService::kAcceptButtonElementId),

                  // Open and close the three-dot menu.
                  PressButton(kToolbarAppMenuButtonElementId),
                  WaitForShow(AppMenuModel::kProfileMenuItem), PressEscape(),
                  WaitForHide(AppMenuModel::kProfileMenuItem),

                  // Ensure the dialog is still open.
                  EnsurePresent(DiceMigrationService::kAcceptButtonElementId));

  ASSERT_TRUE(GetDiceMigrationService()->IsDialogShowing());
}

}  // namespace
