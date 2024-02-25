// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_RENDER_PASS_ALPHA_TYPE_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_RENDER_PASS_ALPHA_TYPE_H_

#include <type_traits>

#include "base/check.h"
#include "third_party/skia/include/core/SkAlphaType.h"

namespace viz {

// Alpha type for use when drawing and compositing a render pass. Most OS
// compositors do not support linear alpha for compositing, so it is excluded
// from this enum.
enum class RenderPassAlphaType : int {
  kPremul = kPremul_SkAlphaType,
  kOpaque = kOpaque_SkAlphaType,
};

static_assert(std::is_same_v<std::underlying_type_t<RenderPassAlphaType>,
                             std::underlying_type_t<SkAlphaType>>);

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_RENDER_PASS_ALPHA_TYPE_H_
