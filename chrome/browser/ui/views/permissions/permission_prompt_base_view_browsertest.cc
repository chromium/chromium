// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permissions/permission_prompt_base_view.h"

#include <memory>

#include "base/containers/contains.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_occlusion_tracker.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/permission_bubble/permission_bubble_test_util.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/permissions/test/mock_permission_request.h"
#include "content/public/test/browser_test.h"
#include "media/base/media_switches.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/events/test/test_event.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/widget/widget.h"

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

class PermissionPromptBaseViewBrowserTest : public InProcessBrowserTest {
 public:
  PermissionPromptBaseViewBrowserTest() {
    feature_list_.InitAndEnableFeature(
        media::kPictureInPictureOcclusionTracking);
  }
  PermissionPromptBaseViewBrowserTest(
      const PermissionPromptBaseViewBrowserTest&) = delete;
  PermissionPromptBaseViewBrowserTest& operator=(
      const PermissionPromptBaseViewBrowserTest&) = delete;
  ~PermissionPromptBaseViewBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(PermissionPromptBaseViewBrowserTest,
                       DisablesButtonsWhenOccludedByPictureInPictureWindows) {
  // Set up a permission delegate with a request.
  TestPermissionBubbleViewDelegate delegate;
  std::vector<std::unique_ptr<permissions::PermissionRequest>> raw_requests;
  raw_requests.push_back(std::make_unique<permissions::MockPermissionRequest>(
      permissions::RequestType::kGeolocation));
  delegate.set_requests(std::move(raw_requests));

  // Get the browser's widget to parent the bubble.
  views::Widget* parent =
      BrowserView::GetBrowserViewForBrowser(browser())->GetWidget();

  // Create the bubble.
  auto prompt_unique = std::make_unique<TestPermissionPromptBaseView>(
      parent, browser(), delegate.GetWeakPtr());
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
  bubble->CloseNow();
}

IN_PROC_BROWSER_TEST_F(PermissionPromptBaseViewBrowserTest,
                       IncludedInTrackedPictureInPictureWidgets) {
  // Set up a permission delegate with a request.
  TestPermissionBubbleViewDelegate delegate;
  std::vector<std::unique_ptr<permissions::PermissionRequest>> raw_requests;
  raw_requests.push_back(std::make_unique<permissions::MockPermissionRequest>(
      permissions::RequestType::kGeolocation));
  delegate.set_requests(std::move(raw_requests));

  // Get the main browser's widget to parent the bubble.
  views::Widget* parent =
      BrowserView::GetBrowserViewForBrowser(browser())->GetWidget();

  // Create a picture-in-picture browser window to request the permission.
  Browser::CreateParams params(Browser::TYPE_PICTURE_IN_PICTURE, GetProfile(),
                               true);
  Browser* pip_browser = Browser::Create(params);
  pip_browser->window()->Show();

  // Create the bubble for a picture-in-picture-window.
  auto prompt_unique = std::make_unique<TestPermissionPromptBaseView>(
      parent, pip_browser, delegate.GetWeakPtr());
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
