// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/exo/surface.h"

#include <tuple>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ref.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "components/exo/buffer.h"
#include "components/exo/shell_surface.h"
#include "components/exo/sub_surface.h"
#include "components/exo/surface_test_util.h"
#include "components/exo/test/exo_test_base.h"
#include "components/exo/test/exo_test_helper.h"
#include "components/exo/test/shell_surface_builder.h"
#include "components/exo/test/surface_tree_host_test_util.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/service/surfaces/surface.h"
#include "components/viz/service/surfaces/surface_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "ui/aura/test/window_occlusion_tracker_test_api.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/display/display.h"
#include "ui/display/display_switches.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/geometry/test/geometry_util.h"
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

std::string TransformToString(Transform transform) {
  std::string prefix = "Transform::";
  std::string name;
  switch (transform) {
    case Transform::NORMAL:
      name = "NORMAL";
      break;
    case Transform::ROTATE_90:
      name = "ROTATE_90";
      break;
    case Transform::ROTATE_180:
      name = "ROTATE_180";
      break;
    case Transform::ROTATE_270:
      name = "ROTATE_270";
      break;
    case Transform::FLIPPED:
      name = "FLIPPED";
      break;
    case Transform::FLIPPED_ROTATE_90:
      name = "FLIPPED_ROTATE_90";
      break;
    case Transform::FLIPPED_ROTATE_180:
      name = "FLIPPED_ROTATE_180";
      break;
    case Transform::FLIPPED_ROTATE_270:
      name = "FLIPPED_ROTATE_270";
      break;
    default:
      return "[UNKNOWN_TRANSFORM]";
  }
  return prefix + name;
}

