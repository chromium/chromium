// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/manifest_update_manager.h"

#include <ios>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/containers/flat_tree.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ref.h"
#include "base/notreached.h"
#include "base/numerics/clamped_math.h"
#include "base/numerics/safe_conversions.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_browser_controller.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/ui/web_applications/web_app_dialogs.h"
#include "chrome/browser/web_applications/external_install_options.h"
#include "chrome/browser/web_applications/externally_managed_app_manager.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/os_integration/web_app_shortcut.h"
#include "chrome/browser/web_applications/proto/web_app_install_state.pb.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_sync_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_callback_app_identity.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_icon_generator.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_params.h"
#include "chrome/browser/web_applications/web_app_proto_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/services/app_service/public/cpp/file_handler.h"
#include "components/services/app_service/public/cpp/icon_info.h"
#include "components/services/app_service/public/cpp/protocol_handler_info.h"
#include "components/services/app_service/public/cpp/share_target.h"
#include "components/webapps/browser/features.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/browser/uninstall_result_code.h"
#include "components/webapps/common/web_app_id.h"
#include "components/webapps/services/web_app_origin_association/test/test_web_app_origin_association_fetcher.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/url_loader_interceptor.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_family.h"
#include "ui/views/test/dialog_test.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/dialog_delegate.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#include "chrome/browser/ash/system_web_apps/test_support/test_system_web_app_installation.h"
#include "chromeos/ash/components/standalone_browser/feature_refs.h"
#endif

#if BUILDFLAG(IS_LINUX)
#include "chrome/browser/web_applications/os_integration/web_app_file_handler_registration.h"
#endif

namespace web_app {

namespace {

// Note: When adding new tests and any bitmap resources they may require, please
// make sure the filename reflects the actual pixel size of the bitmap and that
// it includes a reference to the color of the bitmap. Avoid multi-color
// images unless they are necessary to test something. For example, if you need
// to add a blue square image with edge size 4096, the filename should be
// something like 4096x4096-blue.png and the RGB value of the blue color used
// should match SK_ColorBLUE. This ensures that the test can be validated just
// by reading the code and avoids looking up pixel colors in image editors or
// in defined constants with non-descriptive names.

constexpr char kUpdateHistogramName[] = "Webapp.Update.ManifestUpdateResult";

// DEPRECATED: Do not use in new tests (see note above).
constexpr char kInstallableIconList[] = R"(
  [
    {
      "src": "launcher-icon-4x.png",
      "sizes": "192x192",
      "type": "image/png"
    }
  ]
)";
constexpr SkColor kInstallableIconTopLeftColor =
    SkColorSetRGB(0x15, 0x96, 0xE0);

// DEPRECATED: Do not use in new tests (see note above).
constexpr char kAnotherInstallableIconList[] = R"(
  [
    {
      "src": "/banners/image-512px.png",
      "sizes": "512x512",
      "type": "image/png"
    }
  ]
)";
constexpr SkColor kAnotherInstallableIconTopLeftColor =
    SkColorSetRGB(0x5C, 0x5C, 0x5C);

constexpr char kAnotherShortcutsItemName[] = "Timeline";
constexpr char16_t kAnotherShortcutsItemName16[] = u"Timeline";
constexpr char kAnotherShortcutsItemUrl[] = "/shortcut";
constexpr char kAnotherShortcutsItemShortName[] = "H";
constexpr char kAnotherShortcutsItemDescription[] = "Navigate home";
constexpr char kAnotherIconSrc[] = "/banners/launcher-icon-4x.png";
constexpr int kAnotherIconSize = 192;

constexpr char kShortcutsItem[] = R"(
  [
    {
      "name": "Home",
      "short_name": "HM",
      "description": "Go home",
      "url": ".",
      "icons": [
        {
          "src": "/banners/image-512px.png",
          "sizes": "512x512",
          "type": "image/png"
        }
      ]
    }
  ]
)";

constexpr char kShortcutsItems[] = R"(
  [
    {
      "name": "Home",
      "short_name": "HM",
      "description": "Go home",
      "url": ".",
      "icons": [
        {
          "src": "/banners/image-512px.png",
          "sizes": "512x512",
          "type": "image/png"
        }
      ]
    },
    {
      "name": "Settings",
      "short_name": "ST",
      "description": "App settings",
      "url": "/settings",
      "icons": [
        {
          "src": "launcher-icon-4x.png",
          "sizes": "192x192",
          "type": "image/png"
        }
      ]
    }
  ]
)";

// Two 'unimportant' icon sizes, smaller than the smallest generated icon on all
// platforms. This simplifies creating test expectations as it avoids having
// unimportant icons affecting generated icons, which inherit their bits from
// the next size up when left unspecified by the manifest.
constexpr int kUnimportantIconSize = 2;
constexpr int kUnimportantIconSize2 = 4;

// An icon size guaranteed to meet the installability requirements, and on all
// platforms is larger than both the install icon and launcher icon.
constexpr int kInstallabilityIconSize = 512;
// The minimum icon size to meet the installability criteria.
constexpr int kInstallMinSize = 192;

// Platform definitions for evaluating rules of which size to look for in a
// shortcut.
constexpr int kAll = 0;
constexpr int kWin = 1;     // Windows-only rule.
constexpr int kMac = 2;     // Mac-only rule.
constexpr int kNotWin = 3;  // All platforms except Windows.
constexpr int kNotMac = 4;  // All platforms except Mac.

ManifestUpdateManager& GetManifestUpdateManager(Profile* profile) {
  return WebAppProvider::GetForTest(profile)->manifest_update_manager();
}

// Utility class to wait for a manifest update to finish and get a
// ManifestUpdateResult back.
class UpdateCheckResultAwaiter {
 public:
  explicit UpdateCheckResultAwaiter(const GURL& url) : url_(url) {
    SetCallback();
  }

  void SetCallback() {
    ManifestUpdateManager::SetResultCallbackForTesting(base::BindOnce(
        &UpdateCheckResultAwaiter::OnResult, base::Unretained(this)));
  }

  ManifestUpdateResult AwaitNextResult() && {
    run_loop_.Run();
    return *result_;
  }

  void OnResult(const GURL& url, ManifestUpdateResult result) {
    if (url != url_) {
      SetCallback();
      return;
    }
    result_ = result;
    run_loop_.Quit();
  }

 private:
  const GURL url_;
  base::RunLoop run_loop_;
  std::optional<ManifestUpdateResult> result_;
};

// Utility class to wait for WebContentsObserver to trigger a DidFinishLoad.
class DidFinishLoadObserver : public content::WebContentsObserver {
 public:
  explicit DidFinishLoadObserver(content::WebContents* contents,
                                 const GURL& url)
      : content::WebContentsObserver(contents), expected_url_(url) {}

  bool AwaitCorrectPageLoaded() {
    on_load_run_loop_finished_.Run();
    base::test::TestFuture<void> future;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, future.GetCallback());
    EXPECT_TRUE(future.Wait());
    return correct_page_loaded_;
  }

 private:
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override {
    if (!validated_url.is_valid() || validated_url != expected_url_) {
      return;
    }

    correct_page_loaded_ = true;
    on_load_run_loop_finished_.Quit();
  }

  base::RunLoop on_load_run_loop_finished_;
  const GURL expected_url_;
  bool correct_page_loaded_ = false;
};

}  // namespace

class ManifestUpdateManagerBrowserTest : public WebAppBrowserTestBase {
 public:
  ManifestUpdateManagerBrowserTest()
      : update_dialog_scope_(SetIdentityUpdateDialogActionForTesting(
            AppIdentityUpdate::kSkipped)) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    scoped_feature_list_.InitWithFeatures(
        {}, ash::standalone_browser::GetFeatureRefs());
#endif
  }
  ManifestUpdateManagerBrowserTest(const ManifestUpdateManagerBrowserTest&) =
      delete;
  ManifestUpdateManagerBrowserTest& operator=(
      const ManifestUpdateManagerBrowserTest&) = delete;

  ~ManifestUpdateManagerBrowserTest() override = default;

  void SetUp() override {
    http_server_.AddDefaultHandlers(GetChromeTestDataDir());
    http_server_.RegisterRequestHandler(base::BindRepeating(
        &ManifestUpdateManagerBrowserTest::RequestHandlerOverride,
        base::Unretained(this)));
    ASSERT_TRUE(http_server_.Start());
    // Suppress globally to avoid OS hooks deployed for system web app during
    // WebAppProvider setup.
    WebAppBrowserTestBase::SetUp();
  }

  void SetUpOnMainThread() override {
    // Cannot construct RunLoop in constructor due to threading restrictions.
    shortcut_run_loop_.emplace();
    test::WaitUntilWebAppProviderAndSubsystemsReady(
        WebAppProvider::GetForTest(browser()->profile()));
  }

  void OnShortcutInfoRetrieved(std::unique_ptr<ShortcutInfo> shortcut_info) {
    updated_colors_ = {};
    if (shortcut_info) {
      gfx::ImageFamily::const_iterator it;
      // Loop through each size in the ImgFamily and add it to the color map.
      for (it = shortcut_info->favicon.begin();
           it != shortcut_info->favicon.end(); ++it) {
        updated_colors_.emplace_back(it->Size().width(),
                                     it->AsBitmap().getColor(0, 0));
      }
    }
    shortcut_run_loop_->Quit();
  }

  bool RuleAppliesToThisOS(int os, int size) {
#if BUILDFLAG(IS_WIN)
    return os == kWin || os == kNotMac || os == kAll;
#elif BUILDFLAG(IS_MAC)
    // The Mac code in generating these icons doesn't write a size 48 icon. See
    // chrome/browser/web_applications/web_app_icon_generator.h's
    // `kInstallIconSize`. Skip it.
    if (size == icon_size::k48)
      return false;
    return os == kMac || os == kNotWin || os == kAll;
#else
    return os == kNotWin || os == kNotMac || os == kAll;
#endif
  }

  // Confirms that the platform shortcut for this app (with id `app_id`)
  // contains an icon family that matches exactly the color specified in
  // `expectations`. The latter is a vector mapping (size, os) to an SK_Color
  // value.
  void ConfirmShortcutColors(
      const webapps::AppId& app_id,
      const std::vector<std::pair<std::pair<int, int>, SkColor>>&
          expectations) {
    GetProvider().os_integration_manager().GetShortcutInfoForAppFromRegistrar(
        app_id, base::BindOnce(
                    &ManifestUpdateManagerBrowserTest::OnShortcutInfoRetrieved,
                    base::Unretained(this)));
    shortcut_run_loop_->Run();

    std::vector<std::pair<int /* size */, SkColor>>::const_iterator
        actual_size_to_color_it = updated_colors_.begin();
    for (auto expected_size_to_color_it : expectations) {
      int expected_size = expected_size_to_color_it.first.first;
      int platform = expected_size_to_color_it.first.second;
      SkColor expected_color = expected_size_to_color_it.second;

      if (!RuleAppliesToThisOS(platform, expected_size)) {
        SCOPED_TRACE(::testing::Message() << "Skipping size " << expected_size
                                          << " (wrong os: " << platform << ")");
        continue;
      }

      int actual_size = actual_size_to_color_it->first;
      SkColor actual_color = actual_size_to_color_it->second;
      EXPECT_EQ(expected_size, actual_size);
      EXPECT_EQ(expected_color, actual_color)
          << "Size " << expected_size << ": Expecting ARGB " << std::hex
          << expected_color << " but found " << std::hex << actual_color;
      ++actual_size_to_color_it;
    }

    ASSERT_EQ(updated_colors_.end(), actual_size_to_color_it)
        << "Unexpected size found in shortcut: "
        << actual_size_to_color_it->first << ": ARGB " << std::hex
        << actual_size_to_color_it->second;
  }

  std::unique_ptr<net::test_server::HttpResponse> RequestHandlerOverride(
      const net::test_server::HttpRequest& request) {
    if (request_override_)
      return request_override_.Run(request);
    return nullptr;
  }

  void OverrideManifest(const char* manifest_template,
                        const std::vector<std::string>& substitutions) {
    std::string content = base::ReplaceStringPlaceholders(
        manifest_template, substitutions, nullptr);
    request_override_ = base::BindLambdaForTesting(
        [this, content = std::move(content)](
            const net::test_server::HttpRequest& request)
            -> std::unique_ptr<net::test_server::HttpResponse> {
          if (request.GetURL() != GetManifestURL())
            return nullptr;
          auto http_response =
              std::make_unique<net::test_server::BasicHttpResponse>();
          http_response->set_code(net::HTTP_OK);
          http_response->set_content(content);
          return std::move(http_response);
        });
  }

  GURL GetAppURL() const {
    return http_server_.GetURL("/banners/manifest_test_page.html");
  }

  GURL GetManifestURL() const {
    return http_server_.GetURL("/banners/manifest.json");
  }

  GURL GetAppURLWithoutManifest() const {
    return http_server_.GetURL("/banners/no_manifest_test_page.html");
  }

  // Mimics the Create Shortcut flow from the three dot overflow menu.
  webapps::AppId InstallWebAppWithoutManifest() {
    GURL app_url = GetAppURLWithoutManifest();
    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), app_url));

    webapps::AppId app_id;
    base::test::TestFuture<const webapps::AppId&, webapps::InstallResultCode>
        install_future;
    GetProvider().scheduler().FetchManifestAndInstall(
        webapps::WebappInstallSource::MENU_CREATE_SHORTCUT,
        browser()->tab_strip_model()->GetActiveWebContents()->GetWeakPtr(),
        base::BindOnce(test::TestAcceptDialogCallback),
        install_future.GetCallback(),
        FallbackBehavior::kAllowFallbackDataAlways);
    EXPECT_EQ(install_future.Get<webapps::InstallResultCode>(),
              webapps::InstallResultCode::kSuccessNewInstall);
    return install_future.Get<webapps::AppId>();
  }

  webapps::AppId InstallWebApp() {
    GURL app_url = GetAppURL();
    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), app_url));

    webapps::AppId app_id;
    base::RunLoop run_loop;
    GetProvider().scheduler().FetchManifestAndInstall(
        webapps::WebappInstallSource::OMNIBOX_INSTALL_ICON,
        browser()->tab_strip_model()->GetActiveWebContents()->GetWeakPtr(),
        base::BindOnce(test::TestAcceptDialogCallback),
        base::BindLambdaForTesting([&](const webapps::AppId& new_app_id,
                                       webapps::InstallResultCode code) {
          EXPECT_EQ(code, webapps::InstallResultCode::kSuccessNewInstall);
          app_id = new_app_id;
          run_loop.Quit();
        }),
        FallbackBehavior::kAllowFallbackDataAlways);

    run_loop.Run();
    return app_id;
  }

  webapps::AppId InstallOemWebApp() {
    const GURL app_url = GetAppURL();
    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), app_url));

    webapps::AppId app_id;
    base::RunLoop run_loop;
    GetProvider().scheduler().FetchManifestAndInstall(
        webapps::WebappInstallSource::PRELOADED_OEM,
        browser()->tab_strip_model()->GetActiveWebContents()->GetWeakPtr(),
        base::BindOnce(test::TestAcceptDialogCallback),
        base::BindLambdaForTesting([&](const webapps::AppId& new_app_id,
                                       webapps::InstallResultCode code) {
          EXPECT_EQ(code, webapps::InstallResultCode::kSuccessNewInstall);
          app_id = new_app_id;
          run_loop.Quit();
        }),
        FallbackBehavior::kAllowFallbackDataAlways);

    run_loop.Run();
    return app_id;
  }

  webapps::AppId InstallDefaultApp() {
    const GURL app_url = GetAppURL();
    base::RunLoop run_loop;
    ExternalInstallOptions install_options(
        app_url, mojom::UserDisplayMode::kStandalone,
        ExternalInstallSource::kInternalDefault);
    install_options.add_to_applications_menu = false;
    install_options.add_to_desktop = false;
    install_options.add_to_quick_launch_bar = false;
    install_options.install_placeholder = true;
    GetProvider().externally_managed_app_manager().Install(
        std::move(install_options),
        base::BindLambdaForTesting(
            [&](const GURL& installed_app_url,
                ExternallyManagedAppManager::InstallResult result) {
              EXPECT_EQ(installed_app_url, app_url);
              EXPECT_EQ(result.code,
                        webapps::InstallResultCode::kSuccessNewInstall);
              run_loop.Quit();
            }));
    run_loop.Run();
    return GetProvider()
        .registrar_unsafe()
        .LookupExternalAppId(app_url)
        .value();
  }

  webapps::AppId InstallPolicyApp() {
    const GURL app_url = GetAppURL();
    base::RunLoop run_loop;
    ExternalInstallOptions install_options(
        app_url, mojom::UserDisplayMode::kStandalone,
        ExternalInstallSource::kExternalPolicy);
    install_options.add_to_applications_menu = false;
    install_options.add_to_desktop = false;
    install_options.add_to_quick_launch_bar = false;
    install_options.install_placeholder = true;
    GetProvider().externally_managed_app_manager().Install(
        std::move(install_options),
        base::BindLambdaForTesting(
            [&](const GURL& installed_app_url,
                ExternallyManagedAppManager::InstallResult result) {
              EXPECT_EQ(installed_app_url, app_url);
              EXPECT_EQ(result.code,
                        webapps::InstallResultCode::kSuccessNewInstall);
              run_loop.Quit();
            }));
    run_loop.Run();
    return GetProvider()
        .registrar_unsafe()
        .LookupExternalAppId(app_url)
        .value();
  }

  webapps::AppId InstallKioskApp() {
    const GURL app_url = GetAppURL();
    base::RunLoop run_loop;
    ExternalInstallOptions install_options(app_url,
                                           mojom::UserDisplayMode::kStandalone,
                                           ExternalInstallSource::kKiosk);
    install_options.install_placeholder = true;
    GetProvider().externally_managed_app_manager().Install(
        std::move(install_options),
        base::BindLambdaForTesting(
            [&](const GURL& installed_app_url,
                ExternallyManagedAppManager::InstallResult result) {
              EXPECT_EQ(installed_app_url, app_url);
              EXPECT_EQ(result.code,
                        webapps::InstallResultCode::kSuccessNewInstall);
              run_loop.Quit();
            }));
    run_loop.Run();
    return GetProvider()
        .registrar_unsafe()
        .LookupExternalAppId(app_url)
        .value();
  }

  webapps::AppId InstallWebAppFromSync(const GURL& start_url) {
    const webapps::AppId app_id =
        GenerateAppId(/*manifest_id=*/std::nullopt, start_url);

    std::vector<std::unique_ptr<WebApp>> add_synced_apps_data;
    {
      auto synced_specifics_data = std::make_unique<WebApp>(app_id);
      synced_specifics_data->SetStartUrl(start_url);

      synced_specifics_data->AddSource(WebAppManagement::kSync);
      synced_specifics_data->SetUserDisplayMode(
          mojom::UserDisplayMode::kBrowser);
      synced_specifics_data->SetName("Name From Sync");

      sync_pb::WebAppSpecifics sync_proto;
      sync_proto.set_name("Name From Sync");
      sync_proto.set_theme_color(SK_ColorMAGENTA);
      sync_proto.set_scope(GURL("https://example.com/sync_scope").spec());

      apps::IconInfo apps_icon_info = CreateIconInfo(
          /*icon_base_url=*/start_url, IconPurpose::MONOCHROME, 64);
      sync_proto.mutable_icon_infos()->Add(
          AppIconInfoToSyncProto(std::move(apps_icon_info)));

      synced_specifics_data->SetSyncProto(std::move(sync_proto));

      add_synced_apps_data.push_back(std::move(synced_specifics_data));
    }

    WebAppTestInstallObserver observer(browser()->profile());

    GetProvider().sync_bridge_unsafe().set_disable_checks_for_testing(true);

    sync_bridge_test_utils::AddApps(GetProvider().sync_bridge_unsafe(),
                                    add_synced_apps_data);

    return observer.BeginListeningAndWait({app_id});
  }

  // Simulates what AppLauncherHandler::HandleInstallAppLocally() does.
  void InstallAppLocally(const WebApp* web_app) {
    base::test::TestFuture<void> future;
    GetProvider().scheduler().InstallAppLocally(web_app->app_id(),
                                                future.GetCallback());
    EXPECT_TRUE(future.Wait());
  }

  void SetTimeOverride(base::Time time_override) {
    GetManifestUpdateManager(browser()->profile())
        .set_time_override_for_testing(time_override);
  }

  ManifestUpdateResult GetResultAfterPageLoad(const GURL& url) {
    UpdateCheckResultAwaiter awaiter(url);
    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    return std::move(awaiter).AwaitNextResult();
  }

  WebAppProvider& GetProvider() {
    return *WebAppProvider::GetForTest(browser()->profile());
  }

  SkColor ReadAppIconPixel(const webapps::AppId& app_id,
                           SquareSizePx size,
                           int x = 0,
                           int y = 0) {
    return IconManagerReadAppIconPixel(
        WebAppProvider::GetForTest(browser()->profile())->icon_manager(),
        app_id, size, x, y);
  }

  void AcceptAppIdentityUpdateDialogForTesting() {
    update_dialog_scope_ =
        SetIdentityUpdateDialogActionForTesting(AppIdentityUpdate::kAllowed);
  }

  void ResetAutomatedAppIdentityUpdateDialogBehavior() {
    update_dialog_scope_ =
        SetIdentityUpdateDialogActionForTesting(std::nullopt);
  }

 protected:
  net::EmbeddedTestServer::HandleRequestCallback request_override_;

  base::HistogramTester histogram_tester_;

  net::EmbeddedTestServer http_server_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::optional<base::RunLoop> shortcut_run_loop_;
  // A vector mapping image sizes to shortcut colors. Note that the top left
  // pixel color for each size is used as the representation color for that
  // size, even if the image is multi-colored.
  std::vector<std::pair<int, SkColor>> updated_colors_;
  base::AutoReset<std::optional<AppIdentityUpdate>> update_dialog_scope_;
};

enum class UpdateDialogParam {
  kDisabled = 0,
  kEnabled = 1,
};

