// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/chromeos_camera/mojo_mjpeg_decode_accelerator_service.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/platform_file.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/numerics/safe_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "components/chromeos_camera/common/dmabuf.mojom.h"
#include "components/chromeos_camera/dmabuf_utils.h"
#include "media/base/bitstream_buffer.h"
#include "media/base/video_frame.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace {

bool VerifyDecodeParams(const gfx::Size& coded_size,
                        mojo::ScopedSharedBufferHandle* output_handle,
                        uint32_t output_buffer_size) {
  const int kJpegMaxDimension = UINT16_MAX;
  if (coded_size.IsEmpty() || coded_size.width() > kJpegMaxDimension ||
      coded_size.height() > kJpegMaxDimension) {
    LOG(ERROR) << "invalid coded_size " << coded_size.ToString();
    return false;
  }

  if (!output_handle->is_valid()) {
    LOG(ERROR) << "invalid output_handle";
    return false;
  }

  uint32_t allocation_size =
      media::VideoFrame::AllocationSize(media::PIXEL_FORMAT_I420, coded_size);
  if (output_buffer_size < allocation_size) {
    DLOG(ERROR) << "output_buffer_size is too small: " << output_buffer_size
                << ". It needs: " << allocation_size;
    return false;
  }

  return true;
}

}  // namespace

namespace chromeos_camera {

// static
void MojoMjpegDecodeAcceleratorService::Create(
    mojo::PendingReceiver<chromeos_camera::mojom::MjpegDecodeAccelerator>
        receiver) {
  auto* jpeg_decoder = new MojoMjpegDecodeAcceleratorService();
  mojo::MakeSelfOwnedReceiver(base::WrapUnique(jpeg_decoder),
                              std::move(receiver));
}

MojoMjpegDecodeAcceleratorService::MojoMjpegDecodeAcceleratorService()
    : accelerator_factory_functions_(
          GpuMjpegDecodeAcceleratorFactory::GetAcceleratorFactories()) {}

MojoMjpegDecodeAcceleratorService::~MojoMjpegDecodeAcceleratorService() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void MojoMjpegDecodeAcceleratorService::VideoFrameReady(
    int32_t bitstream_buffer_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  NotifyDecodeStatus(
      bitstream_buffer_id,
      ::chromeos_camera::MjpegDecodeAccelerator::Error::NO_ERRORS);
}

void MojoMjpegDecodeAcceleratorService::NotifyError(
    int32_t bitstream_buffer_id,
    ::chromeos_camera::MjpegDecodeAccelerator::Error error) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  NotifyDecodeStatus(bitstream_buffer_id, error);
}

void MojoMjpegDecodeAcceleratorService::Initialize(
    InitializeCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // When adding non-chromeos platforms, VideoCaptureGpuJpegDecoder::Initialize
  // needs to be updated.

  std::unique_ptr<::chromeos_camera::MjpegDecodeAccelerator> accelerator;
  for (auto& create_jda_function : accelerator_factory_functions_) {
    std::unique_ptr<::chromeos_camera::MjpegDecodeAccelerator> tmp_accelerator =
        std::move(create_jda_function).Run(base::ThreadTaskRunnerHandle::Get());
    if (tmp_accelerator && tmp_accelerator->Initialize(this)) {
      accelerator = std::move(tmp_accelerator);
      break;
    }
  }

  if (!accelerator) {
    DLOG(ERROR) << "JPEG accelerator initialization failed";
    std::move(callback).Run(false);
    return;
  }

  accelerator_ = std::move(accelerator);
  std::move(callback).Run(true);
}

