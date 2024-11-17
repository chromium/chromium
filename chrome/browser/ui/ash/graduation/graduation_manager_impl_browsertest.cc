// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/graduation/graduation_manager_impl.h"

#include <algorithm>
#include <ranges>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/web_app_id_constants.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/shelf_model_observer.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/time/time.h"
#include "base/time/time_override.h"
#include "base/values.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/base/locale_util.h"
#include "chrome/browser/ash/login/test/logged_in_user_mixin.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/ash/system_web_apps/test_support/system_web_app_browsertest_base.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {
constexpr char kSessionStartTime[] = "30 Sep 2024 00:00:00 PST";
apps::AppServiceProxy* GetAppServiceProxy(Profile* profile) {
  return apps::AppServiceProxyFactory::GetForProfile(profile);
}

void WaitForAppRegistryCommands(Profile* profile) {
  auto* web_app_provider = web_app::WebAppProvider::GetForTest(profile);
  base::RunLoop run_loop;
  web_app_provider->on_registry_ready().Post(FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
  web_app_provider->command_manager().AwaitAllCommandsCompleteForTesting();
}

// Called on completion of locale_util::SwitchLanguage.
void OnLocaleSwitched(base::RunLoop* run_loop,
                      const locale_util::LanguageSwitchResult& result) {
  run_loop->Quit();
}
}  // namespace