class ManifestUpdateManagerBrowserTest_UpdateDialog
    : public ManifestUpdateManagerBrowserTest,
      public testing::WithParamInterface<UpdateDialogParam> {
 public:
  ManifestUpdateManagerBrowserTest_UpdateDialog() {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;
    if (IsUpdateDialogEnabled()) {
      enabled_features.push_back(features::kPwaUpdateDialogForIcon);
    } else {
      disabled_features.push_back(features::kPwaUpdateDialogForIcon);
    }
    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

  bool IsUpdateDialogEnabled() const {
    return GetParam() == UpdateDialogParam::kEnabled;
  }

  static std::string ParamToString(
      testing::TestParamInfo<UpdateDialogParam> param) {
    switch (param.param) {
      case UpdateDialogParam::kDisabled:
        return "UpdateDialogDisabled";
      case UpdateDialogParam::kEnabled:
        return "UpdateDialogEnabled";
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       CheckOutOfScopeNavigation) {
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kNoAppInScope);

  webapps::AppId app_id = InstallWebApp();

  EXPECT_EQ(GetResultAfterPageLoad(GURL("http://example.org")),
            ManifestUpdateResult::kNoAppInScope);

  histogram_tester_.ExpectTotalCount(kUpdateHistogramName, 0);
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest, CheckIsThrottled) {
  constexpr base::TimeDelta kDelayBetweenChecks = base::Days(1);
  base::Time time_override = base::Time::UnixEpoch();
  SetTimeOverride(time_override);

  webapps::AppId app_id = InstallWebApp();
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpToDate);

  time_override += kDelayBetweenChecks / 2;
  SetTimeOverride(time_override);
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kThrottled);

  time_override += kDelayBetweenChecks;
  SetTimeOverride(time_override);
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpToDate);

  time_override += kDelayBetweenChecks / 2;
  SetTimeOverride(time_override);
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kThrottled);

  time_override += kDelayBetweenChecks * 2;
  SetTimeOverride(time_override);
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpToDate);

  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kThrottled, 2);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpToDate, 3);
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       CheckCancelledByWebContentsDestroyed) {
  webapps::AppId app_id = InstallWebApp();
  GetManifestUpdateManager(browser()->profile())
      .hang_update_checks_for_testing();

  GURL url = GetAppURL();
  UpdateCheckResultAwaiter awaiter(url);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  chrome::CloseTab(browser());
  EXPECT_EQ(std::move(awaiter).AwaitNextResult(),
            ManifestUpdateResult::kWebContentsDestroyed);
  histogram_tester_.ExpectBucketCount(
      kUpdateHistogramName, ManifestUpdateResult::kWebContentsDestroyed, 1);
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       CheckCancelledByAppUninstalled) {
  webapps::AppId app_id = InstallWebApp();
  GetManifestUpdateManager(browser()->profile())
      .hang_update_checks_for_testing();

  GURL url = GetAppURL();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  base::RunLoop run_loop;
  UpdateCheckResultAwaiter awaiter(url);
  GetProvider().scheduler().RemoveUserUninstallableManagements(
      app_id, webapps::WebappUninstallSource::kAppMenu,
      base::BindLambdaForTesting([&](webapps::UninstallResultCode code) {
        EXPECT_EQ(code, webapps::UninstallResultCode::kAppRemoved);
        run_loop.Quit();
      }));

  EXPECT_EQ(std::move(awaiter).AwaitNextResult(),
            ManifestUpdateResult::kAppUninstalling);

  run_loop.Run();
  histogram_tester_.ExpectBucketCount(
      kUpdateHistogramName, ManifestUpdateResult::kAppUninstalling, 1);
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       TriggersAfterLoadingNewManifestUrl) {
  // Install an app with no manifest, trigger an update by navigation.
  GURL no_manifest_url = GetAppURLWithoutManifest();
  const webapps::AppId app_id = InstallWebAppWithoutManifest();
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  UpdateCheckResultAwaiter result_awaiter(no_manifest_url);
  DidFinishLoadObserver load_observer(web_contents, no_manifest_url);
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), no_manifest_url));
  EXPECT_TRUE(load_observer.AwaitCorrectPageLoaded());
  EXPECT_TRUE(GetManifestUpdateManager(browser()->profile())
                  .IsAppPendingPageAndManifestUrlLoadForTesting(app_id));

  // Inject new manifest into the page once DidFinishLoad() is triggered. This
  // should start the manifest checking command without the need for a refresh.
  EXPECT_TRUE(content::ExecJs(
      web_contents,
      "addManifestLinkTag('/banners/manifest_for_no_manifest_page.json')"));
  GURL newly_loaded_manifest_url =
      http_server_.GetURL("/banners/manifest_for_no_manifest_page.json");
  EXPECT_EQ(ManifestUpdateResult::kAppUpdated,
            std::move(result_awaiter).AwaitNextResult());
  EXPECT_EQ(GetProvider().registrar_unsafe().GetAppManifestUrl(app_id),
            newly_loaded_manifest_url);
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       CheckIgnoresWhitespaceDifferences) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "icons": $1
      $2
    }
  )";
  OverrideManifest(kManifestTemplate, {kInstallableIconList, ""});
  webapps::AppId app_id = InstallWebApp();

  OverrideManifest(kManifestTemplate, {kInstallableIconList, "\n\n\n\n"});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpToDate);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpToDate, 1);
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       CheckIgnoresNameChange) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "$1",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "icons": $2
    }
  )";
  OverrideManifest(kManifestTemplate, {"Test app name", kInstallableIconList});
  webapps::AppId app_id = InstallWebApp();

  OverrideManifest(kManifestTemplate,
                   {"Different app name", kInstallableIconList});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpToDate);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpToDate, 1);
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       CheckIgnoresShortNameChange) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "short_name": "$1",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "icons": $2
    }
  )";
  OverrideManifest(kManifestTemplate,
                   {"Short test app name", kInstallableIconList});
  webapps::AppId app_id = InstallWebApp();

  OverrideManifest(kManifestTemplate,
                   {"Different short test app name", kInstallableIconList});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpToDate);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpToDate, 1);
}

// TODO(crbug.com/40231087): Test is flaky.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_CheckNameUpdatesForDefaultApps \
  DISABLED_CheckNameUpdatesForDefaultApps
#else
#define MAYBE_CheckNameUpdatesForDefaultApps CheckNameUpdatesForDefaultApps
#endif
IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       MAYBE_CheckNameUpdatesForDefaultApps) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "$1",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "icons": $2
    }
  )";
  OverrideManifest(kManifestTemplate, {"Test app name", kInstallableIconList});
  webapps::AppId app_id = InstallDefaultApp();

  OverrideManifest(kManifestTemplate,
                   {"Different app name", kInstallableIconList});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpdated);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);
  EXPECT_EQ(GetProvider().registrar_unsafe().GetAppShortName(app_id),
            "Different app name");
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       CheckIgnoresStartUrlChange) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": "$1",
      "scope": "/",
      "display": "standalone",
      "icons": $2
    }
  )";
  OverrideManifest(kManifestTemplate, {"a.html", kInstallableIconList});
  webapps::AppId app_id = InstallWebApp();

  OverrideManifest(kManifestTemplate, {"b.html", kInstallableIconList});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppIdMismatch);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppIdMismatch, 1);
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       CheckIgnoresNoManifestChange) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "icons": $1
    }
  )";
  OverrideManifest(kManifestTemplate, {kInstallableIconList});
  webapps::AppId app_id = InstallWebApp();
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpToDate);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpToDate, 1);
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       CheckIgnoresInvalidManifest) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "icons": $1
      $2
    }
  )";
  OverrideManifest(kManifestTemplate, {kInstallableIconList, ""});
  webapps::AppId app_id = InstallWebApp();
  OverrideManifest(kManifestTemplate, {kInstallableIconList,
                                       "invalid manifest syntax !@#$%^*&()"});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppNotEligible);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppNotEligible, 1);
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       CheckIgnoresNonLocalApps) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "icons": $1,
      "theme_color": "$2"
    }
  )";
  OverrideManifest(kManifestTemplate, {kInstallableIconList, "blue"});
  webapps::AppId app_id = InstallWebApp();

  // TODO(crbug.com/41490924): Instead of doing this, just install the
  // app from sync in the first place to have it 'not locally installed' in the
  // beginning.
  GetProvider().sync_bridge_unsafe().SetAppNotLocallyInstalledForTesting(
      app_id);
  EXPECT_EQ(GetProvider().registrar_unsafe().GetInstallState(app_id),
            proto::SUGGESTED_FROM_ANOTHER_DEVICE);

  OverrideManifest(kManifestTemplate, {kInstallableIconList, "red"});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kNoAppInScope);
  histogram_tester_.ExpectTotalCount(kUpdateHistogramName, 0);
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       CheckIgnoresPlaceholderApps) {
  // Set up app URL to redirect to force placeholder app to install.
  const GURL app_url = GetAppURL();
  request_override_ = base::BindLambdaForTesting(
      [&app_url](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        if (request.GetURL() != app_url)
          return nullptr;
        auto http_response =
            std::make_unique<net::test_server::BasicHttpResponse>();
        http_response->set_code(net::HTTP_TEMPORARY_REDIRECT);
        http_response->AddCustomHeader(
            "Location", "http://other-origin.com/defaultresponse");
        http_response->set_content("redirect page");
        return std::move(http_response);
      });

  // Install via ExternallyManagedAppManager, the redirect to a different origin
  // should cause it to install a placeholder app.
  webapps::AppId app_id = InstallPolicyApp();
  EXPECT_TRUE(GetProvider().registrar_unsafe().IsPlaceholderApp(
      app_id, WebAppManagement::kPolicy));

  // Manifest updating should ignore non-redirect loads for placeholder apps
  // because the ExternallyManagedAppManager will handle these.
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "icons": $1
    }
  )";
  OverrideManifest(kManifestTemplate, {kInstallableIconList});
  EXPECT_EQ(GetResultAfterPageLoad(app_url),
            ManifestUpdateResult::kAppIsPlaceholder);
  histogram_tester_.ExpectBucketCount(
      kUpdateHistogramName, ManifestUpdateResult::kAppIsPlaceholder, 1);
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       CheckIgnoresPlaceholderAppsForKiosk) {
  // Set up app URL to redirect to force placeholder app to install.
  const GURL app_url = GetAppURL();
  request_override_ = base::BindLambdaForTesting(
      [&app_url](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        if (request.GetURL() != app_url)
          return nullptr;
        auto http_response =
            std::make_unique<net::test_server::BasicHttpResponse>();
        http_response->set_code(net::HTTP_TEMPORARY_REDIRECT);
        http_response->AddCustomHeader(
            "Location", "http://other-origin.com/defaultresponse");
        http_response->set_content("redirect page");
        return std::move(http_response);
      });

  // Install via ExternallyManagedAppManager, the redirect to a different origin
  // should cause it to install a placeholder app.
  webapps::AppId app_id = InstallKioskApp();
  EXPECT_TRUE(GetProvider().registrar_unsafe().IsPlaceholderApp(
      app_id, WebAppManagement::kKiosk));

  // Manifest updating should ignore non-redirect loads for placeholder apps
  // because the ExternallyManagedAppManager will handle these.
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "icons": $1
    }
  )";
  OverrideManifest(kManifestTemplate, {kInstallableIconList});
  EXPECT_EQ(GetResultAfterPageLoad(app_url),
            ManifestUpdateResult::kAppIsPlaceholder);
  histogram_tester_.ExpectBucketCount(
      kUpdateHistogramName, ManifestUpdateResult::kAppIsPlaceholder, 1);
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       CheckFindsThemeColorChange) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "icons": $1,
      "theme_color": "$2"
    }
  )";
  OverrideManifest(kManifestTemplate, {kInstallableIconList, "blue"});
  webapps::AppId app_id = InstallWebApp();
  EXPECT_EQ(GetProvider().registrar_unsafe().GetAppThemeColor(app_id),
            SK_ColorBLUE);

  // Check that OnWebAppInstalled and OnWebAppWillBeUninstalled are not called
  // if in-place web app update happens.
  WebAppInstallManagerObserverAdapter install_observer(
      &GetProvider().install_manager());
  install_observer.SetWebAppInstalledDelegate(base::BindLambdaForTesting(
      [](const webapps::AppId& app_id) { NOTREACHED_IN_MIGRATION(); }));
  install_observer.SetWebAppUninstalledDelegate(base::BindLambdaForTesting(
      [](const webapps::AppId& app_id) { NOTREACHED_IN_MIGRATION(); }));

  // CSS #RRGGBBAA syntax.
  OverrideManifest(kManifestTemplate, {kInstallableIconList, "#00FF00F0"});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpdated);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);
  ConfirmShortcutColors(app_id, {{{32, kAll}, kInstallableIconTopLeftColor},
                                 {{48, kAll}, kInstallableIconTopLeftColor},
                                 {{64, kWin}, kInstallableIconTopLeftColor},
                                 {{96, kWin}, kInstallableIconTopLeftColor},
                                 {{128, kAll}, kInstallableIconTopLeftColor},
                                 {{256, kAll}, kInstallableIconTopLeftColor}});

  // Updated theme_color loses any transparency.
  EXPECT_EQ(GetProvider().registrar_unsafe().GetAppThemeColor(app_id),
            SkColorSetARGB(0xFF, 0x00, 0xFF, 0x00));
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       CheckFindsBackgroundColorChange) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "icons": $1,
      "background_color": "$2"
    }
  )";
  OverrideManifest(kManifestTemplate, {kInstallableIconList, "blue"});
  webapps::AppId app_id = InstallWebApp();
  EXPECT_EQ(GetProvider().registrar_unsafe().GetAppBackgroundColor(app_id),
            SK_ColorBLUE);

  // CSS #RRGGBBAA syntax.
  OverrideManifest(kManifestTemplate, {kInstallableIconList, "red"});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpdated);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);

  EXPECT_EQ(GetProvider().registrar_unsafe().GetAppBackgroundColor(app_id),
            SkColorSetARGB(0xFF, 0xFF, 0x00, 0x00));
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       CheckFindsManifestUrlChange) {
  // This matches the content of chrome/test/data/banners/manifest_one_icon.json
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Manifest test app",
      "icons": [
          {
            "src": "image-512px.png",
            "sizes": "512x512",
            "type": "image/png"
          }
      ],
      "start_url": "manifest_test_page.html",
      "display": "standalone",
      "orientation": "landscape"
    }
  )";
  OverrideManifest(kManifestTemplate, {});
  webapps::AppId app_id = InstallWebApp();
  EXPECT_EQ(GetProvider().registrar_unsafe().GetAppManifestUrl(app_id),
            GetManifestURL());

  // Load a page which contains the same manifest content but at a new manifest
  // URL.
  GURL::Replacements replacements;
  replacements.SetQueryStr("manifest=/banners/manifest_one_icon.json");
  GURL app_url_with_new_manifest = GetAppURL().ReplaceComponents(replacements);
  EXPECT_EQ(GetResultAfterPageLoad(app_url_with_new_manifest),
            ManifestUpdateResult::kAppUpdated);

  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);
  EXPECT_EQ(GetProvider().registrar_unsafe().GetAppManifestUrl(app_id),
            http_server_.GetURL("/banners/manifest_one_icon.json"));
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest, CheckKeepsSameName) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "$1",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "icons": $2,
      "theme_color": "$3"
    }
  )";
  OverrideManifest(kManifestTemplate,
                   {"App name 1", kInstallableIconList, "blue"});
  webapps::AppId app_id = InstallWebApp();
  EXPECT_EQ(GetProvider().registrar_unsafe().GetAppThemeColor(app_id),
            SK_ColorBLUE);
  EXPECT_EQ(GetProvider().registrar_unsafe().GetAppShortName(app_id),
            "App name 1");

  OverrideManifest(kManifestTemplate,
                   {"App name 2", kInstallableIconList, "red"});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpdated);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);
  ConfirmShortcutColors(app_id, {{{32, kAll}, kInstallableIconTopLeftColor},
                                 {{48, kAll}, kInstallableIconTopLeftColor},
                                 {{64, kWin}, kInstallableIconTopLeftColor},
                                 {{96, kWin}, kInstallableIconTopLeftColor},
                                 {{128, kAll}, kInstallableIconTopLeftColor},
                                 {{256, kAll}, kInstallableIconTopLeftColor}});
  EXPECT_EQ(GetProvider().registrar_unsafe().GetAppThemeColor(app_id),
            SK_ColorRED);
  // The app name must not change without user confirmation.
  EXPECT_EQ(GetProvider().registrar_unsafe().GetAppShortName(app_id),
            "App name 1");
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       CheckDoesNotFindIconUrlChange) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "icons": $1
    }
  )";
  OverrideManifest(kManifestTemplate, {kInstallableIconList});
  webapps::AppId app_id = InstallWebApp();

  OverrideManifest(kManifestTemplate, {kAnotherInstallableIconList});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpToDate);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 0);
  ConfirmShortcutColors(app_id, {{{32, kAll}, kInstallableIconTopLeftColor},
                                 {{48, kAll}, kInstallableIconTopLeftColor},
                                 {{64, kWin}, kInstallableIconTopLeftColor},
                                 {{96, kWin}, kInstallableIconTopLeftColor},
                                 {{128, kAll}, kInstallableIconTopLeftColor},
                                 {{256, kAll}, kInstallableIconTopLeftColor}});
}

// TODO(crbug.com/40231087): Test is flaky.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC)
#define MAYBE_CheckDoesFindIconUrlChangeForDefaultApps \
  DISABLED_CheckDoesFindIconUrlChangeForDefaultApps
#else
#define MAYBE_CheckDoesFindIconUrlChangeForDefaultApps \
  CheckDoesFindIconUrlChangeForDefaultApps
#endif
IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       MAYBE_CheckDoesFindIconUrlChangeForDefaultApps) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "icons": $1
    }
  )";
  OverrideManifest(kManifestTemplate, {kInstallableIconList});
  webapps::AppId app_id = InstallDefaultApp();

  OverrideManifest(kManifestTemplate, {kAnotherInstallableIconList});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpdated);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);
  ConfirmShortcutColors(
      app_id, {{{32, kAll}, kAnotherInstallableIconTopLeftColor},
               {{48, kAll}, kAnotherInstallableIconTopLeftColor},
               {{64, kWin}, kAnotherInstallableIconTopLeftColor},
               {{96, kWin}, kAnotherInstallableIconTopLeftColor},
               {{128, kAll}, kAnotherInstallableIconTopLeftColor},
               {{256, kAll}, kAnotherInstallableIconTopLeftColor},
               {{512, kNotWin}, kAnotherInstallableIconTopLeftColor}});
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       CheckUpdatedPolicyAppsNotUninstallable) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "theme_color": "$1",
      "icons": $2
    }
  )";
  OverrideManifest(kManifestTemplate, {"blue", kInstallableIconList});
  webapps::AppId app_id = InstallPolicyApp();
  EXPECT_FALSE(GetProvider().registrar_unsafe().CanUserUninstallWebApp(app_id));

  OverrideManifest(kManifestTemplate, {"red", kInstallableIconList});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpdated);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);
  ConfirmShortcutColors(app_id, {{{32, kAll}, kInstallableIconTopLeftColor},
                                 {{48, kAll}, kInstallableIconTopLeftColor},
                                 {{64, kWin}, kInstallableIconTopLeftColor},
                                 {{96, kWin}, kInstallableIconTopLeftColor},
                                 {{128, kAll}, kInstallableIconTopLeftColor},
                                 {{256, kAll}, kInstallableIconTopLeftColor}});

  // Policy installed apps should continue to be not uninstallable by the user
  // after updating.
  EXPECT_FALSE(GetProvider().registrar_unsafe().CanUserUninstallWebApp(app_id));
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       CheckUpdatedKioskAppsNotUninstallable) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "theme_color": "$1",
      "icons": $2
    }
  )";
  OverrideManifest(kManifestTemplate, {"blue", kInstallableIconList});
  webapps::AppId app_id = InstallKioskApp();
  EXPECT_FALSE(GetProvider().registrar_unsafe().CanUserUninstallWebApp(app_id));

  OverrideManifest(kManifestTemplate, {"red", kInstallableIconList});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpdated);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);
  ConfirmShortcutColors(app_id, {{{32, kAll}, kInstallableIconTopLeftColor},
                                 {{48, kAll}, kInstallableIconTopLeftColor},
                                 {{64, kWin}, kInstallableIconTopLeftColor},
                                 {{96, kWin}, kInstallableIconTopLeftColor},
                                 {{128, kAll}, kInstallableIconTopLeftColor},
                                 {{256, kAll}, kInstallableIconTopLeftColor}});

  // Kiosk installed apps should continue to be not uninstallable by the user
  // after updating.
  EXPECT_FALSE(GetProvider().registrar_unsafe().CanUserUninstallWebApp(app_id));
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       CheckFindsScopeChange) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "$1",
      "display": "standalone",
      "icons": $2
    }
  )";
  OverrideManifest(kManifestTemplate, {"/banners/", kInstallableIconList});
  webapps::AppId app_id = InstallWebApp();

  OverrideManifest(kManifestTemplate, {"/", kInstallableIconList});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpdated);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);
  ConfirmShortcutColors(app_id, {{{32, kAll}, kInstallableIconTopLeftColor},
                                 {{48, kAll}, kInstallableIconTopLeftColor},
                                 {{64, kWin}, kInstallableIconTopLeftColor},
                                 {{96, kWin}, kInstallableIconTopLeftColor},
                                 {{128, kAll}, kInstallableIconTopLeftColor},
                                 {{256, kAll}, kInstallableIconTopLeftColor}});
  EXPECT_EQ(GetProvider().registrar_unsafe().GetAppScope(app_id),
            http_server_.GetURL("/"));
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       UninstallDialogClosesAppUpdateDialogUninstallsApp) {
  ResetAutomatedAppIdentityUpdateDialogBehavior();
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "$1",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "icons": $2
    }
  )";
  OverrideManifest(kManifestTemplate, {"Test app name", kInstallableIconList});
  webapps::AppId app_id = InstallWebApp();
  EXPECT_EQ("Test app name",
            GetProvider().registrar_unsafe().GetAppShortName(app_id));

  views::NamedWidgetShownWaiter manifest_waiter(
      views::test::AnyWidgetTestPasskey{},
      "WebAppIdentityUpdateConfirmationView");
  OverrideManifest(kManifestTemplate, {"Test app name2", kInstallableIconList});

  UpdateCheckResultAwaiter awaiter(GetAppURL());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetAppURL()));

  // Wait for the app identity update dialog to show up.
  auto* manifest_update_widget = manifest_waiter.WaitIfNeededAndGet();
  EXPECT_NE(manifest_update_widget, nullptr);

  views::NamedWidgetShownWaiter uninstall_waiter(
      views::test::AnyWidgetTestPasskey{}, "WebAppUninstallDialogDelegateView");

  // Wait for the uninstall dialog to show up after cancelling the app identity
  // update dialog. We cannot use views::test::CancelDialog here because under
  // the hood, CancelDialog starts running a callback for the update dialog to
  // be closed, which does not happen until uninstallation is scheduled, and
  // uninstallation cannot be scheduled because the callback is running, thus
  // leading to a deadlock.
  manifest_update_widget->widget_delegate()->AsDialogDelegate()->CancelDialog();
  auto* uninstall_widget = uninstall_waiter.WaitIfNeededAndGet();
  EXPECT_NE(uninstall_widget, nullptr);

  // Accept the uninstall dialog, and verify changes are propagated back to the
  // manifest update manager with the correct result.
  views::test::AcceptDialog(uninstall_widget);
  EXPECT_EQ(std::move(awaiter).AwaitNextResult(),
            ManifestUpdateResult::kAppIdentityUpdateRejectedAndUninstalled);

  // This is to ensure that the uninstall command that was scheduled also
  // completes.
  GetProvider().command_manager().AwaitAllCommandsCompleteForTesting();
  EXPECT_FALSE(GetProvider().registrar_unsafe().IsInstalled(app_id));
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       UninstallDialogCancelStillShowsAppUpdateDialog) {
  ResetAutomatedAppIdentityUpdateDialogBehavior();
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "$1",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "icons": $2
    }
  )";
  OverrideManifest(kManifestTemplate, {"Test app name", kInstallableIconList});
  webapps::AppId app_id = InstallWebApp();
  EXPECT_EQ("Test app name",
            GetProvider().registrar_unsafe().GetAppShortName(app_id));

  views::NamedWidgetShownWaiter manifest_waiter(
      views::test::AnyWidgetTestPasskey{},
      "WebAppIdentityUpdateConfirmationView");
  OverrideManifest(kManifestTemplate, {"App Name 2", kInstallableIconList});

  UpdateCheckResultAwaiter awaiter(GetAppURL());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetAppURL()));

  auto* manifest_update_widget = manifest_waiter.WaitIfNeededAndGet();
  EXPECT_NE(manifest_update_widget, nullptr);

  views::NamedWidgetShownWaiter uninstall_waiter(
      views::test::AnyWidgetTestPasskey{}, "WebAppUninstallDialogDelegateView");
  manifest_update_widget->widget_delegate()->AsDialogDelegate()->CancelDialog();
  auto* uninstall_widget = uninstall_waiter.WaitIfNeededAndGet();
  EXPECT_NE(uninstall_widget, nullptr);

  views::test::WidgetVisibleWaiter update_visible_waiter(
      manifest_update_widget);

  // If the uninstall dialog is cancelled, then app identity update dialog
  // should regain visibility.
  views::test::CancelDialog(uninstall_widget);
  update_visible_waiter.Wait();
  views::test::AcceptDialog(manifest_update_widget);
  EXPECT_EQ(std::move(awaiter).AwaitNextResult(),
            ManifestUpdateResult::kAppUpdated);
  EXPECT_EQ("App Name 2",
            GetProvider().registrar_unsafe().GetAppShortName(app_id));
}

