// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_glic_container.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
#include "chrome/browser/ui/views/tabs/fake_base_tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/glic_button.h"
#include "chrome/browser/ui/views/tabs/tab_strip_nudge_button.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/animation/animation_test_api.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/widget/widget.h"

class FakeBaseTabStripControllerWithProfile
    : public FakeBaseTabStripController {
 public:
  Profile* GetProfile() const override { return profile_.get(); }

 private:
  std::unique_ptr<TestingProfile> profile_ = std::make_unique<TestingProfile>();
};

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
    tab_strip_ = std::make_unique<TabStrip>(
        std::make_unique<FakeBaseTabStripControllerWithProfile>());

    tab_strip_model_ = std::make_unique<TabStripModel>(
        &tab_strip_model_delegate_, tab_strip_->controller()->GetProfile());

    browser_window_interface_ = std::make_unique<MockBrowserWindowInterface>();
    ON_CALL(*browser_window_interface_, GetTabStripModel)
        .WillByDefault(::testing::Return(tab_strip_model_.get()));
    ON_CALL(*browser_window_interface_, GetProfile)
        .WillByDefault(
            ::testing::Return(tab_strip_->controller()->GetProfile()));

    tab_declutter_controller_ = std::make_unique<tabs::TabDeclutterController>(
        browser_window_interface_.get());
    tab_glic_container_ = std::make_unique<TabGlicContainer>(
        tab_strip_->controller(), tab_declutter_controller_.get());
  }

 protected:
  std::unique_ptr<TabStrip> tab_strip_;
  std::unique_ptr<TabStripModel> tab_strip_model_;
  std::unique_ptr<TabGlicContainer> tab_glic_container_ = nullptr;
  std::unique_ptr<tabs::TabDeclutterController> tab_declutter_controller_;
  std::unique_ptr<MockBrowserWindowInterface> browser_window_interface_;
  TestTabStripModelDelegate tab_strip_model_delegate_;
  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  // Owned by TabStrip.
  gfx::AnimationTestApi::RenderModeResetter animation_mode_reset_;
};

#if BUILDFLAG(ENABLE_GLIC)
TEST_F(TabGlicContainerTest, GlicButtonDrawing) {
  EXPECT_NE(tab_glic_container_->GetGlicButton(), nullptr);
}

#endif  // BUILDFLAG(ENABLE_GLIC)

TEST_F(TabGlicContainerTest, OrdersButtonsCorrectly) {
  ASSERT_EQ(tab_glic_container_->tab_declutter_button(),
            tab_glic_container_->children()[0]);

#if BUILDFLAG(ENABLE_GLIC)
  ASSERT_EQ(tab_glic_container_->GetGlicButton(),
            tab_glic_container_->children()[1]);
#endif  // BUILDFLAG(ENABLE_GLIC)
}
