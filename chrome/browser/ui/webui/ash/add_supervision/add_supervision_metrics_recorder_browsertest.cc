// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/test_mock_time_task_runner.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/ash/add_supervision/add_supervision.mojom.h"
#include "chrome/browser/ui/webui/ash/add_supervision/add_supervision_handler.h"
#include "chrome/browser/ui/webui/ash/add_supervision/add_supervision_handler_utils.h"
#include "chrome/browser/ui/webui/ash/add_supervision/add_supervision_metrics_recorder.h"
#include "chrome/browser/ui/webui/ash/add_supervision/add_supervision_ui.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_web_ui.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace ash {

// This browser test class is for the Add Supervision metrics recorder.
class AddSupervisionMetricsRecorderTest : public InProcessBrowserTest {
 public:
  AddSupervisionMetricsRecorderTest() = default;

  AddSupervisionMetricsRecorderTest(const AddSupervisionMetricsRecorderTest&) =
      delete;
  AddSupervisionMetricsRecorderTest& operator=(
      const AddSupervisionMetricsRecorderTest&) = delete;

  ~AddSupervisionMetricsRecorderTest() override = default;

  void SetUpOnMainThread() override {
    identity_test_env_ = std::make_unique<signin::IdentityTestEnvironment>();
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    test_web_ui_.set_web_contents(web_contents);
  }

  void ShowAddSupervisionDialog() { AddSupervisionDialog::Show(); }

  void CloseNowForTesting() {
    AddSupervisionDialog* instance =
        static_cast<AddSupervisionDialog*>(AddSupervisionDialog::GetInstance());
    instance->CloseNowForTesting();
  }

  void CloseAddSupervisionDialog() {
    AddSupervisionDialog::GetInstance()->OnDialogWillClose();
    CloseNowForTesting();
  }

  void NotifySupervisionEnabled() {
    mojo::PendingReceiver<add_supervision::mojom::AddSupervisionHandler>
        receiver;
    AddSupervisionUI add_supervision_ui(&test_web_ui_);
    AddSupervisionHandler add_supervision_handler(
        std::move(receiver), &test_web_ui_,
        identity_test_env_->identity_manager(), &add_supervision_ui);
    add_supervision_handler.NotifySupervisionEnabled();
  }

  void LogOutAndClose() {
    LogOutHelper();
    CloseNowForTesting();
  }

 private:
  std::unique_ptr<signin::IdentityTestEnvironment> identity_test_env_;
  content::TestWebUI test_web_ui_;
};

IN_PROC_BROWSER_TEST_F(AddSupervisionMetricsRecorderTest, HistogramTest) {
  base::HistogramTester histogram_tester;

  // Should see 0 Add Supervision enrollment metrics at first.
  histogram_tester.ExpectTotalCount("AddSupervisionDialog.Enrollment", 0);

  // Simulate the user opening the Add Supervision dialog and closing it.
  ShowAddSupervisionDialog();

  // Should see 1 Add Supervision process initiated.
  histogram_tester.ExpectUniqueSample(
      "AddSupervisionDialog.Enrollment",
      AddSupervisionMetricsRecorder::EnrollmentState::kInitiated, 1);
  histogram_tester.ExpectTotalCount("AddSupervisionDialog.Enrollment", 1);

  CloseAddSupervisionDialog();

  // Should see 1 Add Supervision process closed.
  histogram_tester.ExpectBucketCount(
      "AddSupervisionDialog.Enrollment",
      AddSupervisionMetricsRecorder::EnrollmentState::kClosed, 1);
  histogram_tester.ExpectTotalCount("AddSupervisionDialog.Enrollment", 2);

  // Simulate the user opening the Add Supervision dialog and signing out to
  // switch accounts.
  ShowAddSupervisionDialog();

  // Should see 2 Add Supervision processes initiated.
  histogram_tester.ExpectBucketCount(
      "AddSupervisionDialog.Enrollment",
      AddSupervisionMetricsRecorder::EnrollmentState::kInitiated, 2);
  histogram_tester.ExpectTotalCount("AddSupervisionDialog.Enrollment", 3);

  LogOutAndClose();

  // Should see 1 switch accounts attempt.
  histogram_tester.ExpectBucketCount(
      "AddSupervisionDialog.Enrollment",
      AddSupervisionMetricsRecorder::EnrollmentState::kSwitchedAccounts, 1);
  histogram_tester.ExpectTotalCount("AddSupervisionDialog.Enrollment", 4);

  // Simulate the user opening the Add Supervision dialog, enrolling in
  // supervision and signing out.
  ShowAddSupervisionDialog();

  // Should see 3 Add Supervision processes initiated.
  histogram_tester.ExpectBucketCount(
      "AddSupervisionDialog.Enrollment",
      AddSupervisionMetricsRecorder::EnrollmentState::kInitiated, 3);
  histogram_tester.ExpectTotalCount("AddSupervisionDialog.Enrollment", 5);

  NotifySupervisionEnabled();

  // Should see 1 Add Supervision process completed.
  histogram_tester.ExpectBucketCount(
      "AddSupervisionDialog.Enrollment",
      AddSupervisionMetricsRecorder::EnrollmentState::kCompleted, 1);
  histogram_tester.ExpectTotalCount("AddSupervisionDialog.Enrollment", 6);

  LogOutAndClose();

  // Should see 1 sign out attempt.
  histogram_tester.ExpectBucketCount(
      "AddSupervisionDialog.Enrollment",
      AddSupervisionMetricsRecorder::EnrollmentState::kSignedOut, 1);
  histogram_tester.ExpectTotalCount("AddSupervisionDialog.Enrollment", 7);
}

