// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/toolbar/app_menu_control.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_menu_button.h"
#include "chrome/browser/ui/views/web_apps/sub_apps_install_dialog_controller.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_menu_model.h"
#include "chrome/browser/web_applications/isolated_web_apps/commands/isolated_web_app_apply_update_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_test_update_server.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/key_distribution/test_utils.h"
#include "chrome/browser/web_applications/manifest_update_manager.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_page_waiter.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
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
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/test/dialog_test.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/any_widget_observer.h"

namespace web_app {
namespace {

testing::Matcher<WebAppShortcutsMenuItemInfo> Shortcut(std::u16string_view name,
                                                       const GURL& url) {
  return testing::AllOf(
      testing::Field("name", &WebAppShortcutsMenuItemInfo::name,
                     testing::Eq(name)),
      testing::Field("url", &WebAppShortcutsMenuItemInfo::url,
                     testing::Eq(url)));
}

IsolatedWebAppBuilder CreateIwaBuilder(
    std::string_view version,
    std::string_view sub_app_name,
    std::string_view manifest_url,
    std::optional<GURL> update_manifest_url = std::nullopt,
    std::string_view subapp_shortcuts = "") {
  std::string manifest_extra;
  if (!subapp_shortcuts.empty()) {
    base::StrAppend(&manifest_extra, {", \"shortcuts\": ", subapp_shortcuts});
  }

  ManifestBuilder manifest_builder =
      ManifestBuilder()
          .SetName("Parent IWA")
          .SetVersion(version)
          .AddIcon("/icon.png", gfx::Size(256, 256), "image/png")
          .AddPermissionsPolicy(
              network::mojom::PermissionsPolicyFeature::kSubApps,
              /*self=*/true, /*origins=*/{});
  if (update_manifest_url) {
    manifest_builder.SetUpdateManifestUrl(*update_manifest_url);
  }

  return IsolatedWebAppBuilder(manifest_builder)
      .AddIconAsPng("/icon.png", CreateSquareIcon(256, SK_ColorBLUE))
      .AddHtml("/",
               base::ReplaceStringPlaceholders("<h1>Parent IWA v$1</h1>",
                                               {std::string(version)}, nullptr))
      .AddHtml("/subapp/index.html",
               base::ReplaceStringPlaceholders(
                   R"(
            <!DOCTYPE html>
            <html>
              <head>
                <link rel="manifest" href="$1">
                <title>Sub App v$2 Page</title>
              </head>
              <body><h1>Sub App v$2 Page</h1></body>
            </html>
          )",
                   {std::string(manifest_url), std::string(version)}, nullptr))
      .AddResource(
          manifest_url,
          base::ReplaceStringPlaceholders(
              R"(
            {
              "name": "$1",
              "version": "$2",
              "start_url": "/subapp/index.html",
              "display": "standalone",
              "icons": [{
                "src": "/icon.png",
                "sizes": "256x256",
                "type": "image/png"
              }] $3
            }
          )",
              {std::string(sub_app_name), std::string(version), manifest_extra},
              nullptr),
          "application/manifest+json");
}

class SubAppUpdateBrowserTest : public IsolatedWebAppBrowserTestHarness {
 public:
  SubAppUpdateBrowserTest() {
    feature_list_.InitAndEnableFeature(blink::features::kSubApps);
  }

  webapps::AppId InstallIwaV1AndWait(
      const web_package::SignedWebBundleId& bundle_id) {
    GURL update_manifest_url =
        iwa_test_update_server_.GetUpdateManifestUrl(bundle_id);

    IsolatedWebAppBuilder iwa_v1 = CreateIwaBuilder(
        "1.0.0", "Sub App", "/subapp.webmanifest", update_manifest_url);

    iwa_v1
        .BuildBundle(bundle_id, {web_package::test::GetDefaultEd25519KeyPair()})
        ->InstallChecked(profile());

    IsolatedWebAppUrlInfo iwa_url_info =
        IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(bundle_id);
    return iwa_url_info.app_id();
  }

  void UpdateIwaToV2AndWait(const web_package::SignedWebBundleId& bundle_id,
                            std::string_view sub_app_name,
                            std::string_view subapp_shortcuts) {
    GURL update_manifest_url =
        iwa_test_update_server_.GetUpdateManifestUrl(bundle_id);

    IsolatedWebAppBuilder iwa_v2 =
        CreateIwaBuilder("2.0.0", sub_app_name, "/subapp_v2.webmanifest",
                         update_manifest_url, subapp_shortcuts);

    UpdateIwaAndWait(bundle_id, iwa_v2);
  }

