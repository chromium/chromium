// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/chromeos_camera/mojo_jpeg_encode_accelerator_service.h"

#include <linux/videodev2.h>
#include <stdint.h>
#include <sys/mman.h>

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/platform_shared_memory_region.h"
#include "base/memory/ptr_util.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/memory/writable_shared_memory_region.h"
#include "base/not_fatal_until.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "components/chromeos_camera/common/dmabuf.mojom.h"
#include "components/chromeos_camera/dmabuf_utils.h"
#include "media/base/video_frame.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/linux/native_pixmap_dmabuf.h"

namespace chromeos_camera {

namespace {

const int kJpegQuality = 90;

media::VideoPixelFormat ToVideoPixelFormat(uint32_t fourcc_fmt) {
  switch (fourcc_fmt) {
    case V4L2_PIX_FMT_NV12:
    case V4L2_PIX_FMT_NV12M:
      return media::PIXEL_FORMAT_NV12;

    case V4L2_PIX_FMT_YUV420:
    case V4L2_PIX_FMT_YUV420M:
      return media::PIXEL_FORMAT_I420;

    case V4L2_PIX_FMT_RGB32:
      return media::PIXEL_FORMAT_BGRA;

    default:
      return media::PIXEL_FORMAT_UNKNOWN;
  }
}

}  // namespace

// static
void MojoJpegEncodeAcceleratorService::Create(
    mojo::PendingReceiver<chromeos_camera::mojom::JpegEncodeAccelerator>
        receiver) {
  auto* jpeg_encoder = new MojoJpegEncodeAcceleratorService();
  mojo::MakeSelfOwnedReceiver(base::WrapUnique(jpeg_encoder),
                              std::move(receiver));
}

MojoJpegEncodeAcceleratorService::MojoJpegEncodeAcceleratorService()
    : accelerator_initialized_(false), weak_this_factory_(this) {}

MojoJpegEncodeAcceleratorService::~MojoJpegEncodeAcceleratorService() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  accelerator_.reset();
}

void MojoJpegEncodeAcceleratorService::VideoFrameReady(
    int32_t task_id,
    size_t encoded_picture_size) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  NotifyEncodeStatus(
      task_id, encoded_picture_size,
      ::chromeos_camera::JpegEncodeAccelerator::Status::ENCODE_OK);
}

void MojoJpegEncodeAcceleratorService::NotifyError(
    int32_t task_id,
    ::chromeos_camera::JpegEncodeAccelerator::Status error) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  NotifyEncodeStatus(task_id, 0, error);
}

void MojoJpegEncodeAcceleratorService::InitializeInternal(
    std::vector<GpuJpegEncodeAcceleratorFactory::CreateAcceleratorCB>
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
    OnInitialize(
        std::move(remaining_accelerator_factory_functions), std::move(init_cb),
        /*last_initialize_result=*/
        ::chromeos_camera::JpegEncodeAccelerator::HW_JPEG_ENCODE_NOT_SUPPORTED);
    return;
  }
  accelerator_->InitializeAsync(
      this, base::BindOnce(&MojoJpegEncodeAcceleratorService::OnInitialize,
                           weak_this_factory_.GetWeakPtr(),
                           std::move(remaining_accelerator_factory_functions),
                           std::move(init_cb)));
}

void MojoJpegEncodeAcceleratorService::Initialize(InitializeCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // When adding non-chromeos platforms, VideoCaptureGpuJpegEncoder::Initialize
  // needs to be updated.

  InitializeInternal(GpuJpegEncodeAcceleratorFactory::GetAcceleratorFactories(),
                     std::move(callback));
}

void MojoJpegEncodeAcceleratorService::OnInitialize(
    std::vector<GpuJpegEncodeAcceleratorFactory::CreateAcceleratorCB>
        remaining_accelerator_factory_functions,
    InitializeCallback init_cb,
    chromeos_camera::JpegEncodeAccelerator::Status last_initialize_result) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (last_initialize_result ==
      ::chromeos_camera::JpegEncodeAccelerator::ENCODE_OK) {
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
      base::BindOnce(&MojoJpegEncodeAcceleratorService::InitializeInternal,
                     weak_this_factory_.GetWeakPtr(),
                     std::move(remaining_accelerator_factory_functions),
                     std::move(init_cb)));
}

