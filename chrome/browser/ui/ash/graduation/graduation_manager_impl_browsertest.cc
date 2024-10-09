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
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
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

class GraduationManagerTest : public SystemWebAppBrowserTestBase {
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
    WaitForTestSystemAppInstall();
    WaitForAppRegistryCommands(browser()->profile());
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

  std::string GetLanguageCode() {
    return ash::graduation::GraduationManagerImpl::Get()->GetLanguageCode();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  LoggedInUserMixin logged_in_user_mixin_{
      &mixin_host_, /*test_base=*/this, embedded_test_server(),
      LoggedInUserMixin::LogInType::kManaged};
};

IN_PROC_BROWSER_TEST_F(GraduationManagerTest, PRE_AppPinnedWhenPolicyEnabled) {
  // Set pref value in PRE_ to ensure that the pref value is set at the time of
  // user session start in the test.
  SetGraduationEnablement(true);
}

IN_PROC_BROWSER_TEST_F(GraduationManagerTest, AppPinnedWhenPolicyEnabled) {
  EXPECT_EQ(apps::Readiness::kReady,
            GetAppReadiness(web_app::kGraduationAppId));
  EXPECT_TRUE(IsItemPinned(web_app::kGraduationAppId));

  SetGraduationEnablement(false);
  WaitForAppRegistryCommands(browser()->profile());

  EXPECT_FALSE(IsItemPinned(web_app::kGraduationAppId));
  EXPECT_EQ(apps::Readiness::kDisabledByPolicy,
            GetAppReadiness(web_app::kGraduationAppId));
}

IN_PROC_BROWSER_TEST_F(GraduationManagerTest, AppUnpinnedWhenPolicyUnset) {
  EXPECT_FALSE(IsItemPinned(web_app::kGraduationAppId));
  EXPECT_EQ(apps::Readiness::kDisabledByPolicy,
            GetAppReadiness(web_app::kGraduationAppId));

  SetGraduationEnablement(true);
  WaitForAppRegistryCommands(browser()->profile());

  EXPECT_EQ(apps::Readiness::kReady,
            GetAppReadiness(web_app::kGraduationAppId));
  EXPECT_TRUE(IsItemPinned(web_app::kGraduationAppId));
}

IN_PROC_BROWSER_TEST_F(GraduationManagerTest,
                       PRE_AppUnpinnedWhenPolicyDisabled) {
  // Set pref value in PRE_ to ensure that the pref value is set at the time of
  // user session start in the test.
  SetGraduationEnablement(false);
}

IN_PROC_BROWSER_TEST_F(GraduationManagerTest, AppUnpinnedWhenPolicyDisabled) {
  EXPECT_FALSE(IsItemPinned(web_app::kGraduationAppId));
  EXPECT_EQ(apps::Readiness::kDisabledByPolicy,
            GetAppReadiness(web_app::kGraduationAppId));

  SetGraduationEnablement(true);
  WaitForAppRegistryCommands(browser()->profile());

  EXPECT_EQ(apps::Readiness::kReady,
            GetAppReadiness(web_app::kGraduationAppId));
  EXPECT_TRUE(IsItemPinned(web_app::kGraduationAppId));
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
  EXPECT_FALSE(GetManager().IsSystemWebApp(web_app::kGraduationAppId));
}

INSTANTIATE_TEST_SUITE_P(All,
                         GraduationManagerWithConsumerUserTest,
                         testing::Bool());

}  // namespace ash
