// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/policy/web_app_policy_manager.h"

#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/web_applications/externally_installed_web_app_prefs.h"
#include "chrome/browser/web_applications/policy/web_app_policy_constants.h"
#include "chrome/browser/web_applications/test/fake_web_app_registry_controller.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/browser_test.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"

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

base::Value GetForceInstalledAppItem() {
  base::Value item(base::Value::Type::DICTIONARY);
  item.SetKey(kUrlKey, base::Value(kInstallUrl));
  item.SetKey(kDefaultLaunchContainerKey,
              base::Value(kDefaultLaunchContainerWindowValue));
  return item;
}

base::Value GetCustomAppNameItem() {
  base::Value item = GetForceInstalledAppItem();
  item.SetKey(kCustomNameKey, base::Value(kDefaultCustomName));
  return item;
}

base::Value GetCustomAppIconItem() {
  base::Value item = GetForceInstalledAppItem();
  base::Value sub_item(base::Value::Type::DICTIONARY);
  sub_item.SetKey(kCustomIconURLKey, base::Value(kDefaultCustomIconUrl));
  sub_item.SetKey(kCustomIconHashKey, base::Value(kDefaultCustomIconHash));
  item.SetKey(kCustomIconKey, std::move(sub_item));
  return item;
}

base::Value GetCustomAppIconAndNameItem() {
  base::Value item = GetCustomAppIconItem();
  item.SetKey(kCustomNameKey, base::Value(kDefaultCustomName));
  return item;
}

}  // namespace

class WebAppPolicyManagerTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    externally_installed_app_prefs_ =
        std::make_unique<ExternallyInstalledWebAppPrefs>(profile()->GetPrefs());
    fake_registry_controller_ =
        std::make_unique<FakeWebAppRegistryController>();

    controller().SetUp(profile());

    controller().Init();
  }

  void TearDown() override {
    externally_installed_app_prefs_.reset();

    InProcessBrowserTest::TearDown();
  }

  Profile* profile() { return browser()->profile(); }
  FakeWebAppRegistryController& controller() {
    return *fake_registry_controller_;
  }

  content::WebContents* web_contents() const {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  content::RenderFrameHost* RenderFrameHost() const {
    return web_contents()->GetMainFrame();
  }

  ExternallyInstalledWebAppPrefs& externally_installed_app_prefs() {
    return *externally_installed_app_prefs_;
  }

 private:
  std::unique_ptr<ExternallyInstalledWebAppPrefs>
      externally_installed_app_prefs_;

  std::unique_ptr<FakeWebAppRegistryController> fake_registry_controller_;
};

#if BUILDFLAG(IS_CHROMEOS)

IN_PROC_BROWSER_TEST_F(WebAppPolicyManagerTest, DontOverrideManifest) {
  WebAppPolicyManager& policy_manager =
      WebAppProvider::GetForTest(profile())->policy_manager();

  base::Value list(base::Value::Type::LIST);
  list.Append(GetCustomAppIconAndNameItem());
  profile()->GetPrefs()->Set(prefs::kWebAppInstallForceList, std::move(list));

  // Policy is for kInstallUrl, but we pretend to get a manifest
  // from kStartUrl.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kStartUrl)));

  blink::mojom::ManifestPtr manifest = blink::mojom::Manifest::New();
  policy_manager.MaybeOverrideManifest(RenderFrameHost(), manifest);

  EXPECT_EQ(std::u16string(), manifest->name.value_or(std::u16string()));
  EXPECT_EQ(0u, manifest->icons.size());
}

IN_PROC_BROWSER_TEST_F(WebAppPolicyManagerTest,
                       OverrideManifestWithCustomName) {
  WebAppPolicyManager& policy_manager =
      WebAppProvider::GetForTest(profile())->policy_manager();

  base::Value list(base::Value::Type::LIST);
  list.Append(GetCustomAppNameItem());
  profile()->GetPrefs()->Set(prefs::kWebAppInstallForceList, std::move(list));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kInstallUrl)));
  blink::mojom::ManifestPtr manifest = blink::mojom::Manifest::New();
  policy_manager.MaybeOverrideManifest(RenderFrameHost(), manifest);

  EXPECT_EQ(base::UTF8ToUTF16(std::string(kDefaultCustomName)),
            manifest->name.value_or(std::u16string()));
}

IN_PROC_BROWSER_TEST_F(WebAppPolicyManagerTest,
                       OverrideManifestWithCustomIcon) {
  WebAppPolicyManager& policy_manager =
      WebAppProvider::GetForTest(profile())->policy_manager();

  base::Value list(base::Value::Type::LIST);
  list.Append(GetCustomAppIconItem());
  profile()->GetPrefs()->Set(prefs::kWebAppInstallForceList, std::move(list));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(kInstallUrl)));
  blink::mojom::ManifestPtr manifest = blink::mojom::Manifest::New();
  policy_manager.MaybeOverrideManifest(RenderFrameHost(), manifest);

  EXPECT_EQ(1u, manifest->icons.size());
  EXPECT_EQ(GURL(kDefaultCustomIconUrl), manifest->icons[0].src);
}

// Scenario: App with install_url kInstallUrl has a start_url kStartUrl
// specified in manifest. Next time we navigate to kStartUrl, but we still
// need to override the manifest even though the policy key is kInstallUrl.
// This is done by matching the AppId.
IN_PROC_BROWSER_TEST_F(WebAppPolicyManagerTest, MismatchedInstallAndStartUrl) {
  WebAppPolicyManager& policy_manager =
      WebAppProvider::GetForTest(profile())->policy_manager();

  // Set policy:
  base::Value list(base::Value::Type::LIST);
  list.Append(GetCustomAppIconAndNameItem());
  profile()->GetPrefs()->Set(prefs::kWebAppInstallForceList, std::move(list));

  // Create manifest:
  blink::mojom::ManifestPtr manifest = blink::mojom::Manifest::New();
  manifest->name = base::UTF8ToUTF16(std::string(kDefaultAppName));
  manifest->start_url = GURL(kStartUrl);
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
  UpdateWebAppInfoFromManifest(*manifest, GURL(kManifestUrl),
                               install_info.get());
  WebAppProvider::GetForTest(profile())
      ->install_manager()
      .InstallWebAppFromInfo(
          std::move(install_info), true, ForInstallableSite::kYes,
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

#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace web_app
