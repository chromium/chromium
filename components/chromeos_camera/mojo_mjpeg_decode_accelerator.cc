// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/chromeos_camera/mojo_mjpeg_decode_accelerator.h"

#include <stddef.h>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace chromeos_camera {

MojoMjpegDecodeAccelerator::MojoMjpegDecodeAccelerator(
    scoped_refptr<base::SequencedTaskRunner> io_task_runner,
    mojo::PendingRemote<chromeos_camera::mojom::MjpegDecodeAccelerator>
        jpeg_decoder)
    : io_task_runner_(std::move(io_task_runner)),
      jpeg_decoder_remote_(std::move(jpeg_decoder)) {}

MojoMjpegDecodeAccelerator::~MojoMjpegDecodeAccelerator() {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
}

void MojoMjpegDecodeAccelerator::InitializeAsync(
    MjpegDecodeAccelerator::Client* client,
    MjpegDecodeAccelerator::InitCB init_cb) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());

  jpeg_decoder_.Bind(std::move(jpeg_decoder_remote_));

  // base::Unretained is safe because |this| owns |jpeg_decoder_|.
  jpeg_decoder_.set_disconnect_handler(
      base::BindOnce(&MojoMjpegDecodeAccelerator::OnLostConnectionToJpegDecoder,
                     base::Unretained(this)));
  jpeg_decoder_->Initialize(
      base::BindOnce(&MojoMjpegDecodeAccelerator::OnInitializeDone,
                     base::Unretained(this), std::move(init_cb), client));
}

void MojoMjpegDecodeAccelerator::Decode(
    media::BitstreamBuffer bitstream_buffer,
    media::VideoPixelFormat format,
    const gfx::Size& coded_size,
    base::UnsafeSharedMemoryRegion output_region) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(jpeg_decoder_.is_bound());

  const size_t output_buffer_size =
      media::VideoFrame::AllocationSize(format, coded_size);
  DCHECK_GE(output_region.GetSize(), output_buffer_size);

  mojo::ScopedSharedBufferHandle output_frame_handle =
      mojo::WrapUnsafeSharedMemoryRegion(std::move(output_region));
  if (!output_frame_handle.is_valid()) {
    DLOG(ERROR) << "Failed to duplicate handle of VideoFrame";
    return;
  }

  // base::Unretained is safe because |this| owns |jpeg_decoder_|.
  jpeg_decoder_->Decode(std::move(bitstream_buffer), coded_size,
                        std::move(output_frame_handle),
                        base::checked_cast<uint32_t>(output_buffer_size),
                        base::BindOnce(&MojoMjpegDecodeAccelerator::OnDecodeAck,
                                       base::Unretained(this)));
}

bool MojoMjpegDecodeAccelerator::IsSupported() {
  return true;
}

void MojoMjpegDecodeAccelerator::OnInitializeDone(
    MjpegDecodeAccelerator::InitCB init_cb,
    MjpegDecodeAccelerator::Client* client,
    bool success) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());

  if (success)
    client_ = client;

  std::move(init_cb).Run(success);
}

void MojoMjpegDecodeAccelerator::OnDecodeAck(
    int32_t bitstream_buffer_id,
    ::chromeos_camera::MjpegDecodeAccelerator::Error error) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());

  if (!client_)
    return;

  if (error == ::chromeos_camera::MjpegDecodeAccelerator::Error::NO_ERRORS) {
    client_->VideoFrameReady(bitstream_buffer_id);
    return;
  }

  // Only NotifyError once.
  // Client::NotifyError() may trigger deletion of |this|, so calling it needs
  // to be the last thing done on this stack!
  MjpegDecodeAccelerator::Client* client = client_;
  client_ = nullptr;
  client->NotifyError(bitstream_buffer_id, error);
}

void MojoMjpegDecodeAccelerator::OnLostConnectionToJpegDecoder() {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  OnDecodeAck(
      MjpegDecodeAccelerator::kInvalidTaskId,
      ::chromeos_camera::MjpegDecodeAccelerator::Error::PLATFORM_FAILURE);
}

}  // namespace chromeos_camera
