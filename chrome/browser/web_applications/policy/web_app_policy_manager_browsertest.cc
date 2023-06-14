// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"
#include "chrome/browser/web_applications/policy/web_app_policy_manager.h"

#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/web_applications/policy/web_app_policy_constants.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/browser_test.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_switches.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/testing_profile.h"
#include "components/user_manager/user_names.h"
#endif

namespace web_app {

namespace {

const char kDefaultAppName[] = "app name";
const char kDefaultAppIconUrl1[] = "https://example.com/icon1.png";
const char kDefaultAppIconUrl2[] = "https://example.com/icon2.png";
const char kDefaultCustomName[] = "custom name";
const char kDefaultCustomIconUrl[] = "https://foo.example.com/custom_icon.png";
const char kDefaultCustomIconHash[] = "abcdef";

constexpr char kInstallUrl[] = "https://example.com/install";
constexpr char kStartUrl[] = "https://example.com/start/?u=1";
constexpr char kManifestUrl[] = "https://example.com/install/manifest.json";

base::Value::Dict GetForceInstalledAppItem() {
  base::Value::Dict item;
  item.Set(kUrlKey, kInstallUrl);
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
  sub_item.Set(kCustomIconURLKey, kDefaultCustomIconUrl);
  sub_item.Set(kCustomIconHashKey, kDefaultCustomIconHash);
  item.Set(kCustomIconKey, std::move(sub_item));
  return item;
}

base::Value::Dict GetCustomAppIconAndNameItem() {
  base::Value::Dict item = GetCustomAppIconItem();
  item.Set(kCustomNameKey, kDefaultCustomName);
  return item;
}

}  // namespace

class WebAppPolicyManagerBrowserTest : public WebAppControllerBrowserTest {
 public:
  WebAppPolicyManagerBrowserTest() = default;

  void SetUpOnMainThread() override {
    WebAppControllerBrowserTest::SetUpOnMainThread();
  }

  void TearDown() override { WebAppControllerBrowserTest::TearDown(); }

  Profile* profile() { return browser()->profile(); }

  content::WebContents* web_contents() const {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  content::RenderFrameHost* RenderFrameHost() const {
    return web_contents()->GetPrimaryMainFrame();
  }

  void SetPolicyPrefs(base::StringPiece json,
                      std::vector<std::string> replacements = {}) {
    profile()->GetPrefs()->Set(
        prefs::kWebAppInstallForceList,
        base::JSONReader::Read(
            base::ReplaceStringPlaceholders(json, replacements, nullptr))
            .value());
  }
};

IN_PROC_BROWSER_TEST_F(WebAppPolicyManagerBrowserTest,
                       OverrideManifestWithCustomName) {
  WebAppPolicyManager& policy_manager =
      WebAppProvider::GetForTest(profile())->policy_manager();

  base::Value::List list;
  list.Append(GetCustomAppNameItem());
  profile()->GetPrefs()->SetList(prefs::kWebAppInstallForceList,
                                 std::move(list));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kInstallUrl)));
  blink::mojom::ManifestPtr manifest = blink::mojom::Manifest::New();
  policy_manager.MaybeOverrideManifest(RenderFrameHost(), manifest);

  EXPECT_EQ(base::UTF8ToUTF16(std::string(kDefaultCustomName)),
            manifest->name.value_or(std::u16string()));
}

IN_PROC_BROWSER_TEST_F(WebAppPolicyManagerBrowserTest,
                       OverrideManifestWithCustomIcon) {
  WebAppPolicyManager& policy_manager =
      WebAppProvider::GetForTest(profile())->policy_manager();

  base::Value::List list;
  list.Append(GetCustomAppIconItem());
  profile()->GetPrefs()->SetList(prefs::kWebAppInstallForceList,
                                 std::move(list));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kInstallUrl)));
  blink::mojom::ManifestPtr manifest = blink::mojom::Manifest::New();
  policy_manager.MaybeOverrideManifest(RenderFrameHost(), manifest);

  EXPECT_EQ(1u, manifest->icons.size());
  EXPECT_EQ(GURL(kDefaultCustomIconUrl), manifest->icons[0].src);
}

