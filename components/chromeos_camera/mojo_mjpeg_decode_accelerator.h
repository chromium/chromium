// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CHROMEOS_CAMERA_MOJO_MJPEG_DECODE_ACCELERATOR_H_
#define COMPONENTS_CHROMEOS_CAMERA_MOJO_MJPEG_DECODE_ACCELERATOR_H_

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "components/chromeos_camera/common/mjpeg_decode_accelerator.mojom.h"
#include "components/chromeos_camera/mjpeg_decode_accelerator.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace base {
class SequencedTaskRunner;
}

namespace chromeos_camera {

// A MjpegDecodeAccelerator, for use in the browser process, that proxies to a
// chromeos_camera::mojom::MjpegDecodeAccelerator. Created on the owner's
// thread, otherwise operating and deleted on |io_task_runner|.
class MojoMjpegDecodeAccelerator : public MjpegDecodeAccelerator {
 public:
  MojoMjpegDecodeAccelerator(
      scoped_refptr<base::SequencedTaskRunner> io_task_runner,
      mojo::PendingRemote<chromeos_camera::mojom::MjpegDecodeAccelerator>
          jpeg_decoder);
  ~MojoMjpegDecodeAccelerator() override;

  // MjpegDecodeAccelerator implementation.
  // |client| is called on the IO thread, but is never called into after the
  // MojoMjpegDecodeAccelerator is destroyed.
  bool Initialize(Client* client) override;
  void InitializeAsync(Client* client, InitCB init_cb) override;
  void Decode(media::BitstreamBuffer bitstream_buffer,
              scoped_refptr<media::VideoFrame> video_frame) override;
  void Decode(int32_t task_id,
              base::ScopedFD src_dmabuf_fd,
              size_t src_size,
              off_t src_offset,
              scoped_refptr<media::VideoFrame> dst_frame) override;
  bool IsSupported() override;

 private:
  void OnInitializeDone(InitCB init_cb,
                        MjpegDecodeAccelerator::Client* client,
                        bool success);
  void OnDecodeAck(int32_t bitstream_buffer_id,
                   MjpegDecodeAccelerator::Error error);
  void OnLostConnectionToJpegDecoder();

  scoped_refptr<base::SequencedTaskRunner> io_task_runner_;

  Client* client_ = nullptr;

  // Used to safely pass the
  // chromeos_mojo::Remote<camera::mojom::MjpegDecodeAccelerator> from one
  // thread to another. It is set in the constructor and consumed in
  // InitializeAsync().
  // TODO(mcasas): s/jpeg_decoder_/jda_/ https://crbug.com/699255.
  mojo::PendingRemote<chromeos_camera::mojom::MjpegDecodeAccelerator>
      jpeg_decoder_remote_;

  mojo::Remote<chromeos_camera::mojom::MjpegDecodeAccelerator> jpeg_decoder_;

  DISALLOW_COPY_AND_ASSIGN(MojoMjpegDecodeAccelerator);
};

}  // namespace chromeos_camera

#endif  // COMPONENTS_CHROMEOS_CAMERA_MOJO_MJPEG_DECODE_ACCELERATOR_H_
