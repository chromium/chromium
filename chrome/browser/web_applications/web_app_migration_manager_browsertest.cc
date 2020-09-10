// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_migration_manager.h"

#include "base/files/file_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/installable/installable_metrics.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/test/ssl_test_utils.h"
#include "chrome/browser/ui/web_applications/web_app_dialog_manager.h"
#include "chrome/browser/ui/web_applications/web_app_dialog_utils.h"
#include "chrome/browser/ui/web_applications/web_app_ui_manager_impl.h"
#include "chrome/browser/web_applications/components/app_icon_manager.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/os_integration_manager.h"
#include "chrome/browser/web_applications/components/web_app_chromeos_data.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "chrome/browser/web_applications/components/web_app_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_launcher.h"
#include "content/public/test/url_loader_interceptor.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/common/extension.h"
#include "net/ssl/ssl_info.h"

namespace web_app {

namespace {

constexpr char kBaseDataDir[] = "chrome/test/data/banners";

// start_url in manifest.json matches navigation url for the simple
// manifest_test_page.html.
constexpr char kSimpleManifestStartUrl[] =
    "https://example.org/manifest_test_page.html";
// start_url in release_notes_manifest.json generates the id of an app that
// should be hidden from the user.
constexpr char kHiddenAppInstallUrl[] =
    "https://www.google.com/manifest_test_page.html"
    "?manifest=release_notes_manifest.json";

constexpr char kHiddenAppStartUrl[] =
    "https://www.google.com/chromebook/whatsnew/embedded/";

constexpr char kManifestWithShortcutsMenuInstallUrl[] =
    "https://example.org/manifest_test_page.html"
    "?manifest=manifest_with_shortcuts.json";

constexpr char kManifestWithShortcutsMenuStartUrl[] =
    "https://example.org/start";

// Performs blocking IO operations.
base::FilePath GetDataFilePath(const base::FilePath& relative_path,
                               bool* path_exists) {
  base::ScopedAllowBlockingForTesting allow_io;

  base::FilePath root_path;
  CHECK(base::PathService::Get(base::DIR_SOURCE_ROOT, &root_path));
  base::FilePath path = root_path.Append(relative_path);
  *path_exists = base::PathExists(path);
  return path;
}

}  // namespace

class WebAppMigrationManagerBrowserTest : public InProcessBrowserTest {
 public:
  WebAppMigrationManagerBrowserTest() {
    if (content::IsPreTest()) {
      scoped_feature_list_.InitAndDisableFeature(
          features::kDesktopPWAsWithoutExtensions);
    } else {
      scoped_feature_list_.InitAndEnableFeature(
          features::kDesktopPWAsWithoutExtensions);
    }
  }

  ~WebAppMigrationManagerBrowserTest() override = default;

  WebAppMigrationManagerBrowserTest(const WebAppMigrationManagerBrowserTest&) =
      delete;
  WebAppMigrationManagerBrowserTest& operator=(
      const WebAppMigrationManagerBrowserTest&) = delete;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    // We use a URLLoaderInterceptor, rather than the EmbeddedTestServer, since
    // a stable app_id across tests requires stable origin, whereas
    // EmbeddedTestServer serves content on a random port.
    url_loader_interceptor_ =
        std::make_unique<content::URLLoaderInterceptor>(base::BindRepeating(
            [](content::URLLoaderInterceptor::RequestParams* params) -> bool {
              std::string relative_request = base::StrCat(
                  {kBaseDataDir, params->url_request.url.path_piece()});
              base::FilePath relative_path =
                  base::FilePath().AppendASCII(relative_request);

              bool path_exists = false;
              base::FilePath path =
                  GetDataFilePath(relative_path, &path_exists);
              if (!path_exists)
                return /*intercepted=*/false;

              // Provide fake SSLInfo to avoid NOT_FROM_SECURE_ORIGIN error in
              // InstallableManager::GetData().
              net::SSLInfo ssl_info;
              CreateFakeSslInfoCertificate(&ssl_info);

              content::URLLoaderInterceptor::WriteResponse(
                  path, params->client.get(), /*headers=*/nullptr, ssl_info);

              return /*intercepted=*/true;
            }));
  }

