// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <queue>
#include <utility>

#include "base/optional.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/banners/app_banner_settings_helper.h"
#include "chrome/browser/extensions/convert_web_app.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/installable/installable_metrics.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/install_manager.h"
#include "chrome/browser/web_applications/components/os_integration_manager.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "chrome/browser/web_applications/components/web_app_provider_base.h"
#include "chrome/browser/web_applications/extensions/bookmark_app_install_finalizer.h"
#include "chrome/browser/web_applications/extensions/bookmark_app_registrar.h"
#include "chrome/browser/web_applications/extensions/bookmark_app_util.h"
#include "chrome/browser/web_applications/test/test_app_registrar.h"
#include "chrome/browser/web_applications/test/test_data_retriever.h"
#include "chrome/browser/web_applications/test/test_web_app_provider.h"
#include "chrome/browser/web_applications/test/test_web_app_url_loader.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/common/extensions/manifest_handlers/app_theme_color_info.h"
#include "chrome/common/web_application_info.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_icon_set.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "url/gurl.h"

namespace extensions {

namespace {

const char kAppAlternativeScope[] = "http://www.chromium.org/new/";
const char kAppDefaultScope[] = "http://www.chromium.org/";
const char kAppTitle[] = "Test title";
const char kAlternativeAppTitle[] = "Different test title";
const char kAppDescription[] = "Test description";
const char kAppIconURL1[] = "http://foo.com/1.png";
const char kAppIconURL2[] = "http://foo.com/2.png";

const int kIconSizeTiny = extension_misc::EXTENSION_ICON_BITTY;
const int kIconSizeSmall = extension_misc::EXTENSION_ICON_SMALL;
const int kIconSizeMedium = extension_misc::EXTENSION_ICON_MEDIUM;
const int kIconSizeLarge = extension_misc::EXTENSION_ICON_LARGE;

// TODO(https://crbug.com/1042727): Fix test GURL scoping and remove this getter
// function.
GURL AppUrl() {
  return GURL("https://www.chromium.org/index.html");
}
GURL AppScope() {
  return GURL("https://www.chromium.org/");
}

SkBitmap CreateSquareBitmapWithColor(int size, SkColor color) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(size, size);
  bitmap.eraseColor(color);
  return bitmap;
}

void SetAppIcon(WebApplicationInfo* web_app, int size, SkColor color) {
  web_app->icon_bitmaps_any.clear();
  web_app->icon_bitmaps_any[size] = CreateSquareBitmapWithColor(size, color);
}

// Use only real BookmarkAppInstallFinalizer::FinalizeInstall and mock any other
// finalization steps as a no-operation.
class BookmarkAppInstallFinalizerInstallOnly
    : public BookmarkAppInstallFinalizer {
 public:
  using BookmarkAppInstallFinalizer::BookmarkAppInstallFinalizer;
  ~BookmarkAppInstallFinalizerInstallOnly() override = default;

  // InstallFinalizer:
  void ReparentTab(const web_app::AppId& app_id,
                   bool shortcut_created,
                   content::WebContents* web_contents) override {}
};

}  // namespace

class InstallManagerBookmarkAppTest : public ExtensionServiceTestBase {
 public:
  InstallManagerBookmarkAppTest() {
    scoped_feature_list_.InitWithFeatures(
        {}, {features::kDesktopPWAsWithoutExtensions});
  }

  ~InstallManagerBookmarkAppTest() override = default;

