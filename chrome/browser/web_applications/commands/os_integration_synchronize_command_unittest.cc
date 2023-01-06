// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/os_integration_synchronize_command.h"

#include <memory>

#include "base/files/file_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/os_integration/os_integration_test_override.h"
#include "chrome/browser/web_applications/os_integration/web_app_file_handler_manager.h"
#include "chrome/browser/web_applications/os_integration/web_app_protocol_handler_manager.h"
#include "chrome/browser/web_applications/os_integration/web_app_shortcut_manager.h"
#include "chrome/browser/web_applications/proto/web_app_os_integration_state.pb.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/chrome_features.h"
#include "components/services/app_service/public/cpp/protocol_handler_info.h"
#include "components/webapps/browser/install_result_code.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace web_app {
namespace {

using ::testing::Eq;

class OsIntegrationSynchronizeCommandTest
    : public WebAppTest,
      public ::testing::WithParamInterface<OsIntegrationSubManagersState> {
 public:
  const GURL kWebAppUrl = GURL("https://example.com");

  OsIntegrationSynchronizeCommandTest() {
    if (GetParam() == OsIntegrationSubManagersState::kSaveStateToDB) {
      scoped_feature_list_.InitWithFeaturesAndParameters(
          {{features::kOsIntegrationSubManagers, {{"stage", "write_config"}}}},
          /*disabled_features=*/{});
    } else {
      scoped_feature_list_.InitWithFeatures(
          {}, {features::kOsIntegrationSubManagers});
    }
  }

  ~OsIntegrationSynchronizeCommandTest() override = default;

  void SetUp() override {
    WebAppTest::SetUp();
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      test_override_ =
          OsIntegrationTestOverride::OverrideForTesting(base::GetHomeDir());
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
    EXPECT_TRUE(test::UninstallAllWebApps(profile()));
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      test_override_.reset();
    }
    WebAppTest::TearDown();
  }

  AppId InstallAppWithProtocolHandlers(
      const std::vector<apps::ProtocolHandlerInfo>& protocol_handlers) {
    auto info = std::make_unique<WebAppInstallInfo>();
    info->start_url = kWebAppUrl;
    info->title = u"Test App";
    info->user_display_mode = web_app::mojom::UserDisplayMode::kStandalone;
    info->protocol_handlers = protocol_handlers;

    base::test::TestFuture<const AppId&, webapps::InstallResultCode> result;
    // InstallFromInfo is used so that the DB states are updated but OS
    // integration is not triggered.
    provider()->scheduler().InstallFromInfo(
        std::move(info), /*overwrite_existing_manifest_fields=*/true,
        webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON,
        result.GetCallback());
    bool success = result.Wait();
    EXPECT_TRUE(success);
    if (!success) {
      return AppId();
    }
    EXPECT_EQ(result.Get<webapps::InstallResultCode>(),
              webapps::InstallResultCode::kSuccessNewInstall);
    return result.Get<AppId>();
  }

 protected:
  WebAppProvider* provider() { return provider_; }

 private:
  raw_ptr<FakeWebAppProvider> provider_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<OsIntegrationTestOverride::BlockingRegistration>
      test_override_;
};

TEST_P(OsIntegrationSynchronizeCommandTest, SynchronizeWorks) {
  apps::ProtocolHandlerInfo protocol_handler;
  const std::string handler_url =
      std::string(kWebAppUrl.spec()) + "/testing=%s";
  protocol_handler.url = GURL(handler_url);
  protocol_handler.protocol = "web+test";
  const AppId& app_id = InstallAppWithProtocolHandlers({protocol_handler});

  absl::optional<proto::WebAppOsIntegrationState> current_states =
      provider()->registrar_unsafe().GetAppCurrentOsIntegrationState(app_id);
  ASSERT_TRUE(current_states.has_value());
  ASSERT_FALSE(current_states.value().has_protocols_handled());

  // OS Integration should be triggered now.
  base::test::TestFuture<void> synchronize_future;
  provider()->scheduler().SynchronizeOsIntegration(
      app_id, synchronize_future.GetCallback());
  EXPECT_TRUE(synchronize_future.Wait());

  absl::optional<proto::WebAppOsIntegrationState> updated_states =
      provider()->registrar_unsafe().GetAppCurrentOsIntegrationState(app_id);
  ASSERT_TRUE(updated_states.has_value());
  const proto::WebAppOsIntegrationState& os_integration_state =
      updated_states.value();
  if (base::FeatureList::IsEnabled(features::kOsIntegrationSubManagers)) {
    ASSERT_THAT(os_integration_state.protocols_handled().protocols_size(),
                testing::Eq(1));

    const proto::ProtocolsHandled::Protocol& protocol_handler_state =
        os_integration_state.protocols_handled().protocols(0);

    ASSERT_THAT(protocol_handler_state.protocol(),
                testing::Eq(protocol_handler.protocol));
    ASSERT_THAT(protocol_handler_state.url(), testing::Eq(handler_url));
  } else {
    ASSERT_FALSE(os_integration_state.has_protocols_handled());
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    OsIntegrationSynchronizeCommandTest,
    ::testing::Values(OsIntegrationSubManagersState::kSaveStateToDB,
                      OsIntegrationSubManagersState::kDisabled),
    test::GetOsIntegrationSubManagersTestName);

}  // namespace
}  // namespace web_app
