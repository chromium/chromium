// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/frame_deadline_decider.h"

#include <memory>
#include <utility>
#include <vector>

#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace viz {
namespace {

class FrameDeadlineDeciderTest : public testing::Test {
 public:
  FrameDeadlineDeciderTest() = default;
  ~FrameDeadlineDeciderTest() override = default;

 protected:
  FrameDeadlineDecider decider_;
};

PossibleDeadlines CreatePossibleDeadlines(
    size_t preferred_index,
    std::vector<PossibleDeadline> deadlines) {
  PossibleDeadlines possible_deadlines(preferred_index);
  possible_deadlines.deadlines = std::move(deadlines);
  return possible_deadlines;
}

}  // namespace

TEST_F(FrameDeadlineDeciderTest, FallbackToPreferred) {
  // OS Preferred: latch = 4ms, present = 12ms (index 0)
  // Custom: latch = 32ms, present = 40ms (index 1)
  auto deadlines = CreatePossibleDeadlines(
      0, {
             PossibleDeadline(1, base::Milliseconds(4),
                              base::Milliseconds(12)),  // OS preferred
             PossibleDeadline(2, base::Milliseconds(32),
                              base::Milliseconds(40))  // Custom target
         });

  auto selected = decider_.SelectDeadline(deadlines);
  EXPECT_EQ(base::Milliseconds(4), selected.latch_delta);
  EXPECT_EQ(base::Milliseconds(12), selected.present_delta);
  EXPECT_EQ(1, selected.vsync_id);
}

}  // namespace viz
