// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_browser_test_base.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/signin/dice_migration_service.h"
#include "chrome/browser/ui/signin/dice_migration_service_factory.h"
#include "chrome/browser/ui/toasts/toast_view.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_utils.h"
#include "content/public/test/browser_test.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace {
constexpr char kTestEmail[] = "test@gmail.com";
// Baseline Gerrit CL number of the most recent CL that modified the UI.
constexpr char kScreenshotBaselineCL[] = "6727956";
const gfx::Image kAccountImage = gfx::test::CreateImage(20, 20, SK_ColorYELLOW);
const char kAccountImageUrl[] = "ACCOUNT_IMAGE_URL";

// Utility macro to implicitly sign in the user in a PRE test.
// NOTE: `test_suite` must be a subclass of
// `DiceMigrationServicePixelBrowserTest`.
#define DICE_MIGRATION_TEST_F(test_suite, test_name)    \
  IN_PROC_BROWSER_TEST_F(test_suite, PRE_##test_name) { \
    ImplicitlySignIn();                                 \
  }                                                     \
  IN_PROC_BROWSER_TEST_F(test_suite, test_name)

class DiceMigrationServicePixelBrowserTest : public InteractiveBrowserTest {
 public:
  DiceMigrationServicePixelBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{switches::kOfferMigrationToDiceUsers},
        /*disabled_features=*/{switches::kForcedDiceMigration});
  }

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

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// This dialog is shown during all but the final time the migration is offered.
DICE_MIGRATION_TEST_F(DiceMigrationServicePixelBrowserTest, DialogView) {
  // The user is implicitly signed in.
  ASSERT_TRUE(
      GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  ASSERT_FALSE(
      GetProfile()->GetPrefs()->GetBoolean(prefs::kExplicitBrowserSignin));

  RunTestSequence(
      TriggerDialog(),

      SetOnIncompatibleAction(
          OnIncompatibleAction::kIgnoreAndContinue,
          "Screenshots not supported in all testing environments."),

      WaitForShow(DiceMigrationService::kAcceptButtonElementId),

      // Grab a screenshot of the entire dialog that pops up.
      ScreenshotSurface(DiceMigrationService::kAcceptButtonElementId,
                        /*screenshot_name=*/"dice_migration_dialog",
                        /*baseline_cl=*/kScreenshotBaselineCL));
}

DICE_MIGRATION_TEST_F(DiceMigrationServicePixelBrowserTest,
                      DialogViewWithAccountImage) {
  // The user is implicitly signed in.
  ASSERT_TRUE(
      GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  ASSERT_FALSE(
      GetProfile()->GetPrefs()->GetBoolean(prefs::kExplicitBrowserSignin));

  // Set a custom account image.
  signin::SimulateAccountImageFetch(
      GetIdentityManager(),
      GetIdentityManager()
          ->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
          .account_id,
      kAccountImageUrl, kAccountImage);

  RunTestSequence(
      TriggerDialog(),

      SetOnIncompatibleAction(
          OnIncompatibleAction::kIgnoreAndContinue,
          "Screenshots not supported in all testing environments."),

      WaitForShow(DiceMigrationService::kAcceptButtonElementId),

      // Grab a screenshot of the entire dialog that pops up.
      ScreenshotSurface(
          DiceMigrationService::kAcceptButtonElementId,
          /*screenshot_name=*/"dice_migration_dialog_with_account_image",
          /*baseline_cl=*/kScreenshotBaselineCL));
}

// This dialog is shown only during the final time the migration is offered.
DICE_MIGRATION_TEST_F(DiceMigrationServicePixelBrowserTest,
                      DialogViewFinalVariant) {
  // The user is implicitly signed in.
  ASSERT_TRUE(
      GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  ASSERT_FALSE(
      GetProfile()->GetPrefs()->GetBoolean(prefs::kExplicitBrowserSignin));

  // Set the dialog shown count to the max - 1 to show the final variant.
  GetProfile()->GetPrefs()->SetInteger(
      kDiceMigrationDialogShownCount,
      DiceMigrationService::kMaxDialogShownCount - 1);

  RunTestSequence(TriggerDialog(),

                  SetOnIncompatibleAction(
                      OnIncompatibleAction::kIgnoreAndContinue,
                      "Screenshots not supported in all testing environments."),

                  WaitForShow(DiceMigrationService::kAcceptButtonElementId),

                  // Grab a screenshot of the entire dialog that pops up.
                  ScreenshotSurface(
                      DiceMigrationService::kAcceptButtonElementId,
                      /*screenshot_name=*/"dice_migration_dialog_final_variant",
                      /*baseline_cl=*/kScreenshotBaselineCL));
}

DICE_MIGRATION_TEST_F(DiceMigrationServicePixelBrowserTest, Toast) {
  // The user is implicitly signed in.
  ASSERT_TRUE(
      GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  ASSERT_FALSE(
      GetProfile()->GetPrefs()->GetBoolean(prefs::kExplicitBrowserSignin));

  RunTestSequence(TriggerDialog(),

                  SetOnIncompatibleAction(
                      OnIncompatibleAction::kIgnoreAndContinue,
                      "Screenshots not supported in all testing environments."),

                  WaitForShow(DiceMigrationService::kAcceptButtonElementId),

                  // Press the "Got it" button.
                  PressButton(DiceMigrationService::kAcceptButtonElementId),

                  WaitForHide(DiceMigrationService::kAcceptButtonElementId),

                  WaitForShow(toasts::ToastView::kToastViewId),

                  // Grab a screenshot of the toast that pops up.
                  Screenshot(toasts::ToastView::kToastViewId,
                             /*screenshot_name=*/"dice_migration_toast",
                             /*baseline_cl=*/kScreenshotBaselineCL));
}

DICE_MIGRATION_TEST_F(DiceMigrationServicePixelBrowserTest, IdentityPill) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kActiveTab);
  constexpr char16_t kNewUrl1[] = u"chrome://version";
  constexpr char16_t kNewUrl2[] = u"chrome://settings";

  // The user is implicitly signed in.
  ASSERT_TRUE(
      GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  ASSERT_FALSE(
      GetProfile()->GetPrefs()->GetBoolean(prefs::kExplicitBrowserSignin));

  RunTestSequence(
      SetOnIncompatibleAction(
          OnIncompatibleAction::kIgnoreAndContinue,
          "Screenshots not supported in all testing environments."),

      // The identity pill is not expanded by default.
      Screenshot(kToolbarAvatarButtonElementId,
                 /*screenshot_name=*/"dice_migration_identity_pill_closed",
                 /*baseline_cl=*/kScreenshotBaselineCL),

      TriggerDialog(),

      WaitForShow(DiceMigrationService::kAcceptButtonElementId),

      // Navigate to another page using the omnibox.
      // NOTE: This is only done to introduce some delay to finish setting up
      // the expanded identity pill and avoid flakiness.
      // TODO(crbug.com/440020019): Look for a way to avoid this workaround.
      InstrumentTab(kActiveTab), EnterText(kOmniboxElementId, kNewUrl1),
      Confirm(kOmniboxElementId),
      WaitForWebContentsNavigation(kActiveTab, GURL(kNewUrl1)),

      // The identity pill is expanded when the dialog is shown.
      Screenshot(kToolbarAvatarButtonElementId,
                 /*screenshot_name=*/"dice_migration_identity_pill_open",
                 /*baseline_cl=*/kScreenshotBaselineCL),

      // Press the "Got it" button.
      PressButton(DiceMigrationService::kAcceptButtonElementId),

      WaitForHide(DiceMigrationService::kAcceptButtonElementId),

      // Navigate to another page using the omnibox.
      // NOTE: This is only done to introduce some delay to finish collapsing
      // the expanded identity pill and avoid flakiness.
      // TODO(crbug.com/440020019): Look for a way to avoid this workaround.
      EnterText(kOmniboxElementId, kNewUrl2), Confirm(kOmniboxElementId),
      WaitForWebContentsNavigation(kActiveTab, GURL(kNewUrl2)),

      // The identity pill is collapsed again.
      Screenshot(kToolbarAvatarButtonElementId,
                 /*screenshot_name=*/"dice_migration_identity_pill_closed",
                 /*baseline_cl=*/kScreenshotBaselineCL));
}

}  // namespace