  void TearDownOnMainThread() override {
    url_loader_interceptor_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  AppId InstallWebAppAsUserViaOmnibox() {
    chrome::SetAutoAcceptPWAInstallConfirmationForTesting(true);
    chrome::SetAutoAcceptWebAppDialogForTesting(
        /*auto_accept=*/true,
        /*auto_open_in_window=*/true);

    provider().os_integration_manager().SuppressOsHooksForTesting();

    AppId app_id;
    base::RunLoop run_loop;
    bool started = CreateWebAppFromManifest(
        browser()->tab_strip_model()->GetActiveWebContents(),
        /*bypass_service_worker_check=*/false,
        WebappInstallSource::OMNIBOX_INSTALL_ICON,
        base::BindLambdaForTesting(
            [&](const AppId& installed_app_id, InstallResultCode code) {
              EXPECT_EQ(code, InstallResultCode::kSuccessNewInstall);
              app_id = installed_app_id;
              run_loop.Quit();
            }));
    EXPECT_TRUE(started);
    run_loop.Run();
    return app_id;
  }

  void UninstallWebAppAsUserViaMenu(const AppId& app_id) {
    extensions::ScopedTestDialogAutoConfirm confirm{
        extensions::ScopedTestDialogAutoConfirm::ACCEPT};

    base::RunLoop run_loop;
    ui_manager().dialog_manager().UninstallWebApp(
        app_id, WebAppDialogManager::UninstallSource::kAppMenu,
        browser()->window(), base::BindLambdaForTesting([&](bool success) {
          EXPECT_TRUE(success);
          run_loop.Quit();
        }));
    run_loop.Run();
  }

  WebAppProvider& provider() {
    WebAppProvider* provider = WebAppProvider::Get(browser()->profile());
    DCHECK(provider);
    return *provider;
  }

  WebAppUiManagerImpl& ui_manager() {
    auto* ui_manager = WebAppUiManagerImpl::Get(browser()->profile());
    DCHECK(ui_manager);
    return *ui_manager;
  }

  void AwaitRegistryReady() {
    base::RunLoop run_loop;
    provider().on_registry_ready().Post(FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

 private:
  std::unique_ptr<content::URLLoaderInterceptor> url_loader_interceptor_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(WebAppMigrationManagerBrowserTest,
                       PRE_DatabaseMigration_SimpleManifest) {
  ui_test_utils::NavigateToURL(browser(), GURL{kSimpleManifestStartUrl});
  AppId app_id = InstallWebAppAsUserViaOmnibox();
  EXPECT_EQ(GenerateAppIdFromURL(GURL{kSimpleManifestStartUrl}), app_id);

  EXPECT_TRUE(provider().registrar().AsBookmarkAppRegistrar());
  EXPECT_FALSE(provider().registrar().AsWebAppRegistrar());

  EXPECT_TRUE(provider().registrar().IsInstalled(app_id));
}

IN_PROC_BROWSER_TEST_F(WebAppMigrationManagerBrowserTest,
                       DatabaseMigration_SimpleManifest) {
  AwaitRegistryReady();

  AppId app_id = GenerateAppIdFromURL(GURL{kSimpleManifestStartUrl});
  EXPECT_TRUE(provider().registrar().IsInstalled(app_id));

  WebAppRegistrar* registrar = provider().registrar().AsWebAppRegistrar();
  ASSERT_TRUE(registrar);
  EXPECT_FALSE(provider().registrar().AsBookmarkAppRegistrar());

  const WebApp* web_app = registrar->GetAppById(app_id);
  ASSERT_TRUE(web_app);

  EXPECT_EQ("Manifest test app", web_app->name());
  EXPECT_EQ(DisplayMode::kStandalone, web_app->display_mode());

  const std::vector<SquareSizePx> icon_sizes_in_px = {32,  48,  64,  96, 128,
                                                      144, 192, 256, 512};
  EXPECT_EQ(icon_sizes_in_px, web_app->downloaded_icon_sizes(IconPurpose::ANY));

  base::RunLoop run_loop;
  provider().icon_manager().ReadIcons(
      app_id, IconPurpose::ANY,
      web_app->downloaded_icon_sizes(IconPurpose::ANY),
      base::BindLambdaForTesting(
          [&](std::map<SquareSizePx, SkBitmap> icon_bitmaps) {
            EXPECT_EQ(9u, icon_bitmaps.size());
            for (auto& size_px_and_bitmap : icon_bitmaps) {
              SquareSizePx size_px = size_px_and_bitmap.first;
              EXPECT_TRUE(base::Contains(icon_sizes_in_px, size_px));

              SkBitmap bitmap = size_px_and_bitmap.second;
              EXPECT_FALSE(bitmap.empty());
              EXPECT_EQ(size_px, bitmap.width());
              EXPECT_EQ(size_px, bitmap.height());
            }
            run_loop.Quit();
          }));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(WebAppMigrationManagerBrowserTest,
                       PRE_DatabaseMigration_HiddenFromUser) {
  ui_test_utils::NavigateToURL(browser(), GURL{kHiddenAppInstallUrl});
  AppId app_id = InstallWebAppAsUserViaOmnibox();
  EXPECT_EQ(GenerateAppIdFromURL(GURL(kHiddenAppStartUrl)), app_id);

  EXPECT_TRUE(provider().registrar().AsBookmarkAppRegistrar());
  EXPECT_FALSE(provider().registrar().AsWebAppRegistrar());

  EXPECT_TRUE(provider().registrar().IsInstalled(app_id));
}

IN_PROC_BROWSER_TEST_F(WebAppMigrationManagerBrowserTest,
                       DatabaseMigration_HiddenFromUser) {
  AwaitRegistryReady();

  AppId app_id = GenerateAppIdFromURL(GURL{kHiddenAppStartUrl});
  EXPECT_TRUE(provider().registrar().IsInstalled(app_id));

  WebAppRegistrar* registrar = provider().registrar().AsWebAppRegistrar();
  ASSERT_TRUE(registrar);
  EXPECT_FALSE(provider().registrar().AsBookmarkAppRegistrar());

  const WebApp* web_app = registrar->GetAppById(app_id);
  ASSERT_TRUE(web_app);

  if (IsChromeOs()) {
    EXPECT_FALSE(web_app->chromeos_data()->show_in_launcher);
    EXPECT_FALSE(web_app->chromeos_data()->show_in_search);
    EXPECT_FALSE(web_app->chromeos_data()->show_in_management);
  } else {
    EXPECT_FALSE(web_app->chromeos_data().has_value());
  }
}

IN_PROC_BROWSER_TEST_F(WebAppMigrationManagerBrowserTest,
                       InstallShadowBookmarkApp) {
  EXPECT_FALSE(provider().registrar().AsBookmarkAppRegistrar());
  AwaitRegistryReady();

  auto* extensions_registry =
      extensions::ExtensionRegistry::Get(browser()->profile());
  extensions::TestExtensionRegistryObserver extensions_registry_observer(
      extensions_registry);

  ui_test_utils::NavigateToURL(browser(), GURL{kSimpleManifestStartUrl});
  AppId app_id = InstallWebAppAsUserViaOmnibox();

  EXPECT_TRUE(provider().registrar().IsInstalled(app_id));

  scoped_refptr<const extensions::Extension> extension =
      extensions_registry_observer.WaitForExtensionInstalled();
  EXPECT_EQ(extension->id(), app_id);
  EXPECT_EQ("Manifest test app", extension->short_name());
}

IN_PROC_BROWSER_TEST_F(WebAppMigrationManagerBrowserTest,
                       PRE_UninstallShadowBookmarkApp) {
  EXPECT_TRUE(provider().registrar().AsBookmarkAppRegistrar());
  AwaitRegistryReady();

  // Install shadow bookmark app.
  ui_test_utils::NavigateToURL(browser(), GURL{kSimpleManifestStartUrl});
  AppId app_id = InstallWebAppAsUserViaOmnibox();

  EXPECT_TRUE(provider().registrar().IsInstalled(app_id));
  EXPECT_TRUE(ui_manager().dialog_manager().CanUninstallWebApp(app_id));
}

IN_PROC_BROWSER_TEST_F(WebAppMigrationManagerBrowserTest,
                       UninstallShadowBookmarkApp) {
  EXPECT_FALSE(provider().registrar().AsBookmarkAppRegistrar());
  AwaitRegistryReady();

  AppId app_id = GenerateAppIdFromURL(GURL{kSimpleManifestStartUrl});

  EXPECT_TRUE(provider().registrar().IsInstalled(app_id));
  EXPECT_TRUE(ui_manager().dialog_manager().CanUninstallWebApp(app_id));

  auto* extensions_registry =
      extensions::ExtensionRegistry::Get(browser()->profile());
  extensions::TestExtensionRegistryObserver extensions_registry_observer(
      extensions_registry);

  UninstallWebAppAsUserViaMenu(app_id);
  EXPECT_FALSE(provider().registrar().IsInstalled(app_id));

  scoped_refptr<const extensions::Extension> extension =
      extensions_registry_observer.WaitForExtensionUninstalled();
  EXPECT_EQ(extension->id(), app_id);
  EXPECT_EQ("Manifest test app", extension->short_name());
}

// TODO(crbug.com/1020037): Test policy installed bookmark apps with an external
// install source to cover
// WebAppMigrationManager::MigrateBookmarkAppInstallSource() logic.

class WebAppMigrationManagerBrowserTestWithShortcutsMenu
    : public WebAppMigrationManagerBrowserTest {
 public:
  WebAppMigrationManagerBrowserTestWithShortcutsMenu() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kDesktopPWAsAppIconShortcutsMenu);
  }

  void ReadAndVerifyDownloadedShortcutsMenuIcons(
      const AppId& app_id,
      std::vector<std::vector<SquareSizePx>> shortcuts_menu_icons_sizes) {
    EXPECT_EQ(
        provider().registrar().GetAppDownloadedShortcutsMenuIconsSizes(app_id),
        shortcuts_menu_icons_sizes);

    base::RunLoop run_loop;
    provider().icon_manager().ReadAllShortcutsMenuIcons(
        app_id,
        base::BindLambdaForTesting(
            [&](ShortcutsMenuIconsBitmaps shortcuts_menu_icons_bitmaps) {
              EXPECT_EQ(2u, shortcuts_menu_icons_bitmaps.size());
              for (size_t i = 0; i < shortcuts_menu_icons_bitmaps.size(); ++i) {
                EXPECT_EQ(shortcuts_menu_icons_sizes[i].size(),
                          shortcuts_menu_icons_bitmaps[i].size());
                const std::vector<SquareSizePx>& icon_sizes =
                    shortcuts_menu_icons_sizes[i];
                const std::map<SquareSizePx, SkBitmap>& icon_maps =
                    shortcuts_menu_icons_bitmaps[i];
                for (const auto& icon_map : icon_maps) {
                  const SquareSizePx& size_px = icon_map.first;
                  EXPECT_TRUE(base::Contains(icon_sizes, size_px));

                  const SkBitmap& bitmap = icon_map.second;
                  EXPECT_FALSE(bitmap.empty());
                  EXPECT_EQ(size_px, bitmap.width());
                  EXPECT_EQ(size_px, bitmap.height());
                }
              }
              run_loop.Quit();
            }));
    run_loop.Run();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(WebAppMigrationManagerBrowserTestWithShortcutsMenu,
                       PRE_DatabaseMigration_ManifestWithShortcutsMenu) {
  ui_test_utils::NavigateToURL(browser(),
                               GURL{kManifestWithShortcutsMenuInstallUrl});
  AppId app_id = InstallWebAppAsUserViaOmnibox();
  EXPECT_EQ(GenerateAppIdFromURL(GURL{kManifestWithShortcutsMenuStartUrl}),
            app_id);

  EXPECT_TRUE(provider().registrar().AsBookmarkAppRegistrar());
  EXPECT_FALSE(provider().registrar().AsWebAppRegistrar());

  EXPECT_TRUE(provider().registrar().IsInstalled(app_id));

  std::vector<WebApplicationShortcutsMenuItemInfo> shortcuts_menu_item_infos =
      provider().registrar().GetAppShortcutsMenuItemInfos(app_id);
  EXPECT_EQ(shortcuts_menu_item_infos.size(), 2u);
  EXPECT_EQ(shortcuts_menu_item_infos[0].name, base::UTF8ToUTF16("shortcut1"));
  EXPECT_EQ(shortcuts_menu_item_infos[0].shortcut_icon_infos.size(), 1u);
  EXPECT_EQ(shortcuts_menu_item_infos[1].name, base::UTF8ToUTF16("shortcut2"));
  EXPECT_EQ(shortcuts_menu_item_infos[1].shortcut_icon_infos.size(), 2u);

  const std::vector<std::vector<SquareSizePx>> shortcuts_menu_icons_sizes = {
      {48}, {96, 144}};
  ReadAndVerifyDownloadedShortcutsMenuIcons(app_id, shortcuts_menu_icons_sizes);
}

IN_PROC_BROWSER_TEST_F(WebAppMigrationManagerBrowserTestWithShortcutsMenu,
                       DatabaseMigration_ManifestWithShortcutsMenu) {
  AwaitRegistryReady();

  AppId app_id = GenerateAppIdFromURL(GURL{kManifestWithShortcutsMenuStartUrl});
  EXPECT_TRUE(provider().registrar().IsInstalled(app_id));

  EXPECT_FALSE(provider().registrar().AsBookmarkAppRegistrar());
  WebAppRegistrar* registrar = provider().registrar().AsWebAppRegistrar();
  ASSERT_TRUE(registrar);

  const WebApp* web_app = registrar->GetAppById(app_id);
  ASSERT_TRUE(web_app);

  EXPECT_EQ("Manifest test app with Shortcuts", web_app->name());
  EXPECT_EQ(DisplayMode::kStandalone, web_app->display_mode());

  EXPECT_EQ(web_app->shortcuts_menu_item_infos().size(), 2u);
  EXPECT_EQ(web_app->shortcuts_menu_item_infos()[0].name,
            base::UTF8ToUTF16("shortcut1"));
  EXPECT_EQ(web_app->shortcuts_menu_item_infos()[0].shortcut_icon_infos.size(),
            1u);
  EXPECT_EQ(web_app->shortcuts_menu_item_infos()[1].name,
            base::UTF8ToUTF16("shortcut2"));
  EXPECT_EQ(web_app->shortcuts_menu_item_infos()[1].shortcut_icon_infos.size(),
            2u);

  const std::vector<std::vector<SquareSizePx>> shortcuts_menu_icons_sizes = {
      {48}, {96, 144}};
  ReadAndVerifyDownloadedShortcutsMenuIcons(app_id, shortcuts_menu_icons_sizes);
}

}  // namespace web_app
