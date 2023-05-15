// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/contains.h"
#include "base/files/file_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/chrome_features.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/web_applications/app_shim_registry_mac.h"
#endif

namespace web_app {

enum class ApiApprovalState;

namespace {
const char16_t kAppName[] = u"Test App";

// A few tests have Windows specific assertions because Windows is the only
// OS where protocols are registered on the OS differently compared to
// other OSes where protocols are bundled into the shortcut
// registration/update/unregistration flow.
class UpdateProtocolHandlerApprovalCommandTest
    : public WebAppControllerBrowserTest,
      public ::testing::WithParamInterface<OsIntegrationSubManagersState> {
 public:
  const GURL kTestAppUrl = GURL("https://example.com");

  UpdateProtocolHandlerApprovalCommandTest() {
    if (GetParam() == OsIntegrationSubManagersState::kSaveStateToDB) {
      scoped_feature_list_.InitAndEnableFeatureWithParameters(
          features::kOsIntegrationSubManagers, {{"stage", "write_config"}});
    } else if (GetParam() ==
               OsIntegrationSubManagersState::kSaveStateAndExecute) {
      scoped_feature_list_.InitAndEnableFeatureWithParameters(
          features::kOsIntegrationSubManagers,
          {{"stage", "execute_and_write_config"}});
    } else {
      scoped_feature_list_.InitWithFeatures(
          /*enabled_features=*/{},
          /*disabled_features=*/{features::kOsIntegrationSubManagers});
    }
  }
  ~UpdateProtocolHandlerApprovalCommandTest() override = default;

  void SetUpOnMainThread() override {
    os_hooks_suppress_.reset();
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      test_override_ =
          OsIntegrationTestOverrideImpl::OverrideForTesting(base::GetHomeDir());
    }
    WebAppControllerBrowserTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    // Uninstallation of all apps is required for the shortcut override
    // destruction.
    EXPECT_TRUE(test::UninstallAllWebApps(profile()));
    {
      // Blocking required due to file operations in the shortcut override
      // destructor.
      base::ScopedAllowBlockingForTesting allow_blocking;
      test_override_.reset();
    }
    WebAppControllerBrowserTest::TearDownOnMainThread();
  }

