// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/surface.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/strings/stringprintf.h"
#include "components/exo/buffer.h"
#include "components/exo/shell_surface.h"
#include "components/exo/sub_surface.h"
#include "components/exo/test/exo_test_base.h"
#include "components/exo/test/exo_test_helper.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/service/surfaces/surface.h"
#include "components/viz/service/surfaces/surface_manager.h"
#include "components/viz/test/begin_frame_args_test.h"
#include "components/viz/test/fake_external_begin_frame_source.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/display/display.h"
#include "ui/display/display_switches.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gfx/gpu_fence_handle.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/wm/core/window_util.h"

namespace exo {
namespace {

std::unique_ptr<std::vector<gfx::Rect>> GetHitTestShapeRects(Surface* surface) {
  if (surface->hit_test_region().IsEmpty())
    return nullptr;

  auto rects = std::make_unique<std::vector<gfx::Rect>>();
  for (gfx::Rect rect : surface->hit_test_region())
    rects->push_back(rect);
  return rects;
}

class SurfaceObserverForTest : public SurfaceObserver {
 public:
  SurfaceObserverForTest() = default;

  void OnSurfaceDestroying(Surface* surface) override {}

  void OnWindowOcclusionChanged(Surface* surface) override {
    num_occlusion_changes_++;
  }

  int num_occlusion_changes() const { return num_occlusion_changes_; }

 private:
  int num_occlusion_changes_ = 0;

  DISALLOW_COPY_AND_ASSIGN(SurfaceObserverForTest);
};

class SurfaceTest : public test::ExoTestBase,
                    public ::testing::WithParamInterface<float> {
 public:
  SurfaceTest() = default;
  ~SurfaceTest() override = default;
  void SetUp() override {
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    // Set the device scale factor.
    command_line->AppendSwitchASCII(
        switches::kForceDeviceScaleFactor,
        base::StringPrintf("%f", device_scale_factor()));
    test::ExoTestBase::SetUp();
  }

  void TearDown() override {
    test::ExoTestBase::TearDown();
    display::Display::ResetForceDeviceScaleFactorForTesting();
  }

  float device_scale_factor() const { return GetParam(); }

  gfx::Rect ToPixel(const gfx::Rect rect) {
    return gfx::ToEnclosingRect(
        gfx::ConvertRectToPixels(rect, device_scale_factor()));
  }

  gfx::Rect ToTargetSpaceDamage(const gfx::Rect damage_rect) {
    // Map a frame's damage back to the coordinate space of its buffer.
    return gfx::ScaleToEnclosingRect(damage_rect, 1 / device_scale_factor());
  }

  const viz::CompositorFrame& GetFrameFromSurface(ShellSurface* shell_surface) {
    viz::SurfaceId surface_id = shell_surface->host_window()->GetSurfaceId();
    const viz::CompositorFrame& frame =
        GetSurfaceManager()->GetSurfaceForId(surface_id)->GetActiveFrame();
    return frame;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SurfaceTest);
};

void ReleaseBuffer(int* release_buffer_call_count) {
  (*release_buffer_call_count)++;
}

// Instantiate the Boolean which is used to toggle mouse and touch events in
// the parameterized tests.
INSTANTIATE_TEST_SUITE_P(All, SurfaceTest, testing::Values(1.0f, 1.25f, 2.0f));

TEST_P(SurfaceTest, Attach) {
  gfx::Size buffer_size(256, 256);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));

  // Set the release callback that will be run when buffer is no longer in use.
  int release_buffer_call_count = 0;
  buffer->set_release_callback(base::BindRepeating(
      &ReleaseBuffer, base::Unretained(&release_buffer_call_count)));

  std::unique_ptr<Surface> surface(new Surface);

  // Attach the buffer to surface1.
  surface->Attach(buffer.get());
  EXPECT_TRUE(surface->HasPendingAttachedBuffer());
  surface->Commit();

  // Commit without calling Attach() should have no effect.
  surface->Commit();
  EXPECT_EQ(0, release_buffer_call_count);

  // Attach a null buffer to surface, this should release the previously
  // attached buffer.
  surface->Attach(nullptr);
  EXPECT_FALSE(surface->HasPendingAttachedBuffer());
  surface->Commit();
  // LayerTreeFrameSinkHolder::ReclaimResources() gets called via
  // CompositorFrameSinkClient interface. We need to wait here for the mojo
  // call to finish so that the release callback finishes running before
  // the assertion below.
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1, release_buffer_call_count);
}

TEST_P(SurfaceTest, Damage) {
  gfx::Size buffer_size(256, 256);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> surface(new Surface);
  auto shell_surface = std::make_unique<ShellSurface>(surface.get());

  // Attach the buffer to the surface. This will update the pending bounds of
  // the surface to the buffer size.
  surface->Attach(buffer.get());

  // Mark areas inside the bounds of the surface as damaged. This should result
  // in pending damage.
  surface->Damage(gfx::Rect(0, 0, 10, 10));
  surface->Damage(gfx::Rect(10, 10, 10, 10));
  EXPECT_TRUE(surface->HasPendingDamageForTesting(gfx::Rect(0, 0, 10, 10)));
  EXPECT_TRUE(surface->HasPendingDamageForTesting(gfx::Rect(10, 10, 10, 10)));
  EXPECT_FALSE(surface->HasPendingDamageForTesting(gfx::Rect(5, 5, 10, 10)));

  // Check that damage larger than contents is handled correctly at commit.
  surface->Damage(gfx::Rect(gfx::ScaleToCeiledSize(buffer_size, 2.0f)));
  surface->Commit();
  base::RunLoop().RunUntilIdle();

  {
    const viz::CompositorFrame& frame =
        GetFrameFromSurface(shell_surface.get());
    EXPECT_EQ(ToPixel(gfx::Rect(buffer_size)),
              frame.render_pass_list.back()->damage_rect);
  }

  gfx::RectF buffer_damage(32, 64, 16, 32);
  gfx::Rect surface_damage = gfx::ToNearestRect(buffer_damage);

  // Check that damage is correct for a non-square rectangle not at the origin.
  surface->Damage(surface_damage);
  surface->Commit();
  base::RunLoop().RunUntilIdle();

  // Adjust damage for DSF filtering and verify it below.
  if (device_scale_factor() > 1.f)
    buffer_damage.Inset(-1.f, -1.f);

  {
    const viz::CompositorFrame& frame =
        GetFrameFromSurface(shell_surface.get());
    EXPECT_TRUE(ToTargetSpaceDamage(frame.render_pass_list.back()->damage_rect)
                    .Contains(gfx::ToNearestRect(buffer_damage)));
  }
}

