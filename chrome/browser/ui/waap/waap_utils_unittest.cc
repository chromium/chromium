// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/waap/waap_utils.h"

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/task_manager/providers/web_contents/web_contents_tag.h"
#include "chrome/browser/ui/prefs/prefs_tab_helper.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/test_support/test_harness_helper.h"
#include "content/public/browser/security_principal.h"
#include "content/public/common/content_features.h"
#include "content/public/common/url_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/url_constants.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/flags/android/chrome_feature_list.h"
#else
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_webui_profile.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_webui_user_profile.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_provider_key.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace waap {
namespace {

#if BUILDFLAG(IS_ANDROID)
constexpr const char* kInitialWebUIHost = chrome::kChromeUITabStripNtbHost;
#else
constexpr const char* kInitialWebUIHost = chrome::kChromeUIWebUIToolbarHost;
#endif

void InitInitialWebUIFeatures(base::test::ScopedFeatureList& feature_list,
                              bool enabled) {
#if BUILDFLAG(IS_ANDROID)
  const base::Feature& platform_feature =
      chrome::android::kAndroidNewTabButtonTabstripWebUI;
#else
  const base::Feature& platform_feature = features::kWebUIReloadButton;
#endif
  if (enabled) {
    feature_list.InitWithFeatures({features::kInitialWebUI, platform_feature},
                                  {});
  } else {
    feature_list.InitWithFeatures({},
                                  {features::kInitialWebUI, platform_feature});
  }
}

using IsForInitialWebUITest = ChromeRenderViewHostTestHarness;

TEST_F(IsForInitialWebUITest, FeaturesDisabled) {
  base::test::ScopedFeatureList feature_list;
  InitInitialWebUIFeatures(feature_list, /*enabled=*/false);

  const GURL url(std::string(content::kChromeUIScheme) + "://" +
                 kInitialWebUIHost);
  EXPECT_FALSE(IsForInitialWebUI(url));
  std::unique_ptr<content::SecurityPrincipal> principal =
      content::SecurityPrincipal::CreateForTesting(profile(), url);
  EXPECT_FALSE(IsForInitialWebUI(*principal));
}

TEST_F(IsForInitialWebUITest, FeaturesEnabled) {
  base::test::ScopedFeatureList feature_list;
  InitInitialWebUIFeatures(feature_list, /*enabled=*/true);

  const GURL url(std::string(content::kChromeUIScheme) + "://" +
                 kInitialWebUIHost);
  EXPECT_TRUE(IsForInitialWebUI(url));
  std::unique_ptr<content::SecurityPrincipal> principal =
      content::SecurityPrincipal::CreateForTesting(profile(), url);
  EXPECT_TRUE(IsForInitialWebUI(*principal));
}

TEST_F(IsForInitialWebUITest, NonChromeScheme) {
  base::test::ScopedFeatureList feature_list;
  InitInitialWebUIFeatures(feature_list, /*enabled=*/true);

  const GURL url(std::string("https") + "://" + kInitialWebUIHost);
  EXPECT_FALSE(IsForInitialWebUI(url));
  std::unique_ptr<content::SecurityPrincipal> principal =
      content::SecurityPrincipal::CreateForTesting(profile(), url);
  EXPECT_FALSE(IsForInitialWebUI(*principal));
}

TEST_F(IsForInitialWebUITest, NonInitialWebUIHost) {
  base::test::ScopedFeatureList feature_list;
  InitInitialWebUIFeatures(feature_list, /*enabled=*/true);

  const GURL url(std::string(content::kChromeUIScheme) + "://" + "wrong-host");
  EXPECT_FALSE(IsForInitialWebUI(url));
  std::unique_ptr<content::SecurityPrincipal> principal =
      content::SecurityPrincipal::CreateForTesting(profile(), url);
  EXPECT_FALSE(IsForInitialWebUI(*principal));
}

class PrewarmHelperTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    pm_harness_.SetUp();
    ChromeRenderViewHostTestHarness::SetUp();
  }

  void TearDown() override {
    DeleteContents();
    pm_harness_.TearDown();
    ChromeRenderViewHostTestHarness::TearDown();
  }

 private:
  performance_manager::PerformanceManagerTestHarnessHelper pm_harness_;
};

