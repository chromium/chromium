// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_command_scheduler.h"

#include <memory>
#include <optional>
#include <string_view>
#include <utility>

#include "base/command_line.h"
#include "base/functional/callback_helpers.h"
#include "base/test/scoped_command_line.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/policy/web_app_policy_manager.h"
#include "chrome/browser/web_applications/scheduler/update_validated_origin_associations_result.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/fake_web_app_ui_manager.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/services/app_service/public/cpp/app_launch_params.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "content/public/browser/web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/mojom/window_open_disposition.mojom-shared.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_LINUX)
#include "base/nix/xdg_util.h"
#endif

namespace web_app {
namespace {

class WebAppCommandSchedulerTest : public WebAppTest {
 public:
  bool IsCommandQueued(std::string_view command_name) {
    // Note: Accessing & using the debug value for tests is poor practice and
    // should not be done, given how easily the format can be changed.
    // TODO(b/318858671): Update logic to not read command errors from debug
    // log.
    base::DictValue log =
        fake_provider().command_manager().ToDebugValue().TakeDict();
    for (const base::Value& command : *log.FindList("command_queue")) {
      if (*command.GetDict().FindDict("!metadata")->FindString("!name") ==
          command_name) {
        return true;
      }
    }
    return false;
  }
};

TEST_F(WebAppCommandSchedulerTest, FetchManifestAndInstall) {
  EXPECT_FALSE(fake_provider().is_registry_ready());
  fake_provider().scheduler().FetchManifestAndInstall(
      webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON,
      web_contents()->GetWeakPtr(), base::DoNothing(), base::DoNothing(),
      FallbackBehavior::kCraftedManifestOnly);

  fake_provider().StartWithSubsystems();
  EXPECT_EQ(
      fake_provider().command_manager().GetStartedCommandCountForTesting(), 0);
  EXPECT_EQ(fake_provider().command_manager().GetCommandCountForTesting(), 1u);

  EXPECT_TRUE(IsCommandQueued("FetchManifestAndInstallCommand"));
}

TEST_F(WebAppCommandSchedulerTest, PersistFileHandlersUserChoice) {
  EXPECT_FALSE(fake_provider().is_registry_ready());
  fake_provider().scheduler().PersistFileHandlersUserChoice(
      "app id", /*allowed=*/true, base::DoNothing());

  fake_provider().StartWithSubsystems();
  EXPECT_EQ(
      fake_provider().command_manager().GetStartedCommandCountForTesting(), 0);
  EXPECT_EQ(fake_provider().command_manager().GetCommandCountForTesting(), 1u);

  EXPECT_TRUE(IsCommandQueued("UpdateFileHandlerCommand"));

  test::WaitUntilReady(&fake_provider());
  fake_provider().command_manager().AwaitAllCommandsCompleteForTesting();
  EXPECT_FALSE(IsCommandQueued("UpdateFileHandlerCommand"));

  fake_provider().Shutdown();
  base::test::TestFuture<void> after_shutdown;
  fake_provider().scheduler().PersistFileHandlersUserChoice(
      "app id", /*allowed=*/true, after_shutdown.GetCallback());
  EXPECT_EQ(fake_provider().command_manager().GetCommandCountForTesting(), 0u);
  ASSERT_TRUE(after_shutdown.Wait());
}

TEST_F(WebAppCommandSchedulerTest, LaunchAppIgnoresProcessCommandLine) {
  // The standalone launch checks OS integration before reaching the fake UI.
  // Use WebAppTest's OS integration override rather than the fake manager's
  // suppression scope, which makes that check fail.
  fake_provider().UseRealOsIntegrationManager();
  test::AwaitStartWebAppProviderAndSubsystems(profile());
  const webapps::AppId app_id = test::InstallDummyWebApp(
      profile(), "Test App", GURL("https://example.com/"));
  base::test::TestFuture<apps::AppLaunchParams, LaunchWebAppWindowSetting>
      launch;
  fake_ui_manager().SetOnLaunchWebAppCallback(launch.GetRepeatingCallback());

  base::test::ScopedCommandLine scoped_command_line;
  auto* command_line = scoped_command_line.GetProcessCommandLine();
  command_line->AppendArg("https://example.com/process-argument");
  command_line->AppendSwitchASCII(switches::kAppRunOnOsLoginMode, "windowed");
  command_line->AppendSwitchASCII(switches::kAppLaunchUrlForShortcutsMenuItem,
                                  "https://example.com/shortcut");
#if BUILDFLAG(IS_LINUX)
  command_line->AppendSwitchASCII(base::nix::kXdgActivationTokenSwitch,
                                  "process-token");
  command_line->AppendSwitchASCII(base::nix::kDesktopStartupIdSwitch,
                                  "legacy-process-token");
#endif
  const base::CommandLine original_command_line(*command_line);
  const std::optional<GURL> launch_urls[] = {GURL("https://example.com/target"),
                                             std::nullopt};
  for (const auto& launch_url : launch_urls) {
    SCOPED_TRACE(launch_url ? launch_url->spec() : "app start URL");
    for (bool use_default_source : {true, false}) {
      SCOPED_TRACE(use_default_source ? "default source" : "explicit source");
      if (use_default_source) {
        fake_provider().scheduler().LaunchApp(app_id, launch_url,
                                              base::DoNothing());
      } else {
        fake_provider().scheduler().LaunchApp(
            app_id, launch_url, base::DoNothing(),
            apps::LaunchSource::kFromNavigationCapturing);
      }
      fake_provider().command_manager().AwaitAllCommandsCompleteForTesting();
      ASSERT_TRUE(launch.IsReady());
      auto [params, launch_setting] = launch.Take();
      EXPECT_EQ(params.app_id, app_id);
      EXPECT_TRUE(params.command_line.GetSwitches().empty());
      EXPECT_TRUE(params.command_line.GetArgs().empty());
      EXPECT_EQ(params.override_url, launch_url.value_or(GURL()));
      EXPECT_EQ(params.launch_source,
                use_default_source
                    ? apps::LaunchSource::kFromChromeInternal
                    : apps::LaunchSource::kFromNavigationCapturing);
      EXPECT_EQ(params.container, apps::LaunchContainer::kLaunchContainerNone);
      EXPECT_EQ(params.disposition, ui::mojom::WindowOpenDisposition::kUnknown);
      EXPECT_EQ(launch_setting,
                LaunchWebAppWindowSetting::kOverrideWithWebAppConfig);
    }
  }
  EXPECT_EQ(command_line->GetCommandLineString(),
            original_command_line.GetCommandLineString());
}

TEST_F(WebAppCommandSchedulerTest, OriginRevalidationWaitsForStartup) {
  EXPECT_FALSE(fake_provider().is_registry_ready());
  base::test::TestFuture<UpdateValidatedOriginAssociationsResult> result;
  fake_provider().scheduler().UpdateValidatedOriginAssociations(
      "missing", result.GetCallback());
  EXPECT_FALSE(result.IsReady());
  EXPECT_EQ(fake_provider().command_manager().GetCommandCountForTesting(), 1u);
  EXPECT_EQ(
      fake_provider().command_manager().GetStartedCommandCountForTesting(), 0);

  fake_provider().StartWithSubsystems();
  test::WaitUntilReady(&fake_provider());
  EXPECT_EQ(UpdateValidatedOriginAssociationsResult::kWebAppNotInstalled,
            result.Get());
  fake_provider().command_manager().AwaitAllCommandsCompleteForTesting();

  fake_provider().Shutdown();
  base::test::TestFuture<UpdateValidatedOriginAssociationsResult>
      after_shutdown;
  fake_provider().scheduler().UpdateValidatedOriginAssociations(
      "missing", after_shutdown.GetCallback());
  EXPECT_EQ(UpdateValidatedOriginAssociationsResult::kShutdown,
            after_shutdown.Get());
  EXPECT_EQ(fake_provider().command_manager().GetCommandCountForTesting(), 0u);
}

class WebAppCommandSchedulerPolicyDisabledTest
    : public WebAppCommandSchedulerTest {
 public:
  void SetUp() override {
    WebAppTest::SetUp();

    // Set the policy preference before creating the policy manager
    // This simulates the policy being set by enterprise policy.
    profile()->GetPrefs()->SetBoolean(prefs::kWebAppInstallByUserEnabled,
                                      false);

    // Create and set the policy manager after setting the policy pref.
    auto web_app_policy_manager =
        std::make_unique<WebAppPolicyManager>(profile());
    fake_provider().SetWebAppPolicyManager(std::move(web_app_policy_manager));

    // Start subsystems.
    fake_provider().StartWithSubsystems();
  }
};

TEST_F(WebAppCommandSchedulerPolicyDisabledTest,
       InstallAppLocally_PolicyDisabled) {
  const webapps::AppId app_id = "test_app_id";
  fake_provider().scheduler().InstallAppLocally(app_id, base::DoNothing());

  // When policy is disabled, no command should be scheduled.
  EXPECT_EQ(
      fake_provider().command_manager().GetStartedCommandCountForTesting(), 0);
  EXPECT_EQ(fake_provider().command_manager().GetCommandCountForTesting(), 0u);

  EXPECT_FALSE(IsCommandQueued("InstallAppLocallyCommand"));
  fake_provider().Shutdown();
}

}  // namespace
}  // namespace web_app