void MojoMjpegDecodeAcceleratorService::Decode(
    media::BitstreamBuffer input_buffer,
    const gfx::Size& coded_size,
    mojo::ScopedSharedBufferHandle output_handle,
    uint32_t output_buffer_size,
    DecodeCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  TRACE_EVENT0("jpeg", "MojoMjpegDecodeAcceleratorService::Decode");

  DCHECK_EQ(mojo_cb_map_.count(input_buffer.id()), 0u);
  if (mojo_cb_map_.count(input_buffer.id()) != 0) {
    NotifyDecodeStatus(
        input_buffer.id(),
        ::chromeos_camera::MjpegDecodeAccelerator::Error::INVALID_ARGUMENT);
    return;
  }

  mojo_cb_map_[input_buffer.id()] =
      base::BindOnce(std::move(callback), input_buffer.id());

  if (!VerifyDecodeParams(coded_size, &output_handle, output_buffer_size)) {
    NotifyDecodeStatus(
        input_buffer.id(),
        ::chromeos_camera::MjpegDecodeAccelerator::Error::INVALID_ARGUMENT);
    return;
  }

  base::UnsafeSharedMemoryRegion output_region =
      mojo::UnwrapUnsafeSharedMemoryRegion(std::move(output_handle));
  DCHECK(output_region.IsValid());
  DCHECK_GE(output_region.GetSize(), output_buffer_size);

  base::WritableSharedMemoryMapping mapping =
      output_region.MapAt(0, output_buffer_size);
  if (!mapping.IsValid()) {
    LOG(ERROR) << "Could not map output shared memory for input buffer id "
               << input_buffer.id();
    NotifyDecodeStatus(
        input_buffer.id(),
        ::chromeos_camera::MjpegDecodeAccelerator::Error::PLATFORM_FAILURE);
    return;
  }

  uint8_t* shm_memory = mapping.GetMemoryAsSpan<uint8_t>().data();
  scoped_refptr<media::VideoFrame> frame = media::VideoFrame::WrapExternalData(
      media::PIXEL_FORMAT_I420,  // format
      coded_size,                // coded_size
      gfx::Rect(coded_size),     // visible_rect
      coded_size,                // natural_size
      shm_memory,                // data
      output_buffer_size,        // data_size
      base::TimeDelta());        // timestamp
  if (!frame.get()) {
    LOG(ERROR) << "Could not create VideoFrame for input buffer id "
               << input_buffer.id();
    NotifyDecodeStatus(
        input_buffer.id(),
        ::chromeos_camera::MjpegDecodeAccelerator::Error::PLATFORM_FAILURE);
    return;
  }
  frame->BackWithOwnedSharedMemory(std::move(output_region),
                                   std::move(mapping));

  DCHECK(accelerator_);
  accelerator_->Decode(std::move(input_buffer), frame);
}

void MojoMjpegDecodeAcceleratorService::DecodeWithDmaBuf(
    int32_t task_id,
    mojo::ScopedHandle src_dmabuf_fd,
    uint32_t src_size,
    uint32_t src_offset,
    mojom::DmaBufVideoFramePtr dst_frame,
    DecodeWithDmaBufCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  TRACE_EVENT0("jpeg", __FUNCTION__);

  if (src_size == 0) {
    LOG(ERROR) << "Input buffer size should be positive";
    std::move(callback).Run(
        ::chromeos_camera::MjpegDecodeAccelerator::Error::INVALID_ARGUMENT);
    return;
  }
  mojo::PlatformHandle src_handle =
      mojo::UnwrapPlatformHandle(std::move(src_dmabuf_fd));
  if (!src_handle.is_valid()) {
    LOG(ERROR) << "Invalid input DMA-buf FD";
    std::move(callback).Run(
        ::chromeos_camera::MjpegDecodeAccelerator::Error::INVALID_ARGUMENT);
    return;
  }

  const gfx::Size coded_size(base::checked_cast<int>(dst_frame->coded_width),
                             base::checked_cast<int>(dst_frame->coded_height));
  scoped_refptr<media::VideoFrame> frame = ConstructVideoFrame(
      std::move(dst_frame->planes), dst_frame->format, coded_size);
  if (!frame) {
    LOG(ERROR) << "Failed to create video frame";
    std::move(callback).Run(
        ::chromeos_camera::MjpegDecodeAccelerator::Error::INVALID_ARGUMENT);
    return;
  }

  DCHECK_EQ(mojo_cb_map_.count(task_id), 0u);
  mojo_cb_map_[task_id] = std::move(callback);

  DCHECK(accelerator_);
  accelerator_->Decode(task_id, src_handle.TakeFD(),
                       base::strict_cast<size_t>(src_size),
                       base::strict_cast<off_t>(src_offset), std::move(frame));
}

void MojoMjpegDecodeAcceleratorService::Uninitialize() {
  // TODO(c.padhi): see http://crbug.com/699255.
  NOTIMPLEMENTED();
}

void MojoMjpegDecodeAcceleratorService::NotifyDecodeStatus(
    int32_t bitstream_buffer_id,
    ::chromeos_camera::MjpegDecodeAccelerator::Error error) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  auto iter = mojo_cb_map_.find(bitstream_buffer_id);
  DCHECK(iter != mojo_cb_map_.end());
  if (iter == mojo_cb_map_.end()) {
    // Silently ignoring abnormal case.
    return;
  }

  MojoCallback mojo_cb = std::move(iter->second);
  mojo_cb_map_.erase(iter);
  std::move(mojo_cb).Run(error);
}

}  // namespace chromeos_camera