IN_PROC_BROWSER_TEST_F(AddSupervisionMetricsRecorderTest, UserActionTest) {
  base::UserActionTester user_action_tester;
  // Should see 0 user actions at first.
  EXPECT_EQ(user_action_tester.GetActionCount("AddSupervisionDialog_Launched"),
            0);
  EXPECT_EQ(user_action_tester.GetActionCount("AddSupervisionDialog_Closed"),
            0);
  EXPECT_EQ(user_action_tester.GetActionCount(
                "AddSupervisionDialog_AttemptedSignoutAfterEnrollment"),
            0);
  EXPECT_EQ(user_action_tester.GetActionCount(
                "AddSupervisionDialog_EnrollmentCompleted"),
            0);

  // Simulate the user opening the Add Supervision dialog and closing it.
  ShowAddSupervisionDialog();

  // Should see 1 Launched action.
  EXPECT_EQ(user_action_tester.GetActionCount("AddSupervisionDialog_Launched"),
            1);

  CloseAddSupervisionDialog();

  // Should see 1 Closed action.
  EXPECT_EQ(user_action_tester.GetActionCount("AddSupervisionDialog_Closed"),
            1);

  // Simulate the user opening the Add Supervision dialog and signing out to
  // switch accounts.
  ShowAddSupervisionDialog();

  // Should see 2 Launched actions.
  EXPECT_EQ(user_action_tester.GetActionCount("AddSupervisionDialog_Launched"),
            2);

  LogOutAndClose();

  // Should see 1 switch accounts attempt.
  EXPECT_EQ(user_action_tester.GetActionCount(
                "AddSupervisionDialog_SwitchedAccounts"),
            1);

  // Simulate the user opening the Add Supervision dialog, enrolling in
  // supervision and signing out.
  ShowAddSupervisionDialog();

  // Should see 3 Launched actions.
  EXPECT_EQ(user_action_tester.GetActionCount("AddSupervisionDialog_Launched"),
            3);

  NotifySupervisionEnabled();

  // Should see 1 EnrollmentCompleted action.
  EXPECT_EQ(user_action_tester.GetActionCount(
                "AddSupervisionDialog_EnrollmentCompleted"),
            1);

  LogOutAndClose();

  // Should see 1 AttemptedSignoutAfterEnrollment action.
  EXPECT_EQ(user_action_tester.GetActionCount(
                "AddSupervisionDialog_AttemptedSignoutAfterEnrollment"),
            1);
}

// This browser test class is for the Add Supervision timing metrics.
class AddSupervisionMetricsRecorderTimeTest
    : public AddSupervisionMetricsRecorderTest,
      public testing::WithParamInterface<int> {};