void MojoJpegEncodeAcceleratorService::EncodeWithFD(
    int32_t task_id,
    mojo::ScopedHandle input_handle,
    uint32_t input_buffer_size,
    int32_t coded_size_width,
    int32_t coded_size_height,
    mojo::ScopedHandle exif_handle,
    uint32_t exif_buffer_size,
    mojo::ScopedHandle output_handle,
    uint32_t output_buffer_size,
    EncodeWithFDCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!accelerator_initialized_) {
    std::move(callback).Run(
        task_id, 0,
        ::chromeos_camera::JpegEncodeAccelerator::Status::PLATFORM_FAILURE);
    return;
  }

  base::ScopedPlatformFile input_fd;
  base::ScopedPlatformFile exif_fd;
  base::ScopedPlatformFile output_fd;
  MojoResult result;

  if (coded_size_width <= 0 || coded_size_height <= 0) {
    std::move(callback).Run(
        task_id, 0,
        ::chromeos_camera::JpegEncodeAccelerator::Status::INVALID_ARGUMENT);
    return;
  }

  result = mojo::UnwrapPlatformFile(std::move(input_handle), &input_fd);
  if (result != MOJO_RESULT_OK) {
    std::move(callback).Run(
        task_id, 0,
        ::chromeos_camera::JpegEncodeAccelerator::Status::PLATFORM_FAILURE);
    return;
  }

  result = mojo::UnwrapPlatformFile(std::move(exif_handle), &exif_fd);
  if (result != MOJO_RESULT_OK) {
    std::move(callback).Run(
        task_id, 0,
        ::chromeos_camera::JpegEncodeAccelerator::Status::PLATFORM_FAILURE);
    return;
  }

  result = mojo::UnwrapPlatformFile(std::move(output_handle), &output_fd);
  if (result != MOJO_RESULT_OK) {
    std::move(callback).Run(
        task_id, 0,
        ::chromeos_camera::JpegEncodeAccelerator::Status::PLATFORM_FAILURE);
    return;
  }
  // TODO(b/3832599): Make |input_region| read-only.
  base::WritableSharedMemoryRegion writable_input_region =
      base::WritableSharedMemoryRegion::Deserialize(
          base::subtle::PlatformSharedMemoryRegion::Take(
              std::move(input_fd),
              base::subtle::PlatformSharedMemoryRegion::Mode::kWritable,
              input_buffer_size, base::UnguessableToken::Create()));
  base::ReadOnlySharedMemoryRegion input_region =
      base::WritableSharedMemoryRegion::ConvertToReadOnly(
          std::move(writable_input_region));

  base::UnsafeSharedMemoryRegion output_shm_region =
      base::UnsafeSharedMemoryRegion::Deserialize(
          base::subtle::PlatformSharedMemoryRegion::Take(
              std::move(output_fd),
              base::subtle::PlatformSharedMemoryRegion::Mode::kUnsafe,
              output_buffer_size, base::UnguessableToken::Create()));

  media::BitstreamBuffer output_buffer(task_id, std::move(output_shm_region),
                                       output_buffer_size);
  std::unique_ptr<media::BitstreamBuffer> exif_buffer;
  if (exif_buffer_size > 0) {
    base::UnsafeSharedMemoryRegion exif_shm_region =
        base::UnsafeSharedMemoryRegion::Deserialize(
            base::subtle::PlatformSharedMemoryRegion::Take(
                std::move(exif_fd),
                base::subtle::PlatformSharedMemoryRegion::Mode::kUnsafe,
                exif_buffer_size, base::UnguessableToken::Create()));
    exif_buffer = std::make_unique<media::BitstreamBuffer>(
        task_id, std::move(exif_shm_region), exif_buffer_size);
  }
  gfx::Size coded_size(coded_size_width, coded_size_height);

  if (encode_cb_map_.find(task_id) != encode_cb_map_.end()) {
    mojo::ReportBadMessage("task_id is already registered in encode_cb_map_");
    return;
  }
  auto wrapped_callback = base::BindOnce(
      [](int32_t task_id, EncodeWithFDCallback callback,
         uint32_t encoded_picture_size,
         ::chromeos_camera::JpegEncodeAccelerator::Status error) {
        std::move(callback).Run(task_id, encoded_picture_size, error);
      },
      task_id, std::move(callback));
  encode_cb_map_.emplace(task_id, std::move(wrapped_callback));

  base::ReadOnlySharedMemoryMapping input_mapping = input_region.Map();
  if (!input_mapping.IsValid()) {
    DLOG(ERROR) << "Could not map input shared memory for buffer id "
                << task_id;
    NotifyEncodeStatus(
        task_id, 0,
        ::chromeos_camera::JpegEncodeAccelerator::Status::PLATFORM_FAILURE);
    return;
  }

  const uint8_t* input_shm_memory =
      input_mapping.GetMemoryAsSpan<uint8_t>().data();
  scoped_refptr<media::VideoFrame> frame = media::VideoFrame::WrapExternalData(
      media::PIXEL_FORMAT_I420,  // format
      coded_size,                // coded_size
      gfx::Rect(coded_size),     // visible_rect
      coded_size,                // natural_size
      input_shm_memory,          // data
      input_buffer_size,         // data_size
      base::TimeDelta());        // timestamp
  if (!frame.get()) {
    LOG(ERROR) << "Could not create VideoFrame for buffer id " << task_id;
    NotifyEncodeStatus(
        task_id, 0,
        ::chromeos_camera::JpegEncodeAccelerator::Status::PLATFORM_FAILURE);
    return;
  }
  frame->BackWithOwnedSharedMemory(std::move(input_region),
                                   std::move(input_mapping));

  DCHECK(accelerator_);
  accelerator_->Encode(frame, kJpegQuality, exif_buffer.get(),
                       std::move(output_buffer));
}

