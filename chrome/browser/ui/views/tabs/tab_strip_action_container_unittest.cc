// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_strip_action_container.h"

#include <memory>

#include "base/check_deref.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/glic/test_support/glic_test_environment.h"
#include "chrome/browser/glic/test_support/glic_test_util.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/tabs/glic_actor_task_icon_controller.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/commerce/product_specifications_button.h"
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views.h"
#include "chrome/browser/ui/views/tabs/fake_base_tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/glic_button.h"
#include "chrome/browser/ui/views/tabs/tab_strip_nudge_button.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_web_contents_factory.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/animation/animation_test_api.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/test/glic_user_session_test_helper.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

static_assert(BUILDFLAG(ENABLE_GLIC));

namespace {
using testing::SizeIs;
}  // namespace

class FakeGlicTabStripController : public FakeBaseTabStripController {
 public:
  // `profile` must be non-null and must outlive `this`.
  explicit FakeGlicTabStripController(TestingProfile* profile)
      : profile_(CHECK_DEREF(profile)) {}

  Profile* GetProfile() const override {
    if (use_otr_profile_) {
      TestingProfile* otr_profile = TestingProfile::Builder().BuildOffTheRecord(
          &profile_.get(), Profile::OTRProfileID::CreateUniqueForTesting());

      return otr_profile;
    }

    return &profile_.get();
  }
  void Setup() {
    auto browser_window = std::make_unique<TestBrowserWindow>();
    Browser::CreateParams params(&profile_.get(), /*user_gesture*/ true);
    params.type = Browser::TYPE_NORMAL;
    params.window = browser_window.release();
    browser_ = Browser::DeprecatedCreateOwnedForTesting(params);
  }
  void ShouldUseOtrProfile(bool use_otr_profile) {
    use_otr_profile_ = use_otr_profile;
  }

  BrowserWindowInterface* GetBrowserWindowInterface() override {
    return browser_.get();
  }

  bool CanShowModalUI() const override { return true; }

 private:
  bool use_otr_profile_ = false;
  const raw_ref<TestingProfile> profile_;
  std::unique_ptr<Browser> browser_;
};

