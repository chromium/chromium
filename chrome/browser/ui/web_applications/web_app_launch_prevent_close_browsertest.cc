// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/web_app_id_constants.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/web_applications/policy/web_app_policy_constants.h"
#include "chrome/browser/web_applications/policy/web_app_policy_manager.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/window_open_disposition.h"
#include "url/origin.h"
#endif

namespace web_app {

namespace {

constexpr char kCalculatorAppUrl[] = "https://calculator.apps.chrome/";

}  // namespace

class PreventCloseControllerBrowserTest : public WebAppBrowserTestBase {
 public:
  PreventCloseControllerBrowserTest() { BuildAndInitFeatureList(); }

 protected:
  void BuildAndInitFeatureList() {
    scoped_feature_list_.InitWithFeatures({features::kDesktopPWAsPreventClose},
                                          {});
  }

  void ForceInstallWebApp(const webapps::AppId& app_id, const GURL& url) {
    web_app::WebAppTestInstallObserver observer(browser()->GetProfile());
    observer.BeginListening({app_id});

    profile()->GetPrefs()->SetList(
        prefs::kWebAppInstallForceList,
        base::ListValue().Append(base::DictValue()
                                     .Set(kUrlKey, url.spec())
                                     .Set(kDefaultLaunchContainerKey,
                                          kDefaultLaunchContainerWindowValue)));

    const webapps::AppId installed_app_id = observer.Wait();
    EXPECT_EQ(installed_app_id, app_id);
  }

  void ConfigurePreventClose(const GURL& url, bool prevent_close) {
    profile()->GetPrefs()->SetList(
        prefs::kWebAppSettings,
        base::ListValue().Append(base::DictValue()
                                     .Set(kManifestId, url.spec())
                                     .Set(kRunOnOsLogin, kRunWindowed)
                                     .Set(kPreventClose, prevent_close)));
    WebAppProvider::GetForTest(profile())
        ->policy_manager()
        .RefreshPolicySettingsForTesting();
  }