IN_PROC_BROWSER_TEST_F(WebAppPolicyManagerBrowserTest, DontOverrideManifest) {
  WebAppPolicyManager& policy_manager =
      WebAppProvider::GetForTest(profile())->policy_manager();

  base::Value::List list;
  list.Append(GetCustomAppIconAndNameItem());
  profile()->GetPrefs()->SetList(prefs::kWebAppInstallForceList,
                                 std::move(list));

  // Policy is for kInstallUrl, but we pretend to get a manifest
  // from kStartUrl.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kStartUrl)));

  blink::mojom::ManifestPtr manifest = blink::mojom::Manifest::New();
  policy_manager.MaybeOverrideManifest(RenderFrameHost(), manifest);

  EXPECT_EQ(std::u16string(), manifest->name.value_or(std::u16string()));
  EXPECT_EQ(0u, manifest->icons.size());
}

// Ensure the manifest start_url is used as the manifest id when the manifest id
// is not present.
IN_PROC_BROWSER_TEST_F(WebAppPolicyManagerBrowserTest, AppIdWhenNoManifestId) {
  WebAppProvider& provider = *WebAppProvider::GetForTest(profile());

  base::test::TestFuture<void> future;
  provider.policy_manager().SetOnAppsSynchronizedCompletedCallbackForTesting(
      future.GetCallback());
  const GURL install_url =
      https_server()->GetURL("/web_apps/get_manifest.html?no_manifest_id.json");
  profile()->GetPrefs()->SetList(
      prefs::kWebAppInstallForceList,
      base::Value::List().Append(
          base::Value::Dict().Set(kUrlKey, install_url.spec())));
  future.Get();

  const GURL start_url = https_server()->GetURL("/web_apps/basic.html");
  const AppId app_id = GenerateAppIdFromManifestId(
      GenerateManifestIdFromStartUrlOnly(start_url));
  const WebApp* app = provider.registrar_unsafe().GetAppById(app_id);

  ASSERT_TRUE(app) << provider.registrar_unsafe().AsDebugValue();
  EXPECT_EQ(app->management_to_external_config_map(),
            (WebApp::ExternalConfigMap{{WebAppManagement::Type::kPolicy,
                                        {/*is_placeholder=*/false,
                                         /*install_urls=*/{install_url},
                                         /*additional_policy_ids=*/{}}}}));
}

// Scenario: App with install_url kInstallUrl has a start_url kStartUrl
// specified in manifest. Next time we navigate to kStartUrl, but we still
// need to override the manifest even though the policy key is kInstallUrl.
// This is done by matching the AppId.
// TODO(crbug.com/1415979): Flaky on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_MismatchedInstallAndStartUrl DISABLED_MismatchedInstallAndStartUrl
#else
#define MAYBE_MismatchedInstallAndStartUrl MismatchedInstallAndStartUrl
#endif
IN_PROC_BROWSER_TEST_F(WebAppPolicyManagerBrowserTest,
                       MAYBE_MismatchedInstallAndStartUrl) {
  WebAppPolicyManager& policy_manager =
      WebAppProvider::GetForTest(profile())->policy_manager();

  // Set policy:
  base::Value::List list;
  list.Append(GetCustomAppIconAndNameItem());
  profile()->GetPrefs()->SetList(prefs::kWebAppInstallForceList,
                                 std::move(list));

  // Create manifest:
  blink::mojom::ManifestPtr manifest = blink::mojom::Manifest::New();
  manifest->name = base::UTF8ToUTF16(std::string(kDefaultAppName));
  manifest->start_url = GURL(kStartUrl);
  manifest->id = GenerateManifestIdFromStartUrlOnly(manifest->start_url);
  // Populate manifest with 2 icons:
  blink::Manifest::ImageResource icon;
  icon.src = GURL(kDefaultAppIconUrl1);
  icon.sizes.emplace_back(0, 0);  // Represents size "any".
  icon.purpose.push_back(blink::mojom::ManifestImageResource::Purpose::ANY);
  manifest->icons.emplace_back(icon);
  icon.src = GURL(kDefaultAppIconUrl2);
  manifest->icons.emplace_back(icon);

  // Install the web app:
  std::unique_ptr<WebAppInstallInfo> install_info =
      std::make_unique<WebAppInstallInfo>();
  install_info->install_url = GURL(kInstallUrl);
  UpdateWebAppInfoFromManifest(*manifest, GURL(kManifestUrl),
                               install_info.get());

  auto* provider = WebAppProvider::GetForTest(profile());
  provider->scheduler().InstallFromInfo(
      std::move(install_info),
      /*overwrite_existing_manifest_fields=*/true,
      webapps::WebappInstallSource::EXTERNAL_POLICY, base::DoNothing());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kStartUrl)));

  policy_manager.MaybeOverrideManifest(RenderFrameHost(), manifest);

  EXPECT_EQ(base::UTF8ToUTF16(std::string(kDefaultCustomName)),
            manifest->name.value_or(std::u16string()));
  EXPECT_EQ(1u, manifest->icons.size());
  EXPECT_EQ(GURL(kDefaultCustomIconUrl), manifest->icons[0].src);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)

