// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CHROMEOS_CAMERA_FAKE_MJPEG_DECODE_ACCELERATOR_H_
#define COMPONENTS_CHROMEOS_CAMERA_FAKE_MJPEG_DECODE_ACCELERATOR_H_

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread.h"
#include "components/chromeos_camera/mjpeg_decode_accelerator.h"
#include "media/base/bitstream_buffer.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace chromeos_camera {

// Uses software-based decoding. The purpose of this class is to enable testing
// of communication to the MjpegDecodeAccelerator without requiring an actual
// hardware decoder.
class FakeMjpegDecodeAccelerator : public MjpegDecodeAccelerator {
 public:
  FakeMjpegDecodeAccelerator(
      const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner);
  ~FakeMjpegDecodeAccelerator() override;

  // MjpegDecodeAccelerator implementation.
  bool Initialize(MjpegDecodeAccelerator::Client* client) override;
  void Decode(media::BitstreamBuffer bitstream_buffer,
              scoped_refptr<media::VideoFrame> video_frame) override;
  void Decode(int32_t task_id,
              base::ScopedFD src_dmabuf_fd,
              size_t src_size,
              off_t src_offset,
              scoped_refptr<media::VideoFrame> dst_frame) override;
  bool IsSupported() override;

 private:
  void DecodeOnDecoderThread(
      int32_t task_id,
      scoped_refptr<media::VideoFrame> video_frame,
      std::unique_ptr<media::UnalignedSharedMemory> src_shm);
  void NotifyError(int32_t task_id, Error error);
  void NotifyErrorOnClientThread(int32_t task_id, Error error);
  void OnDecodeDoneOnClientThread(int32_t task_id);

  // Task runner for calls to |client_|.
  const scoped_refptr<base::SingleThreadTaskRunner> client_task_runner_;

  // GPU IO task runner.
  const scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  Client* client_ = nullptr;

  base::Thread decoder_thread_;
  scoped_refptr<base::SingleThreadTaskRunner> decoder_task_runner_;

  base::WeakPtrFactory<FakeMjpegDecodeAccelerator> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(FakeMjpegDecodeAccelerator);
};

}  // namespace chromeos_camera

#endif  // COMPONENTS_CHROMEOS_CAMERA_FAKE_MJPEG_DECODE_ACCELERATOR_H_
