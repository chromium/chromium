// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/chromeos_camera/fake_mjpeg_decode_accelerator.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "media/base/unaligned_shared_memory.h"
#include "media/base/video_frame.h"
#include "media/base/video_types.h"

namespace chromeos_camera {

FakeMjpegDecodeAccelerator::FakeMjpegDecodeAccelerator(
    const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner)
    : client_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      io_task_runner_(std::move(io_task_runner)),
      decoder_thread_("FakeMjpegDecoderThread") {}

FakeMjpegDecodeAccelerator::~FakeMjpegDecodeAccelerator() {
  DCHECK(client_task_runner_->BelongsToCurrentThread());
}

bool FakeMjpegDecodeAccelerator::Initialize(
    MjpegDecodeAccelerator::Client* client) {
  DCHECK(client_task_runner_->BelongsToCurrentThread());
  client_ = client;

  if (!decoder_thread_.Start()) {
    DLOG(ERROR) << "Failed to start decoding thread.";
    return false;
  }
  decoder_task_runner_ = decoder_thread_.task_runner();

  return true;
}

void FakeMjpegDecodeAccelerator::Decode(
    media::BitstreamBuffer bitstream_buffer,
    scoped_refptr<media::VideoFrame> video_frame) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  auto src_shm = std::make_unique<media::UnalignedSharedMemory>(
      bitstream_buffer.TakeRegion(), bitstream_buffer.size(),
      false /* read_only */);
  if (!src_shm->MapAt(bitstream_buffer.offset(), bitstream_buffer.size())) {
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
                     std::move(video_frame), base::Passed(&src_shm)));
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
    std::unique_ptr<media::UnalignedSharedMemory> src_shm) {
  DCHECK(decoder_task_runner_->BelongsToCurrentThread());

  // Do not actually decode the Jpeg data.
  // Instead, just fill the output buffer with zeros.
  size_t allocation_size = media::VideoFrame::AllocationSize(
      media::PIXEL_FORMAT_I420, video_frame->coded_size());
  memset(video_frame->data(0), 0, allocation_size);

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
