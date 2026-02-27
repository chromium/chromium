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
#include "chrome/browser/ui/call_to_action/call_to_action_lock.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views.h"
#include "chrome/browser/ui/views/tabs/fake_base_tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/glic/tab_strip_glic_button.h"
#include "chrome/browser/ui/views/tabs/tab_hover_card_controller.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
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
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"
#include "ui/gfx/animation/animation_test_api.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/test/glic_user_session_test_helper.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

// TODO(crbug.com/461140208): Re-enable failing tests on ChromeOS.
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE(test_name) DISABLED_##test_name
#else
#define MAYBE(test_name) test_name
#endif

using ::testing::NiceMock;

namespace {
using testing::SizeIs;
}  // namespace

class FakeGlicTabStripController : public FakeBaseTabStripController {
 public:
  // `profile` must be non-null and must outlive `this`.
  explicit FakeGlicTabStripController(bool use_otr_profile,
                                      TestingProfile* profile)
      : profile_(CHECK_DEREF(profile)) {
    auto browser_window = std::make_unique<TestBrowserWindow>();
    Browser::CreateParams params(GetProfile(use_otr_profile),
                                 /*user_gesture*/ true);
    params.type = Browser::TYPE_NORMAL;
    params.window = browser_window.release();
    browser_ = Browser::DeprecatedCreateOwnedForTesting(params);
  }

  BrowserWindowInterface* GetBrowserWindowInterface() override {
    return browser_.get();
  }

 private:
  Profile* GetProfile(bool use_otr_profile) const {
    if (use_otr_profile) {
      TestingProfile* otr_profile = TestingProfile::Builder().BuildOffTheRecord(
          &profile_.get(), Profile::OTRProfileID::CreateUniqueForTesting());

      return otr_profile;
    }

    return &profile_.get();
  }

 private:
  const raw_ref<TestingProfile> profile_;
  std::unique_ptr<Browser> browser_;
};

class TabStripActionContainerTest : public ChromeViewsTestBase {
 public:
  TabStripActionContainerTest()
      : animation_mode_reset_(gfx::AnimationTestApi::SetRichAnimationRenderMode(
            gfx::Animation::RichAnimationRenderMode::FORCE_ENABLED)) {
    std::vector<base::test::FeatureRefAndParams> enabled_features = {
        {features::kGlic, {}},
        {features::kGlicActor, {}},
        {features::kGlicActorUi,
         {{features::kGlicActorUiTaskIconName, "true"}}}};
    std::vector<base::test::FeatureRef> disabled_features;
    scoped_feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                                       disabled_features);
  }
  TabStripActionContainerTest(const TabStripActionContainerTest&) = delete;
  TabStripActionContainerTest& operator=(const TabStripActionContainerTest&) =
      delete;
  ~TabStripActionContainerTest() override = default;

  void SetUp() override {
    raw_ptr<TestingProfileManager> testing_profile_manager =
        TestingBrowserProcess::GetGlobal()->SetUpGlobalFeaturesForTesting(
            /*profile_manager=*/true);
#if BUILDFLAG(IS_CHROMEOS)
    glic_user_session_test_helper_.PreProfileSetUp(
        testing_profile_manager->profile_manager());
#endif  // BUILDFLAG(IS_CHROMEOS)
    ChromeViewsTestBase::SetUp();
    profile_ = testing_profile_manager->CreateTestingProfile(
        TestingProfile::kDefaultProfileUserName);
    glic_test_environment_.SetupProfile(profile_.get());
    web_contents_ = content::WebContentsTester::CreateTestWebContents(
        profile_.get(), nullptr);
  }

  void TearDown() override {
    tab_strip_action_container_.reset();
    glic_nudge_controller_.reset();
    browser_window_interface_.reset();
    tab_interface_.reset();
    tab_strip_model_.reset();
    tab_strip_.reset();

    web_contents_.reset();
    profile_ = nullptr;

    ChromeViewsTestBase::TearDown();

    TestingBrowserProcess::GetGlobal()->TearDownGlobalFeaturesForTesting();
#if BUILDFLAG(IS_CHROMEOS)
    glic_user_session_test_helper_.PostProfileTearDown();
#endif  // BUILDFLAG(IS_CHROMEOS)
  }

  void BuildGlicContainer(bool use_otr_profile) {
    auto controller = std::make_unique<FakeGlicTabStripController>(
        use_otr_profile, profile_.get());

    tab_strip_ = std::make_unique<TabStrip>(
        std::move(controller),
        std::unique_ptr<NiceMock<TabHoverCardController>>());

    tab_strip_model_ = std::make_unique<TabStripModel>(
        &tab_strip_model_delegate_,
        tab_strip_->GetBrowserWindowInterface()->GetProfile());

    tab_interface_ = std::make_unique<tabs::MockTabInterface>();

    browser_window_interface_ = std::make_unique<MockBrowserWindowInterface>();
    ON_CALL(*browser_window_interface_, GetTabStripModel())
        .WillByDefault(::testing::Return(tab_strip_model_.get()));
    ON_CALL(*browser_window_interface_, GetProfile())
        .WillByDefault(::testing::Return(
            tab_strip_->GetBrowserWindowInterface()->GetProfile()));
    ON_CALL(*browser_window_interface_, GetActiveTabInterface)
        .WillByDefault(::testing::Return(tab_interface_.get()));
    ON_CALL(*browser_window_interface_, GetUnownedUserDataHost())
        .WillByDefault(testing::ReturnRef(data_host_));

    call_to_action_ =
        std::make_unique<CallToActionLock>(browser_window_interface_.get());

    ON_CALL(*tab_interface_, GetContents)
        .WillByDefault(::testing::Return(web_contents_.get()));
    ON_CALL(*browser_window_interface_, RegisterActiveTabDidChange)
        .WillByDefault([this](auto callback) {
          SetActiveTabChangedCallback(callback);
          return base::CallbackListSubscription();
        });

    glic_nudge_controller_ = std::make_unique<tabs::GlicNudgeController>(
        browser_window_interface_.get());

    tab_strip_action_container_ = std::make_unique<TabStripActionContainer>(
        tab_strip_->GetBrowserWindowInterface(), glic_nudge_controller_.get());
  }

  void SetActiveTabChangedCallback(
      base::RepeatingCallback<void(BrowserWindowInterface*)> cb) {
    active_tab_changed_callback_ = cb;
  }

 protected:
  glic::GlicUnitTestEnvironment glic_test_environment_;
  std::unique_ptr<TabStrip> tab_strip_;
  std::unique_ptr<TabStripModel> tab_strip_model_;
  std::unique_ptr<tabs::GlicNudgeController> glic_nudge_controller_;
  std::unique_ptr<tabs::MockTabInterface> tab_interface_;
  std::unique_ptr<MockBrowserWindowInterface> browser_window_interface_;
  ui::UnownedUserDataHost data_host_;
  std::unique_ptr<CallToActionLock> call_to_action_;
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

