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
#include "chrome/common/chrome_features.h"
#include "content/public/browser/render_widget_host_view.h"
#include "third_party/skia/include/core/SkColor.h"
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

}  // namespace
}  // namespace waap