INSTANTIATE_TEST_SUITE_P(AddSupervisionDialogUserTimeInSeconds,
                         AddSupervisionMetricsRecorderTimeTest,
                         ::testing::Values(0, 11, 120, 1800));

IN_PROC_BROWSER_TEST_P(AddSupervisionMetricsRecorderTimeTest, UserTimingTest) {
  base::HistogramTester histogram_tester;

  // Should see 0 Add Supervision timing metrics at first.
  histogram_tester.ExpectTotalCount(
      "AddSupervisionDialog.EnrollmentNotCompletedUserTime", 0);
  histogram_tester.ExpectTotalCount(
      "AddSupervisionDialog.EnrollmentCompletedUserTime", 0);
  histogram_tester.ExpectTotalCount(
      "AddSupervisionDialog.SignoutCompletedUserTime", 0);

  // Simulate the user opening the Add Supervision dialog and closing it
  // after GetParam() seconds.
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_ =
      base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  AddSupervisionMetricsRecorder::GetInstance()->SetClockForTesting(
      task_runner_->GetMockTickClock());
  base::TimeDelta duration(base::Seconds(GetParam()));

  // We need to start at some non-zero point in time or else
  // DCHECK(!start_time_.is_null()) throws.
  task_runner_->FastForwardBy(base::Seconds(1));
  ShowAddSupervisionDialog();
  task_runner_->FastForwardBy(duration);
  CloseAddSupervisionDialog();

  // Should see 1 new EnrollmentNotCompletedUserTime timing.
  histogram_tester.ExpectTimeBucketCount(
      "AddSupervisionDialog.EnrollmentNotCompletedUserTime", duration, 1);
  histogram_tester.ExpectTotalCount(
      "AddSupervisionDialog.EnrollmentNotCompletedUserTime", 1);
  histogram_tester.ExpectTotalCount(
      "AddSupervisionDialog.EnrollmentCompletedUserTime", 0);
  histogram_tester.ExpectTotalCount(
      "AddSupervisionDialog.SignoutCompletedUserTime", 0);

  // Simulate the user opening the Add Supervision dialog and signing out to
  // switch accounts after GetParam() seconds.
  ShowAddSupervisionDialog();
  task_runner_->FastForwardBy(duration);
  LogOutAndClose();

  // Should see 1 new EnrollmentNotCompletedUserTime timing.
  histogram_tester.ExpectTimeBucketCount(
      "AddSupervisionDialog.EnrollmentNotCompletedUserTime", duration, 2);
  histogram_tester.ExpectTotalCount(
      "AddSupervisionDialog.EnrollmentNotCompletedUserTime", 2);
  histogram_tester.ExpectTotalCount(
      "AddSupervisionDialog.EnrollmentCompletedUserTime", 0);
  histogram_tester.ExpectTotalCount(
      "AddSupervisionDialog.SignoutCompletedUserTime", 0);

  // Simulate the user opening the Add Supervision dialog, enrolling in
  // supervision after GetParam() seconds and signing out after GetParam()
  // seconds.
  ShowAddSupervisionDialog();
  task_runner_->FastForwardBy(duration);
  NotifySupervisionEnabled();
  task_runner_->FastForwardBy(duration);
  LogOutAndClose();

  histogram_tester.ExpectTotalCount(
      "AddSupervisionDialog.EnrollmentNotCompletedUserTime", 2);
  // Should see 1 new EnrollmentCompletedUserTime timing.
  histogram_tester.ExpectTimeBucketCount(
      "AddSupervisionDialog.EnrollmentCompletedUserTime", duration, 1);
  histogram_tester.ExpectTotalCount(
      "AddSupervisionDialog.EnrollmentCompletedUserTime", 1);
  // Should see 1 new SignoutCompletedUserTime timing.
  histogram_tester.ExpectTimeBucketCount(
      "AddSupervisionDialog.SignoutCompletedUserTime", 2 * duration, 1);
  histogram_tester.ExpectTotalCount(
      "AddSupervisionDialog.SignoutCompletedUserTime", 1);
}

}  // namespace ash