class TabStripActionContainerTest : public ChromeViewsTestBase,
                                    public ::testing::WithParamInterface<bool> {
 public:
  TabStripActionContainerTest()
      : animation_mode_reset_(gfx::AnimationTestApi::SetRichAnimationRenderMode(
            gfx::Animation::RichAnimationRenderMode::FORCE_ENABLED)) {
    std::vector<base::test::FeatureRefAndParams> enabled_features = {
        {features::kGlic, {}},
        {features::kTabstripComboButton, {}},
        {features::kGlicActor, {}},
        {features::kGlicActorUi,
         {{features::kGlicActorUiTaskIconName, "true"}}}};
    std::vector<base::test::FeatureRef> disabled_features;
    if (GetParam()) {
      enabled_features.push_back({features::kGlicActorUiNudgeRedesign, {}});
    } else {
      disabled_features.push_back(features::kGlicActorUiNudgeRedesign);
    }
    scoped_feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                                       disabled_features);
  }
  TabStripActionContainerTest(const TabStripActionContainerTest&) = delete;
  TabStripActionContainerTest& operator=(const TabStripActionContainerTest&) =
      delete;
  ~TabStripActionContainerTest() override = default;

  void SetUp() override {
    testing_profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(testing_profile_manager_->SetUp());
#if BUILDFLAG(IS_CHROMEOS)
    glic_user_session_test_helper_.PreProfileSetUp(
        testing_profile_manager_->profile_manager());
#endif  // BUILDFLAG(IS_CHROMEOS)
    TestingBrowserProcess::GetGlobal()->CreateGlobalFeaturesForTesting();
    ChromeViewsTestBase::SetUp();
    profile_ = testing_profile_manager_->CreateTestingProfile(
        TestingProfile::kDefaultProfileUserName);
    glic_test_environment_.SetupProfile(profile_.get());
    web_contents_ = content::WebContentsTester::CreateTestWebContents(
        profile_.get(), nullptr);
  }

  void TearDown() override {
    tab_strip_action_container_.reset();
    glic_nudge_controller_.reset();
    tab_declutter_controller_.reset();
    browser_window_interface_.reset();
    tab_interface_.reset();
    tab_strip_model_.reset();
    tab_strip_.reset();

    web_contents_.reset();
    profile_ = nullptr;

    ChromeViewsTestBase::TearDown();
    TestingBrowserProcess::GetGlobal()->GetFeatures()->Shutdown();
    testing_profile_manager_.reset();
#if BUILDFLAG(IS_CHROMEOS)
    glic_user_session_test_helper_.PostProfileTearDown();
#endif  // BUILDFLAG(IS_CHROMEOS)
  }

  void BuildGlicContainer(bool use_otr_profile) {
    auto controller =
        std::make_unique<FakeGlicTabStripController>(profile_.get());
    controller->Setup();
    controller->ShouldUseOtrProfile(use_otr_profile);

    tab_strip_ = std::make_unique<TabStrip>(std::move(controller));

    tab_strip_model_ = std::make_unique<TabStripModel>(
        &tab_strip_model_delegate_, tab_strip_->controller()->GetProfile());

    tab_interface_ = std::make_unique<tabs::MockTabInterface>();

    browser_window_interface_ = std::make_unique<MockBrowserWindowInterface>();
    ON_CALL(*browser_window_interface_, GetTabStripModel())
        .WillByDefault(::testing::Return(tab_strip_model_.get()));
    ON_CALL(*browser_window_interface_, GetProfile())
        .WillByDefault(
            ::testing::Return(tab_strip_->controller()->GetProfile()));
    ON_CALL(*browser_window_interface_, GetActiveTabInterface)
        .WillByDefault(::testing::Return(tab_interface_.get()));
    ON_CALL(*browser_window_interface_, CanShowCallToAction)
        .WillByDefault(::testing::Return(true));
    ON_CALL(*tab_interface_, GetContents)
        .WillByDefault(::testing::Return(web_contents_.get()));
    ON_CALL(*browser_window_interface_, RegisterActiveTabDidChange)
        .WillByDefault([this](auto callback) {
          SetActiveTabChangedCallback(callback);
          return base::CallbackListSubscription();
        });

    tab_declutter_controller_ = std::make_unique<tabs::TabDeclutterController>(
        browser_window_interface_.get());

    glic_nudge_controller_ = std::make_unique<tabs::GlicNudgeController>(
        browser_window_interface_.get());

    tab_strip_action_container_ = std::make_unique<TabStripActionContainer>(
        tab_strip_->controller(), tab_declutter_controller_.get(),
        glic_nudge_controller_.get());
  }

  static std::string GetParamName(const ::testing::TestParamInfo<bool>& info) {
    return info.param ? "NudgeRedesign" : "NoNudgeRedesign";
  }

  void SetActiveTabChangedCallback(
      base::RepeatingCallback<void(BrowserWindowInterface*)> cb) {
    active_tab_changed_callback_ = cb;
  }

 protected:
  glic::GlicUnitTestEnvironment glic_test_environment_;
  std::unique_ptr<TestingProfileManager> testing_profile_manager_;
  std::unique_ptr<TabStrip> tab_strip_;
  std::unique_ptr<TabStripModel> tab_strip_model_;
  std::unique_ptr<tabs::TabDeclutterController> tab_declutter_controller_;
  std::unique_ptr<tabs::GlicNudgeController> glic_nudge_controller_;
  std::unique_ptr<tabs::MockTabInterface> tab_interface_;
  std::unique_ptr<MockBrowserWindowInterface> browser_window_interface_;
  TestTabStripModelDelegate tab_strip_model_delegate_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<views::View> locked_expansion_view_;
  std::unique_ptr<TabStripActionContainer> tab_strip_action_container_;

  content::WebContents* web_contents() { return web_contents_.get(); }

  void SimulateActiveTabChanged() {
    active_tab_changed_callback_.Run(browser_window_interface_.get());
  }

 private:
  // Owned by TabStrip.

  content::RenderViewHostTestEnabler render_view_host_test_enabler_;
