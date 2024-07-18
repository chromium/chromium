// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/apple/foundation_util.h"
#include "base/files/file_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/test/fake_os_integration_manager.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_params.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {

namespace {
const char kFakeChromeBundleId[] = "fake.cfbundleidentifier";
}

class ShortcutsVersioningMacTest : public WebAppTest {
 public:
  ShortcutsVersioningMacTest()
      : WebAppTest(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    WebAppTest::SetUp();

    base::apple::SetBaseBundleID(kFakeChromeBundleId);
    // Put shortcuts somewhere under the home dir, as otherwise LaunchServices
    // won't be able to find them.
    override_registration_ =
        OsIntegrationTestOverrideImpl::OverrideForTesting();

    provider_ = FakeWebAppProvider::Get(profile());

    // These tests require a real OsIntegrationManager, rather than the
    // FakeOsIntegrationManager that is created by default.
    auto file_handler_manager =
        std::make_unique<WebAppFileHandlerManager>(profile());
    auto protocol_handler_manager =
        std::make_unique<WebAppProtocolHandlerManager>(profile());
    provider_->SetOsIntegrationManager(std::make_unique<OsIntegrationManager>(
        profile(), std::move(file_handler_manager),
        std::move(protocol_handler_manager)));

    // Do not yet start WebAppProvider here in SetUp, as tests verify behavior
    // that happens during and is triggered by start. As such individual tests
    // start the WebAppProvider when they need to.
  }

  void TearDown() override {
    OsIntegrationManager::SetUpdateShortcutsForAllAppsCallback(
        base::NullCallback());

    override_registration_.reset();

    WebAppTest::TearDown();
  }

