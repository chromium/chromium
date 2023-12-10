// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_SURFACES_SURFACE_INFO_H_
#define COMPONENTS_VIZ_COMMON_SURFACES_SURFACE_INFO_H_

#include <string>

#include "components/viz/common/surfaces/surface_id.h"
#include "components/viz/common/viz_common_export.h"
#include "ui/gfx/geometry/size.h"

namespace IPC {
template <class T>
struct ParamTraits;
}  // namespace IPC

namespace viz {

namespace mojom {
class SurfaceInfoDataView;
}  // namespace mojom

// This class contains information about the surface that is being embedded.
class VIZ_COMMON_EXPORT SurfaceInfo {
 public:
  SurfaceInfo() = default;
  SurfaceInfo(const SurfaceId& id,
              float device_scale_factor,
              const gfx::Size& size_in_pixels)
      : id_(id),
        device_scale_factor_(device_scale_factor),
        size_in_pixels_(size_in_pixels) {}

  bool is_valid() const {
    return id_.is_valid() && device_scale_factor_ != 0 &&
           !size_in_pixels_.IsEmpty();
  }

  friend bool operator==(const SurfaceInfo&, const SurfaceInfo&) = default;

  const SurfaceId& id() const { return id_; }
  float device_scale_factor() const { return device_scale_factor_; }
  const gfx::Size& size_in_pixels() const { return size_in_pixels_; }

  std::string ToString() const;

 private:
  friend struct mojo::StructTraits<mojom::SurfaceInfoDataView, SurfaceInfo>;
  friend struct IPC::ParamTraits<SurfaceInfo>;

  SurfaceId id_;
  float device_scale_factor_ = 1.f;
  gfx::Size size_in_pixels_;
};

VIZ_COMMON_EXPORT std::ostream& operator<<(std::ostream& out,
                                           const SurfaceInfo& surface_info);

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_SURFACES_SURFACE_INFO_H_