IN_PROC_BROWSER_TEST_P(ManifestUpdateManagerBrowserTest_UpdateDialog,
                       ScopeChangeWithProductIconChange) {
  // This test changes the scope and also the icon list. The scope should
  // update. The icon should update only if identity updates are allowed.
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "$1",
      "display": "standalone",
      "icons": $2
    }
  )";
  OverrideManifest(kManifestTemplate, {"/banners/", kInstallableIconList});
  webapps::AppId app_id = InstallWebApp();

  if (IsUpdateDialogEnabled()) {
    AcceptAppIdentityUpdateDialogForTesting();
  }

  OverrideManifest(kManifestTemplate, {"/", kAnotherInstallableIconList});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpdated);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);

  // The icon should be updated only if product icon updates are allowed.
  if (IsUpdateDialogEnabled()) {
    ConfirmShortcutColors(
        app_id, {{{32, kAll}, kAnotherInstallableIconTopLeftColor},
                 {{48, kAll}, kAnotherInstallableIconTopLeftColor},
                 {{64, kWin}, kAnotherInstallableIconTopLeftColor},
                 {{96, kWin}, kAnotherInstallableIconTopLeftColor},
                 {{128, kAll}, kAnotherInstallableIconTopLeftColor},
                 {{256, kAll}, kAnotherInstallableIconTopLeftColor},
                 {{512, kNotWin}, kAnotherInstallableIconTopLeftColor}});
  } else {
    ConfirmShortcutColors(app_id,
                          {{{32, kAll}, kInstallableIconTopLeftColor},
                           {{48, kAll}, kInstallableIconTopLeftColor},
                           {{64, kWin}, kInstallableIconTopLeftColor},
                           {{96, kWin}, kInstallableIconTopLeftColor},
                           {{128, kAll}, kInstallableIconTopLeftColor},
                           {{256, kAll}, kInstallableIconTopLeftColor}});
  }
  EXPECT_EQ(GetProvider().registrar_unsafe().GetAppScope(app_id),
            http_server_.GetURL("/"));
}

// TODO(crbug.com/40231087): Test is flaky.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_CheckDoesApplyIconURLChangeForDefaultApps \
  DISABLED_CheckDoesApplyIconURLChangeForDefaultApps
#else
#define MAYBE_CheckDoesApplyIconURLChangeForDefaultApps \
  CheckDoesApplyIconURLChangeForDefaultApps
#endif
IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       MAYBE_CheckDoesApplyIconURLChangeForDefaultApps) {
  // This test changes the scope and also the icon list. The scope should update
  // along with the icons.
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "$1",
      "display": "standalone",
      "icons": $2
    }
  )";
  OverrideManifest(kManifestTemplate, {"/banners/", kInstallableIconList});
  webapps::AppId app_id = InstallDefaultApp();

  OverrideManifest(kManifestTemplate, {"/", kAnotherInstallableIconList});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpdated);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);
  // The icon should have updated.
  ConfirmShortcutColors(
      app_id, {{{32, kAll}, kAnotherInstallableIconTopLeftColor},
               {{48, kAll}, kAnotherInstallableIconTopLeftColor},
               {{64, kWin}, kAnotherInstallableIconTopLeftColor},
               {{96, kWin}, kAnotherInstallableIconTopLeftColor},
               {{128, kAll}, kAnotherInstallableIconTopLeftColor},
               {{256, kAll}, kAnotherInstallableIconTopLeftColor},
               {{512, kNotWin}, kAnotherInstallableIconTopLeftColor}});
  EXPECT_EQ(GetProvider().registrar_unsafe().GetAppScope(app_id),
            http_server_.GetURL("/"));
}

class ManifestUpdateManagerBrowserTest_PolicyAppsCanUpdate
    : public ManifestUpdateManagerBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  ManifestUpdateManagerBrowserTest_PolicyAppsCanUpdate() {
    scoped_feature_list_.InitWithFeatureState(
        features::kWebAppManifestPolicyAppIdentityUpdate, GetParam());
  }

  bool ExpectUpdateAllowed() {
    return base::FeatureList::IsEnabled(
        features::kWebAppManifestPolicyAppIdentityUpdate);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(ManifestUpdateManagerBrowserTest_PolicyAppsCanUpdate,
                       CheckDoesApplyIconURLChangeForPolicyAppsWithFlag) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "display": "standalone",
      "icons": $1
    }
  )";
  OverrideManifest(kManifestTemplate, {kInstallableIconList});
  webapps::AppId app_id = InstallPolicyApp();

  OverrideManifest(kManifestTemplate, {kAnotherInstallableIconList});

  if (ExpectUpdateAllowed()) {
    // The icon should have updated (because the flag is enabled).
    EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
              ManifestUpdateResult::kAppUpdated);
    histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                        ManifestUpdateResult::kAppUpdated, 1);
    ConfirmShortcutColors(
        app_id, {{{32, kAll}, kAnotherInstallableIconTopLeftColor},
                 {{48, kAll}, kAnotherInstallableIconTopLeftColor},
                 {{64, kWin}, kAnotherInstallableIconTopLeftColor},
                 {{96, kWin}, kAnotherInstallableIconTopLeftColor},
                 {{128, kAll}, kAnotherInstallableIconTopLeftColor},
                 {{256, kAll}, kAnotherInstallableIconTopLeftColor},
                 {{512, kNotWin}, kAnotherInstallableIconTopLeftColor}});
  } else {
    // The icon should not have updated.
    EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
              ManifestUpdateResult::kAppUpToDate);
    histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                        ManifestUpdateResult::kAppUpdated, 0);
    ConfirmShortcutColors(app_id,
                          {{{32, kAll}, kInstallableIconTopLeftColor},
                           {{48, kAll}, kInstallableIconTopLeftColor},
                           {{64, kWin}, kInstallableIconTopLeftColor},
                           {{96, kWin}, kInstallableIconTopLeftColor},
                           {{128, kAll}, kInstallableIconTopLeftColor},
                           {{256, kAll}, kInstallableIconTopLeftColor}});
  }
}

// This test ensures app name cannot be changed for policy apps (without a flag
// allowing it).
IN_PROC_BROWSER_TEST_P(ManifestUpdateManagerBrowserTest_PolicyAppsCanUpdate,
                       CheckNameUpdatesForPolicyApps) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "$1",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "icons": $2
    }
  )";
  OverrideManifest(kManifestTemplate, {"Test app name", kInstallableIconList});
  webapps::AppId app_id = InstallPolicyApp();

  OverrideManifest(kManifestTemplate,
                   {"Different app name", kInstallableIconList});

  if (ExpectUpdateAllowed()) {
    // Name should have updated (because the flag is enabled).
    EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
              ManifestUpdateResult::kAppUpdated);
    histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                        ManifestUpdateResult::kAppUpdated, 1);
    EXPECT_EQ(GetProvider().registrar_unsafe().GetAppShortName(app_id),
              "Different app name");
  } else {
    // Name should not have updated (because the flag is missing).
    EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
              ManifestUpdateResult::kAppUpToDate);
    histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                        ManifestUpdateResult::kAppUpdated, 0);
    EXPECT_EQ(GetProvider().registrar_unsafe().GetAppShortName(app_id),
              "Test app name");
  }
}

// This test ensures app icon url can always be changed for Kiosk apps.
IN_PROC_BROWSER_TEST_P(ManifestUpdateManagerBrowserTest_PolicyAppsCanUpdate,
                       CheckDoesApplyIconURLChangeForKioskAppsIgnoringFlag) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "display": "standalone",
      "icons": $1
    }
  )";
  OverrideManifest(kManifestTemplate, {kInstallableIconList});
  webapps::AppId app_id = InstallKioskApp();

  OverrideManifest(kManifestTemplate, {kAnotherInstallableIconList});

  // The icon should have updated (regardless of flag status).
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpdated);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);
  ConfirmShortcutColors(
      app_id, {{{32, kAll}, kAnotherInstallableIconTopLeftColor},
               {{48, kAll}, kAnotherInstallableIconTopLeftColor},
               {{64, kWin}, kAnotherInstallableIconTopLeftColor},
               {{96, kWin}, kAnotherInstallableIconTopLeftColor},
               {{128, kAll}, kAnotherInstallableIconTopLeftColor},
               {{256, kAll}, kAnotherInstallableIconTopLeftColor},
               {{512, kNotWin}, kAnotherInstallableIconTopLeftColor}});
}

// This test ensures app name can always be changed for Kiosk apps.
IN_PROC_BROWSER_TEST_P(ManifestUpdateManagerBrowserTest_PolicyAppsCanUpdate,
                       CheckNameUpdatesForKioskApps) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "$1",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "icons": $2
    }
  )";
  OverrideManifest(kManifestTemplate, {"Test app name", kInstallableIconList});
  webapps::AppId app_id = InstallKioskApp();

  OverrideManifest(kManifestTemplate,
                   {"Different app name", kInstallableIconList});

  // Name should have updated (regardless of flag status).
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpdated);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);
  EXPECT_EQ(GetProvider().registrar_unsafe().GetAppShortName(app_id),
            "Different app name");
}

INSTANTIATE_TEST_SUITE_P(PolicyAppParameterizedTest,
                         ManifestUpdateManagerBrowserTest_PolicyAppsCanUpdate,
                         ::testing::Values(true, false));

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       CheckFindsDisplayChange) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "$1",
      "icons": $2
    }
  )";
  OverrideManifest(kManifestTemplate, {"minimal-ui", kInstallableIconList});
  webapps::AppId app_id = InstallWebApp();

  OverrideManifest(kManifestTemplate, {"standalone", kInstallableIconList});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpdated);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);
  ConfirmShortcutColors(app_id, {{{32, kAll}, kInstallableIconTopLeftColor},
                                 {{48, kAll}, kInstallableIconTopLeftColor},
                                 {{64, kWin}, kInstallableIconTopLeftColor},
                                 {{96, kWin}, kInstallableIconTopLeftColor},
                                 {{128, kAll}, kInstallableIconTopLeftColor},
                                 {{256, kAll}, kInstallableIconTopLeftColor}});
  EXPECT_EQ(GetProvider().registrar_unsafe().GetAppDisplayMode(app_id),
            DisplayMode::kStandalone);
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       CheckFindsDisplayBrowserChange) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "$1",
      "icons": $2
    }
  )";
  OverrideManifest(kManifestTemplate, {"standalone", kInstallableIconList});
  webapps::AppId app_id = InstallWebApp();
  GetProvider().sync_bridge_unsafe().SetAppUserDisplayModeForTesting(
      app_id, mojom::UserDisplayMode::kStandalone);

  OverrideManifest(kManifestTemplate, {"browser", kInstallableIconList});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpdated);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);
  EXPECT_EQ(GetProvider().registrar_unsafe().GetAppDisplayMode(app_id),
            DisplayMode::kBrowser);

  // We don't touch the user's launch preference even if the app display mode
  // changes. Instead the effective display mode changes.
  EXPECT_EQ(GetProvider().registrar_unsafe().GetAppUserDisplayMode(app_id),
            mojom::UserDisplayMode::kStandalone);
  EXPECT_EQ(GetProvider().registrar_unsafe().GetAppEffectiveDisplayMode(app_id),
            DisplayMode::kMinimalUi);
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       CheckFindsDisplayOverrideChange) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "display_override": $1,
      "icons": $2
    }
  )";

  OverrideManifest(kManifestTemplate,
                   {R"([ "fullscreen", "standalone" ])", kInstallableIconList});
  webapps::AppId app_id = InstallWebApp();

  OverrideManifest(kManifestTemplate,
                   {R"([ "fullscreen", "minimal-ui" ])", kInstallableIconList});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpdated);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);
  ConfirmShortcutColors(app_id, {{{32, kAll}, kInstallableIconTopLeftColor},
                                 {{48, kAll}, kInstallableIconTopLeftColor},
                                 {{64, kWin}, kInstallableIconTopLeftColor},
                                 {{96, kWin}, kInstallableIconTopLeftColor},
                                 {{128, kAll}, kInstallableIconTopLeftColor},
                                 {{256, kAll}, kInstallableIconTopLeftColor}});

  std::vector<DisplayMode> app_display_mode_override =
      GetProvider().registrar_unsafe().GetAppDisplayModeOverride(app_id);

  ASSERT_EQ(2u, app_display_mode_override.size());
  EXPECT_EQ(DisplayMode::kFullscreen, app_display_mode_override[0]);
  EXPECT_EQ(DisplayMode::kMinimalUi, app_display_mode_override[1]);
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       CheckFindsNewDisplayOverride) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      $1
      "icons": $2
    }
  )";

  // No display_override in manifest
  OverrideManifest(kManifestTemplate, {"", kInstallableIconList});
  webapps::AppId app_id = InstallWebApp();

  // Add display_override field
  OverrideManifest(kManifestTemplate,
                   {R"("display_override": [ "minimal-ui", "standalone" ],)",
                    kInstallableIconList});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpdated);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);
  ConfirmShortcutColors(app_id, {{{32, kAll}, kInstallableIconTopLeftColor},
                                 {{48, kAll}, kInstallableIconTopLeftColor},
                                 {{64, kWin}, kInstallableIconTopLeftColor},
                                 {{96, kWin}, kInstallableIconTopLeftColor},
                                 {{128, kAll}, kInstallableIconTopLeftColor},
                                 {{256, kAll}, kInstallableIconTopLeftColor}});

  std::vector<DisplayMode> app_display_mode_override =
      GetProvider().registrar_unsafe().GetAppDisplayModeOverride(app_id);

  ASSERT_EQ(2u, app_display_mode_override.size());
  EXPECT_EQ(DisplayMode::kMinimalUi, app_display_mode_override[0]);
  EXPECT_EQ(DisplayMode::kStandalone, app_display_mode_override[1]);
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       CheckFindsDeletedDisplayOverride) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      $1
      "icons": $2
    }
  )";

  // Ensure display_override exists in initial manifest
  OverrideManifest(kManifestTemplate,
                   {R"("display_override": [ "fullscreen", "minimal-ui" ],)",
                    kInstallableIconList});
  webapps::AppId app_id = InstallWebApp();

  // Remove display_override from manifest
  OverrideManifest(kManifestTemplate, {"", kInstallableIconList});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpdated);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);
  ConfirmShortcutColors(app_id, {{{32, kAll}, kInstallableIconTopLeftColor},
                                 {{48, kAll}, kInstallableIconTopLeftColor},
                                 {{64, kWin}, kInstallableIconTopLeftColor},
                                 {{96, kWin}, kInstallableIconTopLeftColor},
                                 {{128, kAll}, kInstallableIconTopLeftColor},
                                 {{256, kAll}, kInstallableIconTopLeftColor}});

  std::vector<DisplayMode> app_display_mode_override =
      GetProvider().registrar_unsafe().GetAppDisplayModeOverride(app_id);

  ASSERT_EQ(0u, app_display_mode_override.size());
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       CheckFindsInvalidDisplayOverride) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "display_override": $1,
      "icons": $2
    }
  )";

  OverrideManifest(kManifestTemplate,
                   {R"([ "browser", "fullscreen" ])", kInstallableIconList});
  webapps::AppId app_id = InstallWebApp();

  ASSERT_EQ(2u, GetProvider()
                    .registrar_unsafe()
                    .GetAppDisplayModeOverride(app_id)
                    .size());

  // display_override contains only invalid values
  OverrideManifest(kManifestTemplate,
                   {R"( [ "invalid", 7 ])", kInstallableIconList});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpdated);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);
  ConfirmShortcutColors(app_id, {{{32, kAll}, kInstallableIconTopLeftColor},
                                 {{48, kAll}, kInstallableIconTopLeftColor},
                                 {{64, kWin}, kInstallableIconTopLeftColor},
                                 {{96, kWin}, kInstallableIconTopLeftColor},
                                 {{128, kAll}, kInstallableIconTopLeftColor},
                                 {{256, kAll}, kInstallableIconTopLeftColor}});

  std::vector<DisplayMode> app_display_mode_override =
      GetProvider().registrar_unsafe().GetAppDisplayModeOverride(app_id);

  ASSERT_EQ(0u, app_display_mode_override.size());
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       CheckIgnoresDisplayOverrideInvalidChange) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      $1
      "icons": $2
    }
  )";

  // No display_override in manifest
  OverrideManifest(kManifestTemplate, {"", kInstallableIconList});
  webapps::AppId app_id = InstallWebApp();

  // display_override contains only invalid values
  OverrideManifest(
      kManifestTemplate,
      {R"("display_override": [ "invalid", 7 ],)", kInstallableIconList});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpToDate);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpToDate, 1);
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       CheckIgnoresDisplayOverrideChange) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "display_override": $1,
      "icons": $2
    }
  )";

  OverrideManifest(kManifestTemplate,
                   {R"([ "standard", "fullscreen" ])", kInstallableIconList});
  webapps::AppId app_id = InstallWebApp();

  // display_override contains an additional invalid value
  OverrideManifest(
      kManifestTemplate,
      {R"([ "invalid", "standard", "fullscreen" ])", kInstallableIconList});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpToDate);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpToDate, 1);
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       CheckDoesNotFindIconContentChange) {
  constexpr char kManifest[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "icons": [
        {
          "src": "/banners/192x192-green.png?ignore",
          "sizes": "192x192",
          "type": "image/png"
        }
      ]
    }
  )";
  OverrideManifest(kManifest, {});
  webapps::AppId app_id = InstallWebApp();

  // Replace the contents of 192x192-green.png with 192x192-red.png without
  // changing the URL.
  content::URLLoaderInterceptor url_interceptor(base::BindLambdaForTesting(
      [this](content::URLLoaderInterceptor::RequestParams* params)
          -> bool /*intercepted*/ {
        if (params->url_request.url ==
            http_server_.GetURL("/banners/192x192-green.png?ignore")) {
          content::URLLoaderInterceptor::WriteResponse(
              "chrome/test/data/banners/192x192-red.png", params->client.get());
          return true;
        }
        return false;
      }));

  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpToDate);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 0);

  ConfirmShortcutColors(app_id, {{{32, kAll}, SK_ColorGREEN},
                                 {{48, kAll}, SK_ColorGREEN},
                                 {{64, kWin}, SK_ColorGREEN},
                                 {{96, kWin}, SK_ColorGREEN},
                                 {{128, kAll}, SK_ColorGREEN},
                                 {{256, kAll}, SK_ColorGREEN}});

  EXPECT_EQ(ReadAppIconPixel(app_id, /*size=*/192), SK_ColorGREEN);
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       CheckDoesNotUpdateGeneratedIcons) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "icons": $1
    }
  )";
  OverrideManifest(kManifestTemplate, {"[]"});
  webapps::AppId app_id = InstallWebApp();

  OverrideManifest(kManifestTemplate, {kAnotherInstallableIconList});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpToDate);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 0);
}

