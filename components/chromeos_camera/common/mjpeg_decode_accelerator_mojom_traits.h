// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CHROMEOS_CAMERA_COMMON_MJPEG_DECODE_ACCELERATOR_MOJOM_TRAITS_H_
#define COMPONENTS_CHROMEOS_CAMERA_COMMON_MJPEG_DECODE_ACCELERATOR_MOJOM_TRAITS_H_

#include "base/numerics/safe_conversions.h"
#include "components/chromeos_camera/common/mjpeg_decode_accelerator.mojom-shared.h"
#include "components/chromeos_camera/mjpeg_decode_accelerator.h"
#include "media/base/bitstream_buffer.h"

namespace mojo {

template <>
struct EnumTraits<chromeos_camera::mojom::DecodeError,
                  chromeos_camera::MjpegDecodeAccelerator::Error> {
  static chromeos_camera::mojom::DecodeError ToMojom(
      chromeos_camera::MjpegDecodeAccelerator::Error error);

  static bool FromMojom(chromeos_camera::mojom::DecodeError input,
                        chromeos_camera::MjpegDecodeAccelerator::Error* out);
};

template <>
struct StructTraits<chromeos_camera::mojom::BitstreamBufferDataView,
                    media::BitstreamBuffer> {
  static int32_t id(const media::BitstreamBuffer& input) { return input.id(); }

  static mojo::ScopedSharedBufferHandle memory_handle(
      media::BitstreamBuffer& input);

  static uint32_t size(const media::BitstreamBuffer& input) {
    return base::checked_cast<uint32_t>(input.size());
  }

  static int64_t offset(const media::BitstreamBuffer& input) {
    return base::checked_cast<int64_t>(input.offset());
  }

  static base::TimeDelta timestamp(const media::BitstreamBuffer& input) {
    return input.presentation_timestamp();
  }

  static const std::string& key_id(const media::BitstreamBuffer& input) {
    return input.key_id();
  }

  static const std::string& iv(const media::BitstreamBuffer& input) {
    return input.iv();
  }

  static const std::vector<media::SubsampleEntry>& subsamples(
      const media::BitstreamBuffer& input) {
    return input.subsamples();
  }

  static bool Read(chromeos_camera::mojom::BitstreamBufferDataView input,
                   media::BitstreamBuffer* output);
};

}  // namespace mojo

#endif  // COMPONENTS_CHROMEOS_CAMERA_COMMON_MJPEG_DECODE_ACCELERATOR_MOJOM_TRAITS_H_
