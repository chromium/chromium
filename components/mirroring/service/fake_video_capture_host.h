// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MIRRORING_SERVICE_FAKE_VIDEO_CAPTURE_HOST_H_
#define COMPONENTS_MIRRORING_SERVICE_FAKE_VIDEO_CAPTURE_HOST_H_

#include <string>

#include "base/token.h"
#include "base/unguessable_token.h"
#include "media/capture/mojom/video_capture.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mirroring {

class FakeVideoCaptureHost : public media::mojom::VideoCaptureHost {
 public:
  explicit FakeVideoCaptureHost(
      mojo::PendingReceiver<media::mojom::VideoCaptureHost> receiver);

  FakeVideoCaptureHost(const FakeVideoCaptureHost&) = delete;
  FakeVideoCaptureHost& operator=(const FakeVideoCaptureHost&) = delete;

  ~FakeVideoCaptureHost() override;

  // mojom::VideoCaptureHost implementations
  MOCK_METHOD1(RequestRefreshFrame, void(const base::UnguessableToken&));
  MOCK_METHOD3(ReleaseBuffer,
               void(const base::UnguessableToken&,
                    int32_t,
                    const media::VideoCaptureFeedback&));
  MOCK_METHOD0(OnStopped, void());
  MOCK_METHOD2(OnLog, void(const base::UnguessableToken&, const std::string&));

  void Start(const base::UnguessableToken& device_id,
             const base::UnguessableToken& session_id,
             const media::VideoCaptureParams& params,
             mojo::PendingRemote<media::mojom::VideoCaptureObserver> observer)
      override;
  void Stop(const base::UnguessableToken& device_id) override;
  void Pause(const base::UnguessableToken& device_id) override;
  void Resume(const base::UnguessableToken& device_id,
              const base::UnguessableToken& session_id,
              const media::VideoCaptureParams& params) override;
  void GetDeviceSupportedFormats(
      const base::UnguessableToken& device_id,
      const base::UnguessableToken& session_id,
      GetDeviceSupportedFormatsCallback callback) override {}
  void GetDeviceFormatsInUse(const base::UnguessableToken& device_id,
                             const base::UnguessableToken& session_id,
                             GetDeviceFormatsInUseCallback callback) override {}

  // Create one video frame and send it to |observer_|.
  void SendOneFrame(const gfx::Size& size, base::TimeTicks capture_time);

  // Get the most recent capture parameters passed to Start().
  media::VideoCaptureParams GetVideoCaptureParams() const;

  bool paused() { return paused_; }

 private:
  mojo::Receiver<media::mojom::VideoCaptureHost> receiver_;
  mojo::Remote<media::mojom::VideoCaptureObserver> observer_;
  media::VideoCaptureParams last_params_;
  bool paused_ = false;
};

}  // namespace mirroring

#endif  // COMPONENTS_MIRRORING_SERVICE_FAKE_VIDEO_CAPTURE_HOST_H_
