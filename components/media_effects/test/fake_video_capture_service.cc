// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_effects/test/fake_video_capture_service.h"

#include "base/test/test_future.h"
#include "content/public/browser/video_capture_service.h"

namespace media_effects {

void FakeVideoCaptureService::AddFakeCamera(
    const media::VideoCaptureDeviceDescriptor& descriptor) {
  fake_provider_.AddFakeCamera(descriptor);
}

bool FakeVideoCaptureService::AddFakeCameraBlocking(
    const media::VideoCaptureDeviceDescriptor& descriptor) {
  base::test::TestFuture<void> test_future;
  SetOnRepliedWithSourceInfosCallback(test_future.GetCallback());
  AddFakeCamera(descriptor);
  return test_future.WaitAndClear();
}

void FakeVideoCaptureService::RemoveFakeCamera(const std::string& device_id) {
  fake_provider_.RemoveFakeCamera(device_id);
}

bool FakeVideoCaptureService::RemoveFakeCameraBlocking(
    const std::string& device_id) {
  base::test::TestFuture<void> test_future;
  SetOnRepliedWithSourceInfosCallback(test_future.GetCallback());
  RemoveFakeCamera(device_id);
  return test_future.WaitAndClear();
}

void FakeVideoCaptureService::SetOnRepliedWithSourceInfosCallback(
    base::OnceClosure callback) {
  fake_provider_.SetOnRepliedWithSourceInfosCallback(std::move(callback));
}

// `callback` will be triggered when the source provider receives a
// GetVideoSource call.
void FakeVideoCaptureService::SetOnGetVideoSourceCallback(
    FakeVideoSourceProvider::GetVideoSourceCallback callback) {
  fake_provider_.SetOnGetVideoSourceCallback(std::move(callback));
}

void FakeVideoCaptureService::ConnectToVideoSourceProvider(
    mojo::PendingReceiver<video_capture::mojom::VideoSourceProvider> receiver) {
  fake_provider_.Bind(std::move(receiver));
}

ScopedFakeVideoCaptureService::ScopedFakeVideoCaptureService() {
  content::OverrideVideoCaptureServiceForTesting(this);
}

ScopedFakeVideoCaptureService::~ScopedFakeVideoCaptureService() {
  content::OverrideVideoCaptureServiceForTesting(nullptr);
}

}  // namespace media_effects
