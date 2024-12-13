// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_glic_container.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
#include "chrome/browser/ui/views/tabs/fake_base_tab_strip_controller.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "ui/gfx/animation/animation_test_api.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/widget/widget.h"

class TabGlicContainerTest : public ChromeViewsTestBase {
 public:
  TabGlicContainerTest()
      : animation_mode_reset_(gfx::AnimationTestApi::SetRichAnimationRenderMode(
            gfx::Animation::RichAnimationRenderMode::FORCE_ENABLED)) {}
  TabGlicContainerTest(const TabGlicContainerTest&) = delete;
  TabGlicContainerTest& operator=(const TabGlicContainerTest&) = delete;
  ~TabGlicContainerTest() override = default;

  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    scoped_feature_list_.InitWithFeatures(
        {features::kGlic, features::kTabstripComboButton}, {});
    BuildGlicContainer();
  }

  void TearDown() override {
    ChromeViewsTestBase::TearDown();
    tab_glic_container_.reset();
  }
  void BuildGlicContainer() {
    auto controller = std::make_unique<FakeBaseTabStripController>();
    controller_ = controller.get();
    tab_glic_container_ = std::make_unique<TabGlicContainer>(
        std::unique_ptr<TabStripController>(controller.release()).get());
  }

 protected:
  std::unique_ptr<TabGlicContainer> tab_glic_container_ = nullptr;

 private:
  // Owned by TabStrip.
  raw_ptr<FakeBaseTabStripController, DanglingUntriaged> controller_ = nullptr;
  raw_ptr<TabStrip, DanglingUntriaged> tab_strip_ = nullptr;
  std::unique_ptr<views::Widget> widget_;
  base::test::ScopedFeatureList scoped_feature_list_;
  gfx::AnimationTestApi::RenderModeResetter animation_mode_reset_;
};

#if BUILDFLAG(ENABLE_GLIC)
TEST_F(TabGlicContainerTest, GlicButtonDrawing) {
  EXPECT_NE(tab_glic_container_->GetGlicButton(), nullptr);
}
#endif  // BUILDFLAG(ENABLE_GLIC)
