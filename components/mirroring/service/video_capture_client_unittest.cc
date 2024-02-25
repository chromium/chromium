// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mirroring/service/video_capture_client.h"

#include "base/functional/bind.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/mirroring/service/fake_video_capture_host.h"
#include "media/base/video_frame.h"
#include "media/base/video_frame_metadata.h"
#include "media/capture/mojom/video_capture_buffer.mojom.h"
#include "media/capture/mojom/video_capture_types.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::InvokeWithoutArgs;
using ::testing::_;

namespace mirroring {

namespace {

const media::VideoCaptureFeedback kFeedback(0.6, 30.0, 1000);

constexpr bool kNotPremapped = false;

media::mojom::VideoFrameInfoPtr GetVideoFrameInfo(const gfx::Size& size) {
  media::VideoFrameMetadata metadata;
  metadata.frame_rate = 30;
  metadata.reference_time = base::TimeTicks();
  return media::mojom::VideoFrameInfo::New(
      base::TimeDelta(), metadata, media::PIXEL_FORMAT_I420, size,
      gfx::Rect(size), kNotPremapped, gfx::ColorSpace::CreateREC709(), nullptr);
}

}  // namespace

class VideoCaptureClientTest : public ::testing::Test,
                               public ::testing::WithParamInterface<bool> {
 public:
  VideoCaptureClientTest() {
    mojo::PendingRemote<media::mojom::VideoCaptureHost> host;
    host_impl_ = std::make_unique<FakeVideoCaptureHost>(
        host.InitWithNewPipeAndPassReceiver());
    client_ = std::make_unique<VideoCaptureClient>(media::VideoCaptureParams(),
                                                   std::move(host));
  }

  VideoCaptureClientTest(const VideoCaptureClientTest&) = delete;
  VideoCaptureClientTest& operator=(const VideoCaptureClientTest&) = delete;

  ~VideoCaptureClientTest() override {
    if (client_) {
      base::RunLoop run_loop;
      EXPECT_CALL(*host_impl_, OnStopped())
          .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
      client_->Stop();
      run_loop.Run();
    }
    task_environment_.RunUntilIdle();
  }

  MOCK_METHOD1(OnFrameReceived, void(const gfx::Size&));
  void OnFrameReady(scoped_refptr<media::VideoFrame> video_frame) {
    client_->ProcessFeedback(kFeedback);
    OnFrameReceived(video_frame->coded_size());
  }

 protected:
  void StartCapturing() {
    EXPECT_CALL(error_cb_, Run()).Times(0);
    base::RunLoop run_loop;
    // Expect to call RequestRefreshFrame() after capturing started.
    EXPECT_CALL(*host_impl_, RequestRefreshFrame(_))
        .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    client_->Start(base::BindRepeating(&VideoCaptureClientTest::OnFrameReady,
                                       base::Unretained(this)),
                   error_cb_.Get());
    run_loop.Run();
    task_environment_.RunUntilIdle();
  }

  void OnNewBuffer(int buffer_id, int buffer_size) {
    EXPECT_CALL(error_cb_, Run()).Times(0);
    const bool use_shared_buffer = GetParam();
    if (use_shared_buffer) {
      client_->OnNewBuffer(
          buffer_id, media::mojom::VideoBufferHandle::NewUnsafeShmemRegion(
                         base::UnsafeSharedMemoryRegion::Create(buffer_size)));
    } else {
      client_->OnNewBuffer(
          buffer_id,
          media::mojom::VideoBufferHandle::NewReadOnlyShmemRegion(
              base::ReadOnlySharedMemoryRegion::Create(buffer_size).region));
    }
    task_environment_.RunUntilIdle();
  }

  void OnBufferReady(int buffer_id, const gfx::Size& frame_size) {
    EXPECT_CALL(error_cb_, Run()).Times(0);
    base::RunLoop run_loop;
    // Expects to receive one frame.
    EXPECT_CALL(*this, OnFrameReceived(frame_size)).Times(1);
    // Expects to return the buffer after the frame is consumed.
    EXPECT_CALL(*host_impl_, ReleaseBuffer(_, 0, kFeedback))
        .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    client_->OnBufferReady(media::mojom::ReadyBuffer::New(
        buffer_id, GetVideoFrameInfo(frame_size)));
    run_loop.Run();
    task_environment_.RunUntilIdle();
  }

 private:
  base::test::TaskEnvironment task_environment_;
  base::MockCallback<base::OnceClosure> error_cb_;
  std::unique_ptr<FakeVideoCaptureHost> host_impl_;
  std::unique_ptr<VideoCaptureClient> client_;
};

TEST_P(VideoCaptureClientTest, Basic) {
  StartCapturing();

  // A new buffer is created.
  OnNewBuffer(0, 100000);

  // One captured frame is ready. Expects to receive the frame.
  OnBufferReady(0, gfx::Size(126, 64));

  // A smaller size video frame is received in the same buffer.
  OnBufferReady(0, gfx::Size(64, 32));

  // A larger size video frame is received in the same buffer.
  OnBufferReady(0, gfx::Size(320, 180));
}

INSTANTIATE_TEST_SUITE_P(All,
                         VideoCaptureClientTest,
                         ::testing::Values(true, false));

}  // namespace mirroring
