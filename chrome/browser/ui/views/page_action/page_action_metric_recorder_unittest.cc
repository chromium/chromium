// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/views/page_action/page_action_metrics_recorder.h"
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

using ::testing::_;
using ::testing::Return;

class PageActionMetricsRecorderTest : public testing::Test {
 protected:
  PageActionMetricsRecorderTest() : tab_(&profile_) {}

  void SetUp() override {
    // By default, let the page action be "not visible." Tests can override.
    ON_CALL(mock_model_, GetVisible()).WillByDefault(Return(false));
  }

  void CreateRecorder(bool is_ephemeral) {
    properties_.type = PageActionIconType::kLensOverlay;
    properties_.is_ephemeral = is_ephemeral;
    recorder_ = std::make_unique<PageActionMetricsRecorder>(tab_, properties_,
                                                            mock_model_);
  }

  void FireModelChanged() { recorder_->OnPageActionModelChanged(mock_model_); }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  MockPageActionModel mock_model_;
  FakeTabInterface tab_;
  PageActionProperties properties_;
  std::unique_ptr<PageActionMetricsRecorder> recorder_;
};

TEST_F(PageActionMetricsRecorderTest, NoRecordIfNotEphemeral) {
  base::HistogramTester histogram_tester;
  CreateRecorder(/*is_ephemeral=*/false);

  // Make the action "visible." Because it's not ephemeral, no metric is
  // recorded.
  ON_CALL(mock_model_, GetVisible()).WillByDefault(Return(true));

  FireModelChanged();
  // Because is_ephemeral=false, "PageActionController.ActionTypeShown2" is not
  // recorded.
  histogram_tester.ExpectTotalCount("PageActionController.ActionTypeShown2", 0);
}

TEST_F(PageActionMetricsRecorderTest, RecordOnlyOncePerUrlIfEphemeral) {
  base::HistogramTester histogram_tester;
  CreateRecorder(/*is_ephemeral=*/true);

  // Make the model "visible" and simulate the user visiting a particular URL.
  GURL url1("https://www.example.com/");
  content::WebContentsTester::For(tab_.GetContents())->NavigateAndCommit(url1);
  histogram_tester.ExpectUniqueSample("PageActionController.ActionTypeShown2",
                                      properties_.type, 0);

  ON_CALL(mock_model_, GetVisible()).WillByDefault(Return(true));

  // First time with url1 => record metric once.
  FireModelChanged();
  histogram_tester.ExpectUniqueSample("PageActionController.ActionTypeShown2",
                                      properties_.type, 1);

  // If we call again, and it's the same URL, we expect no additional record.
  FireModelChanged();
  histogram_tester.ExpectUniqueSample("PageActionController.ActionTypeShown2",
                                      properties_.type, 1);

  // Now simulate a second distinct URL. We should get a second record.
  GURL url2("https://www.another-site.org/");
  content::WebContentsTester::For(tab_.GetContents())->NavigateAndCommit(url2);
  FireModelChanged();
  // 2 total samples now (one for url1, one for url2).
  histogram_tester.ExpectUniqueSample("PageActionController.ActionTypeShown2",
                                      properties_.type, 2);
}

TEST_F(PageActionMetricsRecorderTest, NoRecordIfPageActionIsNotVisible) {
  base::HistogramTester histogram_tester;
  CreateRecorder(/*is_ephemeral=*/true);

  // The action is ephemeral, but GetVisible() returns false.
  ON_CALL(mock_model_, GetVisible()).WillByDefault(Return(false));

  // Even if we call the model-changed event, because !GetVisible(),
  // we do nothing.
  FireModelChanged();
  histogram_tester.ExpectTotalCount("PageActionController.ActionTypeShown2", 0);
}

}  // namespace
}  // namespace page_actions