  webapps::AppId InstallSubAppAndWait(content::WebContents* iwa_contents,
                                      std::string_view install_url,
                                      const GURL& expected_sub_app_url) {
    auto dialog_override =
        SubAppsInstallDialogController::SetAutomaticActionForTesting(
            SubAppsInstallDialogController::DialogActionForTesting::kAccept);

    base::test::TestFuture<webapps::AppId> test_future;
    WebAppInstallManagerObserverAdapter observer(profile());
    observer.SetWebAppInstalledWithOsHooksDelegate(
        test_future.GetRepeatingCallback<const webapps::AppId&>());

    EXPECT_TRUE(content::ExecJs(iwa_contents,
                                base::ReplaceStringPlaceholders(
                                    R"(
              navigator.subApps.add({
                "$1": {"installURL": "$1"}
              })
            )",
                                    {std::string(install_url)}, nullptr)));

    webapps::AppId sub_app_id =
        GenerateAppId(/*manifest_id_path=*/std::nullopt, expected_sub_app_url);

    EXPECT_EQ(sub_app_id, test_future.Take());
    return sub_app_id;
  }

  void UpdateIwaAndWait(const web_package::SignedWebBundleId& bundle_id,
                        IsolatedWebAppBuilder& builder) {
    iwa_test_update_server_.AddBundle(builder.BuildBundle(
        bundle_id, {web_package::test::GetDefaultEd25519KeyPair()}));

    base::test::TestFuture<
        IsolatedWebAppUpdateCheckAndPrepareTask::CompletionStatus>
        update_future;
    UpdateDiscoveryTaskResultWaiter update_waiter(
        provider(),
        IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(bundle_id).app_id(),
        update_future.GetCallback());

    EXPECT_EQ(
        1ul, provider().isolated_web_app_update_manager().DiscoverUpdatesNow());
    EXPECT_THAT(
        update_future.Take(),
        base::test::ValueIs(IsolatedWebAppUpdateCheckAndPrepareTask::Success::
                                kUpdateFoundAndSavedInDatabase));
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  IsolatedWebAppTestUpdateServer iwa_test_update_server_;
};

IN_PROC_BROWSER_TEST_F(SubAppUpdateBrowserTest,
                       SubAppManifestUpdatesAfterIwaUpdate) {
  const web_package::SignedWebBundleId bundle_id =
      web_package::test::GetDefaultEd25519WebBundleId();

  const webapps::AppId iwa_app_id = InstallIwaV1AndWait(bundle_id);

  Browser* iwa_browser = LaunchWebAppBrowserAndWait(iwa_app_id);
  ASSERT_NE(iwa_browser, nullptr);

  IsolatedWebAppUrlInfo iwa_url_info =
      IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(bundle_id);
  webapps::AppId sub_app_id = InstallSubAppAndWait(
      iwa_browser->tab_strip_model()->GetActiveWebContents(),
      "/subapp/index.html",
      iwa_url_info.origin().GetURL().Resolve("/subapp/index.html"));

  EXPECT_EQ(provider().registrar_unsafe().GetAppShortName(sub_app_id),
            "Sub App");
  EXPECT_THAT(
      provider().registrar_unsafe().GetAppShortcutsMenuItemInfos(sub_app_id),
      testing::IsEmpty());

  iwa_browser->GetWindow()->Close();

  UpdateIwaToV2AndWait(bundle_id, "Sub App", R"([{
        "name": "Shortcut",
        "url": "subapp/shortcut"
      }])");

  WebAppTestManifestUpdatedObserver manifest_observer(
      &provider().install_manager());
  manifest_observer.BeginListening({sub_app_id});

  // Launch the browser. It will load the updated HTML from the new bundle.
  Browser* sub_app_browser = LaunchWebAppBrowserAndWait(sub_app_id);
  ASSERT_NE(sub_app_browser, nullptr);

  manifest_observer.Wait();

  ASSERT_EQ(provider()
                .registrar_unsafe()
                .GetAppManifestUrl(sub_app_id)
                .ExtractFileName(),
            "subapp_v2.webmanifest");

  EXPECT_THAT(
      provider().registrar_unsafe().GetAppShortcutsMenuItemInfos(sub_app_id),
      testing::ElementsAre(Shortcut(
          u"Shortcut",
          iwa_url_info.origin().GetURL().Resolve("/subapp/shortcut"))));
}

