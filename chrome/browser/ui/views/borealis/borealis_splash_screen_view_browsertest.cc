// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/borealis/borealis_splash_screen_view.h"

#include "ash/constants/ash_features.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/borealis/borealis_app_launcher_impl.h"
#include "chrome/browser/ash/borealis/borealis_context.h"
#include "chrome/browser/ash/borealis/borealis_context_manager.h"
#include "chrome/browser/ash/borealis/borealis_context_manager_mock.h"
#include "chrome/browser/ash/borealis/borealis_features.h"
#include "chrome/browser/ash/borealis/borealis_metrics.h"
#include "chrome/browser/ash/borealis/borealis_prefs.h"
#include "chrome/browser/ash/borealis/borealis_service_factory.h"
#include "chrome/browser/ash/borealis/borealis_service_fake.h"
#include "chrome/browser/ash/borealis/borealis_task.h"
#include "chrome/browser/ash/borealis/borealis_util.h"
#include "chrome/browser/ash/borealis/borealis_window_manager_mock.h"
#include "chrome/browser/ash/borealis/testing/windows.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/common/chrome_features.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace borealis {
namespace {

class FakeBorealisContextManager : public BorealisContextManager {
 public:
  void StartBorealis(ResultCallback callback) override {
    std::move(callback).Run(base::unexpected(Described<BorealisStartupResult>(
        BorealisStartupResult::kDiskImageFailed,
        "failed on purpose for testing")));
  }

  bool IsRunning() override { return false; }

  void ShutDownBorealis(base::OnceCallback<void(BorealisShutdownResult)>
                            on_shutdown_callback) override {}
};

class CallbackFactory
    : public testing::StrictMock<
          testing::MockFunction<void(BorealisAppLauncher::LaunchResult)>> {
 public:
  base::OnceCallback<void(BorealisAppLauncher::LaunchResult)> BindOnce() {
    return base::BindOnce(&CallbackFactory::Call, base::Unretained(this));
  }
};

class BorealisSplashScreenViewBrowserTest : public DialogBrowserTest {
 public:
  BorealisSplashScreenViewBrowserTest() {
    feature_list_.InitWithFeatures(
        {features::kBorealis, ash::features::kBorealisPermitted}, {});
  }

  // DialogBrowserTest:
  void SetUpOnMainThread() override {
    window_manager_ =
        std::make_unique<BorealisWindowManager>(browser()->profile());
    features_ = std::make_unique<BorealisFeatures>(browser()->profile());
    BorealisServiceFake* fake_service =
        BorealisServiceFake::UseFakeForTesting(browser()->profile());
    fake_service->SetContextManagerForTesting(&fake_context_manager_);
    fake_service->SetWindowManagerForTesting(window_manager_.get());
    fake_service->SetFeaturesForTesting(features_.get());
    browser()->profile()->GetPrefs()->SetBoolean(
        prefs::kBorealisInstalledOnDevice, true);
  }

  void ShowUi(const std::string& name) override {
    borealis::BorealisSplashScreenView::Show(browser()->profile());
  }
  BorealisSplashScreenViewBrowserTest(
      const BorealisSplashScreenViewBrowserTest&) = delete;
  BorealisSplashScreenViewBrowserTest& operator=(
      const BorealisSplashScreenViewBrowserTest&) = delete;

 private:
  FakeBorealisContextManager fake_context_manager_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<BorealisWindowManager> window_manager_;
  std::unique_ptr<BorealisFeatures> features_;
};

IN_PROC_BROWSER_TEST_F(BorealisSplashScreenViewBrowserTest, InvokeUi_default) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(BorealisSplashScreenViewBrowserTest,
                       HidesWhenBorealisLaunches) {
  ShowUi("default");
  EXPECT_TRUE(VerifyUi());
  EXPECT_NE(nullptr, BorealisSplashScreenView::GetActiveViewForTesting());
  MakeAndTrackWindow(
      "org.chromium.guest_os.borealis.foobarbaz",
      &borealis::BorealisServiceFactory::GetForProfile(browser()->profile())
           ->WindowManager());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(VerifyUi());
  EXPECT_EQ(nullptr, BorealisSplashScreenView::GetActiveViewForTesting());
}

IN_PROC_BROWSER_TEST_F(BorealisSplashScreenViewBrowserTest,
                       HidesWhenBorealisLaunchFails) {
  ShowUi("default");
  EXPECT_TRUE(VerifyUi());
  EXPECT_NE(nullptr, BorealisSplashScreenView::GetActiveViewForTesting());

  CallbackFactory callback_check;
  EXPECT_CALL(callback_check, Call(BorealisAppLauncher::LaunchResult::kError));
  BorealisAppLauncherImpl launcher(browser()->profile());
  launcher.Launch("foo.desktop", BorealisLaunchSource::kUnknown,
                  callback_check.BindOnce());
  base::RunLoop().RunUntilIdle();

  // The splash screen should have disappeared.
  EXPECT_EQ(nullptr, BorealisSplashScreenView::GetActiveViewForTesting());

  // We should now see an error dialog instead.
  EXPECT_TRUE(VerifyUi());
}

}  // namespace
}  // namespace borealis
