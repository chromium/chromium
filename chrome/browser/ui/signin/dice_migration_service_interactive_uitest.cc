// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_browser_test_base.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/signin/dice_migration_service.h"
#include "chrome/browser/ui/signin/dice_migration_service_factory.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "chrome/browser/ui/toasts/toast_view.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_managed_status_finder.h"
#include "components/signin/public/identity_manager/identity_utils.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/interactive_test.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/window/dialog_client_view.h"

namespace {
constexpr char kTestEmail[] = "test@gmail.com";

constexpr char kDiceMigrationDialogCloseReasonHistogram[] =
    "Signin.DiceMigrationDialog.CloseReason";
constexpr char kToastTriggeredHistogram[] =
    "Signin.DiceMigrationDialog.ToastTriggered";
constexpr char kToastTriggerToShowHistogram[] = "Toast.TriggeredToShow";
constexpr char kToastDismissedHistogram[] = "Toast.DiceUserMigrated.Dismissed";
constexpr char kToastActionButtonUserAction[] =
    "Toast.ActionButtonClicked.DiceUserMigrated";
constexpr char kToastCloseButtonUserAction[] =
    "Toast.CloseButtonClicked.DiceUserMigrated";
constexpr char kForceMigratedHistogram[] = "Signin.DiceMigration.ForceMigrated";

// Utility macro to implicitly sign in the user in a PRE test.
// NOTE: `test_suite` must be a subclass of
// `DiceMigrationServiceInteractiveUiTest`.
#define DICE_MIGRATION_TEST_F(test_suite, test_name)    \
  IN_PROC_BROWSER_TEST_F(test_suite, PRE_##test_name) { \
    ImplicitlySignIn();                                 \
  }                                                     \
  IN_PROC_BROWSER_TEST_F(test_suite, test_name)

class DiceMigrationServiceInteractiveUiTest : public InteractiveBrowserTest {
 public:
  DiceMigrationServiceInteractiveUiTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{switches::kOfferMigrationToDiceUsers},
        /*disabled_features=*/{switches::kForcedDiceMigration});
  }

  Profile* GetProfile() { return browser()->profile(); }

  DiceMigrationService* GetDiceMigrationService() {
    return DiceMigrationServiceFactory::GetForProfile(GetProfile());
  }

  signin::IdentityManager* GetIdentityManager() {
    return IdentityManagerFactory::GetForProfile(GetProfile());
  }

  void ImplicitlySignIn() {
    signin::MakeAccountAvailable(
        GetIdentityManager(),
        signin::AccountAvailabilityOptionsBuilder()
            .AsPrimary(signin::ConsentLevel::kSignin)
            .WithAccessPoint(signin_metrics::AccessPoint::kWebSignin)
            .Build(kTestEmail));
  }

  auto TriggerDialog() {
    return Do([&]() {
      GetDiceMigrationService()->GetDialogTriggerTimerForTesting().FireNow();
    });
  }

  auto PressEscape() {
    return Check([&]() {
      return ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_ESCAPE, false,
                                             false, false, false);
    });
  }

  auto PressCloseXButton() {
    return PressButton(views::BubbleFrameView::kCloseButtonElementId);
  }

  auto PressCancelButton() {
    return PressButton(DiceMigrationService::kCancelButtonElementId);
  }

  auto FireToastCloseTimer() {
    return Do([=, this]() {
      browser()
          ->browser_window_features()
          ->toast_controller()
          ->GetToastCloseTimerForTesting()
          ->FireNow();
    });
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::HistogramTester histogram_tester_;
  base::UserActionTester user_action_tester_;
};