class GraduationManagerTest : public SystemWebAppBrowserTestBase,
                              public ash::ShelfModelObserver {
 public:
  GraduationManagerTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kGraduation);
  }
  ~GraduationManagerTest() override = default;
  GraduationManagerTest(const GraduationManagerTest&) = delete;
  GraduationManagerTest& operator=(const GraduationManagerTest&) = delete;

  void SetUpOnMainThread() override {
    SystemWebAppBrowserTestBase::SetUpOnMainThread();
    logged_in_user_mixin_.LogInUser();
    SetMockClocksAndTaskRunner();
    WaitForTestSystemAppInstall();
    WaitForAppRegistryCommands(browser()->profile());
  }

  static void SetTimeNow(base::Time new_time_now) { time_now_ = new_time_now; }

  static base::Time GetTimeNow() { return time_now_; }

  void SetMockClocksAndTaskRunner() {
    // Set the system time in the task runner.
    base::Time start_time;
    EXPECT_TRUE(base::Time::FromUTCString(kSessionStartTime, &start_time));
    task_runner_ = base::MakeRefCounted<base::TestMockTimeTaskRunner>(
        start_time, base::TimeTicks::UnixEpoch());

    ash::graduation::GraduationManagerImpl::Get()->SetClocksForTesting(
        task_runner_->GetMockClock(), task_runner_->GetMockTickClock());

    // Override base::Time::Now() so the util functions work properly.
    SetTimeNow(start_time);

    time_override_ = std::make_unique<base::subtle::ScopedTimeClockOverrides>(
        /*time_override=*/&GraduationManagerTest::GetTimeNow,
        /*time_ticks_override=*/nullptr,
        /*thread_ticks_override=*/nullptr);
  }

  void AdvanceTimeBy(base::TimeDelta advance_length) {
    task_runner_->FastForwardBy(advance_length);
    SetTimeNow(base::Time::Now() + advance_length);
    ResumeTimer();
  }

  // task_runner_->FastForwardBy(TimeDelta) doesn't trigger the midnight timer
  // so it needs to be manually resumed.
  void ResumeTimer() {
    ash::graduation::GraduationManagerImpl::Get()->ResumeTimerForTesting();
  }

  bool IsItemPinned(const std::string& item_id) {
    const auto& shelf_items = ShelfModel::Get()->items();
    auto pinned_item =
        base::ranges::find_if(shelf_items, [&item_id](const auto& shelf_item) {
          return shelf_item.id.app_id == item_id;
        });
    return pinned_item != std::ranges::end(shelf_items);
  }

  apps::Readiness GetAppReadiness(const webapps::AppId& app_id) {
    apps::Readiness readiness;
    bool app_found =
        GetAppServiceProxy(browser()->profile())
            ->AppRegistryCache()
            .ForOneApp(app_id, [&readiness](const apps::AppUpdate& update) {
              readiness = update.Readiness();
            });
    EXPECT_TRUE(app_found);
    return readiness;
  }

  void SetGraduationEnablement(bool is_enabled) {
    base::Value::Dict status;
    status.Set("is_enabled", is_enabled);
    browser()->profile()->GetPrefs()->SetDict(
        prefs::kGraduationEnablementStatus, status.Clone());
  }

  void SetGraduationEnablementWithStartDate(bool is_enabled,
                                            int day,
                                            int month,
                                            int year) {
    base::Value::Dict status;
    status.Set("is_enabled", is_enabled);
    base::Value::Dict start_date;
    start_date.Set("day", day);
    start_date.Set("month", month);
    start_date.Set("year", year);
    status.Set("start_date", start_date.Clone());
    browser()->profile()->GetPrefs()->SetDict(
        prefs::kGraduationEnablementStatus, status.Clone());
  }

  void SetGraduationEnablementWithEndDate(bool is_enabled,
                                          int day,
                                          int month,
                                          int year) {
    base::Value::Dict status;
    status.Set("is_enabled", is_enabled);
    base::Value::Dict end_date;
    end_date.Set("day", day);
    end_date.Set("month", month);
    end_date.Set("year", year);
    status.Set("end_date", end_date.Clone());
    browser()->profile()->GetPrefs()->SetDict(
        prefs::kGraduationEnablementStatus, status.Clone());
  }

  std::string GetLanguageCode() {
    return ash::graduation::GraduationManagerImpl::Get()->GetLanguageCode();
  }

  void WaitForShelfItemAdd() {
    base::RunLoop run_loop;
    auto* shelf_model = ShelfModel::Get();
    shelf_model->AddObserver(this);
    item_added_callback_ = run_loop.QuitClosure();
    run_loop.Run();
    shelf_model->RemoveObserver(this);
  }

  void WaitForShefItemRemoved() {
    base::RunLoop run_loop;
    auto* shelf_model = ShelfModel::Get();
    shelf_model->AddObserver(this);
    item_removed_callback_ = run_loop.QuitClosure();
    run_loop.Run();
    shelf_model->RemoveObserver(this);
  }

  // ShelfModelObserver:
  void ShelfItemAdded(int index) override {
    if (item_added_callback_) {
      item_added_callback_.Run();
    }
  }

  void ShelfItemRemoved(int index, const ShelfItem& old_item) override {
    if (item_removed_callback_) {
      item_removed_callback_.Run();
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  base::RepeatingClosure item_added_callback_;
  base::RepeatingClosure item_removed_callback_;

  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;

  std::unique_ptr<base::subtle::ScopedTimeClockOverrides> time_override_;

  static base::Time time_now_;

  LoggedInUserMixin logged_in_user_mixin_{
      &mixin_host_, /*test_base=*/this, embedded_test_server(),
      LoggedInUserMixin::LogInType::kManaged};
};

// static
base::Time GraduationManagerTest::time_now_;

IN_PROC_BROWSER_TEST_F(GraduationManagerTest, PRE_AppPinnedWhenPolicyEnabled) {
  // Set pref value in PRE_ to ensure that the pref value is set at the time of
  // user session start in the test.
  SetGraduationEnablement(true);
}

IN_PROC_BROWSER_TEST_F(GraduationManagerTest, AppPinnedWhenPolicyEnabled) {
  EXPECT_EQ(apps::Readiness::kReady, GetAppReadiness(ash::kGraduationAppId));
  EXPECT_TRUE(IsItemPinned(ash::kGraduationAppId));

  SetGraduationEnablement(false);
  WaitForAppRegistryCommands(browser()->profile());

  EXPECT_FALSE(IsItemPinned(ash::kGraduationAppId));
  EXPECT_EQ(apps::Readiness::kDisabledByPolicy,
            GetAppReadiness(ash::kGraduationAppId));
}

IN_PROC_BROWSER_TEST_F(GraduationManagerTest,
                       PRE_AppPinnedWhenStartDateIsReached) {
  // Set pref value in PRE_ to ensure that the pref value is set at the time of
  // user session start in the test.
  SetGraduationEnablementWithStartDate(true, 1, 10, 2024);
}

IN_PROC_BROWSER_TEST_F(GraduationManagerTest, AppPinnedWhenStartDateIsReached) {
  EXPECT_EQ(apps::Readiness::kDisabledByPolicy,
            GetAppReadiness(ash::kGraduationAppId));
  EXPECT_FALSE(IsItemPinned(ash::kGraduationAppId));

  // Fast forward to the policy enablement start date set in the pre-test.
  AdvanceTimeBy(base::Days(1));
  WaitForAppRegistryCommands(browser()->profile());
  WaitForShelfItemAdd();

  EXPECT_TRUE(IsItemPinned(ash::kGraduationAppId));
  EXPECT_EQ(apps::Readiness::kReady,
            GetAppReadiness(ash::kGraduationAppId));
}

IN_PROC_BROWSER_TEST_F(GraduationManagerTest,
                       PRE_AppPinnedWhenStartDateIsReachedInMoreThanOneDay) {
  // Set pref value in PRE_ to ensure that the pref value is set at the time of
  // user session start in the test.
  SetGraduationEnablementWithStartDate(true, 2, 10, 2024);
}

IN_PROC_BROWSER_TEST_F(GraduationManagerTest,
                       AppPinnedWhenStartDateIsReachedInMoreThanOneDay) {
  EXPECT_EQ(apps::Readiness::kDisabledByPolicy,
            GetAppReadiness(ash::kGraduationAppId));
  EXPECT_FALSE(IsItemPinned(ash::kGraduationAppId));

  // Fast forward to the policy enablement start date set in the pre-test.
  AdvanceTimeBy(base::Days(2));
  WaitForAppRegistryCommands(browser()->profile());
  // Wait for the new shelf iteme to finish.
  WaitForShelfItemAdd();

  EXPECT_TRUE(IsItemPinned(ash::kGraduationAppId));
  EXPECT_EQ(apps::Readiness::kReady,
            GetAppReadiness(ash::kGraduationAppId));
}

IN_PROC_BROWSER_TEST_F(GraduationManagerTest, PRE_AppPinnedOnEndDate) {
  // Set pref value in PRE_ to ensure that the pref value is set at the time of
  // user session start in the test.
  SetGraduationEnablementWithEndDate(true, 1, 10, 2024);
}

IN_PROC_BROWSER_TEST_F(GraduationManagerTest, AppPinnedOnEndDate) {
  EXPECT_TRUE(IsItemPinned(ash::kGraduationAppId));
  EXPECT_EQ(apps::Readiness::kReady,
            GetAppReadiness(ash::kGraduationAppId));

  // Fast forward to the policy enablement end date set in the pre-test.
  AdvanceTimeBy(base::Days(1));
  WaitForAppRegistryCommands(browser()->profile());

  // Since this is the last day the app is available, the app should be pinned.
  EXPECT_TRUE(IsItemPinned(ash::kGraduationAppId));
  EXPECT_EQ(apps::Readiness::kReady,
            GetAppReadiness(ash::kGraduationAppId));
}

IN_PROC_BROWSER_TEST_F(GraduationManagerTest, AppUnpinnedWhenPolicyUnset) {
  EXPECT_FALSE(IsItemPinned(ash::kGraduationAppId));
  EXPECT_EQ(apps::Readiness::kDisabledByPolicy,
            GetAppReadiness(ash::kGraduationAppId));

  SetGraduationEnablement(true);
  WaitForAppRegistryCommands(browser()->profile());

  EXPECT_EQ(apps::Readiness::kReady, GetAppReadiness(ash::kGraduationAppId));
  EXPECT_TRUE(IsItemPinned(ash::kGraduationAppId));
}

IN_PROC_BROWSER_TEST_F(GraduationManagerTest,
                       PRE_AppUnpinnedWhenPolicyDisabled) {
  // Set pref value in PRE_ to ensure that the pref value is set at the time of
  // user session start in the test.
  SetGraduationEnablement(false);
}

IN_PROC_BROWSER_TEST_F(GraduationManagerTest, AppUnpinnedWhenPolicyDisabled) {
  EXPECT_FALSE(IsItemPinned(ash::kGraduationAppId));
  EXPECT_EQ(apps::Readiness::kDisabledByPolicy,
            GetAppReadiness(ash::kGraduationAppId));

  SetGraduationEnablement(true);
  WaitForAppRegistryCommands(browser()->profile());

  EXPECT_EQ(apps::Readiness::kReady, GetAppReadiness(ash::kGraduationAppId));
  EXPECT_TRUE(IsItemPinned(ash::kGraduationAppId));
}

IN_PROC_BROWSER_TEST_F(GraduationManagerTest,
                       PRE_AppUnpinnedWhenEndDateHasPassed) {
  // Set pref value in PRE_ to ensure that the pref value is set at the time of
  // user session start in the test.
  SetGraduationEnablementWithEndDate(true, 1, 10, 2024);
}

IN_PROC_BROWSER_TEST_F(GraduationManagerTest, AppUnpinnedWhenEndDateHasPassed) {
  EXPECT_TRUE(IsItemPinned(ash::kGraduationAppId));
  EXPECT_EQ(apps::Readiness::kReady,
            GetAppReadiness(ash::kGraduationAppId));

  // Fast forward to one day past the policy enablement end date set in the
  // pre-test.
  AdvanceTimeBy(base::Days(2));
  WaitForAppRegistryCommands(browser()->profile());
  // Wait for the shelf item to finish being removed.
  WaitForShefItemRemoved();

  EXPECT_EQ(apps::Readiness::kDisabledByPolicy,
            GetAppReadiness(ash::kGraduationAppId));
  EXPECT_FALSE(IsItemPinned(ash::kGraduationAppId));
}

IN_PROC_BROWSER_TEST_F(GraduationManagerTest, GetLanguageCode) {
  // Browser tests should default to English.
  EXPECT_EQ("en-US", GetLanguageCode());

  // Switch the application locale to Spanish.
  base::RunLoop run_loop;
  locale_util::SwitchLanguage("es", true, false,
                              base::BindRepeating(&OnLocaleSwitched, &run_loop),
                              ProfileManager::GetActiveUserProfile());
  run_loop.Run();

  EXPECT_EQ("es", GetLanguageCode());
}

class GraduationManagerWithConsumerUserTest
    : public SystemWebAppBrowserTestBase,
      public ::testing::WithParamInterface<bool> {
 public:
  GraduationManagerWithConsumerUserTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kGraduation);
  }
  ~GraduationManagerWithConsumerUserTest() override = default;
  GraduationManagerWithConsumerUserTest(
      const GraduationManagerWithConsumerUserTest&) = delete;
  GraduationManagerWithConsumerUserTest& operator=(
      const GraduationManagerWithConsumerUserTest&) = delete;

  void SetUpOnMainThread() override {
    SystemWebAppBrowserTestBase::SetUpOnMainThread();
    logged_in_user_mixin_.LogInUser();
    WaitForTestSystemAppInstall();
    WaitForAppRegistryCommands(browser()->profile());
  }

 private:
  bool IsChildUser() const { return GetParam(); }

  base::test::ScopedFeatureList scoped_feature_list_;

  LoggedInUserMixin logged_in_user_mixin_{
      &mixin_host_, /*test_base=*/this, embedded_test_server(),
      IsChildUser() ? LoggedInUserMixin::LogInType::kChild
                    : LoggedInUserMixin::LogInType::kConsumer};
};

IN_PROC_BROWSER_TEST_P(GraduationManagerWithConsumerUserTest, AppNotInstalled) {
  EXPECT_FALSE(GetManager().IsSystemWebApp(ash::kGraduationAppId));
}

INSTANTIATE_TEST_SUITE_P(All,
                         GraduationManagerWithConsumerUserTest,
                         testing::Bool());

}  // namespace ash
