// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/web_applications/policy/web_app_policy_manager.h"

#include <memory>
#include <string_view>

#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/web_applications/policy/web_app_policy_constants.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_management_type.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/page.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/register_basic_auth_handler.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/test/sk_gmock_support.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/constants/ash_switches.h"
#include "chrome/test/base/testing_profile.h"
#include "components/user_manager/user_names.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "base/base_paths_win.h"
#include "base/test/scoped_path_override.h"
#endif

namespace web_app {

class WebAppPolicyManagerBrowserTest : public WebAppBrowserTestBase {
 public:
  static constexpr char kDefaultAppName[] = "Simple web app";
  static constexpr char kDefaultCustomName[] = "Custom name";
  static constexpr char kDefaultCustomIconHash[] = "abcdef";

  static constexpr char kInstallUrlSuffix[] =
      "/web_apps/install_url/install_url.html";
  static constexpr char kNoManifestInstallUrlSuffix[] =
      "/web_apps/no_manifest.html";
  static constexpr char kRedirectingOtherOriginInstallUrlSuffix[] =
      "/web_apps/install_url/install_url_redirect_other_origin.html";
  static constexpr char kRedirectingSameOriginInstallUrlSuffix[] =
      "/web_apps/install_url/install_url_redirect_same_origin.html";
  static constexpr char kStartUrlSuffix[] = "/web_apps/install_url/index.html";
  static constexpr char kManifestUrlSuffix[] =
      "/web_apps/install_url/manifest.json";
  static constexpr char kCustomIconUrlSuffix[] =
      "/web_apps/install_url/blue-192.png";

  WebAppPolicyManagerBrowserTest() = default;

  void SetUpOnMainThread() override {
    WebAppBrowserTestBase::SetUpOnMainThread();
    embedded_https_test_server().RegisterRequestHandler(
        base::BindRepeating(&WebAppPolicyManagerBrowserTest::RedirectInstallUrl,
                            base::Unretained(this)));
    ASSERT_TRUE(embedded_https_test_server().Start());
  }
  Profile* profile() { return browser()->profile(); }

