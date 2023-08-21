// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/skia_output_surface.h"

namespace viz {

SkiaOutputSurface::SkiaOutputSurface() = default;

SkiaOutputSurface::~SkiaOutputSurface() = default;

SkiaOutputSurface* SkiaOutputSurface::AsSkiaOutputSurface() {
  return this;
}

}  // namespace viz
