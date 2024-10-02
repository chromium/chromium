// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string_view>
#include <utility>

#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/memory/raw_ptr.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "build/buildflag.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/policy/web_app_policy_manager.h"
#include "chrome/browser/web_applications/proto/web_app_os_integration_state.pb.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_params.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/webapps/browser/install_result_code.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace web_app {

namespace {

class RunOnOsLoginSubManagerTestBase : public WebAppTest {
 public:
  const GURL kWebAppUrl = GURL("https://example.com/path/index.html");

  RunOnOsLoginSubManagerTestBase() = default;
  ~RunOnOsLoginSubManagerTestBase() override = default;

  void SetUp() override {
    WebAppTest::SetUp();
    provider_ = FakeWebAppProvider::Get(profile());

    auto file_handler_manager =
        std::make_unique<WebAppFileHandlerManager>(profile());
    auto protocol_handler_manager =
        std::make_unique<WebAppProtocolHandlerManager>(profile());
    auto os_integration_manager = std::make_unique<OsIntegrationManager>(
        profile(), std::move(file_handler_manager),
        std::move(protocol_handler_manager));

    provider_->SetOsIntegrationManager(std::move(os_integration_manager));
    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  void TearDown() override {
    test::UninstallAllWebApps(profile());
    WebAppTest::TearDown();
  }

  webapps::AppId InstallWebApp() {
    std::unique_ptr<WebAppInstallInfo> info =
        WebAppInstallInfo::CreateWithStartUrlForTesting(kWebAppUrl);
    info->title = u"Test App";
    info->user_display_mode = web_app::mojom::UserDisplayMode::kStandalone;
    base::test::TestFuture<const webapps::AppId&, webapps::InstallResultCode>
        result;
    // InstallFromInfoWithParams is used instead of InstallFromInfo, because
    // InstallFromInfo doesn't register OS integration.
    provider().scheduler().InstallFromInfoWithParams(
        std::move(info), /*overwrite_existing_manifest_fields=*/true,
        webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON,
        result.GetCallback(), WebAppInstallParams());
    bool success = result.Wait();
    EXPECT_TRUE(success);
    if (!success) {
      return webapps::AppId();
    }
    EXPECT_EQ(result.Get<webapps::InstallResultCode>(),
              webapps::InstallResultCode::kSuccessNewInstall);
    return result.Get<webapps::AppId>();
  }

  void SetWebAppSettingsListPref(std::string_view pref) {
    ASSERT_OK_AND_ASSIGN(
        auto result,
        base::JSONReader::ReadAndReturnValueWithError(
            pref, base::JSONParserOptions::JSON_ALLOW_TRAILING_COMMAS));
    ASSERT_TRUE(result.is_list());
    profile()->GetPrefs()->Set(prefs::kWebAppSettings, std::move(result));
  }

 protected:
  WebAppProvider& provider() { return *provider_; }
  WebAppRegistrar& registrar() { return provider().registrar_unsafe(); }

 private:
  raw_ptr<FakeWebAppProvider, DanglingUntriaged> provider_ = nullptr;
};

// Configure tests only.
using RunOnOsLoginSubManagerConfigureTest = RunOnOsLoginSubManagerTestBase;

TEST_F(RunOnOsLoginSubManagerConfigureTest,
       VerifyRunOnOsLoginSetProperlyOnInstall) {
  const webapps::AppId& app_id = InstallWebApp();

  auto state = registrar().GetAppCurrentOsIntegrationState(app_id);
  ASSERT_TRUE(state.has_value());
  const proto::WebAppOsIntegrationState& os_integration_state = state.value();
    // on installation, both values are set to NOT_RUN.
    ASSERT_TRUE(os_integration_state.has_run_on_os_login());
    const proto::RunOnOsLogin& run_on_os_login =
        os_integration_state.run_on_os_login();
    ASSERT_THAT(run_on_os_login.run_on_os_login_mode(),
                testing::Eq(proto::RunOnOsLoginMode::NOT_RUN));
}

TEST_F(RunOnOsLoginSubManagerConfigureTest, VerifyRunOnOsLoginSetFromCommand) {
  const webapps::AppId& app_id = InstallWebApp();

  base::test::TestFuture<void> future;
  provider().scheduler().SetRunOnOsLoginMode(
      app_id, RunOnOsLoginMode::kWindowed, future.GetCallback());
  EXPECT_TRUE(future.Wait());

  auto state = registrar().GetAppCurrentOsIntegrationState(app_id);
  ASSERT_TRUE(state.has_value());
  const proto::WebAppOsIntegrationState& os_integration_state = state.value();
    ASSERT_TRUE(os_integration_state.has_run_on_os_login());
    const proto::RunOnOsLogin& run_on_os_login =
        os_integration_state.run_on_os_login();
    ASSERT_THAT(run_on_os_login.run_on_os_login_mode(),
                testing::Eq(proto::RunOnOsLoginMode::WINDOWED));
}

TEST_F(RunOnOsLoginSubManagerConfigureTest, VerifyPolicySettingBlocked) {
  const webapps::AppId& app_id = InstallWebApp();

  const char kWebAppSettingPolicyBlockedConfig[] = R"([{
    "manifest_id" : "https://example.com/path/index.html",
    "run_on_os_login": "blocked"
  }])";

  {
    base::test::TestFuture<void> policy_future;
    provider()
        .policy_manager()
        .SetRefreshPolicySettingsCompletedCallbackForTesting(
            policy_future.GetCallback());
    SetWebAppSettingsListPref(kWebAppSettingPolicyBlockedConfig);
    EXPECT_TRUE(policy_future.Wait());
  }

  auto state = registrar().GetAppCurrentOsIntegrationState(app_id);
  ASSERT_TRUE(state.has_value());
  const proto::WebAppOsIntegrationState& os_integration_state = state.value();
    ASSERT_TRUE(os_integration_state.has_run_on_os_login());
    const proto::RunOnOsLogin& run_on_os_login =
        os_integration_state.run_on_os_login();
    ASSERT_THAT(run_on_os_login.run_on_os_login_mode(),
                testing::Eq(proto::RunOnOsLoginMode::NOT_RUN));
}