TEST_P(SurfaceTest, SubsurfaceDamageAggregation) {
  gfx::Size buffer_size(256, 512);
  auto buffer = std::make_unique<Buffer>(
      exo_test_helper()->CreateGpuMemoryBuffer(buffer_size));
  auto surface = std::make_unique<Surface>();
  auto shell_surface = std::make_unique<ShellSurface>(surface.get());
  surface->Attach(buffer.get());

  gfx::Size child_buffer_size(64, 128);
  auto child_buffer = std::make_unique<Buffer>(
      exo_test_helper()->CreateGpuMemoryBuffer(child_buffer_size));
  auto child_surface = std::make_unique<Surface>();
  auto sub_surface =
      std::make_unique<SubSurface>(child_surface.get(), surface.get());
  child_surface->Attach(child_buffer.get());
  child_surface->Commit();
  surface->Commit();
  base::RunLoop().RunUntilIdle();

  {
    // Initial frame has full damage.
    const viz::CompositorFrame& frame =
        GetFrameFromSurface(shell_surface.get());
    const gfx::Rect scaled_damage = gfx::ToNearestRect(gfx::ScaleRect(
        gfx::RectF(gfx::Rect(buffer_size)), device_scale_factor()));
    EXPECT_EQ(scaled_damage, frame.render_pass_list.back()->damage_rect);
  }

  const gfx::RectF surface_damage(16, 16);
  const gfx::RectF subsurface_damage(32, 32, 16, 16);
  int margin = ceil(device_scale_factor());

  child_surface->Damage(gfx::ToNearestRect(subsurface_damage));
  child_surface->Commit();
  surface->Commit();
  base::RunLoop().RunUntilIdle();

  {
    // Subsurface damage should be propagated.
    const viz::CompositorFrame& frame =
        GetFrameFromSurface(shell_surface.get());
    const gfx::Rect scaled_damage = gfx::ToNearestRect(
        gfx::ScaleRect(subsurface_damage, device_scale_factor()));
    EXPECT_TRUE(scaled_damage.ApproximatelyEqual(
        frame.render_pass_list.back()->damage_rect, margin));
  }

  surface->Damage(gfx::ToNearestRect(surface_damage));
  surface->Commit();
  base::RunLoop().RunUntilIdle();

  {
    // When commit is called on the root with no call on the child, the damage
    // from the previous frame shouldn't persist.
    const viz::CompositorFrame& frame =
        GetFrameFromSurface(shell_surface.get());
    const gfx::Rect scaled_damage = gfx::ToNearestRect(
        gfx::ScaleRect(surface_damage, device_scale_factor()));
    EXPECT_TRUE(scaled_damage.ApproximatelyEqual(
        frame.render_pass_list.back()->damage_rect, margin));
  }
}

void SetFrameTime(base::TimeTicks* result, base::TimeTicks frame_time) {
  *result = frame_time;
}

TEST_P(SurfaceTest, RequestFrameCallback) {
  // Must be before surface so it outlives it, for surface's destructor calls
  // SetFrameTime() referencing this.
  base::TimeTicks frame_time;

  std::unique_ptr<Surface> surface(new Surface);

  surface->RequestFrameCallback(
      base::BindRepeating(&SetFrameTime, base::Unretained(&frame_time)));
  surface->Commit();

  // Callback should not run synchronously.
  EXPECT_TRUE(frame_time.is_null());
}

// Disabled due to flakiness: crbug.com/856145
#if defined(LEAK_SANITIZER)
#define MAYBE_SetOpaqueRegion DISABLED_SetOpaqueRegion
#else
#define MAYBE_SetOpaqueRegion SetOpaqueRegion
#endif
TEST_P(SurfaceTest, MAYBE_SetOpaqueRegion) {
  gfx::Size buffer_size(1, 1);
  auto buffer = std::make_unique<Buffer>(
      exo_test_helper()->CreateGpuMemoryBuffer(buffer_size));
  auto surface = std::make_unique<Surface>();
  auto shell_surface = std::make_unique<ShellSurface>(surface.get());

  // Attaching a buffer with alpha channel.
  surface->Attach(buffer.get());

  // Setting an opaque region that contains the buffer size doesn't require
  // draw with blending.
  surface->SetOpaqueRegion(gfx::Rect(256, 256));
  surface->Commit();
  base::RunLoop().RunUntilIdle();

  {
    const viz::CompositorFrame& frame =
        GetFrameFromSurface(shell_surface.get());
    ASSERT_EQ(1u, frame.render_pass_list.size());
    ASSERT_EQ(1u, frame.render_pass_list.back()->quad_list.size());
    auto* texture_draw_quad = viz::TextureDrawQuad::MaterialCast(
        frame.render_pass_list.back()->quad_list.back());

    EXPECT_FALSE(texture_draw_quad->ShouldDrawWithBlending());
    EXPECT_EQ(SK_ColorBLACK, texture_draw_quad->background_color);
    EXPECT_EQ(gfx::Rect(buffer_size),
              ToTargetSpaceDamage(frame.render_pass_list.back()->damage_rect));
  }

  // Setting an empty opaque region requires draw with blending.
  surface->SetOpaqueRegion(gfx::Rect());
  surface->Commit();
  base::RunLoop().RunUntilIdle();

  {
    const viz::CompositorFrame& frame =
        GetFrameFromSurface(shell_surface.get());
    ASSERT_EQ(1u, frame.render_pass_list.size());
    ASSERT_EQ(1u, frame.render_pass_list.back()->quad_list.size());
    auto* texture_draw_quad = viz::TextureDrawQuad::MaterialCast(
        frame.render_pass_list.back()->quad_list.back());
    EXPECT_TRUE(texture_draw_quad->ShouldDrawWithBlending());
    EXPECT_EQ(SK_ColorTRANSPARENT, texture_draw_quad->background_color);
    EXPECT_EQ(gfx::Rect(buffer_size),
              ToTargetSpaceDamage(frame.render_pass_list.back()->damage_rect));
  }

  std::unique_ptr<Buffer> buffer_without_alpha(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(
          buffer_size, gfx::BufferFormat::RGBX_8888)));

  // Attaching a buffer without an alpha channel doesn't require draw with
  // blending.
  surface->Attach(buffer_without_alpha.get());
  surface->Commit();
  base::RunLoop().RunUntilIdle();

  {
    const viz::CompositorFrame& frame =
        GetFrameFromSurface(shell_surface.get());
    ASSERT_EQ(1u, frame.render_pass_list.size());
    ASSERT_EQ(1u, frame.render_pass_list.back()->quad_list.size());
    EXPECT_FALSE(frame.render_pass_list.back()
                     ->quad_list.back()
                     ->ShouldDrawWithBlending());
    EXPECT_EQ(ToPixel(gfx::Rect(0, 0, 0, 0)),
              frame.render_pass_list.back()->damage_rect);
  }
}