  web_app::AppId InstallWebAppWithProtocolHandlers(
      const std::vector<apps::ProtocolHandlerInfo>& protocol_handlers) {
    std::unique_ptr<WebAppInstallInfo> info =
        std::make_unique<WebAppInstallInfo>();
    info->start_url = GURL(kTestAppUrl);
    info->title = kAppName;
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
  std::unique_ptr<OsIntegrationTestOverrideImpl::BlockingRegistration>
      test_override_;
};

IN_PROC_BROWSER_TEST_P(UpdateProtocolHandlerApprovalCommandTest, Install) {
  apps::ProtocolHandlerInfo protocol_handler;
  const std::string handler_url =
      std::string(kTestAppUrl.spec()) + "/testing=%s";
  protocol_handler.url = GURL(handler_url);
  protocol_handler.protocol = "web+test";
  web_app::AppId app_id = InstallWebAppWithProtocolHandlers({protocol_handler});

  EXPECT_THAT(provider().registrar_unsafe().IsAllowedLaunchProtocol(
                  app_id, protocol_handler.protocol),
              testing::IsFalse());
  EXPECT_THAT(provider().registrar_unsafe().IsDisallowedLaunchProtocol(
                  app_id, protocol_handler.protocol),
              testing::IsFalse());

#if BUILDFLAG(IS_MAC)
  EXPECT_THAT(GetAppShimRegisteredProtocolHandlers(app_id),
              testing::ElementsAre(protocol_handler.protocol));
#endif

  if (AreProtocolsRegisteredWithOs()) {
    // Installation registers the protocol handlers.
    EXPECT_THAT(
        OsIntegrationTestOverrideImpl::Get()->protocol_scheme_registrations(),
        testing::ElementsAre(
            std::make_tuple(app_id, std::vector({protocol_handler.protocol}))));
  }
}

IN_PROC_BROWSER_TEST_P(UpdateProtocolHandlerApprovalCommandTest,
                       ProtocolHandlersRegisteredAndAllowed) {
  apps::ProtocolHandlerInfo protocol_handler;
  const std::string handler_url =
      std::string(kTestAppUrl.spec()) + "/testing=%s";
  protocol_handler.url = GURL(handler_url);
  protocol_handler.protocol = "web+test";
  web_app::AppId app_id = InstallWebAppWithProtocolHandlers({protocol_handler});

  base::test::TestFuture<void> future;
  provider().scheduler().UpdateProtocolHandlerUserApproval(
      app_id, protocol_handler.protocol, ApiApprovalState::kAllowed,
      future.GetCallback());
  EXPECT_TRUE(future.Wait());

  EXPECT_THAT(provider().registrar_unsafe().IsAllowedLaunchProtocol(
                  app_id, protocol_handler.protocol),
              testing::IsTrue());
  EXPECT_THAT(provider().registrar_unsafe().IsDisallowedLaunchProtocol(
                  app_id, protocol_handler.protocol),
              testing::IsFalse());

#if BUILDFLAG(IS_MAC)
  EXPECT_THAT(GetAppShimRegisteredProtocolHandlers(app_id),
              testing::ElementsAre(protocol_handler.protocol));
#endif

  if (AreProtocolsRegisteredWithOs()) {
    // Since they were already registered, no work needed to register them
    // again.
    EXPECT_THAT(
        OsIntegrationTestOverrideImpl::Get()->protocol_scheme_registrations(),
        testing::ElementsAre(
            std::make_tuple(app_id, std::vector({protocol_handler.protocol}))));
  }
}

IN_PROC_BROWSER_TEST_P(UpdateProtocolHandlerApprovalCommandTest,
                       ProtocolHandlersAllowedBackToBack) {
  apps::ProtocolHandlerInfo protocol_handler;
  const std::string handler_url =
      std::string(kTestAppUrl.spec()) + "/testing=%s";
  protocol_handler.url = GURL(handler_url);
  protocol_handler.protocol = "web+test";
  web_app::AppId app_id = InstallWebAppWithProtocolHandlers({protocol_handler});

  base::test::TestFuture<void> future_first;
  base::test::TestFuture<void> future_second;
  provider().scheduler().UpdateProtocolHandlerUserApproval(
      app_id, protocol_handler.protocol, ApiApprovalState::kAllowed,
      future_first.GetCallback());
  provider().scheduler().UpdateProtocolHandlerUserApproval(
      app_id, protocol_handler.protocol, ApiApprovalState::kAllowed,
      future_second.GetCallback());
  EXPECT_TRUE(future_first.Wait());
  EXPECT_TRUE(future_second.Wait());

  EXPECT_THAT(provider().registrar_unsafe().IsAllowedLaunchProtocol(
                  app_id, protocol_handler.protocol),
              testing::IsTrue());
  EXPECT_THAT(provider().registrar_unsafe().IsDisallowedLaunchProtocol(
                  app_id, protocol_handler.protocol),
              testing::IsFalse());

#if BUILDFLAG(IS_MAC)
  EXPECT_THAT(GetAppShimRegisteredProtocolHandlers(app_id),
              testing::ElementsAre(protocol_handler.protocol));
#endif

  if (AreProtocolsRegisteredWithOs()) {
    // Since they were already registered, no work needed to register them
    // again.
    EXPECT_THAT(
        OsIntegrationTestOverrideImpl::Get()->protocol_scheme_registrations(),
        testing::ElementsAre(
            std::make_tuple(app_id, std::vector({protocol_handler.protocol}))));
  }
}

IN_PROC_BROWSER_TEST_P(UpdateProtocolHandlerApprovalCommandTest,
                       ProtocolHandlersDisallowed) {
  apps::ProtocolHandlerInfo protocol_handler;
  const std::string handler_url =
      std::string(kTestAppUrl.spec()) + "/testing=%s";
  protocol_handler.url = GURL(handler_url);
  protocol_handler.protocol = "web+test";
  web_app::AppId app_id = InstallWebAppWithProtocolHandlers({protocol_handler});

  base::test::TestFuture<void> future;
  provider().scheduler().UpdateProtocolHandlerUserApproval(
      app_id, protocol_handler.protocol, ApiApprovalState::kDisallowed,
      future.GetCallback());
  EXPECT_TRUE(future.Wait());

  EXPECT_THAT(provider().registrar_unsafe().IsAllowedLaunchProtocol(
                  app_id, protocol_handler.protocol),
              testing::IsFalse());
  EXPECT_THAT(provider().registrar_unsafe().IsDisallowedLaunchProtocol(
                  app_id, protocol_handler.protocol),
              testing::IsTrue());

#if BUILDFLAG(IS_MAC)
  EXPECT_THAT(GetAppShimRegisteredProtocolHandlers(app_id), testing::IsEmpty());
#endif

  if (AreProtocolsRegisteredWithOs()) {
#if BUILDFLAG(IS_WIN)
    EXPECT_THAT(
        OsIntegrationTestOverrideImpl::Get()->protocol_scheme_registrations(),
        testing::ElementsAre(
            std::make_tuple(app_id, std::vector({protocol_handler.protocol})),
            std::make_tuple(app_id, std::vector<std::string>())));
#else
    EXPECT_THAT(
        OsIntegrationTestOverrideImpl::Get()->protocol_scheme_registrations(),
        testing::ElementsAre(
            std::make_tuple(app_id, std::vector({protocol_handler.protocol}))));
#endif  // BUILDFLAG(IS_WIN)
  }
}

IN_PROC_BROWSER_TEST_P(UpdateProtocolHandlerApprovalCommandTest,
                       ProtocolHandlersDisallowedBackToBack) {
  apps::ProtocolHandlerInfo protocol_handler;
  const std::string handler_url =
      std::string(kTestAppUrl.spec()) + "/testing=%s";
  protocol_handler.url = GURL(handler_url);
  protocol_handler.protocol = "web+test";
  web_app::AppId app_id = InstallWebAppWithProtocolHandlers({protocol_handler});

  base::test::TestFuture<void> future_first;
  base::test::TestFuture<void> future_second;
  provider().scheduler().UpdateProtocolHandlerUserApproval(
      app_id, protocol_handler.protocol, ApiApprovalState::kDisallowed,
      future_first.GetCallback());
  provider().scheduler().UpdateProtocolHandlerUserApproval(
      app_id, protocol_handler.protocol, ApiApprovalState::kDisallowed,
      future_second.GetCallback());
  EXPECT_TRUE(future_first.Wait());
  EXPECT_TRUE(future_second.Wait());

  EXPECT_THAT(provider().registrar_unsafe().IsAllowedLaunchProtocol(
                  app_id, protocol_handler.protocol),
              testing::IsFalse());
  EXPECT_THAT(provider().registrar_unsafe().IsDisallowedLaunchProtocol(
                  app_id, protocol_handler.protocol),
              testing::IsTrue());

#if BUILDFLAG(IS_MAC)
  EXPECT_THAT(GetAppShimRegisteredProtocolHandlers(app_id), testing::IsEmpty());
#endif

  if (AreProtocolsRegisteredWithOs()) {
#if BUILDFLAG(IS_WIN)
    EXPECT_THAT(
        OsIntegrationTestOverrideImpl::Get()->protocol_scheme_registrations(),
        testing::ElementsAre(
            std::make_tuple(app_id, std::vector({protocol_handler.protocol})),
            std::make_tuple(app_id, std::vector<std::string>())));
#else
    EXPECT_THAT(
        OsIntegrationTestOverrideImpl::Get()->protocol_scheme_registrations(),
        testing::ElementsAre(
            std::make_tuple(app_id, std::vector({protocol_handler.protocol}))));
#endif  // BUILDFLAG(IS_WIN)
  }
}

IN_PROC_BROWSER_TEST_P(UpdateProtocolHandlerApprovalCommandTest,
                       ProtocolHandlersAllowedThenDisallowed) {
  apps::ProtocolHandlerInfo protocol_handler;
  const std::string handler_url =
      std::string(kTestAppUrl.spec()) + "/testing=%s";
  protocol_handler.url = GURL(handler_url);
  protocol_handler.protocol = "web+test";
  web_app::AppId app_id = InstallWebAppWithProtocolHandlers({protocol_handler});

  {
    base::test::TestFuture<void> future;
    provider().scheduler().UpdateProtocolHandlerUserApproval(
        app_id, protocol_handler.protocol, ApiApprovalState::kAllowed,
        future.GetCallback());
    EXPECT_TRUE(future.Wait());
  }
  {
    base::test::TestFuture<void> future;
    provider().scheduler().UpdateProtocolHandlerUserApproval(
        app_id, protocol_handler.protocol, ApiApprovalState::kDisallowed,
        future.GetCallback());
    EXPECT_TRUE(future.Wait());
  }
  EXPECT_THAT(provider().registrar_unsafe().IsAllowedLaunchProtocol(
                  app_id, protocol_handler.protocol),
              testing::IsFalse());
  EXPECT_THAT(provider().registrar_unsafe().IsDisallowedLaunchProtocol(
                  app_id, protocol_handler.protocol),
              testing::IsTrue());

#if BUILDFLAG(IS_MAC)
  EXPECT_THAT(GetAppShimRegisteredProtocolHandlers(app_id), testing::IsEmpty());
#endif

  if (AreProtocolsRegisteredWithOs()) {
#if BUILDFLAG(IS_WIN)
    EXPECT_THAT(
        OsIntegrationTestOverrideImpl::Get()->protocol_scheme_registrations(),
        testing::ElementsAre(
            std::make_tuple(app_id, std::vector({protocol_handler.protocol})),
            std::make_tuple(app_id, std::vector<std::string>())));
#else
    EXPECT_THAT(
        OsIntegrationTestOverrideImpl::Get()->protocol_scheme_registrations(),
        testing::ElementsAre(
            std::make_tuple(app_id, std::vector({protocol_handler.protocol}))));
#endif  // BUILDFLAG(IS_WIN)
  }
}

IN_PROC_BROWSER_TEST_P(UpdateProtocolHandlerApprovalCommandTest,
                       ProtocolHandlersDisallowedThenAllowed) {
  apps::ProtocolHandlerInfo protocol_handler;
  const std::string handler_url =
      std::string(kTestAppUrl.spec()) + "/testing=%s";
  protocol_handler.url = GURL(handler_url);
  protocol_handler.protocol = "web+test";
  web_app::AppId app_id = InstallWebAppWithProtocolHandlers({protocol_handler});

  {
    base::test::TestFuture<void> future;
    provider().scheduler().UpdateProtocolHandlerUserApproval(
        app_id, protocol_handler.protocol, ApiApprovalState::kDisallowed,
        future.GetCallback());
    EXPECT_TRUE(future.Wait());
  }

  {
    base::test::TestFuture<void> future;
    provider().scheduler().UpdateProtocolHandlerUserApproval(
        app_id, protocol_handler.protocol, ApiApprovalState::kAllowed,
        future.GetCallback());
    EXPECT_TRUE(future.Wait());
  }
  EXPECT_THAT(provider().registrar_unsafe().IsAllowedLaunchProtocol(
                  app_id, protocol_handler.protocol),
              testing::IsTrue());
  EXPECT_THAT(provider().registrar_unsafe().IsDisallowedLaunchProtocol(
                  app_id, protocol_handler.protocol),
              testing::IsFalse());

#if BUILDFLAG(IS_MAC)
  EXPECT_THAT(GetAppShimRegisteredProtocolHandlers(app_id),
              testing::ElementsAre(protocol_handler.protocol));
#endif

  if (AreProtocolsRegisteredWithOs()) {
#if BUILDFLAG(IS_WIN)
    if (AreSubManagersExecuteEnabled()) {
      // The sub managers first add a protocol, then remove it on being
      // disallowed and then adds it again.
      EXPECT_THAT(
          OsIntegrationTestOverrideImpl::Get()->protocol_scheme_registrations(),
          testing::ElementsAre(
              std::make_tuple(app_id, std::vector({protocol_handler.protocol})),
              std::make_tuple(app_id, std::vector<std::string>()),
              std::make_tuple(app_id,
                              std::vector({protocol_handler.protocol}))));
    } else {
      // The old OS integration code first adds a protocol, and then does an
      // update with no approved protocols (hence an unregistration but no
      // registration). The last update call performs an unregistration and a
      // re-addition of the protocol, so there are four entries.
      EXPECT_THAT(
          OsIntegrationTestOverrideImpl::Get()->protocol_scheme_registrations(),
          testing::ElementsAre(
              std::make_tuple(app_id, std::vector({protocol_handler.protocol})),
              std::make_tuple(app_id, std::vector<std::string>()),
              std::make_tuple(app_id, std::vector<std::string>()),
              std::make_tuple(app_id,
                              std::vector({protocol_handler.protocol}))));
    }
#else
    EXPECT_THAT(
        OsIntegrationTestOverrideImpl::Get()->protocol_scheme_registrations(),
        testing::ElementsAre(
            std::make_tuple(app_id, std::vector({protocol_handler.protocol})),
            std::make_tuple(app_id, std::vector({protocol_handler.protocol}))));
#endif  // BUILDFLAG(IS_WIN)
  }
}

IN_PROC_BROWSER_TEST_P(UpdateProtocolHandlerApprovalCommandTest,
                       ProtocolHandlersDisallowedThenAsked) {
  apps::ProtocolHandlerInfo protocol_handler;
  const std::string handler_url =
      std::string(kTestAppUrl.spec()) + "/testing=%s";
  protocol_handler.url = GURL(handler_url);
  protocol_handler.protocol = "web+test";
  web_app::AppId app_id = InstallWebAppWithProtocolHandlers({protocol_handler});

  {
    base::test::TestFuture<void> future;
    provider().scheduler().UpdateProtocolHandlerUserApproval(
        app_id, protocol_handler.protocol, ApiApprovalState::kDisallowed,
        future.GetCallback());
    EXPECT_TRUE(future.Wait());
  }

  {
    base::test::TestFuture<void> future;
    provider().scheduler().UpdateProtocolHandlerUserApproval(
        app_id, protocol_handler.protocol, ApiApprovalState::kRequiresPrompt,
        future.GetCallback());
    EXPECT_TRUE(future.Wait());
  }

  EXPECT_THAT(provider().registrar_unsafe().IsAllowedLaunchProtocol(
                  app_id, protocol_handler.protocol),
              testing::IsFalse());
  EXPECT_THAT(provider().registrar_unsafe().IsDisallowedLaunchProtocol(
                  app_id, protocol_handler.protocol),
              testing::IsFalse());

#if BUILDFLAG(IS_MAC)
  EXPECT_THAT(GetAppShimRegisteredProtocolHandlers(app_id),
              testing::ElementsAre(protocol_handler.protocol));
#endif

  if (AreProtocolsRegisteredWithOs()) {
#if BUILDFLAG(IS_WIN)
    if (AreSubManagersExecuteEnabled()) {
      // The sub managers first add a protocol, then remove it on being
      // disallowed and then adds it again.
      EXPECT_THAT(
          OsIntegrationTestOverrideImpl::Get()->protocol_scheme_registrations(),
          testing::ElementsAre(
              std::make_tuple(app_id, std::vector({protocol_handler.protocol})),
              std::make_tuple(app_id, std::vector<std::string>()),
              std::make_tuple(app_id,
                              std::vector({protocol_handler.protocol}))));
    } else {
      // The old OS integration code first adds a protocol, and then does an
      // update with no approved protocols (hence an unregistration but no
      // registration). The last update call performs an unregistration and a
      // re-addition of the protocol, so there are four entries.
      EXPECT_THAT(
          OsIntegrationTestOverrideImpl::Get()->protocol_scheme_registrations(),
          testing::ElementsAre(
              std::make_tuple(app_id, std::vector({protocol_handler.protocol})),
              std::make_tuple(app_id, std::vector<std::string>()),
              std::make_tuple(app_id, std::vector<std::string>()),
              std::make_tuple(app_id,
                              std::vector({protocol_handler.protocol}))));
    }
#else
    EXPECT_THAT(
        OsIntegrationTestOverrideImpl::Get()->protocol_scheme_registrations(),
        testing::ElementsAre(
            std::make_tuple(app_id, std::vector({protocol_handler.protocol})),
            std::make_tuple(app_id, std::vector({protocol_handler.protocol}))));
#endif  // BUILDFLAG(IS_WIN)
  }
}

IN_PROC_BROWSER_TEST_P(UpdateProtocolHandlerApprovalCommandTest,
                       ProtocolHandlersAllowedThenAsked) {
  apps::ProtocolHandlerInfo protocol_handler;
  const std::string handler_url =
      std::string(kTestAppUrl.spec()) + "/testing=%s";
  protocol_handler.url = GURL(handler_url);
  protocol_handler.protocol = "web+test";
  web_app::AppId app_id = InstallWebAppWithProtocolHandlers({protocol_handler});

  {
    base::test::TestFuture<void> future;
    provider().scheduler().UpdateProtocolHandlerUserApproval(
        app_id, protocol_handler.protocol, ApiApprovalState::kAllowed,
        future.GetCallback());
    EXPECT_TRUE(future.Wait());
  }

  {
    base::test::TestFuture<void> future;
    provider().scheduler().UpdateProtocolHandlerUserApproval(
        app_id, protocol_handler.protocol, ApiApprovalState::kRequiresPrompt,
        future.GetCallback());
    EXPECT_TRUE(future.Wait());
  }

  EXPECT_THAT(provider().registrar_unsafe().IsAllowedLaunchProtocol(
                  app_id, protocol_handler.protocol),
              testing::IsFalse());
  EXPECT_THAT(provider().registrar_unsafe().IsDisallowedLaunchProtocol(
                  app_id, protocol_handler.protocol),
              testing::IsFalse());

#if BUILDFLAG(IS_MAC)
  EXPECT_THAT(GetAppShimRegisteredProtocolHandlers(app_id),
              testing::ElementsAre(protocol_handler.protocol));
#endif

  // They should be registered on first install and not modified on addition or
  // removal from the allowed list.
  if (AreProtocolsRegisteredWithOs()) {
    EXPECT_THAT(
        OsIntegrationTestOverrideImpl::Get()->protocol_scheme_registrations(),
        testing::ElementsAre(
            std::make_tuple(app_id, std::vector({protocol_handler.protocol}))));
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    UpdateProtocolHandlerApprovalCommandTest,
    ::testing::Values(OsIntegrationSubManagersState::kSaveStateToDB,
                      OsIntegrationSubManagersState::kSaveStateAndExecute,
                      OsIntegrationSubManagersState::kDisabled),
    test::GetOsIntegrationSubManagersTestName);

}  // namespace
}  // namespace web_app