TEST_F(RunOnOsLoginSubManagerConfigureTest, VerifyPolicySettingWindowedMode) {
  const webapps::AppId& app_id = InstallWebApp();

  const char kWebAppSettingPolicyWindowedConfig[] = R"([{
    "manifest_id" : "https://example.com/path/index.html",
    "run_on_os_login": "run_windowed"
  }])";

  {
    base::test::TestFuture<void> policy_future;
    provider()
        .policy_manager()
        .SetRefreshPolicySettingsCompletedCallbackForTesting(
            policy_future.GetCallback());
    SetWebAppSettingsListPref(kWebAppSettingPolicyWindowedConfig);
    EXPECT_TRUE(policy_future.Wait());
  }

  auto state = registrar().GetAppCurrentOsIntegrationState(app_id);
  ASSERT_TRUE(state.has_value());
  const proto::WebAppOsIntegrationState& os_integration_state = state.value();
    ASSERT_TRUE(os_integration_state.has_run_on_os_login());
    const proto::RunOnOsLogin& run_on_os_login =
        os_integration_state.run_on_os_login();
    ASSERT_THAT(run_on_os_login.run_on_os_login_mode(),
                testing::Eq(proto::RunOnOsLoginMode::WINDOWED));
}

TEST_F(RunOnOsLoginSubManagerConfigureTest, VerifyPolicySettingAllowedMode) {
  const webapps::AppId& app_id = InstallWebApp();

  const char kWebAppSettingPolicyAllowedConfig[] = R"([{
    "manifest_id" : "https://example.com/path/index.html",
    "run_on_os_login": "allowed"
  }])";

  {
    base::test::TestFuture<void> policy_future;
    provider()
        .policy_manager()
        .SetRefreshPolicySettingsCompletedCallbackForTesting(
            policy_future.GetCallback());
    SetWebAppSettingsListPref(kWebAppSettingPolicyAllowedConfig);
    EXPECT_TRUE(policy_future.Wait());
  }

  auto state = registrar().GetAppCurrentOsIntegrationState(app_id);
  ASSERT_TRUE(state.has_value());
  const proto::WebAppOsIntegrationState& os_integration_state = state.value();
    ASSERT_TRUE(os_integration_state.has_run_on_os_login());
    const proto::RunOnOsLogin& run_on_os_login =
        os_integration_state.run_on_os_login();
    ASSERT_THAT(run_on_os_login.run_on_os_login_mode(),
                testing::Eq(proto::RunOnOsLoginMode::NOT_RUN));
}

TEST_F(RunOnOsLoginSubManagerConfigureTest, StatesEmptyOnUninstall) {
  const webapps::AppId& app_id = InstallWebApp();
  test::UninstallAllWebApps(profile());
  auto state = registrar().GetAppCurrentOsIntegrationState(app_id);
  ASSERT_FALSE(state.has_value());
}

// Configure and Execute tests.
class RunOnOsLoginSubManagerExecuteTest
    : public RunOnOsLoginSubManagerTestBase {
 public:
  RunOnOsLoginSubManagerExecuteTest() = default;
  ~RunOnOsLoginSubManagerExecuteTest() override = default;

  bool IsRunOnOsLoginExecuteEnabled() {
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
    return true;
#else
    return false;
#endif
  }
};

