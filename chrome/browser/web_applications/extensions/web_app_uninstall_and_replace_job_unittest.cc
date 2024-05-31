// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/jobs/uninstall/web_app_uninstall_and_replace_job.h"

#include <memory>
#include <optional>

#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/test/fake_os_integration_manager.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "components/webapps/common/web_app_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {

class TestUninstallAndReplaceJobCommand
    : public WebAppCommand<AppLock, bool /*uninstall_triggered*/> {
 public:
  TestUninstallAndReplaceJobCommand(
      Profile* profile,
      const std::vector<webapps::AppId>& from_apps,
      const webapps::AppId& to_app,
      base::OnceCallback<void(bool uninstall_triggered)> on_complete)
      : WebAppCommand<AppLock, bool>("TestUninstallAndReplaceJobCommand",
                                     AppLockDescription(to_app),
                                     std::move(on_complete),
                                     /*args_for_shutdown=*/false),
        profile_(profile),
        from_apps_(from_apps),
        to_app_(to_app) {}

  ~TestUninstallAndReplaceJobCommand() override = default;

  void StartWithLock(std::unique_ptr<AppLock> lock) override {
    lock_ = std::move(lock);
    uninstall_and_replace_job_.emplace(
        profile_, GetMutableDebugValue(), *lock_, from_apps_, to_app_,
        base::BindOnce(&TestUninstallAndReplaceJobCommand::OnComplete,
                       base::Unretained(this)));
    uninstall_and_replace_job_->Start();
  }

  void OnComplete(bool uninstall_triggered) {
    CompleteAndSelfDestruct(CommandResult::kSuccess, uninstall_triggered);
  }

 private:
  raw_ptr<Profile> profile_ = nullptr;
  std::unique_ptr<AppLock> lock_;

  const std::vector<webapps::AppId> from_apps_;
  const webapps::AppId to_app_;

  std::optional<WebAppUninstallAndReplaceJob> uninstall_and_replace_job_;
};

class WebAppUninstallAndReplaceJobTest : public WebAppTest {
 public:
  void SetUp() override {
    WebAppTest::SetUp();
    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  void ScheduleUninstallAndReplaceJob(
      const std::vector<webapps::AppId>& from_apps,
      const webapps::AppId& to_app,
      base::OnceCallback<void(bool uninstall_triggered)> on_complete) {
    WebAppProvider::GetForTest(profile())->command_manager().ScheduleCommand(
        std::make_unique<TestUninstallAndReplaceJobCommand>(
            profile(), from_apps, to_app, std::move(on_complete)));
  }

  WebAppProvider* provider() { return WebAppProvider::GetForTest(profile()); }

  FakeOsIntegrationManager* os_integration_manager() {
    return WebAppProvider::GetForTest(profile())
        ->os_integration_manager()
        .AsTestOsIntegrationManager();
  }
};

// `WebAppUninstallAndReplaceJob` uses `AppServiceProxy` to do uninstall, app
// service only lives on chromeos ash not lacros.
#if !BUILDFLAG(IS_CHROMEOS_LACROS)
// Regression test for crbug.com/1182030
TEST_F(WebAppUninstallAndReplaceJobTest,
       WebAppMigrationPreservesShortcutStates) {
  const GURL kOldAppUrl("https://old.app.com");
  // Install an old app to be replaced.
  webapps::AppId old_app_id =
      test::InstallDummyWebApp(profile(), "old_app", kOldAppUrl);

  // Install a new app to migrate the old one to.
  webapps::AppId new_app_id = test::InstallDummyWebApp(
      profile(), "new_app", GURL("https://new.app.com"));
  std::optional<proto::WebAppOsIntegrationState> os_state =
      provider()->registrar_unsafe().GetAppCurrentOsIntegrationState(
          new_app_id);
  ASSERT_TRUE(os_state.has_value());
  EXPECT_TRUE(os_state->has_shortcut());
  EXPECT_TRUE(os_state->run_on_os_login().has_run_on_os_login_mode());

  // Set up the existing shortcuts.
  auto shortcut_info = std::make_unique<ShortcutInfo>();
  shortcut_info->url = kOldAppUrl;
  os_integration_manager()->SetShortcutInfoForApp(old_app_id,
                                                  std::move(shortcut_info));
  ShortcutLocations locations;
  locations.on_desktop = true;
  locations.in_startup = true;
  os_integration_manager()->SetAppExistingShortcuts(kOldAppUrl, locations);

  base::test::TestFuture<bool> future;
  ScheduleUninstallAndReplaceJob({old_app_id}, new_app_id,
                                 future.GetCallback());
  EXPECT_TRUE(future.Get());

  os_state = provider()->registrar_unsafe().GetAppCurrentOsIntegrationState(
      new_app_id);
  ASSERT_TRUE(os_state.has_value());
  EXPECT_TRUE(os_state->has_shortcut());
  EXPECT_EQ(os_state->run_on_os_login().run_on_os_login_mode(),
            proto::RunOnOsLoginMode::WINDOWED);
}

TEST_F(WebAppUninstallAndReplaceJobTest, DoubleMigration) {
  // Install an old app to be replaced.
  webapps::AppId old_app_id = test::InstallDummyWebApp(
      profile(), "old_app", GURL("https://old.app.com"));
  // Install a new app to migrate the old one to.
  webapps::AppId new_app_id = test::InstallDummyWebApp(
      profile(), "new_app", GURL("https://new.app.com"));
  {
    WebAppTestUninstallObserver waiter(profile());
    waiter.BeginListening({old_app_id});
    base::test::TestFuture<bool> future;
    ScheduleUninstallAndReplaceJob({old_app_id}, new_app_id,
                                   future.GetCallback());
    EXPECT_TRUE(future.Get());
    waiter.Wait();
  }

  // Do migration again. Uninstall and replace should not be triggered.
  base::test::TestFuture<bool> future;
  ScheduleUninstallAndReplaceJob({old_app_id}, new_app_id,
                                 future.GetCallback());
  EXPECT_FALSE(future.Get());
}
#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)

}  // namespace web_app