// TODO(crbug.com/40250635): Flakes on multiple platforms.
IN_PROC_BROWSER_TEST_P(ManifestUpdateManagerBrowserTest_UpdateDialog,
                       DISABLED_CheckUpdateOfGeneratedIcons_SyncFailure) {
  // The first "name" character is used to generate icons. Make it like a space
  // to probe the background color at the center. Spaces are trimmed by the
  // parser.
  constexpr char kManifest[] = R"(
    {
      "name": "_Test App Name",
      "start_url": "manifest_test_page.html",
      "scope": "/",
      "display": "$1",
      "icons": [
        {
          "src": "/web_apps/blue-192.png",
          "sizes": "192x192",
          "type": "image/png"
        }
      ]
    }
  )";

  OverrideManifest(kManifest, {"standalone"});

  webapps::AppId app_id;

  // Make blue-192.png fail to download for the first sync install.
  {
    std::unique_ptr<content::URLLoaderInterceptor> url_interceptor =
        content::URLLoaderInterceptor::SetupRequestFailForURL(
            http_server_.GetURL("/web_apps/blue-192.png"),
            net::Error::ERR_FILE_NOT_FOUND);

    app_id = InstallWebAppFromSync(GetAppURL());
  }

  const WebApp* web_app = GetProvider().registrar_unsafe().GetAppById(app_id);
  ASSERT_TRUE(web_app);
  EXPECT_TRUE(web_app->is_generated_icon());

  // ManifestUpdateManager updates only locally installed apps. Installs web app
  // locally on Win/Mac/Linux.
  if (GetProvider().registrar_unsafe().IsInstallState(
          app_id, {proto::InstallState::SUGGESTED_FROM_ANOTHER_DEVICE})) {
    InstallAppLocally(web_app);
  }

  // Autogenerated icons in `ResizeIconsAndGenerateMissing()` use hardcoded dark
  // gray color as background.
  EXPECT_EQ(6u, web_app->downloaded_icon_sizes(IconPurpose::ANY).size());
  for (SquareSizePx size_px :
       web_app->downloaded_icon_sizes(IconPurpose::ANY)) {
    SCOPED_TRACE(size_px);
    EXPECT_EQ(color_utils::SkColorToRgbaString(ReadAppIconPixel(
                  app_id, size_px, /*x=*/size_px / 2, /*y=*/size_px / 2)),
              color_utils::SkColorToRgbaString(SK_ColorDKGRAY));
  }

  if (IsUpdateDialogEnabled()) {
    AcceptAppIdentityUpdateDialogForTesting();
  }

  OverrideManifest(kManifest, {"browser"});

  ManifestUpdateResult update_result = GetResultAfterPageLoad(GetAppURL());

  ASSERT_EQ(web_app, GetProvider().registrar_unsafe().GetAppById(app_id));

  EXPECT_EQ(update_result, ManifestUpdateResult::kAppUpdated);

  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);

  if (IsUpdateDialogEnabled()) {
    // An actual icon was downloaded, so icon should not be autogenerated.
    EXPECT_FALSE(web_app->is_generated_icon());
    // A non-generated icon was added, so expect 7 instead of 6.
    EXPECT_EQ(7u, web_app->downloaded_icon_sizes(IconPurpose::ANY).size());
    // Icon should have turned blue.
    for (SquareSizePx size_px :
         web_app->downloaded_icon_sizes(IconPurpose::ANY)) {
      SCOPED_TRACE(size_px);
      EXPECT_EQ(color_utils::SkColorToRgbaString(ReadAppIconPixel(
                    app_id, size_px, /*x=*/size_px / 2, /*y=*/size_px / 2)),
                color_utils::SkColorToRgbaString(SK_ColorBLUE));
    }
  } else {
    // Still autogenerated icons, no change.
    EXPECT_TRUE(web_app->is_generated_icon());
    // Not 7u, no non-generated icon added.
    EXPECT_EQ(6u, web_app->downloaded_icon_sizes(IconPurpose::ANY).size());
    // Not SK_ColorBLUE for blue-192.png.
    for (SquareSizePx size_px :
         web_app->downloaded_icon_sizes(IconPurpose::ANY)) {
      SCOPED_TRACE(size_px);
      EXPECT_EQ(color_utils::SkColorToRgbaString(ReadAppIconPixel(
                    app_id, size_px, /*x=*/size_px / 2, /*y=*/size_px / 2)),
                color_utils::SkColorToRgbaString(SK_ColorDKGRAY));
    }
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ManifestUpdateManagerBrowserTest_UpdateDialog,
    ::testing::Values(UpdateDialogParam::kEnabled,
                      UpdateDialogParam::kDisabled),
    ManifestUpdateManagerBrowserTest_UpdateDialog::ParamToString);

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       CheckFindsLaunchHandlerChange) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "icons": $1,
      "launch_handler": $2
    }
  )";

  OverrideManifest(kManifestTemplate, {kInstallableIconList, R"({
    "client_mode": "focus-existing"
  })"});
  webapps::AppId app_id = InstallWebApp();
  EXPECT_EQ(
      GetProvider().registrar_unsafe().GetAppById(app_id)->launch_handler(),
      (LaunchHandler{LaunchHandler::ClientMode::kFocusExisting}));

  // New launch_handler syntax.
  OverrideManifest(kManifestTemplate, {kInstallableIconList, R"({
    "client_mode": "navigate-existing"
  })"});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpdated);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);
  ConfirmShortcutColors(app_id, {{{32, kAll}, kInstallableIconTopLeftColor},
                                 {{48, kAll}, kInstallableIconTopLeftColor},
                                 {{64, kWin}, kInstallableIconTopLeftColor},
                                 {{96, kWin}, kInstallableIconTopLeftColor},
                                 {{128, kAll}, kInstallableIconTopLeftColor},
                                 {{256, kAll}, kInstallableIconTopLeftColor}});
  EXPECT_EQ(
      GetProvider().registrar_unsafe().GetAppById(app_id)->launch_handler(),
      (LaunchHandler{LaunchHandler::ClientMode::kNavigateExisting}));
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
class ManifestUpdateManagerSystemAppBrowserTest
    : public ManifestUpdateManagerBrowserTest {
 public:
  ManifestUpdateManagerSystemAppBrowserTest()
      : system_app_(ash::TestSystemWebAppInstallation::
                        SetUpStandaloneSingleWindowApp()) {}

  void SetUpOnMainThread() override { system_app_->WaitForAppInstall(); }

 protected:
  std::unique_ptr<ash::TestSystemWebAppInstallation> system_app_;
};

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerSystemAppBrowserTest,
                       CheckUpdateSkipped) {
  webapps::AppId app_id = system_app_->GetAppId();
  EXPECT_EQ(GetResultAfterPageLoad(system_app_->GetAppUrl()),
            ManifestUpdateResult::kAppIsSystemWebApp);

  histogram_tester_.ExpectBucketCount(
      kUpdateHistogramName, ManifestUpdateResult::kAppIsSystemWebApp, 1);
  EXPECT_EQ(GetProvider().registrar_unsafe().GetAppThemeColor(app_id),
            SK_ColorGREEN);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

class ManifestUpdateManagerIsolatedWebAppBrowserTest
    : public IsolatedWebAppBrowserTestHarness {
 public:
  ManifestUpdateManagerIsolatedWebAppBrowserTest() {
    isolated_web_app_dev_server_ =
        CreateAndStartServer(FILE_PATH_LITERAL("web_apps/simple_isolated_app"));
  }

 protected:
  std::unique_ptr<net::EmbeddedTestServer> isolated_web_app_dev_server_;
  base::HistogramTester histogram_tester_;
};

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerIsolatedWebAppBrowserTest,
                       CheckUpdateSkipped) {
  IsolatedWebAppUrlInfo url_info_ = InstallDevModeProxyIsolatedWebApp(
      isolated_web_app_dev_server_->GetOrigin());

  UpdateCheckResultAwaiter awaiter(
      url_info_.origin().GetURL().Resolve("/index.html"));
  EXPECT_TRUE(OpenApp(url_info_.app_id()));
  EXPECT_EQ(std::move(awaiter).AwaitNextResult(),
            ManifestUpdateResult::kAppIsIsolatedWebApp);

  histogram_tester_.ExpectBucketCount(
      kUpdateHistogramName, ManifestUpdateResult::kAppIsIsolatedWebApp, 1);
}

using ManifestUpdateManagerWebAppsBrowserTest =
    ManifestUpdateManagerBrowserTest;

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerWebAppsBrowserTest,
                       CheckFindsAddedShareTarget) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "minimal-ui",
      "icons": $1
    }
  )";

  constexpr char kShareTargetManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "minimal-ui",
      "share_target": {
        "action": "/web_share_target/share.html",
        "method": "GET",
        "params": {
          "url": "link"
        }
      },
      "icons": $1
    }
  )";

  OverrideManifest(kManifestTemplate, {kInstallableIconList});
  webapps::AppId app_id = InstallWebApp();

  OverrideManifest(kShareTargetManifestTemplate, {kInstallableIconList});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpdated);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);

  const WebApp* web_app = GetProvider().registrar_unsafe().GetAppById(app_id);
  EXPECT_TRUE(web_app->share_target().has_value());
  EXPECT_EQ(web_app->share_target()->method, apps::ShareTarget::Method::kGet);
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerWebAppsBrowserTest,
                       CheckFindsShareTargetChange) {
  constexpr char kShareTargetManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "minimal-ui",
      "share_target": {
        "action": "/web_share_target/share.html",
        "method": "$1",
        "params": {
          "url": "link"
        }
      },
      "icons": $2
    }
  )";
  OverrideManifest(kShareTargetManifestTemplate, {"GET", kInstallableIconList});
  webapps::AppId app_id = InstallWebApp();

  OverrideManifest(kShareTargetManifestTemplate,
                   {"POST", kInstallableIconList});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpdated);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);

  const WebApp* web_app = GetProvider().registrar_unsafe().GetAppById(app_id);
  EXPECT_TRUE(web_app->share_target().has_value());
  EXPECT_EQ(web_app->share_target()->method, apps::ShareTarget::Method::kPost);
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerWebAppsBrowserTest,
                       CheckFindsDeletedShareTarget) {
  constexpr char kShareTargetManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "minimal-ui",
      "share_target": {
        "action": "/web_share_target/share.html",
        "method": "GET",
        "params": {
          "url": "link"
        }
      },
      "icons": $1
    }
  )";

  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "minimal-ui",
      "icons": $1
    }
  )";

  OverrideManifest(kShareTargetManifestTemplate, {kInstallableIconList});
  webapps::AppId app_id = InstallWebApp();

  OverrideManifest(kManifestTemplate, {kInstallableIconList});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpdated);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);

  const WebApp* web_app = GetProvider().registrar_unsafe().GetAppById(app_id);
  EXPECT_FALSE(web_app->share_target().has_value());
}

// Functional tests. More tests for detecting file handler updates are
// available in unit tests at ManifestUpdateDataFetchUtilsTest.
class ManifestUpdateManagerBrowserTestWithFileHandling
    : public ManifestUpdateManagerBrowserTest {
 public:
  ManifestUpdateManagerBrowserTestWithFileHandling() = default;

 private:
};

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTestWithFileHandling,
                       CheckFindsAddedFileHandler) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "minimal-ui",
      "icons": $1
    }
  )";

  constexpr char kFileHandlerManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "minimal-ui",
      "file_handlers": [
        {
          "action": "/?plaintext",
          "name": "Plain Text",
          "accept": {
            "text/plain": [".txt"]
          }
        }
      ],
      "icons": $1
    }
  )";

  OverrideManifest(kManifestTemplate, {kInstallableIconList});
  webapps::AppId app_id = InstallWebApp();

  OverrideManifest(kFileHandlerManifestTemplate, {kInstallableIconList});
  EXPECT_EQ(ManifestUpdateResult::kAppUpdated,
            GetResultAfterPageLoad(GetAppURL()));
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);

  const WebApp* web_app = GetProvider().registrar_unsafe().GetAppById(app_id);
  EXPECT_FALSE(web_app->file_handlers().empty());
  const auto& file_handler = web_app->file_handlers()[0];
  EXPECT_EQ("plaintext", file_handler.action.query());
  EXPECT_EQ(1u, file_handler.accept.size());
  EXPECT_EQ("text/plain", file_handler.accept[0].mime_type);
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTestWithFileHandling,
                       CheckIgnoresUnchangedFileHandler) {
  constexpr char kFileHandlerManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "minimal-ui",
      "file_handlers": [
        {
          "action": "/?plaintext",
          "name": "Plain Text",
          "accept": {
            "text/plain": [".txt"]
          }
        }
      ],
      "icons": $1
    }
  )";

  OverrideManifest(kFileHandlerManifestTemplate, {kInstallableIconList});
  webapps::AppId app_id = InstallWebApp();

  OverrideManifest(kFileHandlerManifestTemplate, {kInstallableIconList});
  EXPECT_EQ(ManifestUpdateResult::kAppUpToDate,
            GetResultAfterPageLoad(GetAppURL()));
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpToDate, 1);

  const WebApp* web_app = GetProvider().registrar_unsafe().GetAppById(app_id);
  EXPECT_FALSE(web_app->file_handlers().empty());
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTestWithFileHandling,
                       CheckFindsChangedFileExtension) {
  constexpr char kFileHandlerManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "minimal-ui",
      "file_handlers": [
        {
          "action": "/?plaintext",
          "name": "Plain Text",
          "accept": {
            "text/plain": ["$1"]
          }
        }
      ],
      "icons": $2
    }
  )";

  OverrideManifest(kFileHandlerManifestTemplate,
                   {".txt", kInstallableIconList});
  webapps::AppId app_id = InstallWebApp();
  const WebApp* web_app = GetProvider().registrar_unsafe().GetAppById(app_id);
  const auto& old_file_handler = web_app->file_handlers()[0];
  EXPECT_EQ(1u, old_file_handler.accept.size());
  auto old_extensions = old_file_handler.accept[0].file_extensions;
  EXPECT_EQ(1u, old_extensions.size());
  EXPECT_TRUE(base::Contains(old_extensions, ".txt"));

  OverrideManifest(kFileHandlerManifestTemplate, {".md", kInstallableIconList});
  EXPECT_EQ(ManifestUpdateResult::kAppUpdated,
            GetResultAfterPageLoad(GetAppURL()));
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);

  const auto& new_file_handler = web_app->file_handlers()[0];
  EXPECT_EQ(1u, new_file_handler.accept.size());
  auto new_extensions = new_file_handler.accept[0].file_extensions;
  EXPECT_EQ(1u, new_extensions.size());
  EXPECT_TRUE(base::Contains(new_extensions, ".md"));
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTestWithFileHandling,
                       FileHandlingPermissionResetsOnUpdate) {
  constexpr char kFileHandlerManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "minimal-ui",
      "file_handlers": [
        {
          "action": "/?plaintext",
          "name": "Plain Text",
          "accept": {
            "text/plain": ["$1"]
          }
        }
      ],
      "icons": [
        {
          "src": "launcher-icon-4x.png",
          "sizes": "192x192",
          "type": "image/png"
        }
      ],
      "theme_color": "$2"
    }
  )";

  OverrideManifest(kFileHandlerManifestTemplate, {".txt", "red"});
  webapps::AppId app_id = InstallWebApp();
  const WebApp* web_app = GetProvider().registrar_unsafe().GetAppById(app_id);
  const auto& old_file_handler = web_app->file_handlers()[0];
  auto old_extensions = old_file_handler.accept[0].file_extensions;
  EXPECT_TRUE(base::Contains(old_extensions, ".txt"));
  const GURL url = GetAppURL();
  const GURL origin = url.DeprecatedGetOriginAsURL();

  EXPECT_EQ(
      ApiApprovalState::kRequiresPrompt,
      GetProvider().registrar_unsafe().GetAppFileHandlerApprovalState(app_id));
  GetProvider().sync_bridge_unsafe().SetAppFileHandlerApprovalState(
      app_id, ApiApprovalState::kAllowed);

  // Update manifest, adding an extension to the file handler. Permission should
  // be downgraded to ASK. The time override is necessary to make sure the
  // manifest update isn't skipped due to throttling.
  base::Time time_override = base::Time::Now();
  SetTimeOverride(time_override);
  OverrideManifest(kFileHandlerManifestTemplate, {".md\", \".txt", "red"});
  EXPECT_EQ(ManifestUpdateResult::kAppUpdated, GetResultAfterPageLoad(url));
  auto new_extensions = web_app->file_handlers()[0].accept[0].file_extensions;
  EXPECT_TRUE(base::Contains(new_extensions, ".md"));
  EXPECT_TRUE(base::Contains(new_extensions, ".txt"));

  // Set back to allowed.
  EXPECT_EQ(
      ApiApprovalState::kRequiresPrompt,
      GetProvider().registrar_unsafe().GetAppFileHandlerApprovalState(app_id));
  GetProvider().sync_bridge_unsafe().SetAppFileHandlerApprovalState(
      app_id, ApiApprovalState::kAllowed);

  // Update manifest, but keep same file handlers. Permission should be left on
  // ALLOW.
  time_override += base::Days(10);
  SetTimeOverride(time_override);
  OverrideManifest(kFileHandlerManifestTemplate, {".md\", \".txt", "blue"});
  EXPECT_EQ(ManifestUpdateResult::kAppUpdated, GetResultAfterPageLoad(url));
  new_extensions = web_app->file_handlers()[0].accept[0].file_extensions;
  EXPECT_TRUE(base::Contains(new_extensions, ".md"));
  EXPECT_TRUE(base::Contains(new_extensions, ".txt"));

  EXPECT_EQ(
      ApiApprovalState::kAllowed,
      GetProvider().registrar_unsafe().GetAppFileHandlerApprovalState(app_id));

  // Update manifest, asking for /fewer/ file types. Permission should be left
  // on ALLOW.
  time_override += base::Days(10);
  SetTimeOverride(time_override);
  OverrideManifest(kFileHandlerManifestTemplate, {".txt", "blue"});
  EXPECT_EQ(ManifestUpdateResult::kAppUpdated, GetResultAfterPageLoad(url));
  new_extensions = web_app->file_handlers()[0].accept[0].file_extensions;
  EXPECT_FALSE(base::Contains(new_extensions, ".md"));
  EXPECT_TRUE(base::Contains(new_extensions, ".txt"));
  EXPECT_EQ(
      ApiApprovalState::kAllowed,
      GetProvider().registrar_unsafe().GetAppFileHandlerApprovalState(app_id));

#if BUILDFLAG(IS_LINUX)
  // Make sure that blocking the permission also unregisters the MIME type on
  // Linux.
  SetUpdateMimeInfoDatabaseOnLinuxCallbackForTesting(base::BindLambdaForTesting(
      [](base::FilePath filename, std::string xdg_command,
         std::string file_contents) {
        EXPECT_TRUE(file_contents.empty()) << "'" << file_contents << "'";
        return true;
      }));