// In PWA, updating security fields: icon>10% byte by byte comparison/title,
// causes title bar showing "App update available" button, when clicked,
// user prompted with dialog to confirm that update has succeeded. However,
// sub apps are IWA only API that updates silently even with security
// fields being changed.
IN_PROC_BROWSER_TEST_F(SubAppUpdateBrowserTest,
                       SubAppManifestUpdatesAfterIwaUpdateWithSecurityFields) {
  const web_package::SignedWebBundleId bundle_id =
      web_package::test::GetDefaultEd25519WebBundleId();

  const webapps::AppId iwa_app_id = InstallIwaV1AndWait(bundle_id);

  Browser* iwa_browser = LaunchWebAppBrowserAndWait(iwa_app_id);
  ASSERT_NE(iwa_browser, nullptr);

  IsolatedWebAppUrlInfo iwa_url_info =
      IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(bundle_id);
  webapps::AppId sub_app_id = InstallSubAppAndWait(
      iwa_browser->tab_strip_model()->GetActiveWebContents(),
      "/subapp/index.html",
      iwa_url_info.origin().GetURL().Resolve("/subapp/index.html"));

  EXPECT_EQ(provider().registrar_unsafe().GetAppShortName(sub_app_id),
            "Sub App");
  EXPECT_THAT(
      provider().registrar_unsafe().GetAppShortcutsMenuItemInfos(sub_app_id),
      testing::IsEmpty());

  iwa_browser->GetWindow()->Close();

  // Use different name of sub app to check that the update
  // for title/icon is still applied automatically without user action.
  UpdateIwaToV2AndWait(bundle_id, "Sub App Updated", R"([{
        "name": "Shortcut",
        "url": "subapp/shortcut"
      }])");

  WebAppTestManifestUpdatedObserver manifest_observer(
      &provider().install_manager());
  manifest_observer.BeginListening({sub_app_id});

  Browser* sub_app_browser = LaunchWebAppBrowserAndWait(sub_app_id);
  ASSERT_NE(sub_app_browser, nullptr);
  manifest_observer.Wait();
  provider().command_manager().AwaitAllCommandsCompleteForTesting();

  EXPECT_EQ("Sub App Updated",
            provider().registrar_unsafe().GetAppShortName(sub_app_id));
  EXPECT_THAT(
      provider().registrar_unsafe().GetAppShortcutsMenuItemInfos(sub_app_id),
      testing::ElementsAre(Shortcut(
          u"Shortcut",
          iwa_url_info.origin().GetURL().Resolve("/subapp/shortcut"))));
}

