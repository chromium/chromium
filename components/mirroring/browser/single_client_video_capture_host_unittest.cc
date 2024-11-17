// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mirroring/browser/single_client_video_capture_host.h"

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/task_environment.h"
#include "base/token.h"
#include "media/capture/mojom/video_capture_types.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/video_effects/public/mojom/video_effects_processor.mojom-forward.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::InvokeWithoutArgs;
using media::VideoFrameReceiver;

namespace mirroring {

namespace {

constexpr bool kNotPremapped = false;

class MockVideoCaptureDevice final
    : public content::LaunchedVideoCaptureDevice {
 public:
  MockVideoCaptureDevice() = default;

  MockVideoCaptureDevice(const MockVideoCaptureDevice&) = delete;
  MockVideoCaptureDevice& operator=(const MockVideoCaptureDevice&) = delete;

  ~MockVideoCaptureDevice() override = default;
  void GetPhotoState(
      VideoCaptureDevice::GetPhotoStateCallback callback) override {}
  void SetPhotoOptions(
      media::mojom::PhotoSettingsPtr settings,
      VideoCaptureDevice::SetPhotoOptionsCallback callback) override {}
  void TakePhoto(VideoCaptureDevice::TakePhotoCallback callback) override {}
  void SetDesktopCaptureWindowIdAsync(gfx::NativeViewId window_id,
                                      base::OnceClosure done_cb) override {}
  MOCK_METHOD0(MaybeSuspendDevice, void());
  MOCK_METHOD0(ResumeDevice, void());
  MOCK_METHOD4(
      ApplySubCaptureTarget,
      void(
          media::mojom::SubCaptureTargetType,
          const base::Token&,
          uint32_t,
          base::OnceCallback<void(media::mojom::ApplySubCaptureTargetResult)>));
  MOCK_METHOD0(RequestRefreshFrame, void());
  MOCK_METHOD1(OnUtilizationReport, void(media::VideoCaptureFeedback));
};

class FakeDeviceLauncher final : public content::VideoCaptureDeviceLauncher {
 public:
  using DeviceLaunchedCallback =
      base::OnceCallback<void(base::WeakPtr<VideoFrameReceiver>,
                              MockVideoCaptureDevice*)>;

  explicit FakeDeviceLauncher(DeviceLaunchedCallback launched_cb)
      : after_launch_cb_(std::move(launched_cb)) {}

  FakeDeviceLauncher(const FakeDeviceLauncher&) = delete;
  FakeDeviceLauncher& operator=(const FakeDeviceLauncher&) = delete;

  ~FakeDeviceLauncher() override = default;

  // content::VideoCaptureDeviceLauncher implementation.
  void LaunchDeviceAsync(
      const std::string& device_id,
      blink::mojom::MediaStreamType stream_type,
      const VideoCaptureParams& params,
      base::WeakPtr<VideoFrameReceiver> receiver,
      base::OnceClosure connection_lost_cb,
      Callbacks* callbacks,
      base::OnceClosure done_cb,
      mojo::PendingRemote<video_effects::mojom::VideoEffectsProcessor>
          video_effects_processor) override {
    if (!params.IsValid()) {
      callbacks->OnDeviceLaunchFailed(
          media::VideoCaptureError::
              kVideoCaptureControllerInvalidOrUnsupportedVideoCaptureParametersRequested);
      return;
    }
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&FakeDeviceLauncher::OnDeviceLaunched,
                                  weak_factory_.GetWeakPtr(), receiver,
                                  callbacks, std::move(done_cb)));
  }

  void AbortLaunch() override {}

 private:
  void OnDeviceLaunched(base::WeakPtr<VideoFrameReceiver> receiver,
                        VideoCaptureDeviceLauncher::Callbacks* callbacks,
                        base::OnceClosure done_cb) {
    auto launched_device = std::make_unique<MockVideoCaptureDevice>();
    EXPECT_FALSE(after_launch_cb_.is_null());
    std::move(after_launch_cb_).Run(receiver, launched_device.get());
    callbacks->OnDeviceLaunched(std::move(launched_device));
    std::move(done_cb).Run();
  }

