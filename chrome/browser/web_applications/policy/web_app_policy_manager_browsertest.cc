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
#include "chrome/browser/web_applications/externally_installed_web_app_prefs.h"
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

class WebAppPolicyManagerBrowserTest
    : public WebAppControllerBrowserTest,
      public testing::WithParamInterface<test::ExternalPrefMigrationTestCases> {
 public:
  WebAppPolicyManagerBrowserTest() {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;

    switch (GetParam()) {
      case test::ExternalPrefMigrationTestCases::kDisableMigrationReadPref:
        disabled_features.push_back(features::kMigrateExternalPrefsToWebAppDB);
        disabled_features.push_back(
            features::kUseWebAppDBInsteadOfExternalPrefs);
        break;
      case test::ExternalPrefMigrationTestCases::kDisableMigrationReadDB:
        disabled_features.push_back(features::kMigrateExternalPrefsToWebAppDB);
        enabled_features.push_back(
            features::kUseWebAppDBInsteadOfExternalPrefs);
        break;
      case test::ExternalPrefMigrationTestCases::kEnableMigrationReadPref:
        enabled_features.push_back(features::kMigrateExternalPrefsToWebAppDB);
        disabled_features.push_back(
            features::kUseWebAppDBInsteadOfExternalPrefs);
        break;
      case test::ExternalPrefMigrationTestCases::kEnableMigrationReadDB:
        enabled_features.push_back(features::kMigrateExternalPrefsToWebAppDB);
        enabled_features.push_back(
            features::kUseWebAppDBInsteadOfExternalPrefs);
        break;
    }
    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

  void SetUpOnMainThread() override {
    WebAppControllerBrowserTest::SetUpOnMainThread();
    externally_installed_app_prefs_ =
        std::make_unique<ExternallyInstalledWebAppPrefs>(profile()->GetPrefs());
  }

  void TearDown() override {
    externally_installed_app_prefs_.reset();

    WebAppControllerBrowserTest::TearDown();
  }

  Profile* profile() { return browser()->profile(); }

  content::WebContents* web_contents() const {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  content::RenderFrameHost* RenderFrameHost() const {
    return web_contents()->GetPrimaryMainFrame();
  }

  ExternallyInstalledWebAppPrefs& externally_installed_app_prefs() {
    return *externally_installed_app_prefs_;
  }

  void SetPolicyPrefs(base::StringPiece json,
                      std::vector<std::string> replacements = {}) {
    profile()->GetPrefs()->Set(
        prefs::kWebAppInstallForceList,
        base::JSONReader::Read(
            base::ReplaceStringPlaceholders(json, replacements, nullptr))
            .value());
  }

 private:
  std::unique_ptr<ExternallyInstalledWebAppPrefs>
      externally_installed_app_prefs_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(WebAppPolicyManagerBrowserTest,
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

IN_PROC_BROWSER_TEST_P(WebAppPolicyManagerBrowserTest,
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

IN_PROC_BROWSER_TEST_P(WebAppPolicyManagerBrowserTest, DontOverrideManifest) {
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
IN_PROC_BROWSER_TEST_P(WebAppPolicyManagerBrowserTest,
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

  // Install the web app, and add it in the externally_installed_app_prefs:
  auto install_source = ExternalInstallSource::kExternalPolicy;
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

  externally_installed_app_prefs().Insert(
      GURL(kInstallUrl), GenerateAppId(absl::nullopt, GURL(kStartUrl)),
      install_source);

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
IN_PROC_BROWSER_TEST_P(WebAppPolicyManagerBrowserTest, MigratingPolicyApp) {
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

INSTANTIATE_TEST_SUITE_P(
    All,
    WebAppPolicyManagerBrowserTest,
    ::testing::Values(
        test::ExternalPrefMigrationTestCases::kDisableMigrationReadPref,
        test::ExternalPrefMigrationTestCases::kDisableMigrationReadDB,
        test::ExternalPrefMigrationTestCases::kEnableMigrationReadPref,
        test::ExternalPrefMigrationTestCases::kEnableMigrationReadDB),
    test::GetExternalPrefMigrationTestName);

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