#if BUILDFLAG(IS_CHROMEOS)
  ash::GlicUserSessionTestHelper glic_user_session_test_helper_;
#endif  // BUILDFLAG(IS_CHROMEOS)
  raw_ptr<TestingProfile> profile_ = nullptr;
  std::unique_ptr<content::WebContents> web_contents_;
  gfx::AnimationTestApi::RenderModeResetter animation_mode_reset_;
  base::RepeatingCallback<void(BrowserWindowInterface*)>
      active_tab_changed_callback_;
};

INSTANTIATE_TEST_SUITE_P(/* no prefix */,
                         TabStripActionContainerTest,
                         ::testing::Bool(),
                         &TabStripActionContainerTest::GetParamName);

TEST_P(TabStripActionContainerTest, GlicButtonDrawing) {
  BuildGlicContainer(/*use_otr_profile=*/false);
  EXPECT_TRUE(tab_strip_action_container_->GetGlicButton());
}

TEST_P(TabStripActionContainerTest, GlicButtonUnsupportedProfile) {
  BuildGlicContainer(/*use_otr_profile=*/true);
  EXPECT_FALSE(tab_strip_action_container_->GetGlicButton());
}

TEST_P(TabStripActionContainerTest, OrdersButtonsCorrectlyAtConstruction) {
  BuildGlicContainer(/*use_otr_profile=*/false);
  ASSERT_EQ(tab_strip_action_container_->tab_declutter_button(),
            tab_strip_action_container_->children()[0]);

  ASSERT_EQ(tab_strip_action_container_->auto_tab_group_button(),
            tab_strip_action_container_->children()[1]);

// TODO(crbug.com/437141881): Fix flaky tests on Mac.
// Mac doesn't have a separator, so the children sizes are different.
#if !BUILDFLAG(IS_MAC)
  ASSERT_THAT(tab_strip_action_container_->children(), SizeIs(5));

  ASSERT_EQ(tab_strip_action_container_->glic_actor_button_container(),
            tab_strip_action_container_->children()[2]);

  ASSERT_THAT(
      tab_strip_action_container_->glic_actor_button_container()->children(),
      SizeIs(1));
  ASSERT_EQ(tab_strip_action_container_->glic_actor_task_icon(),
            tab_strip_action_container_->glic_actor_button_container()
                ->children()[0]);

  ASSERT_EQ(tab_strip_action_container_->GetGlicButton(),
            tab_strip_action_container_->children()[3]);
#endif  // !BUILDFLAG(IS_MAC)
}

TEST_P(TabStripActionContainerTest, OrdersButtonsCorrectlyWhenShown) {
  BuildGlicContainer(/*use_otr_profile=*/false);

// TODO(crbug.com/437141881): Fix flaky tests on Mac.
// Mac doesn't have a separator, so the children sizes are different.
#if !BUILDFLAG(IS_MAC)

  // Before `ShowGlicActorTaskIcon()` is called, `glic_button` is a direct
  // child `tab_strip_action_container_`. When `ShowGlicActorTaskIcon()` is
  // called, `glic_button` is moved from being a direct child of
  // `tab_strip_action_container_` to being a child of
  // `glic_actor_button_container()`.
  tab_strip_action_container_->ShowGlicActorTaskIcon();
  ASSERT_THAT(tab_strip_action_container_->children(), SizeIs(4));

  ASSERT_EQ(tab_strip_action_container_->glic_actor_button_container(),
            tab_strip_action_container_->children()[2]);

  ASSERT_THAT(
      tab_strip_action_container_->glic_actor_button_container()->children(),
      SizeIs(2));

  const bool nudge_redesign = GetParam();
  // With redesign, the GlicButton is to the left of the GlicActorTaskIcon.
  if (nudge_redesign) {
    ASSERT_EQ(tab_strip_action_container_->GetGlicButton(),
              tab_strip_action_container_->glic_actor_button_container()
                  ->children()[0]);
    ASSERT_EQ(tab_strip_action_container_->glic_actor_task_icon(),
              tab_strip_action_container_->glic_actor_button_container()
                  ->children()[1]);
  } else {
    ASSERT_EQ(tab_strip_action_container_->glic_actor_task_icon(),
              tab_strip_action_container_->glic_actor_button_container()
                  ->children()[0]);
    ASSERT_EQ(tab_strip_action_container_->GetGlicButton(),
              tab_strip_action_container_->glic_actor_button_container()
                  ->children()[1]);
  }
#endif  // !BUILDFLAG(IS_MAC)
}