TEST_P(SurfaceTest, SetInputRegion) {
  // Create a shell surface which size is 512x512.
  auto surface = std::make_unique<Surface>();
  auto shell_surface = std::make_unique<ShellSurface>(surface.get());
  gfx::Size buffer_size(512, 512);
  auto buffer = std::make_unique<Buffer>(
      exo_test_helper()->CreateGpuMemoryBuffer(buffer_size));
  surface->Attach(buffer.get());
  surface->Commit();

  {
    // Default input region should match surface bounds.
    auto rects = GetHitTestShapeRects(surface.get());
    ASSERT_TRUE(rects);
    ASSERT_EQ(1u, rects->size());
    ASSERT_EQ(gfx::Rect(512, 512), (*rects)[0]);
  }

  {
    // Setting a non-empty input region should succeed.
    surface->SetInputRegion(gfx::Rect(256, 256));
    surface->Commit();

    auto rects = GetHitTestShapeRects(surface.get());
    ASSERT_TRUE(rects);
    ASSERT_EQ(1u, rects->size());
    ASSERT_EQ(gfx::Rect(256, 256), (*rects)[0]);
  }

  {
    // Setting an empty input region should succeed.
    surface->SetInputRegion(gfx::Rect());
    surface->Commit();

    EXPECT_FALSE(GetHitTestShapeRects(surface.get()));
  }

  {
    cc::Region region = gfx::Rect(512, 512);
    region.Subtract(gfx::Rect(0, 64, 64, 64));
    region.Subtract(gfx::Rect(88, 88, 12, 55));
    region.Subtract(gfx::Rect(100, 0, 33, 66));

    // Setting a non-rectangle input region should succeed.
    surface->SetInputRegion(region);
    surface->Commit();

    auto rects = GetHitTestShapeRects(surface.get());
    ASSERT_TRUE(rects);
    ASSERT_EQ(10u, rects->size());
    cc::Region result;
    for (const auto& r : *rects)
      result.Union(r);
    ASSERT_EQ(result, region);
  }

  {
    // Input region should be clipped to surface bounds.
    surface->SetInputRegion(gfx::Rect(-50, -50, 1000, 100));
    surface->Commit();

    auto rects = GetHitTestShapeRects(surface.get());
    ASSERT_TRUE(rects);
    ASSERT_EQ(1u, rects->size());
    ASSERT_EQ(gfx::Rect(512, 50), (*rects)[0]);
  }

  {
    // Hit test region should accumulate input regions of sub-surfaces.
    gfx::Rect input_rect(50, 50, 100, 100);
    surface->SetInputRegion(input_rect);

    gfx::Rect child_input_rect(-50, -50, 1000, 100);
    auto child_buffer = std::make_unique<Buffer>(
        exo_test_helper()->CreateGpuMemoryBuffer(child_input_rect.size()));
    auto child_surface = std::make_unique<Surface>();
    auto sub_surface =
        std::make_unique<SubSurface>(child_surface.get(), surface.get());
    sub_surface->SetPosition(child_input_rect.origin());
    child_surface->Attach(child_buffer.get());
    child_surface->Commit();
    surface->Commit();

    auto rects = GetHitTestShapeRects(surface.get());
    ASSERT_TRUE(rects);
    ASSERT_EQ(2u, rects->size());
    cc::Region result = cc::UnionRegions((*rects)[0], (*rects)[1]);
    ASSERT_EQ(cc::UnionRegions(input_rect, child_input_rect), result);
  }
}

TEST_P(SurfaceTest, SetBufferScale) {
  gfx::Size buffer_size(512, 512);
  auto buffer = std::make_unique<Buffer>(
      exo_test_helper()->CreateGpuMemoryBuffer(buffer_size));
  auto surface = std::make_unique<Surface>();
  auto shell_surface = std::make_unique<ShellSurface>(surface.get());

  // This will update the bounds of the surface and take the buffer scale into
  // account.
  const float kBufferScale = 2.0f;
  surface->Attach(buffer.get());
  surface->SetBufferScale(kBufferScale);
  surface->Commit();
  EXPECT_EQ(
      gfx::ScaleToFlooredSize(buffer_size, 1.0f / kBufferScale).ToString(),
      surface->window()->bounds().size().ToString());
  EXPECT_EQ(
      gfx::ScaleToFlooredSize(buffer_size, 1.0f / kBufferScale).ToString(),
      surface->content_size().ToString());

  base::RunLoop().RunUntilIdle();

  const viz::CompositorFrame& frame = GetFrameFromSurface(shell_surface.get());
  ASSERT_EQ(1u, frame.render_pass_list.size());
  EXPECT_EQ(ToPixel(gfx::Rect(0, 0, 256, 256)),
            frame.render_pass_list.back()->damage_rect);
}