DICE_MIGRATION_TEST_F(DiceMigrationServiceInteractiveUiTest,
                      CancelButtonClosesDialog) {
  // The user is implicitly signed in.
  ASSERT_TRUE(
      GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  ASSERT_FALSE(
      GetProfile()->GetPrefs()->GetBoolean(prefs::kExplicitBrowserSignin));

  RunTestSequence(TriggerDialog(),

                  WaitForShow(DiceMigrationService::kAcceptButtonElementId),

                  PressCancelButton(),

                  WaitForHide(DiceMigrationService::kAcceptButtonElementId));

  ASSERT_FALSE(GetDiceMigrationService()->GetDialogWidgetForTesting());
  EXPECT_FALSE(
      GetProfile()->GetPrefs()->GetBoolean(kDiceMigrationMigrated));
  histogram_tester_.ExpectUniqueSample(
      kDiceMigrationDialogCloseReasonHistogram,
      DiceMigrationService::DialogCloseReason::kCancelled, 1);
}

DICE_MIGRATION_TEST_F(DiceMigrationServiceInteractiveUiTest,
                      CloseXButtonClosesDialog) {
  // The user is implicitly signed in.
  ASSERT_TRUE(
      GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  ASSERT_FALSE(
      GetProfile()->GetPrefs()->GetBoolean(prefs::kExplicitBrowserSignin));

  // Set the dialog shown count to the max - 1 to show the
  // final variant which has the close-x button.
  GetProfile()->GetPrefs()->SetInteger(
      kDiceMigrationDialogShownCount,
      DiceMigrationService::kMaxDialogShownCount - 1);

  RunTestSequence(TriggerDialog(),

                  WaitForShow(DiceMigrationService::kAcceptButtonElementId),

                  PressCloseXButton(),

                  WaitForHide(DiceMigrationService::kAcceptButtonElementId));

  ASSERT_FALSE(GetDiceMigrationService()->GetDialogWidgetForTesting());
  EXPECT_FALSE(
      GetProfile()->GetPrefs()->GetBoolean(kDiceMigrationMigrated));
  histogram_tester_.ExpectUniqueSample(
      kDiceMigrationDialogCloseReasonHistogram,
      DiceMigrationService::DialogCloseReason::kClosed, 1);
}

DICE_MIGRATION_TEST_F(DiceMigrationServiceInteractiveUiTest,
                      AcceptButtonClosesDialog) {
  // The user is implicitly signed in.
  ASSERT_TRUE(
      GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  ASSERT_FALSE(
      GetProfile()->GetPrefs()->GetBoolean(prefs::kExplicitBrowserSignin));

  RunTestSequence(TriggerDialog(),

                  WaitForShow(DiceMigrationService::kAcceptButtonElementId),

                  // Press the "Got it" button.
                  PressButton(DiceMigrationService::kAcceptButtonElementId),

                  WaitForHide(DiceMigrationService::kAcceptButtonElementId));

  ASSERT_FALSE(GetDiceMigrationService()->GetDialogWidgetForTesting());
  EXPECT_TRUE(
      GetProfile()->GetPrefs()->GetBoolean(kDiceMigrationMigrated));
  histogram_tester_.ExpectUniqueSample(
      kDiceMigrationDialogCloseReasonHistogram,
      DiceMigrationService::DialogCloseReason::kAccepted, 1);
}

DICE_MIGRATION_TEST_F(DiceMigrationServiceInteractiveUiTest, EscClosesDialog) {
  // The user is implicitly signed in.
  ASSERT_TRUE(
      GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  ASSERT_FALSE(
      GetProfile()->GetPrefs()->GetBoolean(prefs::kExplicitBrowserSignin));

  RunTestSequence(
      TriggerDialog(),

      WaitForShow(DiceMigrationService::kAcceptButtonElementId),

      // Ensure the surface containing the dialog is active.
      ActivateSurface(DiceMigrationService::kAcceptButtonElementId),
      PressEscape(),

      EnsureNotPresent(DiceMigrationService::kAcceptButtonElementId));

  ASSERT_FALSE(GetDiceMigrationService()->GetDialogWidgetForTesting());
  EXPECT_FALSE(
      GetProfile()->GetPrefs()->GetBoolean(kDiceMigrationMigrated));
  histogram_tester_.ExpectUniqueSample(
      kDiceMigrationDialogCloseReasonHistogram,
      DiceMigrationService::DialogCloseReason::kEscKeyPressed, 1);
}

DICE_MIGRATION_TEST_F(DiceMigrationServiceInteractiveUiTest,
                      AvatarButtonClosesDialog) {
  // The user is implicitly signed in.
  ASSERT_TRUE(
      GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  ASSERT_FALSE(
      GetProfile()->GetPrefs()->GetBoolean(prefs::kExplicitBrowserSignin));

  RunTestSequence(TriggerDialog(),

                  WaitForShow(DiceMigrationService::kAcceptButtonElementId),

                  // Press the avatar button.
                  PressButton(kToolbarAvatarButtonElementId),

                  WaitForHide(DiceMigrationService::kAcceptButtonElementId));

  ASSERT_FALSE(GetDiceMigrationService()->GetDialogWidgetForTesting());
  EXPECT_FALSE(
      GetProfile()->GetPrefs()->GetBoolean(kDiceMigrationMigrated));
  histogram_tester_.ExpectUniqueSample(
      kDiceMigrationDialogCloseReasonHistogram,
      DiceMigrationService::DialogCloseReason::kAvatarButtonClicked, 1);
}

DICE_MIGRATION_TEST_F(DiceMigrationServiceInteractiveUiTest,
                      NavigationDoesNotCloseDialog) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTab);
  constexpr char16_t kNewUrl[] = u"chrome://version";

  // The user is implicitly signed in.
  ASSERT_TRUE(
      GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  ASSERT_FALSE(
      GetProfile()->GetPrefs()->GetBoolean(prefs::kExplicitBrowserSignin));

  RunTestSequence(TriggerDialog(),

                  WaitForShow(DiceMigrationService::kAcceptButtonElementId),

                  // Navigate to another page using the omnibox.
                  InstrumentTab(kActiveTab),
                  EnterText(kOmniboxElementId, kNewUrl),
                  Confirm(kOmniboxElementId),
                  WaitForWebContentsNavigation(kActiveTab, GURL(kNewUrl)),

                  // Ensure the dialog is still open.
                  EnsurePresent(DiceMigrationService::kAcceptButtonElementId));

  ASSERT_TRUE(GetDiceMigrationService()->GetDialogWidgetForTesting());
  histogram_tester_.ExpectTotalCount(kDiceMigrationDialogCloseReasonHistogram,
                                     0);
}

DICE_MIGRATION_TEST_F(DiceMigrationServiceInteractiveUiTest,
                      ClickingElsewhereDoesNotCloseDialog) {
  // The user is implicitly signed in.
  ASSERT_TRUE(
      GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  ASSERT_FALSE(
      GetProfile()->GetPrefs()->GetBoolean(prefs::kExplicitBrowserSignin));

  RunTestSequence(TriggerDialog(),

                  WaitForShow(DiceMigrationService::kAcceptButtonElementId),

                  // Open and close the three-dot menu.
                  PressButton(kToolbarAppMenuButtonElementId),
                  WaitForShow(AppMenuModel::kProfileMenuItem), PressEscape(),
                  WaitForHide(AppMenuModel::kProfileMenuItem),

                  // Ensure the dialog is still open.
                  EnsurePresent(DiceMigrationService::kAcceptButtonElementId));

  ASSERT_TRUE(GetDiceMigrationService()->GetDialogWidgetForTesting());
  histogram_tester_.ExpectTotalCount(kDiceMigrationDialogCloseReasonHistogram,
                                     0);
}

DICE_MIGRATION_TEST_F(DiceMigrationServiceInteractiveUiTest,
                      NonFinalDialogVariant) {
  // The user is implicitly signed in.
  ASSERT_TRUE(
      GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  ASSERT_FALSE(
      GetProfile()->GetPrefs()->GetBoolean(prefs::kExplicitBrowserSignin));

  RunTestSequence(
      TriggerDialog(),

      WaitForShow(DiceMigrationService::kAcceptButtonElementId),

      // The non-final dialog variant has a cancel button.
      EnsurePresent(DiceMigrationService::kCancelButtonElementId),
      // ... but not the close-x button.
      EnsureNotPresent(views::BubbleFrameView::kCloseButtonElementId),

      PressCancelButton(),

      EnsureNotPresent(DiceMigrationService::kAcceptButtonElementId));
}

DICE_MIGRATION_TEST_F(DiceMigrationServiceInteractiveUiTest,
                      FinalDialogVariant) {
  // The user is implicitly signed in.
  ASSERT_TRUE(
      GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  ASSERT_FALSE(
      GetProfile()->GetPrefs()->GetBoolean(prefs::kExplicitBrowserSignin));

  // Set the dialog shown count to the max - 1 to show the final variant.
  GetProfile()->GetPrefs()->SetInteger(
      kDiceMigrationDialogShownCount,
      DiceMigrationService::kMaxDialogShownCount - 1);

  RunTestSequence(
      TriggerDialog(),

      WaitForShow(DiceMigrationService::kAcceptButtonElementId),

      // The final dialog variant does not have a cancel button.
      EnsureNotPresent(DiceMigrationService::kCancelButtonElementId),
      // ... but has the close-x button.
      EnsurePresent(views::BubbleFrameView::kCloseButtonElementId),

      PressCloseXButton(),

      EnsureNotPresent(DiceMigrationService::kAcceptButtonElementId));
}

DICE_MIGRATION_TEST_F(DiceMigrationServiceInteractiveUiTest, ShowToast) {
  // The user is implicitly signed in.
  ASSERT_TRUE(
      GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  ASSERT_FALSE(
      GetProfile()->GetPrefs()->GetBoolean(prefs::kExplicitBrowserSignin));

  RunTestSequence(TriggerDialog(),

                  WaitForShow(DiceMigrationService::kAcceptButtonElementId),

                  // Press the "Got it" button.
                  PressButton(DiceMigrationService::kAcceptButtonElementId),

                  WaitForHide(DiceMigrationService::kAcceptButtonElementId),

                  WaitForShow(toasts::ToastView::kToastViewId),

                  // The toast should auto dismiss when the timer goes off.
                  FireToastCloseTimer(),
                  WaitForHide(toasts::ToastView::kToastViewId));

  histogram_tester_.ExpectUniqueSample(kToastTriggeredHistogram, true, 1);
  histogram_tester_.ExpectUniqueSample(kToastTriggerToShowHistogram,
                                       ToastId::kDiceUserMigrated, 1);
}

DICE_MIGRATION_TEST_F(DiceMigrationServiceInteractiveUiTest,
                      ToastActionButton) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTab);

  // The user is implicitly signed in.
  ASSERT_TRUE(
      GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  ASSERT_FALSE(
      GetProfile()->GetPrefs()->GetBoolean(prefs::kExplicitBrowserSignin));

  RunTestSequence(
      TriggerDialog(),

      WaitForShow(DiceMigrationService::kAcceptButtonElementId),

      // Press the "Got it" button.
      PressButton(DiceMigrationService::kAcceptButtonElementId),

      WaitForHide(DiceMigrationService::kAcceptButtonElementId),

      WaitForShow(toasts::ToastView::kToastViewId),

      // Pressing the toast action button should open the settings
      // page.
      InstrumentTab(kActiveTab),
      PressButton(toasts::ToastView::kToastActionButton),
      WaitForWebContentsNavigation(
          kActiveTab, chrome::GetSettingsUrl(chrome::kSyncSetupSubPage)),

      WaitForHide(toasts::ToastView::kToastViewId));

  histogram_tester_.ExpectUniqueSample(
      kToastDismissedHistogram, toasts::ToastCloseReason::kActionButton, 1);
  EXPECT_EQ(user_action_tester_.GetActionCount(kToastActionButtonUserAction),
            1);
}

DICE_MIGRATION_TEST_F(DiceMigrationServiceInteractiveUiTest, ToastCloseButton) {
  // The user is implicitly signed in.
  ASSERT_TRUE(
      GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  ASSERT_FALSE(
      GetProfile()->GetPrefs()->GetBoolean(prefs::kExplicitBrowserSignin));

  RunTestSequence(TriggerDialog(),

                  WaitForShow(DiceMigrationService::kAcceptButtonElementId),

                  // Press the "Got it" button.
                  PressButton(DiceMigrationService::kAcceptButtonElementId),

                  WaitForHide(DiceMigrationService::kAcceptButtonElementId),

                  WaitForShow(toasts::ToastView::kToastViewId),

                  PressButton(toasts::ToastView::kToastCloseButton),

                  WaitForHide(toasts::ToastView::kToastViewId));

  histogram_tester_.ExpectUniqueSample(
      kToastDismissedHistogram, toasts::ToastCloseReason::kCloseButton, 1);
  EXPECT_EQ(user_action_tester_.GetActionCount(kToastCloseButtonUserAction), 1);
}

DICE_MIGRATION_TEST_F(DiceMigrationServiceInteractiveUiTest,
                      ToastDoesNotCloseOnNavigation) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTab);
  constexpr char16_t kNewUrl[] = u"chrome://version";

  // The user is implicitly signed in.
  ASSERT_TRUE(
      GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  ASSERT_FALSE(
      GetProfile()->GetPrefs()->GetBoolean(prefs::kExplicitBrowserSignin));

  RunTestSequence(TriggerDialog(),

                  WaitForShow(DiceMigrationService::kAcceptButtonElementId),

                  // Press the "Got it" button.
                  PressButton(DiceMigrationService::kAcceptButtonElementId),

                  WaitForHide(DiceMigrationService::kAcceptButtonElementId),

                  WaitForShow(toasts::ToastView::kToastViewId),

                  // Navigate to another page using the omnibox.
                  InstrumentTab(kActiveTab),
                  EnterText(kOmniboxElementId, kNewUrl),
                  Confirm(kOmniboxElementId),
                  WaitForWebContentsNavigation(kActiveTab, GURL(kNewUrl)),

                  // The toast should still be visible.
                  EnsurePresent(toasts::ToastView::kToastViewId));
}

DICE_MIGRATION_TEST_F(DiceMigrationServiceInteractiveUiTest,
                      ToastDoesNotCloseOnTabSwitch) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTab);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewTab);
  constexpr char16_t kNewUrl[] = u"chrome://version";

  // The user is implicitly signed in.
  ASSERT_TRUE(
      GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  ASSERT_FALSE(
      GetProfile()->GetPrefs()->GetBoolean(prefs::kExplicitBrowserSignin));

  RunTestSequence(InstrumentTab(kActiveTab),

                  TriggerDialog(),

                  WaitForShow(DiceMigrationService::kAcceptButtonElementId),

                  // Press the "Got it" button.
                  PressButton(DiceMigrationService::kAcceptButtonElementId),

                  WaitForHide(DiceMigrationService::kAcceptButtonElementId),

                  WaitForShow(toasts::ToastView::kToastViewId),

                  // Switch to another tab.
                  AddInstrumentedTab(kNewTab, GURL(kNewUrl)),

                  // The toast should still be visible because the timeout
                  // hasn't passed yet.
                  EnsurePresent(toasts::ToastView::kToastViewId),

                  // Switch back to the original tab.
                  SelectTab(kTabStripElementId, 0),

                  // The toast should still be visible because the timeout
                  // hasn't passed yet.
                  EnsurePresent(toasts::ToastView::kToastViewId));
}

DICE_MIGRATION_TEST_F(DiceMigrationServiceInteractiveUiTest,
                      IdentityPillExpandsWhenShowingDialog) {
  // The user is implicitly signed in.
  ASSERT_TRUE(
      GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  ASSERT_FALSE(
      GetProfile()->GetPrefs()->GetBoolean(prefs::kExplicitBrowserSignin));

  RunTestSequence(
      // The identity pill is not expanded by default.
      CheckViewProperty(kToolbarAvatarButtonElementId,
                        &views::LabelButton::GetText, u""),

      TriggerDialog(),

      WaitForShow(DiceMigrationService::kAcceptButtonElementId),

      // The identity pill is expanded when the dialog is shown.
      CheckViewProperty(kToolbarAvatarButtonElementId,
                        &views::LabelButton::GetText, u"test@gmail.com"),

      // Press the avatar button.
      PressButton(kToolbarAvatarButtonElementId),

      WaitForHide(DiceMigrationService::kAcceptButtonElementId),

      // The identity pill is collapsed again.
      CheckViewProperty(kToolbarAvatarButtonElementId,
                        &views::LabelButton::GetText, u""));
}

class DiceMigrationServiceForcedMigrationInteractiveUiTest
    : public DiceMigrationServiceInteractiveUiTest {
  base::test::ScopedFeatureList scoped_feature_list_{
      switches::kForcedDiceMigration};
};

IN_PROC_BROWSER_TEST_F(DiceMigrationServiceForcedMigrationInteractiveUiTest,
                       PRE_AccountWithAccountManagement) {
  ImplicitlySignIn();

  // The user has accepted account management.
  enterprise_util::SetUserAcceptedAccountManagement(GetProfile(), true);
  ASSERT_TRUE(enterprise_util::UserAcceptedAccountManagement(GetProfile()));
}

IN_PROC_BROWSER_TEST_F(DiceMigrationServiceForcedMigrationInteractiveUiTest,
                       AccountWithAccountManagement) {
  // The user is explicitly signed in.
  ASSERT_TRUE(
      GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  ASSERT_TRUE(
      GetProfile()->GetPrefs()->GetBoolean(prefs::kExplicitBrowserSignin));
  histogram_tester_.ExpectUniqueSample(kForceMigratedHistogram, true, 1);

  RunTestSequence(If(
                      [&]() {
                        return GetDiceMigrationService()
                            ->GetDialogTriggerTimerForTesting()
                            .IsRunning();
                      },
                      Then(TriggerDialog())),

                  // The toast should be shown following the forced migration.
                  WaitForShow(toasts::ToastView::kToastViewId),

                  // The toast should auto dismiss when the timer goes off.
                  FireToastCloseTimer(),
                  WaitForHide(toasts::ToastView::kToastViewId));

  histogram_tester_.ExpectUniqueSample(kToastTriggerToShowHistogram,
                                       ToastId::kDiceUserMigrated, 1);
}

}  // namespace