TEST_F(RunOnOsLoginSubManagerExecuteTest, InstallRunOnOsLoginNotRun) {
  const webapps::AppId& app_id = InstallWebApp();
  const std::string& app_name = registrar().GetAppShortName(app_id);

  auto state = registrar().GetAppCurrentOsIntegrationState(app_id);
  ASSERT_TRUE(state.has_value());

  // A default install should have a Run on OS Login mode of kNotRun, so no
  // OS integration should be triggered.
    if (IsRunOnOsLoginExecuteEnabled()) {
      ASSERT_FALSE(OsIntegrationTestOverrideImpl::Get()->IsRunOnOsLoginEnabled(
          profile(), app_id, app_name));
    }
}

TEST_F(RunOnOsLoginSubManagerExecuteTest,
       InstallAndExecuteWindowedRunOnOsLogin) {
  const webapps::AppId& app_id = InstallWebApp();
  const std::string& app_name = registrar().GetAppShortName(app_id);

  base::test::TestFuture<void> future;
  provider().scheduler().SetRunOnOsLoginMode(
      app_id, RunOnOsLoginMode::kWindowed, future.GetCallback());
  EXPECT_TRUE(future.Wait());

  auto state = registrar().GetAppCurrentOsIntegrationState(app_id);
  ASSERT_TRUE(state.has_value());
    if (IsRunOnOsLoginExecuteEnabled()) {
      ASSERT_TRUE(OsIntegrationTestOverrideImpl::Get()->IsRunOnOsLoginEnabled(
          profile(), app_id, app_name));
    }
}

TEST_F(RunOnOsLoginSubManagerExecuteTest, BlockedPolicySettingNoOsIntegration) {
  const webapps::AppId& app_id = InstallWebApp();
  const std::string& app_name = registrar().GetAppShortName(app_id);

  const char kWebAppSettingPolicyBlockedConfig[] = R"([{
    "manifest_id" : "https://example.com/path/index.html",
    "run_on_os_login": "blocked"
  }])";

  {
    base::test::TestFuture<void> policy_future;
    provider()
        .policy_manager()
        .SetRefreshPolicySettingsCompletedCallbackForTesting(
            policy_future.GetCallback());
    SetWebAppSettingsListPref(kWebAppSettingPolicyBlockedConfig);
    EXPECT_TRUE(policy_future.Wait());
  }

  auto state = registrar().GetAppCurrentOsIntegrationState(app_id);
  ASSERT_TRUE(state.has_value());
    if (IsRunOnOsLoginExecuteEnabled()) {
      ASSERT_FALSE(OsIntegrationTestOverrideImpl::Get()->IsRunOnOsLoginEnabled(
          profile(), app_id, app_name));
    }
}

TEST_F(RunOnOsLoginSubManagerExecuteTest,
       WindowedPolicySettingAllowsOsIntegration) {
  const webapps::AppId& app_id = InstallWebApp();
  const std::string& app_name = registrar().GetAppShortName(app_id);

  const char kWebAppSettingPolicyBlockedConfig[] = R"([{
    "manifest_id" : "https://example.com/path/index.html",
    "run_on_os_login": "run_windowed"
  }])";

  {
    base::test::TestFuture<void> policy_future;
    provider()
        .policy_manager()
        .SetRefreshPolicySettingsCompletedCallbackForTesting(
            policy_future.GetCallback());
    SetWebAppSettingsListPref(kWebAppSettingPolicyBlockedConfig);
    EXPECT_TRUE(policy_future.Wait());
  }

  auto state = registrar().GetAppCurrentOsIntegrationState(app_id);
  ASSERT_TRUE(state.has_value());
    if (IsRunOnOsLoginExecuteEnabled()) {
      ASSERT_TRUE(OsIntegrationTestOverrideImpl::Get()->IsRunOnOsLoginEnabled(
          profile(), app_id, app_name));
    }
}

TEST_F(RunOnOsLoginSubManagerExecuteTest, UpdateRunOnOsLoginMode) {
  const webapps::AppId& app_id = InstallWebApp();
  const std::string& app_name = registrar().GetAppShortName(app_id);

  base::test::TestFuture<void> future_windowed;
  provider().scheduler().SetRunOnOsLoginMode(
      app_id, RunOnOsLoginMode::kWindowed, future_windowed.GetCallback());
  EXPECT_TRUE(future_windowed.Wait());

  auto state = registrar().GetAppCurrentOsIntegrationState(app_id);
  ASSERT_TRUE(state.has_value());
    if (IsRunOnOsLoginExecuteEnabled()) {
      ASSERT_TRUE(OsIntegrationTestOverrideImpl::Get()->IsRunOnOsLoginEnabled(
          profile(), app_id, app_name));
    }

  // Changing to kNotRun should update and unregister all OS integrations.
  base::test::TestFuture<void> future_not_run;
  provider().scheduler().SetRunOnOsLoginMode(app_id, RunOnOsLoginMode::kNotRun,
                                             future_not_run.GetCallback());
  EXPECT_TRUE(future_not_run.Wait());

  auto updated_state = registrar().GetAppCurrentOsIntegrationState(app_id);
  ASSERT_TRUE(updated_state.has_value());
    if (IsRunOnOsLoginExecuteEnabled()) {
      ASSERT_FALSE(OsIntegrationTestOverrideImpl::Get()->IsRunOnOsLoginEnabled(
          profile(), app_id, app_name));
    }
}