#endif

  // Block the permission, update manifest, permission should still be block.
  GetProvider().sync_bridge_unsafe().SetAppFileHandlerApprovalState(
      app_id, ApiApprovalState::kDisallowed);
  OverrideManifest(kFileHandlerManifestTemplate, {".txt", "red"});
  time_override += base::Days(10);
  SetTimeOverride(time_override);
  EXPECT_EQ(ManifestUpdateResult::kAppUpdated, GetResultAfterPageLoad(url));
  EXPECT_EQ(
      ApiApprovalState::kDisallowed,
      GetProvider().registrar_unsafe().GetAppFileHandlerApprovalState(app_id));
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTestWithFileHandling,
                       BlockedPermissionPreservedOnUpdate) {
  constexpr char kFileHandlerManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "minimal-ui",
      "file_handlers": [
        {
          "action": "/?plaintext",
          "name": "Plain Text",
          "accept": {
            "text/plain": ["$1"]
          }
        }
      ],
      "icons": $2
    }
  )";

  OverrideManifest(kFileHandlerManifestTemplate,
                   {".txt", kInstallableIconList});
  webapps::AppId app_id = InstallWebApp();
  WebAppRegistrar& registrar = GetProvider().registrar_unsafe();
  const WebApp* web_app = registrar.GetAppById(app_id);

  ASSERT_FALSE(web_app->file_handlers().empty());
  const auto& old_file_handler = web_app->file_handlers()[0];
  ASSERT_FALSE(old_file_handler.accept.empty());
  auto old_extensions = old_file_handler.accept[0].file_extensions;
  EXPECT_TRUE(base::Contains(old_extensions, ".txt"));
  const GURL url = GetAppURL();
  const GURL origin = url.DeprecatedGetOriginAsURL();

  // Disallow the API.
  EXPECT_EQ(
      ApiApprovalState::kRequiresPrompt,
      GetProvider().registrar_unsafe().GetAppFileHandlerApprovalState(app_id));
  GetProvider().sync_bridge_unsafe().SetAppFileHandlerApprovalState(
      app_id, ApiApprovalState::kDisallowed);

  // Update manifest.
  OverrideManifest(kFileHandlerManifestTemplate, {".md", kInstallableIconList});
  EXPECT_EQ(ManifestUpdateResult::kAppUpdated, GetResultAfterPageLoad(url));

  // Manifest update task should preserve the permission blocked state.
  EXPECT_EQ(ApiApprovalState::kDisallowed,
            registrar.GetAppById(app_id)->file_handler_approval_state());
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTestWithFileHandling,
                       CheckFindsDeletedFileHandler) {
  constexpr char kFileHandlerManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "minimal-ui",
      "file_handlers": [
        {
          "action": "/?plaintext",
          "name": "Plain Text",
          "accept": {
            "text/plain": [".txt"]
          }
        }
      ],
      "icons": $1
    }
  )";

  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "minimal-ui",
      "icons": $1
    }
  )";

  OverrideManifest(kFileHandlerManifestTemplate, {kInstallableIconList});
  webapps::AppId app_id = InstallWebApp();

  OverrideManifest(kManifestTemplate, {kInstallableIconList});
  EXPECT_EQ(ManifestUpdateResult::kAppUpdated,
            GetResultAfterPageLoad(GetAppURL()));
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);

  auto* file_handlers =
      GetProvider().registrar_unsafe().GetAppFileHandlers(app_id);
  ASSERT_TRUE(file_handlers);
  EXPECT_TRUE(file_handlers->empty());
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTestWithFileHandling,
                       CheckFileExtensionList) {
  constexpr char kFileHandlerManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "minimal-ui",
      "file_handlers": [
        {
          "action": "/?plaintext",
          "name": "Plain Text",
          "accept": {
            "text/plain": [".txt"]
          }
        }
      ],
      "icons": $1
    }
  )";

  OverrideManifest(kFileHandlerManifestTemplate, {kInstallableIconList});
  webapps::AppId app_id = InstallWebApp();

  auto [associations_list, association_count] =
      GetFileTypeAssociationsHandledByWebAppForDisplay(browser()->profile(),
                                                       app_id);
  EXPECT_EQ(u"TXT", associations_list);
  EXPECT_EQ(1U, association_count);
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTestWithFileHandling,
                       CheckFileExtensionsList) {
  constexpr char kFileHandlerManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "minimal-ui",
      "file_handlers": [
        {
          "action": "/?plaintext",
          "name": "Plain Text",
          "accept": {
            "text/plain": [".txt", ".md"]
          }
        }
      ],
      "icons": $1
    }
  )";

  OverrideManifest(kFileHandlerManifestTemplate, {kInstallableIconList});
  webapps::AppId app_id = InstallWebApp();

  auto [associations_list, association_count] =
      GetFileTypeAssociationsHandledByWebAppForDisplay(browser()->profile(),
                                                       app_id);
  EXPECT_EQ(u"MD, TXT", associations_list);
  EXPECT_EQ(2U, association_count);
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTestWithFileHandling,
                       CheckFileExtensionsListWithTwoFileHandlers) {
  constexpr char kFileHandlerManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "minimal-ui",
      "file_handlers": [
        {
          "action": "/?plaintext",
          "name": "Plain Text",
          "accept": {
            "text/plain": [".txt"]
          }
        },
        {
          "action": "/?longtype",
          "name": "Long Custom type",
          "accept": {
            "application/long-type": [".longtype"]
          }
        }
      ],
      "icons": $1
    }
  )";

  OverrideManifest(kFileHandlerManifestTemplate, {kInstallableIconList});
  webapps::AppId app_id = InstallWebApp();

  auto [associations_list, association_count] =
      GetFileTypeAssociationsHandledByWebAppForDisplay(browser()->profile(),
                                                       app_id);
  EXPECT_EQ(u"LONGTYPE, TXT", associations_list);
  EXPECT_EQ(2U, association_count);
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       CheckFindsShortcutsMenuUpdated) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "icons": $1,
      "shortcuts": $2
    }
  )";
  OverrideManifest(kManifestTemplate, {kInstallableIconList, kShortcutsItem});
  webapps::AppId app_id = InstallWebApp();

  OverrideManifest(kManifestTemplate, {kInstallableIconList, kShortcutsItems});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpdated);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);
  EXPECT_EQ(GetProvider()
                .registrar_unsafe()
                .GetAppShortcutsMenuItemInfos(app_id)
                .size(),
            2u);
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       CheckFindsItemNameUpdated) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "icons": $1,
      "shortcuts": [
        {
          "name": "$2",
          "short_name": "HM",
          "description": "Go home",
          "url": ".",
          "icons": [
            {
              "src": "/banners/image-512px.png",
              "sizes": "512x512",
              "type": "image/png"
            }
          ]
        }
      ]
    }
  )";
  OverrideManifest(kManifestTemplate, {kInstallableIconList, "Home"});
  webapps::AppId app_id = InstallWebApp();

  OverrideManifest(kManifestTemplate,
                   {kInstallableIconList, kAnotherShortcutsItemName});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpdated);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);
  EXPECT_EQ(GetProvider()
                .registrar_unsafe()
                .GetAppShortcutsMenuItemInfos(app_id)[0]
                .name,
            kAnotherShortcutsItemName16);
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       CheckIgnoresShortNameAndDescriptionChange) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "icons": $1,
      "shortcuts": [
        {
          "name": "Home",
          "short_name": "$2",
          "description": "$3",
          "url": ".",
          "icons": [
            {
              "src": "/banners/image-512px.png",
              "sizes": "512x512",
              "type": "image/png"
            }
          ]
        }
      ]
    }
  )";
  OverrideManifest(kManifestTemplate, {kInstallableIconList, "HM", "Go home"});
  webapps::AppId app_id = InstallWebApp();

  OverrideManifest(kManifestTemplate,
                   {kInstallableIconList, kAnotherShortcutsItemShortName,
                    kAnotherShortcutsItemDescription});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpToDate);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpToDate, 1);
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       CheckFindsItemUrlUpdated) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "icons": $1,
      "shortcuts": [
        {
          "name": "Home",
          "short_name": "HM",
          "description": "Go home",
          "url": "$2",
          "icons": [
            {
              "src": "/banners/image-512px.png",
              "sizes": "512x512",
              "type": "image/png"
            }
          ]
        }
      ]
    }
  )";
  OverrideManifest(kManifestTemplate, {kInstallableIconList, "/"});
  webapps::AppId app_id = InstallWebApp();

  OverrideManifest(kManifestTemplate,
                   {kInstallableIconList, kAnotherShortcutsItemUrl});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpdated);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);
  EXPECT_EQ(GetProvider()
                .registrar_unsafe()
                .GetAppShortcutsMenuItemInfos(app_id)[0]
                .url,
            http_server_.GetURL(kAnotherShortcutsItemUrl));
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       CheckFindsShortcutIconContentChange) {
  constexpr char kManifest[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "icons": $1,
      "shortcuts": [
        {
          "name": "Home",
          "short_name": "HM",
          "description": "Go home",
          "url": "/",
          "icons": [
            {
              "src": "/web_apps/basic-192.png?ignore",
              "sizes": "192x192",
              "type": "image/png"
            }
          ]
        }
      ]
    }
  )";
  OverrideManifest(kManifest, {kInstallableIconList});
  webapps::AppId app_id = InstallWebApp();

  // Replace the contents of basic-192.png with blue-192.png without changing
  // the URL.
  content::URLLoaderInterceptor url_interceptor(base::BindLambdaForTesting(
      [this](content::URLLoaderInterceptor::RequestParams* params)
          -> bool /*intercepted*/ {
        if (params->url_request.url ==
            http_server_.GetURL("/web_apps/basic-192.png?ignore")) {
          content::URLLoaderInterceptor::WriteResponse(
              "chrome/test/data/web_apps/blue-192.png", params->client.get());
          return true;
        }
        return false;
      }));

  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpdated);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);

  // Check that the installed icon is now blue.
  base::RunLoop run_loop;
  GetProvider().icon_manager().ReadAllShortcutsMenuIcons(
      app_id,
      base::BindLambdaForTesting(
          [&run_loop](ShortcutsMenuIconBitmaps shortcuts_menu_icon_bitmaps) {
            run_loop.Quit();
            EXPECT_EQ(shortcuts_menu_icon_bitmaps[0].any.at(192).getColor(0, 0),
                      SK_ColorBLUE);
          }));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       CheckIconChangeForOemInstallation) {
  constexpr char kNewName[] = "New app name";
  constexpr char kManifest[] = R"(
    {
      "name": "$1",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "icons": [
        {
          "src": "/web_apps/basic-192.png?ignore",
          "sizes": "192x192",
          "type": "image/png"
        }
      ]
    }
  )";
  OverrideManifest(kManifest, {kNewName, kInstallableIconList});
  webapps::AppId app_id = InstallOemWebApp();

  // Replace the contents of basic-192.png with blue-192.png without changing
  // the URL.
  content::URLLoaderInterceptor url_interceptor(base::BindLambdaForTesting(
      [this](content::URLLoaderInterceptor::RequestParams* params)
          -> bool /*intercepted*/ {
        if (params->url_request.url ==
            http_server_.GetURL("/web_apps/basic-192.png?ignore")) {
          content::URLLoaderInterceptor::WriteResponse(
              "chrome/test/data/web_apps/blue-192.png", params->client.get());
          return true;
        }
        return false;
      }));

  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpdated);
  EXPECT_EQ(kNewName, GetProvider().registrar_unsafe().GetAppShortName(app_id));
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);

  // Check that the installed icon is now blue.
  EXPECT_EQ(ReadAppIconPixel(app_id, /*size=*/192), SK_ColorBLUE);
}

IN_PROC_BROWSER_TEST_P(ManifestUpdateManagerBrowserTest_UpdateDialog,
                       ShortcutIconContentChangeWithProductIconChange) {
  // This test changes the shortuct icon contents and also the product icon
  // list. The shortcut icons should update. The icon should update only if
  // identity updates are allowed.
  constexpr char kManifest[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "icons": $1,
      "shortcuts": [
        {
          "name": "Home",
          "short_name": "HM",
          "description": "Go home",
          "url": "/",
          "icons": [
            {
              "src": "/web_apps/basic-192.png?ignore",
              "sizes": "192x192",
              "type": "image/png"
            }
          ]
        }
      ]
    }
  )";
  OverrideManifest(kManifest, {kInstallableIconList});
  webapps::AppId app_id = InstallWebApp();

  // Replace the contents of basic-192.png with blue-192.png without changing
  // the URL.
  content::URLLoaderInterceptor url_interceptor(base::BindLambdaForTesting(
      [this](content::URLLoaderInterceptor::RequestParams* params)
          -> bool /*intercepted*/ {
        if (params->url_request.url ==
            http_server_.GetURL("/web_apps/basic-192.png?ignore")) {
          content::URLLoaderInterceptor::WriteResponse(
              "chrome/test/data/web_apps/blue-192.png", params->client.get());
          return true;
        }
        return false;
      }));

  if (IsUpdateDialogEnabled()) {
    AcceptAppIdentityUpdateDialogForTesting();
  }

  OverrideManifest(kManifest, {kAnotherInstallableIconList});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpdated);
  // The icon should be updated only if product icon updates are allowed.
  if (IsUpdateDialogEnabled()) {
    ConfirmShortcutColors(
        app_id, {{{32, kAll}, kAnotherInstallableIconTopLeftColor},
                 {{48, kAll}, kAnotherInstallableIconTopLeftColor},
                 {{64, kWin}, kAnotherInstallableIconTopLeftColor},
                 {{96, kWin}, kAnotherInstallableIconTopLeftColor},
                 {{128, kAll}, kAnotherInstallableIconTopLeftColor},
                 {{256, kAll}, kAnotherInstallableIconTopLeftColor},
                 {{512, kNotWin}, kAnotherInstallableIconTopLeftColor}});
  } else {
    ConfirmShortcutColors(app_id,
                          {{{32, kAll}, kInstallableIconTopLeftColor},
                           {{48, kAll}, kInstallableIconTopLeftColor},
                           {{64, kWin}, kInstallableIconTopLeftColor},
                           {{96, kWin}, kInstallableIconTopLeftColor},
                           {{128, kAll}, kInstallableIconTopLeftColor},
                           {{256, kAll}, kInstallableIconTopLeftColor}});
  }
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       CheckFindsShortcutIconSrcUpdated) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "icons": $1,
      "shortcuts": [
        {
          "name": "Home",
          "short_name": "HM",
          "description": "Go home",
          "url": ".",
          "icons": [
            {
              "src": "$2",
              "sizes": "512x512",
              "type": "image/png"
            }
          ]
        }
      ]
    }
  )";
  OverrideManifest(kManifestTemplate,
                   {kInstallableIconList, "/banners/image-512px.png"});
  webapps::AppId app_id = InstallWebApp();

  OverrideManifest(kManifestTemplate, {kInstallableIconList, kAnotherIconSrc});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpdated);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);
  EXPECT_EQ(GetProvider()
                .registrar_unsafe()
                .GetAppShortcutsMenuItemInfos(app_id)[0]
                .GetShortcutIconInfosForPurpose(IconPurpose::ANY)[0]
                .url,
            http_server_.GetURL(kAnotherIconSrc));
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       CheckFindsShortcutIconSizesUpdated) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "icons": $1,
      "shortcuts": [
        {
          "name": "Home",
          "short_name": "HM",
          "description": "Go home",
          "url": ".",
          "icons": [
            {
              "src": "/banners/image-512px.png",
              "sizes": "$2",
              "type": "image/png"
            }
          ]
        }
      ]
    }
  )";
  OverrideManifest(kManifestTemplate, {kInstallableIconList, "512x512"});
  webapps::AppId app_id = InstallWebApp();

  OverrideManifest(kManifestTemplate,
                   {kInstallableIconList,
                    gfx::Size(kAnotherIconSize, kAnotherIconSize).ToString()});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpdated);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);
  EXPECT_EQ(GetProvider()
                .registrar_unsafe()
                .GetAppShortcutsMenuItemInfos(app_id)[0]
                .GetShortcutIconInfosForPurpose(IconPurpose::ANY)[0]
                .square_size_px,
            kAnotherIconSize);
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       CheckUpdateTimeChange) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "icons": $1,
      "theme_color": "$2"
    }
  )";
  OverrideManifest(kManifestTemplate, {kInstallableIconList, "blue"});
  webapps::AppId app_id = InstallWebApp();
  const WebApp* web_app = GetProvider().registrar_unsafe().GetAppById(app_id);
  base::Time manifest_update_time = web_app->manifest_update_time();

  // CSS #RRGGBBAA syntax.
  OverrideManifest(kManifestTemplate, {kInstallableIconList, "#00FF00F0"});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpdated);

  // Update time is updated.
  EXPECT_LT(manifest_update_time, web_app->manifest_update_time());
}

class ManifestUpdateManagerIconUpdatingBrowserTest
    : public ManifestUpdateManagerBrowserTest {
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kWebAppManifestIconUpdating};
};

IN_PROC_BROWSER_TEST_P(ManifestUpdateManagerBrowserTest_UpdateDialog,
                       CheckFindsIconContentChange) {
  constexpr char kManifest[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "icons": [
        {
          "src": "/banners/256x256-green.png?ignore",
          "sizes": "256x256",
          "type": "image/png"
        }
      ]
    }
  )";

  if (IsUpdateDialogEnabled()) {
    AcceptAppIdentityUpdateDialogForTesting();
  }

  OverrideManifest(kManifest, {});
  webapps::AppId app_id = InstallWebApp();

  // Replace the green icon with a red icon without changing the URL.
  content::URLLoaderInterceptor url_interceptor(base::BindLambdaForTesting(
      [this](content::URLLoaderInterceptor::RequestParams* params)
          -> bool /*intercepted*/ {
        if (params->url_request.url ==
            http_server_.GetURL("/banners/256x256-green.png?ignore")) {
          content::URLLoaderInterceptor::WriteResponse(
              "chrome/test/data/banners/256x256-red.png", params->client.get());
          return true;
        }
        return false;
      }));

  if (IsUpdateDialogEnabled()) {
    EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
              ManifestUpdateResult::kAppUpdated);
    histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                        ManifestUpdateResult::kAppUpdated, 1);
    // The icon should have changed, as the file has been updated (but the url
    // is the same).
    ConfirmShortcutColors(app_id, {{{32, kAll}, SK_ColorRED},
                                   {{48, kAll}, SK_ColorRED},
                                   {{64, kWin}, SK_ColorRED},
                                   {{96, kWin}, SK_ColorRED},
                                   {{128, kAll}, SK_ColorRED},
                                   {{256, kAll}, SK_ColorRED}});

    EXPECT_EQ(ReadAppIconPixel(app_id, /*size=*/256), SK_ColorRED);
  } else {
    EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
              ManifestUpdateResult::kAppUpToDate);
    histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                        ManifestUpdateResult::kAppUpdated, 0);
    ConfirmShortcutColors(app_id, {{{32, kAll}, SK_ColorGREEN},
                                   {{48, kAll}, SK_ColorGREEN},
                                   {{64, kWin}, SK_ColorGREEN},
                                   {{96, kWin}, SK_ColorGREEN},
                                   {{128, kAll}, SK_ColorGREEN},
                                   {{256, kAll}, SK_ColorGREEN}});

    EXPECT_EQ(ReadAppIconPixel(app_id, /*size=*/256), SK_ColorGREEN);
  }
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerIconUpdatingBrowserTest,
                       CheckFindsIconUrlChange) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "icons": $1
    }
  )";
  OverrideManifest(kManifestTemplate, {kInstallableIconList});
  webapps::AppId app_id = InstallWebApp();

  OverrideManifest(kManifestTemplate, {kAnotherInstallableIconList});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpdated);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);

  histogram_tester_.ExpectBucketCount("WebApp.Icon.DownloadedResultOnUpdate",
                                      IconsDownloadedResult::kCompleted, 1);

  histogram_tester_.ExpectBucketCount(
      "WebApp.Icon.DownloadedHttpStatusCodeOnUpdate",
      net::HttpStatusCode::HTTP_OK, 1);

  // The icon should have changed.
  ConfirmShortcutColors(
      app_id, {{{32, kAll}, kAnotherInstallableIconTopLeftColor},
               {{48, kAll}, kAnotherInstallableIconTopLeftColor},
               {{64, kWin}, kAnotherInstallableIconTopLeftColor},
               {{96, kWin}, kAnotherInstallableIconTopLeftColor},
               {{128, kAll}, kAnotherInstallableIconTopLeftColor},
               {{256, kAll}, kAnotherInstallableIconTopLeftColor},
               {{512, kNotWin}, kAnotherInstallableIconTopLeftColor}});
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerIconUpdatingBrowserTest,
                       CheckIgnoresIconDownloadFail) {
  constexpr char kManifest[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "icons": [
        {
          "src": "/web_apps/basic-48.png?ignore",
          "sizes": "48x48",
          "type": "image/png"
        },
        {
          "src": "/web_apps/basic-192.png?ignore",
          "sizes": "192x192",
          "type": "image/png"
        }
      ]
    }
  )";
  OverrideManifest(kManifest, {});
  webapps::AppId app_id = InstallWebApp();

  histogram_tester_.ExpectBucketCount("WebApp.Icon.DownloadedResultOnCreate",
                                      IconsDownloadedResult::kCompleted, 1);

  histogram_tester_.ExpectBucketCount(
      "WebApp.Icon.DownloadedHttpStatusCodeOnCreate",
      net::HttpStatusCode::HTTP_OK, 1);

  // Make basic-48.png fail to download.
  // Replace the contents of basic-192.png with blue-192.png without changing
  // the URL.
  content::URLLoaderInterceptor url_interceptor(base::BindLambdaForTesting(
      [this](content::URLLoaderInterceptor::RequestParams* params)
          -> bool /*intercepted*/ {
        if (params->url_request.url ==
            http_server_.GetURL("/web_apps/basic-48.png?ignore")) {
          content::URLLoaderInterceptor::WriteResponse("malformed response", "",
                                                       params->client.get());
          return true;
        }
        if (params->url_request.url ==
            http_server_.GetURL("/web_apps/basic-192.png?ignore")) {
          content::URLLoaderInterceptor::WriteResponse(
              "chrome/test/data/web_apps/blue-192.png", params->client.get());
          return true;
        }
        return false;
      }));

  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kIconDownloadFailed);
  histogram_tester_.ExpectBucketCount(
      kUpdateHistogramName, ManifestUpdateResult::kIconDownloadFailed, 1);

  // The `url_interceptor` above can't simulate net::HttpStatusCode error
  // properly, WebApp.Icon.DownloadedHttpStatusCodeOnUpdate left untested here.
  histogram_tester_.ExpectBucketCount(
      "WebApp.Icon.DownloadedResultOnUpdate",
      IconsDownloadedResult::kAbortedDueToFailure, 1);

  // Since one request failed, none of the icons should be updated. So the '192'
  // size here is not updated to blue.
  EXPECT_EQ(ReadAppIconPixel(app_id, /*size=*/48), SK_ColorBLACK);
  EXPECT_EQ(ReadAppIconPixel(app_id, /*size=*/192), SK_ColorBLACK);
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       CheckFindsAddedProtocolHandler) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "minimal-ui",
      "icons": $1
    }
  )";

  constexpr char kProtocolHandlerManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "minimal-ui",
      "protocol_handlers": [
        {
          "protocol": "mailto",
          "url": "?mailto=%s"
        }
      ],
      "icons": $1
    }
  )";

  OverrideManifest(kManifestTemplate, {kInstallableIconList});
  webapps::AppId app_id = InstallWebApp();

  OverrideManifest(kProtocolHandlerManifestTemplate, {kInstallableIconList});
  EXPECT_EQ(ManifestUpdateResult::kAppUpdated,
            GetResultAfterPageLoad(GetAppURL()));
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);

  const WebApp* web_app = GetProvider().registrar_unsafe().GetAppById(app_id);
  EXPECT_FALSE(web_app->protocol_handlers().empty());
  const auto& protocol_handler = web_app->protocol_handlers()[0];
  EXPECT_EQ("mailto", protocol_handler.protocol);
  EXPECT_EQ(http_server_.GetURL("/banners/manifest.json?mailto=%s"),
            protocol_handler.url.spec());
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       CheckIgnoresUnchangedProtocolHandler) {
  constexpr char kProtocolHandlerManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "minimal-ui",
      "protocol_handlers": [
        {
          "protocol": "mailto",
          "url": "?mailto=%s"
        }
      ],
      "icons": $1
    }
  )";

  OverrideManifest(kProtocolHandlerManifestTemplate, {kInstallableIconList});
  webapps::AppId app_id = InstallWebApp();

  OverrideManifest(kProtocolHandlerManifestTemplate, {kInstallableIconList});
  EXPECT_EQ(ManifestUpdateResult::kAppUpToDate,
            GetResultAfterPageLoad(GetAppURL()));
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpToDate, 1);

  const WebApp* web_app = GetProvider().registrar_unsafe().GetAppById(app_id);
  EXPECT_FALSE(web_app->protocol_handlers().empty());
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       CheckFindsChangedProtocolHandler) {
  constexpr char kProtocolHandlerManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "minimal-ui",
      "protocol_handlers": [
        {
          "protocol": "$1",
          "url": "?$2=%s"
        }
      ],
      "icons": $3
    }
  )";

  OverrideManifest(kProtocolHandlerManifestTemplate,
                   {"mailto", "mailto", kInstallableIconList});
  webapps::AppId app_id = InstallWebApp();
  const WebApp* web_app = GetProvider().registrar_unsafe().GetAppById(app_id);
  EXPECT_EQ(1u, web_app->protocol_handlers().size());
  const auto& old_protocol_handler = web_app->protocol_handlers()[0];
  EXPECT_EQ("mailto", old_protocol_handler.protocol);
  EXPECT_EQ(http_server_.GetURL("/banners/manifest.json?mailto=%s"),
            old_protocol_handler.url.spec());

  OverrideManifest(kProtocolHandlerManifestTemplate,
                   {"web+mailto", "web+mailto", kInstallableIconList});
  EXPECT_EQ(ManifestUpdateResult::kAppUpdated,
            GetResultAfterPageLoad(GetAppURL()));
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);

  EXPECT_EQ(1u, web_app->protocol_handlers().size());
  const auto& new_protocol_handler = web_app->protocol_handlers()[0];
  EXPECT_EQ("web+mailto", new_protocol_handler.protocol);
  EXPECT_EQ(http_server_.GetURL("/banners/manifest.json?web+mailto=%s"),
            new_protocol_handler.url.spec());
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       CheckFindsDeletedProtocolHandler) {
  constexpr char kProtocolHandlerManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "minimal-ui",
      "protocol_handlers": [
        {
          "protocol": "mailto",
          "url": "?mailto=%s"
        }
      ],
      "icons": $1
    }
  )";

  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "minimal-ui",
      "icons": $1
    }
  )";

  OverrideManifest(kProtocolHandlerManifestTemplate, {kInstallableIconList});
  webapps::AppId app_id = InstallWebApp();

  OverrideManifest(kManifestTemplate, {kInstallableIconList});
  EXPECT_EQ(ManifestUpdateResult::kAppUpdated,
            GetResultAfterPageLoad(GetAppURL()));
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);

  const WebApp* web_app = GetProvider().registrar_unsafe().GetAppById(app_id);
  EXPECT_TRUE(web_app->protocol_handlers().empty());
}