// Sub apps must never have overlapping scopes with each other,
// this is enforced on installation. This test check that it is also enforced
// on update.
IN_PROC_BROWSER_TEST_F(SubAppUpdateBrowserTest, SubAppScopeOverlap) {
  const web_package::SignedWebBundleId bundle_id =
      web_package::test::GetDefaultEd25519WebBundleId();

  GURL update_manifest_url =
      iwa_test_update_server_.GetUpdateManifestUrl(bundle_id);

  ManifestBuilder parent_manifest =
      ManifestBuilder()
          .SetName("Parent IWA")
          .SetVersion("1.0.0")
          .AddIcon("/icon.png", gfx::Size(256, 256), "image/png")
          .AddPermissionsPolicy(
              network::mojom::PermissionsPolicyFeature::kSubApps,
              /*self=*/true, /*origins=*/{});
  parent_manifest.SetUpdateManifestUrl(update_manifest_url);

  IsolatedWebAppBuilder iwa_v1(parent_manifest);
  iwa_v1.AddIconAsPng("/icon.png", CreateSquareIcon(256, SK_ColorBLUE));
  iwa_v1.AddHtml("/", "<h1>Parent IWA v1</h1>");

  // Sub app 1: scope /sub1/.
  iwa_v1.AddHtml("/sub1/index.html", R"(
    <!DOCTYPE html>
    <html>
      <head>
        <link rel="manifest" href="/sub1/manifest.json">
        <title>Sub App 1</title>
      </head>
      <body><h1>Sub App 1</h1></body>
    </html>
  )");
  iwa_v1.AddResource("/sub1/manifest.json", R"(
    {
      "name": "Sub App 1",
      "version": "1.0.0",
      "id": "/sub1/index.html",
      "start_url": "/sub1/index.html",
      "scope": "/sub1/",
      "icons": [{
        "src": "/icon.png",
        "sizes": "256x256",
        "type": "image/png"
      }]
    }
  )",
                     "application/manifest+json");

  // Sub app 2: scope /sub2/.
  iwa_v1.AddHtml("/sub2/index.html", R"(
    <!DOCTYPE html>
    <html>
      <head>
        <link rel="manifest" href="/sub2/manifest.json">
        <title>Sub App 2</title>
      </head>
      <body><h1>Sub App 2</h1></body>
    </html>
  )");
  iwa_v1.AddResource("/sub2/manifest.json", R"(
    {
      "name": "Sub App 2",
      "version": "1.0.0",
      "id": "/sub2/index.html",
      "start_url": "/sub2/index.html",
      "scope": "/sub2/",
      "icons": [{
        "src": "/icon.png",
        "sizes": "256x256",
        "type": "image/png"
      }]
    }
  )",
                     "application/manifest+json");

  iwa_v1
      .BuildBundle(bundle_id, {web_package::test::GetDefaultEd25519KeyPair()})
      ->InstallChecked(profile());

  IsolatedWebAppUrlInfo iwa_url_info =
      IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(bundle_id);
  webapps::AppId iwa_app_id = iwa_url_info.app_id();

  Browser* iwa_browser = LaunchWebAppBrowserAndWait(iwa_app_id);
  ASSERT_NE(iwa_browser, nullptr);

  webapps::AppId sub_app_1_id = InstallSubAppAndWait(
      iwa_browser->tab_strip_model()->GetActiveWebContents(),
      "/sub1/index.html",
      iwa_url_info.origin().GetURL().Resolve("/sub1/index.html"));

  webapps::AppId sub_app_2_id = InstallSubAppAndWait(
      iwa_browser->tab_strip_model()->GetActiveWebContents(),
      "/sub2/index.html",
      iwa_url_info.origin().GetURL().Resolve("/sub2/index.html"));

  EXPECT_EQ(provider().registrar_unsafe().GetAppShortName(sub_app_1_id),
            "Sub App 1");
  EXPECT_EQ(provider().registrar_unsafe().GetAppScope(sub_app_1_id),
            iwa_url_info.origin().GetURL().Resolve("/sub1/"));
  EXPECT_EQ(provider().registrar_unsafe().GetAppShortName(sub_app_2_id),
            "Sub App 2");
  EXPECT_EQ(provider().registrar_unsafe().GetAppScope(sub_app_2_id),
            iwa_url_info.origin().GetURL().Resolve("/sub2/"));

  iwa_browser->GetWindow()->Close();

  // Update parent IWA that includes the sub apps.
  ManifestBuilder parent_manifest_v2 =
      ManifestBuilder()
          .SetName("Parent IWA")
          .SetVersion("2.0.0")
          .AddIcon("/icon.png", gfx::Size(256, 256), "image/png")
          .AddPermissionsPolicy(
              network::mojom::PermissionsPolicyFeature::kSubApps,
              /*self=*/true, /*origins=*/{});
  parent_manifest_v2.SetUpdateManifestUrl(update_manifest_url);

  IsolatedWebAppBuilder iwa_v2(parent_manifest_v2);
  iwa_v2.AddIconAsPng("/icon.png", CreateSquareIcon(256, SK_ColorBLUE));
  iwa_v2.AddHtml("/", "<h1>Parent IWA v2</h1>");

  // Sub app 1: change name to check if update works.
  iwa_v2.AddHtml("/sub1/index.html", R"(
    <!DOCTYPE html>
    <html>
      <head>
        <link rel="manifest" href="/sub1/manifest.json">
        <title>Sub App 1</title>
      </head>
      <body><h1>Sub App 1</h1></body>
    </html>
  )");
  iwa_v2.AddResource("/sub1/manifest.json", R"(
    {
      "name": "Sub App 1 Updated",
      "version": "2.0.0",
      "id": "/sub1/index.html",
      "start_url": "/sub1/index.html",
      "scope": "/sub1/",
      "icons": [{
        "src": "/icon.png",
        "sizes": "256x256",
        "type": "image/png"
      }]
    }
  )",
                     "application/manifest+json");

  // Sub app 2: scope overlaps with Sub App 1, also change name to check if
  // update is applied. We must keep /sub2/index.html defined in V2 so the
  // initial launch (using the old start URL) works.
  iwa_v2.AddHtml("/sub2/index.html", R"(
    <!DOCTYPE html>
    <html>
      <head>
        <link rel="manifest" href="/sub2/manifest.json">
        <title>Sub App 2</title>
      </head>
      <body><h1>Sub App 2</h1></body>
    </html>
  )");
  // Define the new start URL inside /sub1/
  iwa_v2.AddHtml("/sub1/sub2_index.html", R"(
    <!DOCTYPE html>
    <html>
      <head>
        <link rel="manifest" href="/sub2/manifest.json">
        <title>Sub App 2</title>
      </head>
      <body><h1>Sub App 2</h1></body>
    </html>
  )");
  iwa_v2.AddResource("/sub2/manifest.json", R"(
    {
      "name": "Sub App 2 Updated",
      "version": "2.0.0",
      "id": "/sub2/index.html",
      "start_url": "/sub1/sub2_index.html",
      "scope": "/sub1/",
      "icons": [{
        "src": "/icon.png",
        "sizes": "256x256",
        "type": "image/png"
      }]
    }
  )",
                     "application/manifest+json");

  UpdateIwaAndWait(bundle_id, iwa_v2);

  // Now launch Sub App 2 to trigger its manifest update check.
  // We expect the update to fail due to scope overlap validation.
  Browser* sub_app_2_browser = LaunchWebAppBrowserAndWait(sub_app_2_id);
  ASSERT_NE(sub_app_2_browser, nullptr);

  EXPECT_TRUE(
      test::WebAppPageWaiter(
          sub_app_2_browser->tab_strip_model()->GetActiveWebContents())
          .ExpectManifest(
              provider().registrar_unsafe().GetAppManifestId(sub_app_2_id))
          .WaitAndFlushCommands());

  // The name must not be updated.
  EXPECT_EQ(provider().registrar_unsafe().GetAppShortName(sub_app_2_id),
            "Sub App 2");
  // The scope must remain /sub2/ (not updated).
  EXPECT_EQ(provider().registrar_unsafe().GetAppScope(sub_app_2_id),
            iwa_url_info.origin().GetURL().Resolve("/sub2/"));
}

