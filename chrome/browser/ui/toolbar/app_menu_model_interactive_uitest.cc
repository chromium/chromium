// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <string>

#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/extensions/unpacked_installer.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/accelerator_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/web_applications/proto/web_app_install_state.pb.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/interaction/tracked_element_webcontents.h"
#include "chrome/test/interaction/webcontents_interaction_test_util.h"
#include "components/crx_file/id_util.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/performance_manager/public/features.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/supervised_user/core/common/features.h"
#include "components/supervised_user/test_support/supervised_user_signin_test_utils.h"
#include "components/webapps/browser/banners/app_banner_manager.h"
#include "components/webapps/browser/banners/installable_web_app_check_result.h"
#include "components/webapps/browser/banners/web_app_banner_data.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/common/extension_urls.h"
#include "extensions/test/test_extension_dir.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/accelerators/menu_label_accelerator_util.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/expect_call_in_scope.h"
#include "ui/base/interaction/interaction_sequence.h"
#include "ui/base/interaction/state_observer.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/ui/browser_commands_mac.h"
#include "chrome/browser/ui/fullscreen_util_mac.h"
#endif  // BUILDFLAG(IS_MAC)

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kPrimaryTabPageElementId);
}  // namespace

class AppMenuModelInteractiveTest : public InteractiveBrowserTest {
 public:
  AppMenuModelInteractiveTest() = default;
  ~AppMenuModelInteractiveTest() override = default;
  AppMenuModelInteractiveTest(const AppMenuModelInteractiveTest&) = delete;
  void operator=(const AppMenuModelInteractiveTest&) = delete;

  void SetUp() override {
    set_open_about_blank_on_browser_launch(true);
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    InteractiveBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    embedded_test_server()->StartAcceptingConnections();
  }

  void TearDownOnMainThread() override {
    EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
    InteractiveBrowserTest::TearDownOnMainThread();
  }

 protected:
  auto CheckIncognitoWindowOpened(const Browser* default_browser) {
    return Check(base::BindLambdaForTesting([default_browser]() {
      Browser* new_browser = nullptr;
      if (BrowserList::GetIncognitoBrowserCount() == 1) {
        EXPECT_EQ(2u, BrowserList::GetInstance()->size());
        for (Browser* browser : *BrowserList::GetInstance()) {
          if (browser != default_browser) {
            new_browser = browser;
            break;
          }
        }
        CHECK(new_browser);
      } else {
        new_browser = ui_test_utils::WaitForBrowserToOpen();
      }
      return new_browser->profile()->IsIncognitoProfile();
    }));
  }

  auto CheckGuestWindowOpened(const Browser* default_browser) {
    return Check(base::BindLambdaForTesting([default_browser]() {
      Browser* new_browser = nullptr;
      if (BrowserList::GetGuestBrowserCount() == 1) {
        EXPECT_EQ(2u, BrowserList::GetInstance()->size());
        for (Browser* browser : *BrowserList::GetInstance()) {
          if (browser != default_browser) {
            new_browser = browser;
            break;
          }
        }
        CHECK(new_browser);
      } else {
        new_browser = ui_test_utils::WaitForBrowserToOpen();
      }
      return new_browser->profile()->IsGuestSession();
    }));
  }
};

IN_PROC_BROWSER_TEST_F(AppMenuModelInteractiveTest, PerformanceNavigation) {
  RunTestSequence(
      InstrumentTab(kPrimaryTabPageElementId),
      PressButton(kToolbarAppMenuButtonElementId),
      SelectMenuItem(AppMenuModel::kMoreToolsMenuItem),
      SelectMenuItem(ToolsMenuModel::kPerformanceMenuItem),
      WaitForWebContentsNavigation(
          kPrimaryTabPageElementId,
          GURL(chrome::GetSettingsUrl(chrome::kPerformanceSubPage))));
}

