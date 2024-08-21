// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/chromeos_camera/mojo_mjpeg_decode_accelerator_service.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/files/platform_file.h"
#include "base/files/scoped_file.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/chromeos_camera/common/dmabuf.mojom.h"
#include "components/chromeos_camera/dmabuf_utils.h"
#include "media/base/bitstream_buffer.h"
#include "media/base/media_switches.h"
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
        receiver,
    base::RepeatingCallback<void(MjpegDecodeOnBeginFrameCB)> cb) {
  auto* jpeg_decoder = new MojoMjpegDecodeAcceleratorService();
  jpeg_decoder->set_begin_frame_cb_ = std::move(cb);
  mojo::MakeSelfOwnedReceiver(base::WrapUnique(jpeg_decoder),
                              std::move(receiver));
}

struct MojoMjpegDecodeAcceleratorService::DecodeTask {
  DecodeTask(int32_t task_id,
             base::ScopedFD src_dmabuf_fd,
             size_t src_size,
             off_t src_offset,
             scoped_refptr<media::VideoFrame> dst_frame)
      : task_id(task_id),
        src_dmabuf_fd(std::move(src_dmabuf_fd)),
        src_size(src_size),
        src_offset(src_offset),
        dst_frame(dst_frame) {}
  int32_t task_id;
  base::ScopedFD src_dmabuf_fd;
  size_t src_size;
  off_t src_offset;
  scoped_refptr<media::VideoFrame> dst_frame;
};

MojoMjpegDecodeAcceleratorService::MojoMjpegDecodeAcceleratorService()
    : accelerator_initialized_(false), weak_this_factory_(this) {}

MojoMjpegDecodeAcceleratorService::~MojoMjpegDecodeAcceleratorService() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
#if defined(ARCH_CPU_X86_FAMILY) && BUILDFLAG(IS_CHROMEOS)
  if (base::FeatureList::IsEnabled(media::kVSyncMjpegDecoding)) {
    set_begin_frame_cb_.Run(std::nullopt);
    vsync_driven_decoding_ = false;
  }
#endif

  accelerator_.reset();
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

void MojoMjpegDecodeAcceleratorService::InitializeInternal(
    std::vector<GpuMjpegDecodeAcceleratorFactory::CreateAcceleratorCB>
        remaining_accelerator_factory_functions,
    InitializeCallback init_cb) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (remaining_accelerator_factory_functions.empty()) {
    DLOG(ERROR) << "All JPEG accelerators failed to initialize";
    std::move(init_cb).Run(false);
    return;
  }
  accelerator_ = std::move(remaining_accelerator_factory_functions.front())
                     .Run(base::SingleThreadTaskRunner::GetCurrentDefault());
  remaining_accelerator_factory_functions.erase(
      remaining_accelerator_factory_functions.begin());
  if (!accelerator_) {
    OnInitialize(std::move(remaining_accelerator_factory_functions),
                 std::move(init_cb), /*last_initialize_result=*/false);
    return;
  }

#if defined(ARCH_CPU_X86_FAMILY) && BUILDFLAG(IS_CHROMEOS)
  if (base::FeatureList::IsEnabled(media::kVSyncMjpegDecoding)) {
    vsync_driven_decoding_ = true;
    set_begin_frame_cb_.Run(base::BindRepeating(
        &MojoMjpegDecodeAcceleratorService::DecodeWithDmaBufOnBeginFrame,
        weak_this_factory_.GetWeakPtr()));
  }
#endif

  accelerator_->InitializeAsync(
      this, base::BindOnce(&MojoMjpegDecodeAcceleratorService::OnInitialize,
                           weak_this_factory_.GetWeakPtr(),
                           std::move(remaining_accelerator_factory_functions),
                           std::move(init_cb)));
}

void MojoMjpegDecodeAcceleratorService::Initialize(
    InitializeCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // When adding non-chromeos platforms, VideoCaptureGpuJpegDecoder::Initialize
  // needs to be updated.

  InitializeInternal(
      GpuMjpegDecodeAcceleratorFactory::GetAcceleratorFactories(),
      std::move(callback));
}

void MojoMjpegDecodeAcceleratorService::OnInitialize(
    std::vector<GpuMjpegDecodeAcceleratorFactory::CreateAcceleratorCB>
        remaining_accelerator_factory_functions,
    InitializeCallback init_cb,
    bool last_initialize_result) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (last_initialize_result) {
    accelerator_initialized_ = true;
    std::move(init_cb).Run(true);
    return;
  }
  // Note that we can't call InitializeInternal() directly. The reason is that
  // InitializeInternal() may destroy |accelerator_| which could cause a
  // use-after-free if |accelerator_| needs to do more stuff after calling
  // OnInitialize().
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&MojoMjpegDecodeAcceleratorService::InitializeInternal,
                     weak_this_factory_.GetWeakPtr(),
                     std::move(remaining_accelerator_factory_functions),
                     std::move(init_cb)));
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
  // BackWithOwnedSharedMemory() is not executed because
  // MjpegDecodeAccelerator doesn't use shared memory region of the frame.
  // Just attach the video frame so that the mapped memory is valid until the
  // VideoFrame is alive.
  frame->AddDestructionObserver(base::BindOnce(
      [](base::UnsafeSharedMemoryRegion, base::WritableSharedMemoryMapping) {},
      std::move(output_region), std::move(mapping)));

  if (!accelerator_initialized_) {
    NotifyDecodeStatus(
        input_buffer.id(),
        ::chromeos_camera::MjpegDecodeAccelerator::Error::PLATFORM_FAILURE);
    return;
  }
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
      std::move(dst_frame->planes), dst_frame->format, coded_size,
      dst_frame->has_modifier ? dst_frame->modifier
                              : gfx::NativePixmapHandle::kNoModifier);
  if (!frame) {
    LOG(ERROR) << "Failed to create video frame";
    std::move(callback).Run(
        ::chromeos_camera::MjpegDecodeAccelerator::Error::INVALID_ARGUMENT);
    return;
  }

  DCHECK_EQ(mojo_cb_map_.count(task_id), 0u);
  mojo_cb_map_[task_id] = std::move(callback);

  if (!accelerator_initialized_) {
    NotifyDecodeStatus(
        task_id,
        ::chromeos_camera::MjpegDecodeAccelerator::Error::PLATFORM_FAILURE);
    return;
  }
  DCHECK(accelerator_);
  if (!vsync_driven_decoding_) {
    accelerator_->Decode(
        task_id, src_handle.TakeFD(), base::strict_cast<size_t>(src_size),
        base::strict_cast<off_t>(src_offset), std::move(frame));
  } else {
    // Stores task to pending_tasks_ and wait for VSync event to process.
    input_queue_.emplace_back(
        task_id, src_handle.TakeFD(), base::strict_cast<size_t>(src_size),
        base::strict_cast<off_t>(src_offset), std::move(frame));
  }
}

void MojoMjpegDecodeAcceleratorService::DecodeWithDmaBufOnBeginFrame() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  for (auto& input : input_queue_) {
    accelerator_->Decode(input.task_id, std::move(input.src_dmabuf_fd),
                         input.src_size, input.src_offset,
                         std::move(input.dst_frame));
  }

  input_queue_.clear();
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