  void SetUp() override {
    ExtensionServiceTestBase::SetUp();
    InitializeEmptyExtensionService();
    service_->Init();
    EXPECT_EQ(0u, registry()->enabled_extensions().size());
    web_contents_ =
        content::WebContentsTester::CreateTestWebContents(profile(), nullptr);

    auto* provider = web_app::TestWebAppProvider::Get(profile());

    auto registrar = std::make_unique<BookmarkAppRegistrar>(profile());
    registrar_ = registrar.get();

    auto install_finalizer =
        std::make_unique<BookmarkAppInstallFinalizerInstallOnly>(profile());
    install_finalizer_ = install_finalizer.get();

    auto install_manager =
        std::make_unique<web_app::WebAppInstallManager>(profile());
    web_app::WebAppInstallManager* install_manager_ptr = install_manager.get();

    install_manager->SetDataRetrieverFactoryForTesting(
        base::BindLambdaForTesting([this]() {
          // This factory requires a prepared DataRetriever. A test should
          // create one with CreateDefaultDataToRetrieve, for example.
          DCHECK(!prepared_data_retrievers_.empty());
          std::unique_ptr<web_app::WebAppDataRetriever> data_retriever =
              std::move(prepared_data_retrievers_.front());
          prepared_data_retrievers_.pop();
          return data_retriever;
        }));

    auto test_url_loader = std::make_unique<web_app::TestWebAppUrlLoader>();
    test_url_loader_ = test_url_loader.get();
    install_manager->SetUrlLoaderForTesting(std::move(test_url_loader));

    provider->SetRegistrar(std::move(registrar));
    provider->SetInstallManager(std::move(install_manager));
    provider->SetInstallFinalizer(std::move(install_finalizer));

    provider->Start();
    // Start only WebAppInstallManager for real.
    install_manager_ptr->Start();

    web_app::WebAppProviderBase::GetProviderBase(profile())
        ->os_integration_manager()
        .SuppressOsHooksForTesting();
  }

  void TearDown() override {
    ExtensionServiceTestBase::TearDown();
    for (content::RenderProcessHost::iterator i(
             content::RenderProcessHost::AllHostsIterator());
         !i.IsAtEnd(); i.Advance()) {
      content::RenderProcessHost* host = i.GetCurrentValue();
      if (Profile::FromBrowserContext(host->GetBrowserContext()) ==
          profile_.get())
        host->Cleanup();
    }
  }

  content::WebContents* web_contents() { return web_contents_.get(); }

  web_app::WebAppInstallManager& install_manager() {
    auto* provider = web_app::WebAppProviderBase::GetProviderBase(profile());
    return *static_cast<web_app::WebAppInstallManager*>(
        &provider->install_manager());
  }

  web_app::TestWebAppUrlLoader& url_loader() {
    DCHECK(test_url_loader_);
    return *test_url_loader_;
  }

  web_app::AppRegistrar* app_registrar() {
    DCHECK(registrar_);
    return registrar_;
  }

  web_app::TestDataRetriever* AddEmptyDataRetriever() {
    prepared_data_retrievers_.push(
        std::make_unique<web_app::TestDataRetriever>());
    return prepared_data_retrievers_.back().get();
  }

  web_app::TestDataRetriever* AddDataRetrieverWithManifest(
      std::unique_ptr<blink::Manifest> manifest,
      bool is_installable) {
    web_app::TestDataRetriever* data_retriever = AddEmptyDataRetriever();
    data_retriever->SetRendererWebApplicationInfo(
        std::make_unique<WebApplicationInfo>());
    data_retriever->SetManifest(std::move(manifest), is_installable);
    return data_retriever;
  }

  web_app::TestDataRetriever* AddDataRetrieverWithRendererWebAppInfo(
      std::unique_ptr<WebApplicationInfo> web_app_info,
      bool is_installable) {
    web_app::TestDataRetriever* data_retriever = AddEmptyDataRetriever();

    data_retriever->SetRendererWebApplicationInfo(std::move(web_app_info));

    auto manifest = std::make_unique<blink::Manifest>();
    data_retriever->SetManifest(std::move(manifest), is_installable);

    web_app::IconsMap icons_map;
    icons_map[AppUrl()].push_back(
        CreateSquareBitmapWithColor(kIconSizeSmall, SK_ColorRED));
    data_retriever->SetIcons(std::move(icons_map));

    return data_retriever;
  }

  web_app::TestDataRetriever* AddDataRetrieverWithLaunchContainer(
      const GURL& start_url,
      bool open_as_window,
      bool is_installable) {
    web_app::TestDataRetriever* data_retriever = AddEmptyDataRetriever();

    auto web_app_info = std::make_unique<WebApplicationInfo>();
    web_app_info->open_as_window = open_as_window;
    data_retriever->SetRendererWebApplicationInfo(std::move(web_app_info));

    auto manifest = std::make_unique<blink::Manifest>();
    manifest->start_url = start_url;
    manifest->name = base::ASCIIToUTF16(kAppTitle);
    manifest->scope = GURL(AppScope());
    data_retriever->SetManifest(std::move(manifest), is_installable);

    data_retriever->SetIcons(web_app::IconsMap{});

    return data_retriever;
  }

