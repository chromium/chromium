// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_effects/test/fake_video_source_provider.h"
#include "base/system/system_monitor.h"

namespace media_effects {

using GetSourceInfosResult =
    video_capture::mojom::VideoSourceProvider::GetSourceInfosResult;

FakeVideoSourceProvider::FakeVideoSourceProvider() = default;
FakeVideoSourceProvider::~FakeVideoSourceProvider() = default;

void FakeVideoSourceProvider::Bind(
    mojo::PendingReceiver<video_capture::mojom::VideoSourceProvider>
        pending_receiver) {
  receivers_.Add(this, std::move(pending_receiver));
}

void FakeVideoSourceProvider::AddFakeCamera(
    const media::VideoCaptureDeviceDescriptor& descriptor) {
  device_infos_.emplace(descriptor.device_id,
                        media::VideoCaptureDeviceInfo{descriptor});
  NotifyDevicesChanged();
}

void FakeVideoSourceProvider::RemoveFakeCamera(const std::string& device_id) {
  device_infos_.erase(device_id);
  NotifyDevicesChanged();
}

void FakeVideoSourceProvider::SetOnRepliedWithSourceInfosCallback(
    base::OnceClosure callback) {
  on_replied_with_source_infos_ = std::move(callback);
}

// `callback` will be triggered when the source provider receives a
// GetVideoSource call.
void FakeVideoSourceProvider::SetOnGetVideoSourceCallback(
    GetVideoSourceCallback callback) {
  on_get_video_source_ = std::move(callback);
}

void FakeVideoSourceProvider::GetSourceInfos(GetSourceInfosCallback callback) {
  std::vector<media::VideoCaptureDeviceInfo> devices;
  for (const auto& [_, device_info] : device_infos_) {
    devices.emplace_back(device_info);
  }

  base::OnceClosure reply_callback =
      !on_replied_with_source_infos_.is_null()
          ? std::move(on_replied_with_source_infos_)
          : base::DoNothing();

  // Simulate the asynchronously behavior of the actual VideoSourceProvider
  // which does a lot of asynchronous and mojo calls.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(std::move(callback), GetSourceInfosResult::kSuccess,
                     devices),
      std::move(reply_callback));
}

void FakeVideoSourceProvider::GetVideoSource(
    const std::string& source_id,
    mojo::PendingReceiver<video_capture::mojom::VideoSource> stream) {
  on_get_video_source_.Run(source_id, std::move(stream));
}

void FakeVideoSourceProvider::Close(CloseCallback callback) {}

void FakeVideoSourceProvider::NotifyDevicesChanged() {
  base::SystemMonitor::Get()->ProcessDevicesChanged(
      base::SystemMonitor::DeviceType::DEVTYPE_VIDEO_CAPTURE);
}

}  // namespace media_effects
