// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/zcr_color_space.h"

#include <color-space-unstable-v1-server-protocol.h>
#include <wayland-server-core.h>
#include <wayland-server-protocol-core.h>

#include "base/cxx17_backports.h"
#include "components/exo/wayland/server_util.h"
#include "ui/gfx/color_space.h"

namespace exo {
namespace wayland {

namespace {

////////////////////////////////////////////////////////////////////////////////
// color space interface:

const gfx::ColorSpace::PrimaryID kPrimariesMap[] = {
    // ZCR_COLOR_SPACE_V1_PRIMARIES_BT709
    gfx::ColorSpace::PrimaryID::BT709,
    // ZCR_COLOR_SPACE_V1_PRIMARIES_BT470M
    gfx::ColorSpace::PrimaryID::BT470M,
    // ZCR_COLOR_SPACE_V1_PRIMARIES_BT470BG
    gfx::ColorSpace::PrimaryID::BT470BG,
    // ZCR_COLOR_SPACE_V1_PRIMARIES_SMPTE170M
    gfx::ColorSpace::PrimaryID::SMPTE170M,
    // ZCR_COLOR_SPACE_V1_PRIMARIES_SMPTE240M
    gfx::ColorSpace::PrimaryID::SMPTE240M,
    // ZCR_COLOR_SPACE_V1_PRIMARIES_FILM
    gfx::ColorSpace::PrimaryID::FILM,
    // ZCR_COLOR_SPACE_V1_PRIMARIES_BT2020
    gfx::ColorSpace::PrimaryID::BT2020,
    // ZCR_COLOR_SPACE_V1_PRIMARIES_SMPTEST428_1
    gfx::ColorSpace::PrimaryID::SMPTEST428_1,
    // ZCR_COLOR_SPACE_V1_PRIMARIES_SMPTEST431_2
    gfx::ColorSpace::PrimaryID::SMPTEST431_2,
    // ZCR_COLOR_SPACE_V1_PRIMARIES_SMPTEST432_1
    gfx::ColorSpace::PrimaryID::SMPTEST432_1,
    // ZCR_COLOR_SPACE_V1_PRIMARIES_XYZ_D50
    gfx::ColorSpace::PrimaryID::XYZ_D50,
    // ZCR_COLOR_SPACE_V1_PRIMARIES_ADOBE_RGB
    gfx::ColorSpace::PrimaryID::ADOBE_RGB,
};

const gfx::ColorSpace::TransferID kTransferMap[] = {
    // ZCR_COLOR_SPACE_V1_TRANSFER_FUNCTION_BT709
    gfx::ColorSpace::TransferID::BT709,
    // ZCR_COLOR_SPACE_V1_TRANSFER_FUNCTION_GAMMA18
    gfx::ColorSpace::TransferID::GAMMA18,
    // ZCR_COLOR_SPACE_V1_TRANSFER_FUNCTION_GAMMA22
    gfx::ColorSpace::TransferID::GAMMA22,
    // ZCR_COLOR_SPACE_V1_TRANSFER_FUNCTION_GAMMA24
    gfx::ColorSpace::TransferID::GAMMA24,
    // ZCR_COLOR_SPACE_V1_TRANSFER_FUNCTION_GAMMA28
    gfx::ColorSpace::TransferID::GAMMA28,
    // ZCR_COLOR_SPACE_V1_TRANSFER_FUNCTION_SMPTE170M
    gfx::ColorSpace::TransferID::SMPTE170M,
    // ZCR_COLOR_SPACE_V1_TRANSFER_FUNCTION_SMPTE240M
    gfx::ColorSpace::TransferID::SMPTE240M,
    // ZCR_COLOR_SPACE_V1_TRANSFER_FUNCTION_LINEAR
    gfx::ColorSpace::TransferID::LINEAR,
    // ZCR_COLOR_SPACE_V1_TRANSFER_FUNCTION_LOG
    gfx::ColorSpace::TransferID::LOG,
    // ZCR_COLOR_SPACE_V1_TRANSFER_FUNCTION_LOG_SQRT
    gfx::ColorSpace::TransferID::LOG_SQRT,
    // ZCR_COLOR_SPACE_V1_TRANSFER_FUNCTION_IEC61966_2_4
    gfx::ColorSpace::TransferID::IEC61966_2_4,
    // ZCR_COLOR_SPACE_V1_TRANSFER_FUNCTION_BT1361_ECG
    gfx::ColorSpace::TransferID::BT1361_ECG,
    // ZCR_COLOR_SPACE_V1_TRANSFER_FUNCTION_IEC61966_2_1
    gfx::ColorSpace::TransferID::IEC61966_2_1,
    // ZCR_COLOR_SPACE_V1_TRANSFER_FUNCTION_BT2020_10
    gfx::ColorSpace::TransferID::BT2020_10,
    // ZCR_COLOR_SPACE_V1_TRANSFER_FUNCTION_BT2020_12
    gfx::ColorSpace::TransferID::BT2020_12,
    // ZCR_COLOR_SPACE_V1_TRANSFER_FUNCTION_SMPTEST2084
    gfx::ColorSpace::TransferID::SMPTEST2084,
    // ZCR_COLOR_SPACE_V1_TRANSFER_FUNCTION_SMPTEST428_1
    gfx::ColorSpace::TransferID::SMPTEST428_1,
    // ZCR_COLOR_SPACE_V1_TRANSFER_FUNCTION_ARIB_STD_B67
    gfx::ColorSpace::TransferID::ARIB_STD_B67,
    // ZCR_COLOR_SPACE_V1_TRANSFER_FUNCTION_SMPTEST2084_NON_HDR
    gfx::ColorSpace::TransferID::SMPTEST2084,
    // ZCR_COLOR_SPACE_V1_TRANSFER_FUNCTION_IEC61966_2_1_HDR
    gfx::ColorSpace::TransferID::IEC61966_2_1_HDR,
    // ZCR_COLOR_SPACE_V1_TRANSFER_FUNCTION_LINEAR_HDR
    gfx::ColorSpace::TransferID::LINEAR_HDR,
};

const gfx::ColorSpace::MatrixID kMatrixMap[] = {
    // ZCR_COLOR_SPACE_V1_MATRIX_RGB
    gfx::ColorSpace::MatrixID::RGB,
    // ZCR_COLOR_SPACE_V1_MATRIX_BT709
    gfx::ColorSpace::MatrixID::BT709,
    // ZCR_COLOR_SPACE_V1_MATRIX_FCC
    gfx::ColorSpace::MatrixID::FCC,
    // ZCR_COLOR_SPACE_V1_MATRIX_BT470BG
    gfx::ColorSpace::MatrixID::BT470BG,
    // ZCR_COLOR_SPACE_V1_MATRIX_SMPTE170M
    gfx::ColorSpace::MatrixID::SMPTE170M,
    // ZCR_COLOR_SPACE_V1_MATRIX_SMPTE240M
    gfx::ColorSpace::MatrixID::SMPTE240M,
    // ZCR_COLOR_SPACE_V1_MATRIX_YCOCG
    gfx::ColorSpace::MatrixID::YCOCG,
    // ZCR_COLOR_SPACE_V1_MATRIX_BT2020_NCL
    gfx::ColorSpace::MatrixID::BT2020_NCL,
    // ZCR_COLOR_SPACE_V1_MATRIX_BT2020_CL
    gfx::ColorSpace::MatrixID::BT2020_CL,
    // ZCR_COLOR_SPACE_V1_MATRIX_YDZDX
    gfx::ColorSpace::MatrixID::YDZDX,
    // ZCR_COLOR_SPACE_V1_MATRIX_GBR
    gfx::ColorSpace::MatrixID::GBR,
};

const gfx::ColorSpace::RangeID kRangeMap[] = {
    // ZCR_COLOR_SPACE_V1_RANGE_LIMITED
    gfx::ColorSpace::RangeID::LIMITED,
    // ZCR_COLOR_SPACE_V1_RANGE_FULL
    gfx::ColorSpace::RangeID::FULL,
};

static gfx::ColorSpace GetColorSpace(wl_resource* resource,
                                     uint32_t in_primaries,
                                     uint32_t in_transfer_function,
                                     uint32_t in_matrix,
                                     uint32_t in_range) {
  static_assert(
      base::size(kPrimariesMap) == ZCR_COLOR_SPACE_V1_PRIMARIES_ADOBE_RGB + 1,
      "ColorSpace Primaries don't match Wayland primaries in size");
  static_assert(
      base::size(kTransferMap) ==
          ZCR_COLOR_SPACE_V1_TRANSFER_FUNCTION_LINEAR_HDR + 1,
      "ColorSpace transfer functions don't match Wayland transfer functions "
      "in size");
  static_assert(base::size(kMatrixMap) == ZCR_COLOR_SPACE_V1_MATRIX_GBR + 1,
                "ColorSpace matrices don't match Wayland matrices in size");
  static_assert(base::size(kRangeMap) == ZCR_COLOR_SPACE_V1_RANGE_FULL + 1,
                "ColorSpace ranges don't match Wayland ranges in size");

  gfx::ColorSpace::PrimaryID primary = gfx::ColorSpace::PrimaryID::INVALID;
  gfx::ColorSpace::TransferID transfer = gfx::ColorSpace::TransferID::INVALID;
  gfx::ColorSpace::MatrixID matrix = gfx::ColorSpace::MatrixID::INVALID;
  gfx::ColorSpace::RangeID range = gfx::ColorSpace::RangeID::INVALID;

  if (in_primaries < base::size(kPrimariesMap)) {
    primary = kPrimariesMap[in_primaries];
  } else {
    wl_resource_post_error(resource, ZCR_COLOR_SPACE_V1_ERROR_INVALID_PRIMARIES,
                           "Unrecognized primaries %d", in_primaries);
  }
  if (in_transfer_function < base::size(kTransferMap)) {
    transfer = kTransferMap[in_transfer_function];
  } else {
    wl_resource_post_error(
        resource, ZCR_COLOR_SPACE_V1_ERROR_INVALID_TRANSFER_FUNCTION,
        "Unrecognized transfer_function %d", in_transfer_function);
  }
  if (in_matrix < base::size(kMatrixMap)) {
    matrix = kMatrixMap[in_matrix];
  } else {
    wl_resource_post_error(resource, ZCR_COLOR_SPACE_V1_ERROR_INVALID_MATRIX,
                           "Unrecognized matrix %d", in_matrix);
  }
  if (in_range < base::size(kRangeMap)) {
    range = kRangeMap[in_range];
  } else {
    wl_resource_post_error(resource, ZCR_COLOR_SPACE_V1_ERROR_INVALID_RANGE,
                           "Unrecognized range %d", in_range);
  }

  return gfx::ColorSpace(primary, transfer, matrix, range);
}

void color_space_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void color_space_set_color_space(wl_client* client,
                                 wl_resource* resource,
                                 wl_resource* surface_resource,
                                 uint32_t primaries,
                                 uint32_t transfer_function,
                                 uint32_t matrix,
                                 uint32_t range) {
  gfx::ColorSpace cs =
      GetColorSpace(resource, primaries, transfer_function, matrix, range);
  if (!cs.IsValid()) {
    // Error should have been posted already when we tried to resolve the color
    // space.
    return;
  }

  GetUserDataAs<Surface>(surface_resource)->SetColorSpace(cs);
}

const struct zcr_color_space_v1_interface color_space_implementation = {
    color_space_destroy, color_space_set_color_space};

}  // namespace

void bind_color_space(wl_client* client,
                      void* data,
                      uint32_t version,
                      uint32_t id) {
  wl_resource* resource =
      wl_resource_create(client, &zcr_color_space_v1_interface, version, id);

  wl_resource_set_implementation(resource, &color_space_implementation, data,
                                 nullptr);
}

}  // namespace wayland
}  // namespace exo