TEST_F(PrewarmHelperTest, ConfigureWebUIContents) {
  PrewarmHelper::ConfigureWebUIContents(web_contents(), profile(),
                                        IDS_TASK_MANAGER_WEBUI_TOOLBAR);
  NavigateAndCommit(GURL(url::kAboutBlankURL));

  EXPECT_NE(nullptr, PrefsTabHelper::FromWebContents(web_contents()));
  EXPECT_NE(nullptr,
            task_manager::WebContentsTag::FromWebContents(web_contents()));
#if !BUILDFLAG(IS_ANDROID)
  EXPECT_EQ(SK_ColorTRANSPARENT,
            web_contents()->GetRenderWidgetHostView()->GetBackgroundColor());
#endif  // !BUILDFLAG(IS_ANDROID)

  base::WeakPtr<performance_manager::PageNode> page_node =
      performance_manager::PerformanceManager::GetPrimaryPageNodeForWebContents(
          web_contents());
  ASSERT_TRUE(page_node);
  EXPECT_EQ(performance_manager::PageType::kNonTabWebUI, page_node->GetType());
}

#if !BUILDFLAG(IS_ANDROID)
class ColorProviderChangeCounter : public content::WebContentsObserver {
 public:
  explicit ColorProviderChangeCounter(content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents) {}

  void OnColorProviderChanged() override { ++count_; }
  int count() const { return count_; }

 private:
  int count_ = 0;
};

TEST_F(PrewarmHelperTest,
       BindUserProfileRecomputesWebPreferencesAndSkipsMatchingColorSource) {
  base::test::ScopedFeatureList feature_list{
      features::kTopChromeWebUISharedRenderer};
  webui::ScopedTopChromeWebUIProfileForTesting scoped_top_chrome_profile;
  profile()->GetPrefs()->SetBoolean(prefs::kWebKitDomPasteEnabled, true);

  // Seeded from `profile()`: binding `profile()` skips switching the color
  // provider source while still recomputing WebPreferences once, and runs the
  // seed callback before navigation.
  bool seed_called = false;
  std::unique_ptr<content::WebContents> matching =
      PrewarmHelper::PrewarmUnboundWebUIContents(
          *profile(), [&](content::WebContents& contents) {
            EXPECT_FALSE(webui::GetUserProfile(&contents));
            EXPECT_TRUE(contents.GetController().IsInitialNavigation());
            seed_called = true;
          });
  ASSERT_TRUE(matching);
  EXPECT_TRUE(seed_called);
  EXPECT_FALSE(matching->GetOrCreateWebPreferences().dom_paste_enabled);
  ColorProviderChangeCounter matching_counter(matching.get());
  EXPECT_FALSE(PrewarmHelper::BindUserProfile(*matching, *profile()));
  EXPECT_EQ(0, matching_counter.count());
  EXPECT_EQ(profile(), webui::GetUserProfile(matching.get()));
  EXPECT_TRUE(PrefsTabHelper::FromWebContents(matching.get()));
  EXPECT_TRUE(matching->GetOrCreateWebPreferences().dom_paste_enabled);

  // When the bound profile's color provider key differs from the seed,
  // `BindUserProfile` updates the color provider source and returns true.
  std::unique_ptr<content::WebContents> differing =
      PrewarmHelper::PrewarmUnboundWebUIContents(*profile(),
                                                 [](content::WebContents&) {});
  ASSERT_TRUE(differing);
  const ui::ColorProviderKey seeded_key =
      PrewarmHelper::GetColorProviderKey(profile());
  ThemeServiceFactory::GetForProfile(profile())->SetBrowserColorScheme(
      seeded_key.color_mode == ui::ColorProviderKey::ColorMode::kDark
          ? ThemeService::BrowserColorScheme::kLight
          : ThemeService::BrowserColorScheme::kDark);
  ColorProviderChangeCounter differing_counter(differing.get());
  EXPECT_TRUE(PrewarmHelper::BindUserProfile(*differing, *profile()));
  EXPECT_EQ(1, differing_counter.count());
}
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace
}  // namespace waap