  void ClearPolicySettings() {
    profile()->GetPrefs()->SetList(prefs::kWebAppSettings, base::ListValue());
    profile()->GetPrefs()->SetList(prefs::kWebAppInstallForceList,
                                   base::ListValue());
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(PreventCloseControllerBrowserTest,
                       NonClosablePWADoesNotLaunchAdditionalWindow) {
  absl::Cleanup policy_cleanup = [this] { ClearPolicySettings(); };

  // Arrange with non-closable PWA and start it a first time
  size_t expected_browser_count =
      ProfileBrowserCollection::GetForProfile(profile())->GetSize();

  const GURL url(kCalculatorAppUrl);
  ForceInstallWebApp(ash::kCalculatorAppId, url);

  BrowserWindowInterface* browser =
      LaunchWebAppBrowserAndWait(ash::kCalculatorAppId);
  ++expected_browser_count;

  ASSERT_TRUE(browser);
  EXPECT_EQ(expected_browser_count,
            ProfileBrowserCollection::GetForProfile(profile())->GetSize());

  ConfigurePreventClose(url, /*prevent_close=*/true);

  // Act by launching PWA a second time
  BrowserWindowInterface* second_browser =
      LaunchWebAppBrowser(ash::kCalculatorAppId);

#if BUILDFLAG(IS_CHROMEOS)
  // Assert that the PWA only has one existing window
  EXPECT_TRUE(WebAppProvider::GetForTest(profile())
                  ->registrar_unsafe()
                  .IsPreventCloseEnabled(ash::kCalculatorAppId));
  EXPECT_EQ(browser, second_browser);
  EXPECT_EQ(expected_browser_count,
            ProfileBrowserCollection::GetForProfile(profile())->GetSize());
#else
  // On other platforms, the prevent close should not be enabled.
  EXPECT_FALSE(WebAppProvider::GetForTest(profile())
                   ->registrar_unsafe()
                   .IsPreventCloseEnabled(ash::kCalculatorAppId));
  EXPECT_NE(browser, second_browser);
  EXPECT_EQ(expected_browser_count + 1,
            ProfileBrowserCollection::GetForProfile(profile())->GetSize());
#endif
}

IN_PROC_BROWSER_TEST_F(PreventCloseControllerBrowserTest,
                       ClosablePWALaunchesAdditionalWindow) {
  absl::Cleanup policy_cleanup = [this] { ClearPolicySettings(); };

  size_t expected_browser_count =
      ProfileBrowserCollection::GetForProfile(profile())->GetSize();

  const GURL url(kCalculatorAppUrl);
  ForceInstallWebApp(ash::kCalculatorAppId, url);

  BrowserWindowInterface* browser =
      LaunchWebAppBrowserAndWait(ash::kCalculatorAppId);
  ++expected_browser_count;

  ASSERT_TRUE(browser);
  EXPECT_EQ(expected_browser_count,
            ProfileBrowserCollection::GetForProfile(profile())->GetSize());

  ConfigurePreventClose(url, /*prevent_close=*/false);

  // Act by launching PWA a second time
  BrowserWindowInterface* second_browser =
      LaunchWebAppBrowserAndWait(ash::kCalculatorAppId);
  expected_browser_count++;

  // Assert that the PWA only has one existing window
  EXPECT_NE(browser, second_browser);
  EXPECT_EQ(expected_browser_count,
            ProfileBrowserCollection::GetForProfile(profile())->GetSize());
}

#if BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(PreventCloseControllerBrowserTest,
                       NonClosablePWAHidesSameAppContextMenuItem) {
  absl::Cleanup policy_cleanup = [this] { ClearPolicySettings(); };
  const GURL url(kCalculatorAppUrl);
  ForceInstallWebApp(ash::kCalculatorAppId, url);
  BrowserWindowInterface* app_browser =
      LaunchWebAppBrowserAndWait(ash::kCalculatorAppId);
  content::WebContents* contents =
      app_browser->GetTabStripModel()->GetActiveWebContents();

  content::ContextMenuParams params;
  params.page_url = contents->GetLastCommittedURL();
  params.frame_url = params.page_url;
  params.frame_origin = url::Origin::Create(params.page_url);
  params.link_url = url;
  params.unfiltered_link_url = url;
  {
    TestRenderViewContextMenu menu(*contents->GetPrimaryMainFrame(), params);
    menu.Init();
    ASSERT_TRUE(menu.IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKBOOKMARKAPP));
  }

  ConfigurePreventClose(url, /*prevent_close=*/true);
  ASSERT_TRUE(provider().registrar_unsafe().IsPreventCloseEnabled(
      ash::kCalculatorAppId));
  TestRenderViewContextMenu menu(*contents->GetPrimaryMainFrame(), params);
  menu.Init();
  EXPECT_FALSE(menu.IsItemPresent(IDC_CONTENT_CONTEXT_OPENLINKBOOKMARKAPP));
}

IN_PROC_BROWSER_TEST_F(PreventCloseControllerBrowserTest,
                       NonClosablePWAReusesWindowForNewWindowDisposition) {
  absl::Cleanup policy_cleanup = [this] { ClearPolicySettings(); };
  const GURL url(kCalculatorAppUrl);
  ForceInstallWebApp(ash::kCalculatorAppId, url);
  BrowserWindowInterface* app_browser =
      LaunchWebAppBrowserAndWait(ash::kCalculatorAppId);
  const size_t browser_count =
      ProfileBrowserCollection::GetForProfile(profile())->GetSize();
  ConfigurePreventClose(url, /*prevent_close=*/true);
  ASSERT_TRUE(provider().registrar_unsafe().IsPreventCloseEnabled(
      ash::kCalculatorAppId));

  BrowserWindowInterface* second_browser = web_app::LaunchWebAppBrowser(
      profile(), ash::kCalculatorAppId, WindowOpenDisposition::kNewWindow);
  EXPECT_EQ(app_browser, second_browser);
  EXPECT_EQ(browser_count,
            ProfileBrowserCollection::GetForProfile(profile())->GetSize());
}
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace web_app