IN_PROC_BROWSER_TEST_F(AppMenuModelInteractiveTest, IncognitoMenuItem) {
  RunTestSequence(PressButton(kToolbarAppMenuButtonElementId),
                  SelectMenuItem(AppMenuModel::kIncognitoMenuItem),
                  CheckIncognitoWindowOpened(browser()));
}

IN_PROC_BROWSER_TEST_F(AppMenuModelInteractiveTest, IncognitoAccelerator) {
  ui::Accelerator incognito_accelerator;
  chrome::AcceleratorProviderForBrowser(browser())->GetAcceleratorForCommandId(
      IDC_NEW_INCOGNITO_WINDOW, &incognito_accelerator);

  RunTestSequence(
      SendAccelerator(kToolbarAppMenuButtonElementId, incognito_accelerator),
      CheckIncognitoWindowOpened(browser()));
}

IN_PROC_BROWSER_TEST_F(AppMenuModelInteractiveTest,
                       CastSaveShareSubMenuItemText) {
  if (!media_router::MediaRouterEnabled(browser()->profile())) {
    GTEST_SKIP() << "The cast item only exists if cast is enabled.";
  }
  RunTestSequence(
      InstrumentTab(kPrimaryTabPageElementId),
      PressButton(kToolbarAppMenuButtonElementId),
      EnsurePresent(AppMenuModel::kSaveAndShareMenuItem),
      CheckViewProperty(
          AppMenuModel::kSaveAndShareMenuItem, &views::MenuItemView::title,
          l10n_util::GetStringUTF16(IDS_CAST_SAVE_AND_SHARE_MENU)),
      ScrollIntoView(AppMenuModel::kSaveAndShareMenuItem),
      SelectMenuItem(AppMenuModel::kSaveAndShareMenuItem),
      EnsurePresent(AppMenuModel::kCastTitleItem));
}

#if BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_F(AppMenuModelInteractiveTest,
                       ShowAppMenuInImmersiveFullscreen) {
  chrome::SetAlwaysShowToolbarInFullscreenForTesting(browser(), false);
  ASSERT_TRUE(!fullscreen_utils::IsAlwaysShowToolbarEnabled(browser()));
  ui_test_utils::ToggleFullscreenModeAndWait(browser());
  chrome::RevealToolbarForTesting(browser());
  RunTestSequence(WaitForShow(kToolbarAppMenuButtonElementId),
                  PressButton(kToolbarAppMenuButtonElementId),
                  WaitForShow(AppMenuModel::kMoreToolsMenuItem));
}
#endif  // BUILDFLAG(IS_MAC)

namespace {

enum ExtensionsTestMode {
  kDoNotCollapse,
  kCollapseNoExtensions,
  kCollapseWithExtensions
};

}  // namespace

class AppMenuModelExtensionsInteractiveTest
    : public AppMenuModelInteractiveTest,
      public testing::WithParamInterface<ExtensionsTestMode> {
 public:
  AppMenuModelExtensionsInteractiveTest() = default;
  ~AppMenuModelExtensionsInteractiveTest() override = default;

  bool MenuShouldCollapse() const {
    return GetParam() == ExtensionsTestMode::kCollapseNoExtensions;
  }

  void SetUp() override {
    scoped_feature_list_.InitWithFeatureState(
        features::kExtensionsCollapseMainMenu,
        GetParam() != ExtensionsTestMode::kDoNotCollapse);
    set_open_about_blank_on_browser_launch(true);
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    InteractiveBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    if (GetParam() == ExtensionsTestMode::kCollapseWithExtensions) {
      // Create and load a dummy extension.
      constexpr char kExtensionManifest[] = R"(
        {
          "name": "an extension",
          "version": "1.0",
          "manifest_version": 3,
          "action": {}
        }
      )";
      extensions::TestExtensionDir dir;
      dir.WriteManifest(kExtensionManifest);
      const auto id = crx_file::id_util::GenerateIdForPath(
          base::MakeAbsoluteFilePath(dir.UnpackedPath()));
      auto* const registry =
          extensions::ExtensionRegistry::Get(browser()->profile());
      CHECK(registry);
      extensions::TestExtensionRegistryObserver observer(registry, id);
      extensions::UnpackedInstaller::Create(browser()->profile())
          ->Load(dir.UnpackedPath());
      observer.WaitForExtensionLoaded();
    }
    AppMenuModelInteractiveTest::SetUpOnMainThread();
  }

 protected:
  base::HistogramTester histograms_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    ,
    AppMenuModelExtensionsInteractiveTest,
    testing::Values(ExtensionsTestMode::kDoNotCollapse,
                    ExtensionsTestMode::kCollapseNoExtensions,
                    ExtensionsTestMode::kCollapseWithExtensions),
    [](const testing::TestParamInfo<ExtensionsTestMode>& param) {
      switch (param.param) {
        case ExtensionsTestMode::kDoNotCollapse:
          return "DoNotCollapse";
        case ExtensionsTestMode::kCollapseNoExtensions:
          return "CollapseNoExtensions";
        case ExtensionsTestMode::kCollapseWithExtensions:
          return "CollapseWithExtensions";
      }
    });

