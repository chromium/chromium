// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/page_action_metrics_recorder.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/views/page_action/page_action_enums.h"
#include "chrome/browser/ui/views/page_action/page_action_model.h"
#include "chrome/browser/ui/views/page_action/page_action_model_observer.h"
#include "chrome/browser/ui/views/page_action/page_action_properties_provider.h"
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

constexpr char kFirstUrlStr[] = "https://url-1.test";
constexpr char kSecondUrlStr[] = "https://url-2.test";

constexpr char kIconActionTypeShow[] = "PageActionController.ActionTypeShown2";
constexpr char kIconCTRHistogram[] = "PageActionController.Icon.CTR2";
constexpr char kIconCountHistogram[] =
    "PageActionController.Icon.NumberActionsShownWhenClicked";

constexpr char kChipTypeShowHistogram[] = "PageActionController.ChipTypeShown";
constexpr char kChipCTRHistogram[] = "PageActionController.Chip.CTR2";
constexpr char kChipCountHistogram[] =
    "PageActionController.Chip.NumberActionsShownWhenClicked";

using ::testing::_;
using ::testing::Return;

class PageActionMetricsRecorderTest : public testing::Test {
 protected:
  PageActionMetricsRecorderTest() : tab_(&profile_) {}

  ~PageActionMetricsRecorderTest() override {
    task_environment_.RunUntilIdle();
  }

  void SetUp() override {
    // By default, let the page action be "not visible". Tests can override.
    ON_CALL(mock_model_, GetVisible()).WillByDefault(Return(false));

    // By default, let the page action be "ephemeral". Tests can override.
    ON_CALL(mock_model_, IsEphemeral()).WillByDefault(Return(true));
  }

  void CreateRecorder() {
    properties_.type = PageActionIconType::kLensOverlay;
    properties_.histogram_name = "LensOverlay";
    recorder_ = std::make_unique<PageActionPerActionMetricsRecorder>(
        tab_, properties_, mock_model_,
        base::BindRepeating(&PageActionMetricsRecorderTest::GetVisibleCount,
                            base::Unretained(this)));
  }

  void FireModelChanged() { recorder_->OnPageActionModelChanged(mock_model_); }

  void SimulateClick(PageActionTrigger trigger) {
    recorder_->RecordClick(trigger);
  }

  int GetVisibleCount() const { return visible_count_; }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  MockPageActionModel mock_model_;
  FakeTabInterface tab_;
  PageActionProperties properties_;
  std::unique_ptr<PageActionPerActionMetricsRecorder> recorder_;

  int visible_count_ = 1;
};

TEST_F(PageActionMetricsRecorderTest, RecordOnlyOncePerUrlIfEphemeral) {
  base::HistogramTester histogram_tester;
  CreateRecorder();

  // Make the model "visible" and simulate the user visiting a particular URL.
  GURL url1(kFirstUrlStr);
  content::WebContentsTester::For(tab_.GetContents())->NavigateAndCommit(url1);
  histogram_tester.ExpectUniqueSample(kIconActionTypeShow, properties_.type, 0);

  ON_CALL(mock_model_, GetVisible()).WillByDefault(Return(true));

  // First time with url1 => record metric once.
  FireModelChanged();
  histogram_tester.ExpectUniqueSample(kIconActionTypeShow, properties_.type, 1);

  // If we call again, and it's the same URL, we expect no additional record.
  FireModelChanged();
  histogram_tester.ExpectUniqueSample(kIconActionTypeShow, properties_.type, 1);

  // Now simulate a second distinct URL. We should get a second record.
  GURL url2(kSecondUrlStr);
  content::WebContentsTester::For(tab_.GetContents())->NavigateAndCommit(url2);

  // Real navigation briefly hides the action; reproduce that so the recorder
  // sees a Hidden → IconOnly transition.
  ON_CALL(mock_model_, GetVisible()).WillByDefault(Return(false));
  FireModelChanged();

  ON_CALL(mock_model_, GetVisible()).WillByDefault(Return(true));
  FireModelChanged();

  // We now have two samples (one per distinct URL).
  histogram_tester.ExpectUniqueSample(kIconActionTypeShow, properties_.type, 2);
}