class ManifestUpdateManagerBrowserTest_LockScreen
    : public ManifestUpdateManagerBrowserTest {
 public:
  ManifestUpdateManagerBrowserTest_LockScreen() {
    feature_list_.InitWithFeatures({features::kWebLockScreenApi,
                                    blink::features::kWebAppManifestLockScreen},
                                   /*disabled_features=*/{});
  }
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest_LockScreen,
                       CheckFindsAddedLockScreenStartUrl) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "minimal-ui",
      "icons": $1
    }
  )";

  constexpr char kLockScreenStartUrlManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "minimal-ui",
      "lock_screen": {
        "start_url": "/lock-screen-start"
      },
      "icons": $1
    }
  )";

  OverrideManifest(kManifestTemplate, {kInstallableIconList});
  webapps::AppId app_id = InstallWebApp();
  const WebApp* web_app = GetProvider().registrar_unsafe().GetAppById(app_id);
  EXPECT_TRUE(web_app->lock_screen_start_url().is_empty());

  OverrideManifest(kLockScreenStartUrlManifestTemplate, {kInstallableIconList});
  EXPECT_EQ(ManifestUpdateResult::kAppUpdated,
            GetResultAfterPageLoad(GetAppURL()));
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);
  EXPECT_EQ(http_server_.GetURL("/lock-screen-start"),
            web_app->lock_screen_start_url().spec());
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest_LockScreen,
                       CheckIgnoresUnchangedLockScreenStartUrl) {
  constexpr char kLockScreenStartUrlManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "minimal-ui",
      "lock_screen": {
        "start_url": "/lock-screen-start"
      },
      "icons": $1
    }
  )";

  OverrideManifest(kLockScreenStartUrlManifestTemplate, {kInstallableIconList});
  webapps::AppId app_id = InstallWebApp();
  const WebApp* web_app = GetProvider().registrar_unsafe().GetAppById(app_id);
  EXPECT_EQ(http_server_.GetURL("/lock-screen-start"),
            web_app->lock_screen_start_url().spec());

  OverrideManifest(kLockScreenStartUrlManifestTemplate, {kInstallableIconList});
  EXPECT_EQ(ManifestUpdateResult::kAppUpToDate,
            GetResultAfterPageLoad(GetAppURL()));
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpToDate, 1);
  EXPECT_EQ(http_server_.GetURL("/lock-screen-start"),
            web_app->lock_screen_start_url().spec());
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest_LockScreen,
                       CheckFindsChangedLockScreenStartUrl) {
  constexpr char kLockScreenStartUrlManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "minimal-ui",
      "lock_screen": {
        "start_url": "$1"
      },
      "icons": $2
    }
  )";

  OverrideManifest(kLockScreenStartUrlManifestTemplate,
                   {"old-relative-url", kInstallableIconList});
  webapps::AppId app_id = InstallWebApp();
  const WebApp* web_app = GetProvider().registrar_unsafe().GetAppById(app_id);
  // URL parsed relative to manifest URL, which is in /banners/.
  EXPECT_EQ(http_server_.GetURL("/banners/old-relative-url"),
            web_app->lock_screen_start_url().spec());

  OverrideManifest(kLockScreenStartUrlManifestTemplate,
                   {"/lock-screen-starter", kInstallableIconList});
  EXPECT_EQ(ManifestUpdateResult::kAppUpdated,
            GetResultAfterPageLoad(GetAppURL()));
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);
  EXPECT_EQ(http_server_.GetURL("/lock-screen-starter"),
            web_app->lock_screen_start_url().spec());
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest_LockScreen,
                       CheckFindsDeletedLockScreenStartUrl) {
  constexpr char kLockScreenStartUrlManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "minimal-ui",
      "lock_screen": {
        "start_url": "/lock-screen-start"
      },
      "icons": $1
    }
  )";

  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "minimal-ui",
      "icons": $1
    }
  )";

  OverrideManifest(kLockScreenStartUrlManifestTemplate, {kInstallableIconList});
  webapps::AppId app_id = InstallWebApp();
  const WebApp* web_app = GetProvider().registrar_unsafe().GetAppById(app_id);
  EXPECT_FALSE(web_app->lock_screen_start_url().is_empty());

  OverrideManifest(kManifestTemplate, {kInstallableIconList});
  EXPECT_EQ(ManifestUpdateResult::kAppUpdated,
            GetResultAfterPageLoad(GetAppURL()));
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);
  EXPECT_TRUE(web_app->lock_screen_start_url().is_empty());
}

class ManifestUpdateManagerBrowserTest_NoLockScreen
    : public ManifestUpdateManagerBrowserTest {
 public:
  ManifestUpdateManagerBrowserTest_NoLockScreen() {
    feature_list_.InitAndDisableFeature(
        blink::features::kWebAppManifestLockScreen);
  }
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest_NoLockScreen,
                       WithoutLockScreenFlag_CheckIgnoresLockScreenStartUrl) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "minimal-ui",
      "icons": $1
    }
  )";

  constexpr char kLockScreenStartUrlManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "minimal-ui",
      "lock_screen": {
        "start_url": "/lock-screen-start"
      },
      "icons": $1
    }
  )";

  OverrideManifest(kManifestTemplate, {kInstallableIconList});
  webapps::AppId app_id = InstallWebApp();
  const WebApp* web_app = GetProvider().registrar_unsafe().GetAppById(app_id);
  EXPECT_TRUE(web_app->lock_screen_start_url().is_empty());

  OverrideManifest(kLockScreenStartUrlManifestTemplate, {kInstallableIconList});
  EXPECT_EQ(ManifestUpdateResult::kAppUpToDate,
            GetResultAfterPageLoad(GetAppURL()));
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpToDate, 1);
  EXPECT_TRUE(web_app->lock_screen_start_url().is_empty());
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       CheckFindsAddedNewNoteUrl) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "minimal-ui",
      "icons": $1
    }
  )";

  constexpr char kNewNoteUrlManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "minimal-ui",
      "note_taking": {
        "new_note_url": "/new"
      },
      "icons": $1
    }
  )";

  OverrideManifest(kManifestTemplate, {kInstallableIconList});
  webapps::AppId app_id = InstallWebApp();
  const WebApp* web_app = GetProvider().registrar_unsafe().GetAppById(app_id);
  EXPECT_TRUE(web_app->note_taking_new_note_url().is_empty());

  OverrideManifest(kNewNoteUrlManifestTemplate, {kInstallableIconList});
  EXPECT_EQ(ManifestUpdateResult::kAppUpdated,
            GetResultAfterPageLoad(GetAppURL()));
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);
  EXPECT_EQ(http_server_.GetURL("/new"),
            web_app->note_taking_new_note_url().spec());
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       CheckIgnoresUnchangedNewNoteUrl) {
  constexpr char kNewNoteUrlManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "minimal-ui",
      "note_taking": {
        "new_note_url": "/new"
      },
      "icons": $1
    }
  )";

  OverrideManifest(kNewNoteUrlManifestTemplate, {kInstallableIconList});
  webapps::AppId app_id = InstallWebApp();
  const WebApp* web_app = GetProvider().registrar_unsafe().GetAppById(app_id);
  EXPECT_EQ(http_server_.GetURL("/new"),
            web_app->note_taking_new_note_url().spec());

  OverrideManifest(kNewNoteUrlManifestTemplate, {kInstallableIconList});
  EXPECT_EQ(ManifestUpdateResult::kAppUpToDate,
            GetResultAfterPageLoad(GetAppURL()));
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpToDate, 1);
  EXPECT_EQ(http_server_.GetURL("/new"),
            web_app->note_taking_new_note_url().spec());
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       CheckFindsChangedNewNoteUrl) {
  constexpr char kNewNoteUrlManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "minimal-ui",
      "note_taking": {
        "new_note_url": "$1"
      },
      "icons": $2
    }
  )";

  OverrideManifest(kNewNoteUrlManifestTemplate,
                   {"old-relative-url", kInstallableIconList});
  webapps::AppId app_id = InstallWebApp();
  const WebApp* web_app = GetProvider().registrar_unsafe().GetAppById(app_id);
  // URL parsed relative to manifest URL, which is in /banners/.
  EXPECT_EQ(http_server_.GetURL("/banners/old-relative-url"),
            web_app->note_taking_new_note_url().spec());

  OverrideManifest(kNewNoteUrlManifestTemplate,
                   {"/newer", kInstallableIconList});
  EXPECT_EQ(ManifestUpdateResult::kAppUpdated,
            GetResultAfterPageLoad(GetAppURL()));
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);
  EXPECT_EQ(http_server_.GetURL("/newer"),
            web_app->note_taking_new_note_url().spec());
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest,
                       CheckFindsDeletedNewNoteUrl) {
  constexpr char kNewNoteUrlManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "minimal-ui",
      "note_taking": {
        "new_note_url": "/new"
      },
      "icons": $1
    }
  )";

  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "minimal-ui",
      "icons": $1
    }
  )";

  OverrideManifest(kNewNoteUrlManifestTemplate, {kInstallableIconList});
  webapps::AppId app_id = InstallWebApp();
  const WebApp* web_app = GetProvider().registrar_unsafe().GetAppById(app_id);
  EXPECT_FALSE(web_app->note_taking_new_note_url().is_empty());

  OverrideManifest(kManifestTemplate, {kInstallableIconList});
  EXPECT_EQ(ManifestUpdateResult::kAppUpdated,
            GetResultAfterPageLoad(GetAppURL()));
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);
  EXPECT_TRUE(web_app->note_taking_new_note_url().is_empty());
}

using ManifestUpdateManagerBrowserTest_ManifestId =
    ManifestUpdateManagerBrowserTest;

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest_ManifestId,
                       AllowStartUrlUpdate) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": "$1",
      "scope": "/",
      "display": "minimal-ui",
      "id": "test",
      "icons": $2
    }
  )";
  OverrideManifest(kManifestTemplate, {"/startA", kInstallableIconList});
  webapps::AppId app_id = InstallWebApp();
  EXPECT_EQ(GetProvider().registrar_unsafe().GetAppStartUrl(app_id).path(),
            "/startA");

  OverrideManifest(kManifestTemplate, {"/startB", kInstallableIconList});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpdated);
  EXPECT_EQ(GetProvider().registrar_unsafe().GetAppStartUrl(app_id).path(),
            "/startB");
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest_ManifestId,
                       CheckIgnoresIdChange) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "id": "$1",
      "start_url": "start",
      "scope": "/",
      "display": "standalone",
      "icons": $2
    }
  )";
  OverrideManifest(kManifestTemplate, {"test", kInstallableIconList});
  webapps::AppId app_id = InstallWebApp();

  OverrideManifest(kManifestTemplate, {"testb", kInstallableIconList});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppIdMismatch);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppIdMismatch, 1);
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest_ManifestId,
                       ChecksSettingIdMatchDefault) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": "/start",
      "scope": "/",
      "display": "standalone",
      "icons": $1
    }
  )";
  OverrideManifest(kManifestTemplate, {kInstallableIconList});
  webapps::AppId app_id = InstallWebApp();

  // manifest_id should default to start_url when it's not provided in manifest.
  EXPECT_EQ(GetProvider().registrar_unsafe().GetAppById(app_id)->manifest_id(),
            http_server_.GetURL("/start"));

  constexpr char kManifestTemplate2[] = R"(
    {
      "name": "Test app name",
      "id": "$1",
      "start_url": "/start",
      "scope": "/",
      "display": "standalone",
      "icons": $2
    }
  )";

  // Setting manifest id to match default value won't trigger update as the
  // parsed manifest is the same.
  OverrideManifest(kManifestTemplate2, {"start", kInstallableIconList});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpToDate);

  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpToDate, 1);
}

class ManifestUpdateManagerBrowserTest_ScopeExtensions
    : public ManifestUpdateManagerBrowserTest {
 public:
  static constexpr char kScopeExtensionsManifestTemplate[] = R"(
    {
      "name": "test app name",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "icons": $1,
      "scope_extensions": $2
    }
  )";

  void SetUpOnMainThread() override {
    ManifestUpdateManagerBrowserTest::SetUpOnMainThread();

    auto origin_association_fetcher =
        std::make_unique<webapps::TestWebAppOriginAssociationFetcher>();
    GetProvider().origin_association_manager().SetFetcherForTest(
        std::move(origin_association_fetcher));
  }

  ScopeExtensions GetScopeExtensions(const webapps::AppId& app_id) {
    return GetProvider()
        .registrar_unsafe()
        .GetAppById(app_id)
        ->scope_extensions();
  }

  ScopeExtensions GetValidatedScopeExtensions(const webapps::AppId& app_id) {
    return GetProvider()
        .registrar_unsafe()
        .GetAppById(app_id)
        ->validated_scope_extensions();
  }

  std::string OriginAssociationFileFromAppIdentity(const GURL& app_identity) {
    constexpr char kOriginAssociationTemplate[] = R"(
    {
      "web_apps": [
        {
          "web_app_identity": "$1"
        }
      ]
    })";
    return base::ReplaceStringPlaceholders(kOriginAssociationTemplate,
                                           {app_identity.spec()}, nullptr);
  }

  void SetOriginAssociationData(
      const std::map<url::Origin, std::string>& data) {
    auto& test_fetcher =
        static_cast<webapps::TestWebAppOriginAssociationFetcher&>(
            GetProvider().origin_association_manager().GetFetcherForTest());
    test_fetcher.SetData(data);
  }

  void OverrideScopeExtensions(const std::string& substitution) {
    OverrideManifest(kScopeExtensionsManifestTemplate,
                     {kInstallableIconList, substitution});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      blink::features::kWebAppEnableScopeExtensions};
};

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest_ScopeExtensions,
                       AddedScopeExtensionsWithAssociation) {
  // Install with no scope_extensions.
  OverrideScopeExtensions(R"([])");
  webapps::AppId app_id = InstallWebApp();
  EXPECT_EQ(GetScopeExtensions(app_id), ScopeExtensions());
  EXPECT_EQ(GetValidatedScopeExtensions(app_id), ScopeExtensions());

  // update with 1 entry in scope_extensions.
  OverrideScopeExtensions(R"(
          [
            {
              "origin": "https://extension.com"
            }
          ]
      )");
  // Association file of extension.com confirms association with the installed
  // app.
  SetOriginAssociationData({{url::Origin::Create(GURL("https://extension.com")),
                             OriginAssociationFileFromAppIdentity(
                                 GetAppURL().GetWithoutFilename())}});
  // Check that changes in manifest's scope_extensions causes a successful
  // update.
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpdated);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);
  // Check that origin association validation succeeded with extension.com.
  ScopeExtensions expected_extensions = ScopeExtensions(
      {ScopeExtensionInfo(url::Origin::Create(GURL("https://extension.com")))});
  EXPECT_EQ(GetScopeExtensions(app_id), expected_extensions);
  EXPECT_EQ(GetValidatedScopeExtensions(app_id), expected_extensions);
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest_ScopeExtensions,
                       AddedScopeExtensionsWithoutAssociation) {
  // Install with no scope_extensions.
  OverrideScopeExtensions(R"([])");
  webapps::AppId app_id = InstallWebApp();
  EXPECT_EQ(GetScopeExtensions(app_id), ScopeExtensions());
  EXPECT_EQ(GetValidatedScopeExtensions(app_id), ScopeExtensions());

  // Update with 1 entry in scope_extensions.
  OverrideScopeExtensions(R"(
          [
            {
              "origin": "https://extension.com"
            }
          ]
      )");
  // Association is not validated by extension.com.
  SetOriginAssociationData({});

  // Check that changes in manifest's scope_extensions causes a successful
  // update.
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpdated);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);
  ScopeExtensions expected_extensions = ScopeExtensions(
      {ScopeExtensionInfo(url::Origin::Create(GURL("https://extension.com")))});
  EXPECT_EQ(GetValidatedScopeExtensions(app_id), ScopeExtensions());
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest_ScopeExtensions,
                       RemovedScopeExtensions) {
  // Install app with valid scope_extensions.
  OverrideScopeExtensions(R"(
          [
            {
              "origin": "https://extension.com"
            }
          ]
      )");

  SetOriginAssociationData({{url::Origin::Create(GURL("https://extension.com")),
                             OriginAssociationFileFromAppIdentity(
                                 GetAppURL().GetWithoutFilename())}});
  webapps::AppId app_id = InstallWebApp();
  ScopeExtensions expected_extensions = ScopeExtensions(
      {ScopeExtensionInfo(url::Origin::Create(GURL("https://extension.com")))});
  EXPECT_EQ(GetScopeExtensions(app_id), expected_extensions);

  // Update with empty scope_extensions.
  OverrideScopeExtensions(R"([])");

  // Check that changes in manifest's scope_extensions caused successful update.
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpdated);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);
  // Check that origin association validation succeeded with extension.com.
  EXPECT_EQ(GetScopeExtensions(app_id), ScopeExtensions());
  EXPECT_EQ(GetValidatedScopeExtensions(app_id), ScopeExtensions());
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest_ScopeExtensions,
                       AddedAndRemovedScopeExtensions) {
  // Install app with valid scope_extensions.
  OverrideScopeExtensions(R"(
          [
            {
              "origin": "https://extension_1.com"
            }
          ]
      )");
  SetOriginAssociationData(
      {{url::Origin::Create(GURL("https://extension_1.com")),
        OriginAssociationFileFromAppIdentity(
            GetAppURL().GetWithoutFilename())}});
  webapps::AppId app_id = InstallWebApp();

  ScopeExtensions expected_extensions = ScopeExtensions({ScopeExtensionInfo(
      url::Origin::Create(GURL("https://extension_1.com")))});
  EXPECT_EQ(GetScopeExtensions(app_id), expected_extensions);
  EXPECT_EQ(GetValidatedScopeExtensions(app_id), expected_extensions);

  // Update with empty scope_extensions.
  OverrideScopeExtensions(R"(
          [
            {
              "origin": "https://extension_2.com"
            }
          ]
      )");
  SetOriginAssociationData(
      {{url::Origin::Create(GURL("https://extension_2.com")),
        OriginAssociationFileFromAppIdentity(
            GetAppURL().GetWithoutFilename())}});
  // Check that changes in manifest's scope_extensions caused successful update.
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpdated);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);
  // Check that origin association validation succeeded with extension.com.
  expected_extensions = ScopeExtensions({ScopeExtensionInfo(
      url::Origin::Create(GURL("https://extension_2.com")))});
  EXPECT_EQ(GetScopeExtensions(app_id), expected_extensions);
  EXPECT_EQ(GetValidatedScopeExtensions(app_id), expected_extensions);
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest_ScopeExtensions,
                       AssociationFailsToValidateDuringUpdate) {
  // Install app with valid scope_extensions.
  OverrideScopeExtensions(R"(
          [
            {
              "origin": "https://extension.com"
            }
          ]
      )");
  SetOriginAssociationData({{url::Origin::Create(GURL("https://extension.com")),
                             OriginAssociationFileFromAppIdentity(
                                 GetAppURL().GetWithoutFilename())}});

  webapps::AppId app_id = InstallWebApp();
  ScopeExtensions expected_extensions = ScopeExtensions(
      {ScopeExtensionInfo(url::Origin::Create(GURL("https://extension.com")))});
  EXPECT_EQ(GetScopeExtensions(app_id), expected_extensions);

  // Check that failure to validate origin associations caused successful
  // update.
  SetOriginAssociationData({});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpdated);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);
  // Check that origin association validation succeeded with extension.com.
  expected_extensions = ScopeExtensions(
      {ScopeExtensionInfo(url::Origin::Create(GURL("https://extension.com")))});
  EXPECT_EQ(GetScopeExtensions(app_id), expected_extensions);
  EXPECT_EQ(GetValidatedScopeExtensions(app_id), ScopeExtensions());
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest_ScopeExtensions,
                       FailedOriginAssociationValidatationPassesDuringUpdate) {
  // Install app with valid scope_extensions.
  OverrideScopeExtensions(R"(
          [
            {
              "origin": "https://extension.com"
            }
          ]
      )");
  // Validation should fail during initial install.
  SetOriginAssociationData({});
  webapps::AppId app_id = InstallWebApp();
  ScopeExtensions expected_extensions = ScopeExtensions(
      {ScopeExtensionInfo(url::Origin::Create(GURL("https://extension.com")))});
  EXPECT_EQ(GetScopeExtensions(app_id), expected_extensions);
  EXPECT_EQ(GetValidatedScopeExtensions(app_id), ScopeExtensions());

  // Check that successful validation caused successful update.
  SetOriginAssociationData({{url::Origin::Create(GURL("https://extension.com")),
                             OriginAssociationFileFromAppIdentity(
                                 GetAppURL().GetWithoutFilename())}});

  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpdated);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);
  EXPECT_EQ(GetScopeExtensions(app_id), expected_extensions);
  EXPECT_EQ(GetValidatedScopeExtensions(app_id), expected_extensions);
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest_ScopeExtensions,
                       UnrelatedAssociationDataDoesNotCauseUpdate) {
  // Install app with valid scope_extensions.
  OverrideScopeExtensions(R"(
          [
            {
              "origin": "https://extension.com"
            }
          ]
      )");
  // Validation should fail during initial install.
  SetOriginAssociationData({});
  webapps::AppId app_id = InstallWebApp();
  ScopeExtensions expected_extensions = ScopeExtensions(
      {ScopeExtensionInfo(url::Origin::Create(GURL("https://extension.com")))});
  EXPECT_EQ(GetScopeExtensions(app_id), expected_extensions);
  EXPECT_EQ(GetValidatedScopeExtensions(app_id), ScopeExtensions());

  // Recognize another unrelated app identity.
  SetOriginAssociationData(
      {{url::Origin::Create(GURL("https://extension.com")),
        OriginAssociationFileFromAppIdentity(GURL("https://unrelated.app"))}});
  EXPECT_EQ(GetResultAfterPageLoad(GetAppURL()),
            ManifestUpdateResult::kAppUpToDate);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 0);
  EXPECT_EQ(GetScopeExtensions(app_id), expected_extensions);
  EXPECT_EQ(GetValidatedScopeExtensions(app_id), ScopeExtensions());
}

class ManifestUpdateManagerAppIdentityBrowserTest
    : public ManifestUpdateManagerBrowserTest {
 public:
  ManifestUpdateManagerAppIdentityBrowserTest() = default;

 protected:
  webapps::AppId InstallShortcutAppForCurrentUrl(
      Browser* browser,
      bool open_as_window = false,
      const char* override_title = nullptr) {
    SetAutoAcceptWebAppDialogForTesting(
        /*auto_accept=*/true,
        /*auto_open_in_window=*/open_as_window);
    SetOverrideTitleForTesting(override_title);
    WebAppTestInstallWithOsHooksObserver observer(browser->profile());
    observer.BeginListening();
    CHECK(chrome::ExecuteCommand(browser, IDC_CREATE_SHORTCUT));
    webapps::AppId app_id = observer.Wait();
    SetAutoAcceptWebAppDialogForTesting(false, false);
    SetOverrideTitleForTesting(nullptr);
    return app_id;
  }

  base::test::ScopedFeatureList scoped_feature_list_{
      features::kPwaUpdateDialogForIcon};
};