TEST_P(TabStripActionContainerTest, GlicButtonUpdateLabel) {
  BuildGlicContainer(/*use_otr_profile=*/false);
  glic_nudge_controller_->UpdateNudgeLabel(
      web_contents(), "TEST", /*prompt_suggestion=*/std::nullopt,
      /*activity=*/std::nullopt, base::NullCallback());
  ASSERT_EQ(tab_strip_action_container_->GetGlicButton()->GetText(), u"TEST");
}

TEST_P(TabStripActionContainerTest, GlicButtonHideNudgeOnTabChange) {
  BuildGlicContainer(/*use_otr_profile=*/false);
  glic_nudge_controller_->SetDelegate(tab_strip_action_container_.get());

  ASSERT_FALSE(tab_strip_action_container_->GetIsShowingGlicNudge());

  glic_nudge_controller_->UpdateNudgeLabel(
      web_contents(), "TEST", /*prompt_suggestion=*/std::nullopt,
      /*activity=*/std::nullopt, base::NullCallback());
  ASSERT_TRUE(tab_strip_action_container_->GetIsShowingGlicNudge());
  ASSERT_EQ(tab_strip_action_container_->GetGlicButton()->GetText(), u"TEST");

  SimulateActiveTabChanged();
  ASSERT_FALSE(tab_strip_action_container_->GetIsShowingGlicNudge());
  ASSERT_EQ(tab_strip_action_container_->GetGlicButton()->GetText(), u"Gemini");
}

class TabStripActionContainerTestWithProduct
    : public TabStripActionContainerTest {
 public:
  TabStripActionContainerTestWithProduct() {
    scoped_feature_list_.InitAndEnableFeature(commerce::kProductSpecifications);
  }
  ~TabStripActionContainerTestWithProduct() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(/* no prefix */,
                         TabStripActionContainerTestWithProduct,
                         ::testing::Bool(),
                         &TabStripActionContainerTest::GetParamName);

TEST_P(TabStripActionContainerTestWithProduct, OrdersButtonsCorrectly) {
  BuildGlicContainer(/*use_otr_profile=*/false);

  ASSERT_EQ(tab_strip_action_container_->tab_declutter_button(),
            tab_strip_action_container_->children()[0]);

  ASSERT_EQ(tab_strip_action_container_->auto_tab_group_button(),
            tab_strip_action_container_->children()[1]);

  ASSERT_EQ(tab_strip_action_container_->GetProductSpecificationsButton(),
            tab_strip_action_container_->children()[2]);

// TODO(crbug.com/437141881): Fix flaky tests on Mac.
// Mac doesn't have a separator, so the children sizes are different.
#if !BUILDFLAG(IS_MAC)
  ASSERT_THAT(tab_strip_action_container_->children(), SizeIs(6));

  ASSERT_EQ(tab_strip_action_container_->glic_actor_button_container(),
            tab_strip_action_container_->children()[3]);

  ASSERT_THAT(
      tab_strip_action_container_->glic_actor_button_container()->children(),
      SizeIs(1));
  ASSERT_EQ(tab_strip_action_container_->glic_actor_task_icon(),
            tab_strip_action_container_->glic_actor_button_container()
                ->children()[0]);

  ASSERT_EQ(tab_strip_action_container_->GetGlicButton(),
            tab_strip_action_container_->children()[4]);
#endif  // !BUILDFLAG(IS_MAC)
}
