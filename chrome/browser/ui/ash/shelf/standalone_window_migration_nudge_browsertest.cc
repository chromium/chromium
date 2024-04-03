// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/system/anchored_nudge_data.h"
#include "ash/shell.h"
#include "ash/system/toast/anchored_nudge_manager_impl.h"
#include "chrome/browser/ui/ash/shelf/browser_status_monitor.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/ash/components/standalone_browser/standalone_browser_features.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/test/browser_test.h"

namespace {
constexpr char kNudgeId[] = "migration_ux_nudge_id";
}

class StandaloneWindowMigrationNudgeBrowserTest : public InProcessBrowserTest {
 protected:
  StandaloneWindowMigrationNudgeBrowserTest() {
    std::vector<base::test::FeatureRef> enabled_features = {
        chromeos::features::kCrosShortstand,
        ash::features::kStandaloneWindowMigrationUx};
#if BUILDFLAG(IS_CHROMEOS_ASH)
    scoped_feature_list_.InitWithFeatures(
        enabled_features, {ash::standalone_browser::features::kLacrosOnly});
#else
    enabled_features
        .insert(ash::standalone_browser::features::kLacrosOnly)
            scoped_feature_list_.InitWithFeatures(enabled_features, {});
#endif
  }

  void SetUpOnMainThread() override { ASSERT_TRUE(controller()); }

  webapps::AppId CreateWebApp(const GURL& app_url,
                              const std::u16string& app_name,
                              web_app::mojom::UserDisplayMode display_mode) {
    auto web_app_info = std::make_unique<web_app::WebAppInstallInfo>();
    web_app_info->start_url = app_url;
    web_app_info->title = app_name;
    web_app_info->user_display_mode = display_mode;

    return web_app::test::InstallWebApp(browser()->profile(),
                                        std::move(web_app_info));
  }

  ChromeShelfController* controller() {
    return ChromeShelfController::instance();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// TODO(https://crbug.com/332642058): Test is flaky on Linux Chromium OS ASan
// LSan Tests.
#if BUILDFLAG(IS_LINUX) && defined(ADDRESS_SANITIZER)
#define MAYBE_CheckNudge DISABLED_CheckNudge
#else
#define MAYBE_CheckNudge CheckNudge
#endif
IN_PROC_BROWSER_TEST_F(StandaloneWindowMigrationNudgeBrowserTest,
                       MAYBE_CheckNudge) {
  GURL app_url = GURL("https://example.org/");
  std::u16string app_name = u"app_name";

  auto app_id = CreateWebApp(app_url, app_name,
                             web_app::mojom::UserDisplayMode::kBrowser);

  Browser* app_browser =
      web_app::LaunchWebAppBrowserAndWait(browser()->profile(), app_id);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(app_browser, app_url));

  ash::AnchoredNudgeManager* anchored_nudge_manager =
      ash::AnchoredNudgeManager::Get();
  ASSERT_TRUE(anchored_nudge_manager);

  EXPECT_TRUE(anchored_nudge_manager->IsNudgeShown(kNudgeId));
}

IN_PROC_BROWSER_TEST_F(StandaloneWindowMigrationNudgeBrowserTest,
                       CheckNudgeDoesNotShow) {
  GURL app_url = GURL("https://example.org/");
  std::u16string app_name = u"app_name";

  auto app_id = CreateWebApp(app_url, app_name,
                             web_app::mojom::UserDisplayMode::kStandalone);

  Browser* app_browser =
      web_app::LaunchWebAppBrowserAndWait(browser()->profile(), app_id);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(app_browser, app_url));

  ash::AnchoredNudgeManager* anchored_nudge_manager =
      ash::AnchoredNudgeManager::Get();
  ASSERT_TRUE(anchored_nudge_manager);

  EXPECT_FALSE(anchored_nudge_manager->IsNudgeShown(kNudgeId));
}