TEST_F(PageActionMetricsRecorderTest, NoRecordIfPageActionIsNotVisible) {
  base::HistogramTester histogram_tester;
  CreateRecorder();

  ON_CALL(mock_model_, IsEphemeral()).WillByDefault(Return(false));

  // Even if we call the model-changed event, because !GetVisible(),
  // we do nothing.
  FireModelChanged();
  histogram_tester.ExpectTotalCount(kIconActionTypeShow, 0);
}

TEST_F(PageActionMetricsRecorderTest, RecordShownMetricsGeneralAndSpecific) {
  base::HistogramTester histogram_tester;
  CreateRecorder();

  const std::string specific_histogram = base::StrCat(
      {"PageActionController.", properties_.histogram_name, ".Icon.CTR2"});

  // First navigation.
  GURL url1(kFirstUrlStr);
  content::WebContentsTester::For(tab_.GetContents())->NavigateAndCommit(url1);
  ON_CALL(mock_model_, GetVisible()).WillByDefault(Return(true));

  // First visibility → record once for each histogram with kShown.
  FireModelChanged();
  histogram_tester.ExpectTotalCount(kIconCTRHistogram, 1);
  histogram_tester.ExpectUniqueSample(kIconCTRHistogram,
                                      PageActionCTREvent::kShown, 1);
  histogram_tester.ExpectTotalCount(specific_histogram, 1);
  histogram_tester.ExpectUniqueSample(specific_histogram,
                                      PageActionCTREvent::kShown, 1);

  // Same URL, second visibility → no additional records.
  FireModelChanged();
  histogram_tester.ExpectTotalCount(kIconCTRHistogram, 1);
  histogram_tester.ExpectTotalCount(specific_histogram, 1);

  // New navigation.
  GURL url2(kSecondUrlStr);
  content::WebContentsTester::For(tab_.GetContents())->NavigateAndCommit(url2);
  // Real pages become invisible for a moment during navigation; reproduce that
  // so the recorder observes Hidden → IconOnly.
  ON_CALL(mock_model_, GetVisible()).WillByDefault(Return(false));
  FireModelChanged();

  ON_CALL(mock_model_, GetVisible()).WillByDefault(Return(true));
  FireModelChanged();

  // Should now have two kShown samples (one per distinct URL) in both
  // histograms.
  histogram_tester.ExpectTotalCount(kIconCTRHistogram, 2);
  histogram_tester.ExpectBucketCount(kIconCTRHistogram,
                                     PageActionCTREvent::kShown, 2);
  histogram_tester.ExpectTotalCount(specific_histogram, 2);
  histogram_tester.ExpectBucketCount(specific_histogram,
                                     PageActionCTREvent::kShown, 2);
}

TEST_F(PageActionMetricsRecorderTest, NoShownMetricsWhenNotEphemeral) {
  base::HistogramTester histogram_tester;
  CreateRecorder();

  ON_CALL(mock_model_, IsEphemeral()).WillByDefault(Return(false));

  const std::string specific_histogram = base::StrCat(
      {"PageActionController.", properties_.histogram_name, ".Icon.CTR2"});

  GURL url(kFirstUrlStr);
  content::WebContentsTester::For(tab_.GetContents())->NavigateAndCommit(url);

  FireModelChanged();

  // Because the page action is *not* ephemeral, no kShown samples are recorded.
  histogram_tester.ExpectTotalCount(kIconCTRHistogram, 0);
  histogram_tester.ExpectTotalCount(specific_histogram, 0);
}

