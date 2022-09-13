// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_QUADS_LARGEST_DRAW_QUAD_H_
#define COMPONENTS_VIZ_COMMON_QUADS_LARGEST_DRAW_QUAD_H_

#include <stddef.h>

#include "components/viz/common/viz_common_export.h"

namespace viz {

VIZ_COMMON_EXPORT size_t LargestDrawQuadSize();
VIZ_COMMON_EXPORT size_t LargestDrawQuadAlignment();

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_QUADS_LARGEST_DRAW_QUAD_H_
