// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_browser_test_base.h"
#include "chrome/browser/ui/signin/dice_migration_service.h"
#include "chrome/browser/ui/signin/dice_migration_service_factory.h"
#include "chrome/browser/ui/toasts/toast_view.h"
#include "chrome/test/interaction/interactive_browser_test.h"
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

class DiceMigrationServicePixelBrowserTest
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

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      switches::kOfferMigrationToDiceUsers};
};

// This dialog is shown during all but the final time the migration is offered.
IN_PROC_BROWSER_TEST_F(DiceMigrationServicePixelBrowserTest, DialogView) {
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

IN_PROC_BROWSER_TEST_F(DiceMigrationServicePixelBrowserTest,
                       DialogViewWithAccountImage) {
  // Set a custom account image.
  signin::SimulateAccountImageFetch(
      identity_manager(),
      identity_manager()
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
IN_PROC_BROWSER_TEST_F(DiceMigrationServicePixelBrowserTest,
                       DialogViewFinalVariant) {
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

IN_PROC_BROWSER_TEST_F(DiceMigrationServicePixelBrowserTest, Toast) {
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

}  // namespace