// This test verifies that shortcut apps with custom name overrides don't try to
// update the name back to the manifest app name.
IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerAppIdentityBrowserTest,
                       CheckShortcutAppDoesntPromptForUpdates) {
#if !BUILDFLAG(IS_CHROMEOS)
  if (base::FeatureList::IsEnabled(features::kShortcutsNotApps)) {
    GTEST_SKIP()
        << "Shortcuts are not web apps when ShortcutsNotApps is enabled.";
  }
#endif

  constexpr char kAppName[] = "Test app";
  constexpr char kOverrideName[] = "Override name";

  // Override with a manifest that is missing a few things, so it is not
  // installable, but we can create a shortcut for it.
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "$1",
      "icons": [{
        "src": "$2",
        "sizes": "256x256",
        "type": "image/png"
      }],
      "scope": "/banners/",
      "start_url": "/banners/"
    }
  )";

  OverrideManifest(kManifestTemplate, {kAppName, "256x256-red.png"});

  GURL app_url = GetAppURL();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), app_url));

  // Install a shortcut to the app, but use a different name for it (necessary
  // to reproduce the bug).
  webapps::AppId app_id =
      InstallShortcutAppForCurrentUrl(browser(), false, kOverrideName);

  // The app installed should be the only app installed.
  auto app_ids = GetProvider().registrar_unsafe().GetAppIds();
  ASSERT_EQ(1u, app_ids.size());
  ASSERT_EQ(app_id, app_ids[0]);
  EXPECT_EQ(kOverrideName,
            GetProvider().registrar_unsafe().GetAppShortName(app_id));

  // Expect no updates with a custom name and out of date icons.
  OverrideManifest(kManifestTemplate, {kAppName, "256x256-green.png"});

  views::AnyWidgetObserver observer(views::test::AnyWidgetTestPasskey{});
  observer.set_shown_callback(
      base::BindLambdaForTesting([&](views::Widget* widget) {
        // If the App Identity dialog was shown for the shortcut app, then
        // something is wrong.
        ASSERT_FALSE(widget->GetName() ==
                     "WebAppIdentityUpdateConfirmationView");
      }));

  // Now navigate to the same url and allow the update mechanism to run.
  UpdateCheckResultAwaiter result_awaiter(app_url);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), app_url));
  EXPECT_EQ(std::move(result_awaiter).AwaitNextResult(),
            ManifestUpdateResult::kAppUpToDate);

  EXPECT_EQ(kOverrideName,
            GetProvider().registrar_unsafe().GetAppShortName(app_id));
  EXPECT_EQ(SK_ColorRED, ReadAppIconPixel(app_id, /*size=*/256));
}

// Test that showing the AppIdentity update confirmation and allowing the update
// sends the right signal back.
IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerAppIdentityBrowserTest,
                       VerifyCallbackUpgradeAllowed) {
  base::AutoReset<std::optional<AppIdentityUpdate>> update_dialog_scope =
      SetIdentityUpdateDialogActionForTesting(AppIdentityUpdate::kAllowed);

  constexpr char kManifestTemplate[] = R"(
    {
      "name": "$1",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "icons": $2
    }
  )";
  OverrideManifest(kManifestTemplate, {"Test app name", kInstallableIconList});
  webapps::AppId app_id = InstallWebApp();

  base::RunLoop run_loop;

  ShowWebAppIdentityUpdateDialog(
      app_id,
      /* title_change= */ true,
      /* icon_change= */ false, u"old_title", u"new_title",
      /* old_icon= */ SkBitmap(),
      /* new_icon= */ SkBitmap(),
      browser()->tab_strip_model()->GetActiveWebContents(),
      /* callback= */
      base::BindLambdaForTesting(
          [&](AppIdentityUpdate app_identity_update_allowed) {
            // This verifies that the dialog sends us the signal to update.
            DCHECK_EQ(AppIdentityUpdate::kAllowed, app_identity_update_allowed);
            run_loop.Quit();
          }));

  run_loop.Run();
}

class ManifestUpdateManagerImmediateUpdateBrowserTest
    : public ManifestUpdateManagerBrowserTest {
 public:
  ManifestUpdateManagerImmediateUpdateBrowserTest() = default;

  SkColor GetMiddlePixel(gfx::Image image) {
    SkBitmap bitmap = image.AsBitmap();
    return bitmap.getColor(bitmap.width() / 2, bitmap.height() / 2);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Test whether web app windows update their UI immediately after a manifest
// update gets applied.
IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerImmediateUpdateBrowserTest,
                       WebAppWindowsUpdatedImmediately) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "$1",
      "start_url": "manifest_test_page.html",
      "scope": "/",
      "display": "$2",
      "icons": [{
        "src": "$3",
        "sizes": "256x256",
        "type": "image/png"
      }],
      "theme_color": "$4"
    }
  )";

  // Install default web app (so user confirmations aren't required for updating
  // its identity).
  OverrideManifest(kManifestTemplate,
                   {"Old name", "standalone", "256x256-red.png", "red"});
  webapps::AppId app_id = InstallDefaultApp();
  GURL app_url = GetAppURL();
  Browser* app_browser = nullptr;

  // Launch app window and wait for the page to load, the app icon to load and
  // the manifest update check to complete.
  {
    base::RunLoop icon_load;
    WebAppBrowserController::SetIconLoadCallbackForTesting(
        icon_load.QuitClosure());
    UpdateCheckResultAwaiter result_awaiter(app_url);

    // Synchronize os integration to ensure that mac app shim is created.
    GetProvider().scheduler().SynchronizeOsIntegration(app_id,
                                                       base::DoNothing());
    app_browser = LaunchWebAppBrowserAndWait(app_id);

    icon_load.Run();
    EXPECT_EQ(std::move(result_awaiter).AwaitNextResult(),
              ManifestUpdateResult::kAppUpToDate);
  }

  // Update manifest UI elements.
  OverrideManifest(kManifestTemplate,
                   {"New name", "minimal-ui", "256x256-green.png", "lime"});

  // Set up awaiters.
  base::RunLoop app_window_update;
  WebAppBrowserController::SetManifestUpdateAppliedCallbackForTesting(
      app_window_update.QuitClosure());
  base::RunLoop second_icon_load;
  WebAppBrowserController::SetIconLoadCallbackForTesting(
      second_icon_load.QuitClosure());
  UpdateCheckResultAwaiter result_awaiter(app_url);

  // Reload page to invoke a manifest update check.
  GetProvider().manifest_update_manager().ResetManifestThrottleForTesting(
      app_id);
  chrome::Reload(app_browser, WindowOpenDisposition::CURRENT_TAB);

  // Check update takes effect on web app database.
  EXPECT_EQ(std::move(result_awaiter).AwaitNextResult(),
            ManifestUpdateResult::kAppUpdated);
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);
  const WebApp* web_app = GetProvider().registrar_unsafe().GetAppById(app_id);
  ASSERT_TRUE(web_app);
  EXPECT_EQ(web_app->untranslated_name(), "New name");
  EXPECT_EQ(web_app->display_mode(), DisplayMode::kMinimalUi);
  EXPECT_EQ(web_app->theme_color(), SK_ColorGREEN);

  // Check update takes effect on live web app window.
  app_window_update.Run();
  AppBrowserController* app_controller = app_browser->app_controller();
  EXPECT_EQ(app_controller->GetTitle(), u"New name - Web app banner test page");
  EXPECT_EQ(app_controller->GetThemeColor(), SK_ColorGREEN);

  // Force the app icon to load again and check that it's the new one.
  app_controller->GetWindowIcon();
  second_icon_load.Run();
  // Read the middle of the icon since on Chrome OS it gets circlified.
  EXPECT_EQ(GetMiddlePixel(app_controller->GetWindowIcon().GetImage()),
            SK_ColorGREEN);
}

class ManifestUpdateManagerPrerenderingBrowserTest
    : public ManifestUpdateManagerBrowserTest {
 public:
  ManifestUpdateManagerPrerenderingBrowserTest()
      : prerender_helper_(base::BindRepeating(
            &ManifestUpdateManagerPrerenderingBrowserTest::GetWebContents,
            base::Unretained(this))) {}

  ~ManifestUpdateManagerPrerenderingBrowserTest() override = default;

  content::WebContents* GetWebContents() const {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  content::test::PrerenderTestHelper& prerender_helper() {
    return prerender_helper_;
  }

 private:
  content::test::PrerenderTestHelper prerender_helper_;
};

// Tests that prerendering doesn't change the existing App ID. It also doesn't
// call ManifestUpdateManager as a primary page is not changed.
IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerPrerenderingBrowserTest,
                       NotUpdateInPrerendering) {
  webapps::AppId app_id = InstallWebApp();
  EXPECT_EQ(ManifestUpdateResult::kAppUpToDate,
            GetResultAfterPageLoad(GetAppURL()));

  content::WebContents* web_contents = GetWebContents();
  const webapps::AppId* first_app_id = WebAppTabHelper::GetAppId(web_contents);
  EXPECT_EQ(app_id, *first_app_id);

  base::HistogramTester histogram_tester;
  const GURL prerender_url = http_server_.GetURL("/title1.html");
  content::FrameTreeNodeId host_id =
      prerender_helper().AddPrerender(prerender_url);
  content::test::PrerenderHostObserver host_observer(*web_contents, host_id);
  // Prerendering doesn't update the existing App ID.
  const webapps::AppId* app_id_on_prerendering =
      WebAppTabHelper::GetAppId(web_contents);
  EXPECT_EQ(app_id, *app_id_on_prerendering);

  // In prerendering navigation, it doesn't call ManifestUpdateManager.
  histogram_tester.ExpectTotalCount(kUpdateHistogramName, 0);

  prerender_helper().NavigatePrimaryPage(prerender_url);
  EXPECT_TRUE(host_observer.was_activated());
  const webapps::AppId* app_id_after_activation =
      WebAppTabHelper::GetAppId(web_contents);
  EXPECT_EQ(nullptr, app_id_after_activation);
}

class ManifestUpdateManagerBrowserTest_TabStrip
    : public ManifestUpdateManagerBrowserTest {
 public:
  ManifestUpdateManagerBrowserTest_TabStrip() {
    feature_list_.InitWithFeatures(
        {blink::features::kDesktopPWAsTabStripCustomizations,
         blink::features::kDesktopPWAsTabStrip},
        /*disabled_features=*/{});
  }
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest_TabStrip,
                       CheckFindsAddedTabStripField) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "display_override": ["tabbed"],
      "icons": $1
    }
  )";

  constexpr char kTabStripManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "display_override": ["tabbed"],
      "tab_strip": {
        "new_tab_button": {
          "url": "/new-tab-url"
        },
        "home_tab": {
          "scope_patterns": [
            {"pathname": "/foo/*"}
          ]
        }
      },
      "icons": $1
    }
  )";

  OverrideManifest(kManifestTemplate, {kInstallableIconList});
  webapps::AppId app_id = InstallWebApp();
  const WebApp* web_app = GetProvider().registrar_unsafe().GetAppById(app_id);
  EXPECT_FALSE(web_app->tab_strip().has_value());

  OverrideManifest(kTabStripManifestTemplate, {kInstallableIconList});
  EXPECT_EQ(ManifestUpdateResult::kAppUpdated,
            GetResultAfterPageLoad(GetAppURL()));
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);

  EXPECT_TRUE(web_app->tab_strip().has_value());
  EXPECT_EQ(http_server_.GetURL("/new-tab-url"),
            web_app->tab_strip().value().new_tab_button.url);
  EXPECT_EQ(absl::get<blink::Manifest::HomeTabParams>(
                web_app->tab_strip().value().home_tab)
                .scope_patterns.size(),
            1u);
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest_TabStrip,
                       CheckIgnoresUnchangedTabStripField) {
  constexpr char kTabStripManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "display_override": ["tabbed"],
      "tab_strip": {
        "new_tab_button": {
          "url": "/new-tab-url"
        },
        "home_tab": {}
      },
      "icons": $1
    }
  )";

  OverrideManifest(kTabStripManifestTemplate, {kInstallableIconList});
  webapps::AppId app_id = InstallWebApp();
  const WebApp* web_app = GetProvider().registrar_unsafe().GetAppById(app_id);
  EXPECT_TRUE(web_app->tab_strip().has_value());
  EXPECT_EQ(http_server_.GetURL("/new-tab-url"),
            web_app->tab_strip().value().new_tab_button.url);
  EXPECT_TRUE(absl::holds_alternative<blink::Manifest::HomeTabParams>(
      web_app->tab_strip().value().home_tab));

  OverrideManifest(kTabStripManifestTemplate, {kInstallableIconList});
  EXPECT_EQ(ManifestUpdateResult::kAppUpToDate,
            GetResultAfterPageLoad(GetAppURL()));
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpToDate, 1);
  EXPECT_TRUE(web_app->tab_strip().has_value());
  EXPECT_EQ(http_server_.GetURL("/new-tab-url"),
            web_app->tab_strip().value().new_tab_button.url);
  EXPECT_TRUE(absl::holds_alternative<blink::Manifest::HomeTabParams>(
      web_app->tab_strip().value().home_tab));
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest_TabStrip,
                       CheckFindsChangedTabStripField) {
  constexpr char kTabStripManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "display_override": ["tabbed"],
      "tab_strip": {
        "new_tab_button": {
          "url": "$1"
        }
      },
      "icons": $2
    }
  )";

  OverrideManifest(kTabStripManifestTemplate,
                   {"old-relative-url", kInstallableIconList});
  webapps::AppId app_id = InstallWebApp();
  const WebApp* web_app = GetProvider().registrar_unsafe().GetAppById(app_id);
  // URL parsed relative to manifest URL, which is in /banners/.
  EXPECT_TRUE(web_app->tab_strip().has_value());
  EXPECT_EQ(http_server_.GetURL("/banners/old-relative-url"),
            web_app->tab_strip().value().new_tab_button.url);

  OverrideManifest(kTabStripManifestTemplate,
                   {"/new-tab-url", kInstallableIconList});
  EXPECT_EQ(ManifestUpdateResult::kAppUpdated,
            GetResultAfterPageLoad(GetAppURL()));
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);
  EXPECT_TRUE(web_app->tab_strip().has_value());
  EXPECT_EQ(http_server_.GetURL("/new-tab-url"),
            web_app->tab_strip().value().new_tab_button.url);
}

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest_TabStrip,
                       CheckFindsDeletedTabStripField) {
  constexpr char kTabStripManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "display_override": ["tabbed"],
      "tab_strip": {
        "new_tab_button": {
          "url": "/new-tab-url"
        },
        "home_tab": {}
      },
      "icons": $1
    }
  )";

  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "display_override": ["tabbed"],
      "icons": $1
    }
  )";

  OverrideManifest(kTabStripManifestTemplate, {kInstallableIconList});
  webapps::AppId app_id = InstallWebApp();
  const WebApp* web_app = GetProvider().registrar_unsafe().GetAppById(app_id);
  EXPECT_TRUE(web_app->tab_strip().has_value());
  EXPECT_EQ(http_server_.GetURL("/new-tab-url"),
            web_app->tab_strip().value().new_tab_button.url);
  EXPECT_TRUE(absl::holds_alternative<blink::Manifest::HomeTabParams>(
      web_app->tab_strip().value().home_tab));

  OverrideManifest(kManifestTemplate, {kInstallableIconList});
  EXPECT_EQ(ManifestUpdateResult::kAppUpdated,
            GetResultAfterPageLoad(GetAppURL()));
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpdated, 1);
  EXPECT_FALSE(web_app->tab_strip().has_value());
}

class ManifestUpdateManagerBrowserTest_NoTabStrip
    : public ManifestUpdateManagerBrowserTest {
 public:
  ManifestUpdateManagerBrowserTest_NoTabStrip() {
    feature_list_.InitAndDisableFeature(
        blink::features::kDesktopPWAsTabStripCustomizations);
  }
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(ManifestUpdateManagerBrowserTest_NoTabStrip,
                       WithoutTabStripFlag_CheckIgnoresTabStripField) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "display_override": ["tabbed"],
      "icons": $1
    }
  )";

  constexpr char kTabStripManifestTemplate[] = R"(
    {
      "name": "Test app name",
      "start_url": ".",
      "scope": "/",
      "display": "standalone",
      "display_override": ["tabbed"],
      "tab_strip": {
        "new_tab_button": {
          "url": "/new-tab-url"
        }
      },
      "icons": $1
    }
  )";

  OverrideManifest(kManifestTemplate, {kInstallableIconList});
  webapps::AppId app_id = InstallWebApp();
  const WebApp* web_app = GetProvider().registrar_unsafe().GetAppById(app_id);
  EXPECT_FALSE(web_app->tab_strip().has_value());

  OverrideManifest(kTabStripManifestTemplate, {kInstallableIconList});
  EXPECT_EQ(ManifestUpdateResult::kAppUpToDate,
            GetResultAfterPageLoad(GetAppURL()));
  histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                      ManifestUpdateResult::kAppUpToDate, 1);
  EXPECT_FALSE(web_app->tab_strip().has_value());
}

enum AppIdTestParam {
  kInvalid = 0,
  kTypeWebApp = 1 << 1,
  kTypeDefaultApp = 1 << 2,
  kTypePolicyApp = 1 << 3,
  kTypeKioskApp = 1 << 4,
  kWithFlagNone = 1 << 5,
  kWithFlagPolicyAppIdentity = 1 << 6,
  kWithFlagAppIdDialogForIcon = 1 << 7,
  kActionUpdateTitle = 1 << 8,
  kActionUpdateTitleAndLauncherIcon = 1 << 9,
  kActionUpdateLauncherIcon = 1 << 10,
  kActionUpdateInstallIcon = 1 << 11,
  kActionUpdateLauncherAndInstallIcon = 1 << 12,
  kActionUpdateUnimportantIcon = 1 << 13,
  kActionRemoveLauncherIcon = 1 << 14,
  kActionRemoveInstallIcon = 1 << 15,
  kActionRemoveUnimportantIcon = 1 << 16,
  kActionSwitchFromLauncher = 1 << 17,
  kActionSwitchToLauncher = 1 << 18,
};

