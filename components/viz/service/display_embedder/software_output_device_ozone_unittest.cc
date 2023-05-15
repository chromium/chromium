// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/software_output_device_ozone.h"

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/test/test_context_factories.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/vsync_provider.h"
#include "ui/gl/gl_implementation.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/platform_window_surface.h"
#include "ui/ozone/public/surface_factory_ozone.h"
#include "ui/ozone/public/surface_ozone_canvas.h"
#include "ui/platform_window/platform_window.h"
#include "ui/platform_window/platform_window_delegate.h"
#include "ui/platform_window/platform_window_init_properties.h"

namespace viz {

namespace {

class TestSurfaceOzoneCanvas : public ui::SurfaceOzoneCanvas {
 public:
  TestSurfaceOzoneCanvas() = default;
  ~TestSurfaceOzoneCanvas() override = default;

  // ui::SurfaceOzoneCanvas override:
  SkCanvas* GetCanvas() override { return surface_->getCanvas(); }
  void ResizeCanvas(const gfx::Size& viewport_size, float scale) override {
    surface_ = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(
        viewport_size.width(), viewport_size.height()));
  }
  std::unique_ptr<gfx::VSyncProvider> CreateVSyncProvider() override {
    return nullptr;
  }

  MOCK_METHOD1(PresentCanvas, void(const gfx::Rect& damage));

 private:
  sk_sp<SkSurface> surface_;
};

}  // namespace

class SoftwareOutputDeviceOzoneTest : public testing::Test {
 public:
  SoftwareOutputDeviceOzoneTest();
  ~SoftwareOutputDeviceOzoneTest() override;
  SoftwareOutputDeviceOzoneTest(const SoftwareOutputDeviceOzoneTest&) = delete;
  SoftwareOutputDeviceOzoneTest& operator=(
      const SoftwareOutputDeviceOzoneTest&) = delete;

  void SetUp() override;
  void TearDown() override;

 protected:
  std::unique_ptr<SoftwareOutputDeviceOzone> output_device_;
  bool enable_pixel_output_ = false;

  raw_ptr<TestSurfaceOzoneCanvas> surface_ozone_ = nullptr;
};

SoftwareOutputDeviceOzoneTest::SoftwareOutputDeviceOzoneTest() = default;
SoftwareOutputDeviceOzoneTest::~SoftwareOutputDeviceOzoneTest() = default;

void SoftwareOutputDeviceOzoneTest::SetUp() {
  std::unique_ptr<TestSurfaceOzoneCanvas> surface_ozone =
      std::make_unique<TestSurfaceOzoneCanvas>();
  surface_ozone_ = surface_ozone.get();
  output_device_ = std::make_unique<SoftwareOutputDeviceOzone>(
      nullptr, std::move(surface_ozone));
}

void SoftwareOutputDeviceOzoneTest::TearDown() {
  surface_ozone_ = nullptr;
  output_device_.reset();
}

TEST_F(SoftwareOutputDeviceOzoneTest, CheckCorrectResizeBehavior) {
  constexpr gfx::Size size(200, 100);
  // Reduce size.
  output_device_->Resize(size, 1.f);

  constexpr gfx::Rect damage1(0, 0, 100, 100);
  SkCanvas* canvas = output_device_->BeginPaint(damage1);
  ASSERT_TRUE(canvas);
  gfx::Size canvas_size(canvas->getBaseLayerSize().width(),
                        canvas->getBaseLayerSize().height());
  EXPECT_EQ(size, canvas_size);
  EXPECT_CALL(*surface_ozone_, PresentCanvas(damage1)).Times(1);
  output_device_->EndPaint();

  constexpr gfx::Size size2(1000, 500);
  // Increase size.
  output_device_->Resize(size2, 1.f);

  constexpr gfx::Rect damage2(0, 0, 50, 60);
  canvas = output_device_->BeginPaint(damage2);
  canvas_size.SetSize(canvas->getBaseLayerSize().width(),
                      canvas->getBaseLayerSize().height());
  EXPECT_EQ(size2, canvas_size);
  EXPECT_CALL(*surface_ozone_, PresentCanvas(damage2)).Times(1);
  output_device_->EndPaint();
}

}  // namespace viz
