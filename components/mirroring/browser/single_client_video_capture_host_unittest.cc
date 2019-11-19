// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mirroring/browser/single_client_video_capture_host.h"

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::InvokeWithoutArgs;
using media::VideoFrameReceiver;

namespace mirroring {

namespace {

class MockVideoCaptureDevice final
    : public content::LaunchedVideoCaptureDevice {
 public:
  MockVideoCaptureDevice() {}
  ~MockVideoCaptureDevice() override {}
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
  MOCK_METHOD0(RequestRefreshFrame, void());
  MOCK_METHOD2(OnUtilizationReport, void(int, double));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockVideoCaptureDevice);
};

class FakeDeviceLauncher final : public content::VideoCaptureDeviceLauncher {
 public:
  using DeviceLaunchedCallback =
      base::OnceCallback<void(base::WeakPtr<VideoFrameReceiver>,
                              MockVideoCaptureDevice*)>;

  explicit FakeDeviceLauncher(DeviceLaunchedCallback launched_cb)
      : after_launch_cb_(std::move(launched_cb)) {}

  ~FakeDeviceLauncher() override {}

  // content::VideoCaptureDeviceLauncher implementation.
  void LaunchDeviceAsync(const std::string& device_id,
                         blink::mojom::MediaStreamType stream_type,
                         const VideoCaptureParams& params,
                         base::WeakPtr<VideoFrameReceiver> receiver,
                         base::OnceClosure connection_lost_cb,
                         Callbacks* callbacks,
                         base::OnceClosure done_cb) override {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
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
  DISALLOW_COPY_AND_ASSIGN(FakeDeviceLauncher);
};

class StubReadWritePermission final
    : public VideoCaptureDevice::Client::Buffer::ScopedAccessPermission {
 public:
  StubReadWritePermission() {}
  ~StubReadWritePermission() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(StubReadWritePermission);
};

class MockVideoCaptureObserver final
    : public media::mojom::VideoCaptureObserver {
 public:
  explicit MockVideoCaptureObserver(
      mojo::PendingRemote<media::mojom::VideoCaptureHost> host)
      : host_(std::move(host)) {}
  MOCK_METHOD1(OnBufferCreatedCall, void(int buffer_id));
  void OnNewBuffer(int32_t buffer_id,
                   media::mojom::VideoBufferHandlePtr buffer_handle) override {
    EXPECT_EQ(buffers_.find(buffer_id), buffers_.end());
    EXPECT_EQ(frame_infos_.find(buffer_id), frame_infos_.end());
    buffers_[buffer_id] = std::move(buffer_handle);
    OnBufferCreatedCall(buffer_id);
  }
  MOCK_METHOD1(OnBufferReadyCall, void(int buffer_id));
  void OnBufferReady(int32_t buffer_id,
                     media::mojom::VideoFrameInfoPtr info) override {
    EXPECT_TRUE(buffers_.find(buffer_id) != buffers_.end());
    EXPECT_EQ(frame_infos_.find(buffer_id), frame_infos_.end());
    frame_infos_[buffer_id] = std::move(info);
    OnBufferReadyCall(buffer_id);
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

  MOCK_METHOD1(OnStateChanged, void(media::mojom::VideoCaptureState state));

  void Start() {
    host_->Start(device_id_, session_id_, VideoCaptureParams(),
                 receiver_.BindNewPipeAndPassRemote());
  }

  void FinishConsumingBuffer(int32_t buffer_id, double utilization) {
    EXPECT_TRUE(buffers_.find(buffer_id) != buffers_.end());
    const auto iter = frame_infos_.find(buffer_id);
    EXPECT_TRUE(iter != frame_infos_.end());
    frame_infos_.erase(iter);
    host_->ReleaseBuffer(device_id_, buffer_id, utilization);
  }

  void Stop() { host_->Stop(device_id_); }

 private:
  const base::UnguessableToken device_id_ = base::UnguessableToken::Create();
  const base::UnguessableToken session_id_ = base::UnguessableToken::Create();
  mojo::Remote<media::mojom::VideoCaptureHost> host_;
  mojo::Receiver<media::mojom::VideoCaptureObserver> receiver_{this};
  base::flat_map<int, media::mojom::VideoBufferHandlePtr> buffers_;
  base::flat_map<int, media::mojom::VideoFrameInfoPtr> frame_infos_;

  DISALLOW_COPY_AND_ASSIGN(MockVideoCaptureObserver);
};

media::mojom::VideoFrameInfoPtr GetVideoFrameInfo() {
  return media::mojom::VideoFrameInfo::New(
      base::TimeDelta(), base::Value(base::Value::Type::DICTIONARY),
      media::PIXEL_FORMAT_I420, gfx::Size(320, 180), gfx::Rect(320, 180),
      gfx::ColorSpace::CreateREC709(), nullptr);
}

}  // namespace

class SingleClientVideoCaptureHostTest : public ::testing::Test {
 public:
  SingleClientVideoCaptureHostTest() {
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
    EXPECT_CALL(*this, OnDeviceLaunchedCall())
        .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    consumer_->Start();
    run_loop.Run();

    // The video capture device is launched.
    EXPECT_TRUE(!!launched_device_);
    EXPECT_TRUE(!!frame_receiver_);
  }