// Test to confirm that the manage extensions menu item navigates when selected
// and emit histograms that it did so.
IN_PROC_BROWSER_TEST_P(AppMenuModelExtensionsInteractiveTest,
                       ManageExtensions) {
  if (MenuShouldCollapse()) {
    GTEST_SKIP()
        << "Manage extensions cannot be accessed through collapsed menu.";
  }

  RunTestSequence(
      InstrumentTab(kPrimaryTabPageElementId),
      PressButton(kToolbarAppMenuButtonElementId),
      SelectMenuItem(AppMenuModel::kExtensionsMenuItem),
      SelectMenuItem(ExtensionsMenuModel::kManageExtensionsMenuItem),
      WaitForWebContentsNavigation(kPrimaryTabPageElementId,
                                   GURL(chrome::kChromeUIExtensionsURL)));

  histograms_.ExpectTotalCount("WrenchMenu.TimeToAction.ManageExtensions", 1);
  histograms_.ExpectTotalCount("WrenchMenu.TimeToAction.VisitChromeWebStore",
                               0);
  histograms_.ExpectTotalCount("WrenchMenu.TimeToAction.FindExtensions", 0);
  histograms_.ExpectBucketCount("WrenchMenu.MenuAction",
                                MENU_ACTION_MANAGE_EXTENSIONS, 1);
  histograms_.ExpectBucketCount("WrenchMenu.MenuAction",
                                MENU_ACTION_VISIT_CHROME_WEB_STORE, 0);
  histograms_.ExpectBucketCount("WrenchMenu.MenuAction",
                                MENU_ACTION_FIND_EXTENSIONS, 0);
}

// Test to confirm that the visit Chrome Web Store menu item navigates to the
// correct chrome webstore URL when selected and emits histograms that it did
// so.
IN_PROC_BROWSER_TEST_P(AppMenuModelExtensionsInteractiveTest,
                       VisitChromeWebStore) {
  const bool collapse = MenuShouldCollapse();
  const GURL expected_webstore_launch_url =
      extension_urls::GetNewWebstoreLaunchURL();
  RunTestSequence(
      InstrumentTab(kPrimaryTabPageElementId),
      PressButton(kToolbarAppMenuButtonElementId),
      // If not collapsed, then the web store item is in the extensions submenu.
      If([collapse]() { return !collapse; },
         Then(SelectMenuItem(AppMenuModel::kExtensionsMenuItem))),
      SelectMenuItem(ExtensionsMenuModel::kVisitChromeWebStoreMenuItem),
      WaitForWebContentsNavigation(
          kPrimaryTabPageElementId,
          extension_urls::AppendUtmSource(expected_webstore_launch_url,
                                          extension_urls::kAppMenuUtmSource)));

  histograms_.ExpectTotalCount("WrenchMenu.TimeToAction.VisitChromeWebStore",
                               collapse ? 0 : 1);
  histograms_.ExpectTotalCount("WrenchMenu.TimeToAction.FindExtensions",
                               collapse ? 1 : 0);
  histograms_.ExpectTotalCount("WrenchMenu.TimeToAction.ManageExtensions", 0);
  histograms_.ExpectBucketCount("WrenchMenu.MenuAction",
                                MENU_ACTION_VISIT_CHROME_WEB_STORE,
                                collapse ? 0 : 1);
  histograms_.ExpectBucketCount("WrenchMenu.MenuAction",
                                MENU_ACTION_FIND_EXTENSIONS, collapse ? 1 : 0);
  histograms_.ExpectBucketCount("WrenchMenu.MenuAction",
                                MENU_ACTION_MANAGE_EXTENSIONS, 0);
}