  content::WebContents* web_contents() const {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  content::RenderFrameHost* RenderFrameHost() const {
    return web_contents()->GetPrimaryMainFrame();
  }

  GURL GetStartUrl() const {
    return embedded_https_test_server().GetURL(kStartUrlSuffix);
  }

  GURL GetInstallUrl() const {
    return embedded_https_test_server().GetURL(kInstallUrlSuffix);
  }

  GURL GetNoManifestInstallUrl() const {
    return embedded_https_test_server().GetURL(kNoManifestInstallUrlSuffix);
  }

  GURL GetRedirectingOtherOriginInstallUrl() const {
    return embedded_https_test_server().GetURL(
        kRedirectingOtherOriginInstallUrlSuffix);
  }

  GURL GetRedirectingSameOriginInstallUrl() const {
    return embedded_https_test_server().GetURL(
        kRedirectingSameOriginInstallUrlSuffix);
  }

  webapps::ManifestId GetManifestId() const {
    return GenerateManifestIdFromStartUrlOnly(
        embedded_https_test_server().GetURL(kStartUrlSuffix));
  }

  webapps::AppId GetAppId() const {
    return GenerateAppIdFromManifestId(GetManifestId());
  }

  void SetPolicyPrefs(std::string_view json,
                      std::vector<std::string> replacements = {}) {
    profile()->GetPrefs()->Set(
        prefs::kWebAppInstallForceList,
        base::JSONReader::Read(
            base::ReplaceStringPlaceholders(json, replacements, nullptr),
            base::JSON_PARSE_CHROMIUM_EXTENSIONS)
            .value());
  }

  std::unique_ptr<net::test_server::HttpResponse> RedirectInstallUrl(
      const net::test_server::HttpRequest& request) {
    if (request.GetURL() != GetRedirectingOtherOriginInstallUrl() &&
        request.GetURL() != GetRedirectingSameOriginInstallUrl()) {
      // Fall back to default handlers.
      return nullptr;
    }
    bool same_origin = request.GetURL() == GetRedirectingSameOriginInstallUrl();
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    std::string destination;
    if (same_origin) {
      destination =
          embedded_https_test_server().GetURL(kInstallUrlSuffix).spec();
    } else {
      destination = embedded_https_test_server()
                        .GetURL("example.org", kInstallUrlSuffix)
                        .spec();
    }
    response->set_code(net::HTTP_TEMPORARY_REDIRECT);
    response->set_content_type("text/html");
    response->AddCustomHeader("Location", destination);
    response->AddCustomHeader("Access-Control-Allow-Origin", "*");
    response->set_content(base::StringPrintf(
        "<!doctype html><p>Redirecting to %s", destination.c_str()));
    return response;
  }

  base::Value::Dict GetForceInstalledAppItem() {
    base::Value::Dict item;
    item.Set(kUrlKey, GetInstallUrl().spec());
    item.Set(kDefaultLaunchContainerKey, kDefaultLaunchContainerWindowValue);
    return item;
  }

  base::Value::Dict GetCustomAppNameItem() {
    base::Value::Dict item = GetForceInstalledAppItem();
    item.Set(kCustomNameKey, kDefaultCustomName);
    return item;
  }

  base::Value::Dict GetCustomAppIconItem() {
    base::Value::Dict item = GetForceInstalledAppItem();
    base::Value::Dict sub_item;
    sub_item.Set(
        kCustomIconURLKey,
        embedded_https_test_server().GetURL(kCustomIconUrlSuffix).spec());
    sub_item.Set(kCustomIconHashKey, kDefaultCustomIconHash);
    item.Set(kCustomIconKey, std::move(sub_item));
    return item;
  }

  base::Value::Dict GetCustomAppIconAndNameItem() {
    base::Value::Dict item = GetCustomAppIconItem();
    item.Set(kCustomNameKey, kDefaultCustomName);
    return item;
  }

  bool SetPolicyAndWaitForInstall(
      base::DictValue app_config,
      std::optional<webapps::AppId> app_id = std::nullopt) {
    app_id = app_id.value_or(GetAppId());
    web_app::WebAppTestInstallObserver observer(browser()->profile());
    observer.BeginListening({*app_id});
    profile()->GetPrefs()->SetList(
        prefs::kWebAppInstallForceList,
        base::ListValue().Append(std::move(app_config)));
    return observer.Wait() == *app_id;
  }
};

IN_PROC_BROWSER_TEST_F(WebAppPolicyManagerBrowserTest,
                       OverrideManifestWithCustomName) {
  ASSERT_TRUE(SetPolicyAndWaitForInstall(GetCustomAppNameItem()));

  std::string name = provider().registrar_unsafe().GetAppShortName(GetAppId());
  EXPECT_EQ(kDefaultCustomName, name);
}

IN_PROC_BROWSER_TEST_F(WebAppPolicyManagerBrowserTest,
                       OverrideManifestWithCustomIcon) {
  ASSERT_TRUE(SetPolicyAndWaitForInstall(GetCustomAppIconItem()));

  base::test::TestFuture<WebAppIconManager::WebAppBitmaps> disk_bitmaps;
  provider().icon_manager().ReadAllIcons(GetAppId(),
                                         disk_bitmaps.GetCallback());
  ASSERT_TRUE(disk_bitmaps.Wait());
  const std::map<SquareSizePx, SkBitmap>& any_icons =
      disk_bitmaps.Get().trusted_icons.any;
  ASSERT_THAT(any_icons, testing::Contains(testing::Pair(192, testing::_)));
  EXPECT_THAT(
      any_icons.at(192),
      gfx::test::EqualsBitmap(gfx::test::CreateBitmap(192, SK_ColorBLUE)));
}

// Ensure the manifest start_url is used as the manifest id when the manifest id
// is not present.
IN_PROC_BROWSER_TEST_F(WebAppPolicyManagerBrowserTest, AppIdWhenNoManifestId) {
  WebAppProvider& provider = *WebAppProvider::GetForTest(profile());

  const GURL start_url = https_server()->GetURL("/web_apps/basic.html");
  const webapps::AppId app_id = GenerateAppIdFromManifestId(
      GenerateManifestIdFromStartUrlOnly(start_url));
  web_app::WebAppTestInstallObserver observer(profile());
  observer.BeginListening({});
  const GURL install_url =
      https_server()->GetURL("/web_apps/get_manifest.html?no_manifest_id.json");
  profile()->GetPrefs()->SetList(
      prefs::kWebAppInstallForceList,
      base::Value::List().Append(
          base::Value::Dict().Set(kUrlKey, install_url.spec())));
  ASSERT_EQ(app_id, observer.Wait());

  const WebApp* app = provider.registrar_unsafe().GetAppById(app_id);

  ASSERT_TRUE(app) << provider.registrar_unsafe().AsDebugValue();
  EXPECT_EQ(app->management_to_external_config_map(),
            (WebApp::ExternalConfigMap{{WebAppManagement::Type::kPolicy,
                                        {/*is_placeholder=*/false,
                                         /*install_urls=*/{install_url},
                                         /*additional_policy_ids=*/{}}}}));
}

// TODO(crbug.com/40256661): Flaky on Mac and Linux.
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_OverrideExistingInstall DISABLED_OverrideExistingInstall
#else
#define MAYBE_OverrideExistingInstall OverrideExistingInstall
#endif
IN_PROC_BROWSER_TEST_F(WebAppPolicyManagerBrowserTest,
                       MAYBE_OverrideExistingInstall) {
  // Install app first by user, then policy should override.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetStartUrl()));
  webapps::AppId app_id = test::InstallPwaForCurrentUrl(browser());
  ASSERT_EQ(app_id, GetAppId());

  // Set policy:
  base::test::TestFuture<void> on_apps_synchronized;
  provider().policy_manager().SetOnAppsSynchronizedCompletedCallbackForTesting(
      on_apps_synchronized.GetCallback());
  base::Value::List list;
  list.Append(GetCustomAppIconAndNameItem());
  profile()->GetPrefs()->SetList(prefs::kWebAppInstallForceList,
                                 std::move(list));
  ASSERT_TRUE(on_apps_synchronized.Wait());
  provider().command_manager().AwaitAllCommandsCompleteForTesting();

  // We should have the custom name and icon.
  std::string name = provider().registrar_unsafe().GetAppShortName(GetAppId());
  EXPECT_EQ(kDefaultCustomName, name);
  base::test::TestFuture<WebAppIconManager::WebAppBitmaps> disk_bitmaps;
  provider().icon_manager().ReadAllIcons(GetAppId(),
                                         disk_bitmaps.GetCallback());
  ASSERT_TRUE(disk_bitmaps.Wait());
  const std::map<SquareSizePx, SkBitmap>& any_icons =
      disk_bitmaps.Get().trusted_icons.any;
  ASSERT_THAT(any_icons, testing::Contains(testing::Pair(192, testing::_)));
  EXPECT_THAT(
      any_icons.at(192),
      gfx::test::EqualsBitmap(gfx::test::CreateBitmap(192, SK_ColorBLUE)));
}

// Cross-origin redirection should result in a placeholder app, but the name and
// icon should work.
IN_PROC_BROWSER_TEST_F(WebAppPolicyManagerBrowserTest,
                       RedirectedPlaceholderAppHasNameIcon) {
  base::Value::Dict app = GetCustomAppIconAndNameItem();
  app.Set(kUrlKey, GetRedirectingOtherOriginInstallUrl().spec());
  webapps::AppId app_id =
      GenerateAppIdFromManifestId(GetRedirectingOtherOriginInstallUrl());
  ASSERT_TRUE(SetPolicyAndWaitForInstall(std::move(app), app_id));

  // We should have the custom name and icon.
  std::string name = provider().registrar_unsafe().GetAppShortName(app_id);
  EXPECT_EQ(kDefaultCustomName, name);
  base::test::TestFuture<WebAppIconManager::WebAppBitmaps> disk_bitmaps;
  provider().icon_manager().ReadAllIcons(app_id, disk_bitmaps.GetCallback());
  ASSERT_TRUE(disk_bitmaps.Wait());
  const std::map<SquareSizePx, SkBitmap>& any_icons =
      disk_bitmaps.Get().trusted_icons.any;
  ASSERT_THAT(any_icons, testing::Contains(testing::Pair(192, testing::_)));
  EXPECT_THAT(
      any_icons.at(192),
      gfx::test::EqualsBitmap(gfx::test::CreateBitmap(192, SK_ColorBLUE)));
}

// Same-origin redirect should be 'followed' and the manifest should still be
// overridden.
IN_PROC_BROWSER_TEST_F(WebAppPolicyManagerBrowserTest,
                       RedirectedSameOriginAppHasNameIcon) {
  base::Value::Dict app = GetCustomAppIconAndNameItem();
  app.Set(kUrlKey, GetRedirectingSameOriginInstallUrl().spec());
  ASSERT_TRUE(SetPolicyAndWaitForInstall(std::move(app)));

  // We should have the custom name and icon.
  std::string name = provider().registrar_unsafe().GetAppShortName(GetAppId());
  EXPECT_EQ(kDefaultCustomName, name);
  base::test::TestFuture<WebAppIconManager::WebAppBitmaps> disk_bitmaps;
  provider().icon_manager().ReadAllIcons(GetAppId(),
                                         disk_bitmaps.GetCallback());
  ASSERT_TRUE(disk_bitmaps.Wait());
  const std::map<SquareSizePx, SkBitmap>& any_icons =
      disk_bitmaps.Get().trusted_icons.any;
  ASSERT_THAT(any_icons, testing::Contains(testing::Pair(192, testing::_)));
  EXPECT_THAT(
      any_icons.at(192),
      gfx::test::EqualsBitmap(gfx::test::CreateBitmap(192, SK_ColorBLUE)));
}

#if BUILDFLAG(IS_CHROMEOS)

// Scenario: A policy installed web app is replacing an existing app causing it
// to be uninstalled after the policy app is installed.
IN_PROC_BROWSER_TEST_F(WebAppPolicyManagerBrowserTest, MigratingPolicyApp) {
  // Install old app to replace.
  auto install_info = WebAppInstallInfo::CreateWithStartUrlForTesting(
      GURL("https://some.app.com"));
  install_info->title = u"some app";
  webapps::AppId old_app_id =
      test::InstallWebApp(profile(), std::move(install_info));

  WebAppTestUninstallObserver uninstall_observer(profile());
  uninstall_observer.BeginListening({old_app_id});

  // Update policy app to replace old app.
  SetPolicyPrefs(R"([{
    "url": "https://example.com/install",
    "uninstall_and_replace": ["$1"]
  }])",
                 {old_app_id});

  // Old app should get uninstalled by policy app install.
  uninstall_observer.Wait();
}

