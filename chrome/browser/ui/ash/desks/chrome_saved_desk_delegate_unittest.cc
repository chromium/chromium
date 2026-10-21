// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/desks/chrome_saved_desk_delegate.h"

#include "ash/public/cpp/ash_public_export.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/browser_delegate/browser_controller_impl.h"
#include "chrome/browser/ash/login/test/chrome_user_session_test_environment_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/desks/desks_client.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ui/base/app_types.h"
#include "chromeos/ui/base/window_properties.h"
#include "components/account_id/account_id.h"
#include "components/app_restore/app_launch_info.h"
#include "components/app_restore/full_restore_save_handler.h"
#include "components/app_restore/full_restore_utils.h"
#include "components/app_restore/window_info.h"
#include "components/app_restore/window_properties.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/session_manager/test/user_session_test_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/wm/core/window_properties.h"

using ::testing::_;
using ::testing::Return;

namespace {
constexpr char kTestProfileEmail[] = "test@test.com";
}  // namespace

class ChromeSavedDeskDelegateTest : public testing::Test {
 public:
  ChromeSavedDeskDelegateTest() = default;

  ChromeSavedDeskDelegateTest(const ChromeSavedDeskDelegateTest&) = delete;
  ChromeSavedDeskDelegateTest& operator=(const ChromeSavedDeskDelegateTest&) =
      delete;

  ~ChromeSavedDeskDelegateTest() override = default;

  void SetUp() override {
    auto* browser_process = TestingBrowserProcess::GetGlobal();
    user_session_test_environment_ = std::make_unique<
        ash::test::UserSessionTestEnvironment>(
        browser_process->local_state(),
        std::make_unique<ash::test::ChromeUserSessionTestEnvironmentDelegate>(
            browser_process));

    browser_controller_.emplace();

    // Log in a test user so the `ChromeSavedDeskDelegate` does not return
    // empty result simply because of missing user profile.
    const auto account_id =
        AccountId::FromUserEmailGaiaId(kTestProfileEmail, GaiaId("1234567890"));
    ASSERT_TRUE(user_session_test_environment_->AddRegularUser(account_id));
    user_session_test_environment_->LogIn(account_id);
    profile_ = Profile::FromBrowserContext(
        ash::BrowserContextHelper::Get()->GetBrowserContextByAccountId(
            account_id));
    ASSERT_TRUE(profile_);

    chrome_saved_desk_delegate_ = std::make_unique<ChromeSavedDeskDelegate>();
  }

  void TearDown() override {
    chrome_saved_desk_delegate_.reset();
    profile_ = nullptr;
    browser_controller_.reset();
    user_session_test_environment_.reset();
  }

  ChromeSavedDeskDelegate* chrome_saved_desk_delegate() {
    return chrome_saved_desk_delegate_.get();
  }

  full_restore::FullRestoreSaveHandler* GetSaveHandler(
      bool start_save_timer = true) {
    auto* save_handler = full_restore::FullRestoreSaveHandler::GetInstance();
    save_handler->SetActiveProfilePath(profile_->GetPath());
    save_handler->AllowSave();
    return save_handler;
  }

  Profile* profile() { return profile_; }

 private:
  // Browser profiles need to be created on UI thread.
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};

  std::unique_ptr<ash::test::UserSessionTestEnvironment>
      user_session_test_environment_;
  std::optional<ash::BrowserControllerImpl> browser_controller_;
  raw_ptr<Profile> profile_ = nullptr;
  std::unique_ptr<ChromeSavedDeskDelegate> chrome_saved_desk_delegate_;
};

TEST_F(ChromeSavedDeskDelegateTest, NullWindowReturnsEmptyAppLaunchData) {
  base::test::TestFuture<std::unique_ptr<app_restore::AppLaunchInfo>> future;
  chrome_saved_desk_delegate()->GetAppLaunchDataForSavedDesk(
      /*window=*/nullptr, future.GetCallback());
  auto app_launch_info = future.Take();
  EXPECT_FALSE(app_launch_info);
}

TEST_F(ChromeSavedDeskDelegateTest,
       GetAppLaunchDataForArcAppWithoutRestoreData) {
  constexpr char kArcAppId[] = "arc_app_id";
  constexpr int32_t kTaskId = 100;
  constexpr int32_t kSessionId = 12345;

  // Register ARC app in AppService.
  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile());
  std::vector<apps::AppPtr> deltas;
  auto app = std::make_unique<apps::App>(apps::AppType::kArc, kArcAppId);
  app->readiness = apps::Readiness::kReady;
  deltas.push_back(std::move(app));
  proxy->OnApps(std::move(deltas), apps::AppType::kArc,
                /*should_notify_initialized=*/true);

  // Initialize FullRestoreSaveHandler and register the task.
  auto* save_handler = GetSaveHandler();
  save_handler->SetPrimaryProfilePath(profile()->GetPath());
  save_handler->SaveAppLaunchInfo(profile()->GetPath(),
                                  std::make_unique<app_restore::AppLaunchInfo>(
                                      kArcAppId, 0, kSessionId, 0));
  save_handler->OnTaskCreated(kArcAppId, kTaskId, kSessionId);

  // Verify that the save handler now has restore data.
  ASSERT_TRUE(save_handler->GetRestoreData(profile()->GetPath()));

  // Remove the app restore data to simulate the situation when ARC app is not
  // fully loaded.
  save_handler->RemoveAppRestoreData(profile()->GetPath(), kArcAppId, kTaskId);

  // Create a fake ARC window.
  std::unique_ptr<aura::Window> window(std::make_unique<aura::Window>(nullptr));
  window->Init(ui::LAYER_NOT_DRAWN);

  // Set properties to make it look like an ARC app.
  window->SetProperty(chromeos::kAppTypeKey, chromeos::AppType::ARC_APP);
  window->SetProperty(app_restore::kAppIdKey, std::string(kArcAppId));
  window->SetProperty(app_restore::kWindowIdKey, kTaskId);
  window->SetProperty(wm::kPersistableKey, true);

  // Verify that GetAppId works.
  EXPECT_EQ(save_handler->GetAppId(window.get()), kArcAppId);

  base::test::TestFuture<std::unique_ptr<app_restore::AppLaunchInfo>> future;
  chrome_saved_desk_delegate()->GetAppLaunchDataForSavedDesk(
      window.get(), future.GetCallback());
  auto app_launch_info = future.Take();

  ASSERT_TRUE(app_launch_info);
  EXPECT_EQ(app_launch_info->app_id, kArcAppId);
  EXPECT_TRUE(app_launch_info->event_flag.has_value());
  EXPECT_EQ(app_launch_info->event_flag.value(), 0);
}