class ManifestUpdateManagerBrowserTest_AppIdentityParameterized
    : public ManifestUpdateManagerBrowserTest,
      public testing::WithParamInterface<
          std::tuple<AppIdTestParam, AppIdTestParam, AppIdTestParam>> {
 public:
  ManifestUpdateManagerBrowserTest_AppIdentityParameterized() {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;
    if (IsAppIdentityUpdateDialogForIconEnabled()) {
      enabled_features.push_back(features::kPwaUpdateDialogForIcon);
    } else {
      disabled_features.push_back(features::kPwaUpdateDialogForIcon);
    }
    if (IsPolicyAppIdentityOverrideEnabled()) {
      enabled_features.push_back(
          features::kWebAppManifestPolicyAppIdentityUpdate);
    } else {
      disabled_features.push_back(
          features::kWebAppManifestPolicyAppIdentityUpdate);
    }

    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

  bool IsWebApp() const {
    return std::get<1>(GetParam()) & AppIdTestParam::kTypeWebApp;
  }
  bool IsDefaultApp() const {
    return std::get<1>(GetParam()) & AppIdTestParam::kTypeDefaultApp;
  }
  bool IsPolicyApp() const {
    return std::get<1>(GetParam()) & AppIdTestParam::kTypePolicyApp;
  }
  bool IsKioskApp() const {
    return std::get<1>(GetParam()) & AppIdTestParam::kTypeKioskApp;
  }

  bool IsAppIdentityUpdateDialogForIconEnabled() const {
    return std::get<2>(GetParam()) &
           AppIdTestParam::kWithFlagAppIdDialogForIcon;
  }
  bool IsPolicyAppIdentityOverrideEnabled() const {
    return std::get<2>(GetParam()) & AppIdTestParam::kWithFlagPolicyAppIdentity;
  }

  bool TitleUpdate() const {
    return std::get<0>(GetParam()) & AppIdTestParam::kActionUpdateTitle ||
           std::get<0>(GetParam()) &
               AppIdTestParam::kActionUpdateTitleAndLauncherIcon;
  }

  bool AnyIconUpdate() const {
    return IdentityIconUpdate() || NonIdentityIconUpdate();
  }

  bool IdentityIconUpdate() const {
    return LauncherIconUpdate() || LauncherIconRemove() ||
           InstallIconUpdate() || InstallIconRemove() ||
           IconSwitchFromLauncher() || IconSwitchToLauncher();
  }

  bool NonIdentityIconUpdate() const {
    return UnimportantIconUpdate() || UnimportantIconRemove();
  }

  bool LauncherIconUpdate() const {
    return std::get<0>(GetParam()) &
               AppIdTestParam::kActionUpdateLauncherIcon ||
           std::get<0>(GetParam()) &
               AppIdTestParam::kActionUpdateTitleAndLauncherIcon ||
           std::get<0>(GetParam()) &
               AppIdTestParam::kActionUpdateLauncherAndInstallIcon;
  }

  bool InstallIconUpdate() const {
    return std::get<0>(GetParam()) & AppIdTestParam::kActionUpdateInstallIcon ||
           std::get<0>(GetParam()) &
               AppIdTestParam::kActionUpdateLauncherAndInstallIcon;
  }

  bool UnimportantIconUpdate() const {
    return std::get<0>(GetParam()) &
           AppIdTestParam::kActionUpdateUnimportantIcon;
  }

  bool LauncherIconRemove() const {
    return std::get<0>(GetParam()) & AppIdTestParam::kActionRemoveLauncherIcon;
  }

  bool InstallIconRemove() const {
    return std::get<0>(GetParam()) & AppIdTestParam::kActionRemoveInstallIcon;
  }

  bool UnimportantIconRemove() const {
    return std::get<0>(GetParam()) &
           AppIdTestParam::kActionRemoveUnimportantIcon;
  }

  bool IconSwitchFromLauncher() const {
    return std::get<0>(GetParam()) & AppIdTestParam::kActionSwitchFromLauncher;
  }

  bool IconSwitchToLauncher() const {
    return std::get<0>(GetParam()) & AppIdTestParam::kActionSwitchToLauncher;
  }

  // This function describes in which scenarios the test should expect the title
  // of an app to change. It should mirror exactly the expectations we have of
  // the implementation and be simple to read for easy verification.
  bool ExpectTitleUpdate() const {
    if (!TitleUpdate())
      return false;  // Titles should not update without a request to update.

    if (IsDefaultApp() || IsKioskApp())
      return true;
    if (IsPolicyApp())
      return IsPolicyAppIdentityOverrideEnabled();

    return true;  // App Identity Updates for names have launched.
  }

  // This function describes in which scenarios the test should expect the icons
  // of an app to change. It should mirror exactly the expectations we have of
  // the implementation and be simple to read for easy verification.
  bool ExpectIconUpdate() const {
    return AnyIconUpdate() && IconUpdatesAllowed();
  }

  bool IconUpdatesAllowed() const {
    if (!IdentityIconUpdate() || IsDefaultApp() || IsKioskApp()) {
      return true;
    }

    if (IsPolicyApp()) {
      return IsPolicyAppIdentityOverrideEnabled();
    }

    // User-installed apps don't get title updates unless App Id dialog is
    // enabled for icons.
    return IsAppIdentityUpdateDialogForIconEnabled();
  }

  static std::string ParamToString(
      testing::TestParamInfo<
          std::tuple<AppIdTestParam, AppIdTestParam, AppIdTestParam>>
          param_info) {
    std::string result = "";

    AppIdTestParam action = std::get<0>(param_info.param);
    if (action & AppIdTestParam::kActionUpdateTitle)
      result += "UpdateTitle_";
    if (action & AppIdTestParam::kActionUpdateTitleAndLauncherIcon)
      result += "UpdateTitleAndLauncherIcon_";
    if (action & AppIdTestParam::kActionUpdateLauncherIcon)
      result += "UpdateLauncherIcon_";
    if (action & AppIdTestParam::kActionUpdateInstallIcon)
      result += "UpdateInstallIcon_";
    if (action & AppIdTestParam::kActionUpdateLauncherAndInstallIcon)
      result += "UpdateLauncherAndInstallIcon_";
    if (action & AppIdTestParam::kActionUpdateUnimportantIcon)
      result += "UpdateUnimportantIcon_";
    if (action & AppIdTestParam::kActionRemoveLauncherIcon)
      result += "RemoveLauncherIcon_";
    if (action & AppIdTestParam::kActionRemoveInstallIcon)
      result += "RemoveInstallIcon_";
    if (action & AppIdTestParam::kActionRemoveUnimportantIcon)
      result += "RemoveUnimportantIcon_";
    if (action & AppIdTestParam::kActionSwitchFromLauncher)
      result += "SwitchFromLauncher_";
    if (action & AppIdTestParam::kActionSwitchToLauncher)
      result += "SwitchToLauncher_";

    AppIdTestParam type = std::get<1>(param_info.param);
    if (type & AppIdTestParam::kTypeWebApp)
      result += "WebApp_";
    if (type & AppIdTestParam::kTypeDefaultApp)
      result += "DefaultApp_";
    if (type & AppIdTestParam::kTypePolicyApp)
      result += "PolicyApp_";
    if (type & AppIdTestParam::kTypeKioskApp)
      result += "KioskApp_";

    AppIdTestParam flags = std::get<2>(param_info.param);
    result += "Flags_";
    if (flags & AppIdTestParam::kWithFlagNone)
      result += "None_";
    if (flags & AppIdTestParam::kWithFlagPolicyAppIdentity)
      result += "PolicyCanUpdate_";
    if (flags & AppIdTestParam::kWithFlagAppIdDialogForIcon)
      result += "WithAppIdDlgForIcon_";

    return result;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

std::string GenerateIconRow(SquareSizePx size, SkColor color) {
  std::string size_str = base::NumberToString(size);
  std::string color_str = base::ReplaceStringPlaceholders(
      R"(rgb($1, $2, $3))",
      {base::NumberToString(SkColorGetR(color)),
       base::NumberToString(SkColorGetG(color)),
       base::NumberToString(SkColorGetB(color))},
      nullptr);
  // Encode a square SVG with a flat colored rect of the requested size and
  // color in the icon URL.
  return base::ReplaceStringPlaceholders(R"(      {
        "src": "data:image/svg+xml,<svg xmlns='http://www.w3.org/2000/svg' width='$1' height='$1'><rect fill='$2' width='$1' height='$1' /></svg>",
        "sizes": "$1x$1",
        "type": "image/svg+xml"
      })",
                                         {size_str, color_str}, nullptr);
}

struct SizeColor {
  SquareSizePx size;
  SkColor color;
};

std::string GenerateColoredIconList(std::vector<SizeColor> size_colors) {
  bool installable_icon_included = false;

  std::string icon_list;
  for (const auto [size, color] : size_colors) {
    if (!icon_list.empty()) {
      icon_list += ",\n";
    }
    icon_list += GenerateIconRow(size, color);

    // Installability requirements mandate at least one large icon.
    installable_icon_included |= size >= kInstallMinSize;
  }

  if (!installable_icon_included) {
    if (!icon_list.empty()) {
      icon_list += ",\n";
    }
    icon_list += "      { \"error\": \"Installability requirements not met\" }";
  }

  if (!icon_list.empty()) {
    icon_list += "\n";
  }
  return base::StrCat({"\n    [\n", icon_list, "    ]\n  "});
}

// Disabled due to test flakiness: https://crbug.com/1341617
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#define MAYBE_CheckCombinations DISABLED_CheckCombinations
#else
#define MAYBE_CheckCombinations CheckCombinations
#endif
IN_PROC_BROWSER_TEST_P(
    ManifestUpdateManagerBrowserTest_AppIdentityParameterized,
    MAYBE_CheckCombinations) {
  constexpr char kManifestTemplate[] = R"(
    {
      "name": "$1",
      "start_url": "manifest_test_page.html",
      "scope": "/",
      "display": "standalone",
      "icons": $2
    }
  )";

  ManifestUpdateManager::ScopedBypassWindowCloseWaitingForTesting
      bypass_window_close_waiting;

  testing::TestParamInfo<
      std::tuple<AppIdTestParam, AppIdTestParam, AppIdTestParam>>
      param(GetParam(), 0);

  std::string trace = "\n---------------------------\nParameterized test: " +
                      ParamToString(param) + "\nType: ";
  if (IsPolicyApp())
    trace += "Policy";
  if (IsKioskApp())
    trace += "Kiosk";
  if (IsDefaultApp())
    trace += "Default";
  if (IsWebApp())
    trace += "WebApp";
  trace += (IsAppIdentityUpdateDialogForIconEnabled()
                ? ", with AppIdDlgForIcon: YES\n"
                : ", with AppIdDlgForIcon: NO\n");

  trace += base::ReplaceStringPlaceholders(
      "UPDATE: Title: $1 Launcher $2 Install $3 Other $4\n",
      {base::NumberToString(TitleUpdate()),
       base::NumberToString(LauncherIconUpdate()),
       base::NumberToString(InstallIconUpdate()),
       base::NumberToString(UnimportantIconUpdate())},
      nullptr);
  trace += base::ReplaceStringPlaceholders(
      "REMOVE: Launcher $1 Install $2 Other $3\n",
      {base::NumberToString(LauncherIconRemove()),
       base::NumberToString(InstallIconRemove()),
       base::NumberToString(UnimportantIconRemove())},
      nullptr);
  trace += base::ReplaceStringPlaceholders(
      "SWITCH: FromLauncher $1 ToLauncher $2\n",
      {base::NumberToString(IconSwitchFromLauncher()),
       base::NumberToString(IconSwitchToLauncher())},
      nullptr);
  trace += base::ReplaceStringPlaceholders(
      "Should result in: Title update: $1 Icon update $2\n",
      {base::NumberToString(ExpectTitleUpdate()),
       base::NumberToString(ExpectIconUpdate())},
      nullptr);
  trace += base::ReplaceStringPlaceholders(
      "Sizes: InstallIcon $1, LauncherIcon $2, ExtraIcon1 $3, ExtraIcon2 $4 "
      "Installability $5\n",
      {base::NumberToString(kInstallIconSize),
       base::NumberToString(kLauncherIconSize),
       base::NumberToString(kUnimportantIconSize),
       base::NumberToString(kUnimportantIconSize2),
       base::NumberToString(kInstallabilityIconSize)},
      nullptr);
  trace += "---------------------------\n";

  // We need to auto-accept the App Identity Update dialog whenever the test
  // enables icon updates, but also when title updates are requested (because
  // they are default-enabled). When icon updates become default-enabled also,
  // we can change this to auto-accept when either icon updates are requested,
  // or name updates, or both.
  if (IsAppIdentityUpdateDialogForIconEnabled()) {
    AcceptAppIdentityUpdateDialogForTesting();
  } else if (TitleUpdate()) {
    AcceptAppIdentityUpdateDialogForTesting();
  }

  std::string app_name = "Test app name";

  // The 'before' and 'after' icon lists.
  std::string starting_stage;
  std::string ending_stage;

  // This is the default icon list (all green icons) and is overridden below,
  // if need be.
  starting_stage =
      GenerateColoredIconList({{kUnimportantIconSize, SK_ColorRED},
                               {kInstallIconSize, SK_ColorYELLOW},
                               {kLauncherIconSize, SK_ColorGREEN},
                               {kInstallabilityIconSize, SK_ColorBLUE}});

  // This is the resulting shortcut colors (per size) for the default icon list
  // above, and similar to `starting_stage` it is overridden below when needed.
  // NOTE: When considering which shortcut sizes appear on which platform, the
  // system creates an intersection between `kDesiredIconSizesForShortcut`
  // (which is platform-dependent) and `SizesToGenerate()` (which is hard-coded
  // to { 32, 48, 64, 96, 128, 256 } for all platforms. This can lead to some
  // discrepancies per platform. For example, Windows specifies more sizes
  // in`kDesiredIconSizesForShortcut` than other OS', which is why it is common
  // to find auto-generated icons for size 64 and 96 only on Windows. Similarly,
  // size 512 is not part of `kDesiredIconSizesForShortcut` on Windows, and
  // that size therefore does not always feature in the shortcut expectations.
  std::vector<std::pair<std::pair<int, int>, SkColor>>
      expected_shortcut_colors_before = {
          {{32, kAll}, SK_ColorYELLOW},
          {{48, kAll}, SK_ColorYELLOW},
          // Although sizes 64 and 96 are within the SizesToGenerate() list they
          // are listed in `kDesiredIconSizesForShortcut` on Windows only.
          {{64, kWin}, SK_ColorGREEN},
          {{96, kWin}, SK_ColorGREEN},
          {{128, kAll}, SK_ColorGREEN},
          {{256, kMac}, SK_ColorGREEN},
          {{256, kNotMac}, SK_ColorBLUE},
          // The tests use size 512 as the icon size that guarantees that the
          // installability requirements are met, but that size is not listed as
          // a desired shortcut size on Windows.
          {{512, kNotWin}, SK_ColorBLUE}};

  // This needs to be populated for each test below.
  std::vector<std::pair<std::pair<int, int>, SkColor>>
      expected_shortcut_colors_if_updated;

  if (LauncherIconUpdate() && InstallIconUpdate()) {
    ending_stage =
        GenerateColoredIconList({{kUnimportantIconSize, SK_ColorRED},
                                 {kInstallIconSize, SK_ColorBLACK},
                                 {kLauncherIconSize, SK_ColorWHITE},
                                 {kInstallabilityIconSize, SK_ColorBLUE}});

    expected_shortcut_colors_if_updated = {
        {{32, kAll}, SK_ColorBLACK},
        {{48, kAll}, SK_ColorBLACK},
        {{64, kWin}, SK_ColorWHITE},
        {{96, kWin}, SK_ColorWHITE},
        {{128, kAll}, SK_ColorWHITE},
        // On Mac, this size is the launcher icon, so white is expected.
        {{256, kMac}, SK_ColorWHITE},
        // On other platforms, there is no size 256 specified, so this is
        // generated from the installability icon (size 512), which is blue.
        {{256, kNotMac}, SK_ColorBLUE},
        {{512, kNotWin}, SK_ColorBLUE}};
  } else if (IconSwitchFromLauncher()) {
    // Starting stage is with a launcher icon but without an unimportant icon.
    starting_stage =
        GenerateColoredIconList({{kInstallIconSize, SK_ColorYELLOW},
                                 {kLauncherIconSize, SK_ColorGREEN},
                                 {kInstallabilityIconSize, SK_ColorBLUE}});

    expected_shortcut_colors_before = {
        {{32, kAll}, SK_ColorYELLOW},   {{48, kAll}, SK_ColorYELLOW},
        {{64, kWin}, SK_ColorGREEN},    {{96, kWin}, SK_ColorGREEN},
        {{128, kAll}, SK_ColorGREEN},   {{256, kMac}, SK_ColorGREEN},
        {{256, kNotMac}, SK_ColorBLUE}, {{512, kNotWin}, SK_ColorBLUE}};

    // Ending stage is without a launcher icon but with an unimportant icon.
    ending_stage =
        GenerateColoredIconList({{kUnimportantIconSize, SK_ColorRED},
                                 {kLauncherIconSize, SK_ColorGREEN},
                                 {kInstallabilityIconSize, SK_ColorBLUE}});

    expected_shortcut_colors_if_updated = {
        {{32, kAll}, SK_ColorGREEN},    {{48, kAll}, SK_ColorGREEN},
        {{64, kWin}, SK_ColorGREEN},    {{96, kWin}, SK_ColorGREEN},
        {{128, kAll}, SK_ColorGREEN},   {{256, kMac}, SK_ColorGREEN},
        {{256, kNotMac}, SK_ColorBLUE}, {{512, kNotWin}, SK_ColorBLUE}};
  } else if (IconSwitchToLauncher()) {
    // Starting stage is without a launcher icon but with an unimportant icon.
    starting_stage = GenerateColoredIconList({
        {kUnimportantIconSize, SK_ColorRED},
        {kInstallIconSize, SK_ColorYELLOW},
        {kInstallabilityIconSize, SK_ColorBLUE},
    });

    expected_shortcut_colors_before = {
        {{32, kAll}, SK_ColorYELLOW},   {{48, kAll}, SK_ColorYELLOW},
        {{64, kWin}, SK_ColorYELLOW},   {{96, kWin}, SK_ColorYELLOW},
        {{128, kAll}, SK_ColorBLUE},    {{256, kMac}, SK_ColorBLUE},
        {{256, kNotMac}, SK_ColorBLUE}, {{512, kNotWin}, SK_ColorBLUE}};

    // Ending stage is with the launcher icon but without an unimportant icon.
    ending_stage =
        GenerateColoredIconList({{kInstallIconSize, SK_ColorYELLOW},
                                 {kLauncherIconSize, SK_ColorGREEN},
                                 {kInstallabilityIconSize, SK_ColorBLUE}});

    expected_shortcut_colors_if_updated = {
        {{32, kAll}, SK_ColorYELLOW},   {{48, kAll}, SK_ColorYELLOW},
        {{64, kWin}, SK_ColorGREEN},    {{96, kWin}, SK_ColorGREEN},
        {{128, kAll}, SK_ColorGREEN},   {{256, kMac}, SK_ColorGREEN},
        {{256, kNotMac}, SK_ColorBLUE}, {{512, kNotWin}, SK_ColorBLUE}};
  } else if (LauncherIconUpdate()) {
    ending_stage = GenerateColoredIconList({
        {kUnimportantIconSize, SK_ColorRED},
        {kInstallIconSize, SK_ColorYELLOW},
        {kLauncherIconSize, SK_ColorWHITE},
        {kInstallabilityIconSize, SK_ColorBLUE},
    });
    expected_shortcut_colors_if_updated = {
        {{32, kAll}, SK_ColorYELLOW},   {{48, kAll}, SK_ColorYELLOW},
        {{64, kWin}, SK_ColorWHITE},    {{96, kWin}, SK_ColorWHITE},
        {{128, kAll}, SK_ColorWHITE},   {{256, kMac}, SK_ColorWHITE},
        {{256, kNotMac}, SK_ColorBLUE}, {{512, kNotWin}, SK_ColorBLUE}};
  } else if (InstallIconUpdate()) {
    ending_stage =
        GenerateColoredIconList({{kUnimportantIconSize, SK_ColorRED},
                                 {kInstallIconSize, SK_ColorWHITE},
                                 {kLauncherIconSize, SK_ColorGREEN},
                                 {kInstallabilityIconSize, SK_ColorBLUE}});
    expected_shortcut_colors_if_updated = {
        {{32, kAll}, SK_ColorWHITE},    {{48, kAll}, SK_ColorWHITE},
        {{64, kWin}, SK_ColorGREEN},    {{96, kWin}, SK_ColorGREEN},
        {{128, kAll}, SK_ColorGREEN},   {{256, kMac}, SK_ColorGREEN},
        {{256, kNotMac}, SK_ColorBLUE}, {{512, kNotWin}, SK_ColorBLUE}};
  } else if (UnimportantIconUpdate()) {
    ending_stage =
        GenerateColoredIconList({{kUnimportantIconSize, SK_ColorWHITE},
                                 {kInstallIconSize, SK_ColorYELLOW},
                                 {kLauncherIconSize, SK_ColorGREEN},
                                 {kInstallabilityIconSize, SK_ColorBLUE}});

    // There should be no effect on the shortcut icons when an unimportant icon
    // updates.
    expected_shortcut_colors_if_updated = expected_shortcut_colors_before;
  } else if (LauncherIconRemove()) {
    ending_stage =
        GenerateColoredIconList({{kUnimportantIconSize, SK_ColorRED},
                                 {kInstallIconSize, SK_ColorYELLOW},
                                 {kInstallabilityIconSize, SK_ColorBLUE}});

    expected_shortcut_colors_if_updated = {
        {{32, kAll}, SK_ColorYELLOW},   {{48, kAll}, SK_ColorYELLOW},
        {{64, kWin}, SK_ColorBLUE},     {{96, kWin}, SK_ColorBLUE},
        {{128, kAll}, SK_ColorBLUE},    {{256, kMac}, SK_ColorBLUE},
        {{256, kNotMac}, SK_ColorBLUE}, {{512, kNotWin}, SK_ColorBLUE}};
  } else if (InstallIconRemove()) {
    ending_stage =
        GenerateColoredIconList({{kUnimportantIconSize, SK_ColorRED},
                                 {kLauncherIconSize, SK_ColorGREEN},
                                 {kInstallabilityIconSize, SK_ColorBLUE}});
    expected_shortcut_colors_if_updated = {
        {{32, kAll}, SK_ColorGREEN},    {{48, kAll}, SK_ColorGREEN},
        {{64, kWin}, SK_ColorGREEN},    {{96, kWin}, SK_ColorGREEN},
        {{128, kAll}, SK_ColorGREEN},   {{256, kMac}, SK_ColorGREEN},
        {{256, kNotMac}, SK_ColorBLUE}, {{512, kNotWin}, SK_ColorBLUE}};
  } else if (UnimportantIconRemove()) {
    starting_stage =
        GenerateColoredIconList({{kUnimportantIconSize, SK_ColorBLACK},
                                 {kUnimportantIconSize2, SK_ColorRED},
                                 {kInstallIconSize, SK_ColorYELLOW},
                                 {kLauncherIconSize, SK_ColorGREEN},
                                 {kInstallabilityIconSize, SK_ColorBLUE}});
    expected_shortcut_colors_before = {
        {{32, kAll}, SK_ColorYELLOW},   {{48, kAll}, SK_ColorYELLOW},
        {{64, kWin}, SK_ColorGREEN},    {{96, kWin}, SK_ColorGREEN},
        {{128, kAll}, SK_ColorGREEN},   {{256, kMac}, SK_ColorGREEN},
        {{256, kNotMac}, SK_ColorBLUE}, {{512, kNotWin}, SK_ColorBLUE}};

    // Removing an unimportant icon should have no effect on other icons.
    ending_stage =
        GenerateColoredIconList({{kUnimportantIconSize2, SK_ColorRED},
                                 {kInstallIconSize, SK_ColorYELLOW},
                                 {kLauncherIconSize, SK_ColorGREEN},
                                 {kInstallabilityIconSize, SK_ColorBLUE}});
    expected_shortcut_colors_if_updated = expected_shortcut_colors_before;
  } else if (TitleUpdate()) {
    ending_stage = starting_stage;  // No icon change.
    expected_shortcut_colors_if_updated = expected_shortcut_colors_before;
  } else {
    NOTREACHED_IN_MIGRATION();  // Unhandled test input.
  }

  OverrideManifest(kManifestTemplate, {app_name, starting_stage});

  webapps::AppId app_id;
  if (IsDefaultApp()) {
    app_id = InstallDefaultApp();
  } else if (IsKioskApp()) {
    app_id = InstallKioskApp();
  } else if (IsPolicyApp()) {
    app_id = InstallPolicyApp();
  } else if (IsWebApp()) {
    app_id = InstallWebApp();
  } else {
    NOTREACHED_IN_MIGRATION();
  }

  const WebApp* web_app = GetProvider().registrar_unsafe().GetAppById(app_id);
  ASSERT_TRUE(web_app);

  if (TitleUpdate())
    app_name = "Different app name";

  OverrideManifest(kManifestTemplate, {app_name, ending_stage});
  SCOPED_TRACE(trace + "Icons before: \n" + starting_stage + "\n" +
               "Icons afer (requested): \n" + ending_stage + "\n");

  if (ExpectTitleUpdate() || ExpectIconUpdate()) {
    DCHECK(!ExpectTitleUpdate() || TitleUpdate());
    DCHECK(!ExpectIconUpdate() || AnyIconUpdate());
    ASSERT_EQ(ManifestUpdateResult::kAppUpdated,
              GetResultAfterPageLoad(GetAppURL()));
    histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                        ManifestUpdateResult::kAppUpdated, 1);
  } else {
    ASSERT_EQ(ManifestUpdateResult::kAppUpToDate,
              GetResultAfterPageLoad(GetAppURL()));
    histogram_tester_.ExpectBucketCount(kUpdateHistogramName,
                                        ManifestUpdateResult::kAppUpdated, 0);
  }

  if (ExpectIconUpdate()) {
    ConfirmShortcutColors(app_id, expected_shortcut_colors_if_updated);
  } else {
    ConfirmShortcutColors(app_id, expected_shortcut_colors_before);
  }

  EXPECT_EQ(ExpectTitleUpdate() ? "Different app name" : "Test app name",
            GetProvider().registrar_unsafe().GetAppShortName(app_id));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ManifestUpdateManagerBrowserTest_AppIdentityParameterized,
    testing::Combine(
        testing::Values(AppIdTestParam::kActionUpdateTitle,
                        AppIdTestParam::kActionUpdateTitleAndLauncherIcon,
                        AppIdTestParam::kActionUpdateLauncherIcon,
                        AppIdTestParam::kActionUpdateInstallIcon,
                        AppIdTestParam::kActionUpdateLauncherAndInstallIcon,
                        AppIdTestParam::kActionUpdateUnimportantIcon,
                        AppIdTestParam::kActionRemoveLauncherIcon,
                        AppIdTestParam::kActionRemoveInstallIcon,
                        AppIdTestParam::kActionRemoveUnimportantIcon,
                        AppIdTestParam::kActionSwitchFromLauncher,
                        AppIdTestParam::kActionSwitchToLauncher),
        testing::Values(AppIdTestParam::kTypeDefaultApp,
                        AppIdTestParam::kTypeKioskApp,
                        AppIdTestParam::kTypePolicyApp,
                        AppIdTestParam::kTypeWebApp),
        testing::Values(AppIdTestParam::kWithFlagNone,
                        AppIdTestParam::kWithFlagAppIdDialogForIcon,
                        AppIdTestParam::kWithFlagPolicyAppIdentity,
                        AppIdTestParam::kWithFlagPolicyAppIdentity |
                            AppIdTestParam::kWithFlagAppIdDialogForIcon)),
    ManifestUpdateManagerBrowserTest_AppIdentityParameterized::ParamToString);

}  // namespace web_app
