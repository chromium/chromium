// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_browser_test_base.h"
#include "chrome/browser/ui/signin/dice_migration_service.h"
#include "chrome/browser/ui/signin/dice_migration_service_factory.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_utils.h"
#include "content/public/test/browser_test.h"

namespace {
constexpr char kTestEmail[] = "test@gmail.com";
// Baseline Gerrit CL number of the most recent CL that modified the UI.
constexpr char kScreenshotBaselineCL[] = "6688495";

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

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      switches::kOfferMigrationToDiceUsers};
};

IN_PROC_BROWSER_TEST_F(DiceMigrationServicePixelBrowserTest, DialogView) {
  RunTestSequence(
      // Trigger the dialog.
      Do([this]() {
        GetDiceMigrationService()->ShowDiceMigrationOfferDialogIfUserEligible();
      }),

      SetOnIncompatibleAction(
          OnIncompatibleAction::kIgnoreAndContinue,
          "Screenshots not supported in all testing environments."),

      WaitForShow(DiceMigrationService::kAcceptButtonElementId),

      // Grab a screenshot of the entire dialog that pops up.
      ScreenshotSurface(DiceMigrationService::kAcceptButtonElementId,
                        /*screenshot_name=*/"dice_migration_dialog",
                        /*baseline_cl=*/kScreenshotBaselineCL));
}

}  // namespace
