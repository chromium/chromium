// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_RENDERER_UTILS_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_RENDERER_UTILS_H_

#include "third_party/skia/include/core/SkMatrix.h"
#include "third_party/skia/include/core/SkScalar.h"

namespace viz {

bool IsScaleAndIntegerTranslate(const SkMatrix& matrix);

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_RENDERER_UTILS_H_
