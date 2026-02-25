// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/projects/projects_panel_view.h"

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/tabs/projects/projects_panel_state_controller.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/contextual_tasks/public/mock_contextual_tasks_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/saved_tab_groups/test_support/mock_tab_group_sync_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/actions/action_id.h"
#include "ui/actions/actions.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"
#include "ui/views/test/views_test_base.h"

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
        &mock_browser_window_interface_, root_action_item_.get());

    EXPECT_CALL(mock_browser_window_interface_, GetProfile())
        .WillRepeatedly(testing::Return(profile()));

    auto view = std::make_unique<ProjectsPanelView>(
        &mock_browser_window_interface_, root_action_item_.get());
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
