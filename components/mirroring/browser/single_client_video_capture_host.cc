// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mirroring/browser/single_client_video_capture_host.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/not_fatal_until.h"
#include "base/token.h"
#include "content/public/browser/web_contents_media_capture_id.h"
#include "media/capture/video/video_capture_buffer_pool.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/video_effects/public/mojom/video_effects_processor.mojom.h"

using media::VideoFrameConsumerFeedbackObserver;

namespace mirroring {

namespace {

class DeviceLauncherCallbacks final
    : public content::VideoCaptureDeviceLauncher::Callbacks {
 public:
  explicit DeviceLauncherCallbacks(
      base::WeakPtr<SingleClientVideoCaptureHost> host)
      : video_capture_host_(host) {}

  DeviceLauncherCallbacks(const DeviceLauncherCallbacks&) = delete;
  DeviceLauncherCallbacks& operator=(const DeviceLauncherCallbacks&) = delete;

  ~DeviceLauncherCallbacks() override = default;

  // content::VideoCaptureDeviceLauncher::Callbacks implementations
  void OnDeviceLaunched(
      std::unique_ptr<content::LaunchedVideoCaptureDevice> device) override {
    if (video_capture_host_)
      video_capture_host_->OnDeviceLaunched(std::move(device));
  }

  void OnDeviceLaunchFailed(media::VideoCaptureError error) override {
    if (video_capture_host_)
      video_capture_host_->OnDeviceLaunchFailed(error);
  }

  void OnDeviceLaunchAborted() override {
    if (video_capture_host_)
      video_capture_host_->OnDeviceLaunchAborted();
  }

 private:
  base::WeakPtr<SingleClientVideoCaptureHost> video_capture_host_;
};

}  // namespace

SingleClientVideoCaptureHost::SingleClientVideoCaptureHost(
    const std::string& device_id,
    blink::mojom::MediaStreamType type,
    DeviceLauncherCreateCallback callback)
    : device_id_(device_id),
      type_(type),
      device_launcher_callback_(std::move(callback)) {
  DCHECK(!device_launcher_callback_.is_null());
}

SingleClientVideoCaptureHost::~SingleClientVideoCaptureHost() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Stop(base::UnguessableToken());
}

void SingleClientVideoCaptureHost::Start(
    const base::UnguessableToken& device_id,
    const base::UnguessableToken& session_id,
    const VideoCaptureParams& params,
    mojo::PendingRemote<media::mojom::VideoCaptureObserver> observer) {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!observer_);
  observer_.Bind(std::move(observer));
  DCHECK(observer_);
  DCHECK(!launched_device_);

  // Start a video capture device.
  auto device_launcher_callbacks =
      std::make_unique<DeviceLauncherCallbacks>(weak_factory_.GetWeakPtr());
  DeviceLauncherCallbacks* callbacks = device_launcher_callbacks.get();
  std::unique_ptr<content::VideoCaptureDeviceLauncher> device_launcher =
      device_launcher_callback_.Run();
  content::VideoCaptureDeviceLauncher* launcher = device_launcher.get();
  launcher->LaunchDeviceAsync(
      device_id_, type_, params, weak_factory_.GetWeakPtr(),
      base::BindOnce(&SingleClientVideoCaptureHost::OnError,
                     weak_factory_.GetWeakPtr(),
                     media::VideoCaptureError::
                         kSingleClientVideoCaptureHostLostConnectionToDevice),
      callbacks,
      // The |device_launcher| and |device_launcher_callbacks| must be kept
      // alive until the device launching completes.
      base::BindOnce([](std::unique_ptr<content::VideoCaptureDeviceLauncher>,
                        std::unique_ptr<DeviceLauncherCallbacks>) {},
                     std::move(device_launcher),
                     std::move(device_launcher_callbacks)),
      mojo::PendingRemote<video_effects::mojom::VideoEffectsProcessor>{});
}

