// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/chromeos_camera/common/mjpeg_decode_accelerator_mojom_traits.h"

#include "base/check.h"
#include "base/memory/platform_shared_memory_region.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/notreached.h"
#include "base/numerics/checked_math.h"
#include "base/time/time.h"
#include "media/base/ipc/media_param_traits_macros.h"
#include "mojo/public/cpp/base/time_mojom_traits.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace mojo {

// static
chromeos_camera::mojom::DecodeError
EnumTraits<chromeos_camera::mojom::DecodeError,
           chromeos_camera::MjpegDecodeAccelerator::Error>::
    ToMojom(chromeos_camera::MjpegDecodeAccelerator::Error error) {
  switch (error) {
    case chromeos_camera::MjpegDecodeAccelerator::NO_ERRORS:
      return chromeos_camera::mojom::DecodeError::NO_ERRORS;
    case chromeos_camera::MjpegDecodeAccelerator::INVALID_ARGUMENT:
      return chromeos_camera::mojom::DecodeError::INVALID_ARGUMENT;
    case chromeos_camera::MjpegDecodeAccelerator::UNREADABLE_INPUT:
      return chromeos_camera::mojom::DecodeError::UNREADABLE_INPUT;
    case chromeos_camera::MjpegDecodeAccelerator::PARSE_JPEG_FAILED:
      return chromeos_camera::mojom::DecodeError::PARSE_JPEG_FAILED;
    case chromeos_camera::MjpegDecodeAccelerator::UNSUPPORTED_JPEG:
      return chromeos_camera::mojom::DecodeError::UNSUPPORTED_JPEG;
    case chromeos_camera::MjpegDecodeAccelerator::PLATFORM_FAILURE:
      return chromeos_camera::mojom::DecodeError::PLATFORM_FAILURE;
  }
  NOTREACHED_IN_MIGRATION();
  return chromeos_camera::mojom::DecodeError::NO_ERRORS;
}

// static
bool EnumTraits<chromeos_camera::mojom::DecodeError,
                chromeos_camera::MjpegDecodeAccelerator::Error>::
    FromMojom(chromeos_camera::mojom::DecodeError error,
              chromeos_camera::MjpegDecodeAccelerator::Error* out) {
  switch (error) {
    case chromeos_camera::mojom::DecodeError::NO_ERRORS:
      *out = chromeos_camera::MjpegDecodeAccelerator::Error::NO_ERRORS;
      return true;
    case chromeos_camera::mojom::DecodeError::INVALID_ARGUMENT:
      *out = chromeos_camera::MjpegDecodeAccelerator::Error::INVALID_ARGUMENT;
      return true;
    case chromeos_camera::mojom::DecodeError::UNREADABLE_INPUT:
      *out = chromeos_camera::MjpegDecodeAccelerator::Error::UNREADABLE_INPUT;
      return true;
    case chromeos_camera::mojom::DecodeError::PARSE_JPEG_FAILED:
      *out = chromeos_camera::MjpegDecodeAccelerator::Error::PARSE_JPEG_FAILED;
      return true;
    case chromeos_camera::mojom::DecodeError::UNSUPPORTED_JPEG:
      *out = chromeos_camera::MjpegDecodeAccelerator::Error::UNSUPPORTED_JPEG;
      return true;
    case chromeos_camera::mojom::DecodeError::PLATFORM_FAILURE:
      *out = chromeos_camera::MjpegDecodeAccelerator::Error::PLATFORM_FAILURE;
      return true;
  }
  NOTREACHED_IN_MIGRATION();
  return false;
}

// static
mojo::ScopedSharedBufferHandle StructTraits<
    chromeos_camera::mojom::BitstreamBufferDataView,
    media::BitstreamBuffer>::memory_handle(media::BitstreamBuffer& input) {
  base::subtle::PlatformSharedMemoryRegion input_region =
      base::UnsafeSharedMemoryRegion::TakeHandleForSerialization(
          input.TakeRegion());
  DCHECK(input_region.IsValid()) << "Bad BitstreamBuffer handle";

  // TODO(crbug.com/40553989): Split BitstreamBuffers into ReadOnly and
  // Unsafe versions corresponding to usage, eg video encode accelerators will
  // use writable mappings but audio uses are readonly (see
  // android_video_encode_accelerator.cc and
  // vt_video_encode_accelerator_mac.cc).
  return mojo::WrapPlatformSharedMemoryRegion(std::move(input_region));
}

// static
bool StructTraits<chromeos_camera::mojom::BitstreamBufferDataView,
                  media::BitstreamBuffer>::
    Read(chromeos_camera::mojom::BitstreamBufferDataView input,
         media::BitstreamBuffer* output) {
  base::TimeDelta timestamp;
  if (!input.ReadTimestamp(&timestamp))
    return false;

  std::string key_id;
  if (!input.ReadKeyId(&key_id))
    return false;

  std::string iv;
  if (!input.ReadIv(&iv))
    return false;

  std::vector<media::SubsampleEntry> subsamples;
  if (!input.ReadSubsamples(&subsamples))
    return false;

  mojo::ScopedSharedBufferHandle handle = input.TakeMemoryHandle();
  if (!handle.is_valid())
    return false;

  auto region = base::UnsafeSharedMemoryRegion::Deserialize(
      mojo::UnwrapPlatformSharedMemoryRegion(std::move(handle)));
  if (!region.IsValid())
    return false;

  auto offset = base::MakeCheckedNum(input.offset()).Cast<uint64_t>();
  if (!offset.IsValid())
    return false;

  media::BitstreamBuffer bitstream_buffer(input.id(), std::move(region),
                                          input.size(), offset.ValueOrDie(),
                                          timestamp);
  if (key_id.size()) {
    // Note that BitstreamBuffer currently ignores how each buffer is
    // encrypted and uses the settings from the Audio/VideoDecoderConfig.
    bitstream_buffer.SetDecryptionSettings(key_id, iv, subsamples);
  }
  *output = std::move(bitstream_buffer);

  return true;
}

}  // namespace mojo