TEST_F(TabStripActionContainerTest, MAYBE(GlicButtonDrawing)) {
  BuildGlicContainer(/*use_otr_profile=*/false);
  EXPECT_TRUE(tab_strip_action_container_->GetGlicButton());
}

TEST_F(TabStripActionContainerTest, GlicButtonUnsupportedProfile) {
  BuildGlicContainer(/*use_otr_profile=*/true);
  EXPECT_FALSE(tab_strip_action_container_->GetGlicButton());
}

TEST_F(TabStripActionContainerTest,
       MAYBE(OrdersButtonsCorrectlyAtConstruction)) {
  BuildGlicContainer(/*use_otr_profile=*/false);

// TODO(crbug.com/437141881): Fix flaky tests on Mac.
// Mac doesn't have a separator, so the children sizes are different.
#if !BUILDFLAG(IS_MAC)
  ASSERT_THAT(tab_strip_action_container_->children(), SizeIs(3));

  ASSERT_EQ(tab_strip_action_container_->glic_actor_button_container(),
            tab_strip_action_container_->children()[0]);

  ASSERT_THAT(
      tab_strip_action_container_->glic_actor_button_container()->children(),
      SizeIs(2));
  ASSERT_EQ(tab_strip_action_container_->glic_actor_task_icon(),
            tab_strip_action_container_->glic_actor_button_container()
                ->children()[1]);

  ASSERT_EQ(tab_strip_action_container_->GetGlicButton(),
            tab_strip_action_container_->children()[1]);
#endif  // !BUILDFLAG(IS_MAC)
}

TEST_F(TabStripActionContainerTest, MAYBE(OrdersButtonsCorrectlyWhenShown)) {
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
  ASSERT_THAT(tab_strip_action_container_->children(), SizeIs(2));

  ASSERT_EQ(tab_strip_action_container_->glic_actor_button_container(),
            tab_strip_action_container_->children()[0]);

  ASSERT_THAT(
      tab_strip_action_container_->glic_actor_button_container()->children(),
      SizeIs(3));

  ASSERT_EQ(tab_strip_action_container_->GetGlicButton(),
            tab_strip_action_container_->glic_actor_button_container()
                ->children()[1]);
  ASSERT_EQ(tab_strip_action_container_->glic_actor_task_icon(),
            tab_strip_action_container_->glic_actor_button_container()
                ->children()[2]);

#endif  // !BUILDFLAG(IS_MAC)
}

TEST_F(TabStripActionContainerTest, MAYBE(GlicButtonUpdateLabel)) {
  BuildGlicContainer(/*use_otr_profile=*/false);
  glic_nudge_controller_->UpdateNudgeLabel(
      web_contents(), "TEST", /*prompt_suggestion=*/std::nullopt,
      /*activity=*/std::nullopt, base::NullCallback());
  ASSERT_EQ(tab_strip_action_container_->GetGlicButton()->GetText(), u"TEST");
}

TEST_F(TabStripActionContainerTest, MAYBE(GlicButtonHideNudgeOnTabChange)) {
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
  ASSERT_EQ(tab_strip_action_container_->GetGlicButton()->GetText(),
            u"Ask Gemini");
}
