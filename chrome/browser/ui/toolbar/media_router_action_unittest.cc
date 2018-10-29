// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/macros.h"
#include "chrome/browser/extensions/extension_action_test_util.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/extensions/browser_action_test_util.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/media_router_action.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_delegate.h"
#include "chrome/browser/ui/webui/media_router/media_router_dialog_controller_webui_impl.h"
#include "chrome/browser/ui/webui/media_router/media_router_web_ui_test.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/site_instance.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/paint_vector_icon.h"

using content::WebContents;
using media_router::MediaRouterDialogControllerWebUIImpl;

namespace {

gfx::Image GetActionIcon(ToolbarActionViewController* action) {
  return action->GetIcon(nullptr, gfx::Size());
}

}  // namespace

class MockToolbarActionViewDelegate : public ToolbarActionViewDelegate {
 public:
  MockToolbarActionViewDelegate() {}
  ~MockToolbarActionViewDelegate() override {}

  MOCK_CONST_METHOD0(GetCurrentWebContents, WebContents*());
  MOCK_METHOD0(UpdateState, void());
  MOCK_CONST_METHOD0(IsMenuRunning, bool());
  MOCK_METHOD1(OnPopupShown, void(bool by_user));
  MOCK_METHOD0(OnPopupClosed, void());
};

class TestMediaRouterAction : public MediaRouterAction {
 public:
  TestMediaRouterAction(Browser* browser,
                        ToolbarActionsBar* toolbar_actions_bar)
      : MediaRouterAction(browser, toolbar_actions_bar), controller_(nullptr) {}
  ~TestMediaRouterAction() override {}

  // MediaRouterAction:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override {
    // This would be null if |controller_| hasn't been set.
    if (GetMediaRouterDialogController()) {
      MediaRouterAction::OnTabStripModelChanged(tab_strip_model, change,
                                                selection);
    }
  }

  void SetMediaRouterDialogController(
      MediaRouterDialogControllerWebUIImpl* controller) {
    DCHECK(controller);
    controller_ = controller;
  }

 private:
  // MediaRouterAction:
  MediaRouterDialogControllerWebUIImpl* GetMediaRouterDialogController()
      override {
    return controller_;
  }

  MediaRouterDialogControllerWebUIImpl* controller_;
};

class MediaRouterActionUnitTest : public MediaRouterWebUITest {
 public:
  MediaRouterActionUnitTest()
      : MediaRouterWebUITest(true),
        toolbar_model_(nullptr),
        fake_issue_notification_(media_router::IssueInfo(
            "title notification",
            media_router::IssueInfo::Action::DISMISS,
            media_router::IssueInfo::Severity::NOTIFICATION)),
        fake_issue_warning_(media_router::IssueInfo(
            "title warning",
            media_router::IssueInfo::Action::LEARN_MORE,
            media_router::IssueInfo::Severity::WARNING)),
        fake_issue_fatal_(
            media_router::IssueInfo("title fatal",
                                    media_router::IssueInfo::Action::DISMISS,
                                    media_router::IssueInfo::Severity::FATAL)),
        fake_source1_("fakeSource1"),
        fake_source2_("fakeSource2"),
        active_icon_(GetIcon(vector_icons::kMediaRouterActiveIcon)),
        error_icon_(GetIcon(vector_icons::kMediaRouterErrorIcon)),
        idle_icon_(GetIcon(vector_icons::kMediaRouterIdleIcon)),
        warning_icon_(GetIcon(vector_icons::kMediaRouterWarningIcon)) {}

  ~MediaRouterActionUnitTest() override {}

  // MediaRouterWebUITest:
  void SetUp() override {
    MediaRouterWebUITest::SetUp();
    toolbar_model_ =
        extensions::extension_action_test_util::CreateToolbarModelForProfile(
            profile());

    // browser() will only be valid once BrowserWithTestWindowTest::SetUp()
    // has run.
    browser_action_test_util_ = BrowserActionTestUtil::Create(browser(), false);
    action_.reset(
        new TestMediaRouterAction(
            browser(),
            browser_action_test_util_->GetToolbarActionsBar()));

    local_display_route_list_.push_back(media_router::MediaRoute(
        "routeId1", fake_source1_, "sinkId1", "description", true, true));
    non_local_display_route_list_.push_back(media_router::MediaRoute(
        "routeId2", fake_source1_, "sinkId2", "description", false, true));
    non_local_display_route_list_.push_back(media_router::MediaRoute(
        "routeId3", fake_source2_, "sinkId3", "description", true, false));
  }

  void TearDown() override {
    action_.reset();
    browser_action_test_util_.reset();
    MediaRouterWebUITest::TearDown();
  }