// Disabled due to flakiness: crbug.com/856145
#if defined(LEAK_SANITIZER)
#define MAYBE_SetBufferTransform DISABLED_SetBufferTransform
#else
#define MAYBE_SetBufferTransform SetBufferTransform
#endif
TEST_P(SurfaceTest, MAYBE_SetBufferTransform) {
  gfx::Size buffer_size(256, 512);
  auto buffer = std::make_unique<Buffer>(
      exo_test_helper()->CreateGpuMemoryBuffer(buffer_size));
  auto surface = std::make_unique<Surface>();
  auto shell_surface = std::make_unique<ShellSurface>(surface.get());

  // This will update the bounds of the surface and take the buffer transform
  // into account.
  surface->Attach(buffer.get());
  surface->SetBufferTransform(Transform::ROTATE_90);
  surface->Commit();
  EXPECT_EQ(gfx::Size(buffer_size.height(), buffer_size.width()),
            surface->window()->bounds().size());
  EXPECT_EQ(gfx::Size(buffer_size.height(), buffer_size.width()),
            surface->content_size());

  base::RunLoop().RunUntilIdle();

  {
    const viz::CompositorFrame& frame =
        GetFrameFromSurface(shell_surface.get());
    ASSERT_EQ(1u, frame.render_pass_list.size());
    EXPECT_EQ(
        ToPixel(gfx::Rect(0, 0, buffer_size.height(), buffer_size.width())),
        frame.render_pass_list.back()->damage_rect);
    const auto& quad_list = frame.render_pass_list[0]->quad_list;
    ASSERT_EQ(1u, quad_list.size());
    EXPECT_EQ(
        ToPixel(gfx::Rect(0, 0, 512, 256)),
        cc::MathUtil::MapEnclosingClippedRect(
            quad_list.front()->shared_quad_state->quad_to_target_transform,
            quad_list.front()->rect));
  }

  gfx::Size child_buffer_size(64, 128);
  auto child_buffer = std::make_unique<Buffer>(
      exo_test_helper()->CreateGpuMemoryBuffer(child_buffer_size));
  auto child_surface = std::make_unique<Surface>();
  auto sub_surface =
      std::make_unique<SubSurface>(child_surface.get(), surface.get());

  // Set position to 20, 10.
  gfx::Point child_position(20, 10);
  sub_surface->SetPosition(child_position);

  child_surface->Attach(child_buffer.get());
  child_surface->SetBufferTransform(Transform::ROTATE_180);
  const int kChildBufferScale = 2;
  child_surface->SetBufferScale(kChildBufferScale);
  child_surface->Commit();
  surface->Commit();
  EXPECT_EQ(
      gfx::ScaleToRoundedSize(child_buffer_size, 1.0f / kChildBufferScale),
      child_surface->window()->bounds().size());
  EXPECT_EQ(
      gfx::ScaleToRoundedSize(child_buffer_size, 1.0f / kChildBufferScale),
      child_surface->content_size());

  base::RunLoop().RunUntilIdle();

  {
    const viz::CompositorFrame& frame =
        GetFrameFromSurface(shell_surface.get());
    ASSERT_EQ(1u, frame.render_pass_list.size());
    const auto& quad_list = frame.render_pass_list[0]->quad_list;
    ASSERT_EQ(2u, quad_list.size());
    EXPECT_EQ(
        ToPixel(gfx::Rect(child_position,
                          gfx::ScaleToRoundedSize(child_buffer_size,
                                                  1.0f / kChildBufferScale))),
        cc::MathUtil::MapEnclosingClippedRect(
            quad_list.front()->shared_quad_state->quad_to_target_transform,
            quad_list.front()->rect));
  }
}

TEST_P(SurfaceTest, MirrorLayers) {
  gfx::Size buffer_size(512, 512);
  auto buffer = std::make_unique<Buffer>(
      exo_test_helper()->CreateGpuMemoryBuffer(buffer_size));
  auto surface = std::make_unique<Surface>();
  auto shell_surface = std::make_unique<ShellSurface>(surface.get());

  surface->Attach(buffer.get());
  surface->Commit();

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(buffer_size, surface->window()->bounds().size());
  EXPECT_EQ(buffer_size, surface->window()->layer()->bounds().size());
  std::unique_ptr<ui::LayerTreeOwner> old_layer_owner =
      ::wm::MirrorLayers(shell_surface->host_window(), false /* sync_bounds */);
  EXPECT_EQ(buffer_size, surface->window()->bounds().size());
  EXPECT_EQ(buffer_size, surface->window()->layer()->bounds().size());
  EXPECT_EQ(buffer_size, old_layer_owner->root()->bounds().size());
  EXPECT_TRUE(shell_surface->host_window()->layer()->has_external_content());
  EXPECT_TRUE(old_layer_owner->root()->has_external_content());
}

TEST_P(SurfaceTest, SetViewport) {
  gfx::Size buffer_size(1, 1);
  auto buffer = std::make_unique<Buffer>(
      exo_test_helper()->CreateGpuMemoryBuffer(buffer_size));
  auto surface = std::make_unique<Surface>();
  auto shell_surface = std::make_unique<ShellSurface>(surface.get());

  // This will update the bounds of the surface and take the viewport into
  // account.
  surface->Attach(buffer.get());
  gfx::Size viewport(256, 256);
  surface->SetViewport(viewport);
  surface->Commit();
  EXPECT_EQ(viewport.ToString(), surface->content_size().ToString());

  // This will update the bounds of the surface and take the viewport2 into
  // account.
  gfx::Size viewport2(512, 512);
  surface->SetViewport(viewport2);
  surface->Commit();
  EXPECT_EQ(viewport2.ToString(),
            surface->window()->bounds().size().ToString());
  EXPECT_EQ(viewport2.ToString(), surface->content_size().ToString());

  base::RunLoop().RunUntilIdle();

  const viz::CompositorFrame& frame = GetFrameFromSurface(shell_surface.get());
  ASSERT_EQ(1u, frame.render_pass_list.size());
  EXPECT_EQ(ToPixel(gfx::Rect(0, 0, 512, 512)),
            frame.render_pass_list.back()->damage_rect);
}

TEST_P(SurfaceTest, SetCrop) {
  gfx::Size buffer_size(16, 16);
  auto buffer = std::make_unique<Buffer>(
      exo_test_helper()->CreateGpuMemoryBuffer(buffer_size));
  auto surface = std::make_unique<Surface>();
  auto shell_surface = std::make_unique<ShellSurface>(surface.get());

  surface->Attach(buffer.get());
  gfx::Size crop_size(12, 12);
  surface->SetCrop(gfx::RectF(gfx::PointF(2.0, 2.0), gfx::SizeF(crop_size)));
  surface->Commit();
  EXPECT_EQ(crop_size.ToString(),
            surface->window()->bounds().size().ToString());
  EXPECT_EQ(crop_size.ToString(), surface->content_size().ToString());

  base::RunLoop().RunUntilIdle();

  const viz::CompositorFrame& frame = GetFrameFromSurface(shell_surface.get());
  ASSERT_EQ(1u, frame.render_pass_list.size());
  EXPECT_EQ(ToPixel(gfx::Rect(0, 0, 12, 12)),
            frame.render_pass_list.back()->damage_rect);
}

