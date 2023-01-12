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
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
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

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/web_applications/app_shim_registry_mac.h"
#endif

namespace web_app {

using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::IsEmpty;
enum class ApiApprovalState;

namespace {

class ProtocolHandlingSubManagerTestBase : public WebAppTest {
 public:
  const GURL kWebAppUrl = GURL("https://example.com/path/index.html");

  ProtocolHandlingSubManagerTestBase() = default;
  ~ProtocolHandlingSubManagerTestBase() override = default;

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
    test::UninstallAllWebApps(profile());
    // Blocking required due to file operations in the shortcut override
    // destructor.
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      test_override_.reset();
    }
    WebAppTest::TearDown();
  }

  web_app::AppId InstallWebAppWithProtocolHandlers(
      const std::vector<apps::ProtocolHandlerInfo>& protocol_handlers) {
    std::unique_ptr<WebAppInstallInfo> info =
        std::make_unique<WebAppInstallInfo>();
    info->start_url = kWebAppUrl;
    info->title = u"Test App";
    info->user_display_mode = web_app::mojom::UserDisplayMode::kStandalone;
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

 protected:
  WebAppProvider& provider() { return *provider_; }

 private:
  raw_ptr<FakeWebAppProvider> provider_;
  std::unique_ptr<OsIntegrationTestOverride::BlockingRegistration>
      test_override_;
};

// Synchronize tests only. Tests here should only verify DB updates.
class ProtocolHandlingSynchronizeTest
    : public ProtocolHandlingSubManagerTestBase,
      public ::testing::WithParamInterface<OsIntegrationSubManagersState> {
 public:
  ProtocolHandlingSynchronizeTest() = default;
  ~ProtocolHandlingSynchronizeTest() override = default;

  void SetUp() override {
    if (GetParam() == OsIntegrationSubManagersState::kSaveStateToDB) {
      scoped_feature_list_.InitAndEnableFeatureWithParameters(
          features::kOsIntegrationSubManagers, {{"stage", "write_config"}});
    } else {
      scoped_feature_list_.InitWithFeatures(
          /*enabled_features=*/{},
          /*disabled_features=*/{features::kOsIntegrationSubManagers});
    }
    ProtocolHandlingSubManagerTestBase::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(ProtocolHandlingSynchronizeTest, ConfigureOnlyProtocolHandler) {
  apps::ProtocolHandlerInfo protocol_handler;
  const std::string handler_url =
      std::string(kWebAppUrl.spec()) + "/testing=%s";
  protocol_handler.url = GURL(handler_url);
  protocol_handler.protocol = "web+test";

  const AppId app_id = InstallWebAppWithProtocolHandlers({protocol_handler});

  auto state =
      provider().registrar_unsafe().GetAppCurrentOsIntegrationState(app_id);
  ASSERT_TRUE(state.has_value());
  const proto::WebAppOsIntegrationState& os_integration_state = state.value();
  if (AreOsIntegrationSubManagersEnabled()) {
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

TEST_P(ProtocolHandlingSynchronizeTest, UninstalledAppDoesNotConfigure) {
  apps::ProtocolHandlerInfo protocol_handler;
  const std::string handler_url =
      std::string(kWebAppUrl.spec()) + "/testing=%s";
  protocol_handler.url = GURL(handler_url);
  protocol_handler.protocol = "web+test";

  const AppId app_id = InstallWebAppWithProtocolHandlers({protocol_handler});
  test::UninstallAllWebApps(profile());

  auto state =
      provider().registrar_unsafe().GetAppCurrentOsIntegrationState(app_id);
  ASSERT_FALSE(state.has_value());
}

TEST_P(ProtocolHandlingSynchronizeTest, ConfigureProtocolHandlerDisallowed) {
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
        app_id, "web+test", ApiApprovalState::kDisallowed,
        disallowed_future.GetCallback());
    EXPECT_TRUE(disallowed_future.Wait());
  }

  auto state =
      provider().registrar_unsafe().GetAppCurrentOsIntegrationState(app_id);
  ASSERT_TRUE(state.has_value());
  const proto::WebAppOsIntegrationState& os_integration_state = state.value();
  if (AreOsIntegrationSubManagersEnabled()) {
    ASSERT_THAT(os_integration_state.protocols_handled().protocols_size(),
                testing::Eq(1));

    const proto::ProtocolsHandled::Protocol& protocol_handler_state =
        os_integration_state.protocols_handled().protocols(0);

    ASSERT_THAT(protocol_handler_state.protocol(),
                testing::Eq(protocol_handler2.protocol));
    ASSERT_THAT(protocol_handler_state.url(), testing::Eq(handler_url2));
  } else {
    ASSERT_FALSE(os_integration_state.has_protocols_handled());
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ProtocolHandlingSynchronizeTest,
    ::testing::Values(OsIntegrationSubManagersState::kSaveStateToDB,
                      OsIntegrationSubManagersState::kDisabled),
    test::GetOsIntegrationSubManagersTestName);

// Synchronize and Execute tests from here onwards. Tests here should
// verify both DB updates as well as OS registrations/unregistrations.
class ProtocolHandlingSynchronizeAndExecuteTest
    : public ProtocolHandlingSubManagerTestBase,
      public ::testing::WithParamInterface<OsIntegrationSubManagersState> {
 public:
  ProtocolHandlingSynchronizeAndExecuteTest() = default;
  ~ProtocolHandlingSynchronizeAndExecuteTest() override = default;

  void SetUp() override {
    if (GetParam() == OsIntegrationSubManagersState::kSaveStateAndExecute) {
      scoped_feature_list_.InitAndEnableFeatureWithParameters(
          features::kOsIntegrationSubManagers,
          {{"stage", "execute_and_write_config"}});
    } else {
      scoped_feature_list_.InitWithFeatures(
          /*enabled_features=*/{},
          /*disabled_features=*/{features::kOsIntegrationSubManagers});
    }
    ProtocolHandlingSubManagerTestBase::SetUp();
  }

#if BUILDFLAG(IS_MAC)
  std::vector<std::string> GetAppShimRegisteredProtocolHandlers(
      const AppId& app_id) {
    std::vector<std::string> protocol_schemes;
    for (const auto& [file_path, handler] :
         AppShimRegistry::Get()->GetHandlersForApp(app_id)) {
      protocol_schemes.insert(protocol_schemes.end(),
                              handler.protocol_handlers.begin(),
                              handler.protocol_handlers.end());
    }
    return protocol_schemes;
  }
#endif  // BUILDFLAG(IS_MAC)

  bool AreProtocolsRegisteredWithOs() {
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
    return false;
#else
    return true;
#endif
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(ProtocolHandlingSynchronizeAndExecuteTest, Register) {
  apps::ProtocolHandlerInfo protocol_handler;
  const std::string handler_url =
      std::string(kWebAppUrl.spec()) + "/testing=%s";
  protocol_handler.url = GURL(handler_url);
  protocol_handler.protocol = "web+test";
  const AppId app_id = InstallWebAppWithProtocolHandlers({protocol_handler});

  auto state =
      provider().registrar_unsafe().GetAppCurrentOsIntegrationState(app_id);
  ASSERT_TRUE(state.has_value());
  const proto::WebAppOsIntegrationState& os_integration_state = state.value();
  if (AreSubManagersExecuteEnabled()) {
    ASSERT_THAT(os_integration_state.protocols_handled().protocols_size(),
                testing::Eq(1));

    const proto::ProtocolsHandled::Protocol& protocol_handler_state =
        os_integration_state.protocols_handled().protocols(0);

    ASSERT_THAT(protocol_handler_state.protocol(),
                testing::Eq(protocol_handler.protocol));
    ASSERT_THAT(protocol_handler_state.url(), testing::Eq(handler_url));

#if BUILDFLAG(IS_MAC)
    EXPECT_THAT(GetAppShimRegisteredProtocolHandlers(app_id),
                testing::ElementsAre(protocol_handler.protocol));
#endif

    if (AreProtocolsRegisteredWithOs()) {
      // Installation registers the protocol handlers.
      EXPECT_THAT(
          GetOsIntegrationTestOverride()->protocol_scheme_registrations(),
          testing::ElementsAre(std::make_tuple(
              app_id, std::vector({protocol_handler.protocol}))));
    }
  } else {
    ASSERT_FALSE(os_integration_state.has_protocols_handled());
  }
}

TEST_P(ProtocolHandlingSynchronizeAndExecuteTest, Unregister) {
  apps::ProtocolHandlerInfo protocol_handler;
  const std::string handler_url =
      std::string(kWebAppUrl.spec()) + "/testing=%s";
  protocol_handler.url = GURL(handler_url);
  protocol_handler.protocol = "web+test";
  const AppId app_id = InstallWebAppWithProtocolHandlers({protocol_handler});
  test::UninstallAllWebApps(profile());

  auto state =
      provider().registrar_unsafe().GetAppCurrentOsIntegrationState(app_id);
  ASSERT_FALSE(state.has_value());

#if BUILDFLAG(IS_MAC)
  ASSERT_THAT(GetAppShimRegisteredProtocolHandlers(app_id), testing::IsEmpty());
#endif

  if (AreProtocolsRegisteredWithOs()) {
    // TODO(crbug.com/1404819): Update tests to verify protocol handling
    // unregistration as part of update.
    // There should only be a single value for registration, as unregistration
    // is a no-op for GetOsIntegrationTestOverride().
    ASSERT_THAT(GetOsIntegrationTestOverride()->protocol_scheme_registrations(),
                testing::ElementsAre(std::make_tuple(
                    app_id, std::vector({protocol_handler.protocol}))));
  }
}

TEST_P(ProtocolHandlingSynchronizeAndExecuteTest, UpdateHandlers) {
  apps::ProtocolHandlerInfo protocol_handler_approved;
  const std::string handler_url1 =
      std::string(kWebAppUrl.spec()) + "/testing=%s";
  protocol_handler_approved.url = GURL(handler_url1);
  protocol_handler_approved.protocol = "web+test";

  apps::ProtocolHandlerInfo protocol_handler_disapproved;
  const std::string handler_url2 =
      std::string(kWebAppUrl.spec()) + "/testing_protocol=%s";
  protocol_handler_disapproved.url = GURL(handler_url2);
  protocol_handler_disapproved.protocol = "web+test+protocol";

  const AppId app_id = InstallWebAppWithProtocolHandlers(
      {protocol_handler_approved, protocol_handler_disapproved});
  {
    base::test::TestFuture<void> disallowed_future;
    provider().scheduler().UpdateProtocolHandlerUserApproval(
        app_id, protocol_handler_disapproved.protocol,
        ApiApprovalState::kDisallowed, disallowed_future.GetCallback());
    EXPECT_TRUE(disallowed_future.Wait());
  }

  auto state =
      provider().registrar_unsafe().GetAppCurrentOsIntegrationState(app_id);
  ASSERT_TRUE(state.has_value());
  const proto::WebAppOsIntegrationState& os_integration_state = state.value();

  if (AreSubManagersExecuteEnabled()) {
    ASSERT_THAT(os_integration_state.protocols_handled().protocols_size(),
                testing::Eq(1));

    const proto::ProtocolsHandled::Protocol& protocol_handler_state =
        os_integration_state.protocols_handled().protocols(0);

    ASSERT_THAT(protocol_handler_state.protocol(),
                testing::Eq(protocol_handler_approved.protocol));
    ASSERT_THAT(protocol_handler_state.url(), testing::Eq(handler_url1));

#if BUILDFLAG(IS_MAC)
    ASSERT_THAT(GetAppShimRegisteredProtocolHandlers(app_id),
                testing::ElementsAre(protocol_handler_approved.protocol));
#endif
    if (AreProtocolsRegisteredWithOs()) {
      // TODO(crbug.com/1404819): Update tests to verify protocol handling
      // unregistration as part of update.
      ASSERT_THAT(
          GetOsIntegrationTestOverride()->protocol_scheme_registrations(),
          testing::ElementsAre(
              std::make_tuple(
                  app_id, std::vector({protocol_handler_approved.protocol,
                                       protocol_handler_disapproved.protocol})),
              std::make_tuple(
                  app_id, std::vector({protocol_handler_approved.protocol}))));
    }
  } else {
    ASSERT_FALSE(os_integration_state.has_protocols_handled());
  }
}

TEST_P(ProtocolHandlingSynchronizeAndExecuteTest, DataEqualNoOp) {
  apps::ProtocolHandlerInfo protocol_handler;
  const std::string handler_url =
      std::string(kWebAppUrl.spec()) + "/testing=%s";
  protocol_handler.url = GURL(handler_url);
  protocol_handler.protocol = "web+test";

  const AppId app_id = InstallWebAppWithProtocolHandlers({protocol_handler});
  {
    base::test::TestFuture<void> future;
    provider().scheduler().UpdateProtocolHandlerUserApproval(
        app_id, protocol_handler.protocol, ApiApprovalState::kAllowed,
        future.GetCallback());
    EXPECT_TRUE(future.Wait());
  }

  auto state =
      provider().registrar_unsafe().GetAppCurrentOsIntegrationState(app_id);
  ASSERT_TRUE(state.has_value());
  const proto::WebAppOsIntegrationState& os_integration_state = state.value();

  if (AreSubManagersExecuteEnabled()) {
    ASSERT_THAT(os_integration_state.protocols_handled().protocols_size(),
                testing::Eq(1));

    const proto::ProtocolsHandled::Protocol& protocol_handler_state =
        os_integration_state.protocols_handled().protocols(0);

    ASSERT_THAT(protocol_handler_state.protocol(),
                testing::Eq(protocol_handler.protocol));
    ASSERT_THAT(protocol_handler_state.url(), testing::Eq(handler_url));

#if BUILDFLAG(IS_MAC)
    ASSERT_THAT(GetAppShimRegisteredProtocolHandlers(app_id),
                testing::ElementsAre(protocol_handler.protocol));
#endif
    if (AreProtocolsRegisteredWithOs()) {
      ASSERT_THAT(
          GetOsIntegrationTestOverride()->protocol_scheme_registrations(),
          testing::ElementsAre(std::make_tuple(
              app_id, std::vector({protocol_handler.protocol}))));
    }
  } else {
    ASSERT_FALSE(os_integration_state.has_protocols_handled());
  }
}

TEST_P(ProtocolHandlingSynchronizeAndExecuteTest,
       MultipleSynchronizeEmptyData) {
  const AppId app_id1 = InstallWebAppWithProtocolHandlers(
      std::vector<apps::ProtocolHandlerInfo>());
  const AppId app_id2 = InstallWebAppWithProtocolHandlers(
      std::vector<apps::ProtocolHandlerInfo>());
  ASSERT_THAT(app_id1, testing::Eq(app_id2));

  auto state =
      provider().registrar_unsafe().GetAppCurrentOsIntegrationState(app_id1);
  ASSERT_TRUE(state.has_value());
  const proto::WebAppOsIntegrationState& os_integration_state = state.value();

  if (AreSubManagersExecuteEnabled()) {
    ASSERT_THAT(os_integration_state.protocols_handled().protocols_size(),
                testing::Eq(0));
#if BUILDFLAG(IS_MAC)
    ASSERT_THAT(GetAppShimRegisteredProtocolHandlers(app_id1),
                testing::IsEmpty());
#endif
    if (AreProtocolsRegisteredWithOs()) {
      // TODO(crbug.com/1404819): Update tests to streamline proper
      // representation of protocols registered, independent of OSes.
      // These values are set by the ShortcutHandlingSubManager.
#if BUILDFLAG(IS_WIN)
      ASSERT_THAT(
          GetOsIntegrationTestOverride()->protocol_scheme_registrations(),
          testing::IsEmpty());
#else
      ASSERT_THAT(
          GetOsIntegrationTestOverride()->protocol_scheme_registrations(),
          testing::ElementsAre(
              std::make_tuple(app_id1, std::vector<std::string>()),
              std::make_tuple(app_id1, std::vector<std::string>())));
#endif
    }
  } else {
    ASSERT_FALSE(os_integration_state.has_protocols_handled());
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ProtocolHandlingSynchronizeAndExecuteTest,
    ::testing::Values(OsIntegrationSubManagersState::kSaveStateAndExecute,
                      OsIntegrationSubManagersState::kDisabled),
    test::GetOsIntegrationSubManagersTestName);

}  // namespace

}  // namespace web_app
