// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/viz_pixel_test.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "ui/base/ui_base_features.h"

namespace viz {

// static
cc::PixelTest::GraphicsBackend VizPixelTest::RenderTypeToBackend(
    RendererType renderer_type) {
  if (renderer_type == RendererType::kSkiaVk) {
    return GraphicsBackend::kSkiaVulkan;
  } else if (renderer_type == RendererType::kSkiaGraphiteDawn) {
    return GraphicsBackend::kSkiaGraphiteDawn;
  } else if (renderer_type == RendererType::kSkiaGraphiteMetal) {
    return GraphicsBackend::kSkiaGraphiteMetal;
  }

  return GraphicsBackend::kDefault;
}

VizPixelTest::VizPixelTest(RendererType type)
    : PixelTest(RenderTypeToBackend(type)), renderer_type_(type) {}

void VizPixelTest::SetUp() {
  switch (renderer_type_) {
    case RendererType::kSoftware:
      SetUpSoftwareRenderer();
      break;
    case RendererType::kSkiaGL:
    case RendererType::kSkiaVk:
    case RendererType::kSkiaGraphiteDawn:
    case RendererType::kSkiaGraphiteMetal:
      SetUpSkiaRenderer(GetSurfaceOrigin());
      break;
  }
}

gfx::SurfaceOrigin VizPixelTest::GetSurfaceOrigin() const {
  return gfx::SurfaceOrigin::kBottomLeft;
}

VizPixelTestWithParam::VizPixelTestWithParam() : VizPixelTest(GetParam()) {}

}  // namespace viz
