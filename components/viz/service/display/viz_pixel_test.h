// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_VIZ_PIXEL_TEST_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_VIZ_PIXEL_TEST_H_

#include "cc/test/pixel_test.h"
#include "components/viz/test/test_types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace viz {

// Viz pixel test base class. When SetUp() is called the appropriate
// DirectRenderer implementation is initialized based on RendererType
// parameter passed to the constructor.
//
// Use VizPixelTestWithParam as a pareterized test base class to run the same
// test with different renderers/graphics backends. If a test requires multiple
// parameters then VizPixelTest is the appropriate base class. One of the
// parameters would be passed to the VizPixelTest constructor to select the
// RendererType.
class VizPixelTest : public cc::PixelTest {
 public:
  explicit VizPixelTest(RendererType type);

  // cc::PixelTest implementation.
  void SetUp() override;

  RendererType renderer_type() const { return renderer_type_; }

  const char* renderer_str() {
    switch (renderer_type_) {
      case RendererType::kSoftware:
        return "software";
      case RendererType::kSkiaGL:
      case RendererType::kSkiaVk:
        return "skia";
      case RendererType::kSkiaGraphiteDawn:
      case RendererType::kSkiaGraphiteMetal:
        return "graphite";
    }
  }

  bool is_software_renderer() const {
    return renderer_type_ == RendererType::kSoftware;
  }
  bool is_skia_graphite() const {
    return renderer_type_ == RendererType::kSkiaGraphiteDawn ||
           renderer_type_ == RendererType::kSkiaGraphiteMetal;
  }

 protected:
  static GraphicsBackend RenderTypeToBackend(RendererType renderer_type);

  virtual gfx::SurfaceOrigin GetSurfaceOrigin() const;

 private:
  const RendererType renderer_type_;
};

// Parameterized test helper with a single RendererType parameter.
class VizPixelTestWithParam : public VizPixelTest,
                              public testing::WithParamInterface<RendererType> {
 public:
  VizPixelTestWithParam();
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_VIZ_PIXEL_TEST_H_
