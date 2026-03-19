// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/projects/projects_panel_view.h"

#include <memory>
#include <vector>

#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/tabs/projects/projects_panel_state_controller.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/contextual_tasks/public/mock_contextual_tasks_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/saved_tab_groups/test_support/mock_tab_group_sync_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/actions/action_id.h"
#include "ui/actions/actions.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view_utils.h"

class ProjectsPanelViewTest : public ChromeViewsTestBase {
 public:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    profile_ = std::make_unique<TestingProfile>();

    // Create a root action item for the panel.
    root_action_item_ =
        actions::ActionItem::Builder()
            .AddChildren(actions::ActionItem::Builder().SetActionId(
                kActionToggleProjectsPanel))
            .Build();

    // Setup TabGroupSyncService factory.
    tab_groups::TabGroupSyncServiceFactory::GetInstance()->SetTestingFactory(
        profile(), base::BindRepeating(
                       &ProjectsPanelViewTest::CreateMockTabGroupSyncService,
                       base::Unretained(this)));

    contextual_tasks::ContextualTasksServiceFactory::GetInstance()
        ->SetTestingFactory(
            profile(),
            base::BindRepeating(
                [](content::BrowserContext*) -> std::unique_ptr<KeyedService> {
                  return std::make_unique<testing::NiceMock<
                      contextual_tasks::MockContextualTasksService>>();
                }));

    // Create a real State Controller.
    EXPECT_CALL(mock_browser_window_interface_, GetUnownedUserDataHost())
        .WillRepeatedly(testing::ReturnRef(unowned_user_data_host_));

    state_controller_ = std::make_unique<ProjectsPanelStateController>(
        &mock_browser_window_interface_, root_action_item_.get(),
        /*aim_eligibility_service=*/nullptr, /*glic_enabling=*/nullptr);

    EXPECT_CALL(mock_browser_window_interface_, GetProfile())
        .WillRepeatedly(testing::Return(profile()));
  }

  void CreateView() {
    auto view = std::make_unique<ProjectsPanelView>(
        &mock_browser_window_interface_, root_action_item_.get(),
        state_controller_.get());
    widget_ = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    view_ = widget_->SetContentsView(std::move(view));
    widget_->SetBounds(gfx::Rect(0, 0, 800, 600));
    widget_->Show();
  }

  void TearDown() override {
    view_ = nullptr;
    // Widget owns the view, so it will delete it.
    // We need to close widget first.
    if (widget_ && !widget_->IsClosed()) {
      widget_->CloseNow();
    }
    widget_.reset();

    state_controller_.reset();
    profile_.reset();
    ChromeViewsTestBase::TearDown();
  }

  TestingProfile* profile() { return profile_.get(); }

  std::unique_ptr<KeyedService> CreateMockTabGroupSyncService(
      content::BrowserContext* context) {
    return std::make_unique<
        testing::NiceMock<tab_groups::MockTabGroupSyncService>>();
  }

 protected:
  ProjectsPanelStateController* state_controller() {
    return state_controller_.get();
  }

  ProjectsPanelView* projects_panel_view() { return view_; }

  base::MockCallback<base::OnceClosure> panel_closed_callback_;

 private:
  testing::NiceMock<MockBrowserWindowInterface> mock_browser_window_interface_;
  std::unique_ptr<TestingProfile> profile_;
  ui::UnownedUserDataHost unowned_user_data_host_;
  std::unique_ptr<actions::ActionItem> root_action_item_;
  std::unique_ptr<ProjectsPanelStateController> state_controller_;

  // Widget owns the view.
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<ProjectsPanelView> view_ = nullptr;
};

TEST_F(ProjectsPanelViewTest, CallbackRunsWhenAnimationsDisabled) {
  CreateView();
  ProjectsPanelView::disable_animations_for_testing();

  // Show the panel (animations disabled -> instant show)
  state_controller()->SetProjectsVisible(true);
  EXPECT_TRUE(state_controller()->IsProjectsPanelVisible());
  projects_panel_view()->OnProjectsPanelStateChanged(state_controller());
  EXPECT_TRUE(projects_panel_view()->GetVisible());

  projects_panel_view()->set_on_close_animation_ended_callback_for_testing(
      panel_closed_callback_.Get());

  EXPECT_CALL(panel_closed_callback_, Run());

  // Hide the panel (animations disabled -> instant hide & callback run)
  state_controller()->SetProjectsVisible(false);
  EXPECT_FALSE(state_controller()->IsProjectsPanelVisible());
  projects_panel_view()->OnProjectsPanelStateChanged(state_controller());

  EXPECT_FALSE(projects_panel_view()->GetVisible());
}