class PasswordManagerMenuItemInteractiveTest
    : public AppMenuModelInteractiveTest,
      public testing::WithParamInterface<bool> {
 public:
  PasswordManagerMenuItemInteractiveTest() = default;
  PasswordManagerMenuItemInteractiveTest(
      const PasswordManagerMenuItemInteractiveTest&) = delete;
  void operator=(const PasswordManagerMenuItemInteractiveTest&) = delete;

  ~PasswordManagerMenuItemInteractiveTest() override = default;
};

IN_PROC_BROWSER_TEST_F(PasswordManagerMenuItemInteractiveTest,
                       PasswordManagerMenuItem) {
  base::HistogramTester histograms;

  RunTestSequence(InstrumentTab(kPrimaryTabPageElementId),
                  PressButton(kToolbarAppMenuButtonElementId),
                  SelectMenuItem(AppMenuModel::kPasswordAndAutofillMenuItem),
                  SelectMenuItem(AppMenuModel::kPasswordManagerMenuItem),
                  WaitForWebContentsNavigation(
                      kPrimaryTabPageElementId,
                      GURL("chrome://password-manager/passwords")));

  histograms.ExpectTotalCount("WrenchMenu.TimeToAction.ShowPasswordManager", 1);
  histograms.ExpectBucketCount("WrenchMenu.MenuAction",
                               MENU_ACTION_SHOW_PASSWORD_MANAGER, 1);
}

IN_PROC_BROWSER_TEST_F(PasswordManagerMenuItemInteractiveTest,
                       NoMenuItemOnPasswordManagerPage) {
  RunTestSequence(
      AddInstrumentedTab(kPrimaryTabPageElementId,
                         GURL("chrome://password-manager/passwords")),
      WaitForWebContentsReady(kPrimaryTabPageElementId,
                              GURL("chrome://password-manager/passwords")),
      PressButton(kToolbarAppMenuButtonElementId),
      EnsureNotPresent(AppMenuModel::kPasswordManagerMenuItem));
}

using ui::test::ObservationStateObserver;
using webapps::AppBannerManager;
using webapps::InstallableWebAppCheckResult;
using webapps::WebAppBannerData;

class AppBannerManagerInstallStateObserver
    : public ObservationStateObserver<InstallableWebAppCheckResult,
                                      AppBannerManager,
                                      AppBannerManager::Observer> {
 public:
  explicit AppBannerManagerInstallStateObserver(
      AppBannerManager* app_banner_manager)
      : ObservationStateObserver(app_banner_manager) {}
  ~AppBannerManagerInstallStateObserver() override = default;

  // ObservationStateObserver:
  InstallableWebAppCheckResult GetStateObserverInitialState() const override {
    return source()->GetInstallableWebAppCheckResult();
  }

  // AppBannerManager::Observer:
  void OnInstallableWebAppStatusUpdated(
      InstallableWebAppCheckResult result,
      const std::optional<WebAppBannerData>& data) override {
    OnStateObserverStateChanged(result);
  }
};

namespace {
DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(AppBannerManagerInstallStateObserver,
                                    kAppBannerManagerState);
}  // namespace