// Disabled due to flakiness: crbug.com/856145
#if defined(LEAK_SANITIZER)
#define MAYBE_SetCropAndBufferTransform DISABLED_SetCropAndBufferTransform
#else
#define MAYBE_SetCropAndBufferTransform SetCropAndBufferTransform
#endif
TEST_P(SurfaceTest, MAYBE_SetCropAndBufferTransform) {
  gfx::Size buffer_size(128, 64);
  auto buffer = std::make_unique<Buffer>(
      exo_test_helper()->CreateGpuMemoryBuffer(buffer_size));
  auto surface = std::make_unique<Surface>();
  auto shell_surface = std::make_unique<ShellSurface>(surface.get());

  const gfx::RectF crop_0(
      gfx::SkRectToRectF(SkRect::MakeLTRB(0.03125f, 0.1875f, 0.4375f, 0.25f)));
  const gfx::RectF crop_90(
      gfx::SkRectToRectF(SkRect::MakeLTRB(0.875f, 0.0625f, 0.90625f, 0.875f)));
  const gfx::RectF crop_180(
      gfx::SkRectToRectF(SkRect::MakeLTRB(0.5625f, 0.75f, 0.96875f, 0.8125f)));
  const gfx::RectF crop_270(
      gfx::SkRectToRectF(SkRect::MakeLTRB(0.09375f, 0.125f, 0.125f, 0.9375f)));
  const gfx::Rect target_with_no_viewport(ToPixel(gfx::Rect(gfx::Size(52, 4))));
  const gfx::Rect target_with_viewport(ToPixel(gfx::Rect(gfx::Size(128, 64))));

  surface->Attach(buffer.get());
  gfx::Size crop_size(52, 4);
  surface->SetCrop(gfx::RectF(gfx::PointF(4, 12), gfx::SizeF(crop_size)));
  surface->SetBufferTransform(Transform::NORMAL);
  surface->Commit();

  base::RunLoop().RunUntilIdle();

  {
    const viz::CompositorFrame& frame =
        GetFrameFromSurface(shell_surface.get());
    ASSERT_EQ(1u, frame.render_pass_list.size());
    const viz::QuadList& quad_list = frame.render_pass_list[0]->quad_list;
    ASSERT_EQ(1u, quad_list.size());
    const viz::TextureDrawQuad* quad =
        viz::TextureDrawQuad::MaterialCast(quad_list.front());
    EXPECT_EQ(crop_0.origin(), quad->uv_top_left);
    EXPECT_EQ(crop_0.bottom_right(), quad->uv_bottom_right);
    EXPECT_EQ(
        ToPixel(gfx::Rect(0, 0, 52, 4)),
        cc::MathUtil::MapEnclosingClippedRect(
            quad->shared_quad_state->quad_to_target_transform, quad->rect));
  }

  surface->SetBufferTransform(Transform::ROTATE_90);
  surface->Commit();

  base::RunLoop().RunUntilIdle();

  {
    const viz::CompositorFrame& frame =
        GetFrameFromSurface(shell_surface.get());
    ASSERT_EQ(1u, frame.render_pass_list.size());
    const viz::QuadList& quad_list = frame.render_pass_list[0]->quad_list;
    ASSERT_EQ(1u, quad_list.size());
    const viz::TextureDrawQuad* quad =
        viz::TextureDrawQuad::MaterialCast(quad_list.front());
    EXPECT_EQ(crop_90.origin(), quad->uv_top_left);
    EXPECT_EQ(crop_90.bottom_right(), quad->uv_bottom_right);
    EXPECT_EQ(
        ToPixel(gfx::Rect(0, 0, 52, 4)),
        cc::MathUtil::MapEnclosingClippedRect(
            quad->shared_quad_state->quad_to_target_transform, quad->rect));
  }

  surface->SetBufferTransform(Transform::ROTATE_180);
  surface->Commit();

  base::RunLoop().RunUntilIdle();

  {
    const viz::CompositorFrame& frame =
        GetFrameFromSurface(shell_surface.get());
    ASSERT_EQ(1u, frame.render_pass_list.size());
    const viz::QuadList& quad_list = frame.render_pass_list[0]->quad_list;
    ASSERT_EQ(1u, quad_list.size());
    const viz::TextureDrawQuad* quad =
        viz::TextureDrawQuad::MaterialCast(quad_list.front());
    EXPECT_EQ(crop_180.origin(), quad->uv_top_left);
    EXPECT_EQ(crop_180.bottom_right(), quad->uv_bottom_right);
    EXPECT_EQ(
        ToPixel(gfx::Rect(0, 0, 52, 4)),
        cc::MathUtil::MapEnclosingClippedRect(
            quad->shared_quad_state->quad_to_target_transform, quad->rect));
  }

  surface->SetBufferTransform(Transform::ROTATE_270);
  surface->Commit();

  base::RunLoop().RunUntilIdle();

  {
    const viz::CompositorFrame& frame =
        GetFrameFromSurface(shell_surface.get());
    ASSERT_EQ(1u, frame.render_pass_list.size());
    const viz::QuadList& quad_list = frame.render_pass_list[0]->quad_list;
    ASSERT_EQ(1u, quad_list.size());
    const viz::TextureDrawQuad* quad =
        viz::TextureDrawQuad::MaterialCast(quad_list.front());
    EXPECT_EQ(crop_270.origin(), quad->uv_top_left);
    EXPECT_EQ(crop_270.bottom_right(), quad->uv_bottom_right);
    EXPECT_EQ(
        target_with_no_viewport,
        cc::MathUtil::MapEnclosingClippedRect(
            quad->shared_quad_state->quad_to_target_transform, quad->rect));
  }

  surface->SetViewport(gfx::Size(128, 64));
  surface->SetBufferTransform(Transform::NORMAL);
  surface->Commit();

  base::RunLoop().RunUntilIdle();

  {
    const viz::CompositorFrame& frame =
        GetFrameFromSurface(shell_surface.get());
    ASSERT_EQ(1u, frame.render_pass_list.size());
    const viz::QuadList& quad_list = frame.render_pass_list[0]->quad_list;
    ASSERT_EQ(1u, quad_list.size());
    const viz::TextureDrawQuad* quad =
        viz::TextureDrawQuad::MaterialCast(quad_list.front());
    EXPECT_EQ(crop_0.origin(), quad->uv_top_left);
    EXPECT_EQ(crop_0.bottom_right(), quad->uv_bottom_right);
    EXPECT_EQ(
        target_with_viewport,
        cc::MathUtil::MapEnclosingClippedRect(
            quad->shared_quad_state->quad_to_target_transform, quad->rect));
  }

  surface->SetBufferTransform(Transform::ROTATE_90);
  surface->Commit();

  base::RunLoop().RunUntilIdle();

  {
    const viz::CompositorFrame& frame =
        GetFrameFromSurface(shell_surface.get());
    ASSERT_EQ(1u, frame.render_pass_list.size());
    const viz::QuadList& quad_list = frame.render_pass_list[0]->quad_list;
    ASSERT_EQ(1u, quad_list.size());
    const viz::TextureDrawQuad* quad =
        viz::TextureDrawQuad::MaterialCast(quad_list.front());
    EXPECT_EQ(crop_90.origin(), quad->uv_top_left);
    EXPECT_EQ(crop_90.bottom_right(), quad->uv_bottom_right);
    EXPECT_EQ(
        target_with_viewport,
        cc::MathUtil::MapEnclosingClippedRect(
            quad->shared_quad_state->quad_to_target_transform, quad->rect));
  }

  surface->SetBufferTransform(Transform::ROTATE_180);
  surface->Commit();

  base::RunLoop().RunUntilIdle();

  {
    const viz::CompositorFrame& frame =
        GetFrameFromSurface(shell_surface.get());
    ASSERT_EQ(1u, frame.render_pass_list.size());
    const viz::QuadList& quad_list = frame.render_pass_list[0]->quad_list;
    ASSERT_EQ(1u, quad_list.size());
    const viz::TextureDrawQuad* quad =
        viz::TextureDrawQuad::MaterialCast(quad_list.front());
    EXPECT_EQ(crop_180.origin(), quad->uv_top_left);
    EXPECT_EQ(crop_180.bottom_right(), quad->uv_bottom_right);
    EXPECT_EQ(
        target_with_viewport,
        cc::MathUtil::MapEnclosingClippedRect(
            quad->shared_quad_state->quad_to_target_transform, quad->rect));
  }

  surface->SetBufferTransform(Transform::ROTATE_270);
  surface->Commit();

  base::RunLoop().RunUntilIdle();

  {
    const viz::CompositorFrame& frame =
        GetFrameFromSurface(shell_surface.get());
    ASSERT_EQ(1u, frame.render_pass_list.size());
    const viz::QuadList& quad_list = frame.render_pass_list[0]->quad_list;
    ASSERT_EQ(1u, quad_list.size());
    const viz::TextureDrawQuad* quad =
        viz::TextureDrawQuad::MaterialCast(quad_list.front());
    EXPECT_EQ(crop_270.origin(), quad->uv_top_left);
    EXPECT_EQ(crop_270.bottom_right(), quad->uv_bottom_right);
    EXPECT_EQ(
        target_with_viewport,
        cc::MathUtil::MapEnclosingClippedRect(
            quad->shared_quad_state->quad_to_target_transform, quad->rect));
  }
}

