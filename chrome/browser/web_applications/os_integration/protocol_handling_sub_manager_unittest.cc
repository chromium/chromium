// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>
#include <vector>

#include "base/files/file_util.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/os_integration/web_app_file_handler_manager.h"
#include "chrome/browser/web_applications/os_integration/web_app_protocol_handler_manager.h"
#include "chrome/browser/web_applications/os_integration/web_app_shortcut.h"
#include "chrome/browser/web_applications/os_integration/web_app_shortcut_manager.h"
#include "chrome/browser/web_applications/proto/web_app_os_integration_state.pb.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_params.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/chrome_features.h"
#include "components/services/app_service/public/cpp/protocol_handler_info.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/uninstall_result_code.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace web_app {

using ::testing::Eq;

namespace {

class ProtocolHandlingSubManagerTest
    : public WebAppTest,
      public ::testing::WithParamInterface<OsIntegrationSubManagersState> {
 public:
  const GURL kWebAppUrl = GURL("https://example.com/path/index.html");

  ProtocolHandlingSubManagerTest() = default;
  ~ProtocolHandlingSubManagerTest() override = default;

  void SetUp() override {
    WebAppTest::SetUp();
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      shortcut_override_ =
          ShortcutOverrideForTesting::OverrideForTesting(base::GetHomeDir());
    }

    if (EnableOsIntegrationSubManager()) {
      scoped_feature_list_.InitAndEnableFeatureWithParameters(
          features::kOsIntegrationSubManagers, {{"stage", "write_config"}});
    } else {
      scoped_feature_list_.InitWithFeatures(
          /*enabled_features=*/{},
          /*disabled_features=*/{features::kOsIntegrationSubManagers});
    }

    provider_ = FakeWebAppProvider::Get(profile());

    auto file_handler_manager =
        std::make_unique<WebAppFileHandlerManager>(profile());
    auto protocol_handler_manager =
        std::make_unique<WebAppProtocolHandlerManager>(profile());
    auto shortcut_manager = std::make_unique<WebAppShortcutManager>(
        profile(), /*icon_manager=*/nullptr, file_handler_manager.get(),
        protocol_handler_manager.get());
    auto os_integration_manager = std::make_unique<OsIntegrationManager>(
        profile(), std::move(shortcut_manager), std::move(file_handler_manager),
        std::move(protocol_handler_manager), /*url_handler_manager=*/nullptr);

    provider_->SetOsIntegrationManager(std::move(os_integration_manager));
    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  void TearDown() override {
    // Blocking required due to file operations in the shortcut override
    // destructor.
    base::ScopedAllowBlockingForTesting allow_blocking;
    shortcut_override_.reset();
    WebAppTest::TearDown();
  }

  web_app::AppId InstallWebAppWithProtocolHandlers(
      const std::vector<apps::ProtocolHandlerInfo>& protocol_handlers) {
    std::unique_ptr<WebAppInstallInfo> info =
        std::make_unique<WebAppInstallInfo>();
    info->start_url = kWebAppUrl;
    info->title = u"Test App";
    info->user_display_mode = web_app::UserDisplayMode::kStandalone;
    info->protocol_handlers = protocol_handlers;
    base::test::TestFuture<const AppId&, webapps::InstallResultCode> result;
    // InstallFromInfoWithParams is used instead of InstallFromInfo, because
    // InstallFromInfo doesn't register OS integration.
    provider().scheduler().InstallFromInfoWithParams(
        std::move(info), /*overwrite_existing_manifest_fields=*/true,
        webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON,
        result.GetCallback(), WebAppInstallParams());
    bool success = result.Wait();
    EXPECT_TRUE(success);
    if (!success)
      return AppId();
    EXPECT_EQ(result.Get<webapps::InstallResultCode>(),
              webapps::InstallResultCode::kSuccessNewInstall);
    return result.Get<AppId>();
  }

  void UninstallWebApp(const AppId& app_id) {
    base::test::TestFuture<webapps::UninstallResultCode> uninstall_future;
    provider_->install_finalizer().UninstallWebApp(
        app_id, webapps::WebappUninstallSource::kAppsPage,
        uninstall_future.GetCallback());
    EXPECT_THAT(uninstall_future.Get(),
                testing::Eq(webapps::UninstallResultCode::kSuccess));
  }

  bool EnableOsIntegrationSubManager() {
    return GetParam() == OsIntegrationSubManagersState::kEnabled;
  }

 protected:
  WebAppProvider& provider() { return *provider_; }

 private:
  raw_ptr<FakeWebAppProvider> provider_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<ShortcutOverrideForTesting::BlockingRegistration>
      shortcut_override_;
};

TEST_P(ProtocolHandlingSubManagerTest, ConfigureOnlyProtocolHandler) {
  apps::ProtocolHandlerInfo protocol_handler;
  const std::string handler_url =
      std::string(kWebAppUrl.spec()) + "/testing=%s";
  protocol_handler.url = GURL(handler_url);
  protocol_handler.protocol = "web+test";

  const AppId app_id = InstallWebAppWithProtocolHandlers({protocol_handler});

  auto state = provider().registrar().GetAppCurrentOsIntegrationState(app_id);
  if (EnableOsIntegrationSubManager()) {
    ASSERT_TRUE(state.has_value());
    const proto::WebAppOsIntegrationState& os_integration_state = state.value();

    ASSERT_THAT(os_integration_state.manifest_protocol_handlers_states_size(),
                testing::Eq(1));

    const proto::WebAppProtocolHandler& protocol_handler_state =
        os_integration_state.manifest_protocol_handlers_states(0);

    ASSERT_THAT(protocol_handler_state.protocol(),
                testing::Eq(protocol_handler.protocol));
    ASSERT_THAT(protocol_handler_state.url(), testing::Eq(handler_url));
  } else {
    ASSERT_FALSE(state.has_value());
  }

  UninstallWebApp(app_id);
}

TEST_P(ProtocolHandlingSubManagerTest, UninstalledAppDoesNotConfigure) {
  apps::ProtocolHandlerInfo protocol_handler;
  const std::string handler_url =
      std::string(kWebAppUrl.spec()) + "/testing=%s";
  protocol_handler.url = GURL(handler_url);
  protocol_handler.protocol = "web+test";

  const AppId app_id = InstallWebAppWithProtocolHandlers({protocol_handler});
  UninstallWebApp(app_id);

  auto state = provider().registrar().GetAppCurrentOsIntegrationState(app_id);
  ASSERT_FALSE(state.has_value());
}

TEST_P(ProtocolHandlingSubManagerTest, ConfigureProtocolHandlerDisallowed) {
  apps::ProtocolHandlerInfo protocol_handler1;
  const std::string handler_url1 =
      std::string(kWebAppUrl.spec()) + "/testing=%s";
  protocol_handler1.url = GURL(handler_url1);
  protocol_handler1.protocol = "web+test";

  apps::ProtocolHandlerInfo protocol_handler2;
  const std::string handler_url2 =
      std::string(kWebAppUrl.spec()) + "/testing_protocol=%s";
  protocol_handler2.url = GURL(handler_url2);
  protocol_handler2.protocol = "web+test+protocol";

  const AppId app_id =
      InstallWebAppWithProtocolHandlers({protocol_handler1, protocol_handler2});
  {
    base::test::TestFuture<void> disallowed_future;
    provider().scheduler().UpdateProtocolHandlerUserApproval(
        app_id, "web+test", /*allowed=*/false, disallowed_future.GetCallback());
    EXPECT_TRUE(disallowed_future.Wait());
  }

  auto state = provider().registrar().GetAppCurrentOsIntegrationState(app_id);
  if (EnableOsIntegrationSubManager()) {
    ASSERT_TRUE(state.has_value());
    const proto::WebAppOsIntegrationState& os_integration_state = state.value();

    ASSERT_THAT(os_integration_state.manifest_protocol_handlers_states_size(),
                testing::Eq(1));

    const proto::WebAppProtocolHandler& protocol_handler_state =
        os_integration_state.manifest_protocol_handlers_states(0);

    ASSERT_THAT(protocol_handler_state.protocol(),
                testing::Eq(protocol_handler2.protocol));
    ASSERT_THAT(protocol_handler_state.url(), testing::Eq(handler_url2));
  } else {
    ASSERT_FALSE(state.has_value());
  }

  UninstallWebApp(app_id);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ProtocolHandlingSubManagerTest,
    ::testing::Values(OsIntegrationSubManagersState::kEnabled,
                      OsIntegrationSubManagersState::kDisabled),
    test::GetOsIntegrationSubManagersTestName);

}  // namespace

}  // namespace web_app