class UniversalInstallAppMenuModelInteractiveTest
    : public AppMenuModelInteractiveTest {
 public:
  UniversalInstallAppMenuModelInteractiveTest() = default;
  UniversalInstallAppMenuModelInteractiveTest(
      const UniversalInstallAppMenuModelInteractiveTest&) = delete;
  void operator=(const UniversalInstallAppMenuModelInteractiveTest&) = delete;
  ~UniversalInstallAppMenuModelInteractiveTest() override = default;

 protected:
  GURL GetNonInstallableAppUrl() {
    return embedded_test_server()->GetURL(
        "/banners/no_manifest_test_page.html");
  }

  GURL GetInstallableAppUrl() {
    return embedded_test_server()->GetURL("/banners/manifest_test_page.html");
  }

  GURL GetInvalidManifestParsingAppUrl() {
    return embedded_test_server()->GetURL(
        "/banners/invalid_manifest_test_page.html");
  }

  // If universal install is enabled, non installable sites (DIY apps) will have
  // a corresponding menu item entry for installation, as well as the default
  // install icon next to them.
  auto VerifyDiyAppMenuItemViews() {
    const ui::ImageModel icon_image = ui::ImageModel::FromVectorIcon(
        kInstallDesktopChromeRefreshIcon, ui::kColorMenuIcon,
        ui::SimpleMenuModel::kDefaultIconSize);
    return Steps(
        EnsurePresent(AppMenuModel::kInstallAppItem),
        CheckViewProperty(
            AppMenuModel::kInstallAppItem, &views::MenuItemView::title,
            l10n_util::GetStringUTF16(IDS_INSTALL_DIY_TO_OS_LAUNCH_SURFACE)),
        CheckViewProperty(AppMenuModel::kInstallAppItem,
                          &views::MenuItemView::GetIcon, icon_image));
  }

  AppBannerManager* GetManager() {
    return AppBannerManager::FromWebContents(
        browser()->tab_strip_model()->GetActiveWebContents());
  }

  // If universal install is enabled, the app icon should be shown in the menu
  // entry, so compare the middle most color as an indicator of similarity (as
  // is the case for most web_applications/ based tests). For the other case,
  // the default chrome refresh icon is a vector icon that is easy to compare,
  // so we do a 1:1 comparison.
  auto CompareIcons() {
    return base::BindLambdaForTesting([&](views::MenuItemView* item_view) {
      EXPECT_TRUE(item_view->GetIcon().IsImage());
      EXPECT_EQ(
          GetMidColorFromBitmap(item_view->GetIcon().GetImage().AsBitmap()),
          GetAppIconColorBasedOnBannerData());
    });
  }

  bool InstallNonLocallyInstalledApp(const GURL& url) {
    auto install_info =
        web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(url);
    install_info->title = u"Test App";
    install_info->user_display_mode =
        web_app::mojom::UserDisplayMode::kStandalone;
    web_app::WebAppInstallParams params;
    params.install_state =
        web_app::proto::InstallState::SUGGESTED_FROM_ANOTHER_DEVICE;
    params.add_to_applications_menu = false;
    params.add_to_desktop = false;
    params.add_to_quick_launch_bar = false;
    params.add_to_search = false;
    base::test::TestFuture<const webapps::AppId&, webapps::InstallResultCode>
        result;
    auto* provider = web_app::WebAppProvider::GetForTest(browser()->profile());
    provider->scheduler().InstallFromInfoWithParams(
        std::move(install_info), /*overwrite_existing_manifest_fields=*/true,
        webapps::WebappInstallSource::SYNC, result.GetCallback(), params);
    bool success = result.Wait();
    const webapps::AppId& app_id = result.Get<webapps::AppId>();
    EXPECT_EQ(provider->registrar_unsafe().GetInstallState(app_id),
              web_app::proto::SUGGESTED_FROM_ANOTHER_DEVICE);
    return success;
  }

 private:
  SkColor GetAppIconColorBasedOnBannerData() {
    std::optional<WebAppBannerData> banner_data =
        GetManager()->GetCurrentWebAppBannerData();

    if (banner_data->primary_icon.empty()) {
      return SK_ColorTRANSPARENT;
    }

    gfx::ImageSkia primary_icon =
        gfx::ImageSkia::CreateFrom1xBitmap(banner_data->primary_icon);
    gfx::ImageSkia resized_app_icon =
        gfx::ImageSkiaOperations::CreateResizedImage(
            primary_icon, skia::ImageOperations::RESIZE_BEST,
            gfx::Size(ui::SimpleMenuModel::kDefaultIconSize,
                      ui::SimpleMenuModel::kDefaultIconSize));

    return GetMidColorFromBitmap(*resized_app_icon.bitmap());
  }

  SkColor GetMidColorFromBitmap(const SkBitmap& bitmap) {
    return bitmap.getColor(bitmap.width() / 2, bitmap.height() / 2);
  }
};

