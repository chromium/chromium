// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/waap/waap_utils.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/task_manager/providers/web_contents/web_contents_tag.h"
#include "chrome/browser/ui/prefs/prefs_tab_helper.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/test_support/test_harness_helper.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/common/content_features.h"
#include "content/public/common/url_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "url/gurl.h"

namespace waap {
namespace {

TEST(IsForInitialWebUITest, FeaturesDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {}, {features::kInitialWebUI, features::kWebUIReloadButton});

  EXPECT_FALSE(
      IsForInitialWebUI(GURL(std::string(content::kChromeUIScheme) + "://" +
                             chrome::kChromeUIWebUIToolbarHost)));
}

TEST(IsForInitialWebUITest, FeaturesEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {features::kInitialWebUI, features::kWebUIReloadButton}, {});

  EXPECT_TRUE(
      IsForInitialWebUI(GURL(std::string(content::kChromeUIScheme) + "://" +
                             chrome::kChromeUIWebUIToolbarHost)));
}

TEST(IsForInitialWebUITest, NonChromeScheme) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {features::kInitialWebUI, features::kWebUIReloadButton}, {});

  EXPECT_FALSE(IsForInitialWebUI(
      GURL(std::string("https") + "://" + chrome::kChromeUIWebUIToolbarHost)));
}

TEST(IsForInitialWebUITest, NonInitialWebUIHost) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {features::kInitialWebUI, features::kWebUIReloadButton}, {});

  EXPECT_FALSE(IsForInitialWebUI(
      GURL(std::string(content::kChromeUIScheme) + "://" + "wrong-host")));
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
  PrewarmHelper::ConfigureWebUIContents(web_contents(), profile());
  NavigateAndCommit(GURL(url::kAboutBlankURL));

  EXPECT_NE(nullptr, PrefsTabHelper::FromWebContents(web_contents()));
  EXPECT_NE(nullptr,
            task_manager::WebContentsTag::FromWebContents(web_contents()));
  EXPECT_EQ(SK_ColorTRANSPARENT,
            web_contents()->GetRenderWidgetHostView()->GetBackgroundColor());

  base::WeakPtr<performance_manager::PageNode> page_node =
      performance_manager::PerformanceManager::GetPrimaryPageNodeForWebContents(
          web_contents());
  ASSERT_TRUE(page_node);
  EXPECT_EQ(performance_manager::PageType::kNonTabWebUI, page_node->GetType());
}

}  // namespace
}  // namespace waap