// Parent scope must not be in scope of a sub app.
// e.g. illegal state when parent.scope == "/scope/some". sub_app.scope =
// "/scope".
IN_PROC_BROWSER_TEST_F(SubAppUpdateBrowserTest, SubAppParentInScope) {
  const web_package::SignedWebBundleId bundle_id =
      web_package::test::GetDefaultEd25519WebBundleId();

  GURL update_manifest_url =
      iwa_test_update_server_.GetUpdateManifestUrl(bundle_id);

  ManifestBuilder parent_manifest =
      ManifestBuilder()
          .SetName("Parent IWA")
          .SetVersion("1.0.0")
          .AddIcon("/icon.png", gfx::Size(256, 256), "image/png")
          .AddPermissionsPolicy(
              network::mojom::PermissionsPolicyFeature::kSubApps,
              /*self=*/true, /*origins=*/{});
  parent_manifest.SetUpdateManifestUrl(update_manifest_url);

  IsolatedWebAppBuilder iwa_v1(parent_manifest);
  iwa_v1.AddIconAsPng("/icon.png", CreateSquareIcon(256, SK_ColorBLUE));
  iwa_v1.AddHtml("/", "<h1>Parent IWA v1</h1>");

  // Sub app: scope /sub1/ initially to be allowed to install.
  iwa_v1.AddHtml("/sub1/index.html", R"(
    <!DOCTYPE html>
    <html>
      <head>
        <link rel="manifest" href="/sub1/manifest.json">
        <title>Sub App 1</title>
      </head>
      <body><h1>Sub App 1</h1></body>
    </html>
  )");
  iwa_v1.AddResource("/sub1/manifest.json", R"(
    {
      "name": "Sub App 1",
      "version": "1.0.0",
      "id": "/sub1/index.html",
      "start_url": "/sub1/index.html",
      "scope": "/sub1/",
      "icons": [{
        "src": "/icon.png",
        "sizes": "256x256",
        "type": "image/png"
      }]
    }
  )",
                     "application/manifest+json");

  iwa_v1
      .BuildBundle(bundle_id, {web_package::test::GetDefaultEd25519KeyPair()})
      ->InstallChecked(profile());

  IsolatedWebAppUrlInfo iwa_url_info =
      IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(bundle_id);
  webapps::AppId iwa_app_id = iwa_url_info.app_id();

  Browser* iwa_browser = LaunchWebAppBrowserAndWait(iwa_app_id);
  ASSERT_NE(iwa_browser, nullptr);

  webapps::AppId sub_app_1_id = InstallSubAppAndWait(
      iwa_browser->tab_strip_model()->GetActiveWebContents(),
      "/sub1/index.html",
      iwa_url_info.origin().GetURL().Resolve("/sub1/index.html"));

  EXPECT_EQ(provider().registrar_unsafe().GetAppShortName(sub_app_1_id),
            "Sub App 1");
  EXPECT_EQ(provider().registrar_unsafe().GetAppScope(sub_app_1_id),
            iwa_url_info.origin().GetURL().Resolve("/sub1/"));

  iwa_browser->GetWindow()->Close();

  // Update parent IWA that includes the sub app.
  ManifestBuilder parent_manifest_v2 =
      ManifestBuilder()
          .SetName("Parent IWA")
          .SetVersion("2.0.0")
          .AddIcon("/icon.png", gfx::Size(256, 256), "image/png")
          .AddPermissionsPolicy(
              network::mojom::PermissionsPolicyFeature::kSubApps,
              /*self=*/true, /*origins=*/{});
  parent_manifest_v2.SetUpdateManifestUrl(update_manifest_url);

  IsolatedWebAppBuilder iwa_v2(parent_manifest_v2);
  iwa_v2.AddIconAsPng("/icon.png", CreateSquareIcon(256, SK_ColorBLUE));
  iwa_v2.AddHtml("/", "<h1>Parent IWA v2</h1>");

  // Sub app: make scope to be /, identical to the parent.
  // Include also initial html to be sure that first open before update works.
  iwa_v2.AddHtml("/sub1/index.html", R"(
    <!DOCTYPE html>
    <html>
      <head>
        <link rel="manifest" href="/manifest.json">
        <title>Sub App 1</title>
      </head>
      <body><h1>Sub App 1</h1></body>
    </html>
  )");
  // New start index.
  iwa_v2.AddHtml("/sub_index.html", R"(
    <!DOCTYPE html>
    <html>
      <head>
        <link rel="manifest" href="/manifest.json">
        <title>Sub App 1</title>
      </head>
      <body><h1>Sub App 1</h1></body>
    </html>
  )");

  // Scope will be implicitly taken as "parent_url/".
  iwa_v2.AddResource("/manifest.json", R"(
    {
      "name": "Sub App 1 Updated",
      "version": "2.0.0",
      "id": "/sub1/index.html",
      "start_url": "/sub_index.html",
      "icons": [{
        "src": "/icon.png",
        "sizes": "256x256",
        "type": "image/png"
      }]
    }
  )",
                     "application/manifest+json");

  UpdateIwaAndWait(bundle_id, iwa_v2);

  // Now launch Sub App to trigger its manifest update check.
  // We expect the update to fail due to scope overlap validation.
  Browser* sub_app_1_browser = LaunchWebAppBrowserAndWait(sub_app_1_id);
  ASSERT_NE(sub_app_1_browser, nullptr);

  EXPECT_TRUE(
      test::WebAppPageWaiter(
          sub_app_1_browser->tab_strip_model()->GetActiveWebContents())
          .ExpectManifest(
              provider().registrar_unsafe().GetAppManifestId(sub_app_1_id))
          .WaitAndFlushCommands());

  // The name must not be updated.
  EXPECT_EQ(provider().registrar_unsafe().GetAppShortName(sub_app_1_id),
            "Sub App 1");
  // The scope must remain /sub1/ (not updated).
  EXPECT_EQ(provider().registrar_unsafe().GetAppScope(sub_app_1_id),
            iwa_url_info.origin().GetURL().Resolve("/sub1/"));
}

}  // namespace
}  // namespace web_app