class SurfaceTest : public test::ExoTestBase,
                    public ::testing::WithParamInterface<
                        std::tuple<test::FrameSubmissionType, float>> {
 public:
  SurfaceTest() {
    test::SetFrameSubmissionFeatureFlags(&feature_list_,
                                         GetFrameSubmissionType());
  }

  SurfaceTest(const SurfaceTest&) = delete;
  SurfaceTest& operator=(const SurfaceTest&) = delete;

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

  test::FrameSubmissionType GetFrameSubmissionType() const {
    return std::get<0>(GetParam());
  }
  float device_scale_factor() const { return std::get<1>(GetParam()); }

  gfx::Rect ToPixel(const gfx::Rect rect) {
    return gfx::ToEnclosingRect(
        gfx::ConvertRectToPixels(rect, device_scale_factor()));
  }

  gfx::Rect GetCompleteDamage(const viz::CompositorFrame& frame) {
    auto& root_pass = frame.render_pass_list.back();
    gfx::Rect complete_damage = root_pass->damage_rect;

    for (auto* quad : root_pass->quad_list) {
      if (quad->material == viz::DrawQuad::Material::kTextureContent) {
        auto* texture_quad = viz::TextureDrawQuad::MaterialCast(quad);
        if (texture_quad->damage_rect.has_value()) {
          complete_damage.Union(texture_quad->damage_rect.value());
        }
      }
    }
    return complete_damage;
  }

  gfx::Rect ToTargetSpaceDamage(const viz::CompositorFrame& frame) {
    // Map a frame's damage back to the coordinate space of its buffer.
    return gfx::ScaleToEnclosingRect(GetCompleteDamage(frame),
                                     1 / device_scale_factor());
  }

  const viz::CompositorFrame& GetFrameFromSurface(ShellSurface* shell_surface) {
    viz::SurfaceId surface_id =
        *shell_surface->host_window()->layer()->GetSurfaceId();
    const viz::CompositorFrame& frame =
        GetSurfaceManager()->GetSurfaceForId(surface_id)->GetActiveFrame();
    return frame;
  }

  void SetBufferTransformHelperTransformAndTest(Surface* surface,
                                                ShellSurface* shell_surface,
                                                Transform transform,
                                                const gfx::Size& expected_size);

  void SetCropAndBufferTransformHelperTransformAndTest(
      Surface* surface,
      ShellSurface* shell_surface,
      Transform transform,
      const gfx::RectF& expected_rect,
      bool has_viewport);

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Instantiate the values of frame submission types and device scale factor in
// the parameterized tests.
INSTANTIATE_TEST_SUITE_P(
    All,
    SurfaceTest,
    testing::Combine(testing::Values(test::FrameSubmissionType::kNoReactive,
                                     test::FrameSubmissionType::kReactive),
                     testing::Values(1.0f, 1.25f, 2.0f)));

TEST_P(SurfaceTest, AttachOffset) {
  gfx::Size buffer_size(256, 256);
  auto buffer = test::ExoTestHelper::CreateBuffer(buffer_size);
  auto surface = std::make_unique<Surface>();
  auto shell_surface = std::make_unique<ShellSurface>(surface.get());

  surface->Attach(buffer.get(), gfx::Vector2d(0, 0));
  surface->Commit();
  EXPECT_EQ(surface->GetBufferOffset(), gfx::Vector2d(0, 0));

  surface->Attach(buffer.get(), gfx::Vector2d(1, 2));
  surface->Commit();
  EXPECT_EQ(surface->GetBufferOffset(), gfx::Vector2d(1, 2));

  surface->Attach(buffer.get(), gfx::Vector2d(1, 2));
  surface->Commit();
  EXPECT_EQ(surface->GetBufferOffset(), gfx::Vector2d(2, 4));

  surface->Attach(buffer.get(), gfx::Vector2d(-2, -4));
  surface->Commit();
  EXPECT_EQ(surface->GetBufferOffset(), gfx::Vector2d(0, 0));

  // Pending updates for the offset should not be accumulated.
  surface->Attach(buffer.get(), gfx::Vector2d(1, 2));
  surface->Attach(buffer.get(), gfx::Vector2d(3, 4));
  surface->Attach(buffer.get(), gfx::Vector2d(5, 6));
  surface->Commit();
  EXPECT_EQ(surface->GetBufferOffset(), gfx::Vector2d(5, 6));
}

TEST_P(SurfaceTest, AttachOffsetSynchronizedSubsurface) {
  gfx::Size buffer_size(256, 256);
  auto buffer = test::ExoTestHelper::CreateBuffer(buffer_size);
  auto surface = std::make_unique<Surface>();
  auto shell_surface = std::make_unique<ShellSurface>(surface.get());

  gfx::Size child_buffer_size(128, 128);
  auto child_buffer = test::ExoTestHelper::CreateBuffer(child_buffer_size);
  auto child_surface = std::make_unique<Surface>();
  auto sub = std::make_unique<SubSurface>(child_surface.get(), surface.get());
  sub->surface()->Attach(child_buffer.get(), gfx::Vector2d(0, 0));
  sub->SetCommitBehavior(/*synchronized=*/true);
  EXPECT_EQ(sub->surface()->GetBufferOffset(), gfx::Vector2d(0, 0));

  sub->surface()->Attach(child_buffer.get(), gfx::Vector2d(1, 2));
  sub->surface()->Commit();
  sub->surface()->Attach(child_buffer.get(), gfx::Vector2d(1, 2));
  sub->surface()->Commit();

  // The offset should not be updated by subsurface commits since this
  // subsurface is in the synchronized mode.
  EXPECT_EQ(sub->surface()->GetBufferOffset(), gfx::Vector2d(0, 0));

  // Once parent surface is committed, the offset should be updated. The cached
  // offset should be accumulated.
  surface->Commit();
  EXPECT_EQ(sub->surface()->GetBufferOffset(), gfx::Vector2d(2, 4));

  // Try again.
  sub->surface()->Attach(child_buffer.get(), gfx::Vector2d(1, 2));
  sub->surface()->Commit();
  surface->Commit();
  EXPECT_EQ(sub->surface()->GetBufferOffset(), gfx::Vector2d(3, 6));
}

TEST_P(SurfaceTest, AttachOffsetDesynchronizedSubsurface) {
  gfx::Size buffer_size(256, 256);
  auto buffer = test::ExoTestHelper::CreateBuffer(buffer_size);
  auto surface = std::make_unique<Surface>();
  auto shell_surface = std::make_unique<ShellSurface>(surface.get());

  gfx::Size child_buffer_size(128, 128);
  auto child_buffer = test::ExoTestHelper::CreateBuffer(child_buffer_size);
  auto child_surface = std::make_unique<Surface>();
  auto sub = std::make_unique<SubSurface>(child_surface.get(), surface.get());
  sub->surface()->Attach(child_buffer.get(), gfx::Vector2d(0, 0));
  sub->SetCommitBehavior(/*synchronized=*/false);
  EXPECT_EQ(sub->surface()->GetBufferOffset(), gfx::Vector2d(0, 0));

  sub->surface()->Attach(child_buffer.get(), gfx::Vector2d(1, 2));

  // Parent's commit does not take affect for the subsurface.
  surface->Commit();
  EXPECT_EQ(sub->surface()->GetBufferOffset(), gfx::Vector2d(0, 0));

  // This should replace the pending offset because the previous one is not
  // committed.
  sub->surface()->Attach(child_buffer.get(), gfx::Vector2d(10, 20));

  // The offset should be updated by subsurface commit.
  sub->surface()->Commit();
  EXPECT_EQ(sub->surface()->GetBufferOffset(), gfx::Vector2d(10, 20));
}

TEST_P(SurfaceTest, Damage) {
  gfx::Size buffer_size(256, 256);
  auto buffer = test::ExoTestHelper::CreateBuffer(buffer_size);
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
  test::WaitForLastFrameAck(shell_surface.get());

  {
    const viz::CompositorFrame& frame =
        GetFrameFromSurface(shell_surface.get());
    EXPECT_EQ(ToPixel(gfx::Rect(buffer_size)), GetCompleteDamage(frame));
  }

  gfx::RectF buffer_damage(32, 64, 16, 32);
  gfx::Rect surface_damage = gfx::ToNearestRect(buffer_damage);

  // Check that damage is correct for a non-square rectangle not at the origin.
  surface->Damage(surface_damage);
  surface->Commit();
  test::WaitForLastFrameAck(shell_surface.get());

  // Adjust damage for DSF filtering and verify it below.
  if (device_scale_factor() > 1.f)
    buffer_damage.Inset(-1.f);

  {
    const viz::CompositorFrame& frame =
        GetFrameFromSurface(shell_surface.get());
    EXPECT_TRUE(
        ToTargetSpaceDamage(frame).Contains(gfx::ToNearestRect(buffer_damage)));
  }
}

TEST_P(SurfaceTest, SubsurfaceDamageAggregation) {
  gfx::Size buffer_size(256, 512);
  auto buffer = test::ExoTestHelper::CreateBuffer(buffer_size);
  auto surface = std::make_unique<Surface>();
  auto shell_surface = std::make_unique<ShellSurface>(surface.get());
  surface->Attach(buffer.get());

  gfx::Size child_buffer_size(64, 128);
  auto child_buffer = test::ExoTestHelper::CreateBuffer(child_buffer_size);
  auto child_surface = std::make_unique<Surface>();
  auto sub_surface =
      std::make_unique<SubSurface>(child_surface.get(), surface.get());
  child_surface->Attach(child_buffer.get());
  child_surface->Commit();
  surface->Commit();
  test::WaitForLastFrameAck(shell_surface.get());

  {
    // Initial frame has full damage.
    const viz::CompositorFrame& frame =
        GetFrameFromSurface(shell_surface.get());
    const gfx::Rect scaled_damage = gfx::ToNearestRect(gfx::ScaleRect(
        gfx::RectF(gfx::Rect(buffer_size)), device_scale_factor()));
    EXPECT_EQ(scaled_damage, GetCompleteDamage(frame));
  }

  const gfx::RectF surface_damage(16, 16);
  const gfx::RectF subsurface_damage(32, 32, 16, 16);
  int margin = ceil(device_scale_factor());

  child_surface->Damage(gfx::ToNearestRect(subsurface_damage));
  child_surface->Commit();
  surface->Commit();
  test::WaitForLastFrameAck(shell_surface.get());

  {
    // Subsurface damage should be propagated.
    const viz::CompositorFrame& frame =
        GetFrameFromSurface(shell_surface.get());
    const gfx::Rect scaled_damage = gfx::ToNearestRect(
        gfx::ScaleRect(subsurface_damage, device_scale_factor()));
    EXPECT_TRUE(
        scaled_damage.ApproximatelyEqual(GetCompleteDamage(frame), margin));
  }

  surface->Damage(gfx::ToNearestRect(surface_damage));
  surface->Commit();
  test::WaitForLastFrameAck(shell_surface.get());

  {
    // When commit is called on the root with no call on the child, the damage
    // from the previous frame shouldn't persist.
    const viz::CompositorFrame& frame =
        GetFrameFromSurface(shell_surface.get());
    const gfx::Rect scaled_damage = gfx::ToNearestRect(
        gfx::ScaleRect(surface_damage, device_scale_factor()));
    EXPECT_TRUE(
        scaled_damage.ApproximatelyEqual(GetCompleteDamage(frame), margin));
  }
}

TEST_P(SurfaceTest, SubsurfaceDamageSynchronizedCommitBehavior) {
  gfx::Size buffer_size(256, 512);
  auto buffer = test::ExoTestHelper::CreateBuffer(buffer_size);
  auto surface = std::make_unique<Surface>();
  auto shell_surface = std::make_unique<ShellSurface>(surface.get());
  surface->Attach(buffer.get());
  gfx::Size child_buffer_size(64, 128);
  auto child_buffer = test::ExoTestHelper::CreateBuffer(child_buffer_size);
  auto child_surface = std::make_unique<Surface>();
  auto sub_surface =
      std::make_unique<SubSurface>(child_surface.get(), surface.get());
  // Set commit behavior to synchronized.
  sub_surface->SetCommitBehavior(true);
  child_surface->Attach(child_buffer.get());
  child_surface->Commit();
  surface->Commit();
  test::WaitForLastFrameAck(shell_surface.get());

  {
    // Initial frame has full damage.
    const viz::CompositorFrame& frame =
        GetFrameFromSurface(shell_surface.get());
    const gfx::Rect scaled_damage = gfx::ToNearestRect(gfx::ScaleRect(
        gfx::RectF(gfx::Rect(buffer_size)), device_scale_factor()));
    EXPECT_EQ(scaled_damage, GetCompleteDamage(frame));
  }

  const gfx::RectF subsurface_damage(32, 32, 16, 16);
  const gfx::RectF subsurface_damage2(0, 0, 16, 16);
  int margin = ceil(device_scale_factor());

  child_surface->Damage(gfx::ToNearestRect(subsurface_damage));
  EXPECT_TRUE(child_surface->HasPendingDamageForTesting(
      gfx::ToNearestRect(subsurface_damage)));
  // Subsurface damage is cached.
  child_surface->Commit();
  EXPECT_FALSE(child_surface->HasPendingDamageForTesting(
      gfx::ToNearestRect(subsurface_damage)));
  EXPECT_TRUE(shell_surface->GetFrameCallbacksForTesting().empty());

  {
    // Subsurface damage should not be propagated at all.
    const viz::CompositorFrame& frame =
        GetFrameFromSurface(shell_surface.get());
    const gfx::Rect scaled_damage = gfx::ToNearestRect(gfx::ScaleRect(
        gfx::RectF(gfx::Rect(buffer_size)), device_scale_factor()));
    EXPECT_EQ(scaled_damage, GetCompleteDamage(frame));
  }

  // Damage but do not commit.
  child_surface->Damage(gfx::ToNearestRect(subsurface_damage2));
  EXPECT_TRUE(child_surface->HasPendingDamageForTesting(
      gfx::ToNearestRect(subsurface_damage2)));
  // Apply subsurface damage from cached state, not pending state.
  surface->Commit();
  test::WaitForLastFrameAck(shell_surface.get());

  {
    // Subsurface damage in cached state should be propagated.
    const viz::CompositorFrame& frame =
        GetFrameFromSurface(shell_surface.get());
    const gfx::Rect scaled_damage = gfx::ToNearestRect(
        gfx::ScaleRect(subsurface_damage, device_scale_factor()));
    EXPECT_TRUE(
        scaled_damage.ApproximatelyEqual(GetCompleteDamage(frame), margin));
  }
}

TEST_P(SurfaceTest, SubsurfaceDamageDesynchronizedCommitBehavior) {
  gfx::Size buffer_size(256, 512);
  auto buffer = test::ExoTestHelper::CreateBuffer(buffer_size);
  auto surface = std::make_unique<Surface>();
  auto shell_surface = std::make_unique<ShellSurface>(surface.get());
  surface->Attach(buffer.get());
  gfx::Size child_buffer_size(64, 128);
  auto child_buffer = test::ExoTestHelper::CreateBuffer(child_buffer_size);
  auto child_surface = std::make_unique<Surface>();
  auto sub_surface =
      std::make_unique<SubSurface>(child_surface.get(), surface.get());
  // Set commit behavior to desynchronized.
  sub_surface->SetCommitBehavior(false);
  child_surface->Attach(child_buffer.get());
  child_surface->Commit();
  surface->Commit();
  test::WaitForLastFrameAck(shell_surface.get());

  {
    // Initial frame has full damage.
    const viz::CompositorFrame& frame =
        GetFrameFromSurface(shell_surface.get());
    const gfx::Rect scaled_damage = gfx::ToNearestRect(gfx::ScaleRect(
        gfx::RectF(gfx::Rect(buffer_size)), device_scale_factor()));
    EXPECT_EQ(scaled_damage, GetCompleteDamage(frame));
  }

  const gfx::RectF subsurface_damage(32, 32, 16, 16);
  int margin = ceil(device_scale_factor());

  child_surface->Damage(gfx::ToNearestRect(subsurface_damage));
  EXPECT_TRUE(child_surface->HasPendingDamageForTesting(
      gfx::ToNearestRect(subsurface_damage)));
  // Subsurface damage is applied.
  child_surface->Commit();
  EXPECT_FALSE(child_surface->HasPendingDamageForTesting(
      gfx::ToNearestRect(subsurface_damage)));
  test::WaitForLastFrameAck(shell_surface.get());

  {
    // Subsurface damage should be propagated.
    const viz::CompositorFrame& frame =
        GetFrameFromSurface(shell_surface.get());
    const gfx::Rect scaled_damage = gfx::ToNearestRect(
        gfx::ScaleRect(subsurface_damage, device_scale_factor()));
    EXPECT_TRUE(
        scaled_damage.ApproximatelyEqual(GetCompleteDamage(frame), margin));
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
  auto buffer = test::ExoTestHelper::CreateBuffer(buffer_size);
  auto surface = std::make_unique<Surface>();
  auto shell_surface = std::make_unique<ShellSurface>(surface.get());

  // Attaching a buffer with alpha channel.
  surface->Attach(buffer.get());

  // Setting an opaque region that contains the buffer size doesn't require
  // draw with blending.
  surface->SetOpaqueRegion(gfx::Rect(256, 256));
  surface->Commit();
  test::WaitForLastFrameAck(shell_surface.get());

  {
    const viz::CompositorFrame& frame =
        GetFrameFromSurface(shell_surface.get());
    ASSERT_EQ(1u, frame.render_pass_list.size());
    ASSERT_EQ(1u, frame.render_pass_list.back()->quad_list.size());
    auto* texture_draw_quad = viz::TextureDrawQuad::MaterialCast(
        frame.render_pass_list.back()->quad_list.back());

    EXPECT_FALSE(texture_draw_quad->ShouldDrawWithBlending());
    EXPECT_EQ(SkColors::kBlack, texture_draw_quad->background_color);
    EXPECT_EQ(gfx::Rect(buffer_size), ToTargetSpaceDamage(frame));
  }

  // Setting an empty opaque region requires draw with blending.
  surface->SetOpaqueRegion(gfx::Rect());
  surface->Commit();
  test::WaitForLastFrameAck(shell_surface.get());

  {
    const viz::CompositorFrame& frame =
        GetFrameFromSurface(shell_surface.get());
    ASSERT_EQ(1u, frame.render_pass_list.size());
    ASSERT_EQ(1u, frame.render_pass_list.back()->quad_list.size());
    auto* texture_draw_quad = viz::TextureDrawQuad::MaterialCast(
        frame.render_pass_list.back()->quad_list.back());
    EXPECT_TRUE(texture_draw_quad->ShouldDrawWithBlending());
    EXPECT_EQ(SkColors::kTransparent, texture_draw_quad->background_color);
    EXPECT_EQ(gfx::Rect(buffer_size), ToTargetSpaceDamage(frame));
  }

  auto buffer_without_alpha = test::ExoTestHelper::CreateBuffer(
      buffer_size, gfx::BufferFormat::RGBX_8888);

  // Attaching a buffer without an alpha channel doesn't require draw with
  // blending.
  surface->Attach(buffer_without_alpha.get());
  surface->Commit();
  test::WaitForLastFrameAck(shell_surface.get());

  {
    const viz::CompositorFrame& frame =
        GetFrameFromSurface(shell_surface.get());
    ASSERT_EQ(1u, frame.render_pass_list.size());
    ASSERT_EQ(1u, frame.render_pass_list.back()->quad_list.size());
    EXPECT_FALSE(frame.render_pass_list.back()
                     ->quad_list.back()
                     ->ShouldDrawWithBlending());
    EXPECT_EQ(ToPixel(gfx::Rect(0, 0, 0, 0)), GetCompleteDamage(frame));
  }
}

TEST_P(SurfaceTest, SetInputRegion) {
  // Create a shell surface which size is 512x512.
  auto surface = std::make_unique<Surface>();
  auto shell_surface = std::make_unique<ShellSurface>(surface.get());
  gfx::Size buffer_size(512, 512);
  auto buffer = test::ExoTestHelper::CreateBuffer(buffer_size);
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
    auto child_buffer =
        test::ExoTestHelper::CreateBuffer(child_input_rect.size());
    auto child_surface = std::make_unique<Surface>();
    auto sub_surface =
        std::make_unique<SubSurface>(child_surface.get(), surface.get());
    sub_surface->SetPosition(gfx::PointF(child_input_rect.origin()));
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
  auto buffer = test::ExoTestHelper::CreateBuffer(buffer_size);
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
  gfx::SizeF buffer_size_float = gfx::SizeF(buffer_size);
  buffer_size_float.Scale(1.0f / kBufferScale);
  EXPECT_EQ(buffer_size_float.ToString(), surface->content_size().ToString());

  test::WaitForLastFrameAck(shell_surface.get());

  const viz::CompositorFrame& frame = GetFrameFromSurface(shell_surface.get());
  ASSERT_EQ(1u, frame.render_pass_list.size());
  EXPECT_EQ(ToPixel(gfx::Rect(0, 0, 256, 256)), GetCompleteDamage(frame));
}

void SurfaceTest::SetBufferTransformHelperTransformAndTest(
    Surface* surface,
    ShellSurface* shell_surface,
    Transform transform,
    const gfx::Size& expected_size) {
  std::stringstream scoped_trace_message;
  scoped_trace_message << "SetBufferTransformHelperTransformAndTest("
                       << "transform=" << TransformToString(transform) << ")";
  SCOPED_TRACE(scoped_trace_message.str());

  surface->SetBufferTransform(transform);
  surface->Commit();
  EXPECT_EQ(gfx::Size(expected_size.width(), expected_size.height()),
            surface->window()->bounds().size());
  EXPECT_EQ(gfx::SizeF(expected_size.width(), expected_size.height()),
            surface->content_size());

  test::WaitForLastFrameAck(shell_surface);

  {
    const viz::CompositorFrame& frame = GetFrameFromSurface(shell_surface);
    ASSERT_EQ(1u, frame.render_pass_list.size());
    EXPECT_EQ(
        ToPixel(gfx::Rect(0, 0, expected_size.width(), expected_size.height())),
        GetCompleteDamage(frame));
    const auto& quad_list = frame.render_pass_list[0]->quad_list;
    ASSERT_EQ(1u, quad_list.size());
    EXPECT_EQ(
        ToPixel(gfx::Rect(0, 0, 512, 256)),
        cc::MathUtil::MapEnclosingClippedRect(
            quad_list.front()->shared_quad_state->quad_to_target_transform,
            quad_list.front()->rect));
  }
}

// Disabled due to flakiness: crbug.com/856145
#if defined(LEAK_SANITIZER)
#define MAYBE_SetBufferTransform DISABLED_SetBufferTransform
#else
#define MAYBE_SetBufferTransform SetBufferTransform
#endif
TEST_P(SurfaceTest, MAYBE_SetBufferTransform) {
  gfx::Size buffer_size(256, 512);
  auto buffer = test::ExoTestHelper::CreateBuffer(buffer_size);
  auto surface = std::make_unique<Surface>();
  auto shell_surface = std::make_unique<ShellSurface>(surface.get());

  // This will update the bounds of the surface and take the buffer transform
  // into account.
  surface->Attach(buffer.get());

  gfx::Size inverted_size(buffer_size.height(), buffer_size.width());

  SetBufferTransformHelperTransformAndTest(surface.get(), shell_surface.get(),
                                           Transform::ROTATE_90, inverted_size);

  SetBufferTransformHelperTransformAndTest(surface.get(), shell_surface.get(),
                                           Transform::FLIPPED_ROTATE_90,
                                           inverted_size);

  gfx::Size child_buffer_size(64, 128);
  auto child_buffer = test::ExoTestHelper::CreateBuffer(child_buffer_size);
  auto child_surface = std::make_unique<Surface>();
  auto sub_surface =
      std::make_unique<SubSurface>(child_surface.get(), surface.get());

  // Set position to 20, 10.
  gfx::PointF child_position(20, 10);
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
      gfx::ToRoundedSize(child_surface->content_size()));

  test::WaitForLastFrameAck(shell_surface.get());

  {
    const viz::CompositorFrame& frame =
        GetFrameFromSurface(shell_surface.get());
    ASSERT_EQ(1u, frame.render_pass_list.size());
    const auto& quad_list = frame.render_pass_list[0]->quad_list;
    ASSERT_EQ(2u, quad_list.size());
    EXPECT_EQ(
        ToPixel(gfx::Rect(gfx::ToRoundedPoint(child_position),
                          gfx::ScaleToRoundedSize(child_buffer_size,
                                                  1.0f / kChildBufferScale))),
        cc::MathUtil::MapEnclosingClippedRect(
            quad_list.front()->shared_quad_state->quad_to_target_transform,
            quad_list.front()->rect));
  }
}

TEST_P(SurfaceTest, MirrorLayers) {
  gfx::Size buffer_size(512, 512);
  auto buffer = test::ExoTestHelper::CreateBuffer(buffer_size);
  auto surface = std::make_unique<Surface>();
  auto shell_surface = std::make_unique<ShellSurface>(surface.get());

  surface->Attach(buffer.get());
  surface->Commit();

  test::WaitForLastFrameAck(shell_surface.get());

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
  auto buffer = test::ExoTestHelper::CreateBuffer(buffer_size);
  auto surface = std::make_unique<Surface>();
  auto shell_surface = std::make_unique<ShellSurface>(surface.get());

  // This will update the bounds of the surface and take the viewport into
  // account.
  surface->Attach(buffer.get());
  gfx::SizeF viewport(256, 256);
  surface->SetViewport(viewport);
  surface->Commit();
  EXPECT_EQ(viewport.ToString(), surface->content_size().ToString());

  // This will update the bounds of the surface and take the viewport2 into
  // account.
  gfx::SizeF viewport2(512, 512);
  surface->SetViewport(viewport2);
  surface->Commit();
  EXPECT_EQ(viewport2.ToString(),
            gfx::SizeF(surface->window()->bounds().size()).ToString());
  EXPECT_EQ(viewport2.ToString(), surface->content_size().ToString());

  test::WaitForLastFrameAck(shell_surface.get());

  const viz::CompositorFrame& frame = GetFrameFromSurface(shell_surface.get());
  ASSERT_EQ(1u, frame.render_pass_list.size());
  EXPECT_EQ(ToPixel(gfx::Rect(0, 0, 512, 512)), GetCompleteDamage(frame));

  // This will make the surface have no content regardless of the viewport.
  surface->Attach(nullptr);
  surface->Commit();
  EXPECT_TRUE(surface->content_size().IsEmpty());
}

TEST_P(SurfaceTest, SubpixelCoordinate) {
  gfx::Size buffer_size(512, 512);
  auto buffer = test::ExoTestHelper::CreateBuffer(buffer_size);
  auto surface = std::make_unique<Surface>();
  auto shell_surface = std::make_unique<ShellSurface>(surface.get());

  // This will update the bounds of the surface and take the buffer transform
  // into account.
  surface->Attach(buffer.get());

  gfx::Size inverted_size(buffer_size.height(), buffer_size.width());

  gfx::Size child_buffer_size(64, 64);
  auto child_buffer = test::ExoTestHelper::CreateBuffer(child_buffer_size);
  auto child_surface = std::make_unique<Surface>();
  auto sub_surface =
      std::make_unique<SubSurface>(child_surface.get(), surface.get());

  gfx::Transform device_scale_transform;
  device_scale_transform.Scale(1.f / device_scale_factor(),
                               1.f / device_scale_factor());

  child_surface->Attach(child_buffer.get());

  // These rects are in pixel coordinates with some having subpixel coordinates.
  gfx::RectF kTestRects[] = {
      gfx::RectF(10, 20, 30, 40),     gfx::RectF(11, 22, 33, 44),
      gfx::RectF(10.5, 20, 30, 40),   gfx::RectF(10, 20.5, 30, 40),
      gfx::RectF(10, 20, 30.5, 40),   gfx::RectF(10, 20, 30, 40.5),
      gfx::RectF(10.5, 20, 30, 40.5), gfx::RectF(10.5, 20.5, 30, 40)};
  bool kExpectedAligned[] = {true,  true,  false, false,
                             false, false, false, false};
  static_assert(std::size(kTestRects) == std::size(kExpectedAligned),
                "Number of elements in each list should be the identical.");
  for (int j = 0; j < 2; j++) {
    const bool kTestCaseRotation = (j == 1);
    for (size_t i = 0; i < std::size(kTestRects); i++) {
      auto rect_in_dip = device_scale_transform.MapRect(kTestRects[i]);
      sub_surface->SetPosition(rect_in_dip.origin());
      child_surface->SetViewport(rect_in_dip.size());
      const int kChildBufferScale = 2;
      child_surface->SetBufferScale(kChildBufferScale);
      if (kTestCaseRotation) {
        child_surface->SetBufferTransform(Transform::ROTATE_90);
      }
      child_surface->Commit();
      surface->Commit();
      test::WaitForLastFrameAck(shell_surface.get());

      const viz::CompositorFrame& frame =
          GetFrameFromSurface(shell_surface.get());
      ASSERT_EQ(1u, frame.render_pass_list.size());
      const auto& quad_list = frame.render_pass_list[0]->quad_list;
      ASSERT_EQ(2u, quad_list.size());
      auto transform =
          quad_list.front()->shared_quad_state->quad_to_target_transform;
      auto rect = transform.MapRect(gfx::RectF(quad_list.front()->rect));
      if (kExpectedAligned[i] && !kTestCaseRotation) {
        // A transformed rect cannot express a rotation.
        // Manipulation of texture coordinates, in addition to a transformed
        // rect, can represent flip/mirror but only as two uv points and not as
        // a uv rect.
        auto* tex_draw_quad =
            viz::TextureDrawQuad::MaterialCast(quad_list.front());
        EXPECT_POINTF_NEAR(tex_draw_quad->uv_top_left, gfx::PointF(0, 0),
                           0.001f);
        EXPECT_POINTF_NEAR(tex_draw_quad->uv_bottom_right, gfx::PointF(1, 1),
                           0.001f);
        EXPECT_EQ(gfx::Transform(), transform);
        EXPECT_EQ(kTestRects[i], rect);
      } else {
        EXPECT_EQ(gfx::Rect(1, 1), quad_list.front()->rect);
        // Subpixel quads have non identity transforms and due to floating point
        // math can only be approximately compared.
        EXPECT_NEAR(kTestRects[i].x(), rect.x(), 0.001f);
        EXPECT_NEAR(kTestRects[i].y(), rect.y(), 0.001f);
        EXPECT_NEAR(kTestRects[i].width(), rect.width(), 0.001f);
        EXPECT_NEAR(kTestRects[i].height(), rect.height(), 0.001f);
      }
    }
  }
}

TEST_P(SurfaceTest, SetCrop) {
  gfx::Size buffer_size(16, 16);
  auto buffer = test::ExoTestHelper::CreateBuffer(buffer_size);
  auto surface = std::make_unique<Surface>();
  auto shell_surface = std::make_unique<ShellSurface>(surface.get());

  surface->Attach(buffer.get());
  gfx::Size crop_size(12, 12);
  surface->SetCrop(gfx::RectF(gfx::PointF(2.0, 2.0), gfx::SizeF(crop_size)));
  surface->Commit();
  EXPECT_EQ(crop_size.ToString(),
            surface->window()->bounds().size().ToString());
  EXPECT_EQ(gfx::SizeF(crop_size).ToString(),
            surface->content_size().ToString());

  test::WaitForLastFrameAck(shell_surface.get());

  const viz::CompositorFrame& frame = GetFrameFromSurface(shell_surface.get());
  ASSERT_EQ(1u, frame.render_pass_list.size());
  EXPECT_EQ(ToPixel(gfx::Rect(0, 0, 12, 12)), GetCompleteDamage(frame));

  // This will make the surface have no content regardless of the crop.
  surface->Attach(nullptr);
  surface->Commit();
  EXPECT_TRUE(surface->content_size().IsEmpty());
}

void SurfaceTest::SetCropAndBufferTransformHelperTransformAndTest(
    Surface* surface,
    ShellSurface* shell_surface,
    Transform transform,
    const gfx::RectF& expected_rect,
    bool has_viewport) {
  const gfx::Rect target_with_no_viewport(ToPixel(gfx::Rect(gfx::Size(52, 4))));
  const gfx::Rect target_with_viewport(ToPixel(gfx::Rect(gfx::Size(128, 64))));

  std::stringstream scoped_trace_message;
  scoped_trace_message << "SetCropAndBufferTransformHelperTransformAndTest("
                       << "transform=" << TransformToString(transform)
                       << ", has_viewport="
                       << ((has_viewport) ? "true" : "false") << ")";
  SCOPED_TRACE(scoped_trace_message.str());

  surface->SetBufferTransform(transform);
  surface->Commit();

  test::WaitForLastFrameAck(shell_surface);

  {
    const viz::CompositorFrame& frame = GetFrameFromSurface(shell_surface);
    ASSERT_EQ(1u, frame.render_pass_list.size());
    const viz::QuadList& quad_list = frame.render_pass_list[0]->quad_list;
    ASSERT_EQ(1u, quad_list.size());
    const viz::TextureDrawQuad* quad =
        viz::TextureDrawQuad::MaterialCast(quad_list.front());
    EXPECT_EQ(expected_rect.origin(), quad->uv_top_left);
    EXPECT_EQ(expected_rect.bottom_right(), quad->uv_bottom_right);
    EXPECT_EQ(
        (has_viewport) ? target_with_viewport : target_with_no_viewport,
        cc::MathUtil::MapEnclosingClippedRect(
            quad->shared_quad_state->quad_to_target_transform, quad->rect));
  }
}

// Disabled due to flakiness: crbug.com/856145
#if defined(LEAK_SANITIZER)
#define MAYBE_SetCropAndBufferTransform DISABLED_SetCropAndBufferTransform
#else
#define MAYBE_SetCropAndBufferTransform SetCropAndBufferTransform
#endif
TEST_P(SurfaceTest, MAYBE_SetCropAndBufferTransform) {
  gfx::Size buffer_size(128, 64);
  auto buffer = test::ExoTestHelper::CreateBuffer(buffer_size);
  auto surface = std::make_unique<Surface>();
  auto shell_surface = std::make_unique<ShellSurface>(surface.get());

  surface->Attach(buffer.get());
  gfx::Size crop_size(52, 4);
  gfx::Point crop_origin(4, 12);

  gfx::Rect crop_rect(crop_origin, crop_size);

  // These rects represent the left, right, top, bottom values of the crop rect
  // normalized from the buffer size for each transformation.
  static constexpr SkRect crop_0 =
      SkRect::MakeLTRB(0.03125f, 0.1875f, 0.4375f, 0.25f);
  static constexpr SkRect crop_90 =
      SkRect::MakeLTRB(0.875f, 0.0625f, 0.90625f, 0.875f);
  static constexpr SkRect crop_180 =
      SkRect::MakeLTRB(0.5625f, 0.75f, 0.96875f, 0.8125f);
  static constexpr SkRect crop_270 =
      SkRect::MakeLTRB(0.09375f, 0.125f, 0.125f, 0.9375f);
  static constexpr SkRect flipped_crop_0 =
      SkRect::MakeLTRB(0.5625f, 0.1875f, 0.96875f, 0.25f);
  static constexpr SkRect flipped_crop_90 =
      SkRect::MakeLTRB(0.09375f, 0.0625f, 0.125f, 0.875f);
  static constexpr SkRect flipped_crop_180 =
      SkRect::MakeLTRB(0.03125f, 0.75f, 0.4375f, 0.8125f);
  static constexpr SkRect flipped_crop_270 =
      SkRect::MakeLTRB(0.875f, 0.125f, 0.90625f, 0.9375f);

  surface->SetCrop(gfx::RectF(gfx::PointF(crop_origin), gfx::SizeF(crop_size)));

  struct TransformTestcase {
    Transform transform;
    const raw_ref<const SkRect> expected_rect;

    constexpr TransformTestcase(Transform transform_in,
                                const SkRect& expected_rect_in)
        : transform(transform_in), expected_rect(expected_rect_in) {}
  };

  constexpr std::array<TransformTestcase, 8> testcases{
      TransformTestcase(Transform::NORMAL, crop_0),
      TransformTestcase(Transform::ROTATE_90, crop_90),
      TransformTestcase(Transform::ROTATE_180, crop_180),
      TransformTestcase(Transform::ROTATE_270, crop_270),
      TransformTestcase(Transform::FLIPPED, flipped_crop_0),
      TransformTestcase(Transform::FLIPPED_ROTATE_90, flipped_crop_90),
      TransformTestcase(Transform::FLIPPED_ROTATE_180, flipped_crop_180),
      TransformTestcase(Transform::FLIPPED_ROTATE_270, flipped_crop_270)};

  for (const auto& tc : testcases) {
    SetCropAndBufferTransformHelperTransformAndTest(
        surface.get(), shell_surface.get(), tc.transform,
        gfx::SkRectToRectF(*tc.expected_rect), false);
  }

  surface->SetViewport(gfx::SizeF(128, 64));

  for (const auto& tc : testcases) {
    SetCropAndBufferTransformHelperTransformAndTest(
        surface.get(), shell_surface.get(), tc.transform,
        gfx::SkRectToRectF(*tc.expected_rect), true);
  }
}

TEST_P(SurfaceTest, SetBlendMode) {
  gfx::Size buffer_size(1, 1);
  auto buffer = test::ExoTestHelper::CreateBuffer(buffer_size);
  auto surface = std::make_unique<Surface>();
  auto shell_surface = std::make_unique<ShellSurface>(surface.get());

  surface->Attach(buffer.get());
  surface->SetBlendMode(SkBlendMode::kSrc);
  surface->Commit();
  test::WaitForLastFrameAck(shell_surface.get());

  const viz::CompositorFrame& frame = GetFrameFromSurface(shell_surface.get());
  ASSERT_EQ(1u, frame.render_pass_list.size());
  ASSERT_EQ(1u, frame.render_pass_list.back()->quad_list.size());
  EXPECT_FALSE(frame.render_pass_list.back()
                   ->quad_list.back()
                   ->ShouldDrawWithBlending());
}

TEST_P(SurfaceTest, OverlayCandidate) {
  gfx::Size buffer_size(1, 1);
  auto buffer = test::ExoTestHelper::CreateBuffer(
      buffer_size, gfx::BufferFormat::RGBA_8888,
      /*is_overlay_candidate=*/true);
  auto surface = std::make_unique<Surface>();
  auto shell_surface = std::make_unique<ShellSurface>(surface.get());

  surface->Attach(buffer.get());
  surface->Commit();
  test::WaitForLastFrameAck(shell_surface.get());

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
  auto buffer = test::ExoTestHelper::CreateBuffer(
      buffer_size, gfx::BufferFormat::RGBA_8888,
      /*is_overlay_candidate=*/true);
  auto surface = std::make_unique<Surface>();
  auto shell_surface = std::make_unique<ShellSurface>(surface.get());

  {
    surface->Attach(buffer.get());
    surface->SetAlpha(0.5f);
    surface->Commit();
    test::WaitForLastFrameAck(shell_surface.get());

    const viz::CompositorFrame& frame =
        GetFrameFromSurface(shell_surface.get());
    ASSERT_EQ(1u, frame.render_pass_list.size());
    ASSERT_EQ(1u, frame.render_pass_list.back()->quad_list.size());
    ASSERT_EQ(1u, frame.resource_list.size());
    ASSERT_EQ(viz::ResourceId(1u), frame.resource_list.back().id);
    EXPECT_EQ(gfx::Rect(buffer_size), ToTargetSpaceDamage(frame));
  }

  {
    surface->SetAlpha(0.f);
    surface->Commit();
    test::WaitForLastFrameAck(shell_surface.get());

    const viz::CompositorFrame& frame =
        GetFrameFromSurface(shell_surface.get());
    ASSERT_EQ(1u, frame.render_pass_list.size());
    // We always need to submit surface resources because we have created shared
    // images that have release callbacks that will only fire when releasing a
    // compositor frame.
    ASSERT_EQ(1u, frame.resource_list.size());
    ASSERT_EQ(0u, frame.render_pass_list.back()->quad_list.size());
    EXPECT_EQ(gfx::Rect(buffer_size), ToTargetSpaceDamage(frame));
  }

  {
    surface->SetAlpha(1.f);
    surface->Commit();
    test::WaitForLastFrameAck(shell_surface.get());

    const viz::CompositorFrame& frame =
        GetFrameFromSurface(shell_surface.get());
    ASSERT_EQ(1u, frame.render_pass_list.size());
    ASSERT_EQ(1u, frame.render_pass_list.back()->quad_list.size());
    ASSERT_EQ(1u, frame.resource_list.size());
    // The resource should be updated again, the id should be changed.
    ASSERT_EQ(viz::ResourceId(2u), frame.resource_list.back().id);
    EXPECT_EQ(gfx::Rect(buffer_size), ToTargetSpaceDamage(frame));
  }
}

TEST_P(SurfaceTest, ForceRgbxTest) {
  gfx::Size buffer_size(1, 1);
  auto buffer = test::ExoTestHelper::CreateBuffer(
      buffer_size, gfx::BufferFormat::RGBA_8888,
      /*is_overlay_candidate=*/true);
  auto surface = std::make_unique<Surface>();
  auto shell_surface = std::make_unique<ShellSurface>(surface.get());

  {
    surface->Attach(buffer.get());
    // Blend mode 'kSrc' will result in an opaque surface.
    surface->SetBlendMode(SkBlendMode::kSrc);
    surface->Commit();
    test::WaitForLastFrameAck(shell_surface.get());

    const viz::CompositorFrame& frame =
        GetFrameFromSurface(shell_surface.get());
    ASSERT_EQ(1u, frame.render_pass_list.size());
    ASSERT_EQ(1u, frame.render_pass_list.back()->quad_list.size());
    ASSERT_EQ(1u, frame.resource_list.size());
    ASSERT_EQ(viz::ResourceId(1u), frame.resource_list.back().id);
    EXPECT_EQ(gfx::Rect(buffer_size), ToTargetSpaceDamage(frame));
    auto& quad_list = frame.render_pass_list.back()->quad_list;
    auto* texture_quad = quad_list.front()->DynamicCast<viz::TextureDrawQuad>();
    ASSERT_TRUE(texture_quad);
    ASSERT_TRUE(texture_quad->force_rgbx);
  }
}

TEST_P(SurfaceTest, ForceRgbxTestNoBufferAlpha) {
  gfx::Size buffer_size(1, 1);
  auto buffer = test::ExoTestHelper::CreateBuffer(
      buffer_size, gfx::BufferFormat::RGBX_8888,
      /*is_overlay_candidate=*/true);
  auto surface = std::make_unique<Surface>();
  auto shell_surface = std::make_unique<ShellSurface>(surface.get());

  {
    surface->Attach(buffer.get());
    // Blend mode 'kSrc' will result in an opaque surface.
    surface->SetBlendMode(SkBlendMode::kSrc);
    surface->Commit();
    test::WaitForLastFrameAck(shell_surface.get());

    const viz::CompositorFrame& frame =
        GetFrameFromSurface(shell_surface.get());
    ASSERT_EQ(1u, frame.render_pass_list.size());
    ASSERT_EQ(1u, frame.render_pass_list.back()->quad_list.size());
    ASSERT_EQ(1u, frame.resource_list.size());
    ASSERT_EQ(viz::ResourceId(1u), frame.resource_list.back().id);
    EXPECT_EQ(gfx::Rect(buffer_size), ToTargetSpaceDamage(frame));
    auto& quad_list = frame.render_pass_list.back()->quad_list;
    auto* texture_quad = quad_list.front()->DynamicCast<viz::TextureDrawQuad>();
    ASSERT_TRUE(texture_quad);
    ASSERT_FALSE(texture_quad->force_rgbx);
  }
}

TEST_P(SurfaceTest, ColorBufferAlpha) {
  gfx::Size buffer_size(1, 1);
  constexpr SkColor4f kBuffColorExpected[] = {{1.f, 128.0f / 255.0f, 0.f, 1.f},
                                              {0.f, 128.0f / 255.0f, 1.f, 0.f}};
  constexpr bool kExpectedOpaque[] = {true, false};
  for (size_t i = 0; i < std::size(kBuffColorExpected); i++) {
    auto buffer =
        std::make_unique<SolidColorBuffer>(kBuffColorExpected[i], buffer_size);
    auto surface = std::make_unique<Surface>();
    auto shell_surface = std::make_unique<ShellSurface>(surface.get());
    surface->Attach(buffer.get());
    surface->SetAlpha(1.0f);

    {
      surface->Commit();
      test::WaitForLastFrameAck(shell_surface.get());

      const viz::CompositorFrame& frame =
          GetFrameFromSurface(shell_surface.get());
      EXPECT_EQ(1u, frame.render_pass_list.size());
      EXPECT_EQ(1u, frame.render_pass_list.back()->quad_list.size());
      EXPECT_EQ(0u, frame.resource_list.size());
      auto* draw_quad = frame.render_pass_list.back()->quad_list.back();
      EXPECT_EQ(viz::DrawQuad::Material::kSolidColor, draw_quad->material);
      EXPECT_EQ(kExpectedOpaque[i],
                draw_quad->shared_quad_state->are_contents_opaque);
      auto* solid_color_quad = viz::SolidColorDrawQuad::MaterialCast(draw_quad);
      EXPECT_EQ(kBuffColorExpected[i], solid_color_quad->color);
    }
  }
}

TEST_P(SurfaceTest, Commit) {
  std::unique_ptr<Surface> surface(new Surface);

  // Calling commit without a buffer should succeed.
  surface->Commit();
}

TEST_P(SurfaceTest, RemoveSubSurface) {
  gfx::Size buffer_size(256, 256);
  auto buffer = test::ExoTestHelper::CreateBuffer(buffer_size);
  std::unique_ptr<Surface> surface(new Surface);
  auto shell_surface = std::make_unique<ShellSurface>(surface.get());
  surface->Attach(buffer.get());

  // Create a subsurface:
  gfx::Size child_buffer_size(64, 128);
  auto child_buffer = test::ExoTestHelper::CreateBuffer(child_buffer_size);
  auto child_surface = std::make_unique<Surface>();
  auto sub_surface =
      std::make_unique<SubSurface>(child_surface.get(), surface.get());
  sub_surface->SetPosition(gfx::PointF(20, 10));
  child_surface->Attach(child_buffer.get());
  child_surface->Commit();
  surface->Commit();
  test::WaitForLastFrameAck(shell_surface.get());

  // Remove the subsurface by destroying it. This should not damage |surface|.
  // TODO(penghuang): Make the damage more precise for sub surface changes.
  // https://crbug.com/779704
  sub_surface.reset();
  EXPECT_FALSE(surface->HasPendingDamageForTesting(gfx::Rect(20, 10, 64, 128)));
}

TEST_P(SurfaceTest, DestroyAttachedBuffer) {
  gfx::Size buffer_size(1, 1);
  auto buffer = test::ExoTestHelper::CreateBuffer(buffer_size);
  auto surface = std::make_unique<Surface>();
  auto shell_surface = std::make_unique<ShellSurface>(surface.get());

  surface->Attach(buffer.get());
  surface->Commit();
  test::WaitForLastFrameAck(shell_surface.get());

  // Make sure surface size is still valid after buffer is destroyed.
  buffer.reset();
  surface->Commit();
  EXPECT_FALSE(surface->content_size().IsEmpty());
}

TEST_P(SurfaceTest, SetClientSurfaceId) {
  auto surface = std::make_unique<Surface>();
  const std::string kTestId = "42";

  surface->SetClientSurfaceId(kTestId.c_str());
  EXPECT_EQ(kTestId, surface->GetClientSurfaceId());
}

TEST_P(SurfaceTest, DestroyWithAttachedBufferReleasesBuffer) {
  gfx::Size buffer_size(1, 1);
  auto buffer = test::ExoTestHelper::CreateBuffer(buffer_size);
  auto surface = std::make_unique<Surface>();
  auto shell_surface = std::make_unique<ShellSurface>(surface.get());

  int release_buffer_call_count = 0;
  base::RunLoop run_loop;
  buffer->set_release_callback(test::CreateReleaseBufferClosure(
      &release_buffer_call_count, run_loop.QuitClosure()));

  surface->Attach(buffer.get());
  surface->Commit();
  test::WaitForLastFrameAck(shell_surface.get());

  // Buffer is still attached at this point.
  EXPECT_EQ(0, release_buffer_call_count);

  // After the surface is destroyed, we should get a release event for the
  // attached buffer.
  shell_surface.reset();
  surface.reset();
  run_loop.Run();
  ASSERT_EQ(1, release_buffer_call_count);
}

TEST_P(SurfaceTest, AcquireFence) {
  auto buffer = test::ExoTestHelper::CreateBuffer(gfx::Size(1, 1));
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
  auto buffer = test::ExoTestHelper::CreateBuffer(buffer_size);
  auto surface = std::make_unique<Surface>();
  auto shell_surface = std::make_unique<ShellSurface>(surface.get());
  surface->Attach(buffer.get());
  surface->Commit();

  gfx::Size child_buffer_size(64, 128);
  auto child_buffer = test::ExoTestHelper::CreateBuffer(child_buffer_size);
  auto child_surface = std::make_unique<Surface>();
  auto sub_surface =
      std::make_unique<SubSurface>(child_surface.get(), surface.get());
  child_surface->Attach(child_buffer.get());
  // Turn on occlusion tracking.
  child_surface->SetOcclusionTracking(true);
  child_surface->Commit();
  surface->Commit();

  SurfaceObserverForTest observer(
      child_surface.get()->window()->GetOcclusionState());
  ScopedSurface scoped_child_surface(child_surface.get(), &observer);

  // Destroy the subsurface and expect to get an occlusion update.
  sub_surface.reset();
  EXPECT_EQ(1, observer.num_occlusion_changes());
  EXPECT_EQ(aura::Window::OcclusionState::HIDDEN,
            child_surface->window()->GetOcclusionState());
}

TEST_P(SurfaceTest, OcclusionNotRecomputedOnWidgetCommit) {
  constexpr gfx::Size kBufferSize(32, 32);
  auto shell_surface =
      test::ShellSurfaceBuilder(kBufferSize).BuildShellSurface();
  auto* surface = shell_surface->root_surface();

  // Turn on occlusion tracking.
  surface->SetOcclusionTracking(true);
  surface->Commit();

  // Commit the surface with no changes and expect not to get an occlusion
  // update.
  aura::test::WindowOcclusionTrackerTestApi window_occlusion_tracker_test_api(
      aura::Env::GetInstance()->GetWindowOcclusionTracker());
  const int num_times_occlusion_recomputed =
      window_occlusion_tracker_test_api.GetNumTimesOcclusionRecomputed();
  surface->Commit();
  EXPECT_EQ(num_times_occlusion_recomputed,
            window_occlusion_tracker_test_api.GetNumTimesOcclusionRecomputed());

  // Set a non-null alpha shape and make sure occlusion is recomputed.
  shell_surface->SetShape(cc::Region(gfx::Rect(0, 0, 24, 24)));
  surface->Commit();
  EXPECT_EQ(num_times_occlusion_recomputed + 1,
            window_occlusion_tracker_test_api.GetNumTimesOcclusionRecomputed());
}

TEST_P(SurfaceTest, HasPendingPerCommitBufferReleaseCallback) {
  auto buffer = test::ExoTestHelper::CreateBuffer(gfx::Size(1, 1));
  auto surface = std::make_unique<Surface>();

  // We can only commit a buffer release callback if a buffer is attached.
  surface->Attach(buffer.get());

  EXPECT_FALSE(surface->HasPendingPerCommitBufferReleaseCallback());
  surface->SetPerCommitBufferReleaseCallback(
      base::BindOnce([](gfx::GpuFenceHandle) {}));
  EXPECT_TRUE(surface->HasPendingPerCommitBufferReleaseCallback());
  surface->Commit();
  EXPECT_FALSE(surface->HasPendingPerCommitBufferReleaseCallback());
}

TEST_P(SurfaceTest, PerCommitBufferReleaseCallbackForSameSurface) {
  gfx::Size buffer_size(64, 64);
  auto buffer1 = test::ExoTestHelper::CreateBuffer(buffer_size);
  auto buffer2 = test::ExoTestHelper::CreateBuffer(buffer_size);
  auto surface = std::make_unique<Surface>();
  auto shell_surface = std::make_unique<ShellSurface>(surface.get());
  int per_commit_release_count = 0;

  // Set the release callback that will be run when buffer is no longer in use.
  int buffer_release_count = 0;
  base::RunLoop run_loop1;
  buffer1->set_release_callback(test::CreateReleaseBufferClosure(
      &buffer_release_count, run_loop1.QuitClosure()));

  base::RunLoop run_loop2;
  surface->SetPerCommitBufferReleaseCallback(
      test::CreateExplicitReleaseCallback(&per_commit_release_count,
                                          run_loop2.QuitClosure()));
  surface->Attach(buffer1.get());
  surface->Damage(gfx::Rect(buffer_size));
  surface->Commit();
  test::WaitForLastFramePresentation(shell_surface.get());
  EXPECT_EQ(per_commit_release_count, 0);
  EXPECT_EQ(buffer_release_count, 0);

  // Attaching the same buffer causes the per-commit callback to be emitted.
  surface->SetPerCommitBufferReleaseCallback(
      test::CreateExplicitReleaseCallback(&per_commit_release_count,
                                          base::DoNothing()));
  surface->Attach(buffer1.get());
  surface->Damage(gfx::Rect(buffer_size));
  surface->Commit();
  test::WaitForLastFramePresentation(shell_surface.get());

  run_loop2.Run();
  EXPECT_EQ(per_commit_release_count, 1);
  EXPECT_EQ(buffer_release_count, 0);

  // Attaching a different buffer causes the per-commit callback to be emitted.
  surface->Attach(buffer2.get());
  surface->Damage(gfx::Rect(buffer_size));
  surface->Commit();
  test::WaitForLastFramePresentation(shell_surface.get());

  run_loop1.Run();
  EXPECT_EQ(per_commit_release_count, 2);
  // The buffer should now be completely released.
  EXPECT_EQ(buffer_release_count, 1);
}

TEST_P(SurfaceTest, PerCommitBufferReleaseCallbackForDifferentSurfaces) {
  gfx::Size buffer_size(64, 64);
  auto buffer1 = test::ExoTestHelper::CreateBuffer(buffer_size);
  auto buffer2 = test::ExoTestHelper::CreateBuffer(buffer_size);
  auto surface1 = std::make_unique<Surface>();
  auto shell_surface1 = std::make_unique<ShellSurface>(surface1.get());
  auto surface2 = std::make_unique<Surface>();
  auto shell_surface2 = std::make_unique<ShellSurface>(surface2.get());
  int per_commit_release_count1 = 0;
  int per_commit_release_count2 = 0;

  // Set the release callback that will be run when buffer is no longer in use.
  int buffer_release_count = 0;
  base::RunLoop run_loop1;
  buffer1->set_release_callback(test::CreateReleaseBufferClosure(
      &buffer_release_count, run_loop1.QuitClosure()));

  // Attach buffer1 to both surface1 and surface2.
  base::RunLoop run_loop2;
  surface1->SetPerCommitBufferReleaseCallback(
      test::CreateExplicitReleaseCallback(&per_commit_release_count1,
                                          run_loop2.QuitClosure()));
  surface1->Attach(buffer1.get());
  surface1->Damage(gfx::Rect(buffer_size));
  surface1->Commit();
  surface2->SetPerCommitBufferReleaseCallback(
      test::CreateExplicitReleaseCallback(&per_commit_release_count2,
                                          base::DoNothing()));
  surface2->Attach(buffer1.get());
  surface2->Damage(gfx::Rect(buffer_size));
  surface2->Commit();
  test::WaitForLastFramePresentation(shell_surface2.get());

  EXPECT_EQ(per_commit_release_count1, 0);
  EXPECT_EQ(per_commit_release_count2, 0);
  EXPECT_EQ(buffer_release_count, 0);

  // Attach buffer2 to surface1, only the surface1 callback should be emitted.
  surface1->Attach(buffer2.get());
  surface1->Damage(gfx::Rect(buffer_size));
  surface1->Commit();
  test::WaitForLastFramePresentation(shell_surface1.get());

  run_loop2.Run();
  EXPECT_EQ(per_commit_release_count1, 1);
  EXPECT_EQ(per_commit_release_count2, 0);
  EXPECT_EQ(buffer_release_count, 0);

  // Attach buffer2 to surface2, only the surface2 callback should be emitted.
  surface2->Attach(buffer2.get());
  surface2->Damage(gfx::Rect(buffer_size));
  surface2->Commit();
  test::WaitForLastFramePresentation(shell_surface2.get());

  run_loop1.Run();
  EXPECT_EQ(per_commit_release_count1, 1);
  EXPECT_EQ(per_commit_release_count2, 1);
  // The buffer should now be completely released.
  EXPECT_EQ(buffer_release_count, 1);
}

//
TEST_P(SurfaceTest, SimpleSurfaceGraphicsOcclusion) {
  if (!base::FeatureList::IsEnabled(kExoPerSurfaceOcclusion)) {
    GTEST_SKIP();
  }

  auto canonical_form_check = [](const auto& frame) {
    EXPECT_EQ(1u, frame.render_pass_list.size());
    auto& quad_list = frame.render_pass_list.back()->quad_list;
    bool is_canonical_form = true;
    for (auto it = quad_list.begin(); it != quad_list.end(); ++it) {
      // For this test we assume that a 1x1 quad indicates a AA quad. This
      // assumption is only valid for this test because of our input rects are
      // not 1x1.
      is_canonical_form &= (*it)->rect != gfx::Rect(1, 1);
    }
    return is_canonical_form;
  };

  // This parent is merely the background for our children and plays no role in
  // this test.
  gfx::Size buffer_size(256, 256);
  auto buffer = test::ExoTestHelper::CreateBuffer(buffer_size);
  auto surface = std::make_unique<Surface>();
  auto shell_surface = std::make_unique<ShellSurface>(surface.get());
  surface->Attach(buffer.get());
  surface->SetViewport(gfx::SizeF(13, 13));

  // # Basic occlusion

  // The order of subsurface parent attachment is the inverse order of quad
  // submission so child B comes first.
  auto child_buffer_b = test::ExoTestHelper::CreateBuffer(gfx::Size(64, 64));
  auto child_surface_b = std::make_unique<Surface>();
  auto sub_surface_b =
      std::make_unique<SubSurface>(child_surface_b.get(), surface.get());
  child_surface_b->Attach(child_buffer_b.get());
  sub_surface_b->SetPosition(gfx::PointF(40, 10));
  child_surface_b->SetViewport(gfx::SizeF(20, 10));
  child_surface_b->Commit();

  auto child_buffer_a = test::ExoTestHelper::CreateBuffer(gfx::Size(64, 64));
  auto child_surface_a = std::make_unique<Surface>();
  auto sub_surface_a =
      std::make_unique<SubSurface>(child_surface_a.get(), surface.get());
  child_surface_a->Attach(child_buffer_a.get());
  sub_surface_a->SetPosition(gfx::PointF(40, 10));
  child_surface_a->SetViewport(gfx::SizeF(20, 10));
  child_surface_a->SetBlendMode(SkBlendMode::kSrc);
  child_surface_a->Commit();

  surface->Commit();
  test::WaitForLastFrameAck(shell_surface.get());
  {
    const viz::CompositorFrame& frame =
        GetFrameFromSurface(shell_surface.get());
    bool const is_canonical_form = canonical_form_check(frame);
    auto const kExpectedNumSQSs = is_canonical_form ? 2u : 3u;
    ASSERT_EQ(kExpectedNumSQSs,
              frame.render_pass_list.back()->shared_quad_state_list.size());
  }

  // # Non occlusion location
  sub_surface_a->SetPosition(gfx::PointF(20, 10));
  child_surface_a->SetViewport(gfx::SizeF(20, 10));
  child_surface_a->Commit();

  sub_surface_b->SetPosition(gfx::PointF(30, 10));
  child_surface_b->SetViewport(gfx::SizeF(20, 10));
  child_surface_b->Commit();

  surface->Commit();
  test::WaitForLastFrameAck(shell_surface.get());
  {
    const viz::CompositorFrame& frame =
        GetFrameFromSurface(shell_surface.get());

    auto const kExpectedNumSQSs = 3u;
    ASSERT_EQ(kExpectedNumSQSs,
              frame.render_pass_list.back()->shared_quad_state_list.size());
  }

  // # Non occluding size.
  sub_surface_a->SetPosition(gfx::PointF(20, 10));
  child_surface_a->SetViewport(gfx::SizeF(20, 10));
  child_surface_a->Commit();

  sub_surface_b->SetPosition(gfx::PointF(20, 10));
  child_surface_b->SetViewport(gfx::SizeF(30, 10));
  child_surface_b->Commit();

  surface->Commit();
  test::WaitForLastFrameAck(shell_surface.get());
  {
    const viz::CompositorFrame& frame =
        GetFrameFromSurface(shell_surface.get());

    auto const kExpectedNumSQSs = 3u;
    ASSERT_EQ(kExpectedNumSQSs,
              frame.render_pass_list.back()->shared_quad_state_list.size());
  }

  // # Different occlusion
  sub_surface_a->SetPosition(gfx::PointF(30, 20));
  child_surface_a->SetViewport(gfx::SizeF(30, 15));
  child_surface_a->Commit();

  sub_surface_b->SetPosition(gfx::PointF(30, 20));
  child_surface_b->SetViewport(gfx::SizeF(30, 15));
  child_surface_b->Commit();

  surface->Commit();
  test::WaitForLastFrameAck(shell_surface.get());
  {
    const viz::CompositorFrame& frame =
        GetFrameFromSurface(shell_surface.get());
    bool const is_canonical_form = canonical_form_check(frame);
    auto const kExpectedNumSQSs = is_canonical_form ? 2u : 3u;
    ASSERT_EQ(kExpectedNumSQSs,
              frame.render_pass_list.back()->shared_quad_state_list.size());
  }

  // # Rounded occlusion not matching
  child_surface_a->SetRoundedCorners(gfx::RRectF(gfx::RectF(0, 0, 30, 15), 6.0),
                                     false);
  child_surface_a->Commit();

  child_surface_b->SetRoundedCorners(gfx::RRectF(gfx::RectF(0, 0, 30, 15), 1.0),
                                     false);
  child_surface_b->Commit();

  surface->Commit();
  test::WaitForLastFrameAck(shell_surface.get());
  {
    const viz::CompositorFrame& frame =
        GetFrameFromSurface(shell_surface.get());
    auto const kExpectedNumSQSs = 3u;
    ASSERT_EQ(kExpectedNumSQSs,
              frame.render_pass_list.back()->shared_quad_state_list.size());
  }

  // # Rounded occlusion matching
  child_surface_a->SetRoundedCorners(gfx::RRectF(gfx::RectF(0, 0, 20, 10), 6.0),
                                     false);
  child_surface_a->Commit();

  child_surface_b->SetRoundedCorners(gfx::RRectF(gfx::RectF(0, 0, 20, 10), 6.0),
                                     false);
  child_surface_b->Commit();

  surface->Commit();
  test::WaitForLastFrameAck(shell_surface.get());
  {
    const viz::CompositorFrame& frame =
        GetFrameFromSurface(shell_surface.get());
    bool const is_canonical_form = canonical_form_check(frame);
    auto const kExpectedNumSQSs = is_canonical_form ? 2u : 3u;
    ASSERT_EQ(kExpectedNumSQSs,
              frame.render_pass_list.back()->shared_quad_state_list.size());
  }

  // # Clip rect too small
  child_surface_a->SetClipRect(gfx::RectF(10, 10));
  child_surface_a->Commit();

  child_surface_b->Commit();

  surface->Commit();
  test::WaitForLastFrameAck(shell_surface.get());
  {
    const viz::CompositorFrame& frame =
        GetFrameFromSurface(shell_surface.get());
    auto const kExpectedNumSQSs = 3u;
    ASSERT_EQ(kExpectedNumSQSs,
              frame.render_pass_list.back()->shared_quad_state_list.size());
  }

  //  # Clip rect large enough
  child_surface_a->SetClipRect(gfx::RectF(100, 100));
  child_surface_a->Commit();
  child_surface_b->Commit();
  surface->Commit();
  test::WaitForLastFrameAck(shell_surface.get());
  {
    const viz::CompositorFrame& frame =
        GetFrameFromSurface(shell_surface.get());
    bool const is_canonical_form = canonical_form_check(frame);
    auto const kExpectedNumSQSs = is_canonical_form ? 2u : 3u;
    ASSERT_EQ(kExpectedNumSQSs,
              frame.render_pass_list.back()->shared_quad_state_list.size());
  }
}

// This test makes sure that when we associate 1 or more rect with the same sqs
// we do so only for canonical rects that form a sealed single layer.
TEST_P(SurfaceTest, LayerSharedQuadState) {
  auto canonical_form_check = [](const auto& frame) {
    EXPECT_EQ(1u, frame.render_pass_list.size());
    auto& quad_list = frame.render_pass_list.back()->quad_list;
    bool is_canonical_form = true;
    for (auto it = quad_list.begin(); it != quad_list.end(); ++it) {
      // For this test we assume that a 1x1 quad indicates a AA quad. This
      // assumption is only valid for this test because of our input rects are
      // not 1x1.
      is_canonical_form &= (*it)->rect != gfx::Rect(1, 1);
    }
    return is_canonical_form;
  };

  // This parent is merely the background for our children and plays no role in
  // this test.
  gfx::Size buffer_size(256, 256);
  auto buffer = test::ExoTestHelper::CreateBuffer(buffer_size);
  auto surface = std::make_unique<Surface>();
  auto shell_surface = std::make_unique<ShellSurface>(surface.get());
  surface->Attach(buffer.get());
  surface->SetViewport(gfx::SizeF(13, 13));

  // Test layer joining in x.
  auto child_buffer_a = test::ExoTestHelper::CreateBuffer(gfx::Size(64, 64));
  auto child_surface_a = std::make_unique<Surface>();
  auto sub_surface_a =
      std::make_unique<SubSurface>(child_surface_a.get(), surface.get());
  child_surface_a->Attach(child_buffer_a.get());
  child_surface_a->SetOverlayPriorityHint(OverlayPriority::LOW);
  sub_surface_a->SetPosition(gfx::PointF(20, 10));
  child_surface_a->SetViewport(gfx::SizeF(20, 10));
  child_surface_a->Commit();

  auto child_buffer_b = test::ExoTestHelper::CreateBuffer(gfx::Size(64, 64));
  auto child_surface_b = std::make_unique<Surface>();
  auto sub_surface_b =
      std::make_unique<SubSurface>(child_surface_b.get(), surface.get());
  child_surface_b->Attach(child_buffer_b.get());
  child_surface_b->SetOverlayPriorityHint(OverlayPriority::LOW);
  sub_surface_b->SetPosition(gfx::PointF(40, 10));
  child_surface_b->SetViewport(gfx::SizeF(20, 10));
  child_surface_b->Commit();

  surface->Commit();
  test::WaitForLastFrameAck(shell_surface.get());
  {
    const viz::CompositorFrame& frame =
        GetFrameFromSurface(shell_surface.get());
    bool const is_canonical_form = canonical_form_check(frame);
    auto const kExpectedNumSQSs = is_canonical_form ? 2u : 3u;
    ASSERT_EQ(kExpectedNumSQSs,
              frame.render_pass_list.back()->shared_quad_state_list.size());
  }

  // Test Layer joining in y.
  sub_surface_a->SetPosition(gfx::PointF(20, 10));
  child_surface_a->SetViewport(gfx::SizeF(20, 10));
  child_surface_a->Commit();

  sub_surface_b->SetPosition(gfx::PointF(20, 20));
  child_surface_b->SetViewport(gfx::SizeF(20, 10));
  child_surface_b->Commit();

  surface->Commit();
  test::WaitForLastFrameAck(shell_surface.get());
  {
    const viz::CompositorFrame& frame =
        GetFrameFromSurface(shell_surface.get());
    bool const is_canonical_form = canonical_form_check(frame);
    auto const kExpectedNumSQSs = is_canonical_form ? 2u : 3u;
    ASSERT_EQ(kExpectedNumSQSs,
              frame.render_pass_list.back()->shared_quad_state_list.size());
  }

  // Test Layer joining with overlapping rects but still sealed.
  sub_surface_a->SetPosition(gfx::PointF(20, 10));
  child_surface_a->SetViewport(gfx::SizeF(20, 10));
  child_surface_a->Commit();

  sub_surface_b->SetPosition(gfx::PointF(30, 10));
  child_surface_b->SetViewport(gfx::SizeF(20, 10));
  child_surface_b->Commit();

  surface->Commit();
  test::WaitForLastFrameAck(shell_surface.get());
  {
    const viz::CompositorFrame& frame =
        GetFrameFromSurface(shell_surface.get());
    bool const is_canonical_form = canonical_form_check(frame);
    auto const kExpectedNumSQSs = is_canonical_form ? 2u : 3u;
    ASSERT_EQ(kExpectedNumSQSs,
              frame.render_pass_list.back()->shared_quad_state_list.size());
  }

  // Fail overlapping but not sealed.
  sub_surface_a->SetPosition(gfx::PointF(20, 10));
  child_surface_a->SetViewport(gfx::SizeF(20, 10));
  child_surface_a->Commit();

  sub_surface_b->SetPosition(gfx::PointF(30, 16));
  child_surface_b->SetViewport(gfx::SizeF(20, 10));
  child_surface_b->Commit();

  surface->Commit();
  test::WaitForLastFrameAck(shell_surface.get());
  {
    const viz::CompositorFrame& frame =
        GetFrameFromSurface(shell_surface.get());
    ASSERT_EQ(1u, frame.render_pass_list.size());
    ASSERT_EQ(3u, frame.render_pass_list.back()->shared_quad_state_list.size());
  }

  // Fail non overlapping rects
  sub_surface_a->SetPosition(gfx::PointF(20, 10));
  child_surface_a->SetViewport(gfx::SizeF(20, 10));
  child_surface_a->Commit();

  sub_surface_b->SetPosition(gfx::PointF(42, 10));
  child_surface_b->SetViewport(gfx::SizeF(20, 10));
  child_surface_b->Commit();

  surface->Commit();
  test::WaitForLastFrameAck(shell_surface.get());
  {
    const viz::CompositorFrame& frame =
        GetFrameFromSurface(shell_surface.get());
    ASSERT_EQ(1u, frame.render_pass_list.size());
    ASSERT_EQ(3u, frame.render_pass_list.back()->shared_quad_state_list.size());
  }

  // Let us prove that we can join more than 2 rects by having 3 rects
  // that should form a single layer.
  sub_surface_a->SetPosition(gfx::PointF(20, 10));
  child_surface_a->SetViewport(gfx::SizeF(20, 10));
  child_surface_a->Commit();

  sub_surface_b->SetPosition(gfx::PointF(20, 20));
  child_surface_b->SetViewport(gfx::SizeF(20, 10));
  child_surface_b->Commit();

  auto child_buffer_c = test::ExoTestHelper::CreateBuffer(gfx::Size(64, 64));
  auto child_surface_c = std::make_unique<Surface>();
  auto sub_surface_c =
      std::make_unique<SubSurface>(child_surface_c.get(), surface.get());
  child_surface_c->Attach(child_buffer_c.get());
  sub_surface_c->SetPosition(gfx::PointF(20, 30));
  child_surface_c->SetViewport(gfx::SizeF(20, 10));
  child_surface_c->SetOverlayPriorityHint(OverlayPriority::LOW);
  child_surface_c->Commit();

  surface->Commit();
  test::WaitForLastFrameAck(shell_surface.get());
  {
    const viz::CompositorFrame& frame =
        GetFrameFromSurface(shell_surface.get());
    bool const is_canonical_form = canonical_form_check(frame);
    auto const kExpectedNumSQSs = is_canonical_form ? 2u : 4u;
    ASSERT_EQ(kExpectedNumSQSs,
              frame.render_pass_list.back()->shared_quad_state_list.size());
  }

  // Setting overlay on the middle quad should cause all to get a unique sqs.
  child_surface_a->Commit();
  child_surface_b->SetOverlayPriorityHint(OverlayPriority::REGULAR);
  child_surface_b->Commit();
  child_surface_c->Commit();
  surface->Commit();
  test::WaitForLastFrameAck(shell_surface.get());
  {
    const viz::CompositorFrame& frame =
        GetFrameFromSurface(shell_surface.get());
    auto const kExpectedNumSQSs = 4u;
    ASSERT_EQ(kExpectedNumSQSs,
              frame.render_pass_list.back()->shared_quad_state_list.size());
  }

  // Setting overlay on the first quad should cause quad b and quad c to still
  // use the same sqs.
  child_surface_a->SetOverlayPriorityHint(OverlayPriority::REGULAR);
  child_surface_a->Commit();
  child_surface_b->SetOverlayPriorityHint(OverlayPriority::LOW);
  child_surface_b->Commit();
  child_surface_c->Commit();
  surface->Commit();
  test::WaitForLastFrameAck(shell_surface.get());
  {
    const viz::CompositorFrame& frame =
        GetFrameFromSurface(shell_surface.get());
    bool const is_canonical_form = canonical_form_check(frame);
    auto const kExpectedNumSQSs = is_canonical_form ? 3u : 4u;
    ASSERT_EQ(kExpectedNumSQSs,
              frame.render_pass_list.back()->shared_quad_state_list.size());
  }
}

// Tests that only apply if ExoReactiveFrameSubmission is enabled.
class ReactiveFrameSubmissionSurfaceTest : public SurfaceTest {
 public:
  ReactiveFrameSubmissionSurfaceTest() {
    DCHECK_EQ(GetFrameSubmissionType(), test::FrameSubmissionType::kReactive);
  }

  ReactiveFrameSubmissionSurfaceTest(
      const ReactiveFrameSubmissionSurfaceTest&) = delete;
  ReactiveFrameSubmissionSurfaceTest& operator=(
      const ReactiveFrameSubmissionSurfaceTest&) = delete;

  ~ReactiveFrameSubmissionSurfaceTest() override = default;
};

// Instantiate the values of frame submission types and device scale factor in
// the parameterized tests.
INSTANTIATE_TEST_SUITE_P(
    All,
    ReactiveFrameSubmissionSurfaceTest,
    testing::Combine(testing::Values(test::FrameSubmissionType::kReactive),
                     testing::Values(1.0f, 1.25f, 2.0f)));

TEST_P(ReactiveFrameSubmissionSurfaceTest, FullDamageAfterDiscardingFrame) {
  gfx::Size buffer_size(256, 256);
  auto buffer = test::ExoTestHelper::CreateBuffer(buffer_size);
  std::unique_ptr<Surface> surface(new Surface);
  auto shell_surface = std::make_unique<ShellSurface>(surface.get());

  surface->Attach(buffer.get());

  shell_surface->layer_tree_frame_sink_holder()
      ->ClearPendingBeginFramesForTesting();

  // This will result in a cached frame in LayerTreeFrameSinkHolder.
  // Do the action twice is necessary when AutoNeedsBeginFrame is enabled,
  // because the first commit will be an unsolicited frame submission and
  // therefore not cached.
  for (int i = 0; i < 2; ++i) {
    surface->Damage(gfx::Rect(10, 10, 10, 10));
    surface->Commit();
  }

  // Commit a frame without any damage. It will cause the previously cached
  // frame to be discarded.
  // It is expected that the damage area of the new frame is expanded to full
  // damage.
  surface->Commit();
  test::WaitForLastFrameAck(shell_surface.get());

  {
    const viz::CompositorFrame& frame =
        GetFrameFromSurface(shell_surface.get());
    EXPECT_EQ(ToPixel(gfx::Rect(buffer_size)), GetCompleteDamage(frame));
  }
}

}  // namespace
}  // namespace exo