TEST_F(PageActionMetricsRecorderTest, RecordClickMetric) {
  base::HistogramTester histogram_tester;
  CreateRecorder();

  // Prep the model so the recorder’s display-state becomes IconOnly.
  ON_CALL(mock_model_, GetVisible()).WillByDefault(Return(true));
  ON_CALL(mock_model_, IsChipShowing()).WillByDefault(Return(false));

  const std::string specific_histogram = base::StrCat(
      {"PageActionController.", properties_.histogram_name, ".Icon.CTR2"});

  histogram_tester.ExpectTotalCount(kIconCTRHistogram, 0);
  histogram_tester.ExpectTotalCount(specific_histogram, 0);

  FireModelChanged();

  histogram_tester.ExpectTotalCount(kIconCTRHistogram, 1);

  SimulateClick(PageActionTrigger::kMouse);

  // Verify both histograms logged once with kClicked value.
  histogram_tester.ExpectTotalCount(kIconCTRHistogram, 2);
  histogram_tester.ExpectBucketCount(kIconCTRHistogram,
                                     PageActionCTREvent::kClicked, 1);
  histogram_tester.ExpectTotalCount(specific_histogram, 2);
  histogram_tester.ExpectBucketCount(specific_histogram,
                                     PageActionCTREvent::kClicked, 1);

  SimulateClick(PageActionTrigger::kKeyboard);

  // Verify both histograms logged again (total 2), check bucket count.
  histogram_tester.ExpectTotalCount(kIconCTRHistogram, 3);
  histogram_tester.ExpectBucketCount(kIconCTRHistogram,
                                     PageActionCTREvent::kClicked, 2);
  histogram_tester.ExpectTotalCount(specific_histogram, 3);
  histogram_tester.ExpectBucketCount(specific_histogram,
                                     PageActionCTREvent::kClicked, 2);

  SimulateClick(PageActionTrigger::kGesture);

  // Verify both histograms logged again (total 3), check bucket count.
  histogram_tester.ExpectTotalCount(kIconCTRHistogram, 4);
  histogram_tester.ExpectBucketCount(kIconCTRHistogram,
                                     PageActionCTREvent::kClicked, 3);
  histogram_tester.ExpectTotalCount(specific_histogram, 4);
  histogram_tester.ExpectBucketCount(specific_histogram,
                                     PageActionCTREvent::kClicked, 3);
}

TEST_F(PageActionMetricsRecorderTest,
       NumberActionsShownWhenClickedOneVisibleAction) {
  base::HistogramTester histogram_tester;
  CreateRecorder();

  // Make the action visible so the recorder’s state is IconOnly.
  ON_CALL(mock_model_, GetVisible()).WillByDefault(Return(true));
  ON_CALL(mock_model_, IsChipShowing()).WillByDefault(Return(false));
  FireModelChanged();

  // Simulate a click when exactly one ephemeral page action is visible.
  visible_count_ = 1;
  recorder_->RecordClick(PageActionTrigger::kMouse);

  histogram_tester.ExpectTotalCount(kIconCountHistogram, 1);
  histogram_tester.ExpectBucketCount(kIconCountHistogram, /*sample=*/1,
                                     /*expected_count=*/1);
}

TEST_F(PageActionMetricsRecorderTest,
       NumberActionsShownWhenClickedVariousVisibleCounts) {
  base::HistogramTester histogram_tester;
  CreateRecorder();

  // Make the action visible so the recorder’s state is IconOnly.
  ON_CALL(mock_model_, GetVisible()).WillByDefault(Return(true));
  ON_CALL(mock_model_, IsChipShowing()).WillByDefault(Return(false));
  FireModelChanged();

  // First click with 2 visible actions.
  visible_count_ = 2;
  recorder_->RecordClick(PageActionTrigger::kMouse);

  // Second click with 5 visible actions.
  visible_count_ = 5;
  recorder_->RecordClick(PageActionTrigger::kKeyboard);

  // Third click with 2 visible actions again.
  visible_count_ = 2;
  recorder_->RecordClick(PageActionTrigger::kGesture);

  histogram_tester.ExpectTotalCount(kIconCountHistogram, 3);
  histogram_tester.ExpectBucketCount(kIconCountHistogram, /*sample=*/2,
                                     /*expected_count=*/2);
  histogram_tester.ExpectBucketCount(kIconCountHistogram, /*sample=*/5,
                                     /*expected_count=*/1);

  // No other buckets should be populated.
  for (int bucket = 0; bucket <= 20; ++bucket) {
    if (bucket == 2 || bucket == 5) {
      continue;
    }
    histogram_tester.ExpectBucketCount(kIconCountHistogram, bucket,
                                       /*expected_count=*/0);
  }
}

