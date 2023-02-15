// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/chromeos_camera/fake_mjpeg_decode_accelerator.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"
#include "media/base/video_frame.h"
#include "media/base/video_types.h"

namespace chromeos_camera {

FakeMjpegDecodeAccelerator::FakeMjpegDecodeAccelerator(
    const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner)
    : client_task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()),
      io_task_runner_(std::move(io_task_runner)),
      decoder_thread_("FakeMjpegDecoderThread") {}

FakeMjpegDecodeAccelerator::~FakeMjpegDecodeAccelerator() {
  DCHECK(client_task_runner_->BelongsToCurrentThread());
}

void FakeMjpegDecodeAccelerator::InitializeOnTaskRunner(
    MjpegDecodeAccelerator::Client* client,
    InitCB init_cb) {
  DCHECK(client_task_runner_->BelongsToCurrentThread());
  client_ = client;

  if (!decoder_thread_.Start()) {
    DLOG(ERROR) << "Failed to start decoding thread.";
    std::move(init_cb).Run(false);
    return;
  }

  decoder_task_runner_ = decoder_thread_.task_runner();
  std::move(init_cb).Run(true);
}

void FakeMjpegDecodeAccelerator::InitializeAsync(
    MjpegDecodeAccelerator::Client* client,
    InitCB init_cb) {
  DCHECK(client_task_runner_->BelongsToCurrentThread());

  client_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&FakeMjpegDecodeAccelerator::InitializeOnTaskRunner,
                     weak_factory_.GetWeakPtr(), client,
                     base::BindPostTaskToCurrentDefault(std::move(init_cb))));
}

void FakeMjpegDecodeAccelerator::Decode(
    media::BitstreamBuffer bitstream_buffer,
    scoped_refptr<media::VideoFrame> video_frame) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  base::UnsafeSharedMemoryRegion src_shm_region = bitstream_buffer.TakeRegion();
  base::WritableSharedMemoryMapping src_shm_mapping =
      src_shm_region.MapAt(bitstream_buffer.offset(), bitstream_buffer.size());
  if (!src_shm_mapping.IsValid()) {
    DLOG(ERROR) << "Unable to map shared memory in FakeMjpegDecodeAccelerator";
    NotifyError(bitstream_buffer.id(),
                MjpegDecodeAccelerator::UNREADABLE_INPUT);
    return;
  }

  // Unretained |this| is safe because |this| owns |decoder_thread_|.
  decoder_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&FakeMjpegDecodeAccelerator::DecodeOnDecoderThread,
                     base::Unretained(this), bitstream_buffer.id(),
                     std::move(video_frame), std::move(src_shm_mapping)));
}

void FakeMjpegDecodeAccelerator::Decode(
    int32_t task_id,
    base::ScopedFD src_dmabuf_fd,
    size_t src_size,
    off_t src_offset,
    scoped_refptr<media::VideoFrame> dst_frame) {
  NOTIMPLEMENTED();
}

void FakeMjpegDecodeAccelerator::DecodeOnDecoderThread(
    int32_t task_id,
    scoped_refptr<media::VideoFrame> video_frame,
    base::WritableSharedMemoryMapping src_shm_mapping) {
  DCHECK(decoder_task_runner_->BelongsToCurrentThread());

  // Do not actually decode the Jpeg data.
  // Instead, just fill the output buffer with zeros.
  size_t allocation_size = media::VideoFrame::AllocationSize(
      media::PIXEL_FORMAT_I420, video_frame->coded_size());
  memset(video_frame->writable_data(0), 0, allocation_size);

  client_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&FakeMjpegDecodeAccelerator::OnDecodeDoneOnClientThread,
                     weak_factory_.GetWeakPtr(), task_id));
}

bool FakeMjpegDecodeAccelerator::IsSupported() {
  return true;
}

void FakeMjpegDecodeAccelerator::NotifyError(int32_t task_id, Error error) {
  client_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&FakeMjpegDecodeAccelerator::NotifyErrorOnClientThread,
                     weak_factory_.GetWeakPtr(), task_id, error));
}

void FakeMjpegDecodeAccelerator::NotifyErrorOnClientThread(int32_t task_id,
                                                           Error error) {
  DCHECK(client_task_runner_->BelongsToCurrentThread());
  client_->NotifyError(task_id, error);
}

void FakeMjpegDecodeAccelerator::OnDecodeDoneOnClientThread(int32_t task_id) {
  DCHECK(client_task_runner_->BelongsToCurrentThread());
  client_->VideoFrameReady(task_id);
}

}  // namespace chromeos_camera
