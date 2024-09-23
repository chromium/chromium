// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/shell_surface_presentation_time_recorder.h"

#include <utility>
#include <vector>

#include "base/auto_reset.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "components/exo/test/exo_test_base.h"
#include "components/exo/test/shell_surface_builder.h"
#include "components/exo/test/surface_tree_host_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/gfx/presentation_feedback.h"

using testing::ElementsAre;

namespace exo {
namespace {

class TestReporter : public ShellSurfacePresentationTimeRecorder::Reporter {
 public:
  TestReporter() = default;
  ~TestReporter() override = default;

  // ShellSurfacePresentationTimeRecorder::Reporter:
  void ReportTime(base::TimeDelta delta) override { ++report_count_; }

  int GetReportCountAndReset() {
    int count = report_count_;
    report_count_ = 0;
    return count;
  }

 private:
  int report_count_ = 0;
};

class TestRecorder : public ShellSurfacePresentationTimeRecorder {
 public:
  TestRecorder(ShellSurface* shell_surface, std::unique_ptr<Reporter> reporter)
      : ShellSurfacePresentationTimeRecorder(shell_surface,
                                             std::move(reporter)) {}

  // ShellSurfacePresentationTimeRecorder:
  void OnFramePresented(const Request& request,
                        const gfx::PresentationFeedback& feedback) override {
    presented_serials_.push_back(request.serial.value());
    ShellSurfacePresentationTimeRecorder::OnFramePresented(request,
                                                           fake_feedback_);
    if (run_loop_)
      run_loop_->Quit();
  }

  void WaitForFramePresented() {
    base::RunLoop run_loop;

    base::AutoReset<raw_ptr<base::RunLoop>> scoped(&run_loop_, &run_loop);
    run_loop.Run();
  }

  std::vector<uint32_t> TakePresentedSerials() {
    return std::move(presented_serials_);
  }

  void set_fake_feedback(const gfx::PresentationFeedback& fake_feedback) {
    fake_feedback_ = fake_feedback;
  }

 private:
  gfx::PresentationFeedback fake_feedback_;
  raw_ptr<base::RunLoop> run_loop_ = nullptr;
  std::vector<uint32_t> presented_serials_;
};

}  // namespace

class ShellSurfacePresentationTimeRecorderTest : public test::ExoTestBase {
 public:
  ShellSurfacePresentationTimeRecorderTest() = default;
  ~ShellSurfacePresentationTimeRecorderTest() override = default;

  // test::ExoTestBase:
  void SetUp() override {
    test::ExoTestBase::SetUp();

    shell_surface_ = test::ShellSurfaceBuilder({32, 32}).BuildShellSurface();

    auto reporter = std::make_unique<TestReporter>();
    reporter_ = reporter.get();
    recorder_ = std::make_unique<TestRecorder>(shell_surface_.get(),
                                               std::move(reporter));
  }
  void TearDown() override {
    shell_surface_.reset();

    test::ExoTestBase::TearDown();
  }

  void FakeFrameSubmitAndPresent() {
    base::TimeDelta interval_not_used = base::Milliseconds(0);
    // Create feedback with an extra 1s to ensure that presentation timestamp
    // is later than the request time on slow bots. Otherwise, the presentation
    // would not be reported and fail test expectations.
    gfx::PresentationFeedback feedback(
        base::TimeTicks().Now() + base::Milliseconds(1000), interval_not_used,
        /*flags=*/0);
    recorder_->set_fake_feedback(feedback);

    // Fake damage so that the committed frame will generate a presentation
    // feedback when the next DrawAndSwap happens. Without damage the
    // presentation feedback could be delayed till the next frame submission.
    root_surface()->Damage(gfx::Rect(0, 0, 32, 32));
    root_surface()->Commit();
    recorder_->WaitForFramePresented();
  }

  Surface* root_surface() { return shell_surface_->root_surface(); }

 protected:
  std::unique_ptr<ShellSurface> shell_surface_;
  std::unique_ptr<TestRecorder> recorder_;
  raw_ptr<TestReporter, DanglingUntriaged> reporter_ = nullptr;
};

TEST_F(ShellSurfacePresentationTimeRecorderTest, Request) {
  // Request without "config" fails.
  recorder_->PrepareToRecord();
  EXPECT_FALSE(recorder_->RequestNext());

  // Fake a "Connfigure".
  recorder_->OnConfigure(1u);

  // Request should succeed.
  EXPECT_TRUE(recorder_->RequestNext());
}

TEST_F(ShellSurfacePresentationTimeRecorderTest, AckSkippedOrOutOfOrder) {
  // Issue 4 requests with configure serial 1-5.
  for (size_t i = 1u; i <= 5u; ++i) {
    recorder_->PrepareToRecord();
    recorder_->OnConfigure(i);
    ASSERT_TRUE(recorder_->RequestNext());
  }

  // Ack 2 and skip 1.
  recorder_->OnAcknowledgeConfigure(2u);

  // FramePrsented would be reported for 1 and 2, even though 1 is not acked.
  FakeFrameSubmitAndPresent();
  EXPECT_THAT(recorder_->TakePresentedSerials(), ElementsAre(1, 2));
  EXPECT_EQ(2, reporter_->GetReportCountAndReset());

  // Ack 4 and 3 out of order.
  recorder_->OnAcknowledgeConfigure(4u);
  recorder_->OnAcknowledgeConfigure(3u);
  recorder_->OnAcknowledgeConfigure(5u);

  // FramePresented would be reported for 3, 4, and 5, even though 3 and 4
  // is acked out of order.
  FakeFrameSubmitAndPresent();
  EXPECT_THAT(recorder_->TakePresentedSerials(), ElementsAre(3, 4, 5));
  EXPECT_EQ(3, reporter_->GetReportCountAndReset());
}

TEST_F(ShellSurfacePresentationTimeRecorderTest,
       RecorderDestroyedBeforePresent) {
  // Create a pending request on the recorder.
  recorder_->PrepareToRecord();
  recorder_->OnConfigure(1u);
  EXPECT_TRUE(recorder_->RequestNext());
  recorder_->OnAcknowledgeConfigure(1u);

  // `recorder_` is gone before frame submission and presentation. `reporter_`
  // is owned by `recorder_` so clear its reference too.
  recorder_.reset();
  reporter_ = nullptr;

  // Fake frame submission. No FakeFrameSubmitAndPresent() because it depends
  // on `recorder_`.
  root_surface()->Damage(gfx::Rect(0, 0, 32, 32));
  root_surface()->Commit();
  test::WaitForLastFramePresentation(shell_surface_.get());
}

TEST_F(ShellSurfacePresentationTimeRecorderTest,
       ShellSurfaceDestroyedBeforeRecorder) {
  // Create a pending request on the recorder.
  recorder_->PrepareToRecord();
  recorder_->OnConfigure(1u);
  EXPECT_TRUE(recorder_->RequestNext());
  recorder_->OnAcknowledgeConfigure(1u);

  // ShellSurface gets destroyed before recorder.
  shell_surface_.reset();
}

}  // namespace exo
