// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/web_apps/sub_apps_install_dialog_controller.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/commands/isolated_web_app_apply_update_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_test_update_server.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/key_distribution/test_utils.h"
#include "chrome/browser/web_applications/manifest_update_manager.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/browser/web_applications/web_app_callback_app_identity.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "components/web_package/test_support/signed_web_bundles/signing_keys.h"
#include "components/webapps/isolated_web_apps/types/iwa_version.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "services/network/public/mojom/clear_data_filter.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/switches.h"
#include "ui/gfx/geometry/size.h"

namespace web_app {
namespace {

using ::testing::Eq;

IsolatedWebAppBuilder CreateIwaBuilder(const std::string& version,
                                       const std::string& subapp_shortcuts_part,
                                       const std::string& manifest_url,
                                       const GURL& update_manifest_url) {
  return IsolatedWebAppBuilder(
             ManifestBuilder()
                 .SetName("Parent IWA")
                 .SetVersion(version)
                 .AddIcon("/icon.png", gfx::Size(256, 256), "image/png")
                 .SetUpdateManifestUrl(update_manifest_url)
                 .AddPermissionsPolicy(
                     network::mojom::PermissionsPolicyFeature::kSubApps,
                     /*self=*/true, /*origins=*/{}))
      .AddIconAsPng("/icon.png", CreateSquareIcon(256, SK_ColorBLUE))
      .AddHtml("/", "<h1>Parent IWA v" + version + "</h1>")
      .AddHtml("/subapp", R"(
            <!DOCTYPE html>
            <html>
              <head>
                <link rel="manifest" href=")" +
                              manifest_url + R"(">
                <title>Sub App v)" +
                              version + R"( Page</title>
              </head>
              <body><h1>Sub App v)" +
                              version + R"( Page</h1></body>
            </html>
          )")
      .AddResource(manifest_url,
                   R"(
            {
              "name": "Sub App",
              "version": ")" +
                       version + R"(",
              "start_url": "/subapp",
              "display": "standalone",
              "icons": [{
                "src": "/icon.png",
                "sizes": "256x256",
                "type": "image/png"
              }] )" + subapp_shortcuts_part +
                       R"(
            }
          )",
                   "application/manifest+json");
}

class IsolatedWebAppSubAppUpdateBrowserTest
    : public IsolatedWebAppBrowserTestHarness {
 public:
  IsolatedWebAppSubAppUpdateBrowserTest() {
    feature_list_.InitAndEnableFeature(blink::features::kSubApps);
  }

  webapps::AppId InstallSubAppAndWait(content::WebContents* iwa_contents,
                                      const std::string& install_url,
                                      const GURL& expected_sub_app_url) {
    auto dialog_override =
        SubAppsInstallDialogController::SetAutomaticActionForTesting(
            SubAppsInstallDialogController::DialogActionForTesting::kAccept);

    base::test::TestFuture<webapps::AppId> test_future;
    WebAppInstallManagerObserverAdapter observer(profile());
    observer.SetWebAppInstalledWithOsHooksDelegate(
        test_future.GetRepeatingCallback<const webapps::AppId&>());

    EXPECT_TRUE(content::ExecJs(iwa_contents, R"(
      navigator.subApps.add({
        ")" + install_url + R"(": {"installURL": ")" +
                                                  install_url + R"("}
      })
    )"));

    webapps::AppId sub_app_id =
        GenerateAppId(/*manifest_id_path=*/std::nullopt, expected_sub_app_url);

    EXPECT_TRUE(test_future.Wait());
    EXPECT_EQ(sub_app_id, test_future.Take());
    return sub_app_id;
  }

  void UpdateIwaAndWait(const web_package::SignedWebBundleId& bundle_id,
                        IsolatedWebAppBuilder builder) {
    iwa_test_update_server_.AddBundle(builder.BuildBundle(
        bundle_id, {web_package::test::GetDefaultEd25519KeyPair()}));

    base::test::TestFuture<IsolatedWebAppUpdateDiscoveryTask::CompletionStatus>
        update_future;
    UpdateDiscoveryTaskResultWaiter update_waiter(
        provider(),
        IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(bundle_id).app_id(),
        update_future.GetCallback());

    EXPECT_EQ(
        1ul, provider().isolated_web_app_update_manager().DiscoverUpdatesNow());
    EXPECT_TRUE(update_future.Wait());
    EXPECT_THAT(update_future.Take(),
                base::test::ValueIs(IsolatedWebAppUpdateDiscoveryTask::Success::
                                        kUpdateFoundAndSavedInDatabase));
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  IsolatedWebAppTestUpdateServer iwa_test_update_server_;
};

IN_PROC_BROWSER_TEST_F(IsolatedWebAppSubAppUpdateBrowserTest,
                       SubAppManifestUpdatesAfterIwaUpdate) {
  const web_package::SignedWebBundleId bundle_id =
      web_package::test::GetDefaultEd25519WebBundleId();
  IsolatedWebAppUrlInfo iwa_url_info =
      IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(bundle_id);
  const webapps::AppId iwa_app_id = iwa_url_info.app_id();

  test::KeyDistributionComponentBuilder(base::Version("1.0.0"))
      .AddToManagedAllowlist(bundle_id)
      .Build()
      .InjectComponentDataDirectly();

  GURL update_manifest_url =
      iwa_test_update_server_.GetUpdateManifestUrl(bundle_id);

  IsolatedWebAppBuilder iwa_v1 =
      CreateIwaBuilder("1.0.0", "", "/subapp.webmanifest", update_manifest_url);

  // Install the v1 app directly instead of via policy.
  iwa_v1
      .BuildBundle(bundle_id, {web_package::test::GetDefaultEd25519KeyPair()})
      ->InstallChecked(profile());

  Browser* iwa_browser = LaunchWebAppBrowserAndWait(iwa_app_id);
  content::WebContents* iwa_contents =
      iwa_browser->tab_strip_model()->GetActiveWebContents();

  // Navigate to / so that we are in the parent IWA context.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      iwa_browser, iwa_url_info.origin().GetURL().Resolve("/")));

  webapps::AppId sub_app_id =
      InstallSubAppAndWait(iwa_contents, "/subapp",
                           iwa_url_info.origin().GetURL().Resolve("/subapp"));

  EXPECT_EQ(provider().registrar_unsafe().GetAppShortName(sub_app_id),
            "Sub App");

  IsolatedWebAppBuilder iwa_v2 =
      CreateIwaBuilder("2.0.0", R"(,
              "shortcuts": [{
                "name": "Shortcut",
                "url": "/shortcut"
              }])",
                       "/subapp_v2.webmanifest", update_manifest_url);

  iwa_browser->window()->Close();

  UpdateIwaAndWait(bundle_id, std::move(iwa_v2));

  WebAppTestManifestUpdatedObserver manifest_observer(
      &provider().install_manager());
  manifest_observer.BeginListening({sub_app_id});

  // Launch the browser. It will load the updated HTML from the new bundle.
  LaunchWebAppBrowserAndWait(sub_app_id);

  manifest_observer.Wait();

  ASSERT_EQ(provider()
                .registrar_unsafe()
                .GetAppManifestUrl(sub_app_id)
                .ExtractFileName(),
            "subapp_v2.webmanifest");

  EXPECT_EQ(1u, provider()
                    .registrar_unsafe()
                    .GetAppShortcutsMenuItemInfos(sub_app_id)
                    .size());
}

}  // namespace
}  // namespace web_app