IN_PROC_BROWSER_TEST_F(UniversalInstallAppMenuModelInteractiveTest,
                       DIYAppMenuWorksCorrectly) {
  RunTestSequence(
      InstrumentTab(kPrimaryTabPageElementId),
      ObserveState(kAppBannerManagerState, GetManager()),
      NavigateWebContents(kPrimaryTabPageElementId, GetNonInstallableAppUrl()),
      WaitForWebContentsReady(kPrimaryTabPageElementId),
      WaitForState(kAppBannerManagerState, InstallableWebAppCheckResult::kNo),
      PressButton(kToolbarAppMenuButtonElementId),
      EnsurePresent(AppMenuModel::kSaveAndShareMenuItem),
      ScrollIntoView(AppMenuModel::kSaveAndShareMenuItem),
      SelectMenuItem(AppMenuModel::kSaveAndShareMenuItem),
      VerifyDiyAppMenuItemViews());
}

IN_PROC_BROWSER_TEST_F(UniversalInstallAppMenuModelInteractiveTest,
                       DIYAppMenuWorksCorrectlyInvalidManifestParsingSites) {
  RunTestSequence(InstrumentTab(kPrimaryTabPageElementId),
                  ObserveState(kAppBannerManagerState, GetManager()),
                  NavigateWebContents(kPrimaryTabPageElementId,
                                      GetInvalidManifestParsingAppUrl()),
                  WaitForWebContentsReady(kPrimaryTabPageElementId),
                  // Invalid parsing currently leads the AppBannerManager to
                  // early exit the pipeline without modifying the default value
                  // of `InstallableWebAppCheckResult`, which is `kUnknown`.
                  // This should almost never trigger a wait, but it's better to
                  // be safe than introduce flakiness.
                  WaitForState(kAppBannerManagerState,
                               InstallableWebAppCheckResult::kUnknown),
                  PressButton(kToolbarAppMenuButtonElementId),
                  EnsurePresent(AppMenuModel::kSaveAndShareMenuItem),
                  SelectMenuItem(AppMenuModel::kSaveAndShareMenuItem),
                  VerifyDiyAppMenuItemViews());
}

IN_PROC_BROWSER_TEST_F(UniversalInstallAppMenuModelInteractiveTest,
                       InstallAppMenuWorksCorrectly) {
  RunTestSequence(
      InstrumentTab(kPrimaryTabPageElementId),
      ObserveState(kAppBannerManagerState, GetManager()),
      NavigateWebContents(kPrimaryTabPageElementId, GetInstallableAppUrl()),
      WaitForWebContentsReady(kPrimaryTabPageElementId),
      WaitForState(kAppBannerManagerState,
                   InstallableWebAppCheckResult::kYes_Promotable),
      PressButton(kToolbarAppMenuButtonElementId),
      EnsurePresent(AppMenuModel::kSaveAndShareMenuItem),
      ScrollIntoView(AppMenuModel::kSaveAndShareMenuItem),
      SelectMenuItem(AppMenuModel::kSaveAndShareMenuItem),
      EnsurePresent(AppMenuModel::kInstallAppItem),
      CheckViewProperty(
          AppMenuModel::kInstallAppItem, &views::MenuItemView::title,
          l10n_util::GetStringFUTF16(
              IDS_INSTALL_TO_OS_LAUNCH_SURFACE,
              ui::EscapeMenuLabelAmpersands(u"Manifest test app"))),
      WithView(AppMenuModel::kInstallAppItem, CompareIcons()));
}

