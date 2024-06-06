// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permissions/permission_prompt_base_view.h"

#include "base/containers/contains.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_occlusion_tracker.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/permission_bubble/permission_bubble_test_util.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/permissions/test/mock_permission_request.h"
#include "media/base/media_switches.h"
#include "ui/events/test/test_event.h"

namespace {

// Minimal implementation of the PermissionPromptBaseView for testing.
class TestPermissionPromptBaseView : public PermissionPromptBaseView {
  METADATA_HEADER(TestPermissionPromptBaseView, PermissionPromptBaseView)

 public:
  TestPermissionPromptBaseView(
      views::Widget* parent,
      Browser* browser,
      base::WeakPtr<permissions::PermissionPrompt::Delegate> delegate)
      : PermissionPromptBaseView(browser, delegate) {
    set_parent_window(parent->GetNativeView());
  }
  TestPermissionPromptBaseView(const TestPermissionPromptBaseView&) = delete;
  TestPermissionPromptBaseView& operator=(const TestPermissionPromptBaseView&) =
      delete;
  ~TestPermissionPromptBaseView() override = default;

  // PermissionPromptBaseView:
  void RunButtonCallback(int button_view_id) override {}
};

BEGIN_METADATA(TestPermissionPromptBaseView)
END_METADATA

class PermissionPromptBaseViewTest : public ChromeViewsTestBase {
 public:
  PermissionPromptBaseViewTest() = default;
  PermissionPromptBaseViewTest(const PermissionPromptBaseViewTest&) = delete;
  PermissionPromptBaseViewTest& operator=(const PermissionPromptBaseViewTest&) =
      delete;
  ~PermissionPromptBaseViewTest() override = default;

  void SetUp() override {
    feature_list_.InitAndEnableFeature(
        media::kPictureInPictureOcclusionTracking);
    ChromeViewsTestBase::SetUp();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(PermissionPromptBaseViewTest,
       DisablesButtonsWhenOccludedByPictureInPictureWindows) {
  // Set up a permission delegate with a request.
  permissions::MockPermissionRequest request(
      permissions::RequestType::kGeolocation);
  TestPermissionBubbleViewDelegate delegate;
  std::vector<raw_ptr<permissions::PermissionRequest, VectorExperimental>>
      raw_requests;
  raw_requests.push_back(&request);
  delegate.set_requests(raw_requests);

  // Create a widget to parent the bubble.
  std::unique_ptr<views::Widget> parent =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  parent->Show();

  // Create the bubble.
  auto prompt_unique = std::make_unique<TestPermissionPromptBaseView>(
      parent.get(), /*browser=*/nullptr, delegate.GetWeakPtr());
  PermissionPromptBaseView* prompt = prompt_unique.get();
  views::Widget* bubble =
      views::BubbleDialogDelegateView::CreateBubble(std::move(prompt_unique));
  bubble->Show();

  // Button presses should not be ignored as the prompt is not occluded.
  EXPECT_FALSE(prompt->ShouldIgnoreButtonPressedEventHandling(
      /*button=*/nullptr, ui::test::TestEvent()));

  // Occlude the prompt.
  PictureInPictureWindowManager::GetInstance()
      ->GetOcclusionTracker()
      ->SetWidgetOcclusionStateForTesting(bubble, true);

  // Button presses should be ignored.
  EXPECT_TRUE(prompt->ShouldIgnoreButtonPressedEventHandling(
      /*button=*/nullptr, ui::test::TestEvent()));

  // Unocclude the prompt.
  PictureInPictureWindowManager::GetInstance()
      ->GetOcclusionTracker()
      ->SetWidgetOcclusionStateForTesting(bubble, false);

  // Button presses should no longer be ignored.
  EXPECT_FALSE(prompt->ShouldIgnoreButtonPressedEventHandling(
      /*button=*/nullptr, ui::test::TestEvent()));
}

TEST_F(PermissionPromptBaseViewTest, IncludedInTrackedPictureInPictureWidgets) {
  // Set up a permission delegate with a request.
  permissions::MockPermissionRequest request(
      permissions::RequestType::kGeolocation);
  TestPermissionBubbleViewDelegate delegate;
  std::vector<raw_ptr<permissions::PermissionRequest, VectorExperimental>>
      raw_requests;
  raw_requests.push_back(&request);
  delegate.set_requests(raw_requests);

  // Create a widget to parent the bubble.
  std::unique_ptr<views::Widget> parent =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  parent->Show();

  // Create a picture-in-picture browser window to request the permission.
  TestingProfile profile;
  TestBrowserWindow browser_window;
  std::unique_ptr<Browser> browser;
  Browser::CreateParams params(&profile, /*user_gesture=*/true);
  params.type = Browser::TYPE_PICTURE_IN_PICTURE;
  params.window = &browser_window;
  browser.reset(Browser::Create(params));

  // Create the bubble for a picture-in-picture-window.
  auto prompt_unique = std::make_unique<TestPermissionPromptBaseView>(
      parent.get(), /*browser=*/browser.get(), delegate.GetWeakPtr());
  views::Widget* bubble =
      views::BubbleDialogDelegateView::CreateBubble(std::move(prompt_unique));
  bubble->Show();

  // The PictureInPictureOcclusionTracker should be tracking the bubble as an
  // always-on-top window that can occlude UI.
  PictureInPictureOcclusionTracker* tracker =
      PictureInPictureWindowManager::GetInstance()->GetOcclusionTracker();
  EXPECT_TRUE(
      base::Contains(tracker->GetPictureInPictureWidgetsForTesting(), bubble));

  bubble->CloseNow();
}

}  // namespace