  gfx::Image GetIcon(const gfx::VectorIcon& icon) {
    return gfx::Image(
        gfx::CreateVectorIcon(icon, MediaRouterAction::GetIconColor(icon)));
  }

  TestMediaRouterAction* action() { return action_.get(); }
  const media_router::Issue& fake_issue_notification() {
    return fake_issue_notification_;
  }
  const media_router::Issue& fake_issue_warning() {
    return fake_issue_warning_;
  }
  const media_router::Issue& fake_issue_fatal() { return fake_issue_fatal_; }
  const gfx::Image active_icon() { return active_icon_; }
  const gfx::Image error_icon() { return error_icon_; }
  const gfx::Image idle_icon() { return idle_icon_; }
  const gfx::Image warning_icon() { return warning_icon_; }
  const std::vector<media_router::MediaRoute>& local_display_route_list()
      const {
    return local_display_route_list_;
  }
  const std::vector<media_router::MediaRoute>& non_local_display_route_list()
      const {
    return non_local_display_route_list_;
  }
  const std::vector<media_router::MediaRoute::Id>& empty_route_id_list() const {
    return empty_route_id_list_;
  }

 private:
  // A BrowserActionTestUtil object constructed with the associated
  // ToolbarActionsBar.
  std::unique_ptr<BrowserActionTestUtil> browser_action_test_util_;

  std::unique_ptr<TestMediaRouterAction> action_;

  // The associated ToolbarActionsModel (owned by the keyed service setup).
  ToolbarActionsModel* toolbar_model_;

  // Fake Issues.
  const media_router::Issue fake_issue_notification_;
  const media_router::Issue fake_issue_warning_;
  const media_router::Issue fake_issue_fatal_;

  // Fake Sources, used for the Routes.
  const media_router::MediaSource fake_source1_;
  const media_router::MediaSource fake_source2_;

  // Cached images.
  const gfx::Image active_icon_;
  const gfx::Image error_icon_;
  const gfx::Image idle_icon_;
  const gfx::Image warning_icon_;

  std::vector<media_router::MediaRoute> local_display_route_list_;
  std::vector<media_router::MediaRoute> non_local_display_route_list_;
  std::vector<media_router::MediaRoute::Id> empty_route_id_list_;

  DISALLOW_COPY_AND_ASSIGN(MediaRouterActionUnitTest);
};

// Tests the initial state of MediaRouterAction after construction.
TEST_F(MediaRouterActionUnitTest, Initialization) {
  EXPECT_EQ("media_router_action", action()->GetId());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_MEDIA_ROUTER_TITLE),
      action()->GetActionName());
  EXPECT_TRUE(gfx::test::AreImagesEqual(idle_icon(), GetActionIcon(action())));
}

// Tests the MediaRouterAction icon based on updates to issues.
TEST_F(MediaRouterActionUnitTest, UpdateIssues) {
  // Initially, there are no issues.
  EXPECT_TRUE(gfx::test::AreImagesEqual(idle_icon(), GetActionIcon(action())));

  // Don't update |current_icon_| since the issue is only a notification.
  action()->OnIssue(fake_issue_notification());
  EXPECT_TRUE(gfx::test::AreImagesEqual(idle_icon(), GetActionIcon(action())));

  // Update |current_icon_| since the issue is a warning.
  action()->OnIssue(fake_issue_warning());
  EXPECT_TRUE(
      gfx::test::AreImagesEqual(warning_icon(), GetActionIcon(action())));

  // Update |current_icon_| since the issue is fatal.
  action()->OnIssue(fake_issue_fatal());
  EXPECT_TRUE(gfx::test::AreImagesEqual(error_icon(), GetActionIcon(action())));

  // Clear the issue.
  action()->OnIssuesCleared();
  EXPECT_TRUE(gfx::test::AreImagesEqual(idle_icon(), GetActionIcon(action())));
}

// Tests the MediaRouterAction state updates based on whether there are local
// routes.
TEST_F(MediaRouterActionUnitTest, UpdateRoutes) {
  // Initially, there are no routes.
  EXPECT_TRUE(gfx::test::AreImagesEqual(idle_icon(), GetActionIcon(action())));

  // Update |current_icon_| since there is a local route.
  action()->OnRoutesUpdated(local_display_route_list(), empty_route_id_list());
  EXPECT_TRUE(
      gfx::test::AreImagesEqual(active_icon(), GetActionIcon(action())));

  // Update |current_icon_| since there are no local routes.
  action()->OnRoutesUpdated(non_local_display_route_list(),
                            empty_route_id_list());
  EXPECT_TRUE(gfx::test::AreImagesEqual(idle_icon(), GetActionIcon(action())));

  action()->OnRoutesUpdated(std::vector<media_router::MediaRoute>(),
                            empty_route_id_list());
  EXPECT_TRUE(gfx::test::AreImagesEqual(idle_icon(), GetActionIcon(action())));
}