TEST_P(SurfaceTest, SetBlendMode) {
  gfx::Size buffer_size(1, 1);
  auto buffer = std::make_unique<Buffer>(
      exo_test_helper()->CreateGpuMemoryBuffer(buffer_size));
  auto surface = std::make_unique<Surface>();
  auto shell_surface = std::make_unique<ShellSurface>(surface.get());

  surface->Attach(buffer.get());
  surface->SetBlendMode(SkBlendMode::kSrc);
  surface->Commit();
  base::RunLoop().RunUntilIdle();

  const viz::CompositorFrame& frame = GetFrameFromSurface(shell_surface.get());
  ASSERT_EQ(1u, frame.render_pass_list.size());
  ASSERT_EQ(1u, frame.render_pass_list.back()->quad_list.size());
  EXPECT_FALSE(frame.render_pass_list.back()
                   ->quad_list.back()
                   ->ShouldDrawWithBlending());
}

TEST_P(SurfaceTest, OverlayCandidate) {
  gfx::Size buffer_size(1, 1);
  auto buffer = std::make_unique<Buffer>(
      exo_test_helper()->CreateGpuMemoryBuffer(buffer_size), GL_TEXTURE_2D, 0,
      true, true, false);
  auto surface = std::make_unique<Surface>();
  auto shell_surface = std::make_unique<ShellSurface>(surface.get());

  surface->Attach(buffer.get());
  surface->Commit();
  base::RunLoop().RunUntilIdle();

  const viz::CompositorFrame& frame = GetFrameFromSurface(shell_surface.get());
  ASSERT_EQ(1u, frame.render_pass_list.size());
  ASSERT_EQ(1u, frame.render_pass_list.back()->quad_list.size());
  viz::DrawQuad* draw_quad = frame.render_pass_list.back()->quad_list.back();
  ASSERT_EQ(viz::DrawQuad::Material::kTextureContent, draw_quad->material);

  const viz::TextureDrawQuad* texture_quad =
      viz::TextureDrawQuad::MaterialCast(draw_quad);
  EXPECT_FALSE(texture_quad->resource_size_in_pixels().IsEmpty());
}

