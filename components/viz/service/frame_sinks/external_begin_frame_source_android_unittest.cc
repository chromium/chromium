// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/frame_sinks/external_begin_frame_source_android.h"

#include "base/android/java_handler_thread.h"
#include "base/bind.h"
#include "base/synchronization/waitable_event.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace viz {

class ExternalBeginFrameSourceAndroidTest : public ::testing::Test,
                                            public BeginFrameObserverBase {
 public:
  ~ExternalBeginFrameSourceAndroidTest() override { thread_->Stop(); }

  void CreateThread() {
    thread_ = std::make_unique<base::android::JavaHandlerThread>("TestThread");
    thread_->Start();

    thread_->task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&ExternalBeginFrameSourceAndroidTest::InitOnThread,
                       base::Unretained(this)));
  }

  void WaitForFrames(uint32_t frame_count) {
    frames_done_event_.Reset();
    thread_->task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &ExternalBeginFrameSourceAndroidTest::AddObserverOnThread,
            base::Unretained(this), frame_count));
    frames_done_event_.Wait();
  }

  ExternalBeginFrameSourceAndroid* begin_frame_source() {
    return begin_frame_source_.get();
  }

 private:
  void InitOnThread() {
    begin_frame_source_ = std::make_unique<ExternalBeginFrameSourceAndroid>(
        BeginFrameSource::kNotRestartableId, 60.f);
  }

  void AddObserverOnThread(uint32_t frame_count) {
    pending_frames_ = frame_count;
    begin_frame_source_->AddObserver(this);
  }

  bool OnBeginFrameDerivedImpl(const BeginFrameArgs& args) override {
    if (pending_frames_ == 0)
      return false;

    if (--pending_frames_ == 0) {
      begin_frame_source_->RemoveObserver(this);
      frames_done_event_.Signal();
    }
    return true;
  }
  void OnBeginFrameSourcePausedChanged(bool paused) override {}

  base::WaitableEvent frames_done_event_;
  std::unique_ptr<base::android::JavaHandlerThread> thread_;

  // Only accessed from TestThread.
  std::unique_ptr<ExternalBeginFrameSourceAndroid> begin_frame_source_;
  uint32_t pending_frames_ = 0;
};

TEST_F(ExternalBeginFrameSourceAndroidTest, DeliversFrames) {
  CreateThread();
  // Ensure we receive frames. When this returns we are no longer observing the
  // BeginFrameSource.
  WaitForFrames(10);
  // Ensure we can re-observe the same BeginFrameSource and get more frames.
  WaitForFrames(10);
}

TEST_F(ExternalBeginFrameSourceAndroidTest, DeliversFramesAfterIntervalChange) {
  CreateThread();
  // Ensure we receive frames. When this returns we are no longer observing the
  // BeginFrameSource.
  WaitForFrames(10);
  begin_frame_source()->UpdateRefreshRate(30.f);
  // Ensure we can re-observe the same BeginFrameSource and get more frames.
  WaitForFrames(10);
}

}  // namespace viz
