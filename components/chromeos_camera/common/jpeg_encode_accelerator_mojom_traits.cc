// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/chromeos_camera/common/jpeg_encode_accelerator_mojom_traits.h"

#include "base/notreached.h"

namespace mojo {

// static
chromeos_camera::mojom::EncodeStatus
EnumTraits<chromeos_camera::mojom::EncodeStatus,
           chromeos_camera::JpegEncodeAccelerator::Status>::
    ToMojom(chromeos_camera::JpegEncodeAccelerator::Status status) {
  switch (status) {
    case chromeos_camera::JpegEncodeAccelerator::ENCODE_OK:
      return chromeos_camera::mojom::EncodeStatus::ENCODE_OK;
    case chromeos_camera::JpegEncodeAccelerator::HW_JPEG_ENCODE_NOT_SUPPORTED:
      return chromeos_camera::mojom::EncodeStatus::HW_JPEG_ENCODE_NOT_SUPPORTED;
    case chromeos_camera::JpegEncodeAccelerator::THREAD_CREATION_FAILED:
      return chromeos_camera::mojom::EncodeStatus::THREAD_CREATION_FAILED;
    case chromeos_camera::JpegEncodeAccelerator::INVALID_ARGUMENT:
      return chromeos_camera::mojom::EncodeStatus::INVALID_ARGUMENT;
    case chromeos_camera::JpegEncodeAccelerator::INACCESSIBLE_OUTPUT_BUFFER:
      return chromeos_camera::mojom::EncodeStatus::INACCESSIBLE_OUTPUT_BUFFER;
    case chromeos_camera::JpegEncodeAccelerator::PARSE_IMAGE_FAILED:
      return chromeos_camera::mojom::EncodeStatus::PARSE_IMAGE_FAILED;
    case chromeos_camera::JpegEncodeAccelerator::PLATFORM_FAILURE:
      return chromeos_camera::mojom::EncodeStatus::PLATFORM_FAILURE;
  }
  NOTREACHED();
  return chromeos_camera::mojom::EncodeStatus::ENCODE_OK;
}

// static
bool EnumTraits<chromeos_camera::mojom::EncodeStatus,
                chromeos_camera::JpegEncodeAccelerator::Status>::
    FromMojom(chromeos_camera::mojom::EncodeStatus status,
              chromeos_camera::JpegEncodeAccelerator::Status* out) {
  switch (status) {
    case chromeos_camera::mojom::EncodeStatus::ENCODE_OK:
      *out = chromeos_camera::JpegEncodeAccelerator::Status::ENCODE_OK;
      return true;
    case chromeos_camera::mojom::EncodeStatus::HW_JPEG_ENCODE_NOT_SUPPORTED:
      *out = chromeos_camera::JpegEncodeAccelerator::Status::
          HW_JPEG_ENCODE_NOT_SUPPORTED;
      return true;
    case chromeos_camera::mojom::EncodeStatus::THREAD_CREATION_FAILED:
      *out = chromeos_camera::JpegEncodeAccelerator::Status::
          THREAD_CREATION_FAILED;
      return true;
    case chromeos_camera::mojom::EncodeStatus::INVALID_ARGUMENT:
      *out = chromeos_camera::JpegEncodeAccelerator::Status::INVALID_ARGUMENT;
      return true;
    case chromeos_camera::mojom::EncodeStatus::INACCESSIBLE_OUTPUT_BUFFER:
      *out = chromeos_camera::JpegEncodeAccelerator::Status::
          INACCESSIBLE_OUTPUT_BUFFER;
      return true;
    case chromeos_camera::mojom::EncodeStatus::PARSE_IMAGE_FAILED:
      *out = chromeos_camera::JpegEncodeAccelerator::Status::PARSE_IMAGE_FAILED;
      return true;
    case chromeos_camera::mojom::EncodeStatus::PLATFORM_FAILURE:
      *out = chromeos_camera::JpegEncodeAccelerator::Status::PLATFORM_FAILURE;
      return true;
  }
  NOTREACHED();
  return false;
}

}  // namespace mojo
