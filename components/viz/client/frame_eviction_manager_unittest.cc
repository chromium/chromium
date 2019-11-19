// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/client/frame_eviction_manager.h"

#include <algorithm>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace viz {

namespace {

class TestFrameEvictionManagerClient : public FrameEvictionManagerClient {
 public:
  TestFrameEvictionManagerClient() = default;
  ~TestFrameEvictionManagerClient() override = default;

  // FrameEvictionManagerClient:
  void EvictCurrentFrame() override {
    FrameEvictionManager::GetInstance()->RemoveFrame(this);
    has_frame_ = false;
  }

  bool has_frame() const { return has_frame_; }

 private:
  bool has_frame_ = true;

  DISALLOW_COPY_AND_ASSIGN(TestFrameEvictionManagerClient);
};

}  // namespace

using FrameEvictionManagerTest = testing::Test;

TEST_F(FrameEvictionManagerTest, ScopedPause) {
  constexpr int kMaxSavedFrames = 1;
  constexpr int kFrames = 2;

  FrameEvictionManager* manager = FrameEvictionManager::GetInstance();
  manager->set_max_number_of_saved_frames(kMaxSavedFrames);

  std::vector<TestFrameEvictionManagerClient> frames(kFrames);
  {
    FrameEvictionManager::ScopedPause scoped_pause;

    for (auto& frame : frames)
      manager->AddFrame(&frame, /*locked=*/false);

    // All frames stays because |scoped_pause| holds off frame eviction.
    EXPECT_EQ(kFrames,
              std::count_if(frames.begin(), frames.end(),
                            [](const TestFrameEvictionManagerClient& frame) {
                              return frame.has_frame();
                            }));
  }

  // Frame eviction happens when |scoped_pause| goes out of scope.
  EXPECT_EQ(kMaxSavedFrames,
            std::count_if(frames.begin(), frames.end(),
                          [](const TestFrameEvictionManagerClient& frame) {
                            return frame.has_frame();
                          }));
}

}  // namespace viz