// Tests the MediaRouterAction icon based on updates to both issues and routes.
TEST_F(MediaRouterActionUnitTest, UpdateIssuesAndRoutes) {
  // Initially, there are no issues or routes.
  EXPECT_TRUE(gfx::test::AreImagesEqual(idle_icon(), GetActionIcon(action())));

  // There is no change in |current_icon_| since notification issues do not
  // update the state.
  action()->OnIssue(fake_issue_notification());
  EXPECT_TRUE(gfx::test::AreImagesEqual(idle_icon(), GetActionIcon(action())));

  // Non-local routes also do not have an effect on |current_icon_|.
  action()->OnRoutesUpdated(non_local_display_route_list(),
                            empty_route_id_list());
  EXPECT_TRUE(gfx::test::AreImagesEqual(idle_icon(), GetActionIcon(action())));

  // Update |current_icon_| since there is a local route.
  action()->OnRoutesUpdated(local_display_route_list(), empty_route_id_list());
  EXPECT_TRUE(
      gfx::test::AreImagesEqual(active_icon(), GetActionIcon(action())));

  // Update |current_icon_|, with a priority to reflect the warning issue
  // rather than the local route.
  action()->OnIssue(fake_issue_warning());
  EXPECT_TRUE(
      gfx::test::AreImagesEqual(warning_icon(), GetActionIcon(action())));

  // Closing a local route makes no difference to |current_icon_|.
  action()->OnRoutesUpdated(non_local_display_route_list(),
                            empty_route_id_list());
  EXPECT_TRUE(
      gfx::test::AreImagesEqual(warning_icon(), GetActionIcon(action())));

  // Update |current_icon_| since the issue has been updated to fatal.
  action()->OnIssue(fake_issue_fatal());
  EXPECT_TRUE(gfx::test::AreImagesEqual(error_icon(), GetActionIcon(action())));

  // Fatal issues still take precedent over local routes.
  action()->OnRoutesUpdated(local_display_route_list(), empty_route_id_list());
  EXPECT_TRUE(gfx::test::AreImagesEqual(error_icon(), GetActionIcon(action())));

  // When the fatal issue is dismissed, |current_icon_| reflects the existing
  // local route.
  action()->OnIssuesCleared();
  EXPECT_TRUE(
      gfx::test::AreImagesEqual(active_icon(), GetActionIcon(action())));

  // Update |current_icon_| when the local route is closed.
  action()->OnRoutesUpdated(non_local_display_route_list(),
                            empty_route_id_list());
  EXPECT_TRUE(gfx::test::AreImagesEqual(idle_icon(), GetActionIcon(action())));
}

TEST_F(MediaRouterActionUnitTest, IconPressedState) {
  // Start with one window with one tab.
  EXPECT_EQ(0, browser()->tab_strip_model()->count());
  chrome::NewTab(browser());
  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  WebContents* initiator = browser()->tab_strip_model()->GetActiveWebContents();
  MediaRouterDialogControllerWebUIImpl::CreateForWebContents(initiator);
  MediaRouterDialogControllerWebUIImpl* dialog_controller =
      MediaRouterDialogControllerWebUIImpl::FromWebContents(initiator);
  ASSERT_TRUE(dialog_controller);

  // Sets the controller to use for TestMediaRouterAction.
  action()->SetMediaRouterDialogController(dialog_controller);

  // Create a ToolbarActionViewDelegate to use for MediaRouterAction.
  std::unique_ptr<MockToolbarActionViewDelegate> mock_delegate(
      new MockToolbarActionViewDelegate());

  EXPECT_CALL(*mock_delegate, GetCurrentWebContents())
      .WillOnce(testing::Return(initiator));
  action()->SetDelegate(mock_delegate.get());

  // Skip closing the overflow menu in tests.
  action()->set_skip_close_overflow_menu_for_testing(true);

  EXPECT_CALL(*mock_delegate, OnPopupShown(true)).Times(1);
  action()->ExecuteAction(true);
  EXPECT_TRUE(dialog_controller->IsShowingMediaRouterDialog());

  // Pressing the icon while the popup is shown should close the popup
  EXPECT_CALL(*mock_delegate, OnPopupClosed()).Times(1);
  action()->ExecuteAction(true);
  EXPECT_FALSE(dialog_controller->IsShowingMediaRouterDialog());

  EXPECT_CALL(*mock_delegate, OnPopupShown(true)).Times(1);
  dialog_controller->CreateMediaRouterDialog();

  EXPECT_CALL(*mock_delegate, OnPopupClosed()).Times(1);
  dialog_controller->HideMediaRouterDialog();
}
