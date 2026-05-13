// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/record_replay/replay_recording_bubble_view.h"

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/view.h"
#include "ui/views/view_tracker.h"
#include "ui/views/widget/widget.h"

namespace record_replay {

class ReplayRecordingBubbleViewTest : public DialogBrowserTest {
 public:
  ReplayRecordingBubbleViewTest() = default;

  void ShowUi(const std::string& name) override {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    views::BubbleAnchor anchor(browser_view->toolbar());
    record_replay::Recording recording;
    recording.set_name("Test Recording");
    auto* bubble = new ReplayRecordingBubbleView(
        anchor, browser()->tab_strip_model()->GetActiveWebContents(),
        {recording}, nullptr /* manager */);
    views::BubbleDialogDelegateView::CreateBubble(bubble);
    bubble->ShowForReason(LocationBarBubbleDelegateView::USER_GESTURE);

    bubble_tracker_.SetView(bubble);
  }

  ReplayRecordingBubbleView* GetBubbleView() {
    return static_cast<ReplayRecordingBubbleView*>(bubble_tracker_.view());
  }

  void TearDownOnMainThread() override {
    DialogBrowserTest::TearDownOnMainThread();
  }

 protected:
  base::MockCallback<base::RepeatingClosure> on_replay_;
  views::ViewTracker bubble_tracker_;
};

IN_PROC_BROWSER_TEST_F(ReplayRecordingBubbleViewTest, InvokeUi_default) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(ReplayRecordingBubbleViewTest, AcceptInvokesCallback) {
  ShowUi("");
  ReplayRecordingBubbleView* bubble = GetBubbleView();
  ASSERT_NE(bubble, nullptr);

  bubble->SetReplayCallbackForTesting(on_replay_.Get());

  EXPECT_CALL(on_replay_, Run()).Times(1);
  bubble->OnReplayButtonClicked(0);
}

IN_PROC_BROWSER_TEST_F(ReplayRecordingBubbleViewTest,
                       CancelDoesNotInvokeCallback) {
  ShowUi("");
  ReplayRecordingBubbleView* bubble = GetBubbleView();
  ASSERT_NE(bubble, nullptr);

  bubble->SetReplayCallbackForTesting(on_replay_.Get());

  EXPECT_CALL(on_replay_, Run()).Times(0);
  bubble->Cancel();
}

}  // namespace record_replay
