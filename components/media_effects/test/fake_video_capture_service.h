// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_EFFECTS_TEST_FAKE_VIDEO_CAPTURE_SERVICE_H_
#define COMPONENTS_MEDIA_EFFECTS_TEST_FAKE_VIDEO_CAPTURE_SERVICE_H_

#include "components/media_effects/test/fake_video_source_provider.h"
#include "services/video_capture/public/mojom/video_capture_service.mojom.h"

#include <string>

namespace media_effects {

class FakeVideoCaptureService
    : public video_capture::mojom::VideoCaptureService {
 public:
  FakeVideoCaptureService() = default;
  ~FakeVideoCaptureService() override = default;

  FakeVideoCaptureService(const FakeVideoCaptureService&) = delete;
  FakeVideoCaptureService& operator=(const FakeVideoCaptureService&) = delete;
  FakeVideoCaptureService(FakeVideoCaptureService&&) = delete;
  FakeVideoCaptureService& operator=(FakeVideoCaptureService&&) = delete;

  void AddFakeCamera(const media::VideoCaptureDeviceDescriptor& descriptor);

  void RemoveFakeCamera(const std::string& device_id);

  // `callback` will be triggered after the source provider replies back to its
  // client in GetSourceInfos(). Useful as a stopping point for a base::RunLoop.
  void SetOnRepliedWithSourceInfosCallback(base::OnceClosure callback);

  // `callback` will be triggered when the source provider receives a
  // GetVideoSource call.
  void SetOnGetVideoSourceCallback(
      FakeVideoSourceProvider::GetVideoSourceCallback callback);

  // video_capture::mojom::VideoCaptureService implementation
  void ConnectToVideoSourceProvider(
      mojo::PendingReceiver<video_capture::mojom::VideoSourceProvider> receiver)
      override;

  void BindControlsForTesting(
      mojo::PendingReceiver<video_capture::mojom::TestingControls> receiver)
      override {}

#if BUILDFLAG(IS_WIN)
  void OnGpuInfoUpdate(const CHROME_LUID& luid) override {}
#endif  // BUILDFLAG(IS_WIN)

 private:
  FakeVideoSourceProvider fake_provider_;
};

}  // namespace media_effects

#endif  // COMPONENTS_MEDIA_EFFECTS_TEST_FAKE_VIDEO_CAPTURE_SERVICE_H_