TEST_P(SurfaceTest, SetAlpha) {
  gfx::Size buffer_size(1, 1);
  auto buffer = std::make_unique<Buffer>(
      exo_test_helper()->CreateGpuMemoryBuffer(buffer_size), GL_TEXTURE_2D, 0,
      true, true, false);
  auto surface = std::make_unique<Surface>();
  auto shell_surface = std::make_unique<ShellSurface>(surface.get());

  {
    surface->Attach(buffer.get());
    surface->SetAlpha(0.5f);
    surface->Commit();
    base::RunLoop().RunUntilIdle();

    const viz::CompositorFrame& frame =
        GetFrameFromSurface(shell_surface.get());
    ASSERT_EQ(1u, frame.render_pass_list.size());
    ASSERT_EQ(1u, frame.render_pass_list.back()->quad_list.size());
    ASSERT_EQ(1u, frame.resource_list.size());
    ASSERT_EQ(1u, frame.resource_list.back().id);
    EXPECT_EQ(gfx::Rect(buffer_size),
              ToTargetSpaceDamage(frame.render_pass_list.back()->damage_rect));
  }

  {
    surface->SetAlpha(0.f);
    surface->Commit();
    base::RunLoop().RunUntilIdle();

    const viz::CompositorFrame& frame =
        GetFrameFromSurface(shell_surface.get());
    ASSERT_EQ(1u, frame.render_pass_list.size());
    // No quad if alpha is 0.
    ASSERT_EQ(0u, frame.render_pass_list.back()->quad_list.size());
    ASSERT_EQ(0u, frame.resource_list.size());
    EXPECT_EQ(gfx::Rect(buffer_size),
              ToTargetSpaceDamage(frame.render_pass_list.back()->damage_rect));
  }

  {
    surface->SetAlpha(1.f);
    surface->Commit();
    base::RunLoop().RunUntilIdle();

    const viz::CompositorFrame& frame =
        GetFrameFromSurface(shell_surface.get());
    ASSERT_EQ(1u, frame.render_pass_list.size());
    ASSERT_EQ(1u, frame.render_pass_list.back()->quad_list.size());
    ASSERT_EQ(1u, frame.resource_list.size());
    // The resource should be updated again, the id should be changed.
    ASSERT_EQ(2u, frame.resource_list.back().id);
    EXPECT_EQ(gfx::Rect(buffer_size),
              ToTargetSpaceDamage(frame.render_pass_list.back()->damage_rect));
  }
}

TEST_P(SurfaceTest, SurfaceQuad) {
  gfx::Size buffer_size(1, 1);
  auto buffer = std::make_unique<Buffer>(
      exo_test_helper()->CreateGpuMemoryBuffer(buffer_size), GL_TEXTURE_2D, 0,
      true, true, false);
  auto surface = std::make_unique<Surface>();
  auto shell_surface = std::make_unique<ShellSurface>(surface.get());
  surface->Attach(buffer.get());
  surface->SetAlpha(1.0f);

  surface->SetEmbeddedSurfaceSize(gfx::Size(1, 1));
  surface->SetEmbeddedSurfaceId(base::BindRepeating([]() -> viz::SurfaceId {
    return viz::SurfaceId(
        viz::FrameSinkId(1, 1),
        viz::LocalSurfaceId(1, 1, base::UnguessableToken::Create()));
  }));

  {
    surface->Commit();
    base::RunLoop().RunUntilIdle();

    const viz::CompositorFrame& frame =
        GetFrameFromSurface(shell_surface.get());
    EXPECT_EQ(1u, frame.render_pass_list.size());
    EXPECT_EQ(1u, frame.render_pass_list.back()->quad_list.size());
    EXPECT_EQ(1u, frame.resource_list.size());
    // Ensure that the quad is correct and the resource is included.
    EXPECT_EQ(1u, frame.resource_list.back().id);
    EXPECT_EQ(viz::DrawQuad::Material::kSurfaceContent,
              frame.render_pass_list.back()->quad_list.back()->material);
  }
}

TEST_P(SurfaceTest, EmptySurfaceQuad) {
  gfx::Size buffer_size(1, 1);
  auto buffer = std::make_unique<Buffer>(
      exo_test_helper()->CreateGpuMemoryBuffer(buffer_size), GL_TEXTURE_2D, 0,
      true, true, false);
  auto surface = std::make_unique<Surface>();
  auto shell_surface = std::make_unique<ShellSurface>(surface.get());
  surface->Attach(buffer.get());
  surface->SetAlpha(1.0f);

  // Explicitly zero the size, no quad should be produced.
  surface->SetEmbeddedSurfaceSize(gfx::Size(0, 0));
  surface->SetEmbeddedSurfaceId(base::BindRepeating([]() -> viz::SurfaceId {
    return viz::SurfaceId(
        viz::FrameSinkId(1, 1),
        viz::LocalSurfaceId(1, 1, base::UnguessableToken::Create()));
  }));

  {
    surface->Commit();
    base::RunLoop().RunUntilIdle();

    const viz::CompositorFrame& frame =
        GetFrameFromSurface(shell_surface.get());
    EXPECT_EQ(1u, frame.render_pass_list.size());
    EXPECT_EQ(0u, frame.render_pass_list.back()->quad_list.size());
    // No quad but still has a resource though.
    EXPECT_EQ(1u, frame.resource_list.size());
    EXPECT_EQ(1u, frame.resource_list.back().id);
  }
}

TEST_P(SurfaceTest, ScaledSurfaceQuad) {
  gfx::Size buffer_size(1, 1);
  auto buffer = std::make_unique<Buffer>(
      exo_test_helper()->CreateGpuMemoryBuffer(buffer_size), GL_TEXTURE_2D, 0,
      true, true, false);
  auto surface = std::make_unique<Surface>();
  auto shell_surface = std::make_unique<ShellSurface>(surface.get());
  surface->Attach(buffer.get());
  surface->SetAlpha(1.0f);

  surface->SetEmbeddedSurfaceId(base::BindRepeating([]() -> viz::SurfaceId {
    return viz::SurfaceId(
        viz::FrameSinkId(1, 1),
        viz::LocalSurfaceId(1, 1, base::UnguessableToken::Create()));
  }));

  // A 256x256 surface, of which as 128x128 chunk is selected, drawn into a
  // 128x64 rect.
  surface->SetEmbeddedSurfaceSize(gfx::Size(256, 256));

  surface->SetViewport(gfx::Size(128, 64));
  surface->SetCrop(
      gfx::RectF(gfx::PointF(32.0f, 32.0f), gfx::SizeF(128.0f, 128.0f)));

  {
    surface->Commit();
    base::RunLoop().RunUntilIdle();

    const viz::CompositorFrame& frame =
        GetFrameFromSurface(shell_surface.get());
    EXPECT_EQ(1u, frame.render_pass_list.size());
    EXPECT_EQ(1u, frame.render_pass_list.back()->quad_list.size());
    EXPECT_EQ(1u, frame.resource_list.size());
    // Ensure that the quad is correct and the resource is included.
    EXPECT_EQ(1u, frame.resource_list.back().id);
    EXPECT_EQ(viz::DrawQuad::Material::kSurfaceContent,
              frame.render_pass_list.back()->quad_list.back()->material);
    // We are outputting to 0,0 -> 128,64.
    EXPECT_EQ(gfx::Rect(gfx::Point(), gfx::Size(128, 64)),
              frame.render_pass_list.back()
                  ->quad_list.back()
                  ->shared_quad_state->clip_rect);
    // Rect should be the unmodified surface size.
    EXPECT_EQ(gfx::Rect(gfx::Point(0, 0), gfx::Size(256, 256)),
              frame.render_pass_list.back()->quad_list.back()->rect);
    // To get 32,32 -> 160,160 into the correct position it must be translated
    // backwards and scaled 0.5x in Y, then everything is scaled by the scale
    // factor.
    EXPECT_EQ(gfx::Transform(1.0f * device_scale_factor(), 0.0f, 0.0f,
                             0.5f * device_scale_factor(),
                             -32.0f * device_scale_factor(),
                             -16.0f * device_scale_factor()),
              frame.render_pass_list.back()
                  ->quad_list.back()
                  ->shared_quad_state->quad_to_target_transform);
  }
}

