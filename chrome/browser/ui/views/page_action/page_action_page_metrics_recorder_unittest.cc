// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/page_action_page_metrics_recorder.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/views/page_action/page_action_enums.h"
#include "chrome/browser/ui/views/page_action/page_action_page_metrics_recorder.h"
#include "chrome/browser/ui/views/page_action/test_support/fake_tab_interface.h"
#include "chrome/browser/ui/views/page_action/test_support/mock_page_action_model.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace page_actions {
namespace {

using ::testing::Return;

class PageActionPageMetricsRecorderTest : public testing::Test {
 protected:
  PageActionPageMetricsRecorderTest() : tab_(&profile_) {}

  ~PageActionPageMetricsRecorderTest() override {
    task_environment_.RunUntilIdle();
  }

  void SetUp() override {
    // All tests assume the action is *ephemeral* and can toggle visibility.
    ON_CALL(mock_model_, IsEphemeral()).WillByDefault(Return(true));
  }

  void CreateRecorder() {
    recorder_ = std::make_unique<PageActionPageMetricsRecorder>(
        tab_, base::BindRepeating(
                  &PageActionPageMetricsRecorderTest::GetVisibleEphemeralCount,
                  base::Unretained(this)));
    // Recorder must observe at least one model to receive callbacks.
    recorder_->Observe(mock_model_);
  }

  // Simulate a main-frame navigation.
  void Navigate(const GURL& url) {
    content::WebContentsTester::For(tab_.GetContents())->NavigateAndCommit(url);
  }

  // Tell the recorder that the model’s visibility changed.
  void NotifyModelChanged() {
    recorder_->OnPageActionModelChanged(mock_model_);
  }

  // Convenience wrapper for “icon became visible”.
  void ShowIconAndNotify() {
    ON_CALL(mock_model_, GetVisible()).WillByDefault(Return(true));
    NotifyModelChanged();
  }

  // Callback used by recorder to learn how many *ephemeral* actions are
  // visible.
  int GetVisibleEphemeralCount() const { return visible_count_; }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  FakeTabInterface tab_;

  MockPageActionModel mock_model_;
  std::unique_ptr<PageActionPageMetricsRecorder> recorder_;

  // Manually controlled by tests before calling ShowIconAndNotify().
  int visible_count_ = 0;
};

TEST_F(PageActionPageMetricsRecorderTest,
       NumberActionsShown2_RecordsCurrentCount) {
  base::HistogramTester histogram_tester;
  CreateRecorder();

  Navigate(GURL("https://a.test/"));

  // First visibility – one action.
  visible_count_ = 1;
  ShowIconAndNotify();  //  → records "1"

  // Second visibility change – three actions.
  visible_count_ = 3;
  ShowIconAndNotify();  //  → records "3"

  constexpr char kHistogram[] = "PageActionController.NumberActionsShown3";
  histogram_tester.ExpectTotalCount(kHistogram, /*expected_count=*/2);
  histogram_tester.ExpectBucketCount(kHistogram, /*sample=*/1,
                                     /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(kHistogram, /*sample=*/3,
                                     /*expected_count=*/1);
}

TEST_F(PageActionPageMetricsRecorderTest,
       PagesWithActionsShown2_BasicSequence) {
  base::HistogramTester histogram_tester;
  CreateRecorder();

  constexpr char kHistogram[] = "PageActionController.PagesWithActionsShown3";

  // (1) First navigation, single visible icon.
  Navigate(GURL("https://a.test/"));
  visible_count_ = 1;
  ShowIconAndNotify();

  histogram_tester.ExpectBucketCount(
      kHistogram, PageActionPageEvent::kPageShown, /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      kHistogram, PageActionPageEvent::kActionShown, /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(kHistogram,
                                     PageActionPageEvent::kMultipleActionsShown,
                                     /*expected_count=*/0);

  // (2) Same page, now two visible icons.
  visible_count_ = 2;
  ShowIconAndNotify();

  histogram_tester.ExpectBucketCount(kHistogram,
                                     PageActionPageEvent::kMultipleActionsShown,
                                     /*expected_count=*/1);

  // (3) Navigate to a new page, first icon appears.
  Navigate(GURL("https://b.example/"));
  visible_count_ = 1;
  ShowIconAndNotify();

  histogram_tester.ExpectBucketCount(
      kHistogram, PageActionPageEvent::kPageShown, /*expected_count=*/2);
  histogram_tester.ExpectBucketCount(
      kHistogram, PageActionPageEvent::kActionShown, /*expected_count=*/2);
  // Still only one multi-action record.
  histogram_tester.ExpectBucketCount(kHistogram,
                                     PageActionPageEvent::kMultipleActionsShown,
                                     /*expected_count=*/1);
}

}  // namespace
}  // namespace page_actions