  const Extension* InstallWebAppFromManifestWithFallback() {
    base::RunLoop run_loop;
    web_app::AppId app_id;

    auto* provider = web_app::WebAppProviderBase::GetProviderBase(profile());

    provider->install_manager().InstallWebAppFromManifestWithFallback(
        web_contents(),
        /*force_shortcut_app=*/false, WebappInstallSource::MENU_BROWSER_TAB,
        base::BindOnce(web_app::TestAcceptDialogCallback),
        base::BindLambdaForTesting([&](const web_app::AppId& installed_app_id,
                                       web_app::InstallResultCode code) {
          EXPECT_EQ(web_app::InstallResultCode::kSuccessNewInstall, code);
          app_id = installed_app_id;
          run_loop.Quit();
        }));

    run_loop.Run();

    const Extension* extension = registry()->GetInstalledExtension(app_id);
    DCHECK(extension);
    return extension;
  }

  const Extension* InstallWebAppWithParams(
      WebappInstallSource install_source,
      web_app::InstallManager::InstallParams install_params =
          web_app::InstallManager::InstallParams{}) {
    base::RunLoop run_loop;
    web_app::AppId app_id;
    install_params.fallback_start_url = GURL("https://example.com/fallback");

    auto* provider = web_app::WebAppProviderBase::GetProviderBase(profile());

    provider->install_manager().InstallWebAppWithParams(
        web_contents(), install_params, install_source,
        base::BindLambdaForTesting([&](const web_app::AppId& installed_app_id,
                                       web_app::InstallResultCode code) {
          EXPECT_EQ(web_app::InstallResultCode::kSuccessNewInstall, code);
          app_id = installed_app_id;
          run_loop.Quit();
        }));

    run_loop.Run();

    const Extension* extension = registry()->GetInstalledExtension(app_id);
    DCHECK(extension);
    return extension;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<content::WebContents> web_contents_;

  BookmarkAppRegistrar* registrar_ = nullptr;
  BookmarkAppInstallFinalizerInstallOnly* install_finalizer_ = nullptr;

  web_app::TestWebAppUrlLoader* test_url_loader_ = nullptr;

  // Initially owned by this test fixture. Passed to |install_manager_| from
  // front to back with each install task.
  std::queue<std::unique_ptr<web_app::TestDataRetriever>>
      prepared_data_retrievers_;

  DISALLOW_COPY_AND_ASSIGN(InstallManagerBookmarkAppTest);
};

TEST_F(InstallManagerBookmarkAppTest, CreateBookmarkApp) {
  auto web_app_info = std::make_unique<WebApplicationInfo>();
  web_app_info->start_url = AppUrl();
  web_app_info->title = base::UTF8ToUTF16(kAppTitle);
  web_app_info->description = base::UTF8ToUTF16(kAppDescription);
  AddDataRetrieverWithRendererWebAppInfo(std::move(web_app_info),
                                         /*is_installable=*/false);

  const Extension* extension = InstallWebAppFromManifestWithFallback();

  EXPECT_EQ(1u, registry()->enabled_extensions().size());
  EXPECT_TRUE(extension->from_bookmark());
  EXPECT_FALSE(extension->was_installed_by_default());
  EXPECT_FALSE(Manifest::IsPolicyLocation(extension->location()));

  EXPECT_EQ(kAppTitle, extension->name());
  EXPECT_EQ(kAppDescription, extension->description());
  EXPECT_EQ(AppUrl(), AppLaunchInfo::GetLaunchWebURL(extension));
  EXPECT_FALSE(IconsInfo::GetIconResource(extension, kIconSizeSmall,
                                          ExtensionIconSet::MATCH_EXACTLY)
                   .empty());
  base::Optional<base::Time> added_time =
      AppBannerSettingsHelper::GetSingleBannerEvent(
          web_contents(), AppUrl(), AppUrl().spec(),
          AppBannerSettingsHelper::APP_BANNER_EVENT_DID_ADD_TO_HOMESCREEN);
  EXPECT_FALSE(added_time && added_time->is_null());
}

TEST_F(InstallManagerBookmarkAppTest, CreateBookmarkAppDefaultApp) {
  auto web_app_info = std::make_unique<WebApplicationInfo>();
  web_app_info->start_url = AppUrl();
  web_app_info->title = base::UTF8ToUTF16(kAppTitle);
  web_app_info->description = base::UTF8ToUTF16(kAppDescription);
  AddDataRetrieverWithRendererWebAppInfo(std::move(web_app_info),
                                         /*is_installable=*/false);

  const Extension* extension =
      InstallWebAppWithParams(WebappInstallSource::EXTERNAL_DEFAULT);

  EXPECT_TRUE(extension->from_bookmark());
  EXPECT_TRUE(extension->was_installed_by_default());
  EXPECT_EQ(Manifest::EXTERNAL_PREF_DOWNLOAD, extension->location());
  EXPECT_FALSE(Manifest::IsPolicyLocation(extension->location()));
}

TEST_F(InstallManagerBookmarkAppTest, CreateBookmarkAppPolicyInstalled) {
  auto web_app_info = std::make_unique<WebApplicationInfo>();
  web_app_info->start_url = AppUrl();
  web_app_info->title = base::UTF8ToUTF16(kAppTitle);
  web_app_info->description = base::UTF8ToUTF16(kAppDescription);
  AddDataRetrieverWithRendererWebAppInfo(std::move(web_app_info),
                                         /*is_installable=*/false);

  const Extension* extension =
      InstallWebAppWithParams(WebappInstallSource::EXTERNAL_POLICY);

  EXPECT_TRUE(extension->from_bookmark());
  EXPECT_FALSE(extension->was_installed_by_default());
  EXPECT_TRUE(Manifest::IsPolicyLocation(extension->location()));
}

class InstallManagerBookmarkAppInstallableSiteTest
    : public InstallManagerBookmarkAppTest,
      public ::testing::WithParamInterface<web_app::ForInstallableSite> {
 public:
  InstallManagerBookmarkAppInstallableSiteTest() {}
  ~InstallManagerBookmarkAppInstallableSiteTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(InstallManagerBookmarkAppInstallableSiteTest);
};

TEST_P(InstallManagerBookmarkAppInstallableSiteTest,
       CreateBookmarkAppWithManifest) {
  auto manifest = std::make_unique<blink::Manifest>();
  manifest->start_url = AppUrl();
  manifest->name = base::ASCIIToUTF16(kAppTitle);
  manifest->scope = GURL(AppScope());
  manifest->theme_color = SK_ColorBLUE;

  const bool is_installable = GetParam() == web_app::ForInstallableSite::kYes;
  AddDataRetrieverWithManifest(std::move(manifest), is_installable);

  const Extension* extension = InstallWebAppFromManifestWithFallback();

  EXPECT_EQ(1u, registry()->enabled_extensions().size());
  EXPECT_TRUE(extension->from_bookmark());
  EXPECT_EQ(kAppTitle, extension->name());
  EXPECT_EQ(AppUrl(), AppLaunchInfo::GetLaunchWebURL(extension));
  EXPECT_EQ(SK_ColorBLUE, AppThemeColorInfo::GetThemeColor(extension).value());
  base::Optional<base::Time> added_time =
      AppBannerSettingsHelper::GetSingleBannerEvent(
          web_contents(), AppUrl(), AppUrl().spec(),
          AppBannerSettingsHelper::APP_BANNER_EVENT_DID_ADD_TO_HOMESCREEN);
  EXPECT_FALSE(added_time && added_time->is_null());
  EXPECT_EQ(GURL(AppScope()), GetScopeURLFromBookmarkApp(extension));
}

TEST_P(InstallManagerBookmarkAppInstallableSiteTest,
       CreateBookmarkAppWithManifestIcons) {
  auto manifest = std::make_unique<blink::Manifest>();
  manifest->start_url = AppUrl();
  manifest->name = base::ASCIIToUTF16(kAppTitle);
  manifest->scope = GURL(AppScope());

  blink::Manifest::ImageResource icon;
  icon.src = GURL(kAppIconURL1);
  icon.purpose = {blink::Manifest::ImageResource::Purpose::ANY};
  manifest->icons.push_back(icon);
  icon.src = GURL(kAppIconURL2);
  manifest->icons.push_back(icon);

  const bool is_installable = GetParam() == web_app::ForInstallableSite::kYes;

  web_app::TestDataRetriever* data_retriever =
      AddDataRetrieverWithManifest(std::move(manifest), is_installable);

  // In the legacy system Favicon URLs were ignored by WebAppIconDownloader
  // because the site had a manifest with icons: Only 1 icon had to be
  // downloaded since the other was provided by the InstallableManager. In the
  // new extension-independent system we prefer to redownload all the icons: 2
  // icons have to be downloaded.
  data_retriever->SetGetIconsDelegate(base::BindLambdaForTesting(
      [&](content::WebContents* web_contents,
          const std::vector<GURL>& icon_urls, bool skip_page_favicons) {
        // Instructs the downloader to not query the page for favicons.
        EXPECT_TRUE(skip_page_favicons);

        const std::vector<GURL> expected_icon_urls{GURL(kAppIconURL1),
                                                   GURL(kAppIconURL2)};
        EXPECT_EQ(expected_icon_urls, icon_urls);

        web_app::IconsMap icons_map;
        icons_map[GURL(kAppIconURL1)].push_back(
            CreateSquareBitmapWithColor(kIconSizeTiny, SK_ColorBLUE));
        icons_map[GURL(kAppIconURL2)].push_back(
            CreateSquareBitmapWithColor(kIconSizeSmall, SK_ColorRED));
        return icons_map;
      }));

  const Extension* extension = InstallWebAppFromManifestWithFallback();

  EXPECT_EQ(1u, registry()->enabled_extensions().size());
  EXPECT_TRUE(extension->from_bookmark());
  EXPECT_EQ(kAppTitle, extension->name());
  EXPECT_EQ(AppUrl(), AppLaunchInfo::GetLaunchWebURL(extension));
  EXPECT_EQ(GURL(AppScope()), GetScopeURLFromBookmarkApp(extension));
}

TEST_P(InstallManagerBookmarkAppInstallableSiteTest,
       CreateBookmarkAppWithManifestNoScope) {
  auto manifest = std::make_unique<blink::Manifest>();
  manifest->start_url = AppUrl();
  manifest->scope = GURL(kAppDefaultScope);
  manifest->name = base::ASCIIToUTF16(kAppTitle);

  const bool is_installable = GetParam() == web_app::ForInstallableSite::kYes;
  AddDataRetrieverWithManifest(std::move(manifest), is_installable);

  const Extension* extension = InstallWebAppFromManifestWithFallback();

  EXPECT_EQ(GURL(kAppDefaultScope), GetScopeURLFromBookmarkApp(extension));
}

INSTANTIATE_TEST_SUITE_P(All,
                         InstallManagerBookmarkAppInstallableSiteTest,
                         ::testing::Values(web_app::ForInstallableSite::kNo,
                                           web_app::ForInstallableSite::kYes));

TEST_F(InstallManagerBookmarkAppTest,
       CreateBookmarkAppDefaultLauncherContainers) {
  {
    AddDataRetrieverWithLaunchContainer(AppUrl(), /*open_as_window=*/true,
                                        /*is_installable=*/true);

    const Extension* extension = InstallWebAppFromManifestWithFallback();

    EXPECT_EQ(LaunchContainer::kLaunchContainerWindow,
              GetLaunchContainer(ExtensionPrefs::Get(profile()), extension));

    // Mark the app as not locally installed and check that it now opens in a
    // tab.
    SetBookmarkAppIsLocallyInstalled(profile(), extension, false);
    EXPECT_EQ(LaunchContainer::kLaunchContainerTab,
              GetLaunchContainer(ExtensionPrefs::Get(profile()), extension));

    // Mark the app as locally installed and check that it now opens in a
    // window.
    SetBookmarkAppIsLocallyInstalled(profile(), extension, true);
    EXPECT_EQ(LaunchContainer::kLaunchContainerWindow,
              GetLaunchContainer(ExtensionPrefs::Get(profile()), extension));
  }
  {
    AddDataRetrieverWithLaunchContainer(GURL("https://www.example.org/"),
                                        /*open_as_window=*/false,
                                        /*is_installable=*/false);

    const Extension* extension = InstallWebAppFromManifestWithFallback();

    EXPECT_EQ(LaunchContainer::kLaunchContainerTab,
              GetLaunchContainer(ExtensionPrefs::Get(profile()), extension));
  }
}

TEST_F(InstallManagerBookmarkAppTest,
       CreateBookmarkAppForcedLauncherContainers) {
  {
    const GURL start_url("https://www.example.org/");
    AddDataRetrieverWithLaunchContainer(start_url,
                                        /*open_as_window=*/true,
                                        /*is_installable=*/true);

    web_app::InstallManager::InstallParams params;
    params.user_display_mode = web_app::DisplayMode::kBrowser;

    const Extension* extension =
        InstallWebAppWithParams(WebappInstallSource::INTERNAL_DEFAULT, params);

    EXPECT_EQ(LaunchContainer::kLaunchContainerTab,
              GetLaunchContainer(ExtensionPrefs::Get(profile()), extension));
  }
  {
    AddDataRetrieverWithLaunchContainer(AppUrl(), /*open_as_window=*/false,
                                        /*is_installable=*/false);

    web_app::InstallManager::InstallParams params;
    params.user_display_mode = web_app::DisplayMode::kStandalone;

    const Extension* extension =
        InstallWebAppWithParams(WebappInstallSource::INTERNAL_DEFAULT, params);

    EXPECT_EQ(LaunchContainer::kLaunchContainerWindow,
              GetLaunchContainer(ExtensionPrefs::Get(profile()), extension));
  }
}

TEST_F(InstallManagerBookmarkAppTest, CreateBookmarkAppWithoutManifest) {
  auto web_app_info = std::make_unique<WebApplicationInfo>();
  web_app_info->start_url = AppUrl();
  web_app_info->title = base::UTF8ToUTF16(kAppTitle);
  web_app_info->description = base::UTF8ToUTF16(kAppDescription);

  web_app::TestDataRetriever* data_retriever = AddEmptyDataRetriever();
  data_retriever->SetRendererWebApplicationInfo(std::move(web_app_info));
  auto manifest = std::make_unique<blink::Manifest>();
  data_retriever->SetManifest(std::move(manifest), /*is_installable=*/false);
  data_retriever->SetIcons(web_app::IconsMap{});

  const Extension* extension = InstallWebAppFromManifestWithFallback();

  EXPECT_EQ(kAppTitle, extension->name());
  EXPECT_EQ(kAppDescription, extension->description());
  EXPECT_EQ(AppUrl(), AppLaunchInfo::GetLaunchWebURL(extension));
  EXPECT_EQ(GURL(), GetScopeURLFromBookmarkApp(extension));
  EXPECT_FALSE(AppThemeColorInfo::GetThemeColor(extension));
}

TEST_F(InstallManagerBookmarkAppTest, CreateWebAppFromInfo) {
  AddEmptyDataRetriever();

  web_app::InstallManager::InstallParams params;

  auto web_app_info = std::make_unique<WebApplicationInfo>();
  web_app_info->start_url = AppUrl();
  web_app_info->title = base::UTF8ToUTF16(kAppTitle);
  web_app_info->description = base::UTF8ToUTF16(kAppDescription);
  web_app_info->scope = AppScope();
  SetAppIcon(web_app_info.get(), kIconSizeTiny, SK_ColorRED);

  base::RunLoop run_loop;
  web_app::AppId app_id;

  auto* provider = web_app::WebAppProviderBase::GetProviderBase(profile());

  provider->install_manager().InstallWebAppFromInfo(
      std::move(web_app_info), web_app::ForInstallableSite::kYes, params,
      WebappInstallSource::ARC,
      base::BindLambdaForTesting([&](const web_app::AppId& installed_app_id,
                                     web_app::InstallResultCode code) {
        EXPECT_EQ(web_app::InstallResultCode::kSuccessNewInstall, code);
        app_id = installed_app_id;
        run_loop.Quit();
      }));

  run_loop.Run();

  const Extension* extension = registry()->GetInstalledExtension(app_id);
  ASSERT_TRUE(extension);

  EXPECT_EQ(1u, registry()->enabled_extensions().size());
  EXPECT_TRUE(extension->from_bookmark());
  EXPECT_EQ(kAppTitle, extension->name());
  EXPECT_EQ(kAppDescription, extension->description());
  EXPECT_EQ(AppUrl(), AppLaunchInfo::GetLaunchWebURL(extension));
  EXPECT_EQ(AppScope(), GetScopeURLFromBookmarkApp(extension));
  EXPECT_FALSE(IconsInfo::GetIconResource(extension, kIconSizeTiny,
                                          ExtensionIconSet::MATCH_EXACTLY)
                   .empty());
  EXPECT_FALSE(IconsInfo::GetIconResource(extension, kIconSizeSmall,
                                          ExtensionIconSet::MATCH_EXACTLY)
                   .empty());
  EXPECT_FALSE(IconsInfo::GetIconResource(extension, kIconSizeSmall * 2,
                                          ExtensionIconSet::MATCH_EXACTLY)
                   .empty());
  EXPECT_FALSE(IconsInfo::GetIconResource(extension, kIconSizeMedium,
                                          ExtensionIconSet::MATCH_EXACTLY)
                   .empty());
  EXPECT_FALSE(IconsInfo::GetIconResource(extension, kIconSizeMedium * 2,
                                          ExtensionIconSet::MATCH_EXACTLY)
                   .empty());
}

TEST_F(InstallManagerBookmarkAppTest, InstallBookmarkAppFromSync) {
  // This process runs two install tasks if the first attempt fails.
  AddEmptyDataRetriever();
  AddEmptyDataRetriever();

  EXPECT_EQ(0u, registry()->enabled_extensions().size());

  auto web_app_info = std::make_unique<WebApplicationInfo>();
  web_app_info->start_url = AppUrl();
  web_app_info->title = base::UTF8ToUTF16(kAppTitle);
  web_app_info->description = base::UTF8ToUTF16(kAppDescription);
  web_app_info->scope = GURL(AppScope());
  SetAppIcon(web_app_info.get(), kIconSizeSmall, SK_ColorRED);

  auto web_app_info2 = std::make_unique<WebApplicationInfo>(*web_app_info);
  web_app_info2->title = base::UTF8ToUTF16(kAlternativeAppTitle);
  SetAppIcon(web_app_info2.get(), kIconSizeLarge, SK_ColorRED);
  web_app_info2->scope = GURL(kAppAlternativeScope);

  auto* provider = web_app::WebAppProviderBase::GetProviderBase(profile());
  web_app::AppId app_id;

  url_loader().SetNextLoadUrlResult(
      GURL("about:blank"), web_app::WebAppUrlLoader::Result::kUrlLoaded);
  url_loader().SetNextLoadUrlResult(
      AppUrl(), web_app::WebAppUrlLoader::Result::kRedirectedUrlLoaded);

  {
    base::RunLoop run_loop;

    provider->install_manager().InstallBookmarkAppFromSync(
        app_id, std::move(web_app_info),
        base::BindLambdaForTesting([&](const web_app::AppId& installed_app_id,
                                       web_app::InstallResultCode code) {
          EXPECT_EQ(web_app::InstallResultCode::kSuccessNewInstall, code);
          app_id = installed_app_id;
          run_loop.Quit();
        }));

    run_loop.Run();
  }

#if defined(OS_CHROMEOS)
  // On Chrome OS, sync always locally installs an app.
  const bool expect_locally_installed = true;
#else
  const bool expect_locally_installed = false;
#endif

  {
    EXPECT_EQ(1u, registry()->enabled_extensions().size());
    const Extension* extension =
        registry()->enabled_extensions().begin()->get();
    EXPECT_TRUE(extension->from_bookmark());
    EXPECT_EQ(kAppTitle, extension->name());
    EXPECT_EQ(kAppDescription, extension->description());
    EXPECT_EQ(AppUrl(), AppLaunchInfo::GetLaunchWebURL(extension));
    EXPECT_EQ(GURL(AppScope()), GetScopeURLFromBookmarkApp(extension));
    EXPECT_FALSE(extensions::IconsInfo::GetIconResource(
                     extension, kIconSizeSmall, ExtensionIconSet::MATCH_EXACTLY)
                     .empty());
    EXPECT_EQ(expect_locally_installed,
              BookmarkAppIsLocallyInstalled(profile(), extension));
  }

  url_loader().SetNextLoadUrlResult(
      GURL("about:blank"), web_app::WebAppUrlLoader::Result::kUrlLoaded);

  // On non-ChromeOS platforms is_locally_installed case depends on IsInstalled.
  // On ChromeOS, it always behaves as if app is installed.
  EXPECT_TRUE(provider->registrar().IsInstalled(app_id));

  AddEmptyDataRetriever();
  {
    base::RunLoop run_loop;

    provider->install_manager().InstallBookmarkAppFromSync(
        app_id, std::move(web_app_info2),
        base::BindLambdaForTesting([&](const web_app::AppId& installed_app_id,
                                       web_app::InstallResultCode code) {
          EXPECT_EQ(web_app::InstallResultCode::kSuccessAlreadyInstalled, code);
          EXPECT_EQ(app_id, installed_app_id);
          run_loop.Quit();
        }));

    run_loop.Run();
  }

  {
    // New fields from sync are not deployed as they are now managed by the
    // ManifestUpdateManager.
    EXPECT_EQ(1u, registry()->enabled_extensions().size());
    const Extension* extension =
        registry()->enabled_extensions().begin()->get();
    EXPECT_TRUE(extension->from_bookmark());
    EXPECT_EQ(kAppTitle, extension->name());
    EXPECT_EQ(kAppDescription, extension->description());
    EXPECT_EQ(AppUrl(), AppLaunchInfo::GetLaunchWebURL(extension));
    EXPECT_EQ(GURL(AppScope()), GetScopeURLFromBookmarkApp(extension));
    EXPECT_FALSE(extensions::IconsInfo::GetIconResource(
                     extension, kIconSizeSmall, ExtensionIconSet::MATCH_EXACTLY)
                     .empty());
    EXPECT_FALSE(extensions::IconsInfo::GetIconResource(
                     extension, kIconSizeLarge, ExtensionIconSet::MATCH_EXACTLY)
                     .empty());
    EXPECT_EQ(expect_locally_installed,
              BookmarkAppIsLocallyInstalled(profile(), extension));
  }
}

TEST_F(InstallManagerBookmarkAppTest, GetAppDetails) {
  EXPECT_EQ(std::string(), app_registrar()->GetAppShortName("unknown"));
  EXPECT_EQ(GURL(), app_registrar()->GetAppStartUrl("unknown"));
  const base::Optional<SkColor> theme_color = SK_ColorBLUE;  // 0xAABBCCDD;

  auto web_app_info = std::make_unique<WebApplicationInfo>();
  web_app_info->start_url = AppUrl();
  web_app_info->title = base::UTF8ToUTF16(kAppTitle);
  web_app_info->description = base::UTF8ToUTF16(kAppDescription);
  web_app_info->theme_color = theme_color;
  AddDataRetrieverWithRendererWebAppInfo(std::move(web_app_info),
                                         /*is_installable=*/false);

  const Extension* extension = InstallWebAppFromManifestWithFallback();
  EXPECT_EQ(kAppTitle, app_registrar()->GetAppShortName(extension->id()));
  EXPECT_EQ(kAppDescription,
            app_registrar()->GetAppDescription(extension->id()));
  EXPECT_EQ(theme_color, app_registrar()->GetAppThemeColor(extension->id()));
  EXPECT_EQ(AppUrl(), app_registrar()->GetAppStartUrl(extension->id()));
}

}  // namespace extensions