TEST_P(SurfaceTest, Commit) {
  std::unique_ptr<Surface> surface(new Surface);

  // Calling commit without a buffer should succeed.
  surface->Commit();
}

TEST_P(SurfaceTest, RemoveSubSurface) {
  gfx::Size buffer_size(256, 256);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  std::unique_ptr<Surface> surface(new Surface);
  auto shell_surface = std::make_unique<ShellSurface>(surface.get());
  surface->Attach(buffer.get());

  // Create a subsurface:
  gfx::Size child_buffer_size(64, 128);
  auto child_buffer = std::make_unique<Buffer>(
      exo_test_helper()->CreateGpuMemoryBuffer(child_buffer_size));
  auto child_surface = std::make_unique<Surface>();
  auto sub_surface =
      std::make_unique<SubSurface>(child_surface.get(), surface.get());
  sub_surface->SetPosition(gfx::Point(20, 10));
  child_surface->Attach(child_buffer.get());
  child_surface->Commit();
  surface->Commit();
  base::RunLoop().RunUntilIdle();

  // Remove the subsurface by destroying it. This should not damage |surface|.
  // TODO(penghuang): Make the damage more precise for sub surface changes.
  // https://crbug.com/779704
  sub_surface.reset();
  EXPECT_FALSE(surface->HasPendingDamageForTesting(gfx::Rect(20, 10, 64, 128)));
}

TEST_P(SurfaceTest, DestroyAttachedBuffer) {
  gfx::Size buffer_size(1, 1);
  auto buffer = std::make_unique<Buffer>(
      exo_test_helper()->CreateGpuMemoryBuffer(buffer_size));
  auto surface = std::make_unique<Surface>();
  auto shell_surface = std::make_unique<ShellSurface>(surface.get());

  surface->Attach(buffer.get());
  surface->Commit();
  base::RunLoop().RunUntilIdle();

  // Make sure surface size is still valid after buffer is destroyed.
  buffer.reset();
  surface->Commit();
  EXPECT_FALSE(surface->content_size().IsEmpty());
}

TEST_P(SurfaceTest, SetClientSurfaceId) {
  auto surface = std::make_unique<Surface>();
  constexpr int kTestId = 42;

  surface->SetClientSurfaceId(kTestId);
  EXPECT_EQ(kTestId, surface->GetClientSurfaceId());
}

TEST_P(SurfaceTest, DestroyWithAttachedBufferReleasesBuffer) {
  gfx::Size buffer_size(1, 1);
  auto buffer = std::make_unique<Buffer>(
      exo_test_helper()->CreateGpuMemoryBuffer(buffer_size));
  auto surface = std::make_unique<Surface>();
  auto shell_surface = std::make_unique<ShellSurface>(surface.get());

  int release_buffer_call_count = 0;
  buffer->set_release_callback(base::BindRepeating(
      &ReleaseBuffer, base::Unretained(&release_buffer_call_count)));

  surface->Attach(buffer.get());
  surface->Commit();
  base::RunLoop().RunUntilIdle();
  // Buffer is still attached at this point.
  EXPECT_EQ(0, release_buffer_call_count);

  // After the surface is destroyed, we should get a release event for the
  // attached buffer.
  shell_surface.reset();
  surface.reset();
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1, release_buffer_call_count);
}

TEST_P(SurfaceTest, AcquireFence) {
  auto buffer = std::make_unique<Buffer>(
      exo_test_helper()->CreateGpuMemoryBuffer(gfx::Size(1, 1)));
  auto surface = std::make_unique<Surface>();

  // We can only commit an acquire fence if a buffer is attached.
  surface->Attach(buffer.get());

  EXPECT_FALSE(surface->HasPendingAcquireFence());
  surface->SetAcquireFence(
      std::make_unique<gfx::GpuFence>(gfx::GpuFenceHandle()));
  EXPECT_TRUE(surface->HasPendingAcquireFence());
  surface->Commit();
  EXPECT_FALSE(surface->HasPendingAcquireFence());
}

TEST_P(SurfaceTest, UpdatesOcclusionOnDestroyingSubsurface) {
  gfx::Size buffer_size(256, 512);
  auto buffer = std::make_unique<Buffer>(
      exo_test_helper()->CreateGpuMemoryBuffer(buffer_size));
  auto surface = std::make_unique<Surface>();
  auto shell_surface = std::make_unique<ShellSurface>(surface.get());
  surface->Attach(buffer.get());
  surface->Commit();

  gfx::Size child_buffer_size(64, 128);
  auto child_buffer = std::make_unique<Buffer>(
      exo_test_helper()->CreateGpuMemoryBuffer(child_buffer_size));
  auto child_surface = std::make_unique<Surface>();
  auto sub_surface =
      std::make_unique<SubSurface>(child_surface.get(), surface.get());
  child_surface->Attach(child_buffer.get());
  // Turn on occlusion tracking.
  child_surface->SetOcclusionTracking(true);
  child_surface->Commit();
  surface->Commit();

  SurfaceObserverForTest observer;
  ScopedSurface scoped_child_surface(child_surface.get(), &observer);

  // Destroy the subsurface and expect to get an occlusion update.
  sub_surface.reset();
  EXPECT_EQ(1, observer.num_occlusion_changes());
  EXPECT_EQ(aura::Window::OcclusionState::HIDDEN,
            child_surface->window()->occlusion_state());
}

}  // namespace
}  // namespace exo