  DeviceLaunchedCallback after_launch_cb_;
  base::WeakPtrFactory<FakeDeviceLauncher> weak_factory_{this};
};

class StubReadWritePermission final
    : public VideoCaptureDevice::Client::Buffer::ScopedAccessPermission {
 public:
  StubReadWritePermission() = default;

  StubReadWritePermission(const StubReadWritePermission&) = delete;
  StubReadWritePermission& operator=(const StubReadWritePermission&) = delete;

  ~StubReadWritePermission() override = default;
};

class MockVideoCaptureObserver final
    : public media::mojom::VideoCaptureObserver {
 public:
  explicit MockVideoCaptureObserver(
      mojo::PendingRemote<media::mojom::VideoCaptureHost> host)
      : host_(std::move(host)) {}

  MockVideoCaptureObserver(const MockVideoCaptureObserver&) = delete;
  MockVideoCaptureObserver& operator=(const MockVideoCaptureObserver&) = delete;

  MOCK_METHOD1(OnBufferCreatedCall, void(int buffer_id));
  void OnNewBuffer(int32_t buffer_id,
                   media::mojom::VideoBufferHandlePtr buffer_handle) override {
    EXPECT_EQ(buffers_.find(buffer_id), buffers_.end());
    EXPECT_EQ(frame_infos_.find(buffer_id), frame_infos_.end());
    buffers_[buffer_id] = std::move(buffer_handle);
    OnBufferCreatedCall(buffer_id);
  }
  MOCK_METHOD1(OnBufferReadyCall, void(int buffer_id));
  void OnBufferReady(media::mojom::ReadyBufferPtr buffer) override {
    EXPECT_TRUE(buffers_.find(buffer->buffer_id) != buffers_.end());
    EXPECT_EQ(frame_infos_.find(buffer->buffer_id), frame_infos_.end());
    frame_infos_[buffer->buffer_id] = std::move(buffer->info);
    OnBufferReadyCall(buffer->buffer_id);
  }

  MOCK_METHOD1(OnBufferDestroyedCall, void(int buffer_id));
  void OnBufferDestroyed(int32_t buffer_id) override {
    // The consumer should have finished consuming the buffer before it is being
    // destroyed.
    EXPECT_TRUE(frame_infos_.find(buffer_id) == frame_infos_.end());
    const auto iter = buffers_.find(buffer_id);
    EXPECT_TRUE(iter != buffers_.end());
    buffers_.erase(iter);
    OnBufferDestroyedCall(buffer_id);
  }
  MOCK_METHOD1(OnFrameDropped, void(media::VideoCaptureFrameDropReason reason));

  MOCK_METHOD1(OnNewSubCaptureTargetVersion,
               void(uint32_t sub_capture_target_version));

  MOCK_METHOD1(OnStateChangedCall, void(media::mojom::VideoCaptureState state));
  MOCK_METHOD1(OnVideoCaptureErrorCall, void(media::VideoCaptureError error));
  void OnStateChanged(media::mojom::VideoCaptureResultPtr result) override {
    if (result->which() == media::mojom::VideoCaptureResult::Tag::kState)
      OnStateChangedCall(result->get_state());
    else
      OnVideoCaptureErrorCall(result->get_error_code());
  }

  void Start(bool valid_params) {
    VideoCaptureParams params = VideoCaptureParams();
    if (!valid_params)
      params.requested_format.frame_rate = std::numeric_limits<float>::max();

    host_->Start(device_id_, session_id_, params,
                 receiver_.BindNewPipeAndPassRemote());
  }

  void FinishConsumingBuffer(int32_t buffer_id,
                             media::VideoCaptureFeedback feedback) {
    EXPECT_TRUE(buffers_.find(buffer_id) != buffers_.end());
    const auto iter = frame_infos_.find(buffer_id);
    EXPECT_TRUE(iter != frame_infos_.end());
    frame_infos_.erase(iter);
    host_->ReleaseBuffer(device_id_, buffer_id, feedback);
  }

  void Stop() { host_->Stop(device_id_); }

 private:
  const base::UnguessableToken device_id_ = base::UnguessableToken::Create();
  const base::UnguessableToken session_id_ = base::UnguessableToken::Create();
  mojo::Remote<media::mojom::VideoCaptureHost> host_;
  mojo::Receiver<media::mojom::VideoCaptureObserver> receiver_{this};
  base::flat_map<int, media::mojom::VideoBufferHandlePtr> buffers_;
  base::flat_map<int, media::mojom::VideoFrameInfoPtr> frame_infos_;
};

