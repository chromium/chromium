// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/ui/web_applications/web_app_dialogs.h"
#include "chrome/browser/web_applications/external_install_options.h"
#include "chrome/browser/web_applications/externally_managed_app_manager.h"
#include "chrome/browser/web_applications/model/migration_behavior.h"
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
#include "components/permissions/permission_request_manager.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/features_generated.h"

namespace web_app {

namespace {

// Various install flows that can be triggered by the web applications system.
enum class WebAppInstallFlow {
  kUserInstall,
  kExternalInstall,
  kInstallFromInfo,
  kInstallLocally,
  kSyncInstall,
  kInstallApiCurrentDocument,
  kInstallApiManifest
};

// Verifies that install flows promote migration-suggested placeholders, while
// local install leaves these hidden apps unchanged.
class ReplaceMigrationSuggestedAppBrowserTest
    : public WebAppBrowserTestBase,
      public testing::WithParamInterface<WebAppInstallFlow> {
 public:
  ReplaceMigrationSuggestedAppBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {blink::features::kWebAppMigrationApi,
         blink::features::kWebAppInstallation},
        {});
  }
  ~ReplaceMigrationSuggestedAppBrowserTest() override = default;

  WebAppProvider& provider() {
    return *WebAppProvider::GetForTest(browser()->GetProfile());
  }

  webapps::AppId InstallSuggestedFromMigrationApp(
      const GURL& start_url,
      std::optional<webapps::ManifestId> manifest_id = std::nullopt) {
    auto web_app_info =
        WebAppInstallInfo::CreateWithStartUrlForTesting(start_url);
    if (manifest_id) {
      web_app_info->SetManifestIdAndStartUrl(*manifest_id, start_url);
    }
    web_app_info->scope = start_url.GetWithoutFilename();
    web_app_info->title = u"Test App";
    web_app_info->user_display_mode = mojom::UserDisplayMode::kStandalone;

    web_app_info->migration_sources.emplace_back(
        manifest_id.value_or(
            webapps::ManifestId(GURL(start_url.GetWithoutFilename().spec()))),
        MigrationBehavior::kSuggest);

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
      case WebAppInstallFlow::kInstallApiCurrentDocument:
      case WebAppInstallFlow::kInstallApiManifest:
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

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(ReplaceMigrationSuggestedAppBrowserTest,
                       PerInstallFlow) {
  const bool is_current_document_install =
      GetParam() == WebAppInstallFlow::kInstallApiCurrentDocument;
  const GURL install_url = embedded_https_test_server().GetURL(
      is_current_document_install ? "/banners/manifest_with_id_test_page.html"
                                  : "/banners/manifest_test_page.html");
  const GURL start_url =
      is_current_document_install
          ? embedded_https_test_server().GetURL("/banners/start")
          : install_url;
  const std::optional<webapps::ManifestId> explicit_manifest_id =
      is_current_document_install
          ? std::make_optional(webapps::ManifestId(
                embedded_https_test_server().GetURL("/some_id")))
          : std::nullopt;
  const webapps::ManifestId expected_manifest_id =
      explicit_manifest_id.value_or(
          GenerateManifestIdFromStartUrlOnly(start_url));
  const webapps::AppId app_id =
      InstallSuggestedFromMigrationApp(start_url, explicit_manifest_id);

  // Verify initial state.
  EXPECT_EQ(provider().registrar_unsafe().GetInstallState(app_id),
            proto::SUGGESTED_FROM_MIGRATION);

  WebAppInstallFlow app_install_flow = GetParam();
  switch (app_install_flow) {
    case WebAppInstallFlow::kUserInstall: {
      ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), install_url));
      base::test::TestFuture<const webapps::AppId&, webapps::InstallResultCode>
          install_future;
      provider().scheduler().FetchManifestAndInstall(
          webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON,
          browser()->tab_strip_model()->GetActiveWebContents()->GetWeakPtr(),
          base::BindOnce(test::TestAcceptDialogCallback),
          install_future.GetCallback(),
          FallbackBehavior::kAllowFallbackDataAlways);
      ASSERT_TRUE(install_future.Wait());
      provider().command_manager().AwaitAllCommandsCompleteForTesting();
      EXPECT_EQ(install_future.Get<webapps::InstallResultCode>(),
                webapps::InstallResultCode::kSuccessNewInstall);
      EXPECT_EQ(install_future.Get<webapps::AppId>(), app_id);
      break;
    }
    case WebAppInstallFlow::kExternalInstall: {
      ExternalInstallOptions options(install_url,
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
      sync_proto.set_start_url(start_url.spec());
      sync_proto.set_relative_manifest_id(
          RelativeManifestIdPath(expected_manifest_id));
      sync_proto.set_scope(start_url.GetWithoutFilename().spec());
      auto app = test::CreateWebAppFromSyncProto(sync_proto);
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
    case WebAppInstallFlow::kInstallApiCurrentDocument:
    case WebAppInstallFlow::kInstallApiManifest: {
      ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), install_url));
      content::WebContents* web_contents =
          browser()->tab_strip_model()->GetActiveWebContents();

      permissions::PermissionRequestManager::FromWebContents(web_contents)
          ->set_auto_response_for_test(permissions::PermissionRequestManager::
                                           AutoResponseType::ACCEPT_ALL);
      base::AutoReset<web_app::InstallDialogTestResponse> auto_accept_pwa =
          web_app::SetPwaInstallationAutoRespondForTesting(
              web_app::InstallDialogTestResponse::kAcceptAndLaunch);

      std::string install_script =
          "navigator.install()"
          ".then(result => { webInstallResult = result; })"
          ".catch(error => { webInstallError = error; });";
      if (app_install_flow == WebAppInstallFlow::kInstallApiManifest) {
        const GURL manifest_url =
            embedded_https_test_server().GetURL("/banners/manifest.json");
        install_script = content::JsReplace(
            "navigator.install({manifest: $1, manifestId: $2})"
            ".then(result => { webInstallResult = result; })"
            ".catch(error => { webInstallError = error; });",
            manifest_url.spec(), expected_manifest_id.value().spec());
      }
      ASSERT_TRUE(content::ExecJs(web_contents, install_script));

      ASSERT_TRUE(base::test::RunUntil([&]() {
        return content::EvalJs(web_contents,
                               "typeof webInstallResult !== 'undefined' || "
                               "typeof webInstallError !== 'undefined'")
            .ExtractBool();
      }));
      if (content::EvalJs(web_contents,
                          "typeof webInstallError !== 'undefined'")
              .ExtractBool()) {
        ADD_FAILURE() << content::EvalJs(
            web_contents,
            "webInstallError.name + ': ' + webInstallError.message");
      }
      EXPECT_TRUE(content::ExecJs(web_contents, "webInstallResult"));
      EXPECT_TRUE(content::ExecJs(web_contents,
                                  "typeof webInstallError === 'undefined'"));

      provider().command_manager().AwaitAllCommandsCompleteForTesting();
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
                    WebAppInstallFlow::kInstallApiCurrentDocument,
                    WebAppInstallFlow::kInstallApiManifest),
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
        case WebAppInstallFlow::kInstallApiCurrentDocument:
          return "InstallApiCurrentDocument";
        case WebAppInstallFlow::kInstallApiManifest:
          return "InstallApiManifest";
      }
    });

}  // namespace

}  // namespace web_app
