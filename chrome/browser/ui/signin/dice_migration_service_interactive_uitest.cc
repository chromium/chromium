// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/signin/dice_migration_service.h"
#include "chrome/browser/ui/signin/dice_migration_service_factory.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "chrome/browser/ui/toasts/toast_view.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_test.h"
#include "ui/gfx/scoped_animation_duration_scale_mode.h"

namespace {
constexpr char kTestEmail[] = "test@gmail.com";

constexpr char kToastTriggerToShowHistogram[] = "Toast.TriggeredToShow";
constexpr char kToastDismissedHistogram[] = "Toast.DiceUserMigrated.Dismissed";
constexpr char kToastActionButtonUserAction[] =
    "Toast.ActionButtonClicked.DiceUserMigrated";
constexpr char kToastCloseButtonUserAction[] =
    "Toast.CloseButtonClicked.DiceUserMigrated";
constexpr char kForceMigratedHistogram[] = "Signin.DiceMigration.ForceMigrated";

// Utility macro to implicitly sign in the user in a PRE test.
// NOTE: `test_suite` must be a subclass of
// `DiceMigrationServiceForcedMigrationInteractiveUiTest`.
#define DICE_MIGRATION_TEST_F(test_suite, test_name)                       \
  IN_PROC_BROWSER_TEST_F(test_suite, PRE_##test_name) {                    \
    ImplicitlySignIn();                                                    \
    enterprise_util::SetUserAcceptedAccountManagement(GetProfile(), true); \
  }                                                                        \
  IN_PROC_BROWSER_TEST_F(test_suite, test_name)

class DiceMigrationServiceForcedMigrationInteractiveUiTest
    : public InteractiveBrowserTest {
 public:
  Profile* GetProfile() { return browser()->profile(); }

  DiceMigrationService* GetDiceMigrationService() {
    DiceMigrationService* service =
        DiceMigrationServiceFactory::GetForProfileIfExists(GetProfile());
    EXPECT_TRUE(service);
    return service;
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

  auto TriggerToastTimer() {
    return If(
        [&]() {
          return GetDiceMigrationService()
              ->GetToastTriggerTimerForTesting()
              .IsRunning();
        },
        Then(Do([&]() {
          GetDiceMigrationService()->GetToastTriggerTimerForTesting().FireNow();
        })));
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
  base::HistogramTester histogram_tester_;
  base::UserActionTester user_action_tester_;

 private:
  gfx::ScopedAnimationDurationScaleMode zero_duration_mode_ =
      gfx::ScopedAnimationDurationScaleMode(
          gfx::ScopedAnimationDurationScaleMode::ZERO_DURATION);
};

DICE_MIGRATION_TEST_F(DiceMigrationServiceForcedMigrationInteractiveUiTest,
                      ToastActionButton) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTab);

  // The user is explicitly signed in.
  ASSERT_TRUE(
      GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  ASSERT_TRUE(
      GetProfile()->GetPrefs()->GetBoolean(prefs::kExplicitBrowserSignin));

  RunTestSequence(
      TriggerToastTimer(),

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

DICE_MIGRATION_TEST_F(DiceMigrationServiceForcedMigrationInteractiveUiTest,
                      ToastCloseButton) {
  // The user is explicitly signed in.
  ASSERT_TRUE(
      GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  ASSERT_TRUE(
      GetProfile()->GetPrefs()->GetBoolean(prefs::kExplicitBrowserSignin));

  RunTestSequence(TriggerToastTimer(),

                  WaitForShow(toasts::ToastView::kToastViewId),

                  PressButton(toasts::ToastView::kToastCloseButton),

                  WaitForHide(toasts::ToastView::kToastViewId));

  histogram_tester_.ExpectUniqueSample(
      kToastDismissedHistogram, toasts::ToastCloseReason::kCloseButton, 1);
  EXPECT_EQ(user_action_tester_.GetActionCount(kToastCloseButtonUserAction), 1);
}

DICE_MIGRATION_TEST_F(DiceMigrationServiceForcedMigrationInteractiveUiTest,
                      ToastDoesNotCloseOnNavigation) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTab);
  constexpr char16_t kNewUrl[] = u"chrome://version";

  // The user is explicitly signed in.
  ASSERT_TRUE(
      GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  ASSERT_TRUE(
      GetProfile()->GetPrefs()->GetBoolean(prefs::kExplicitBrowserSignin));

  RunTestSequence(TriggerToastTimer(),

                  WaitForShow(toasts::ToastView::kToastViewId),

                  // Navigate to another page using the omnibox.
                  InstrumentTab(kActiveTab),
                  EnterText(kOmniboxElementId, kNewUrl),
                  Confirm(kOmniboxElementId),
                  WaitForWebContentsNavigation(kActiveTab, GURL(kNewUrl)),

                  // The toast should still be visible.
                  EnsurePresent(toasts::ToastView::kToastViewId));
}

DICE_MIGRATION_TEST_F(DiceMigrationServiceForcedMigrationInteractiveUiTest,
                      ToastDoesNotCloseOnTabSwitch) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTab);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kNewTab);
  constexpr char16_t kNewUrl[] = u"chrome://version";

  // The user is explicitly signed in.
  ASSERT_TRUE(
      GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  ASSERT_TRUE(
      GetProfile()->GetPrefs()->GetBoolean(prefs::kExplicitBrowserSignin));

  RunTestSequence(InstrumentTab(kActiveTab),

                  TriggerToastTimer(),

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

DICE_MIGRATION_TEST_F(DiceMigrationServiceForcedMigrationInteractiveUiTest,
                      AutoCloseToast) {
  // The user is explicitly signed in.
  ASSERT_TRUE(
      GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  ASSERT_TRUE(
      GetProfile()->GetPrefs()->GetBoolean(prefs::kExplicitBrowserSignin));
  histogram_tester_.ExpectUniqueSample(kForceMigratedHistogram, true, 1);

  RunTestSequence(TriggerToastTimer(),

                  // The toast should be shown following the forced migration.
                  WaitForShow(toasts::ToastView::kToastViewId),

                  // The toast should auto dismiss when the timer goes off.
                  FireToastCloseTimer(),
                  WaitForHide(toasts::ToastView::kToastViewId));

  histogram_tester_.ExpectUniqueSample(kToastTriggerToShowHistogram,
                                       ToastId::kDiceUserMigrated, 1);
}

}  // namespace