media::mojom::VideoFrameInfoPtr GetVideoFrameInfo() {
  return media::mojom::VideoFrameInfo::New(
      base::TimeDelta(), media::VideoFrameMetadata(), media::PIXEL_FORMAT_I420,
      gfx::Size(320, 180), gfx::Rect(320, 180), kNotPremapped,
      gfx::ColorSpace::CreateREC709(), nullptr);
}

}  // namespace

class SingleClientVideoCaptureHostTest : public ::testing::Test {
 public:
  SingleClientVideoCaptureHostTest() = default;

  void CustomSetUp(bool valid_params = true) {
    auto host_impl = std::make_unique<SingleClientVideoCaptureHost>(
        std::string(), blink::mojom::MediaStreamType::GUM_TAB_VIDEO_CAPTURE,
        base::BindRepeating(
            &SingleClientVideoCaptureHostTest::CreateDeviceLauncher,
            base::Unretained(this)));
    mojo::PendingRemote<media::mojom::VideoCaptureHost> host;
    mojo::MakeSelfOwnedReceiver(std::move(host_impl),
                                host.InitWithNewPipeAndPassReceiver());
    consumer_ = std::make_unique<MockVideoCaptureObserver>(std::move(host));
    base::RunLoop run_loop;
    if (valid_params) {
      EXPECT_CALL(*this, OnDeviceLaunchedCall())
          .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    } else {
      EXPECT_CALL(
          *consumer_,
          OnVideoCaptureErrorCall(
              media::VideoCaptureError::
                  kVideoCaptureControllerInvalidOrUnsupportedVideoCaptureParametersRequested))
          .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    }
    consumer_->Start(valid_params);
    run_loop.Run();

    if (valid_params) {
      // The video capture device is launched.
      EXPECT_TRUE(launched_device_);
      EXPECT_TRUE(frame_receiver_);
    }
  }

  ~SingleClientVideoCaptureHostTest() override {
    base::RunLoop run_loop;
    EXPECT_CALL(*consumer_,
                OnStateChangedCall(media::mojom::VideoCaptureState::ENDED))
        .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    consumer_->Stop();

    launched_device_ = nullptr;
    run_loop.Run();
  }

 protected:
  void CreateBuffer(int buffer_id, int expected_buffer_context_id) {
    base::RunLoop run_loop;
    EXPECT_CALL(*consumer_, OnBufferCreatedCall(expected_buffer_context_id))
        .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    media::mojom::VideoBufferHandlePtr stub_buffer_handle =
        media::mojom::VideoBufferHandle::NewUnsafeShmemRegion(
            base::UnsafeSharedMemoryRegion::Create(10));
    frame_receiver_->OnNewBuffer(buffer_id, std::move(stub_buffer_handle));
    run_loop.Run();
  }

  void FrameReadyInBuffer(int buffer_id,
                          int buffer_context_id,
                          int feedback_id) {
    base::RunLoop run_loop;
    EXPECT_CALL(*consumer_, OnBufferReadyCall(buffer_context_id))
        .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    frame_receiver_->OnFrameReadyInBuffer(media::ReadyFrameInBuffer(
        buffer_id, feedback_id, std::make_unique<StubReadWritePermission>(),
        GetVideoFrameInfo()));
    run_loop.Run();
  }

