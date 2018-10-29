// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/common/video_encode_accelerator_struct_traits.h"

#include "base/optional.h"
#include "components/arc/common/video_accelerator_struct_traits.h"

namespace mojo {

// Make sure values in arc::mojom::VideoEncodeAccelerator::Error and
// media::VideoEncodeAccelerator::Error match.
#define CHECK_ERROR_ENUM(value)                                             \
  static_assert(                                                            \
      static_cast<int>(arc::mojom::VideoEncodeAccelerator::Error::value) == \
          media::VideoEncodeAccelerator::Error::value,                      \
      "enum ##value mismatch")

CHECK_ERROR_ENUM(kIllegalStateError);
CHECK_ERROR_ENUM(kInvalidArgumentError);
CHECK_ERROR_ENUM(kPlatformFailureError);
CHECK_ERROR_ENUM(kErrorMax);

#undef CHECK_ERROR_ENUM

// static
arc::mojom::VideoFrameStorageType
EnumTraits<arc::mojom::VideoFrameStorageType,
           media::VideoEncodeAccelerator::Config::StorageType>::
    ToMojom(media::VideoEncodeAccelerator::Config::StorageType input) {
  NOTIMPLEMENTED();
  return arc::mojom::VideoFrameStorageType::SHMEM;
}

bool EnumTraits<arc::mojom::VideoFrameStorageType,
                media::VideoEncodeAccelerator::Config::StorageType>::
    FromMojom(arc::mojom::VideoFrameStorageType input,
              media::VideoEncodeAccelerator::Config::StorageType* output) {
  switch (input) {
    case arc::mojom::VideoFrameStorageType::SHMEM:
      *output = media::VideoEncodeAccelerator::Config::StorageType::kShmem;
      return true;
    case arc::mojom::VideoFrameStorageType::DMABUF:
      *output = media::VideoEncodeAccelerator::Config::StorageType::kDmabuf;
      return true;
  }
  return false;
}

// static
arc::mojom::VideoEncodeAccelerator::Error
EnumTraits<arc::mojom::VideoEncodeAccelerator::Error,
           media::VideoEncodeAccelerator::Error>::
    ToMojom(media::VideoEncodeAccelerator::Error input) {
  return static_cast<arc::mojom::VideoEncodeAccelerator::Error>(input);
}

// static
bool EnumTraits<arc::mojom::VideoEncodeAccelerator::Error,
                media::VideoEncodeAccelerator::Error>::
    FromMojom(arc::mojom::VideoEncodeAccelerator::Error input,
              media::VideoEncodeAccelerator::Error* output) {
  NOTIMPLEMENTED();
  return false;
}

// Make sure values in arc::mojom::VideoPixelFormat match to the values in
// media::VideoPixelFormat. The former is a subset of the later.
#define CHECK_PIXEL_FORMAT_ENUM(value)                                       \
  static_assert(                                                             \
      static_cast<int>(arc::mojom::VideoPixelFormat::value) == media::value, \
      "enum ##value mismatch")

CHECK_PIXEL_FORMAT_ENUM(PIXEL_FORMAT_I420);
CHECK_PIXEL_FORMAT_ENUM(PIXEL_FORMAT_YV12);
CHECK_PIXEL_FORMAT_ENUM(PIXEL_FORMAT_NV12);
CHECK_PIXEL_FORMAT_ENUM(PIXEL_FORMAT_NV21);
CHECK_PIXEL_FORMAT_ENUM(PIXEL_FORMAT_ARGB);
CHECK_PIXEL_FORMAT_ENUM(PIXEL_FORMAT_ABGR);
CHECK_PIXEL_FORMAT_ENUM(PIXEL_FORMAT_XBGR);

#undef CHECK_PXIEL_FORMAT_ENUM

// static
arc::mojom::VideoPixelFormat
EnumTraits<arc::mojom::VideoPixelFormat, media::VideoPixelFormat>::ToMojom(
    media::VideoPixelFormat input) {
  NOTIMPLEMENTED();
  return arc::mojom::VideoPixelFormat::PIXEL_FORMAT_I420;
}

// static
bool EnumTraits<arc::mojom::VideoPixelFormat, media::VideoPixelFormat>::
    FromMojom(arc::mojom::VideoPixelFormat input,
              media::VideoPixelFormat* output) {
  switch (input) {
    case arc::mojom::VideoPixelFormat::PIXEL_FORMAT_UNKNOWN:
    case arc::mojom::VideoPixelFormat::PIXEL_FORMAT_I420:
    case arc::mojom::VideoPixelFormat::PIXEL_FORMAT_YV12:
    case arc::mojom::VideoPixelFormat::PIXEL_FORMAT_NV12:
    case arc::mojom::VideoPixelFormat::PIXEL_FORMAT_NV21:
    case arc::mojom::VideoPixelFormat::PIXEL_FORMAT_ARGB:
    case arc::mojom::VideoPixelFormat::PIXEL_FORMAT_ABGR:
    case arc::mojom::VideoPixelFormat::PIXEL_FORMAT_XBGR:
      *output = static_cast<media::VideoPixelFormat>(input);
      return true;
  }
  NOTREACHED();
  return false;
}

// static
bool StructTraits<arc::mojom::VideoEncodeAcceleratorConfigDataView,
                  media::VideoEncodeAccelerator::Config>::
    Read(arc::mojom::VideoEncodeAcceleratorConfigDataView input,
         media::VideoEncodeAccelerator::Config* output) {
  media::VideoPixelFormat input_format;
  if (!input.ReadInputFormat(&input_format))
    return false;

  gfx::Size input_visible_size;
  if (!input.ReadInputVisibleSize(&input_visible_size))
    return false;

  media::VideoCodecProfile output_profile;
  if (!input.ReadOutputProfile(&output_profile))
    return false;

  base::Optional<uint32_t> initial_framerate;
  if (input.has_initial_framerate()) {
    initial_framerate = input.initial_framerate();
  }

  base::Optional<uint8_t> h264_output_level;
  if (input.has_h264_output_level()) {
    h264_output_level = input.h264_output_level();
  }

  media::VideoEncodeAccelerator::Config::StorageType storage_type;
  if (!input.ReadStorageType(&storage_type))
    return false;

  *output = media::VideoEncodeAccelerator::Config(
      input_format, input_visible_size, output_profile, input.initial_bitrate(),
      initial_framerate, h264_output_level, storage_type);
  return true;
}

// static
bool StructTraits<arc::mojom::VideoEncodeAcceleratorConfigDeprecatedDataView,
                  media::VideoEncodeAccelerator::Config>::
    Read(arc::mojom::VideoEncodeAcceleratorConfigDeprecatedDataView input,
         media::VideoEncodeAccelerator::Config* output) {
  media::VideoPixelFormat input_format;
  if (!input.ReadInputFormat(&input_format))
    return false;

  gfx::Size input_visible_size;
  if (!input.ReadInputVisibleSize(&input_visible_size))
    return false;

  media::VideoCodecProfile output_profile;
  if (!input.ReadOutputProfile(&output_profile))
    return false;

  base::Optional<uint32_t> initial_framerate;
  if (input.has_initial_framerate()) {
    initial_framerate = input.initial_framerate();
  }

  base::Optional<uint8_t> h264_output_level;
  if (input.has_h264_output_level()) {
    h264_output_level = input.h264_output_level();
  }

  *output = media::VideoEncodeAccelerator::Config(
      input_format, input_visible_size, output_profile, input.initial_bitrate(),
      initial_framerate, h264_output_level);
  return true;
}

}  // namespace mojo