void MojoJpegEncodeAcceleratorService::EncodeWithDmaBuf(
    int32_t task_id,
    uint32_t input_format,
    std::vector<chromeos_camera::mojom::DmaBufPlanePtr> input_planes,
    std::vector<chromeos_camera::mojom::DmaBufPlanePtr> output_planes,
    mojo::ScopedHandle exif_handle,
    uint32_t exif_buffer_size,
    int32_t coded_size_width,
    int32_t coded_size_height,
    int32_t quality,
    bool has_input_modifier,
    uint64_t input_modifier,
    EncodeWithDmaBufCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!accelerator_initialized_) {
    std::move(callback).Run(
        0, ::chromeos_camera::JpegEncodeAccelerator::Status::PLATFORM_FAILURE);
    return;
  }

  const gfx::Size coded_size(coded_size_width, coded_size_height);
  if (coded_size.IsEmpty()) {
    std::move(callback).Run(
        0, ::chromeos_camera::JpegEncodeAccelerator::Status::INVALID_ARGUMENT);
    return;
  }
  if (encode_cb_map_.find(task_id) != encode_cb_map_.end()) {
    mojo::ReportBadMessage("task_id is already registered in encode_cb_map_");
    return;
  }

  base::ScopedPlatformFile exif_fd;
  auto result = mojo::UnwrapPlatformFile(std::move(exif_handle), &exif_fd);
  if (result != MOJO_RESULT_OK) {
    std::move(callback).Run(
        0, ::chromeos_camera::JpegEncodeAccelerator::Status::PLATFORM_FAILURE);
    return;
  }

  auto input_video_frame = ConstructVideoFrame(
      std::move(input_planes), ToVideoPixelFormat(input_format), coded_size,
      has_input_modifier ? input_modifier
                         : gfx::NativePixmapHandle::kNoModifier);
  if (!input_video_frame) {
    std::move(callback).Run(
        0, ::chromeos_camera::JpegEncodeAccelerator::Status::PLATFORM_FAILURE);
    return;
  }
  auto output_video_frame = ConstructVideoFrame(
      std::move(output_planes), media::PIXEL_FORMAT_MJPEG, coded_size);
  if (!output_video_frame) {
    std::move(callback).Run(
        0, ::chromeos_camera::JpegEncodeAccelerator::Status::PLATFORM_FAILURE);
    return;
  }
  std::unique_ptr<media::BitstreamBuffer> exif_buffer;
  if (exif_buffer_size > 0) {
    // Currently we use our zero-based |task_id| as id of |exif_buffer| to track
    // the encode task process from both Chrome OS and Chrome side.
    base::UnsafeSharedMemoryRegion exif_shm_region =
        base::UnsafeSharedMemoryRegion::Deserialize(
            base::subtle::PlatformSharedMemoryRegion::Take(
                std::move(exif_fd),
                base::subtle::PlatformSharedMemoryRegion::Mode::kUnsafe,
                exif_buffer_size, base::UnguessableToken::Create()));
    exif_buffer = std::make_unique<media::BitstreamBuffer>(
        task_id, std::move(exif_shm_region), exif_buffer_size);
  }
  encode_cb_map_.emplace(task_id, std::move(callback));

  DCHECK(accelerator_);
  accelerator_->EncodeWithDmaBuf(input_video_frame, output_video_frame, quality,
                                 task_id, exif_buffer.get());
}

void MojoJpegEncodeAcceleratorService::NotifyEncodeStatus(
    int32_t task_id,
    size_t encoded_picture_size,
    ::chromeos_camera::JpegEncodeAccelerator::Status error) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  auto iter = encode_cb_map_.find(task_id);
  CHECK(iter != encode_cb_map_.end(), base::NotFatalUntil::M130);
  EncodeWithDmaBufCallback encode_cb = std::move(iter->second);
  encode_cb_map_.erase(iter);
  std::move(encode_cb).Run(encoded_picture_size, error);
}

}  // namespace chromeos_camera
