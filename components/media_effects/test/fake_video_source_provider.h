// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_EFFECTS_TEST_FAKE_VIDEO_SOURCE_PROVIDER_H_
#define COMPONENTS_MEDIA_EFFECTS_TEST_FAKE_VIDEO_SOURCE_PROVIDER_H_

#include "base/functional/callback_helpers.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/video_capture/public/mojom/video_source_provider.mojom.h"

namespace media_effects {

// Defines a fake implementation of the `VideoSourceProvider` mojo interface
// for testing the interaction with the video capture service.
class FakeVideoSourceProvider
    : public video_capture::mojom::VideoSourceProvider {
 public:
  FakeVideoSourceProvider();
  ~FakeVideoSourceProvider() override;

  FakeVideoSourceProvider(const FakeVideoSourceProvider&) = delete;
  FakeVideoSourceProvider& operator=(const FakeVideoSourceProvider&) = delete;
  FakeVideoSourceProvider(FakeVideoSourceProvider&&) = delete;
  FakeVideoSourceProvider& operator=(const FakeVideoSourceProvider&&) = delete;

  void Bind(mojo::PendingReceiver<video_capture::mojom::VideoSourceProvider>
                pending_receiver);

  // Simulate connecting and disconnecting a camera device with the given
  // `descriptor`.
  void AddFakeCamera(const media::VideoCaptureDeviceDescriptor& descriptor);
  void RemoveFakeCamera(const std::string& device_id);

  // `callback` will be triggered after this source provider replies back to its
  // client in GetSourceInfos(). Useful as a stopping point for a base::RunLoop.
  void SetOnRepliedWithSourceInfosCallback(base::OnceClosure callback);

  using GetVideoSourceCallback = base::RepeatingCallback<void(
      const std::string& source_id,
      mojo::PendingReceiver<video_capture::mojom::VideoSource> stream)>;

  // `callback` will be triggered when this source provider receives a
  // GetVideoSource call.
  void SetOnGetVideoSourceCallback(GetVideoSourceCallback callback);

  // video_capture::mojom::VideoSourceProvider:
  void GetSourceInfos(GetSourceInfosCallback callback) override;
  void GetVideoSource(
      const std::string& source_id,
      mojo::PendingReceiver<video_capture::mojom::VideoSource> stream) override;
  void AddSharedMemoryVirtualDevice(
      const media::VideoCaptureDeviceInfo& device_info,
      mojo::PendingRemote<video_capture::mojom::Producer> producer,
      mojo::PendingReceiver<video_capture::mojom::SharedMemoryVirtualDevice>
          virtual_device_receiver) override {}
  void AddTextureVirtualDevice(
      const media::VideoCaptureDeviceInfo& device_info,
      mojo::PendingReceiver<video_capture::mojom::TextureVirtualDevice>
          virtual_device_receiver) override {}
  void RegisterVirtualDevicesChangedObserver(
      mojo::PendingRemote<video_capture::mojom::DevicesChangedObserver>
          observer,
      bool raise_event_if_virtual_devices_already_present) override {}
  void RegisterDevicesChangedObserver(
      mojo::PendingRemote<video_capture::mojom::DevicesChangedObserver>
          observer) override {}
  void Close(CloseCallback callback) override;

 private:
  void NotifyDevicesChanged();

  mojo::ReceiverSet<video_capture::mojom::VideoSourceProvider> receivers_;

  base::flat_map</*device_id=*/std::string, media::VideoCaptureDeviceInfo>
      device_infos_;

  base::OnceClosure on_replied_with_source_infos_;
  GetVideoSourceCallback on_get_video_source_ = base::DoNothing();
};

}  // namespace media_effects

#endif  // COMPONENTS_MEDIA_EFFECTS_TEST_FAKE_VIDEO_SOURCE_PROVIDER_H_
