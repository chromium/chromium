// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COLOR_COLOR_ID_H_
#define COMPONENTS_COLOR_COLOR_ID_H_

#include "build/build_config.h"
#include "ui/color/color_id.h"

namespace color {

// clang-format off
// Cross-platform IDs should be added here.
#define COMMON_COMPONENTS_COLOR_IDS \

#if defined(USE_AURA)
#define COMPONENTS_COLOR_IDS COMMON_COMPONENTS_COLOR_IDS \
  /* Eyedropper colors. */ \
  E_CPONLY(kColorEyedropperBoundary) \
  E_CPONLY(kColorEyedropperCentralPixelInnerRing) \
  E_CPONLY(kColorEyedropperCentralPixelOuterRing) \
  E_CPONLY(kColorEyedropperGrid) \

#else
#define COMPONENTS_COLOR_IDS COMMON_COMPONENTS_COLOR_IDS
#endif

#include "ui/color/color_id_macros.inc"

enum ComponentsColorIds : ui::ColorId {
  kComponentsColorsStart = ui::kUiColorsEnd,

  COMPONENTS_COLOR_IDS

  kComponentsColorsEnd,
};

// Note that this second include is not redundant. The second inclusion of the
// .inc file serves to undefine the macros the first inclusion defined.
#include "ui/color/color_id_macros.inc"  // NOLINT(build/include)

// clang-format on

}  // namespace color

#endif  // COMPONENTS_COLOR_COLOR_ID_H_
