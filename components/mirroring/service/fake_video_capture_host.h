// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MIRRORING_SERVICE_FAKE_VIDEO_CAPTURE_HOST_H_
#define COMPONENTS_MIRRORING_SERVICE_FAKE_VIDEO_CAPTURE_HOST_H_

#include <map>
#include <string>

#include "base/memory/shared_memory_mapping.h"
#include "base/token.h"
#include "base/unguessable_token.h"
#include "media/base/video_frame.h"
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
  MOCK_METHOD(void, RequestRefreshFrame, (const base::UnguessableToken&));
  MOCK_METHOD(void,
              ReleaseBuffer,
              (const base::UnguessableToken&,
               int32_t,
               const media::VideoCaptureFeedback&));
  MOCK_METHOD(void, OnStopped, ());
  MOCK_METHOD(void, OnPaused, ());
  MOCK_METHOD(void, OnLog, (const base::UnguessableToken&, const std::string&));
  MOCK_METHOD(void, OnResumed, ());

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
  void SendOneFrame(const gfx::Size& size,
                    base::TimeTicks capture_time,
                    media::VideoPixelFormat format = media::PIXEL_FORMAT_I420,
                    int start_value = 0);

  // Copies `source` (an I420 frame) into a pooled shared memory buffer and
  // delivers it to |observer_| with the given `capture_time`. Buffers are
  // reused once returned through ReturnBuffer(), so long-running simulations
  // don't accumulate buffers. Returns the buffer ID used, or -1 if the
  // frame could not be sent.
  int32_t SendFrame(const media::VideoFrame& source,
                    base::TimeTicks capture_time);

  // Marks a buffer used by SendFrame() as available for reuse. Tests that use
  // SendFrame() should call this from their ReleaseBuffer() expectation.
  void ReturnBuffer(int32_t buffer_id);

  // Get the most recent capture parameters passed to Start().
  media::VideoCaptureParams GetVideoCaptureParams() const;

  bool paused() { return paused_; }

  using OnStartedCallback = base::OnceClosure;
  void set_on_started_callback(OnStartedCallback cb) {
    on_started_cb_ = std::move(cb);
  }

  void FlushForTesting() { receiver_.FlushForTesting(); }

 private:
  mojo::Receiver<media::mojom::VideoCaptureHost> receiver_;
  mojo::Remote<media::mojom::VideoCaptureObserver> observer_;
  media::VideoCaptureParams last_params_;
  bool paused_ = false;
  int32_t next_buffer_id_ = 0;
  OnStartedCallback on_started_cb_;

  struct PooledBuffer {
    PooledBuffer();
    PooledBuffer(PooledBuffer&&);
    PooledBuffer& operator=(PooledBuffer&&);
    ~PooledBuffer();

    base::WritableSharedMemoryMapping mapping;
    size_t size = 0;
    bool in_use = false;
  };
  std::map<int32_t, PooledBuffer> buffer_pool_;
};

}  // namespace mirroring

#endif  // COMPONENTS_MIRRORING_SERVICE_FAKE_VIDEO_CAPTURE_HOST_H_
