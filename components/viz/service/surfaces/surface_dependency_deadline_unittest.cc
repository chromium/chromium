// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/surfaces/surface_dependency_deadline.h"
#include "base/test/simple_test_tick_clock.h"
#include "components/viz/common/quads/frame_deadline.h"
#include "components/viz/service/surfaces/surface_deadline_client.h"
#include "components/viz/test/begin_frame_args_test.h"
#include "components/viz/test/fake_external_begin_frame_source.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace viz {

class FakeSurfaceDeadlineClient : public SurfaceDeadlineClient {
 public:
  FakeSurfaceDeadlineClient() = default;
  ~FakeSurfaceDeadlineClient() = default;

  // SurfaceDeadlineClient implementation:
  void OnDeadline(base::TimeDelta duration) override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeSurfaceDeadlineClient);
};

class FakeSlowBeginFrameSource : public FakeExternalBeginFrameSource {
 public:
  FakeSlowBeginFrameSource(double refresh_rate,
                           bool tick_automatically,
                           base::SimpleTestTickClock* tick_clock)
      : FakeExternalBeginFrameSource(refresh_rate, tick_automatically),
        tick_clock_(tick_clock) {}

  ~FakeSlowBeginFrameSource() override {}

  // FakeExternalBeginFrameSource overrides:
  void AddObserver(BeginFrameObserver* obs) override {
    // Advancing time here simulates a slow AddObserver operation.
    tick_clock_->Advance(BeginFrameArgs::DefaultInterval());
    FakeExternalBeginFrameSource::AddObserver(obs);
  }

 private:
  base::SimpleTestTickClock* const tick_clock_;
  DISALLOW_COPY_AND_ASSIGN(FakeSlowBeginFrameSource);
};

class SurfaceDependencyDeadlineTest : public testing::Test {
 public:
  SurfaceDependencyDeadlineTest() = default;

  ~SurfaceDependencyDeadlineTest() override {}

  FakeSlowBeginFrameSource* begin_frame_source() {
    return begin_frame_source_.get();
  }

  FrameDeadline MakeDefaultDeadline() {
    return FrameDeadline(now_src_->NowTicks(), 4u,
                         BeginFrameArgs::DefaultInterval(), false);
  }

  SurfaceDependencyDeadline* deadline() { return deadline_.get(); }

  SurfaceDependencyDeadline* deadline2() { return deadline2_.get(); }

  void SendLateBeginFrame(uint32_t frames_late) {
    // Creep the time forward so that any BeginFrameArgs is not equal to the
    // last one otherwise we violate the BeginFrameSource contract.
    now_src_->Advance(frames_late * BeginFrameArgs::DefaultInterval());
    BeginFrameArgs args = begin_frame_source_->CreateBeginFrameArgs(
        BEGINFRAME_FROM_HERE, now_src_.get());
    begin_frame_source_->TestOnBeginFrame(args);
  }

  // testing::Test:
  void SetUp() override {
    testing::Test::SetUp();

    now_src_ = std::make_unique<base::SimpleTestTickClock>();
    begin_frame_source_ =
        std::make_unique<FakeSlowBeginFrameSource>(0.f, false, now_src_.get());

    deadline_ = std::make_unique<SurfaceDependencyDeadline>(
        &client_, begin_frame_source_.get(), now_src_.get());

    deadline2_ = std::make_unique<SurfaceDependencyDeadline>(
        &client_, begin_frame_source_.get(), now_src_.get());
  }

  void TearDown() override {
    deadline2_->Cancel();
    deadline2_.reset();
    deadline_->Cancel();
    deadline_.reset();
    begin_frame_source_.reset();
    now_src_.reset();
  }

 private:
  std::unique_ptr<base::SimpleTestTickClock> now_src_;
  std::unique_ptr<FakeSlowBeginFrameSource> begin_frame_source_;
  FakeSurfaceDeadlineClient client_;
  std::unique_ptr<SurfaceDependencyDeadline> deadline_;
  std::unique_ptr<SurfaceDependencyDeadline> deadline2_;

  DISALLOW_COPY_AND_ASSIGN(SurfaceDependencyDeadlineTest);
};