  base::FilePath GetShortcutPath(const std::string& app_name) {
    std::string shortcut_filename = app_name + ".app";
    return override_registration_->test_override()
        .chrome_apps_folder()
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
  std::unique_ptr<OsIntegrationTestOverrideImpl::BlockingRegistration>
      override_registration_;

  raw_ptr<FakeWebAppProvider, DanglingUntriaged> provider_ = nullptr;
};

TEST_F(ShortcutsVersioningMacTest, InitialVersionIsStored) {
  // Starting the WebAppProvider, and more importantly its OsIntegrationManager
  // subsystem should cause the current shortcuts version to be written to
  // prefs.
  EXPECT_FALSE(profile()->GetPrefs()->HasPrefPath(prefs::kAppShortcutsVersion));
  web_app::test::AwaitStartWebAppProviderAndSubsystems(profile());
  EXPECT_TRUE(profile()->GetPrefs()->HasPrefPath(prefs::kAppShortcutsVersion));
  EXPECT_TRUE(profile()->GetPrefs()->HasPrefPath(prefs::kAppShortcutsArch));
}

TEST_F(ShortcutsVersioningMacTest, RebuildShortcutsOnVersionChange) {
  profile()->GetPrefs()->SetInteger(prefs::kAppShortcutsVersion, 0);

  base::OnceClosure done_update_callback;
  OsIntegrationManager::SetUpdateShortcutsForAllAppsCallback(
      base::BindLambdaForTesting([&](Profile* p, base::OnceClosure callback) {
        EXPECT_EQ(p, profile());
        done_update_callback = std::move(callback);
      }));

  web_app::test::AwaitStartWebAppProviderAndSubsystems(profile());
  // Starting the WebAppProvider should not synchronously trigger shortcut
  // updating.
  EXPECT_TRUE(done_update_callback.is_null());

  // Install two apps, but don't create shortcuts for one.
  webapps::AppId app_id1 =
      test::InstallDummyWebApp(profile(), kTestApp1Name, kTestApp1Url);

  // The second app will not have os hooks synchronized.
  auto web_app_info =
      WebAppInstallInfo::CreateWithStartUrlForTesting(kTestApp2Url);
  web_app_info->title = base::UTF8ToUTF16(kTestApp2Name);

  // Ensure the second app is not locally installed, to prevent OS integration
  // from ever triggering on it.
  // TODO(crbug.com/343754406): Investigate why all the fields need to be
  // explicitly set to false.
  WebAppInstallParams params;
  params.install_state = proto::InstallState::SUGGESTED_FROM_ANOTHER_DEVICE;
  params.add_to_applications_menu = false;
  params.add_to_desktop = false;
  params.add_to_quick_launch_bar = false;
  base::test::TestFuture<const webapps::AppId&, webapps::InstallResultCode>
      future;
  fake_provider().scheduler().InstallFromInfoWithParams(
      std::move(web_app_info), /*overwrite_existing_manifest_fields=*/false,
      webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON, future.GetCallback(),
      params);
  ASSERT_TRUE(future.Wait());
  webapps::AppId app_id2 = future.Get<webapps::AppId>();

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
  EXPECT_TRUE(done_update_callback.is_null());

  // However eventually shortcut updating should trigger.
  FastForwardBy(base::Seconds(15));
  ASSERT_FALSE(done_update_callback.is_null());

  // Make sure the updated shortcuts version is not persisted to prefs until
  // after we signal completion of updating.
  EXPECT_EQ(0, profile()->GetPrefs()->GetInteger(prefs::kAppShortcutsVersion));
  {
    base::RunLoop run_loop;
    OsIntegrationManager::OnSetCurrentAppShortcutsVersionCallbackForTesting() =
        run_loop.QuitClosure();
    std::move(done_update_callback).Run();
    run_loop.Run();
  }
  EXPECT_NE(0, profile()->GetPrefs()->GetInteger(prefs::kAppShortcutsVersion));

  // Verify shortcut was rebuild, and shortcuts weren't created for the second
  // app.
  EXPECT_TRUE(base::PathExists(app_binary_path));
  EXPECT_FALSE(base::PathExists(app2_path));
}

TEST_F(ShortcutsVersioningMacTest, UserDeletedShortcutsNotUpdated) {
  profile()->GetPrefs()->SetInteger(prefs::kAppShortcutsVersion, 0);

  base::OnceClosure done_update_callback;
  OsIntegrationManager::SetUpdateShortcutsForAllAppsCallback(
      base::BindLambdaForTesting([&](Profile* p, base::OnceClosure callback) {
        EXPECT_EQ(p, profile());
        done_update_callback = std::move(callback);
      }));

  web_app::test::AwaitStartWebAppProviderAndSubsystems(profile());
  // Starting the WebAppProvider should not synchronously trigger shortcut
  // updating.
  EXPECT_TRUE(done_update_callback.is_null());

  // Install app with shortcuts created.
  webapps::AppId app_id1 =
      test::InstallDummyWebApp(profile(), kTestApp1Name, kTestApp1Url);
  base::FilePath app1_path = GetShortcutPath(kTestApp1Name);
  EXPECT_TRUE(base::PathExists(app1_path));

  // Mimic shortcut deletion by end user.
  EXPECT_TRUE(base::DeletePathRecursively(app1_path));

  // Shortcut updating should be automatically triggered after 10 seconds.
  FastForwardBy(base::Seconds(15));
  ASSERT_FALSE(done_update_callback.is_null());

  // Make sure the updated shortcuts version is not persisted to prefs until
  // after we signal completion of updating.
  EXPECT_EQ(0, profile()->GetPrefs()->GetInteger(prefs::kAppShortcutsVersion));
  {
    base::RunLoop run_loop;
    OsIntegrationManager::OnSetCurrentAppShortcutsVersionCallbackForTesting() =
        run_loop.QuitClosure();
    std::move(done_update_callback).Run();
    run_loop.Run();
  }
  EXPECT_NE(0, profile()->GetPrefs()->GetInteger(prefs::kAppShortcutsVersion));

  // Verify that shortcut isn't re-created.
  EXPECT_FALSE(base::PathExists(app1_path));
}

}  // namespace web_app
