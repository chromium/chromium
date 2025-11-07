// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/experiences/arc/video_accelerator/gpu_arc_video_encode_accelerator.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/platform_shared_memory_region.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "base/posix/eintr_wrapper.h"
#include "base/system/sys_info.h"
#include "base/task/bind_post_task.h"
#include "chromeos/ash/experiences/arc/arc_features.h"
#include "chromeos/ash/experiences/arc/video_accelerator/arc_video_accelerator_util.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/ipc/service/arc_shared_image_interface.h"
#include "media/base/bitrate.h"
#include "media/base/bitstream_buffer.h"
#include "media/base/color_plane_layout.h"
#include "media/base/format_utils.h"
#include "media/base/media_log.h"
#include "media/base/video_codecs.h"
#include "media/base/video_frame.h"
#include "media/base/video_types.h"
#include "media/gpu/buffer_validation.h"
#include "media/gpu/gpu_video_encode_accelerator_factory.h"
#include "media/gpu/macros.h"
#include "media/video/video_encode_accelerator.h"
#include "mojo/public/cpp/bindings/type_converter.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "ui/ozone/public/ozone_platform.h"

namespace arc {

namespace {

// Maximum number of concurrent ARC video clients.
// Currently we have no way to know the resources are not enough to create more
// VEAs. Currently this value is selected as 40 instances are enough to pass
// the CTS tests.
constexpr size_t kMaxConcurrentClients = 8;

bool ForceL1T3Encode(const media::VideoEncodeAccelerator::Config& config) {
  if (media::VideoCodecProfileToVideoCodec(config.output_profile) !=
      media::VideoCodec::kH264) {
    return false;
  }
  // We force to encode in L1T3 for H264 stream in ChromeOS Selphie.
  static bool isSelphie = base::SysInfo::GetLsbReleaseBoard() == "selphie";

  return isSelphie;
}
}  // namespace

// static
size_t GpuArcVideoEncodeAccelerator::client_count_ = 0;

GpuArcVideoEncodeAccelerator::GpuArcVideoEncodeAccelerator(
    scoped_refptr<gpu::ArcSharedImageInterface> sii,
    const gpu::GpuPreferences& gpu_preferences,
    const gpu::GpuDriverBugWorkarounds& gpu_workarounds)
    : sii_(sii),
      gpu_preferences_(gpu_preferences),
      gpu_workarounds_(gpu_workarounds),
      bitstream_buffer_serial_(0),
      client_native_pixmap_factory_(
          ui::CreateClientNativePixmapFactoryOzone()) {}

GpuArcVideoEncodeAccelerator::~GpuArcVideoEncodeAccelerator() {
  // Normally |client_count_| should always be > 0 if vea_ is set, but if it
  // isn't and we underflow then we won't be able to create any new decoder
  // forever. (b/173700103). So let's use an extra check to avoid this...
  if (accelerator_ && client_count_ > 0) {
    client_count_--;
  }
}

// VideoEncodeAccelerator::Client implementation.
void GpuArcVideoEncodeAccelerator::RequireBitstreamBuffers(
    unsigned int input_count,
    const gfx::Size& coded_size,
    size_t output_buffer_size) {
  DVLOGF(2) << "input_count=" << input_count
            << ", coded_size=" << coded_size.ToString()
            << ", output_buffer_size=" << output_buffer_size;
  DCHECK(client_);
  coded_size_ = coded_size;
  client_->RequireBitstreamBuffers(input_count, coded_size, output_buffer_size);
}

void GpuArcVideoEncodeAccelerator::BitstreamBufferReady(
    int32_t bitstream_buffer_id,
    const media::BitstreamBufferMetadata& metadata) {
  DVLOGF(2) << "id=" << bitstream_buffer_id;
  DCHECK(client_);
  auto iter = use_bitstream_cbs_.find(bitstream_buffer_id);
  CHECK(iter != use_bitstream_cbs_.end());
  std::move(iter->second)
      .Run(metadata.payload_size_bytes, metadata.key_frame,
           metadata.timestamp.InMicroseconds());
  use_bitstream_cbs_.erase(iter);
}

void GpuArcVideoEncodeAccelerator::NotifyErrorStatus(
    const media::EncoderStatus& status) {
  LOG(ERROR) << "NotifyErrorStatus() is called, code="
             << static_cast<int32_t>(status.code())
             << ", message=" << status.message();
  DCHECK(client_);
  client_->NotifyError(
      mojom::VideoEncodeAccelerator::Error::kPlatformFailureError);
}

// ::arc::mojom::VideoEncodeAccelerator implementation.
void GpuArcVideoEncodeAccelerator::GetSupportedProfiles(
    GetSupportedProfilesCallback callback) {
  std::move(callback).Run(
      media::GpuVideoEncodeAcceleratorFactory::GetSupportedProfiles(
          gpu_preferences_, gpu_workarounds_, gpu::GPUInfo::GPUDevice()));
}

void GpuArcVideoEncodeAccelerator::Initialize(
    const media::VideoEncodeAccelerator::Config& config,
    mojo::PendingRemote<mojom::VideoEncodeClient> client,
    InitializeCallback callback) {
  auto result = InitializeTask(config, std::move(client));
  std::move(callback).Run(result);
}

mojom::VideoEncodeAccelerator::Result
GpuArcVideoEncodeAccelerator::InitializeTask(
    const media::VideoEncodeAccelerator::Config& config,
    mojo::PendingRemote<mojom::VideoEncodeClient> client) {
  DVLOGF(2) << config.AsHumanReadableString();

  if (config.input_format != media::PIXEL_FORMAT_NV12) {
    VLOGF(1) << "Unsupported pixel format: " << config.input_format;
    return mojom::VideoEncodeAccelerator::Result::kInvalidArgumentError;
  }

  if (client_count_ >= kMaxConcurrentClients) {
    VLOGF(1) << "Reject to Initialize() due to too many clients: "
             << client_count_;
    return mojom::VideoEncodeAccelerator::Result::kInsufficientResourcesError;
  }

  if (base::FeatureList::IsEnabled(kVideoEncodeUseMappableSI) && !sii_) {
    DLOG(ERROR) << "Was passed null SharedImageInterface on construction";
    return mojom::VideoEncodeAccelerator::Result::kPlatformFailureError;
  }

  if (ForceL1T3Encode(config)) {
    auto& cfg = const_cast<media::VideoEncodeAccelerator::Config&>(config);
    cfg.spatial_layers.clear();
    cfg.spatial_layers.push_back(
        media::VideoEncodeAccelerator::Config::SpatialLayer{
            .width = config.input_visible_size.width(),
            .height = config.input_visible_size.height(),
            .bitrate_bps = config.bitrate.target_bps(),
            .framerate = config.framerate,
            .max_qp = 0,  // Not used by ChromeOS VEA.
            .num_of_temporal_layers = 3,
        });
    DVLOGF(1) << "Enforce L1T3 encoding for H264 stream for ARC";
  }

  visible_size_ = config.input_visible_size;
  accelerator_.reset();
  auto accelerator_or_error =
      media::GpuVideoEncodeAcceleratorFactory::CreateVEA(
          config, this, gpu_preferences_, gpu_workarounds_,
          gpu::GPUInfo::GPUDevice());
  if (!accelerator_or_error.has_value()) {
    DLOG(ERROR) << "Failed to create a VideoEncodeAccelerator.";
    return mojom::VideoEncodeAccelerator::Result::kPlatformFailureError;
  }
  accelerator_ = std::move(accelerator_or_error).value();

  client_.Bind(std::move(client));

  client_count_++;
  VLOGF(2) << "Number of concurrent clients: " << client_count_;
  return mojom::VideoEncodeAccelerator::Result::kSuccess;
}

void GpuArcVideoEncodeAccelerator::Encode(
    media::VideoPixelFormat format,
    mojo::ScopedHandle handle,
    std::vector<::arc::VideoFramePlane> planes,
    int64_t timestamp,
    bool force_keyframe,
    EncodeCallback callback) {
  DVLOGF(2) << "timestamp=" << timestamp;
  if (!accelerator_) {
    DLOG(ERROR) << "Accelerator is not initialized.";
    return;
  }

  if (planes.empty()) {  // EOS
    accelerator_->Encode(media::VideoFrame::CreateEOSFrame(), force_keyframe);
    return;
  }

  base::ScopedFD fd = UnwrapFdFromMojoHandle(std::move(handle));
  if (!fd.is_valid()) {
    client_->NotifyError(Error::kPlatformFailureError);
    return;
  }

  if (format != media::PIXEL_FORMAT_NV12) {
    DLOG(ERROR) << "Formats other than NV12 are unsupported. format=" << format;
    client_->NotifyError(Error::kInvalidArgumentError);
    return;
  }

  std::vector<base::ScopedFD> fds = DuplicateFD(std::move(fd), planes.size());
  if (fds.empty()) {
    DLOG(ERROR) << "Failed to duplicate fd";
    client_->NotifyError(Error::kInvalidArgumentError);
    return;
  }
  auto gmb_handle =
      CreateGpuMemoryBufferHandle(format, gfx::NativePixmapHandle::kNoModifier,
                                  coded_size_, std::move(fds), planes);
  if (!gmb_handle) {
    DLOG(ERROR) << "Failed to create GpuMemoryBufferHandle";
    client_->NotifyError(Error::kInvalidArgumentError);
    return;
  }

  std::optional<viz::SharedImageFormat> si_format =
      VideoPixelFormatToSharedImageFormat(format);
  if (!si_format) {
    DLOG(ERROR) << "Unexpected si_format";
    client_->NotifyError(Error::kInvalidArgumentError);
    return;
  }
  scoped_refptr<media::VideoFrame> frame;
  if (base::FeatureList::IsEnabled(kVideoEncodeUseMappableSI)) {
    auto shared_image = sii_->CreateSharedImage(
        {*si_format, visible_size_, gfx::ColorSpace(),
         gpu::SHARED_IMAGE_USAGE_CPU_ONLY_READ_WRITE,
         "GpuArcVideoEncodeAccelerator"},
        gpu::kNullSurfaceHandle,
        gfx::BufferUsage::VEA_READ_CAMERA_AND_CPU_READ_WRITE,
        std::move(gmb_handle).value());
    if (!shared_image) {
      DLOG(ERROR) << "Failed to create mappable SharedImage";
      client_->NotifyError(Error::kInvalidArgumentError);
    }

    frame = media::VideoFrame::WrapMappableSharedImage(
        std::move(shared_image), gpu::SyncToken(), base::NullCallback(),
        gfx::Rect(visible_size_), visible_size_, base::Microseconds(timestamp));
  } else {
    frame = media::VideoFrame::WrapExternalGpuMemoryBufferHandle(
        gfx::Rect(visible_size_), visible_size_,
        client_native_pixmap_factory_.get(), std::move(gmb_handle).value(),
        coded_size_, *si_format,
        gfx::BufferUsage::VEA_READ_CAMERA_AND_CPU_READ_WRITE,
        base::Microseconds(timestamp));
  }

  if (!frame) {
    DLOG(ERROR) << "Failed to create VideoFrame";
    client_->NotifyError(Error::kInvalidArgumentError);
    return;
  }

  // Make sure the Mojo callback is called on the same thread as where the Mojo
  // call is received (here).
  frame->AddDestructionObserver(
      base::BindPostTaskToCurrentDefault(std::move(callback)));
  accelerator_->Encode(std::move(frame), force_keyframe);
}

void GpuArcVideoEncodeAccelerator::UseBitstreamBuffer(
    mojo::ScopedHandle shmem_fd,
    uint32_t offset,
    uint32_t size,
    UseBitstreamBufferCallback callback) {
  DVLOGF(2) << "serial=" << bitstream_buffer_serial_;
  if (!accelerator_) {
    DLOG(ERROR) << "Accelerator is not initialized.";
    return;
  }

  base::ScopedFD fd = UnwrapFdFromMojoHandle(std::move(shmem_fd));
  if (!fd.is_valid()) {
    client_->NotifyError(Error::kPlatformFailureError);
    return;
  }

  size_t shmem_size;
  if (!media::GetFileSize(fd.get(), &shmem_size)) {
    client_->NotifyError(Error::kInvalidArgumentError);
    return;
  }

  // TODO(rockot): Pass GUIDs through Mojo. https://crbug.com/713763.
  // TODO(rockot): This fd comes from a mojo::ScopedHandle in
  // GpuArcVideoService::BindSharedMemory. That should be passed through,
  // rather than pulling out the fd. https://crbug.com/713763.
  // TODO(rockot): Pass through a real size rather than |0|.
  base::UnguessableToken guid = base::UnguessableToken::Create();
  auto shm_region = base::UnsafeSharedMemoryRegion::Deserialize(
      base::subtle::PlatformSharedMemoryRegion::Take(
          std::move(fd),
          base::subtle::PlatformSharedMemoryRegion::Mode::kUnsafe, shmem_size,
          guid));
  if (!shm_region.IsValid()) {
    client_->NotifyError(Error::kInvalidArgumentError);
    return;
  }
  use_bitstream_cbs_.emplace(bitstream_buffer_serial_, std::move(callback));
  accelerator_->UseOutputBitstreamBuffer(media::BitstreamBuffer(
      bitstream_buffer_serial_, std::move(shm_region), size, offset));

  // Mask against 30 bits to avoid (undefined) wraparound on signed integer.
  bitstream_buffer_serial_ = (bitstream_buffer_serial_ + 1) & 0x3FFFFFFF;
}

void GpuArcVideoEncodeAccelerator::RequestEncodingParametersChange(
    const media::Bitrate& bitrate,
    uint32_t framerate) {
  DVLOGF(2) << bitrate.ToString();

  if (!accelerator_) {
    DLOG(ERROR) << "Accelerator is not initialized.";
    return;
  }

  // Note that dynamic bitrate mode changes are not allowed. Attempting to
  // change the bitrate mode at runtime will result in the |accelerator_|
  // reporting an error through NotifyError.
  accelerator_->RequestEncodingParametersChange(bitrate, framerate,
                                                std::nullopt);
}

void GpuArcVideoEncodeAccelerator::RequestEncodingParametersChangeDeprecated(
    uint32_t bitrate,
    uint32_t framerate) {
  DVLOGF(2) << "bitrate=" << bitrate << ", framerate=" << framerate;
  if (!accelerator_) {
    DLOG(ERROR) << "Accelerator is not initialized.";
    return;
  }
  accelerator_->RequestEncodingParametersChange(
      media::Bitrate::ConstantBitrate(bitrate), framerate, std::nullopt);
}

void GpuArcVideoEncodeAccelerator::Flush(FlushCallback callback) {
  DVLOGF(2);
  if (!accelerator_) {
    DLOG(ERROR) << "Accelerator is not initialized.";
    return;
  }
  accelerator_->Flush(std::move(callback));
}

}  // namespace arc
