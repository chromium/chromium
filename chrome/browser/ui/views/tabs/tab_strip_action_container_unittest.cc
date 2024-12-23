// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_strip_action_container.h"

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

class FakeGlicTabStripController : public FakeBaseTabStripController {
 public:
  Profile* GetProfile() const override {
    if (use_otr_profile_) {
      TestingProfile* otr_profile = TestingProfile::Builder().BuildOffTheRecord(
          profile_.get(), Profile::OTRProfileID::CreateUniqueForTesting());

      return otr_profile;
    }

    return profile_.get();
  }

  void ShouldUseOtrProfile(bool use_otr_profile) {
    use_otr_profile_ = use_otr_profile;
  }

 private:
  bool use_otr_profile_ = false;
  std::unique_ptr<TestingProfile> profile_ = std::make_unique<TestingProfile>();
};

class TabStripActionContainerTest : public ChromeViewsTestBase {
 public:
  TabStripActionContainerTest()
      : animation_mode_reset_(gfx::AnimationTestApi::SetRichAnimationRenderMode(
            gfx::Animation::RichAnimationRenderMode::FORCE_ENABLED)) {}
  TabStripActionContainerTest(const TabStripActionContainerTest&) = delete;
  TabStripActionContainerTest& operator=(const TabStripActionContainerTest&) =
      delete;
  ~TabStripActionContainerTest() override = default;

  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    scoped_feature_list_.InitWithFeatures(
        {features::kGlic, features::kTabstripComboButton}, {});
  }

  void TearDown() override {
    ChromeViewsTestBase::TearDown();
    tab_strip_action_container_.reset();
  }

  void BuildGlicContainer(bool use_otr_profile) {
    controller_ = std::make_unique<FakeGlicTabStripController>();
    controller_->ShouldUseOtrProfile(use_otr_profile);

    tab_strip_ = std::make_unique<TabStrip>(std::move(controller_));

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

    locked_expansion_view_ = std::make_unique<views::View>();

    tab_strip_action_container_ = std::make_unique<TabStripActionContainer>(
        tab_strip_->controller(), locked_expansion_view_.get(),
        tab_declutter_controller_.get());
  }

 protected:
  std::unique_ptr<TabStrip> tab_strip_;
  std::unique_ptr<TabStripModel> tab_strip_model_;
  std::unique_ptr<tabs::TabDeclutterController> tab_declutter_controller_;
  std::unique_ptr<MockBrowserWindowInterface> browser_window_interface_;
  TestTabStripModelDelegate tab_strip_model_delegate_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<views::View> locked_expansion_view_;
  std::unique_ptr<FakeGlicTabStripController> controller_;
  std::unique_ptr<TabStripActionContainer> tab_strip_action_container_;

 private:
  // Owned by TabStrip.
  gfx::AnimationTestApi::RenderModeResetter animation_mode_reset_;
};

#if BUILDFLAG(ENABLE_GLIC)
TEST_F(TabStripActionContainerTest, GlicButtonDrawing) {
  BuildGlicContainer(/*use_otr_profile=*/false);
  EXPECT_TRUE(tab_strip_action_container_->GetGlicButton());
}

TEST_F(TabStripActionContainerTest, GlicButtonUnsupportedProfile) {
  BuildGlicContainer(/*use_otr_profile=*/true);
  EXPECT_FALSE(tab_strip_action_container_->GetGlicButton());
}

#endif  // BUILDFLAG(ENABLE_GLIC)

TEST_F(TabStripActionContainerTest, OrdersButtonsCorrectly) {
  BuildGlicContainer(/*use_otr_profile=*/false);
  ASSERT_EQ(tab_strip_action_container_->tab_declutter_button(),
            tab_strip_action_container_->children()[0]);

  ASSERT_EQ(tab_strip_action_container_->auto_tab_group_button(),
            tab_strip_action_container_->children()[1]);

#if BUILDFLAG(ENABLE_GLIC)
  ASSERT_EQ(tab_strip_action_container_->GetGlicButton(),
            tab_strip_action_container_->children()[2]);
#endif  // BUILDFLAG(ENABLE_GLIC)
}
