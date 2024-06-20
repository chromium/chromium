// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/contains.h"
#include "base/files/file_util.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/chrome_features.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/web_applications/os_integration/mac/app_shim_registry.h"
#endif

namespace web_app {

enum class ApiApprovalState;

namespace {
const char16_t kAppName[] = u"Test App";

// A few tests have Windows specific assertions because Windows is the only
// OS where protocols are registered on the OS differently compared to
// other OSes where protocols are bundled into the shortcut
// registration/update/unregistration flow.
class UpdateProtocolHandlerApprovalCommandTest : public WebAppBrowserTestBase {
 public:
  const GURL kTestAppUrl = GURL("https://example.com");

  UpdateProtocolHandlerApprovalCommandTest() = default;
  ~UpdateProtocolHandlerApprovalCommandTest() override = default;

  void TearDownOnMainThread() override {
    EXPECT_TRUE(test::UninstallAllWebApps(profile()));
    WebAppBrowserTestBase::TearDownOnMainThread();
  }

  webapps::AppId InstallWebAppWithProtocolHandlers(
      const std::vector<apps::ProtocolHandlerInfo>& protocol_handlers) {
    std::unique_ptr<WebAppInstallInfo> info =
        WebAppInstallInfo::CreateWithStartUrlForTesting(GURL(kTestAppUrl));
    info->title = kAppName;
    info->user_display_mode = web_app::mojom::UserDisplayMode::kStandalone;
    info->protocol_handlers = protocol_handlers;
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
    if (!success)
      return webapps::AppId();
    EXPECT_EQ(result.Get<webapps::InstallResultCode>(),
              webapps::InstallResultCode::kSuccessNewInstall);
    return result.Get<webapps::AppId>();
  }

#if BUILDFLAG(IS_MAC)
  std::vector<std::string> GetAppShimRegisteredProtocolHandlers(
      const webapps::AppId& app_id) {
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
};

IN_PROC_BROWSER_TEST_F(UpdateProtocolHandlerApprovalCommandTest, Install) {
  apps::ProtocolHandlerInfo protocol_handler;
  const std::string handler_url =
      std::string(kTestAppUrl.spec()) + "/testing=%s";
  protocol_handler.url = GURL(handler_url);
  protocol_handler.protocol = "web+test";
  webapps::AppId app_id = InstallWebAppWithProtocolHandlers({protocol_handler});

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

IN_PROC_BROWSER_TEST_F(UpdateProtocolHandlerApprovalCommandTest,
                       ProtocolHandlersRegisteredAndAllowed) {
  apps::ProtocolHandlerInfo protocol_handler;
  const std::string handler_url =
      std::string(kTestAppUrl.spec()) + "/testing=%s";
  protocol_handler.url = GURL(handler_url);
  protocol_handler.protocol = "web+test";
  webapps::AppId app_id = InstallWebAppWithProtocolHandlers({protocol_handler});

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

IN_PROC_BROWSER_TEST_F(UpdateProtocolHandlerApprovalCommandTest,
                       ProtocolHandlersAllowedBackToBack) {
  apps::ProtocolHandlerInfo protocol_handler;
  const std::string handler_url =
      std::string(kTestAppUrl.spec()) + "/testing=%s";
  protocol_handler.url = GURL(handler_url);
  protocol_handler.protocol = "web+test";
  webapps::AppId app_id = InstallWebAppWithProtocolHandlers({protocol_handler});

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

IN_PROC_BROWSER_TEST_F(UpdateProtocolHandlerApprovalCommandTest,
                       ProtocolHandlersDisallowed) {
  apps::ProtocolHandlerInfo protocol_handler;
  const std::string handler_url =
      std::string(kTestAppUrl.spec()) + "/testing=%s";
  protocol_handler.url = GURL(handler_url);
  protocol_handler.protocol = "web+test";
  webapps::AppId app_id = InstallWebAppWithProtocolHandlers({protocol_handler});

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

IN_PROC_BROWSER_TEST_F(UpdateProtocolHandlerApprovalCommandTest,
                       ProtocolHandlersDisallowedBackToBack) {
  apps::ProtocolHandlerInfo protocol_handler;
  const std::string handler_url =
      std::string(kTestAppUrl.spec()) + "/testing=%s";
  protocol_handler.url = GURL(handler_url);
  protocol_handler.protocol = "web+test";
  webapps::AppId app_id = InstallWebAppWithProtocolHandlers({protocol_handler});

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

IN_PROC_BROWSER_TEST_F(UpdateProtocolHandlerApprovalCommandTest,
                       ProtocolHandlersAllowedThenDisallowed) {
  apps::ProtocolHandlerInfo protocol_handler;
  const std::string handler_url =
      std::string(kTestAppUrl.spec()) + "/testing=%s";
  protocol_handler.url = GURL(handler_url);
  protocol_handler.protocol = "web+test";
  webapps::AppId app_id = InstallWebAppWithProtocolHandlers({protocol_handler});

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

IN_PROC_BROWSER_TEST_F(UpdateProtocolHandlerApprovalCommandTest,
                       ProtocolHandlersDisallowedThenAllowed) {
  apps::ProtocolHandlerInfo protocol_handler;
  const std::string handler_url =
      std::string(kTestAppUrl.spec()) + "/testing=%s";
  protocol_handler.url = GURL(handler_url);
  protocol_handler.protocol = "web+test";
  webapps::AppId app_id = InstallWebAppWithProtocolHandlers({protocol_handler});

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
    // The sub managers first add a protocol, then remove it on being
    // disallowed and then adds it again.
    EXPECT_THAT(
        OsIntegrationTestOverrideImpl::Get()->protocol_scheme_registrations(),
        testing::ElementsAre(
            std::make_tuple(app_id, std::vector({protocol_handler.protocol})),
#if BUILDFLAG(IS_WIN)
            std::make_tuple(app_id, std::vector<std::string>()),
#endif  // BUILDFLAG(IS_WIN)
            std::make_tuple(app_id, std::vector({protocol_handler.protocol}))));
  }
}

IN_PROC_BROWSER_TEST_F(UpdateProtocolHandlerApprovalCommandTest,
                       ProtocolHandlersDisallowedThenAsked) {
  apps::ProtocolHandlerInfo protocol_handler;
  const std::string handler_url =
      std::string(kTestAppUrl.spec()) + "/testing=%s";
  protocol_handler.url = GURL(handler_url);
  protocol_handler.protocol = "web+test";
  webapps::AppId app_id = InstallWebAppWithProtocolHandlers({protocol_handler});

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
    EXPECT_THAT(
        OsIntegrationTestOverrideImpl::Get()->protocol_scheme_registrations(),
        testing::ElementsAre(
            std::make_tuple(app_id, std::vector({protocol_handler.protocol})),
#if BUILDFLAG(IS_WIN)
            std::make_tuple(app_id, std::vector<std::string>()),
#endif  // BUILDFLAG(IS_WIN)
            std::make_tuple(app_id, std::vector({protocol_handler.protocol}))));
  }
}

IN_PROC_BROWSER_TEST_F(UpdateProtocolHandlerApprovalCommandTest,
                       ProtocolHandlersAllowedThenAsked) {
  apps::ProtocolHandlerInfo protocol_handler;
  const std::string handler_url =
      std::string(kTestAppUrl.spec()) + "/testing=%s";
  protocol_handler.url = GURL(handler_url);
  protocol_handler.protocol = "web+test";
  webapps::AppId app_id = InstallWebAppWithProtocolHandlers({protocol_handler});

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

}  // namespace
}  // namespace web_app