TEST_F(ProjectsPanelViewTest, CallbackDoesNotRunWhenVisible) {
  CreateView();
  ProjectsPanelView::disable_animations_for_testing();

  // Show the panel
  state_controller()->SetProjectsVisible(true);
  EXPECT_TRUE(state_controller()->IsProjectsPanelVisible());
  projects_panel_view()->OnProjectsPanelStateChanged(state_controller());

  projects_panel_view()->set_on_close_animation_ended_callback_for_testing(
      panel_closed_callback_.Get());

  EXPECT_CALL(panel_closed_callback_, Run()).Times(0);

  // Re-trigger show (should not run callback)
  projects_panel_view()->OnProjectsPanelStateChanged(state_controller());

  EXPECT_TRUE(projects_panel_view()->GetVisible());
}

TEST_F(ProjectsPanelViewTest, ThreadsContainerHiddenWhenNoThreads) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      tab_groups::kProjectsPanel,
      {{tab_groups::kProjectsPanelWithThreads.name, "true"}});

  CreateView();
  ProjectsPanelView::disable_animations_for_testing();

  // Show the panel
  state_controller()->SetProjectsVisible(true);
  projects_panel_view()->OnProjectsPanelStateChanged(state_controller());

  // Verify threads container and separator are hidden when there are no
  // threads.
  EXPECT_FALSE(
      projects_panel_view()->threads_container_for_testing()->GetVisible());
  EXPECT_FALSE(projects_panel_view()->separator_for_testing()->GetVisible());
}

class ProjectsPanelViewRTLTest : public ProjectsPanelViewTest {
 public:
  void SetUp() override {
    original_locale_ = base::i18n::GetConfiguredLocale();
    base::i18n::SetICUDefaultLocale("ar");
    ProjectsPanelViewTest::SetUp();
  }

  void TearDown() override {
    ProjectsPanelViewTest::TearDown();
    base::i18n::SetICUDefaultLocale(original_locale_);
  }

 private:
  std::string original_locale_;
};

TEST_F(ProjectsPanelViewRTLTest, RoundedCornersInRTL) {
  ASSERT_TRUE(base::i18n::IsRTL());
  CreateView();
  projects_panel_view()->SetIsElevated(true);

  auto radii = projects_panel_view()
                   ->content_container_for_testing()
                   ->layer()
                   ->rounded_corner_radii();

  // In RTL, we expect the left corners to be rounded.
  EXPECT_GT(radii.upper_left(), 0);
  EXPECT_GT(radii.lower_left(), 0);
  EXPECT_EQ(radii.upper_right(), 0);
  EXPECT_EQ(radii.lower_right(), 0);
}

TEST_F(ProjectsPanelViewRTLTest, ClipRectInRTL) {
  ASSERT_TRUE(base::i18n::IsRTL());
  CreateView();

  // Set some width to trigger layout.
  projects_panel_view()->SetBounds(0, 0, 100, 600);
  projects_panel_view()->SetTargetWidth(300);

  auto clip_rect = projects_panel_view()->layer()->clip_rect();

  // In RTL, we expect the clip rect to extend to the left to allow the shadow.
  EXPECT_LT(clip_rect.x(), 0);
}

TEST_F(ProjectsPanelViewTest, CloseButtonFadeWhenExpandingOnMac) {
  CreateView();
  auto* controls_view = projects_panel_view()->controls_view_for_testing();
  auto* projects_button = controls_view->projects_button_for_testing();
  gfx::SlideAnimation animation(projects_panel_view());

#if BUILDFLAG(IS_MAC)
  // At 0.5 or less, opacity should be 0.
  animation.Reset(0.0);
  projects_panel_view()->AnimationProgressed(&animation);
  EXPECT_FLOAT_EQ(0.0f, projects_button->layer()->opacity());

  animation.Reset(0.5);
  projects_panel_view()->AnimationProgressed(&animation);
  EXPECT_FLOAT_EQ(0.0f, projects_button->layer()->opacity());

  // At 0.75, opacity should be 0.5.
  animation.Reset(0.75);
  projects_panel_view()->AnimationProgressed(&animation);
  EXPECT_FLOAT_EQ(0.5f, projects_button->layer()->opacity());

  // At 1.0, opacity should be 1.0.
  animation.Reset(1.0);
  projects_panel_view()->AnimationProgressed(&animation);
  EXPECT_FLOAT_EQ(1.0f, projects_button->layer()->opacity());
#else
  // On other platforms, it should always be 1.0.
  animation.Reset(0.0);
  projects_panel_view()->AnimationProgressed(&animation);
  EXPECT_FLOAT_EQ(1.0f, projects_button->layer()->opacity());

  animation.Reset(0.5);
  projects_panel_view()->AnimationProgressed(&animation);
  EXPECT_FLOAT_EQ(1.0f, projects_button->layer()->opacity());

  animation.Reset(1.0);
  projects_panel_view()->AnimationProgressed(&animation);
  EXPECT_FLOAT_EQ(1.0f, projects_button->layer()->opacity());
#endif
}