  ~SingleClientVideoCaptureHostTest() override {
    base::RunLoop run_loop;
    EXPECT_CALL(*consumer_,
                OnStateChanged(media::mojom::VideoCaptureState::ENDED))
        .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    consumer_->Stop();
    run_loop.Run();
  }

 protected:
  void CreateBuffer(int buffer_id, int expected_buffer_context_id) {
    base::RunLoop run_loop;
    EXPECT_CALL(*consumer_, OnBufferCreatedCall(expected_buffer_context_id))
        .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    media::mojom::VideoBufferHandlePtr stub_buffer_handle =
        media::mojom::VideoBufferHandle::New();
    stub_buffer_handle->set_shared_buffer_handle(
        mojo::SharedBufferHandle::Create(10));
    frame_receiver_->OnNewBuffer(buffer_id, std::move(stub_buffer_handle));
    run_loop.Run();
  }

  void FrameReadyInBuffer(int buffer_id,
                          int buffer_context_id,
                          int feedback_id) {
    base::RunLoop run_loop;
    EXPECT_CALL(*consumer_, OnBufferReadyCall(buffer_context_id))
        .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    frame_receiver_->OnFrameReadyInBuffer(
        buffer_id, feedback_id, std::make_unique<StubReadWritePermission>(),
        GetVideoFrameInfo());
    run_loop.Run();
  }

  void FinishConsumingBuffer(int buffer_context_id,
                             int feedback_id,
                             double utilization) {
    base::RunLoop run_loop;
    EXPECT_CALL(*launched_device_,
                OnUtilizationReport(feedback_id, utilization))
        .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    consumer_->FinishConsumingBuffer(buffer_context_id, utilization);
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
  MockVideoCaptureDevice* launched_device_ = nullptr;

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
  CreateBuffer(1, 0);
  FrameReadyInBuffer(1, 0, 5);
  FinishConsumingBuffer(0, 5, 1.0);
  RetireBuffer(1, 0);
}

TEST_F(SingleClientVideoCaptureHostTest, ReuseBufferId) {
  CreateBuffer(0, 0);
  FrameReadyInBuffer(0, 0, 3);
  // Retire buffer 0. The consumer is not expected to receive OnBufferDestroyed
  // since the buffer is not returned yet.
  {
    EXPECT_CALL(*consumer_, OnBufferDestroyedCall(0)).Times(0);
    frame_receiver_->OnBufferRetired(0);
    task_environment_.RunUntilIdle();
  }

  // Re-use buffer 0.
  CreateBuffer(0, 1);
  FrameReadyInBuffer(0, 1, 7);

  // Finish consuming frame in the retired buffer 0.
  FinishConsumingBuffer(0, 3, 1.0);
  // The retired buffer is expected to be destroyed since the consumer finished
  // consuming the frame in that buffer.
  base::RunLoop run_loop;
  EXPECT_CALL(*consumer_, OnBufferDestroyedCall(0))
      .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
  run_loop.Run();

  FinishConsumingBuffer(1, 7, 0.5);
  RetireBuffer(0, 1);
}

TEST_F(SingleClientVideoCaptureHostTest, StopCapturingWhileBuffersInUse) {
  for (int i = 0; i < 10; ++i) {
    CreateBuffer(i, i);
    FrameReadyInBuffer(i, i, i);
  }
}

}  // namespace mirroring
