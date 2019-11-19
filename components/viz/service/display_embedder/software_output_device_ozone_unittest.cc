// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/software_output_device_ozone.h"

#include <memory>

#include "base/macros.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/test/test_context_factories.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/skia_util.h"
#include "ui/gfx/vsync_provider.h"
#include "ui/gl/gl_implementation.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/platform_window_surface.h"
#include "ui/ozone/public/surface_factory_ozone.h"
#include "ui/ozone/public/surface_ozone_canvas.h"
#include "ui/platform_window/platform_window_base.h"
#include "ui/platform_window/platform_window_delegate.h"
#include "ui/platform_window/platform_window_init_properties.h"

namespace viz {

namespace {

class TestPlatformWindowDelegate : public ui::PlatformWindowDelegate {
 public:
  TestPlatformWindowDelegate() : widget_(gfx::kNullAcceleratedWidget) {}
  ~TestPlatformWindowDelegate() override {}

  gfx::AcceleratedWidget GetAcceleratedWidget() const { return widget_; }

  // ui::PlatformWindowDelegate:
  void OnBoundsChanged(const gfx::Rect& new_bounds) override {}
  void OnDamageRect(const gfx::Rect& damaged_region) override {}
  void DispatchEvent(ui::Event* event) override {}
  void OnCloseRequest() override {}
  void OnClosed() override {}
  void OnWindowStateChanged(ui::PlatformWindowState new_state) override {}
  void OnLostCapture() override {}
  void OnAcceleratedWidgetAvailable(gfx::AcceleratedWidget widget) override {
    widget_ = widget;
  }
  void OnAcceleratedWidgetDestroyed() override {}
  void OnActivationChanged(bool active) override {}
  void OnMouseEnter() override {}

 private:
  gfx::AcceleratedWidget widget_;

  DISALLOW_COPY_AND_ASSIGN(TestPlatformWindowDelegate);
};

}  // namespace

class SoftwareOutputDeviceOzoneTest : public testing::Test {
 public:
  SoftwareOutputDeviceOzoneTest();
  ~SoftwareOutputDeviceOzoneTest() override;

  void SetUp() override;
  void TearDown() override;

 protected:
  std::unique_ptr<SoftwareOutputDeviceOzone> output_device_;
  bool enable_pixel_output_ = false;

 private:
  std::unique_ptr<ui::TestContextFactories> context_factories_;
  std::unique_ptr<ui::Compositor> compositor_;
  TestPlatformWindowDelegate window_delegate_;

  DISALLOW_COPY_AND_ASSIGN(SoftwareOutputDeviceOzoneTest);
};

SoftwareOutputDeviceOzoneTest::SoftwareOutputDeviceOzoneTest() = default;
SoftwareOutputDeviceOzoneTest::~SoftwareOutputDeviceOzoneTest() = default;

void SoftwareOutputDeviceOzoneTest::SetUp() {
  ui::OzonePlatform::InitParams params;
  params.single_process = true;
  ui::OzonePlatform::InitializeForUI(params);
  ui::OzonePlatform::InitializeForGPU(params);

  ui::PlatformWindowInitProperties properties;
  properties.bounds = gfx::Rect(800, 600, 100, 100);
  auto platform_window = ui::OzonePlatform::GetInstance()->CreatePlatformWindow(
      &window_delegate_, std::move(properties));
  platform_window->Show();

  context_factories_ =
      std::make_unique<ui::TestContextFactories>(enable_pixel_output_);

  const gfx::Size size(500, 400);
  compositor_ = std::make_unique<ui::Compositor>(
      FrameSinkId(1, 1), context_factories_->GetContextFactory(), nullptr,
      base::ThreadTaskRunnerHandle::Get(), false /* enable_pixel_canvas */);
  compositor_->SetAcceleratedWidget(window_delegate_.GetAcceleratedWidget());
  compositor_->SetScaleAndSize(1.0f, size, LocalSurfaceIdAllocation());

  ui::SurfaceFactoryOzone* factory =
      ui::OzonePlatform::GetInstance()->GetSurfaceFactoryOzone();
  std::unique_ptr<ui::PlatformWindowSurface> platform_window_surface =
      factory->CreatePlatformWindowSurface(compositor_->widget());
  std::unique_ptr<ui::SurfaceOzoneCanvas> surface_ozone =
      factory->CreateCanvasForWidget(compositor_->widget(), nullptr);
  if (!surface_ozone) {
    LOG(ERROR) << "SurfaceOzoneCanvas not constructible on this platform";
  } else {
    output_device_ = std::make_unique<SoftwareOutputDeviceOzone>(
        std::move(platform_window_surface), std::move(surface_ozone));
  }
  if (output_device_)
    output_device_->Resize(size, 1.f);
}

void SoftwareOutputDeviceOzoneTest::TearDown() {
  output_device_.reset();
  compositor_.reset();
  context_factories_.reset();
}

class SoftwareOutputDeviceOzonePixelTest
    : public SoftwareOutputDeviceOzoneTest {
 protected:
  void SetUp() override;
};

void SoftwareOutputDeviceOzonePixelTest::SetUp() {
  enable_pixel_output_ = true;
  SoftwareOutputDeviceOzoneTest::SetUp();
}

TEST_F(SoftwareOutputDeviceOzoneTest, CheckCorrectResizeBehavior) {
  // Check if software rendering mode is not supported.
  if (!output_device_)
    return;

  gfx::Rect damage(0, 0, 100, 100);
  gfx::Size size(200, 100);
  // Reduce size.
  output_device_->Resize(size, 1.f);

  SkCanvas* canvas = output_device_->BeginPaint(damage);
  gfx::Size canvas_size(canvas->getBaseLayerSize().width(),
                        canvas->getBaseLayerSize().height());
  EXPECT_EQ(size.ToString(), canvas_size.ToString());

  size.SetSize(1000, 500);
  // Increase size.
  output_device_->Resize(size, 1.f);

  canvas = output_device_->BeginPaint(damage);
  canvas_size.SetSize(canvas->getBaseLayerSize().width(),
                      canvas->getBaseLayerSize().height());
  EXPECT_EQ(size.ToString(), canvas_size.ToString());
}

}  // namespace viz