TEST_F(PageActionMetricsRecorderTest, ChipTypeShownRecordedOncePerNavigation) {
  base::HistogramTester histogram_tester;
  CreateRecorder();

  GURL url1(kFirstUrlStr);
  content::WebContentsTester::For(tab_.GetContents())->NavigateAndCommit(url1);

  // Hidden → IconOnly (no chip yet, so nothing logged).
  ON_CALL(mock_model_, GetVisible()).WillByDefault(Return(true));
  ON_CALL(mock_model_, IsChipShowing()).WillByDefault(Return(false));
  FireModelChanged();

  // IconOnly → Chip (chip appears → should log kChipTypeShowHistogram once).
  ON_CALL(mock_model_, IsChipShowing()).WillByDefault(Return(true));
  FireModelChanged();

  histogram_tester.ExpectTotalCount(kChipTypeShowHistogram, 1);
  histogram_tester.ExpectUniqueSample(kChipTypeShowHistogram, properties_.type,
                                      1);

  // Repeat visibility on the same URL → still only 1 sample.
  FireModelChanged();
  histogram_tester.ExpectTotalCount(kChipTypeShowHistogram, 1);

  GURL url2(kSecondUrlStr);
  content::WebContentsTester::For(tab_.GetContents())->NavigateAndCommit(url2);

  // Real pages go Hidden for a moment; reproduce.
  ON_CALL(mock_model_, GetVisible()).WillByDefault(Return(false));
  ON_CALL(mock_model_, IsChipShowing()).WillByDefault(Return(false));
  FireModelChanged();  // IconOnly → Hidden

  // Hidden → IconOnly.
  ON_CALL(mock_model_, GetVisible()).WillByDefault(Return(true));
  FireModelChanged();

  // IconOnly → Chip again.
  ON_CALL(mock_model_, IsChipShowing()).WillByDefault(Return(true));
  FireModelChanged();

  // Expect a second sample (one per distinct navigation).
  histogram_tester.ExpectTotalCount(kChipTypeShowHistogram, 2);
  histogram_tester.ExpectBucketCount(kChipTypeShowHistogram, properties_.type,
                                     2);
}

TEST_F(PageActionMetricsRecorderTest, ChipIconOnlyToChipRecordsChipShownOnly) {
  base::HistogramTester histogram_tester;
  CreateRecorder();

  GURL url(kFirstUrlStr);
  content::WebContentsTester::For(tab_.GetContents())->NavigateAndCommit(url);

  // Hidden → IconOnly first.
  ON_CALL(mock_model_, GetVisible()).WillByDefault(Return(true));
  ON_CALL(mock_model_, IsChipShowing()).WillByDefault(Return(false));
  FireModelChanged();

  // IconOnly → Chip.
  ON_CALL(mock_model_, IsChipShowing()).WillByDefault(Return(true));
  FireModelChanged();

  histogram_tester.ExpectTotalCount(kIconCTRHistogram, 1);
  histogram_tester.ExpectTotalCount(kChipCTRHistogram, 1);
}

TEST_F(PageActionMetricsRecorderTest, ChipShownOnlyOncePerNavigation) {
  base::HistogramTester histogram_tester;
  CreateRecorder();

  GURL url1(kFirstUrlStr);
  content::WebContentsTester::For(tab_.GetContents())->NavigateAndCommit(url1);
  ON_CALL(mock_model_, GetVisible()).WillByDefault(Return(true));
  FireModelChanged();
  ON_CALL(mock_model_, IsChipShowing()).WillByDefault(Return(true));
  FireModelChanged();

  // Repeat visibility on same URL → no new sample.
  FireModelChanged();
  histogram_tester.ExpectTotalCount(kChipCTRHistogram, 1);

  GURL url2(kSecondUrlStr);
  content::WebContentsTester::For(tab_.GetContents())->NavigateAndCommit(url2);

  // Simulate the brief "hidden" phase that occurs during real navigation.
  ON_CALL(mock_model_, GetVisible()).WillByDefault(Return(false));
  ON_CALL(mock_model_, IsChipShowing()).WillByDefault(Return(false));
  FireModelChanged();

  ON_CALL(mock_model_, GetVisible()).WillByDefault(Return(true));
  FireModelChanged();
  ON_CALL(mock_model_, IsChipShowing()).WillByDefault(Return(true));
  FireModelChanged();

  histogram_tester.ExpectTotalCount(kChipCTRHistogram, 2);
}

