// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/surfaces/surface_info.h"

#include <ostream>

#include "base/strings/stringprintf.h"

namespace viz {

std::string SurfaceInfo::ToString() const {
  return base::StringPrintf("SurfaceInfo(%s, DeviceScaleFactor(%f), Size(%s))",
                            id_.ToString().c_str(), device_scale_factor_,
                            size_in_pixels_.ToString().c_str());
}

std::ostream& operator<<(std::ostream& out, const SurfaceInfo& surface_info) {
  return out << surface_info.ToString();
}

}  // namespace viz