void SingleClientVideoCaptureHost::Stop(
    const base::UnguessableToken& device_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(1) << __func__;

  if (!observer_)
    return;

  // Returns all the buffers.
  std::vector<int> buffers_in_use;
  buffers_in_use.reserve(buffer_context_map_.size());
  for (const auto& entry : buffer_context_map_)
    buffers_in_use.push_back(entry.first);
  for (int buffer_id : buffers_in_use) {
    OnFinishedConsumingBuffer(buffer_id, media::VideoCaptureFeedback());
  }
  DCHECK(buffer_context_map_.empty());
  observer_->OnStateChanged(media::mojom::VideoCaptureResult::NewState(
      media::mojom::VideoCaptureState::ENDED));
  observer_.reset();
  weak_factory_.InvalidateWeakPtrs();
  launched_device_ = nullptr;
  id_map_.clear();
  retired_buffers_.clear();
}

void SingleClientVideoCaptureHost::Pause(
    const base::UnguessableToken& device_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (launched_device_)
    launched_device_->MaybeSuspendDevice();
}

void SingleClientVideoCaptureHost::Resume(
    const base::UnguessableToken& device_id,
    const base::UnguessableToken& session_id,
    const VideoCaptureParams& params) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (launched_device_)
    launched_device_->ResumeDevice();
}

void SingleClientVideoCaptureHost::RequestRefreshFrame(
    const base::UnguessableToken& device_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (launched_device_)
    launched_device_->RequestRefreshFrame();
}

void SingleClientVideoCaptureHost::ReleaseBuffer(
    const base::UnguessableToken& device_id,
    int32_t buffer_id,
    const media::VideoCaptureFeedback& feedback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(3) << __func__ << ": buffer_id=" << buffer_id;

  OnFinishedConsumingBuffer(buffer_id, feedback);
}

void SingleClientVideoCaptureHost::GetDeviceSupportedFormats(
    const base::UnguessableToken& device_id,
    const base::UnguessableToken& session_id,
    GetDeviceSupportedFormatsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
  std::move(callback).Run(media::VideoCaptureFormats());
}

void SingleClientVideoCaptureHost::GetDeviceFormatsInUse(
    const base::UnguessableToken& device_id,
    const base::UnguessableToken& session_id,
    GetDeviceFormatsInUseCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
  std::move(callback).Run(media::VideoCaptureFormats());
}

void SingleClientVideoCaptureHost::OnCaptureConfigurationChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Ignore this call.
}

void SingleClientVideoCaptureHost::OnNewSubCaptureTargetVersion(
    uint32_t sub_capture_target_version) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Ignore this call.
}

void SingleClientVideoCaptureHost::OnFrameWithEmptyRegionCapture() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Ignore this call.
}

void SingleClientVideoCaptureHost::OnLog(
    const base::UnguessableToken& device_id,
    const std::string& message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Ignore this call.
}

void SingleClientVideoCaptureHost::OnNewBuffer(
    int buffer_id,
    media::mojom::VideoBufferHandlePtr buffer_handle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(3) << __func__ << ": buffer_id=" << buffer_id;
  DCHECK(observer_);
  DCHECK_NE(buffer_id, media::VideoCaptureBufferPool::kInvalidId);
  const auto insert_result =
      id_map_.emplace(std::make_pair(buffer_id, next_buffer_context_id_));
  DCHECK(insert_result.second);
  observer_->OnNewBuffer(next_buffer_context_id_++, std::move(buffer_handle));
}