// Scenario: A policy installed web app is replacing an existing app causing it
// to be uninstalled after the policy app is installed.
// This test does not yet work in Lacros because
// AppServiceProxyLacros::UninstallSilently() has not yet been implemented.
IN_PROC_BROWSER_TEST_F(WebAppPolicyManagerBrowserTest, MigratingPolicyApp) {
  // Install old app to replace.
  auto install_info = std::make_unique<WebAppInstallInfo>();
  install_info->start_url = GURL("https://some.app.com");
  install_info->title = u"some app";
  AppId old_app_id = test::InstallWebApp(profile(), std::move(install_info));

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

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

class WebAppPolicyManagerGuestModeTest : public InProcessBrowserTest {
 public:
  WebAppPolicyManagerGuestModeTest() = default;
  WebAppPolicyManagerGuestModeTest(const WebAppPolicyManagerGuestModeTest&) =
      delete;
  WebAppPolicyManagerGuestModeTest& operator=(
      const WebAppPolicyManagerGuestModeTest&) = delete;

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    command_line->AppendSwitch(ash::switches::kGuestSession);
    command_line->AppendSwitchASCII(ash::switches::kLoginUser,
                                    user_manager::kGuestUserName);
    command_line->AppendSwitchASCII(ash::switches::kLoginProfile,
                                    TestingProfile::kTestUserProfileDir);
    command_line->AppendSwitch(switches::kIncognito);
#endif
  }
};

IN_PROC_BROWSER_TEST_F(WebAppPolicyManagerGuestModeTest,
                       DoNotCreateAppsOnGuestMode) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  test::ScopedSkipMainProfileCheck skip_main_profile_check;
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  base::Value::List app_list;
  app_list.Append(GetForceInstalledAppItem());

  Profile* test_profile = browser()->profile();
  WebAppProvider* test_provider = WebAppProvider::GetForTest(test_profile);

  base::test::TestFuture<void> future;
  test_provider->policy_manager()
      .SetOnAppsSynchronizedCompletedCallbackForTesting(future.GetCallback());
  test_profile->GetPrefs()->SetList(prefs::kWebAppInstallForceList,
                                    std::move(app_list));
  EXPECT_TRUE(future.Wait());

  const AppId& app_id =
      GenerateAppId(/*manifest_id=*/absl::nullopt, GURL(kInstallUrl));

  // This test should pass on all platforms, including on a ChromeOS
  // guest session.
  EXPECT_TRUE(test_provider->registrar_unsafe().IsInstalled(app_id));

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  // This waits until ExternallyManagedAppManager::SynchronizeInstalledApps()
  // has finished running, hence we know that for guest mode, the app was not
  // installed.
  Profile* guest_profile = CreateGuestBrowser()->profile();
  WebAppProvider* guest_provider = WebAppProvider::GetForTest(guest_profile);
  DCHECK(guest_provider);
  test::WaitUntilWebAppProviderAndSubsystemsReady(guest_provider);
  EXPECT_FALSE(guest_provider->registrar_unsafe().IsInstalled(app_id));
#endif
}

}  // namespace web_app