#endif  // BUILDFLAG(IS_CHROMEOS)

class WebAppPolicyManagerGuestModeTest : public WebAppPolicyManagerBrowserTest {
 public:
  WebAppPolicyManagerGuestModeTest() = default;
  WebAppPolicyManagerGuestModeTest(const WebAppPolicyManagerGuestModeTest&) =
      delete;
  WebAppPolicyManagerGuestModeTest& operator=(
      const WebAppPolicyManagerGuestModeTest&) = delete;

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    WebAppPolicyManagerBrowserTest::SetUpCommandLine(command_line);
#if BUILDFLAG(IS_CHROMEOS)
    command_line->AppendSwitch(ash::switches::kGuestSession);
    command_line->AppendSwitchASCII(ash::switches::kLoginUser,
                                    user_manager::kGuestUserName);
    command_line->AppendSwitchASCII(ash::switches::kLoginProfile,
                                    TestingProfile::kTestUserProfileDir);
    command_line->AppendSwitch(switches::kIncognito);
#endif
  }
  web_app::OsIntegrationTestOverrideBlockingRegistration faked_os_integration_;
};

IN_PROC_BROWSER_TEST_F(WebAppPolicyManagerGuestModeTest,
                       DoNotCreateAppsOnGuestMode) {
  const webapps::AppId app_id =
#if BUILDFLAG(IS_CHROMEOS)
      // Installable Manager returns an IN_INCOGNITO error for incognito
      // profiles on CrOS, even in guest profiles. So the app id will be based
      // on the install url, not the start url (as no manifest is loaded).
      // TODO(http://crbug.com/452122299): This probably needs to be fixed where
      // the InstallableManager should allow the install.
      GenerateAppIdFromManifestId(
          GenerateManifestIdFromStartUrlOnly(GetInstallUrl()));
#else
      // Note: Incognito flag is not set on other platforms.
      GetAppId();
#endif
  ASSERT_TRUE(SetPolicyAndWaitForInstall(GetForceInstalledAppItem(), app_id));

  // This test should pass on all platforms, including on a ChromeOS
  // guest session.
  EXPECT_EQ(proto::InstallState::INSTALLED_WITH_OS_INTEGRATION,
            provider().registrar_unsafe().GetInstallState(app_id));

#if !BUILDFLAG(IS_CHROMEOS)
  Profile* guest_profile = CreateGuestBrowser()->profile();
  EXPECT_FALSE(WebAppProvider::GetForTest(guest_profile));
#endif
}