TEST_F(PageActionMetricsRecorderTest, ChipClickLogsClickedAndVisibleCount) {
  base::HistogramTester histogram_tester;
  CreateRecorder();

  // Make recorder think Chip is showing.
  ON_CALL(mock_model_, GetVisible()).WillByDefault(Return(true));
  FireModelChanged();
  ON_CALL(mock_model_, IsChipShowing()).WillByDefault(Return(true));
  FireModelChanged();

  visible_count_ = 1;
  recorder_->RecordClick(PageActionTrigger::kMouse);

  // Two samples in `kChipCTRHistogram`: one kShown + one kClicked.
  histogram_tester.ExpectTotalCount(kChipCTRHistogram, 2);
  histogram_tester.ExpectBucketCount(kChipCTRHistogram,
                                     PageActionCTREvent::kShown, 1);
  histogram_tester.ExpectBucketCount(kChipCTRHistogram,
                                     PageActionCTREvent::kClicked, 1);
  histogram_tester.ExpectTotalCount(kChipCountHistogram, 1);
  histogram_tester.ExpectBucketCount(kChipCountHistogram, 1, 1);
}

TEST_F(PageActionMetricsRecorderTest, ChipClickVariousVisibleCounts) {
  base::HistogramTester histogram_tester;
  CreateRecorder();

  // Ensure state = kChip.
  ON_CALL(mock_model_, GetVisible()).WillByDefault(Return(true));
  FireModelChanged();
  ON_CALL(mock_model_, IsChipShowing()).WillByDefault(Return(true));
  FireModelChanged();

  // 1st click (2 visible).
  visible_count_ = 2;
  recorder_->RecordClick(PageActionTrigger::kMouse);

  // 2nd click (5 visible).
  visible_count_ = 5;
  recorder_->RecordClick(PageActionTrigger::kKeyboard);

  // 3rd click (2 visible again).
  visible_count_ = 2;
  recorder_->RecordClick(PageActionTrigger::kGesture);

  histogram_tester.ExpectTotalCount(kChipCountHistogram, 3);
  histogram_tester.ExpectBucketCount(kChipCountHistogram, 2, 2);
  histogram_tester.ExpectBucketCount(kChipCountHistogram, 5, 1);
}

TEST_F(PageActionMetricsRecorderTest, ChipToIconOnlyDoesNotLogExtraShown) {
  base::HistogramTester histogram_tester;
  CreateRecorder();

  const std::string specific_chip_histogram = base::StrCat(
      {"PageActionController.", properties_.histogram_name, ".Chip.CTR2"});

  // Navigate to a URL and reach the IconOnly state first.
  GURL url(kFirstUrlStr);
  content::WebContentsTester::For(tab_.GetContents())->NavigateAndCommit(url);
  ON_CALL(mock_model_, GetVisible()).WillByDefault(Return(true));
  ON_CALL(mock_model_, IsChipShowing()).WillByDefault(Return(false));
  FireModelChanged();

  // IconOnly → Chip: should log `kShown` once for both generic & specific
  // histograms.
  ON_CALL(mock_model_, IsChipShowing()).WillByDefault(Return(true));
  FireModelChanged();

  histogram_tester.ExpectTotalCount(kChipCTRHistogram, 1);
  histogram_tester.ExpectBucketCount(kChipCTRHistogram,
                                     PageActionCTREvent::kShown, 1);
  histogram_tester.ExpectTotalCount(specific_chip_histogram, 1);
  histogram_tester.ExpectBucketCount(specific_chip_histogram,
                                     PageActionCTREvent::kShown, 1);

  // Chip → IconOnly: no new `kShown` samples should be recorded.
  ON_CALL(mock_model_, IsChipShowing()).WillByDefault(Return(false));
  FireModelChanged();

  histogram_tester.ExpectTotalCount(kChipCTRHistogram, 1);
  histogram_tester.ExpectTotalCount(specific_chip_histogram, 1);
}

}  // namespace
}  // namespace page_actions