IN_PROC_BROWSER_TEST_F(UniversalInstallAppMenuModelInteractiveTest,
                       InstallAppMenuShowsForNonLocallyInstalledApps) {
  EXPECT_TRUE(InstallNonLocallyInstalledApp(GetInstallableAppUrl()));
  RunTestSequence(
      InstrumentTab(kPrimaryTabPageElementId),
      ObserveState(kAppBannerManagerState, GetManager()),
      NavigateWebContents(kPrimaryTabPageElementId, GetInstallableAppUrl()),
      WaitForWebContentsReady(kPrimaryTabPageElementId),
      WaitForState(kAppBannerManagerState,
                   InstallableWebAppCheckResult::kYes_Promotable),
      PressButton(kToolbarAppMenuButtonElementId),
      EnsurePresent(AppMenuModel::kSaveAndShareMenuItem),
      ScrollIntoView(AppMenuModel::kSaveAndShareMenuItem),
      SelectMenuItem(AppMenuModel::kSaveAndShareMenuItem),
      EnsurePresent(AppMenuModel::kInstallAppItem));
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
class SupervisedUserAppMenuModelInteractiveTest
    : public AppMenuModelInteractiveTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    unused_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating([](content::BrowserContext* context) {
                  // Required to use IdentityTestEnvironmentAdaptor.
                  IdentityTestEnvironmentProfileAdaptor::
                      SetIdentityTestEnvironmentFactoriesOnBrowserContext(
                          context);
                }));
  }

 protected:
  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    IdentityTestEnvironmentProfileAdaptor::
        SetIdentityTestEnvironmentFactoriesOnBrowserContext(context);
  }

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    identity_test_environment_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(
            browser()->profile());
  }

  void SignIn(bool is_supervised_user) {
    AccountInfo account_info =
        identity_test_environment_adaptor_->identity_test_env()
            ->MakePrimaryAccountAvailable("name@gmail.com",
                                          signin::ConsentLevel::kSignin);
    supervised_user::UpdateSupervisionStatusForAccount(
        account_info,
        identity_test_environment_adaptor_->identity_test_env()
            ->identity_manager(),
        is_supervised_user);
  }

 private:
  base::CallbackListSubscription unused_subscription_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_environment_adaptor_;
};

IN_PROC_BROWSER_TEST_F(SupervisedUserAppMenuModelInteractiveTest,
                       OpenGuestSessionForSignedOutUser) {
  RunTestSequence(PressButton(kToolbarAppMenuButtonElementId),
                  SelectMenuItem(AppMenuModel::kProfileMenuItem),
                  SelectMenuItem(AppMenuModel::kProfileOpenGuestItem),
                  CheckGuestWindowOpened(browser()));
}

IN_PROC_BROWSER_TEST_F(SupervisedUserAppMenuModelInteractiveTest,
                       OpenGuestSessionForSignedInRegularUser) {
  SignIn(/*is_supervised_user=*/false);
  RunTestSequence(PressButton(kToolbarAppMenuButtonElementId),
                  SelectMenuItem(AppMenuModel::kProfileMenuItem),
                  SelectMenuItem(AppMenuModel::kProfileOpenGuestItem),
                  CheckGuestWindowOpened(browser()));
}

IN_PROC_BROWSER_TEST_F(SupervisedUserAppMenuModelInteractiveTest,
                       OpenGuestSessionForSignedInSupervisedUser) {
  SignIn(/*is_supervised_user=*/true);

  RunTestSequence(PressButton(kToolbarAppMenuButtonElementId),
                  SelectMenuItem(AppMenuModel::kProfileMenuItem),
                  EnsureNotPresent(AppMenuModel::kProfileOpenGuestItem));
}

#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