// This test verifies that if the FrameDeadline is in the past then
// SurfaceDependencyDeadline::Set will return false.
TEST_F(SurfaceDependencyDeadlineTest, DeadlineInPast) {
  FrameDeadline frame_deadline = MakeDefaultDeadline();
  SendLateBeginFrame(4u);
  EXPECT_FALSE(deadline()->Set(frame_deadline));
  EXPECT_FALSE(deadline()->has_deadline());
}

// This test verifies that if Set returns false, then SurfaceDependencyDeadline
// does not have a pending deadline.
TEST_F(SurfaceDependencyDeadlineTest, SetMatchesHasDeadlineIfFalse) {
  FrameDeadline frame_deadline = MakeDefaultDeadline();
  SendLateBeginFrame(3u);
  EXPECT_FALSE(deadline()->Set(frame_deadline));
  EXPECT_FALSE(deadline()->has_deadline());
}

// This test verifies that if Set returns true, then SurfaceDependencyDeadline
// has a pending deadline.
TEST_F(SurfaceDependencyDeadlineTest, SetMatchesHasDeadlineIfTrue) {
  FrameDeadline frame_deadline = MakeDefaultDeadline();
  SendLateBeginFrame(2u);
  EXPECT_TRUE(deadline()->Set(frame_deadline));
  EXPECT_TRUE(deadline()->has_deadline());
}

// This test verifies that inheriting a deadline with no pre-existing deadline
// sets up the start time of the event to the time of inheritance.
TEST_F(SurfaceDependencyDeadlineTest, InheritDeadline) {
  FrameDeadline frame_deadline = MakeDefaultDeadline();
  SendLateBeginFrame(1u);
  EXPECT_TRUE(deadline()->Set(frame_deadline));
  EXPECT_TRUE(deadline()->has_deadline());

  SendLateBeginFrame(1u);
  EXPECT_FALSE(deadline2()->has_deadline());
  deadline2()->InheritFrom(*deadline());
  EXPECT_TRUE(deadline()->has_deadline());
  EXPECT_EQ(deadline()->deadline_for_testing(),
            deadline2()->deadline_for_testing());

  base::Optional<base::TimeDelta> duration1 = deadline()->Cancel();
  base::Optional<base::TimeDelta> duration2 = deadline2()->Cancel();
  ASSERT_TRUE(duration1.has_value());
  ASSERT_TRUE(duration2.has_value());

  // We inject time on BeginFrameSource::AddObserver and in practice we cannot
  // know the exact difference in duration between two events a priori so we
  // just verify that the first event was longer than the second.
  EXPECT_GT(duration1, duration2);
}

// This test verifies that if an active deadline object inherits a deadline
// from another object, it does not inherit the start time of the event.
TEST_F(SurfaceDependencyDeadlineTest, InheritDeadlineWithActiveDeadline) {
  {
    FrameDeadline frame_deadline = MakeDefaultDeadline();
    SendLateBeginFrame(1u);
    EXPECT_TRUE(deadline()->Set(frame_deadline));
    EXPECT_TRUE(deadline()->has_deadline());
  }

  {
    FrameDeadline frame_deadline = MakeDefaultDeadline();
    SendLateBeginFrame(1u);
    // deadline2's start time is later than deadline2.
    EXPECT_TRUE(deadline2()->Set(frame_deadline));
    EXPECT_TRUE(deadline2()->has_deadline());
  }

  deadline()->InheritFrom(*deadline2());
  EXPECT_TRUE(deadline()->has_deadline());
  EXPECT_EQ(deadline()->deadline_for_testing(),
            deadline2()->deadline_for_testing());

  base::Optional<base::TimeDelta> duration1 = deadline()->Cancel();
  base::Optional<base::TimeDelta> duration2 = deadline2()->Cancel();
  ASSERT_TRUE(duration1.has_value());
  ASSERT_TRUE(duration2.has_value());

  // We inject time on BeginFrameSource::AddObserver and in practice we cannot
  // know the exact difference in duration between two events a priori so we
  // just verify that the first event was longer than the second.
  EXPECT_GT(duration1, duration2);
}

}  // namespace viz
