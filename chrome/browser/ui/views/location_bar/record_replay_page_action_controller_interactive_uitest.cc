// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/location_bar/record_replay_page_action_controller.h"
#include "chrome/browser/ui/views/page_action/page_action_container_view.h"
#include "chrome/browser/ui/views/page_action/page_action_view.h"
#include "chrome/browser/ui/views/record_replay/replay_recording_bubble_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/record_replay/core/browser/record_replay_client.h"
#include "components/record_replay/core/browser/record_replay_manager.h"
#include "components/record_replay/core/common/record_replay_features.h"
#include "content/public/test/browser_test.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/widget/any_widget_observer.h"

namespace {

class RecordReplayPageActionControllerInteractiveTest
    : public InProcessBrowserTest {
 protected:
  RecordReplayPageActionControllerInteractiveTest() {
    feature_list_.InitAndEnableFeature(
        record_replay::features::kRecordReplayBase);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(RecordReplayPageActionControllerInteractiveTest,
                       ExecuteAction_ShowsBubbleWhenRecordingExists) {
  tabs::TabInterface* tab = browser()->tab_strip_model()->GetActiveTab();
  record_replay::RecordReplayClient* client =
      tab->GetTabFeatures()->record_replay_client();
  ASSERT_TRUE(client);
  record_replay::RecordReplayManager& manager = client->GetManager();

  // Fake a recording.
  record_replay::Recording recording;
  recording.set_name("Test Recording");
  manager.SetRecordingForTesting(std::move(recording));

  RecordReplayPageActionController* controller =
      tab->GetTabFeatures()->record_replay_page_action_controller();
  ASSERT_TRUE(controller);

  // We need to wait for the recording metadata to be retrieved (UpdateState
  // calls manager.GetMatchingRecording). For simplicity in this test, we can
  // manually trigger the callback if we have access, or just wait. Let's assume
  // UpdateState will eventually pick it up.
  base::RunLoop run_loop;
  base::RepeatingTimer check_timer;
  check_timer.Start(FROM_HERE, base::Milliseconds(100),
                    base::BindLambdaForTesting([&]() {
                      if (controller->has_recording_for_testing()) {
                        run_loop.Quit();
                      }
                    }));
  run_loop.Run();

  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       "ReplayRecordingBubbleView");

  controller->ExecuteAction(nullptr);

  views::Widget* bubble_widget = waiter.WaitIfNeededAndGet();
  ASSERT_TRUE(bubble_widget);

  auto* bubble_delegate =
      bubble_widget->widget_delegate()->AsBubbleDialogDelegate();
  ASSERT_TRUE(bubble_delegate);
  EXPECT_EQ(bubble_delegate->GetWindowTitle(), u"Replay Recording (UT)");
}

}  // namespace