  void FinishConsumingBuffer(int buffer_context_id,
                             const media::VideoCaptureFeedback& feedback) {
    base::RunLoop run_loop;
    EXPECT_CALL(*launched_device_, OnUtilizationReport(feedback))
        .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    consumer_->FinishConsumingBuffer(buffer_context_id, feedback);
    run_loop.Run();
  }

  void RetireBuffer(int buffer_id, int buffer_context_id) {
    base::RunLoop run_loop;
    EXPECT_CALL(*consumer_, OnBufferDestroyedCall(buffer_context_id))
        .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    frame_receiver_->OnBufferRetired(buffer_id);
    run_loop.Run();
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<MockVideoCaptureObserver> consumer_;
  base::WeakPtr<VideoFrameReceiver> frame_receiver_;
  raw_ptr<MockVideoCaptureDevice> launched_device_ = nullptr;

 private:
  std::unique_ptr<content::VideoCaptureDeviceLauncher> CreateDeviceLauncher() {
    return std::make_unique<FakeDeviceLauncher>(
        base::BindOnce(&SingleClientVideoCaptureHostTest::OnDeviceLaunched,
                       weak_factory_.GetWeakPtr()));
  }

  MOCK_METHOD0(OnDeviceLaunchedCall, void());
  void OnDeviceLaunched(base::WeakPtr<VideoFrameReceiver> receiver,
                        MockVideoCaptureDevice* launched_device) {
    frame_receiver_ = std::move(receiver);
    launched_device_ = launched_device;
    OnDeviceLaunchedCall();
  }

  base::WeakPtrFactory<SingleClientVideoCaptureHostTest> weak_factory_{this};
};

TEST_F(SingleClientVideoCaptureHostTest, Basic) {
  CustomSetUp();
  CreateBuffer(1, 0);
  const int feedback_id = 5;
  FrameReadyInBuffer(1, 0, feedback_id);
  auto feedback = media::VideoCaptureFeedback(1.0);
  feedback.frame_id = feedback_id;
  FinishConsumingBuffer(0, feedback);
  RetireBuffer(1, 0);
}

TEST_F(SingleClientVideoCaptureHostTest, InvalidParams) {
  CustomSetUp(false);
}

TEST_F(SingleClientVideoCaptureHostTest, ReuseBufferId) {
  CustomSetUp();
  CreateBuffer(0, 0);
  const int feedback_id = 3;
  FrameReadyInBuffer(0, 0, feedback_id);
  // Retire buffer 0. The consumer is not expected to receive OnBufferDestroyed
  // since the buffer is not returned yet.
  {
    EXPECT_CALL(*consumer_, OnBufferDestroyedCall(0)).Times(0);
    frame_receiver_->OnBufferRetired(0);
    task_environment_.RunUntilIdle();
  }

  // Re-use buffer 0.
  const int feedback_id_2 = 7;
  CreateBuffer(0, 1);
  FrameReadyInBuffer(0, 1, feedback_id_2);

  // Finish consuming frame in the retired buffer 0.
  auto feedback = media::VideoCaptureFeedback(1.0);
  feedback.frame_id = feedback_id;
  FinishConsumingBuffer(0, feedback);
  // The retired buffer is expected to be destroyed since the consumer finished
  // consuming the frame in that buffer.
  base::RunLoop run_loop;
  EXPECT_CALL(*consumer_, OnBufferDestroyedCall(0))
      .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
  run_loop.Run();

  auto feedback_2 = media::VideoCaptureFeedback(0.5);
  feedback_2.frame_id = feedback_id_2;
  FinishConsumingBuffer(1, feedback_2);
  RetireBuffer(0, 1);
}

TEST_F(SingleClientVideoCaptureHostTest, StopCapturingWhileBuffersInUse) {
  CustomSetUp();
  for (int i = 0; i < 10; ++i) {
    CreateBuffer(i, i);
    FrameReadyInBuffer(i, i, i);
  }
}

}  // namespace mirroring