TEST_F(RunOnOsLoginSubManagerExecuteTest, UnregisterRunOnOsLogin) {
  const webapps::AppId& app_id = InstallWebApp();
  const std::string& app_name = registrar().GetAppShortName(app_id);

  base::test::TestFuture<void> future;
  provider().scheduler().SetRunOnOsLoginMode(
      app_id, RunOnOsLoginMode::kWindowed, future.GetCallback());
  EXPECT_TRUE(future.Wait());

  auto state = registrar().GetAppCurrentOsIntegrationState(app_id);
  ASSERT_TRUE(state.has_value());
  if (IsRunOnOsLoginExecuteEnabled()) {
    EXPECT_TRUE(OsIntegrationTestOverrideImpl::Get()->IsRunOnOsLoginEnabled(
        profile(), app_id, app_name));
  }

  test::UninstallAllWebApps(profile());
  if (IsRunOnOsLoginExecuteEnabled()) {
    EXPECT_FALSE(OsIntegrationTestOverrideImpl::Get()->IsRunOnOsLoginEnabled(
        profile(), app_id, app_name));
  }
}

TEST_F(RunOnOsLoginSubManagerExecuteTest, ForceUnregisterAppInRegistry) {
  const webapps::AppId& app_id = InstallWebApp();
  const std::string& app_name = registrar().GetAppShortName(app_id);

  base::test::TestFuture<void> future;
  provider().scheduler().SetRunOnOsLoginMode(
      app_id, RunOnOsLoginMode::kWindowed, future.GetCallback());
  EXPECT_TRUE(future.Wait());

  auto state = registrar().GetAppCurrentOsIntegrationState(app_id);
  ASSERT_TRUE(state.has_value());
  if (IsRunOnOsLoginExecuteEnabled()) {
    EXPECT_TRUE(OsIntegrationTestOverrideImpl::Get()->IsRunOnOsLoginEnabled(
        profile(), app_id, app_name));
  }

  SynchronizeOsOptions options;
  options.force_unregister_os_integration = true;
  test::SynchronizeOsIntegration(profile(), app_id, options);

  if (IsRunOnOsLoginExecuteEnabled()) {
    EXPECT_FALSE(OsIntegrationTestOverrideImpl::Get()->IsRunOnOsLoginEnabled(
        profile(), app_id, app_name));
  }
}

TEST_F(RunOnOsLoginSubManagerExecuteTest, ForceUnregisterAppNotInRegistry) {
  const webapps::AppId& app_id = InstallWebApp();
  const std::string& app_name = registrar().GetAppShortName(app_id);

  base::test::TestFuture<void> future;
  provider().scheduler().SetRunOnOsLoginMode(
      app_id, RunOnOsLoginMode::kWindowed, future.GetCallback());
  EXPECT_TRUE(future.Wait());

  auto state = registrar().GetAppCurrentOsIntegrationState(app_id);
  ASSERT_TRUE(state.has_value());
  if (IsRunOnOsLoginExecuteEnabled()) {
    ASSERT_TRUE(OsIntegrationTestOverrideImpl::Get()->IsRunOnOsLoginEnabled(
        profile(), app_id, app_name));
  }

  test::UninstallAllWebApps(profile());
  if (IsRunOnOsLoginExecuteEnabled()) {
    EXPECT_FALSE(OsIntegrationTestOverrideImpl::Get()->IsRunOnOsLoginEnabled(
        profile(), app_id, app_name));
  }
  EXPECT_FALSE(provider().registrar_unsafe().IsInstalled(app_id));

  // This should have no affect.
  SynchronizeOsOptions options;
  options.force_unregister_os_integration = true;
  test::SynchronizeOsIntegration(profile(), app_id, options);
  if (IsRunOnOsLoginExecuteEnabled()) {
    EXPECT_FALSE(OsIntegrationTestOverrideImpl::Get()->IsRunOnOsLoginEnabled(
        profile(), app_id, app_name));
  }
}

}  // namespace

}  // namespace web_app