void SingleClientVideoCaptureHost::OnFrameReadyInBuffer(
    media::ReadyFrameInBuffer frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(3) << __func__ << ": buffer_id=" << frame.buffer_id;
  DCHECK(observer_);
  const auto id_iter = id_map_.find(frame.buffer_id);
  CHECK(id_iter != id_map_.end(), base::NotFatalUntil::M130);
  const int buffer_context_id = id_iter->second;
  const auto insert_result = buffer_context_map_.emplace(
      std::make_pair(buffer_context_id,
                     std::make_pair(frame.frame_feedback_id,
                                    std::move(frame.buffer_read_permission))));
  DCHECK(insert_result.second);
  // This implementation does not forward scaled frames.
  observer_->OnBufferReady(media::mojom::ReadyBuffer::New(
      buffer_context_id, std::move(frame.frame_info)));
}

void SingleClientVideoCaptureHost::OnBufferRetired(int buffer_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(3) << __func__ << ": buffer_id=" << buffer_id;

  const auto id_iter = id_map_.find(buffer_id);
  CHECK(id_iter != id_map_.end(), base::NotFatalUntil::M130);
  const int buffer_context_id = id_iter->second;
  id_map_.erase(id_iter);
  if (buffer_context_map_.find(buffer_context_id) ==
      buffer_context_map_.end()) {
    DCHECK(observer_);
    observer_->OnBufferDestroyed(buffer_context_id);
  } else {
    // The consumer is still using the buffer. The BufferContext needs to be
    // held until the consumer finishes.
    const auto insert_result = retired_buffers_.insert(buffer_context_id);
    DCHECK(insert_result.second);
  }
}

void SingleClientVideoCaptureHost::OnError(media::VideoCaptureError error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << __func__;
  if (observer_)
    observer_->OnStateChanged(
        media::mojom::VideoCaptureResult::NewErrorCode(error));
}

void SingleClientVideoCaptureHost::OnFrameDropped(
    media::VideoCaptureFrameDropReason reason) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void SingleClientVideoCaptureHost::OnLog(const std::string& message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(3) << message;
}

void SingleClientVideoCaptureHost::OnStarted() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(2) << __func__;
  DCHECK(observer_);
  observer_->OnStateChanged(media::mojom::VideoCaptureResult::NewState(
      media::mojom::VideoCaptureState::STARTED));
}

void SingleClientVideoCaptureHost::OnStartedUsingGpuDecode() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTIMPLEMENTED();
}

void SingleClientVideoCaptureHost::OnStopped() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void SingleClientVideoCaptureHost::OnDeviceLaunched(
    std::unique_ptr<content::LaunchedVideoCaptureDevice> device) {
  DVLOG(1) << __func__;
  launched_device_ = std::move(device);
}

void SingleClientVideoCaptureHost::OnDeviceLaunchFailed(
    media::VideoCaptureError error) {
  DVLOG(1) << __func__;
  launched_device_ = nullptr;
  OnError(error);
}

void SingleClientVideoCaptureHost::OnDeviceLaunchAborted() {
  DVLOG(1) << __func__;
  launched_device_ = nullptr;
  OnError(
      media::VideoCaptureError::kSingleClientVideoCaptureDeviceLaunchAborted);
}

void SingleClientVideoCaptureHost::OnFinishedConsumingBuffer(
    int buffer_context_id,
    media::VideoCaptureFeedback feedback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(observer_);
  const auto buffer_context_iter = buffer_context_map_.find(buffer_context_id);
  if (buffer_context_iter == buffer_context_map_.end()) {
    // Stop() must have been called before.
    DCHECK(!launched_device_);
    return;
  }
  VideoFrameConsumerFeedbackObserver* feedback_observer =
      launched_device_.get();
  if (feedback_observer && !feedback.Empty()) {
    feedback.frame_id = buffer_context_iter->second.first;
    feedback_observer->OnUtilizationReport(feedback);
  }
  buffer_context_map_.erase(buffer_context_iter);
  const auto retired_iter = retired_buffers_.find(buffer_context_id);
  if (retired_iter != retired_buffers_.end()) {
    retired_buffers_.erase(retired_iter);
    observer_->OnBufferDestroyed(buffer_context_id);
  }
}

}  // namespace mirroring
