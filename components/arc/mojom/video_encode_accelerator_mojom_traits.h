// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_MOJOM_VIDEO_ENCODE_ACCELERATOR_MOJOM_TRAITS_H_
#define COMPONENTS_ARC_MOJOM_VIDEO_ENCODE_ACCELERATOR_MOJOM_TRAITS_H_

#include "components/arc/mojom/video_encode_accelerator.mojom.h"
#include "media/video/video_encode_accelerator.h"

namespace mojo {

template <>
struct EnumTraits<arc::mojom::VideoFrameStorageType,
                  media::VideoEncodeAccelerator::Config::StorageType> {
  static arc::mojom::VideoFrameStorageType ToMojom(
      media::VideoEncodeAccelerator::Config::StorageType input);
  static bool FromMojom(
      arc::mojom::VideoFrameStorageType input,
      media::VideoEncodeAccelerator::Config::StorageType* output);
};

template <>
struct EnumTraits<arc::mojom::VideoEncodeAccelerator::Error,
                  media::VideoEncodeAccelerator::Error> {
  static arc::mojom::VideoEncodeAccelerator::Error ToMojom(
      media::VideoEncodeAccelerator::Error input);

  static bool FromMojom(arc::mojom::VideoEncodeAccelerator::Error input,
                        media::VideoEncodeAccelerator::Error* output);
};

template <>
struct StructTraits<arc::mojom::VideoEncodeProfileDataView,
                    media::VideoEncodeAccelerator::SupportedProfile> {
  static media::VideoCodecProfile profile(
      const media::VideoEncodeAccelerator::SupportedProfile& r) {
    return r.profile;
  }
  static const gfx::Size& max_resolution(
      const media::VideoEncodeAccelerator::SupportedProfile& r) {
    return r.max_resolution;
  }
  static uint32_t max_framerate_numerator(
      const media::VideoEncodeAccelerator::SupportedProfile& r) {
    return r.max_framerate_numerator;
  }
  static uint32_t max_framerate_denominator(
      const media::VideoEncodeAccelerator::SupportedProfile& r) {
    return r.max_framerate_denominator;
  }

  static bool Read(arc::mojom::VideoEncodeProfileDataView data,
                   media::VideoEncodeAccelerator::SupportedProfile* out) {
    NOTIMPLEMENTED();
    return false;
  }
};

template <>
struct StructTraits<arc::mojom::VideoEncodeAcceleratorConfigDataView,
                    media::VideoEncodeAccelerator::Config> {
  static media::VideoPixelFormat input_format(
      const media::VideoEncodeAccelerator::Config& input) {
    return input.input_format;
  }

  static const gfx::Size& input_visible_size(
      const media::VideoEncodeAccelerator::Config& input) {
    return input.input_visible_size;
  }

  static media::VideoCodecProfile output_profile(
      const media::VideoEncodeAccelerator::Config& input) {
    return input.output_profile;
  }

  static uint32_t initial_bitrate(
      const media::VideoEncodeAccelerator::Config& input) {
    return input.initial_bitrate;
  }

  static uint32_t initial_framerate(
      const media::VideoEncodeAccelerator::Config& input) {
    return input.initial_framerate.value_or(0);
  }

  static bool has_initial_framerate(
      const media::VideoEncodeAccelerator::Config& input) {
    return input.initial_framerate.has_value();
  }

  static uint32_t gop_length(
      const media::VideoEncodeAccelerator::Config& input) {
    return input.gop_length.value_or(0);
  }

  static bool has_gop_length(
      const media::VideoEncodeAccelerator::Config& input) {
    return input.gop_length.has_value();
  }

  static uint8_t h264_output_level(
      const media::VideoEncodeAccelerator::Config& input) {
    return input.h264_output_level.value_or(0);
  }

  static bool has_h264_output_level(
      const media::VideoEncodeAccelerator::Config& input) {
    return input.h264_output_level.has_value();
  }

  static arc::mojom::VideoFrameStorageType storage_type(
      const media::VideoEncodeAccelerator::Config& input) {
    auto storage_type = input.storage_type.value_or(
        media::VideoEncodeAccelerator::Config::StorageType::kShmem);
    switch (storage_type) {
      case media::VideoEncodeAccelerator::Config::StorageType::kShmem:
        return arc::mojom::VideoFrameStorageType::SHMEM;
      case media::VideoEncodeAccelerator::Config::StorageType::kDmabuf:
        return arc::mojom::VideoFrameStorageType::DMABUF;
    }
  }

  static bool Read(arc::mojom::VideoEncodeAcceleratorConfigDataView input,
                   media::VideoEncodeAccelerator::Config* output);
};

}  // namespace mojo

#endif  // COMPONENTS_ARC_MOJOM_VIDEO_ENCODE_ACCELERATOR_MOJOM_TRAITS_H_