class WebAppPolicyManagerBrowserTestWithAuthProxy
    : public WebAppPolicyManagerBrowserTest {
 public:
  WebAppPolicyManagerBrowserTestWithAuthProxy() = default;

  // WebAppControllerBrowserTest:
  void SetUp() override {
    // Set up and start "proxy" server. Since this test doesn't actually
    // authenticate with the proxy, but instead only test the case of unhandled
    // proxy auth challenges, only need to wire up those auth challenges.
    RegisterProxyBasicAuthHandler(auth_proxy_server_, "user", "pass");
    ASSERT_TRUE(auth_proxy_server_.Start());

    WebAppPolicyManagerBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(
        ::switches::kProxyServer,
        auth_proxy_server_.host_port_pair().ToString());
    WebAppPolicyManagerBrowserTest::SetUpCommandLine(command_line);
  }

  Profile* profile() { return browser()->profile(); }

  net::test_server::EmbeddedTestServer auth_proxy_server_{
      net::test_server::EmbeddedTestServer ::Type::TYPE_HTTPS};
};

IN_PROC_BROWSER_TEST_F(WebAppPolicyManagerBrowserTestWithAuthProxy, Install) {
  ASSERT_TRUE(SetPolicyAndWaitForInstall(GetCustomAppIconAndNameItem()));

  // Policy is for kInstallUrl, but we pretend to get a manifest
  // from kStartUrl.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(GetStartUrl())));

  base::test::TestFuture<blink::mojom::ManifestRequestResult, const GURL&,
                         blink::mojom::ManifestPtr>
      manifest_future;
  browser()
      ->GetActiveTabInterface()
      ->GetContents()
      ->GetPrimaryPage()
      .GetManifest(manifest_future.GetCallback());
  ASSERT_TRUE(manifest_future.Wait());
  ASSERT_EQ(blink::mojom::ManifestRequestResult::kSuccess,
            manifest_future.Get<blink::mojom::ManifestRequestResult>());
  const blink::mojom::ManifestPtr& manifest =
      manifest_future.Get<blink::mojom::ManifestPtr>();

  EXPECT_EQ(kDefaultCustomName,
            base::UTF16ToASCII(manifest->name.value_or(std::u16string())));
  ASSERT_EQ(1u, manifest->icons.size());
  EXPECT_TRUE(manifest->icons[0].src.spec().ends_with(kCustomIconUrlSuffix));
}

}  // namespace web_app
