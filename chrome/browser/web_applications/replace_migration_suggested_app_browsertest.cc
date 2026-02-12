// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/web_applications/external_install_options.h"
#include "chrome/browser/web_applications/externally_managed_app_manager.h"
#include "chrome/browser/web_applications/proto/web_app_install_state.pb.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_params.h"
#include "chrome/browser/web_applications/web_app_proto_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace web_app {

namespace {

// Various install flows that can be triggered by the web applications system.
enum class WebAppInstallFlow {
  kUserInstall,
  kExternalInstall,
  kInstallFromInfo,
  kInstallLocally,
  kSyncInstall,
  kInstallApi
};

// Test that verifies that any install flow triggered by the web_applications
// system for an app suggested for migration has the authority to overwrite that
// app and change its install state, except for locally installing an app, which
// should be exiting early.
class ReplaceMigrationSuggestedAppBrowserTest
    : public WebAppBrowserTestBase,
      public testing::WithParamInterface<WebAppInstallFlow> {
 public:
  ReplaceMigrationSuggestedAppBrowserTest() = default;
  ~ReplaceMigrationSuggestedAppBrowserTest() override = default;

  WebAppProvider& provider() {
    return *WebAppProvider::GetForTest(browser()->profile());
  }

  webapps::AppId InstallSuggestedFromMigrationApp(const GURL& start_url) {
    auto web_app_info =
        WebAppInstallInfo::CreateWithStartUrlForTesting(start_url);
    web_app_info->scope = start_url.GetWithoutFilename();
    web_app_info->title = u"Test App";
    web_app_info->user_display_mode = mojom::UserDisplayMode::kStandalone;

    web_app::proto::WebAppMigrationSource source;
    source.set_manifest_id(start_url.GetWithoutFilename().spec());
    web_app_info->migration_sources.push_back(std::move(source));

    base::test::TestFuture<const webapps::AppId&, webapps::InstallResultCode>
        install_future;
    WebAppInstallParams params;
    params.install_state = proto::InstallState::SUGGESTED_FROM_MIGRATION;
    params.add_to_applications_menu = false;
    params.add_to_desktop = false;
    params.add_to_quick_launch_bar = false;
    provider().scheduler().InstallFromInfoWithParams(
        std::move(web_app_info), /*overwrite_existing_manifest_fields=*/false,
        webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON,
        install_future.GetCallback(), params);
    EXPECT_TRUE(install_future.Wait());
    EXPECT_EQ(install_future.Get<webapps::InstallResultCode>(),
              webapps::InstallResultCode::kSuccessNewInstall);
    return install_future.Get<webapps::AppId>();
  }

  proto::InstallState GetExpectedInstallStatePerWebAppInstallFlow() {
    switch (GetParam()) {
      case WebAppInstallFlow::kUserInstall:
      case WebAppInstallFlow::kExternalInstall:
      case WebAppInstallFlow::kInstallFromInfo:
      case WebAppInstallFlow::kInstallApi:
        return proto::InstallState::INSTALLED_WITH_OS_INTEGRATION;
      case WebAppInstallFlow::kInstallLocally:
        // This flow is only supported for apps that are surfaced to the user,
        // and doesn't do anything for apps suggested from migration.
        return proto::InstallState::SUGGESTED_FROM_MIGRATION;
      case WebAppInstallFlow::kSyncInstall:
#if BUILDFLAG(IS_CHROMEOS)
        return proto::InstallState::INSTALLED_WITH_OS_INTEGRATION;
#else
        return proto::InstallState::SUGGESTED_FROM_ANOTHER_DEVICE;
#endif  // BUILDFLAG(IS_CHROMEOS)
    }
  }
};

IN_PROC_BROWSER_TEST_P(ReplaceMigrationSuggestedAppBrowserTest,
                       PerInstallFlow) {
  const GURL start_url =
      https_server()->GetURL("/banners/manifest_test_page.html");
  const webapps::AppId app_id = InstallSuggestedFromMigrationApp(start_url);

  // Verify initial state.
  EXPECT_EQ(provider().registrar_unsafe().GetInstallState(app_id),
            proto::SUGGESTED_FROM_MIGRATION);

  WebAppInstallFlow app_install_flow = GetParam();
  switch (app_install_flow) {
    case WebAppInstallFlow::kUserInstall: {
      ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), start_url));
      base::test::TestFuture<const webapps::AppId&, webapps::InstallResultCode>
          install_future;
      provider().scheduler().FetchManifestAndInstall(
          webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON,
          browser()->tab_strip_model()->GetActiveWebContents()->GetWeakPtr(),
          base::BindOnce(test::TestAcceptDialogCallback),
          install_future.GetCallback(),
          FallbackBehavior::kAllowFallbackDataAlways);
      ASSERT_TRUE(install_future.Wait());
      EXPECT_EQ(install_future.Get<webapps::InstallResultCode>(),
                webapps::InstallResultCode::kSuccessNewInstall);
      EXPECT_EQ(install_future.Get<webapps::AppId>(), app_id);
      break;
    }
    case WebAppInstallFlow::kExternalInstall: {
      ExternalInstallOptions options(start_url,
                                     mojom::UserDisplayMode::kStandalone,
                                     ExternalInstallSource::kExternalPolicy);
      base::test::TestFuture<ExternallyManagedAppManagerInstallResult>
          install_future;
      provider().scheduler().InstallExternallyManagedApp(
          options, /*installed_placeholder_app_id=*/std::nullopt,
          install_future.GetCallback());
      ASSERT_TRUE(install_future.Wait());
      EXPECT_EQ(install_future.Get().code,
                webapps::InstallResultCode::kSuccessNewInstall);
      EXPECT_EQ(install_future.Get().app_id, app_id);
      break;
    }
    case WebAppInstallFlow::kInstallFromInfo: {
      auto web_app_info =
          WebAppInstallInfo::CreateWithStartUrlForTesting(start_url);
      web_app_info->scope = start_url.GetWithoutFilename();
      web_app_info->title = u"Test App";
      web_app_info->user_display_mode = mojom::UserDisplayMode::kStandalone;

      base::test::TestFuture<const webapps::AppId&, webapps::InstallResultCode>
          install_future;
      WebAppInstallParams params;
      // Default install state is INSTALLED_WITH_OS_INTEGRATION.
      provider().scheduler().InstallFromInfoWithParams(
          std::move(web_app_info), /*overwrite_existing_manifest_fields=*/true,
          webapps::WebappInstallSource::ARC, install_future.GetCallback(),
          params);
      ASSERT_TRUE(install_future.Wait());
      EXPECT_EQ(install_future.Get<webapps::InstallResultCode>(),
                webapps::InstallResultCode::kSuccessNewInstall);
      EXPECT_EQ(install_future.Get<webapps::AppId>(), app_id);
      break;
    }
    case WebAppInstallFlow::kInstallLocally: {
      base::test::TestFuture<void> install_future;
      provider().scheduler().InstallAppLocally(app_id,
                                               install_future.GetCallback());
      ASSERT_TRUE(install_future.Wait());
      break;
    }
    case WebAppInstallFlow::kSyncInstall: {
      // Create a web app for syncing that is similar to the already installed
      // one.
      sync_pb::WebAppSpecifics sync_proto;
      webapps::ManifestId manifest_id =
          GenerateManifestIdFromStartUrlOnly(start_url);
      sync_proto.set_start_url(start_url.spec());
      sync_proto.set_relative_manifest_id(RelativeManifestIdPath(manifest_id));
      sync_proto.set_scope(start_url.GetWithoutFilename().spec());
      auto app = test::CreateWebAppFromSyncProto(std::move(sync_proto));
      app->SetName("Test App");
      app->SetUserDisplayMode(mojom::UserDisplayMode::kStandalone);
      app->SetInstallState(
          proto::InstallState::INSTALLED_WITHOUT_OS_INTEGRATION);
      app->SetDisplayMode(blink::mojom::DisplayMode::kStandalone);
      proto::os_state::WebAppOsIntegration os_state;
      app->SetCurrentOsIntegrationStates(os_state);
      app->SetIsFromSyncAndPendingInstallation(
          /*is_from_sync_and_pending_installation=*/true);

      base::test::TestFuture<const webapps::AppId&, webapps::InstallResultCode>
          install_future;
      provider().scheduler().InstallFromSync(*app,
                                             install_future.GetCallback());
      ASSERT_TRUE(install_future.Wait());
      EXPECT_EQ(install_future.Get<webapps::InstallResultCode>(),
                webapps::InstallResultCode::kSuccessNewInstall);
      EXPECT_EQ(install_future.Get<webapps::AppId>(), app_id);
      break;
    }
    case WebAppInstallFlow::kInstallApi: {
      ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), start_url));
      base::test::TestFuture<const webapps::AppId&, webapps::InstallResultCode>
          install_future;
      provider().scheduler().InstallAppFromUrl(
          start_url, GenerateManifestIdFromStartUrlOnly(start_url),
          browser()->tab_strip_model()->GetActiveWebContents()->GetWeakPtr(),
          start_url, base::BindOnce(test::TestAcceptDialogCallback),
          install_future.GetCallback());
      ASSERT_TRUE(install_future.Wait());
      EXPECT_EQ(install_future.Get<webapps::InstallResultCode>(),
                webapps::InstallResultCode::kSuccessNewInstall);
      EXPECT_EQ(install_future.Get<webapps::AppId>(), app_id);
      break;
    }
  }

  // Verify final state post installation.
  auto install_state = provider().registrar_unsafe().GetInstallState(app_id);
  EXPECT_EQ(GetExpectedInstallStatePerWebAppInstallFlow(), install_state);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ReplaceMigrationSuggestedAppBrowserTest,
    testing::Values(WebAppInstallFlow::kUserInstall,
                    WebAppInstallFlow::kExternalInstall,
                    WebAppInstallFlow::kInstallFromInfo,
                    WebAppInstallFlow::kInstallLocally,
                    WebAppInstallFlow::kSyncInstall,
                    WebAppInstallFlow::kInstallApi),
    [](const testing::TestParamInfo<WebAppInstallFlow>& info) {
      switch (info.param) {
        case WebAppInstallFlow::kUserInstall:
          return "UserInstall";
        case WebAppInstallFlow::kExternalInstall:
          return "ExternalInstall";
        case WebAppInstallFlow::kInstallFromInfo:
          return "InstallFromInfo";
        case WebAppInstallFlow::kInstallLocally:
          return "InstallLocally";
        case WebAppInstallFlow::kSyncInstall:
          return "SyncInstall";
        case WebAppInstallFlow::kInstallApi:
          return "InstallApi";
      }
    });

}  // namespace

}  // namespace web_app
