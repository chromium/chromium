// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/os_integration/web_app_shortcut_manager.h"

#include "base/files/file_util.h"
#include "base/mac/foundation_util.h"
#include "base/test/bind.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/test/fake_os_integration_manager.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {

namespace {
const char kFakeChromeBundleId[] = "fake.cfbundleidentifier";
}

class WebAppShortcutManagerMacTest : public WebAppTest {
 public:
  WebAppShortcutManagerMacTest()
      : WebAppTest(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    WebAppTest::SetUp();

    base::mac::SetBaseBundleID(kFakeChromeBundleId);
    // Put shortcuts somewhere under the home dir, as otherwise LaunchServices
    // won't be able to find them.
    override_registration_ =
        ShortcutOverrideForTesting::OverrideForTesting(base::GetHomeDir());

    provider_ = FakeWebAppProvider::Get(profile());

    // These tests require a real OsIntegrationManager, rather than the
    // FakeOsIntegrationManager that is created by default.
    auto file_handler_manager =
        std::make_unique<WebAppFileHandlerManager>(profile());
    auto protocol_handler_manager =
        std::make_unique<WebAppProtocolHandlerManager>(profile());
    auto shortcut_manager = std::make_unique<WebAppShortcutManager>(
        profile(), /*icon_manager=*/nullptr, file_handler_manager.get(),
        protocol_handler_manager.get());
    provider_->SetOsIntegrationManager(std::make_unique<OsIntegrationManager>(
        profile(), std::move(shortcut_manager), std::move(file_handler_manager),
        std::move(protocol_handler_manager),
        /*url_handler_manager*/ nullptr));

    // Do not yet start WebAppProvider here in SetUp, as tests verify behavior
    // that happens during and is triggered by start. As such individual tests
    // start the WebAppProvider when they need to.
  }

  void TearDown() override {
    WebAppShortcutManager::SetUpdateShortcutsForAllAppsCallback(
        base::NullCallback());

    // To prevent OS hooks from sticking around on bots, destroying the shortcut
    // override DCHECK fails if the directories are not empty. To bypass this in
    // this unittest, we manually delete it.
    // TODO: If these unittests leave OS hook artifacts on bots, undo that here.
    if (override_registration_->shortcut_override->chrome_apps_folder.IsValid())
      EXPECT_TRUE(override_registration_->shortcut_override->chrome_apps_folder
                      .Delete());
    override_registration_.reset();

    WebAppTest::TearDown();
  }

  WebAppShortcutManager& shortcut_manager() {
    return provider_->os_integration_manager().shortcut_manager_for_testing();
  }

  void CreateShortcutForApp(AppId app_id) {
    base::RunLoop loop;
    shortcut_manager().CreateShortcuts(
        app_id, /*add_to_desktop=*/false, SHORTCUT_CREATION_AUTOMATED,
        base::BindLambdaForTesting([&](bool result) {
          EXPECT_TRUE(result);
          loop.Quit();
        }));
    loop.Run();
  }

  base::FilePath GetShortcutPath(const std::string& app_name) {
    std::string shortcut_filename = app_name + ".app";
    return override_registration_->shortcut_override->chrome_apps_folder
        .GetPath()
        .AppendASCII(shortcut_filename);
  }

  void FastForwardBy(base::TimeDelta delta) {
    task_environment()->FastForwardBy(delta);
  }

  const char* kTestApp1Name = "test app";
  const GURL kTestApp1Url = GURL("https://foobar.com");
  const char* kTestApp2Name = "example app";
  const GURL kTestApp2Url = GURL("https://example.com");

 private:
  std::unique_ptr<ShortcutOverrideForTesting::BlockingRegistration>
      override_registration_;

  raw_ptr<FakeWebAppProvider> provider_;
};

TEST_F(WebAppShortcutManagerMacTest, InitialVersionIsStored) {
  // Starting the WebAppProvider, and more importantly its WebAppShortcutManager
  // subsystem should cause the current shortcuts version to be written to
  // prefs.
  EXPECT_FALSE(profile()->GetPrefs()->HasPrefPath(prefs::kAppShortcutsVersion));
  web_app::test::AwaitStartWebAppProviderAndSubsystems(profile());
  EXPECT_TRUE(profile()->GetPrefs()->HasPrefPath(prefs::kAppShortcutsVersion));
  EXPECT_TRUE(profile()->GetPrefs()->HasPrefPath(prefs::kAppShortcutsArch));
}

TEST_F(WebAppShortcutManagerMacTest, RebuildShortcutsOnVersionChange) {
  profile()->GetPrefs()->SetInteger(prefs::kAppShortcutsVersion, 0);

  base::OnceClosure done_update_callback_;
  WebAppShortcutManager::SetUpdateShortcutsForAllAppsCallback(
      base::BindLambdaForTesting([&](Profile* p, base::OnceClosure callback) {
        EXPECT_EQ(p, profile());
        done_update_callback_ = std::move(callback);
      }));

  web_app::test::AwaitStartWebAppProviderAndSubsystems(profile());
  // Starting the WebAppProvider should not synchronously trigger shortcut
  // updating.
  EXPECT_TRUE(done_update_callback_.is_null());

  // Install two apps, but only create shortcuts for one.
  AppId app_id1 =
      test::InstallDummyWebApp(profile(), kTestApp1Name, kTestApp1Url);
  CreateShortcutForApp(app_id1);
  AppId app_id2 =
      test::InstallDummyWebApp(profile(), kTestApp2Name, kTestApp2Url);

  base::FilePath app1_path = GetShortcutPath(kTestApp1Name);
  EXPECT_TRUE(base::PathExists(app1_path));
  base::FilePath app2_path = GetShortcutPath(kTestApp2Name);
  EXPECT_FALSE(base::PathExists(app2_path));

  // Mess up contents of the installed app, to verify rebuilding works.
  base::FilePath app_binary_path =
      app1_path.AppendASCII("Contents").AppendASCII("MacOS");
  EXPECT_TRUE(base::DeletePathRecursively(app_binary_path));

  // A couple of seconds later shortcut updating should still not have happened.
  FastForwardBy(base::Seconds(5));
  EXPECT_TRUE(done_update_callback_.is_null());

  // However eventually shortcut updating should trigger.
  FastForwardBy(base::Seconds(15));
  ASSERT_FALSE(done_update_callback_.is_null());

  // Make sure the updated shortcuts version is not persisted to prefs until
  // after we signal completion of updating.
  EXPECT_EQ(0, profile()->GetPrefs()->GetInteger(prefs::kAppShortcutsVersion));
  std::move(done_update_callback_).Run();
  EXPECT_NE(0, profile()->GetPrefs()->GetInteger(prefs::kAppShortcutsVersion));

  // Verify shortcut was rebuild, and shortcuts weren't created for the second
  // app.
  EXPECT_TRUE(base::PathExists(app_binary_path));
  EXPECT_FALSE(base::PathExists(app2_path));
}

}  // namespace web_app
