// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/signin/dice_migration_service.h"
#include "chrome/browser/ui/signin/dice_migration_service_factory.h"
#include "chrome/browser/ui/toasts/toast_view.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_test.h"

namespace {
constexpr char kTestEmail[] = "test@gmail.com";
// Baseline Gerrit CL number of the most recent CL that modified the UI.
constexpr char kScreenshotBaselineCL[] = "6727956";

// Utility macro to implicitly sign in the user in a PRE test.
// NOTE: `test_suite` must be a subclass of
// `DiceMigrationServicePixelBrowserTest`.
#define DICE_MIGRATION_TEST_F(test_suite, test_name)                       \
  IN_PROC_BROWSER_TEST_F(test_suite, PRE_##test_name) {                    \
    ImplicitlySignIn();                                                    \
    enterprise_util::SetUserAcceptedAccountManagement(GetProfile(), true); \
  }                                                                        \
  IN_PROC_BROWSER_TEST_F(test_suite, test_name)

class DiceMigrationServicePixelBrowserTest : public InteractiveBrowserTest {
 public:
  DiceMigrationService* GetDiceMigrationService() {
    return DiceMigrationServiceFactory::GetForProfileIfExists(GetProfile());
  }

  Profile* GetProfile() { return browser()->profile(); }

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
};

DICE_MIGRATION_TEST_F(DiceMigrationServicePixelBrowserTest, Toast) {
  // The user is explicitly signed in.
  ASSERT_TRUE(
      GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  ASSERT_TRUE(
      GetProfile()->GetPrefs()->GetBoolean(prefs::kExplicitBrowserSignin));

  RunTestSequence(TriggerToastTimer(),

                  SetOnIncompatibleAction(
                      OnIncompatibleAction::kIgnoreAndContinue,
                      "Screenshots not supported in all testing environments."),

                  WaitForShow(toasts::ToastView::kToastViewId),

                  // Grab a screenshot of the toast that pops up.
                  Screenshot(toasts::ToastView::kToastViewId,
                             /*screenshot_name=*/"dice_migration_toast",
                             /*baseline_cl=*/kScreenshotBaselineCL));
}

}  // namespace
