// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/viz/service/display/surface_aggregator.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/format_macros.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "cc/base/math_util.h"
#include "cc/test/render_pass_test_utils.h"
#include "components/viz/common/features.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/quads/aggregated_render_pass.h"
#include "components/viz/common/quads/aggregated_render_pass_draw_quad.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/quads/compositor_render_pass.h"
#include "components/viz/common/quads/compositor_render_pass_draw_quad.h"
#include "components/viz/common/quads/draw_quad.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/surface_draw_quad.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/resources/resource_id.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "components/viz/common/surfaces/subtree_capture_id.h"
#include "components/viz/service/display/aggregated_frame.h"
#include "components/viz/service/display/display_resource_provider_software.h"
#include "components/viz/service/display_embedder/server_shared_bitmap_manager.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "components/viz/service/surfaces/pending_copy_output_request.h"
#include "components/viz/service/surfaces/surface.h"
#include "components/viz/service/surfaces/surface_manager.h"
#include "components/viz/test/begin_frame_args_test.h"
#include "components/viz/test/compositor_frame_helpers.h"
#include "components/viz/test/draw_quad_matchers.h"
#include "components/viz/test/fake_compositor_frame_sink_client.h"
#include "components/viz/test/fake_surface_observer.h"
#include "components/viz/test/stub_surface_client.h"
#include "components/viz/test/test_surface_id_allocator.h"
#include "gpu/command_buffer/service/scheduler.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/video_types.h"

namespace viz {
namespace {

using ::testing::_;
using ::testing::ElementsAre;

constexpr FrameSinkId kArbitraryRootFrameSinkId(1, 1);
constexpr FrameSinkId kArbitraryFrameSinkId1(2, 2);
constexpr FrameSinkId kArbitraryFrameSinkId2(3, 3);
constexpr FrameSinkId kArbitraryMiddleFrameSinkId(4, 4);
constexpr FrameSinkId kArbitraryReservedFrameSinkId(5, 5);
constexpr FrameSinkId kArbitraryFrameSinkId3(6, 6);
constexpr FrameSinkId kArbitraryFrameSinkId4(7, 7);
constexpr FrameSinkId kArbitraryFrameSinkId5(8, 8);

constexpr gfx::Size kSurfaceSize(100, 100);
constexpr gfx::Rect kEmptyDamage(0, 0);

class MockAggregatedDamageCallback {
 public:
  MockAggregatedDamageCallback() = default;

  MockAggregatedDamageCallback(const MockAggregatedDamageCallback&) = delete;
  MockAggregatedDamageCallback& operator=(const MockAggregatedDamageCallback&) =
      delete;

  ~MockAggregatedDamageCallback() = default;

  CompositorFrameSinkSupport::AggregatedDamageCallback GetCallback() {
    return base::BindRepeating(
        &MockAggregatedDamageCallback::OnAggregatedDamage,
        weak_ptr_factory_.GetWeakPtr());
  }

  MOCK_METHOD4(OnAggregatedDamage,
               void(const LocalSurfaceId& local_surface_id,
                    const gfx::Size& frame_size_in_pixels,
                    const gfx::Rect& damage_rect,
                    base::TimeTicks expected_display_time));

 private:
  base::WeakPtrFactory<MockAggregatedDamageCallback> weak_ptr_factory_{this};
};

class DisplayTimeSource {
 public:
  base::TimeTicks next_display_time() const { return next_display_time_; }

  base::TimeTicks GetNextDisplayTimeAndIncrement() {
    const base::TimeTicks display_time = next_display_time_;
    next_display_time_ += BeginFrameArgs::DefaultInterval();
    return display_time;
  }

 private:
  base::TimeTicks next_display_time_ = base::TimeTicks() + base::Seconds(1);
};

}  // namespace

class SurfaceAggregatorTest : public testing::Test, public DisplayTimeSource {
 public:
  explicit SurfaceAggregatorTest(
      SurfaceAggregator::ExtraPassForReadbackOption extra_pass_option,
      bool prevent_merging_surfaces_to_root_pass)
      : root_sink_(std::make_unique<CompositorFrameSinkSupport>(
            &fake_client_,
            &manager_,
            kArbitraryRootFrameSinkId,
            /*is_root=*/true)),
        aggregator_(manager_.surface_manager(),
                    &resource_provider_,
                    true,
                    extra_pass_option,
                    prevent_merging_surfaces_to_root_pass) {
  }

  SurfaceAggregatorTest()
      : SurfaceAggregatorTest(
            SurfaceAggregator::ExtraPassForReadbackOption::kNone,
            /*prevent_merging_surfaces_to_root_pass=*/false) {}

  void TearDown() override {
    observer_.Reset();
    testing::Test::TearDown();
  }

  AggregatedFrame AggregateFrame(const SurfaceId& surface_id,
                                 gfx::Rect target_damage = gfx::Rect()) {
    AggregatedFrame aggregated_frame = aggregator_.Aggregate(
        surface_id, GetNextDisplayTimeAndIncrement(),
        /*display_transform=*/gfx::OVERLAY_TRANSFORM_NONE, target_damage);

    // Ensure no duplicate pass ids output.
    std::set<AggregatedRenderPassId> used_passes;
    for (const auto& pass : aggregated_frame.render_pass_list)
      EXPECT_TRUE(used_passes.insert(pass->id).second);

    return aggregated_frame;
  }

  struct Quad {
    static Quad SolidColorQuad(SkColor4f color, const gfx::Rect& rect) {
      Quad quad;
      quad.material = DrawQuad::Material::kSolidColor;
      quad.color = color;
      quad.rect = rect;
      return quad;
    }

    static Quad TransparentSolidColorQuad(SkColor4f color,
                                          const gfx::Rect& rect,
                                          float opacity) {
      Quad quad;
      quad.material = DrawQuad::Material::kSolidColor;
      quad.color = color;
      quad.rect = rect;
      quad.opacity = opacity;
      return quad;
    }

    static Quad TextureQuad(const gfx::Rect& rect,
                            bool per_quad_damage_output = false) {
      Quad quad;
      quad.material = DrawQuad::Material::kTextureContent;
      quad.rect = rect;
      quad.per_quad_damage_output = per_quad_damage_output;
      return quad;
    }

    // If |fallback_surface_id| is a valid surface Id then this will generate
    // two SurfaceDrawQuads.
    static Quad SurfaceQuad(const SurfaceRange& surface_range,
                            SkColor4f default_background_color,
                            const gfx::Rect& primary_surface_rect,
                            bool stretch_content_to_fill_bounds,
                            bool allow_merge = true) {
      Quad quad;
      quad.material = DrawQuad::Material::kSurfaceContent;
      quad.primary_surface_rect = primary_surface_rect;
      quad.surface_range = surface_range;
      quad.default_background_color = default_background_color;
      quad.stretch_content_to_fill_bounds = stretch_content_to_fill_bounds;
      quad.allow_merge = allow_merge;
      return quad;
    }

    static Quad SurfaceQuad(const SurfaceRange& surface_range,
                            SkColor4f default_background_color,
                            const gfx::Rect& primary_surface_rect,
                            float opacity,
                            const gfx::Transform& transform,
                            bool stretch_content_to_fill_bounds,
                            const gfx::MaskFilterInfo& mask_filter_info,
                            bool is_fast_rounded_corner) {
      Quad quad;
      quad.material = DrawQuad::Material::kSurfaceContent;
      quad.primary_surface_rect = primary_surface_rect;
      quad.opacity = opacity;
      quad.to_target_transform = transform;
      quad.surface_range = surface_range;
      quad.default_background_color = default_background_color;
      quad.stretch_content_to_fill_bounds = stretch_content_to_fill_bounds;
      quad.mask_filter_info = mask_filter_info;
      quad.is_fast_rounded_corner = is_fast_rounded_corner;
      return quad;
    }

    static Quad RenderPassQuad(CompositorRenderPassId id,
                               const gfx::Transform& transform,
                               bool intersects_damage_under) {
      Quad quad;
      quad.material = DrawQuad::Material::kCompositorRenderPass;
      quad.render_pass_id = id;
      quad.transform = transform;
      quad.intersects_damage_under = intersects_damage_under;
      return quad;
    }

    DrawQuad::Material material = DrawQuad::Material::kInvalid;

    // Set when material==DrawQuad::Material::kSurfaceContent.
    SurfaceRange surface_range;
    SkColor4f default_background_color;
    bool stretch_content_to_fill_bounds;
    gfx::Rect primary_surface_rect;
    float opacity = 1.0f;
    gfx::Transform to_target_transform;
    gfx::MaskFilterInfo mask_filter_info;
    bool is_fast_rounded_corner = false;
    bool allow_merge = true;
    bool per_quad_damage_output = false;

    // Set when material==DrawQuad::Material::kSolidColor.
    SkColor4f color{SkColors::kWhite};
    gfx::Rect rect;

    // Set when material==DrawQuad::Material::kCompositorRenderPass.
    CompositorRenderPassId render_pass_id;
    gfx::Transform transform;
    bool intersects_damage_under = true;

   private:
    Quad() = default;
  };

  struct Pass {
    Pass(const std::vector<Quad>& quads,
         CompositorRenderPassId id,
         const gfx::Size& size)
        : Pass(quads, id, gfx::Rect(size)) {}
    Pass(const std::vector<Quad>& quads,
         CompositorRenderPassId id,
         const gfx::Rect& output_rect)
        : quads(quads),
          id(id),
          output_rect(output_rect),
          damage_rect(output_rect) {}
    Pass(const std::vector<Quad>& quads, const gfx::Size& size)
        : quads(quads), output_rect(size), damage_rect(size) {}
    Pass(const std::vector<Quad>& quads,
         const gfx::Size& size,
         const gfx::Rect& damage_rect)
        : quads(quads), output_rect(size), damage_rect(damage_rect) {}

    std::vector<Quad> quads;
    CompositorRenderPassId id{1};
    gfx::Rect output_rect;
    gfx::Rect damage_rect;
    bool has_transparent_background = true;
    bool has_damage_from_contributing_content = false;
  };

  // |referenced_surfaces| refers to the SurfaceRanges of all the
  // SurfaceDrawQuads added to the provided |pass|.
  static void AddQuadInPass(const Quad& desc,
                            CompositorRenderPass* pass,
                            std::vector<SurfaceRange>* referenced_surfaces) {
    switch (desc.material) {
      case DrawQuad::Material::kSolidColor:
        cc::AddTransparentQuad(pass, desc.rect, desc.color, desc.opacity);
        break;
      case DrawQuad::Material::kSurfaceContent:
        referenced_surfaces->emplace_back(desc.surface_range);
        AddSurfaceQuad(pass, desc.primary_surface_rect, desc.opacity,
                       desc.to_target_transform, desc.surface_range,
                       desc.default_background_color,
                       desc.stretch_content_to_fill_bounds,
                       desc.mask_filter_info, desc.is_fast_rounded_corner,
                       desc.allow_merge);
        break;
      case DrawQuad::Material::kCompositorRenderPass:
        AddRenderPassQuad(pass, desc.render_pass_id, desc.transform,
                          desc.intersects_damage_under);
        break;
      case DrawQuad::Material::kTextureContent:
        AddTextureDrawQuad(pass, desc.rect, desc.per_quad_damage_output,
                           desc.to_target_transform);
        break;
      default:
        NOTREACHED_IN_MIGRATION();
    }
  }

  static void AddPasses(CompositorRenderPassList* pass_list,
                        const std::vector<Pass>& passes,
                        std::vector<SurfaceRange>* referenced_surfaces) {
    gfx::Transform root_transform;
    for (auto& pass : passes) {
      CompositorRenderPass* test_pass = AddRenderPassWithDamage(
          pass_list, pass.id, pass.output_rect, pass.damage_rect,
          root_transform, cc::FilterOperations());
      test_pass->has_transparent_background = pass.has_transparent_background;
      test_pass->has_damage_from_contributing_content =
          pass.has_damage_from_contributing_content;
      for (const auto& quad : pass.quads)
        AddQuadInPass(quad, test_pass, referenced_surfaces);
    }
  }

  static void TestQuadMatchesExpectations(Quad expected_quad,
                                          const DrawQuad* quad) {
    switch (expected_quad.material) {
      case DrawQuad::Material::kSolidColor: {
        ASSERT_EQ(DrawQuad::Material::kSolidColor, quad->material);

        const auto* solid_color_quad = SolidColorDrawQuad::MaterialCast(quad);

        EXPECT_EQ(expected_quad.color, solid_color_quad->color);
        EXPECT_EQ(expected_quad.rect, solid_color_quad->rect);
        break;
      }
      // Expected RenderPass quad will become AggregatedRenderPass after
      // aggregation.
      case DrawQuad::Material::kCompositorRenderPass: {
        ASSERT_EQ(DrawQuad::Material::kAggregatedRenderPass, quad->material);

        const auto* render_pass_quad =
            AggregatedRenderPassDrawQuad::MaterialCast(quad);

        EXPECT_EQ(
            expected_quad.render_pass_id,
            CompositorRenderPassId{uint64_t{render_pass_quad->render_pass_id}});
        EXPECT_EQ(expected_quad.intersects_damage_under,
                  render_pass_quad->intersects_damage_under);
        break;
      }
      default:
        NOTREACHED_IN_MIGRATION();
        break;
    }
  }

  static void TestPassMatchesExpectations(Pass expected_pass,
                                          const AggregatedRenderPass* pass) {
    ASSERT_EQ(expected_pass.quads.size(), pass->quad_list.size());
    for (auto iter = pass->quad_list.cbegin(); iter != pass->quad_list.cend();
         ++iter) {
      SCOPED_TRACE(base::StringPrintf("Quad number %" PRIuS, iter.index()));
      TestQuadMatchesExpectations(expected_pass.quads[iter.index()], *iter);
    }
  }

  static void TestPassesMatchExpectations(
      const std::vector<Pass>& expected_passes,
      const AggregatedRenderPassList* passes) {
    ASSERT_EQ(expected_passes.size(), passes->size());

    for (size_t i = 0; i < expected_passes.size(); ++i) {
      SCOPED_TRACE(base::StringPrintf("Pass number %" PRIuS, i));
      auto* pass = (*passes)[i].get();
      TestPassMatchesExpectations(expected_passes[i], pass);
    }
  }

 private:
  static void AddSurfaceQuad(CompositorRenderPass* pass,
                             const gfx::Rect& primary_surface_rect,
                             float opacity,
                             const gfx::Transform& transform,
                             const SurfaceRange& surface_range,
                             SkColor4f default_background_color,
                             bool stretch_content_to_fill_bounds,
                             const gfx::MaskFilterInfo& mask_filter_info,
                             bool is_fast_rounded_corner,
                             bool allow_merge) {
    gfx::Transform layer_to_target_transform = transform;
    gfx::Rect layer_bounds(primary_surface_rect);
    gfx::Rect visible_layer_rect(primary_surface_rect);
    bool are_contents_opaque = false;
    SkBlendMode blend_mode = SkBlendMode::kSrcOver;

    auto* shared_quad_state = pass->CreateAndAppendSharedQuadState();
    shared_quad_state->SetAll(layer_to_target_transform, layer_bounds,
                              visible_layer_rect, mask_filter_info,
                              std::nullopt, are_contents_opaque, opacity,
                              blend_mode, /*sorting_context=*/0,
                              /*layer_id=*/0u, is_fast_rounded_corner);

    SurfaceDrawQuad* surface_quad =
        pass->CreateAndAppendDrawQuad<SurfaceDrawQuad>();
    // TODO(crbug.com/40219248): Pass an SkColor4f into this function.
    surface_quad->SetAll(pass->shared_quad_state_list.back(),
                         primary_surface_rect, primary_surface_rect,
                         /*needs_blending=*/true, surface_range,
                         default_background_color,
                         stretch_content_to_fill_bounds,
                         /*is_reflection=*/false, allow_merge);
  }

  static void AddRenderPassQuad(CompositorRenderPass* pass,
                                CompositorRenderPassId render_pass_id,
                                const gfx::Transform& transform,
                                bool intersects_damage_under) {
    gfx::Rect output_rect = gfx::Rect(0, 0, 5, 5);
    auto* shared_state = pass->CreateAndAppendSharedQuadState();
    shared_state->SetAll(transform, output_rect, output_rect,
                         gfx::MaskFilterInfo(), std::nullopt, false, 1,
                         SkBlendMode::kSrcOver, /*sorting_context=*/0,
                         /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    auto* quad = pass->CreateAndAppendDrawQuad<CompositorRenderPassDrawQuad>();
    quad->SetAll(shared_state, output_rect, output_rect,
                 /*needs_blending=*/true, render_pass_id, kInvalidResourceId,
                 gfx::RectF(), gfx::Size(), gfx::Vector2dF(1.0f, 1.0f),
                 gfx::PointF(), gfx::RectF(),
                 /*force_anti_aliasing_off=*/false,
                 /*backdrop_filter_quality=*/1.0f, intersects_damage_under);
  }

  static void AddTextureDrawQuad(CompositorRenderPass* pass,
                                 const gfx::Rect& output_rect,
                                 bool per_quad_damage_output,
                                 const gfx::Transform& transform) {
    auto* shared_state = pass->CreateAndAppendSharedQuadState();
    shared_state->SetAll(transform, output_rect, output_rect,
                         gfx::MaskFilterInfo(), std::nullopt, false, 1,
                         SkBlendMode::kSrcOver, /*sorting_context=*/0,
                         /*layer_id=*/0u, /*fast_rounded_corner=*/false);
    auto* quad = pass->CreateAndAppendDrawQuad<TextureDrawQuad>();
    const gfx::PointF kUVTopLeft(0.1f, 0.2f);
    const gfx::PointF kUVBottomRight(1.0f, 1.0f);
    quad->SetNew(shared_state, output_rect, output_rect,
                 false /*needs_blending*/, ResourceId(1),
                 false /*premultiplied_alpha*/, kUVTopLeft, kUVBottomRight,
                 SkColors::kTransparent, false /*flipped*/,
                 false /*nearest_neighbor*/, false /*secure_output_only*/,
                 gfx::ProtectedVideoType::kClear);

    if (per_quad_damage_output) {
      quad->damage_rect = output_rect;
    }
  }

 protected:
  ServerSharedBitmapManager shared_bitmap_manager_;
  gpu::SharedImageManager shared_image_manager_;
  gpu::SyncPointManager sync_point_manager_;
  gpu::Scheduler gpu_scheduler_{&sync_point_manager_};

  FrameSinkManagerImpl manager_{
      FrameSinkManagerImpl::InitParams(&shared_bitmap_manager_)};
  DisplayResourceProviderSoftware resource_provider_{
      &shared_bitmap_manager_, &shared_image_manager_, &sync_point_manager_,
      &gpu_scheduler_};
  FakeSurfaceObserver observer_{manager_.surface_manager(), false};
  FakeCompositorFrameSinkClient fake_client_;
  std::unique_ptr<CompositorFrameSinkSupport> root_sink_;
  SurfaceAggregator aggregator_;
};

class SurfaceAggregatorValidSurfaceTest : public SurfaceAggregatorTest {
 public:
  SurfaceAggregatorValidSurfaceTest(
      SurfaceAggregator::ExtraPassForReadbackOption extra_pass_option,
      bool prevent_merging_surfaces_to_root_pass)
      : SurfaceAggregatorTest(extra_pass_option,
                              prevent_merging_surfaces_to_root_pass),
        child_sink_(std::make_unique<CompositorFrameSinkSupport>(
            nullptr,
            &manager_,
            kArbitraryReservedFrameSinkId,
            /*is_root=*/false)),
        root_surface_id_(kArbitraryRootFrameSinkId) {
    child_sink_->set_allow_copy_output_requests_for_testing();
  }

  SurfaceAggregatorValidSurfaceTest()
      : SurfaceAggregatorValidSurfaceTest(
            SurfaceAggregator::ExtraPassForReadbackOption::kNone,
            /*prevent_merging_surfaces_to_root_pass=*/false) {}

  void SetUp() override {
    SurfaceAggregatorTest::SetUp();
    root_surface_ =
        manager_.surface_manager()->GetSurfaceForId(root_surface_id_);
  }

  void TearDown() override { SurfaceAggregatorTest::TearDown(); }

  // Verifies that if the |SharedQuadState::quad_layer_rect| can be covered by
  // |DrawQuad::Rect| in the SharedQuadState.
  void VerifyQuadCoverSQS(AggregatedFrame* aggregated_frame) {
    const SharedQuadState* shared_quad_state = nullptr;
    gfx::Rect draw_quad_coverage;
    for (auto& render_pass : aggregated_frame->render_pass_list) {
      for (auto* quad : render_pass->quad_list) {
        if (shared_quad_state != quad->shared_quad_state) {
          if (shared_quad_state)
            EXPECT_EQ(shared_quad_state->quad_layer_rect, draw_quad_coverage);

          shared_quad_state = quad->shared_quad_state;
          draw_quad_coverage = quad->rect;
        }
        draw_quad_coverage.Union(quad->rect);
      }
    }
  }

  void AggregateAndVerify(const std::vector<Pass>& expected_passes,
                          const std::vector<SurfaceId>& expected_surface_ids) {
    auto aggregated_frame = AggregateFrame(root_surface_id_);

    TestPassesMatchExpectations(expected_passes,
                                &aggregated_frame.render_pass_list);
    VerifyQuadCoverSQS(&aggregated_frame);
    VerifyExpectedSurfaceIds(expected_surface_ids);
  }

  void VerifyExpectedSurfaceIds(
      const std::vector<SurfaceId>& expected_surface_ids) {
    EXPECT_THAT(aggregator_.previous_contained_surfaces(),
                testing::UnorderedElementsAreArray(expected_surface_ids));

    EXPECT_EQ(expected_surface_ids.size(),
              aggregator_.previous_contained_frame_sinks().size());
    for (const SurfaceId& surface_id : expected_surface_ids) {
      EXPECT_THAT(
          aggregator_.previous_contained_frame_sinks(),
          testing::Contains(testing::Pair(surface_id.frame_sink_id(),
                                          surface_id.local_surface_id())));
    }
  }

  void SubmitPassListAsFrame(CompositorFrameSinkSupport* support,
                             const LocalSurfaceId& local_surface_id,
                             CompositorRenderPassList* pass_list,
                             std::vector<SurfaceRange> referenced_surfaces,
                             float device_scale_factor) {
    CompositorFrame frame =
        CompositorFrameBuilder()
            .SetRenderPassList(std::move(*pass_list))
            .SetDeviceScaleFactor(device_scale_factor)
            .SetReferencedSurfaces(std::move(referenced_surfaces))
            .Build();
    frame.metadata.content_color_usage = gfx::ContentColorUsage::kHDR;
    pass_list->clear();

    support->SubmitCompositorFrame(local_surface_id, std::move(frame));
  }

  CompositorRenderPassList GenerateRenderPassList(
      const std::vector<Pass>& passes,
      std::vector<SurfaceRange>* referenced_surfaces) {
    CompositorRenderPassList pass_list;
    AddPasses(&pass_list, passes, referenced_surfaces);
    return pass_list;
  }

  void SubmitCompositorFrame(CompositorFrameSinkSupport* support,
                             const std::vector<Pass>& passes,
                             const LocalSurfaceId& local_surface_id,
                             float device_scale_factor) {
    std::vector<SurfaceRange> referenced_surfaces;
    CompositorRenderPassList pass_list =
        GenerateRenderPassList(passes, &referenced_surfaces);
    SubmitPassListAsFrame(support, local_surface_id, &pass_list,
                          std::move(referenced_surfaces), device_scale_factor);
  }

  void QueuePassAsFrame(std::unique_ptr<CompositorRenderPass> pass,
                        const LocalSurfaceId& local_surface_id,
                        float device_scale_factor,
                        CompositorFrameSinkSupport* support) {
    CompositorFrame child_frame = CompositorFrameBuilder()
                                      .AddRenderPass(std::move(pass))
                                      .SetDeviceScaleFactor(device_scale_factor)
                                      .Build();

    support->SubmitCompositorFrame(local_surface_id, std::move(child_frame));
  }

  gfx::Rect DamageListUnion(SurfaceDamageRectList& surface_damage_rect_list) {
    gfx::Rect damage_rect_union;
    for (auto damage_rect : surface_damage_rect_list)
      damage_rect_union.Union(damage_rect);

    return damage_rect_union;
  }

 protected:
  raw_ptr<Surface> root_surface_;
  std::unique_ptr<CompositorFrameSinkSupport> child_sink_;
  TestSurfaceIdAllocator root_surface_id_;
};

// This test is parameterized on a boolean value to allow the
// SurfaceDrawQuad(s) in the test to merge the root render pass of its embedded
// surface to its parent render pass.
class SurfaceAggregatorValidSurfaceWithMergingPassesTest
    : public SurfaceAggregatorValidSurfaceTest,
      public testing::WithParamInterface<bool> {
 public:
  bool AllowMerge() const { return GetParam(); }
};

// Tests that a very simple frame containing only two solid color quads makes it
// through the aggregator correctly.
TEST_F(SurfaceAggregatorValidSurfaceTest, SimpleFrame) {
  CompositorFrame frame =
      CompositorFrameBuilder()
          .AddRenderPass(
              RenderPassBuilder(kSurfaceSize)
                  .AddSolidColorQuad(gfx::Rect(5, 5), SkColors::kRed)
                  .AddSolidColorQuad(gfx::Rect(5, 5), SkColors::kBlue))
          .Build();

  root_sink_->SubmitCompositorFrame(root_surface_id_.local_surface_id(),
                                    std::move(frame));

  // Add a callback for when the surface is damaged.
  MockAggregatedDamageCallback aggregated_damage_callback;
  root_sink_->SetAggregatedDamageCallbackForTesting(
      aggregated_damage_callback.GetCallback());

  // Check that the AggregatedDamageCallback is called with the right arguments.
  EXPECT_CALL(
      aggregated_damage_callback,
      OnAggregatedDamage(root_surface_id_.local_surface_id(), kSurfaceSize,
                         gfx::Rect(kSurfaceSize), next_display_time()));

  auto aggregated_frame = AggregateFrame(root_surface_id_);
  EXPECT_EQ(aggregated_frame.render_pass_list.size(), 1u);

  auto& render_pass = aggregated_frame.render_pass_list[0];
  EXPECT_THAT(render_pass->quad_list,
              ElementsAre(IsSolidColorQuad(SkColors::kRed),
                          IsSolidColorQuad(SkColors::kBlue)));

  VerifyExpectedSurfaceIds({root_surface_id_});
}

// Tests that SharedElement quads are skipped during aggregation.
TEST_F(SurfaceAggregatorValidSurfaceTest, SharedElementQuad) {
  ViewTransitionElementResourceId vt_resource_id(blink::ViewTransitionToken(),
                                                 1);

  CompositorFrame frame =
      CompositorFrameBuilder()
          .AddRenderPass(
              RenderPassBuilder(kSurfaceSize)
                  .AddSolidColorQuad(gfx::Rect(5, 5), SkColors::kRed)
                  .AddSharedElementQuad(gfx::Rect(5, 5), vt_resource_id))
          .Build();

  root_sink_->SubmitCompositorFrame(root_surface_id_.local_surface_id(),
                                    std::move(frame));
  auto aggregated_frame = AggregateFrame(root_surface_id_);
  EXPECT_EQ(aggregated_frame.render_pass_list.size(), 1u);

  auto& render_pass = aggregated_frame.render_pass_list[0];
  EXPECT_THAT(render_pass->quad_list,
              ElementsAre(IsSolidColorQuad(SkColors::kRed)));
}

// Test that when surface is translucent and we need the render surface to apply
// the opacity, we would keep the render surface.
TEST_F(SurfaceAggregatorValidSurfaceTest, OpacityCopied) {
  auto embedded_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryFrameSinkId1, /*is_root=*/true);
  TestSurfaceIdAllocator embedded_surface_id(embedded_support->frame_sink_id());

  {
    CompositorFrame frame =
        CompositorFrameBuilder()
            .AddRenderPass(
                RenderPassBuilder(CompositorRenderPassId{1},
                                  gfx::Rect(kSurfaceSize))
                    .AddSolidColorQuad(gfx::Rect(5, 5), SkColors::kGreen)
                    .AddSolidColorQuad(gfx::Rect(5, 5), SkColors::kBlue))
            .Build();
    embedded_support->SubmitCompositorFrame(
        embedded_surface_id.local_surface_id(), std::move(frame));
  }

  {
    CompositorFrame frame =
        CompositorFrameBuilder()
            .AddRenderPass(
                RenderPassBuilder(CompositorRenderPassId{1},
                                  gfx::Rect(kSurfaceSize))
                    .AddSurfaceQuad(gfx::Rect(5, 5),
                                    SurfaceRange(embedded_surface_id))
                    .SetQuadOpacity(0.5f))
            .Build();
    root_sink_->SubmitCompositorFrame(root_surface_id_.local_surface_id(),
                                      std::move(frame));

    auto aggregated_frame = AggregateFrame(root_surface_id_);

    auto& render_pass_list = aggregated_frame.render_pass_list;
    EXPECT_EQ(2u, render_pass_list.size());

    auto& shared_quad_state_list2 = render_pass_list[1]->shared_quad_state_list;
    ASSERT_EQ(1u, shared_quad_state_list2.size());
    EXPECT_EQ(.5f, shared_quad_state_list2.ElementAt(0)->opacity);
  }

  // For the case where opacity is close to 1.f, we treat it as opaque, and not
  // use a render surface.
  {
    CompositorFrame frame =
        CompositorFrameBuilder()
            .AddRenderPass(
                RenderPassBuilder(CompositorRenderPassId{2},
                                  gfx::Rect(kSurfaceSize))
                    .AddSurfaceQuad(gfx::Rect(5, 5),
                                    SurfaceRange(embedded_surface_id))
                    .SetQuadOpacity(0.9999f))
            .Build();
    root_sink_->SubmitCompositorFrame(root_surface_id_.local_surface_id(),
                                      std::move(frame));

    auto aggregated_frame = AggregateFrame(root_surface_id_);

    auto& render_pass_list = aggregated_frame.render_pass_list;
    EXPECT_EQ(1u, render_pass_list.size());
  }
}

// Test that when surface is rotated and we need the render surface to apply the
// clip, we would keep the render surface.
TEST_F(SurfaceAggregatorValidSurfaceTest, RotatedClip) {
  auto embedded_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryFrameSinkId1, /*is_root=*/true);
  TestSurfaceIdAllocator embedded_surface_id(embedded_support->frame_sink_id());

  {
    CompositorFrame frame =
        CompositorFrameBuilder()
            .AddRenderPass(
                RenderPassBuilder(kSurfaceSize)
                    .AddSolidColorQuad(gfx::Rect(5, 5), SkColors::kGreen)
                    .AddSolidColorQuad(gfx::Rect(5, 5), SkColors::kBlue))
            .Build();
    embedded_support->SubmitCompositorFrame(
        embedded_surface_id.local_surface_id(), std::move(frame));
  }

  gfx::Transform rotate;
  rotate.Rotate(30);

  CompositorFrame frame =
      CompositorFrameBuilder()
          .AddRenderPass(RenderPassBuilder(kSurfaceSize)
                             .AddSurfaceQuad(gfx::Rect(5, 5),
                                             SurfaceRange(std::nullopt,
                                                          embedded_surface_id))
                             .SetQuadToTargetTransform(rotate))
          .Build();
  root_sink_->SubmitCompositorFrame(root_surface_id_.local_surface_id(),
                                    std::move(frame));

  auto aggregated_frame = AggregateFrame(root_surface_id_);
  ASSERT_EQ(2u, aggregated_frame.render_pass_list.size());

  auto& embedded_pass = aggregated_frame.render_pass_list[0];
  EXPECT_THAT(embedded_pass->quad_list,
              ElementsAre(IsSolidColorQuad(SkColors::kGreen),
                          IsSolidColorQuad(SkColors::kBlue)));

  auto& root_pass = aggregated_frame.render_pass_list[1];
  EXPECT_THAT(
      root_pass->quad_list,
      ElementsAre(AllOf(IsAggregatedRenderPassQuad(), HasTransform(rotate))));
}

// Validate that implicit clipping when quads are drawn to an intermediate
// render pass texture the same size as the render pass output_rect is
// maintained even if the root render pass for a surface is merged into the
// embedding render pass and there is no intermediate texture.
TEST_F(SurfaceAggregatorValidSurfaceTest, ClipMergedPasses) {
  // The grandchild surface is larger than the child surface, making it possible
  // for a SolidColorDrawQuad from the granchild surface to draw beyond the
  // child surface intermediate render pass texture when surfaces are merged
  // together and intermediate textures are skipped.
  constexpr gfx::Rect grandchild_child_rect(150, 150);
  constexpr gfx::Rect child_rect(100, 100);
  constexpr gfx::Size root_size(200, 200);

  auto grandchild_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &this->manager_, kArbitraryFrameSinkId1, false);
  TestSurfaceIdAllocator grandchild_surface_id(
      grandchild_support->frame_sink_id());
  TestSurfaceIdAllocator child_surface_id(child_sink_->frame_sink_id());

  {
    auto frame = CompositorFrameBuilder()
                     .AddRenderPass(RenderPassBuilder(grandchild_child_rect)
                                        .AddSolidColorQuad(
                                            gfx::Rect(grandchild_child_rect),
                                            SkColors::kBlue))
                     .Build();
    grandchild_support->SubmitCompositorFrame(
        grandchild_surface_id.local_surface_id(), std::move(frame));
  }

  {
    // There is a 150x150 SurfaceDrawQuad translated 50,50 inside of a 100x100
    // CompositorFrame. As a result, only a 50x50 portion of the SurfaceDrawQuad
    // can be drawn and the rest extends outside the output_rect and should be
    // clipped.
    auto frame = CompositorFrameBuilder()
                     .AddRenderPass(RenderPassBuilder(child_rect)
                                        .AddSurfaceQuad(
                                            grandchild_child_rect,
                                            SurfaceRange(grandchild_surface_id))
                                        .SetQuadToTargetTranslation(50, 50))
                     .Build();
    child_sink_->SubmitCompositorFrame(child_surface_id.local_surface_id(),
                                       std::move(frame));
  }

  {
    // The SurfaceDrawQuad here is using 150x150 as the rect/visible_rect which
    // is intentionally bigger than the 100x100 output_rect of the child
    // surface.
    auto frame =
        CompositorFrameBuilder()
            .AddRenderPass(
                RenderPassBuilder(root_size)
                    .AddSurfaceQuad(grandchild_child_rect,
                                    SurfaceRange(child_surface_id))
                    .SetQuadToTargetTranslation(50, 50)
                    .AddSolidColorQuad(gfx::Rect(root_size), SkColors::kWhite))
            .Build();
    root_sink_->SubmitCompositorFrame(root_surface_id_.local_surface_id(),
                                      std::move(frame));
  }

  auto aggregated_frame = AggregateFrame(root_surface_id_);
  EXPECT_EQ(1u, aggregated_frame.render_pass_list.size());

  auto& render_pass = aggregated_frame.render_pass_list[0];
  EXPECT_THAT(render_pass->quad_list,
              ElementsAre(IsSolidColorQuad(SkColors::kBlue),
                          IsSolidColorQuad(SkColors::kWhite)));

  // Make sure there is a 150x150 solid color quad in the final frame.
  auto* clipped_quad = render_pass->quad_list.ElementAt(0);
  EXPECT_EQ(clipped_quad->rect, grandchild_child_rect);
  EXPECT_EQ(clipped_quad->visible_rect, grandchild_child_rect);
  EXPECT_TRUE(clipped_quad->shared_quad_state->clip_rect);

  // Only a 50x50 chunk of the 150x150 solid color quad should be visible. This
  // is due to the child surface root render pass output_rect being added to the
  // surface clip rect. Even if the visible_rect is wrong this ensures the
  // merged and unmerged cases produce the same output.
  EXPECT_THAT(clipped_quad->shared_quad_state->clip_rect,
              testing::Optional(gfx::Rect(100, 100, 50, 50)));
}

TEST_F(SurfaceAggregatorValidSurfaceTest, MultiPassSimpleFrame) {
  std::vector<Quad> quads[2] = {
      {Quad::SolidColorQuad(SkColors::kWhite, gfx::Rect(5, 5)),
       Quad::SolidColorQuad(SkColors::kLtGray, gfx::Rect(5, 5))},
      {Quad::SolidColorQuad(SkColors::kGray, gfx::Rect(5, 5)),
       Quad::SolidColorQuad(SkColors::kDkGray, gfx::Rect(5, 5))}};
  std::vector<Pass> passes = {
      Pass(quads[0], CompositorRenderPassId{1}, kSurfaceSize),
      Pass(quads[1], CompositorRenderPassId{2}, kSurfaceSize)};

  constexpr float device_scale_factor = 1.0f;

  SubmitCompositorFrame(root_sink_.get(), passes,
                        root_surface_id_.local_surface_id(),
                        device_scale_factor);

  AggregateAndVerify(passes, {root_surface_id_});
}

// Ensure that the render pass ID map properly keeps and deletes entries.
TEST_F(SurfaceAggregatorValidSurfaceTest, MultiPassDeallocation) {
  std::vector<Quad> quads[2] = {
      {Quad::SolidColorQuad(SkColors::kWhite, gfx::Rect(5, 5)),
       Quad::SolidColorQuad(SkColors::kLtGray, gfx::Rect(5, 5))},
      {Quad::SolidColorQuad(SkColors::kGray, gfx::Rect(5, 5)),
       Quad::SolidColorQuad(SkColors::kDkGray, gfx::Rect(5, 5))}};
  std::vector<Pass> passes = {
      Pass(quads[0], CompositorRenderPassId{2}, kSurfaceSize),
      Pass(quads[1], CompositorRenderPassId{1}, kSurfaceSize)};

  constexpr float device_scale_factor = 1.0f;
  SubmitCompositorFrame(root_sink_.get(), passes,
                        root_surface_id_.local_surface_id(),
                        device_scale_factor);

  auto aggregated_frame = AggregateFrame(root_surface_id_);
  auto id0 = aggregated_frame.render_pass_list[0]->id;
  auto id1 = aggregated_frame.render_pass_list[1]->id;
  EXPECT_NE(id1, id0);

  // Aggregated RenderPass ids should remain the same between frames.
  aggregated_frame = AggregateFrame(root_surface_id_);
  EXPECT_EQ(id0, aggregated_frame.render_pass_list[0]->id);
  EXPECT_EQ(id1, aggregated_frame.render_pass_list[1]->id);

  std::vector<Pass> passes2 = {
      Pass(quads[0], CompositorRenderPassId{3}, kSurfaceSize),
      Pass(quads[1], CompositorRenderPassId{1}, kSurfaceSize)};

  SubmitCompositorFrame(root_sink_.get(), passes2,
                        root_surface_id_.local_surface_id(),
                        device_scale_factor);

  // The RenderPass that still exists should keep the same ID.
  aggregated_frame = AggregateFrame(root_surface_id_);
  auto id2 = aggregated_frame.render_pass_list[0]->id;
  EXPECT_NE(id2, id1);
  EXPECT_NE(id2, id0);
  EXPECT_EQ(id1, aggregated_frame.render_pass_list[1]->id);

  SubmitCompositorFrame(root_sink_.get(), passes,
                        root_surface_id_.local_surface_id(),
                        device_scale_factor);

  // |id1| didn't exist in the previous frame, so it should be
  // mapped to a new ID.
  aggregated_frame = AggregateFrame(root_surface_id_);
  auto id3 = aggregated_frame.render_pass_list[0]->id;
  EXPECT_NE(id3, id2);
  EXPECT_NE(id3, id1);
}

// Ensure that the render pass ID map properly keeps and deletes entries.
TEST_F(SurfaceAggregatorValidSurfaceTest, MultiSurfacePassDeallocation) {
  std::vector<Quad> quads[2] = {
      {Quad::SolidColorQuad(SkColors::kWhite, gfx::Rect(5, 5)),
       Quad::SolidColorQuad(SkColors::kLtGray, gfx::Rect(5, 5))},
      {Quad::SolidColorQuad(SkColors::kGray, gfx::Rect(5, 5)),
       Quad::SolidColorQuad(SkColors::kDkGray, gfx::Rect(5, 5))}};
  std::vector<Pass> passes = {
      Pass(quads[0], CompositorRenderPassId{2}, kSurfaceSize),
      Pass(quads[1], CompositorRenderPassId{1}, kSurfaceSize)};
  constexpr float device_scale_factor = 1.0f;

  // 1. Submit a frame to the root surface.
  SubmitCompositorFrame(root_sink_.get(), passes,
                        root_surface_id_.local_surface_id(),
                        device_scale_factor);
  auto aggregated_frame = AggregateFrame(root_surface_id_);

  auto id0 = aggregated_frame.render_pass_list[0]->id;
  auto id1 = aggregated_frame.render_pass_list[1]->id;
  EXPECT_NE(id1, id0);

  // 2. Add a child surface to the mix.
  std::vector<Pass> child_passes = {
      Pass(quads[0], CompositorRenderPassId{1}, kSurfaceSize)};
  TestSurfaceIdAllocator child_surface_id(child_sink_->frame_sink_id());
  SubmitCompositorFrame(child_sink_.get(), child_passes,
                        child_surface_id.local_surface_id(),
                        device_scale_factor);
  // Disallow merging so the child pass ids can be tested.
  std::vector<Quad> child_surface_quads = {Quad::SurfaceQuad(
      SurfaceRange(child_surface_id), SkColors::kBlack, gfx::Rect(5, 5),
      /*stretch_content_to_fill_bounds=*/false, /*allow_merge=*/false)};
  std::vector<Pass> root_embedding_passes = {
      Pass(child_surface_quads, CompositorRenderPassId{3}, kSurfaceSize),
      Pass(quads[0], CompositorRenderPassId{2}, kSurfaceSize),
      Pass(quads[1], CompositorRenderPassId{1}, kSurfaceSize),
  };

  SubmitCompositorFrame(root_sink_.get(), root_embedding_passes,
                        root_surface_id_.local_surface_id(),
                        device_scale_factor);
  aggregated_frame = AggregateFrame(root_surface_id_);

  // The child pass should be added at the beginning of the pass list.
  EXPECT_EQ(aggregated_frame.render_pass_list.size(), 4u);
  auto child_id0 = aggregated_frame.render_pass_list[0]->id;
  auto id3 = aggregated_frame.render_pass_list[1]->id;
  // These should be mapped to different ids than the ones in the root pass.
  EXPECT_NE(child_id0, id0);
  EXPECT_NE(child_id0, id1);
  EXPECT_NE(id3, id0);
  EXPECT_NE(id3, id1);
  EXPECT_NE(child_id0, id3);
  // These should have the same ids as they did in the first aggregated frame.
  EXPECT_EQ(id0, aggregated_frame.render_pass_list[2]->id);
  EXPECT_EQ(id1, aggregated_frame.render_pass_list[3]->id);

  // 3. Submit a new root frame that still embeds the child surface.
  SubmitCompositorFrame(root_sink_.get(), root_embedding_passes,
                        root_surface_id_.local_surface_id(),
                        device_scale_factor);
  aggregated_frame = AggregateFrame(root_surface_id_);

  // All render pass ids should be the same as last frame.
  EXPECT_EQ(aggregated_frame.render_pass_list.size(), 4u);
  EXPECT_EQ(child_id0, aggregated_frame.render_pass_list[0]->id);
  EXPECT_EQ(id3, aggregated_frame.render_pass_list[1]->id);
  EXPECT_EQ(id0, aggregated_frame.render_pass_list[2]->id);
  EXPECT_EQ(id1, aggregated_frame.render_pass_list[3]->id);

  // 4. Now drop the child surface.
  SubmitCompositorFrame(root_sink_.get(), passes,
                        root_surface_id_.local_surface_id(),
                        device_scale_factor);
  aggregated_frame = AggregateFrame(root_surface_id_);

  EXPECT_EQ(id0, aggregated_frame.render_pass_list[0]->id);
  EXPECT_EQ(id1, aggregated_frame.render_pass_list[1]->id);

  // 5. Embed the child surface again.
  SubmitCompositorFrame(child_sink_.get(), child_passes,
                        child_surface_id.local_surface_id(),
                        device_scale_factor);
  SubmitCompositorFrame(root_sink_.get(), root_embedding_passes,
                        root_surface_id_.local_surface_id(),
                        device_scale_factor);
  aggregated_frame = AggregateFrame(root_surface_id_);

  EXPECT_EQ(aggregated_frame.render_pass_list.size(), 4u);
  auto child_id1 = aggregated_frame.render_pass_list[0]->id;
  // The child surface wasn't embedded in the last frame, so it's render pass
  // and the embedder render pass should get new ids.
  EXPECT_NE(child_id1, child_id0);
  EXPECT_NE(id3, aggregated_frame.render_pass_list[1]->id);
  // These should still have the same ids as they did in the first aggregated
  // frame.
  EXPECT_EQ(id0, aggregated_frame.render_pass_list[2]->id);
  EXPECT_EQ(id1, aggregated_frame.render_pass_list[3]->id);
}

// This tests very simple embedding. root_surface has a frame containing a few
// solid color quads and a surface quad referencing embedded_surface.
// embedded_surface has a frame containing only a solid color quad. The solid
// color quad should be aggregated into the final frame.
TEST_F(SurfaceAggregatorValidSurfaceTest, SimpleSurfaceReference) {
  auto embedded_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryFrameSinkId1, /*is_root=*/true);
  TestSurfaceIdAllocator embedded_surface_id(embedded_support->frame_sink_id());

  {
    CompositorFrame frame =
        CompositorFrameBuilder()
            .AddRenderPass(
                RenderPassBuilder(kSurfaceSize)
                    .AddSolidColorQuad(gfx::Rect(5, 5), SkColors::kGreen))
            .Build();
    embedded_support->SubmitCompositorFrame(
        embedded_surface_id.local_surface_id(), std::move(frame));
  }

  {
    CompositorFrame frame =
        CompositorFrameBuilder()
            .AddRenderPass(
                RenderPassBuilder(kSurfaceSize)
                    .AddSolidColorQuad(gfx::Rect(5, 5), SkColors::kWhite)
                    .AddSurfaceQuad(
                        gfx::Rect(5, 5),
                        SurfaceRange(std::nullopt, embedded_surface_id))
                    .AddSolidColorQuad(gfx::Rect(5, 5), SkColors::kBlack))
            .Build();
    root_sink_->SubmitCompositorFrame(root_surface_id_.local_surface_id(),
                                      std::move(frame));
  }

  auto aggregated_frame = AggregateFrame(root_surface_id_);
  ASSERT_EQ(aggregated_frame.render_pass_list.size(), 1u);

  auto& render_pass = aggregated_frame.render_pass_list[0];
  EXPECT_THAT(render_pass->quad_list,
              ElementsAre(IsSolidColorQuad(SkColors::kWhite),
                          IsSolidColorQuad(SkColors::kGreen),
                          IsSolidColorQuad(SkColors::kBlack)));

  VerifyExpectedSurfaceIds({root_surface_id_, embedded_surface_id});
}

class TestVizClient {
 public:
  TestVizClient(SurfaceAggregatorValidSurfaceTest* test,
                FrameSinkManagerImpl* manager,
                const FrameSinkId& frame_sink_id,
                const gfx::Rect& bounds)
      : test_(test),
        manager_(manager),
        frame_sink_id_(frame_sink_id),
        bounds_(bounds) {
    constexpr bool is_root = false;
    root_sink_ = std::make_unique<CompositorFrameSinkSupport>(
        nullptr, manager_, frame_sink_id, is_root);
    allocator_.GenerateId();
  }

  TestVizClient(const TestVizClient&) = delete;
  TestVizClient& operator=(const TestVizClient&) = delete;

  ~TestVizClient() = default;

  Surface* GetSurface() const {
    return manager_->surface_manager()->GetSurfaceForId(
        SurfaceId(frame_sink_id_, local_surface_id()));
  }

  void SubmitCompositorFrame(SkColor4f bgcolor) {
    using Quad = SurfaceAggregatorValidSurfaceTest::Quad;
    using Pass = SurfaceAggregatorValidSurfaceTest::Pass;

    std::vector<SurfaceRange> referenced_surfaces;
    std::vector<Quad> embedded_quads = {Quad::SolidColorQuad(bgcolor, bounds_)};
    for (const auto& embed : embedded_clients_) {
      if (embed.second) {
        embedded_quads.push_back(Quad::SurfaceQuad(
            SurfaceRange(std::nullopt, embed.first->surface_id()),
            SkColors::kWhite, embed.first->bounds(),
            /*stretch_content_to_fill_bounds=*/false));
      } else {
        referenced_surfaces.emplace_back(
            SurfaceRange(std::nullopt, embed.first->surface_id()));
      }
    }
    std::vector<Pass> embedded_passes = {Pass(embedded_quads, bounds_.size())};

    constexpr float device_scale_factor = 1.0f;
    CompositorRenderPassList pass_list =
        test_->GenerateRenderPassList(embedded_passes, &referenced_surfaces);
    test_->SubmitPassListAsFrame(root_sink_.get(), local_surface_id(),
                                 &pass_list, referenced_surfaces,
                                 device_scale_factor);
  }

  void SetEmbeddedClient(TestVizClient* embedded, bool add_quad) {
    embedded_clients_[embedded] = add_quad;
  }

  CopyOutputRequest* RequestCopyOfOutput() {
    auto copy_request = CopyOutputRequest::CreateStubForTesting();
    auto* copy_request_ptr = copy_request.get();
    root_sink_->RequestCopyOfOutput(PendingCopyOutputRequest{
        local_surface_id(), SubtreeCaptureId(), std::move(copy_request)});
    return copy_request_ptr;
  }

  SurfaceId surface_id() const { return {frame_sink_id_, local_surface_id()}; }
  const gfx::Rect& bounds() const { return bounds_; }
  const LocalSurfaceId& local_surface_id() const {
    return allocator_.GetCurrentLocalSurfaceId();
  }

 private:
  const raw_ptr<SurfaceAggregatorValidSurfaceTest> test_;
  const raw_ptr<FrameSinkManagerImpl> manager_;
  std::unique_ptr<CompositorFrameSinkSupport> root_sink_;
  const FrameSinkId frame_sink_id_;
  const gfx::Rect bounds_;
  ParentLocalSurfaceIdAllocator allocator_;

  std::map<TestVizClient*, bool> embedded_clients_;
};

TEST_F(SurfaceAggregatorValidSurfaceTest, UndrawnSurfaces) {
  TestVizClient child(this, &manager_, kArbitraryFrameSinkId1,
                      gfx::Rect(10, 10));
  child.SubmitCompositorFrame(SkColors::kBlue);

  // Parent first submits a CompositorFrame that references |child|, but does
  // not provide a DrawQuad that embeds it.
  TestVizClient parent(this, &manager_, kArbitraryFrameSinkId2,
                       gfx::Rect(15, 15));
  parent.SetEmbeddedClient(&child, false);
  parent.SubmitCompositorFrame(SkColors::kGreen);

  std::vector<Quad> root_quads = {
      Quad::SolidColorQuad(SkColors::kWhite, gfx::Rect(5, 5)),
      Quad::SurfaceQuad(SurfaceRange(std::nullopt, parent.surface_id()),
                        SkColors::kWhite, parent.bounds(),
                        /*stretch_content_to_fill_bounds=*/false),
      Quad::SolidColorQuad(SkColors::kBlack, gfx::Rect(5, 5))};
  std::vector<Pass> root_passes = {Pass(root_quads, kSurfaceSize)};

  constexpr float device_scale_factor = 1.0f;
  SubmitCompositorFrame(root_sink_.get(), root_passes,
                        root_surface_id_.local_surface_id(),
                        device_scale_factor);

  std::vector<Quad> expected_quads = {
      Quad::SolidColorQuad(SkColors::kWhite, gfx::Rect(5, 5)),
      Quad::SolidColorQuad(SkColors::kGreen, parent.bounds()),
      Quad::SolidColorQuad(SkColors::kBlack, gfx::Rect(5, 5))};
  std::vector<Pass> expected_passes = {Pass(expected_quads, kSurfaceSize)};
  AggregateAndVerify(expected_passes, {root_surface_id_, parent.surface_id(),
                                       child.surface_id()});
  // |child| should not be drawn.
  EXPECT_TRUE(child.GetSurface()->HasUndrawnActiveFrame());
  EXPECT_FALSE(parent.GetSurface()->HasUndrawnActiveFrame());

  // Submit another CompositorFrame from |parent|, this time with a DrawQuad for
  // |child|.
  parent.SetEmbeddedClient(&child, true);
  parent.SubmitCompositorFrame(SkColors::kGreen);

  expected_quads = {Quad::SolidColorQuad(SkColors::kWhite, gfx::Rect(5, 5)),
                    Quad::SolidColorQuad(SkColors::kGreen, parent.bounds()),
                    Quad::SolidColorQuad(SkColors::kBlue, child.bounds()),
                    Quad::SolidColorQuad(SkColors::kBlack, gfx::Rect(5, 5))};
  AggregateAndVerify(
      {Pass(expected_quads, kSurfaceSize)},
      {root_surface_id_, parent.surface_id(), child.surface_id()});
  EXPECT_FALSE(child.GetSurface()->HasUndrawnActiveFrame());
}

TEST_F(SurfaceAggregatorValidSurfaceTest, UndrawnSurfacesWithCopyRequests) {
  TestVizClient child(this, &manager_, kArbitraryFrameSinkId1,
                      gfx::Rect(10, 10));
  child.SubmitCompositorFrame(SkColors::kBlue);
  child.RequestCopyOfOutput();

  // Parent first submits a CompositorFrame that references |child|, but does
  // not provide a DrawQuad that embeds it.
  TestVizClient parent(this, &manager_, kArbitraryFrameSinkId2,
                       gfx::Rect(15, 15));
  parent.SetEmbeddedClient(&child, false);
  parent.SubmitCompositorFrame(SkColors::kGreen);

  std::vector<Quad> root_quads = {
      Quad::SolidColorQuad(SkColors::kWhite, gfx::Rect(5, 5)),
      Quad::SurfaceQuad(SurfaceRange(std::nullopt, parent.surface_id()),
                        SkColors::kWhite, parent.bounds(),
                        /*stretch_content_to_fill_bounds=*/false),
      Quad::SolidColorQuad(SkColors::kBlack, gfx::Rect(5, 5))};
  std::vector<Pass> root_passes = {Pass(root_quads, kSurfaceSize)};

  constexpr float device_scale_factor = 1.0f;
  SubmitCompositorFrame(root_sink_.get(), root_passes,
                        root_surface_id_.local_surface_id(),
                        device_scale_factor);

  std::vector<Quad> expected_quads = {
      Quad::SolidColorQuad(SkColors::kWhite, gfx::Rect(5, 5)),
      Quad::SolidColorQuad(SkColors::kGreen, parent.bounds()),
      Quad::SolidColorQuad(SkColors::kBlack, gfx::Rect(5, 5))};
  std::vector<Quad> expected_copy_quads = {
      Quad::SolidColorQuad(SkColors::kBlue, child.bounds())};
  std::vector<Pass> expected_passes = {Pass(expected_copy_quads, kSurfaceSize),
                                       Pass(expected_quads, kSurfaceSize)};
  AggregateAndVerify(expected_passes, {root_surface_id_, parent.surface_id(),
                                       child.surface_id()});
  EXPECT_FALSE(child.GetSurface()->HasUndrawnActiveFrame());
  EXPECT_FALSE(parent.GetSurface()->HasUndrawnActiveFrame());
}

TEST_F(SurfaceAggregatorValidSurfaceTest,
       SurfacesWithMultipleEmbeddersBothVisibleAndInvisible) {
  TestVizClient child(this, &manager_, kArbitraryFrameSinkId1,
                      gfx::Rect(10, 10));
  child.SubmitCompositorFrame(SkColors::kBlue);

  // First parent submits a CompositorFrame that references |child|, but does
  // not provide a DrawQuad that embeds it.
  TestVizClient first_parent(this, &manager_, kArbitraryFrameSinkId2,
                             gfx::Rect(15, 15));
  first_parent.SetEmbeddedClient(&child, false);
  first_parent.SubmitCompositorFrame(SkColors::kGreen);

  // Second parent submits a CompositorFrame referencing |child|, and also
  // includes a draw-quad for it.
  TestVizClient second_parent(this, &manager_, kArbitraryMiddleFrameSinkId,
                              gfx::Rect(25, 25));
  second_parent.SetEmbeddedClient(&child, true);
  second_parent.SubmitCompositorFrame(SkColors::kYellow);

  // Submit a root CompositorFrame that embeds both parents.
  std::vector<Quad> root_quads = {
      Quad::SolidColorQuad(SkColors::kWhite, gfx::Rect(5, 5)),
      Quad::SurfaceQuad(SurfaceRange(std::nullopt, first_parent.surface_id()),
                        SkColors::kCyan, first_parent.bounds(),
                        /*stretch_content_to_fill_bounds=*/false),
      Quad::SurfaceQuad(SurfaceRange(std::nullopt, second_parent.surface_id()),
                        SkColors::kMagenta, second_parent.bounds(),
                        /*stretch_content_to_fill_bounds=*/false),
      Quad::SolidColorQuad(SkColors::kBlack, gfx::Rect(5, 5))};
  std::vector<Pass> root_passes = {Pass(root_quads, kSurfaceSize)};

  constexpr float device_scale_factor = 1.0f;
  SubmitCompositorFrame(root_sink_.get(), root_passes,
                        root_surface_id_.local_surface_id(),
                        device_scale_factor);

  EXPECT_TRUE(child.GetSurface()->HasUndrawnActiveFrame());
  EXPECT_TRUE(first_parent.GetSurface()->HasUndrawnActiveFrame());
  EXPECT_TRUE(second_parent.GetSurface()->HasUndrawnActiveFrame());

  std::vector<Quad> expected_quads = {
      Quad::SolidColorQuad(SkColors::kWhite, gfx::Rect(5, 5)),
      Quad::SolidColorQuad(SkColors::kGreen, first_parent.bounds()),
      Quad::SolidColorQuad(SkColors::kYellow, second_parent.bounds()),
      Quad::SolidColorQuad(SkColors::kBlue, gfx::Rect(10, 10)),
      Quad::SolidColorQuad(SkColors::kBlack, gfx::Rect(5, 5))};
  std::vector<Quad> expected_copy_quads = {};
  std::vector<Pass> expected_passes = {Pass(expected_quads, kSurfaceSize)};
  AggregateAndVerify(expected_passes,
                     {root_surface_id_, first_parent.surface_id(),
                      second_parent.surface_id(), child.surface_id()});
  EXPECT_FALSE(child.GetSurface()->HasUndrawnActiveFrame());
  EXPECT_FALSE(first_parent.GetSurface()->HasUndrawnActiveFrame());
  EXPECT_FALSE(second_parent.GetSurface()->HasUndrawnActiveFrame());
}

// Verify that when the parent and child surface have different device scale
// factors both the damage_rect and aggregated quads from the child surface are
// scaled appropriately. https://crbug.com/1115896 was caused by a mismatch in
// the scaling of quads and damage from a child surface.
TEST_F(SurfaceAggregatorValidSurfaceTest, ScaleForDeviceScaleFactor) {
  auto child_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryFrameSinkId1, /*is_root=*/false);
  TestSurfaceIdAllocator child_surface_id(child_support->frame_sink_id());

  constexpr gfx::Rect child_surface_rect(200, 200);
  constexpr gfx::Rect child_quad_rect(50, 50);
  // Matches the where the solid color draw quad appears.
  constexpr gfx::Rect child_damage_rect(100, 100, 50, 50);

  auto child_pass =
      RenderPassBuilder(CompositorRenderPassId{1}, child_surface_rect)
          .AddSolidColorQuad(child_quad_rect, SkColors::kRed)
          .SetQuadToTargetTranslation(100, 100)
          .SetDamageRect(child_damage_rect)
          .Build();

  {
    child_support->SubmitCompositorFrame(
        child_surface_id.local_surface_id(),
        CompositorFrameBuilder()
            .AddRenderPass(child_pass->DeepCopy())
            .SetDeviceScaleFactor(2.0f)
            .Build());
  }

  constexpr gfx::Rect root_quad_rect(10, 10);
  {
    auto pass = RenderPassBuilder(kSurfaceSize)
                    .AddSolidColorQuad(root_quad_rect, SkColors::kRed)
                    .AddSurfaceQuad(gfx::Rect(kSurfaceSize),
                                    SurfaceRange(child_surface_id))
                    .Build();

    CompositorFrame frame = CompositorFrameBuilder()
                                .AddRenderPass(std::move(pass))
                                .SetDeviceScaleFactor(1.0f)
                                .Build();
    root_sink_->SubmitCompositorFrame(root_surface_id_.local_surface_id(),
                                      std::move(frame));
  }

  {
    // The first aggregation will have full damage so the results aren't super
    // interesting.
    auto frame = AggregateFrame(root_surface_id_);
    EXPECT_EQ(frame.render_pass_list.size(), 1u);
  }

  {
    // Submit a new CF to the child surface. Nothing has changed but we'll use
    // the real damage this time.
    child_support->SubmitCompositorFrame(
        child_surface_id.local_surface_id(),
        CompositorFrameBuilder()
            .AddRenderPass(std::move(child_pass))
            .SetDeviceScaleFactor(2.0f)
            .Build());
  }

  {
    auto frame = AggregateFrame(root_surface_id_);
    ASSERT_EQ(frame.render_pass_list.size(), 1u);
    auto& render_pass = *frame.render_pass_list[0];

    // Since the child surface has DSF=2 and root surface has DSF=1 the child
    // surface will be scaled by a factor of 0.5. Both the damage and quads
    // should be scaled by this factor. Only the child surface contributes
    // damage this aggregation so the (100,100 50x50) damage_rect will be scaled
    // to (50,50 25x25).
    constexpr gfx::Rect expected_scaled_child_rect(50, 50, 25, 25);
    EXPECT_EQ(render_pass.damage_rect, expected_scaled_child_rect);

    EXPECT_EQ(render_pass.quad_list.size(), 2u);

    // The quad coming from the root render pass isn't scaled.
    auto* root_quad = render_pass.quad_list.ElementAt(0);
    EXPECT_EQ(root_quad->material, DrawQuad::Material::kSolidColor);
    EXPECT_EQ(root_quad->rect, root_quad_rect);
    EXPECT_TRUE(
        root_quad->shared_quad_state->quad_to_target_transform.IsIdentity());

    // The quad coming from the child render pass has the same scale factor
    // applied to it as the damage. The transformed quad rect should match the
    // expected damage rect since the quad rect and damage rect matched before
    // scaling.
    auto* child_quad = render_pass.quad_list.ElementAt(1);
    EXPECT_EQ(child_quad->material, DrawQuad::Material::kSolidColor);
    EXPECT_EQ(child_quad->rect, child_quad_rect);
    gfx::Rect scaled_child_quad_rect = cc::MathUtil::MapEnclosingClippedRect(
        child_quad->shared_quad_state->quad_to_target_transform,
        child_quad->rect);
    EXPECT_EQ(expected_scaled_child_rect, scaled_child_quad_rect);
  }
}

// Verify that layer_ids are deduplicated in the final AggregatedFrame
// correctly.
TEST_F(SurfaceAggregatorValidSurfaceTest, LayerIds) {
  TestSurfaceIdAllocator child_surface_id(child_sink_->frame_sink_id());

  gfx::Rect child_surface_rect(20, 20);
  {
    CompositorFrame frame =
        CompositorFrameBuilder()
            .AddRenderPass(
                RenderPassBuilder(child_surface_rect)
                    .AddSolidColorQuad(gfx::Rect(20, 20), SkColors::kRed)
                    .SetQuadLayerId(1u))
            .Build();

    child_sink_->SubmitCompositorFrame(child_surface_id.local_surface_id(),
                                       std::move(frame));
  }

  {
    CompositorFrame frame =
        CompositorFrameBuilder()
            .AddRenderPass(RenderPassBuilder(kSurfaceSize)
                               .AddSurfaceQuad(child_surface_rect,
                                               SurfaceRange(child_surface_id))
                               .SetQuadLayerId(2)
                               .AddSolidColorQuad(gfx::Rect(kSurfaceSize),
                                                  SkColors::kBlack)
                               .SetQuadLayerId(3))
            .Build();
    root_sink_->SubmitCompositorFrame(root_surface_id_.local_surface_id(),
                                      std::move(frame));
  }

  {
    auto frame = AggregateFrame(root_surface_id_);
    ASSERT_EQ(1u, frame.render_pass_list.size());
    auto* render_pass = frame.render_pass_list.back().get();

    uint32_t root_surface_namespace =
        aggregator_.GetLatestFrameData(root_surface_id_)
            ->GetClientNamespaceId();
    uint32_t child_surface_namespace =
        aggregator_.GetLatestFrameData(child_surface_id)
            ->GetClientNamespaceId();

    // The child surface is merged into the root surface so there is a single
    // render pass with a solid color draw quad from both clients. Both will
    // have client namespace ID + original layer ID as their final layer ID.
    EXPECT_THAT(render_pass->quad_list,
                ElementsAre(AllOf(IsSolidColorQuad(SkColors::kRed),
                                  HasLayerNamespaceId(child_surface_namespace),
                                  HasLayerId(1u)),
                            AllOf(IsSolidColorQuad(SkColors::kBlack),
                                  HasLayerNamespaceId(root_surface_namespace),
                                  HasLayerId(3u))));
  }

  // Redo the aggregation but don't allow merging child surface into the root
  // render pass.
  {
    CompositorFrame frame =
        CompositorFrameBuilder()
            .AddRenderPass(RenderPassBuilder(kSurfaceSize)
                               .AddSurfaceQuad(child_surface_rect,
                                               SurfaceRange(child_surface_id),
                                               {.allow_merge = false})
                               .SetQuadLayerId(2)
                               .AddSolidColorQuad(gfx::Rect(kSurfaceSize),
                                                  SkColors::kBlack)
                               .SetQuadLayerId(3))
            .Build();
    root_sink_->SubmitCompositorFrame(root_surface_id_.local_surface_id(),
                                      std::move(frame));
  }

  {
    auto frame = AggregateFrame(root_surface_id_);
    ASSERT_EQ(2u, frame.render_pass_list.size());
    auto* child_pass = frame.render_pass_list.at(0).get();
    auto* root_pass = frame.render_pass_list.at(1).get();

    uint32_t root_surface_namespace =
        aggregator_.GetLatestFrameData(root_surface_id_)
            ->GetClientNamespaceId();
    uint32_t child_surface_namespace =
        aggregator_.GetLatestFrameData(child_surface_id)
            ->GetClientNamespaceId();

    EXPECT_THAT(child_pass->quad_list,
                ElementsAre(AllOf(IsSolidColorQuad(SkColors::kRed),
                                  HasLayerNamespaceId(child_surface_namespace),
                                  HasLayerId(1u))));

    // The AggregatedRenderPassDrawQuad is taking the place of the
    // SurfaceDrawQuad so it should have the same client namespace as other
    // quads from the root surface.
    EXPECT_THAT(root_pass->quad_list,
                ElementsAre(AllOf(IsAggregatedRenderPassQuad(),
                                  HasLayerNamespaceId(root_surface_namespace),
                                  HasLayerId(2u)),
                            AllOf(IsSolidColorQuad(SkColors::kBlack),
                                  HasLayerNamespaceId(root_surface_namespace),
                                  HasLayerId(3u))));
  }
}

// This test verifies that the appropriate transform will be applied to a
// surface embedded by a parent SurfaceDrawQuad marked as
// stretch_content_to_fill_bounds.
TEST_F(SurfaceAggregatorValidSurfaceTest, StretchContentToFillBounds) {
  auto primary_child_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryFrameSinkId1, /*is_root=*/false);
  TestSurfaceIdAllocator primary_child_surface_id(
      primary_child_support->frame_sink_id());

  {
    CompositorFrame frame =
        CompositorFrameBuilder()
            .AddRenderPass(
                RenderPassBuilder(gfx::Rect(20, 20))
                    .AddSolidColorQuad(gfx::Rect(20, 20), SkColors::kRed))
            .Build();

    primary_child_support->SubmitCompositorFrame(
        primary_child_surface_id.local_surface_id(), std::move(frame));
  }

  MockAggregatedDamageCallback aggregated_damage_callback;
  root_sink_->SetAggregatedDamageCallbackForTesting(
      aggregated_damage_callback.GetCallback());

  {
    constexpr gfx::Rect surface_quad_rect(10, 5);
    CompositorFrame frame =
        CompositorFrameBuilder()
            .AddRenderPass(
                RenderPassBuilder(kSurfaceSize)
                    .AddSurfaceQuad(surface_quad_rect,
                                    SurfaceRange(primary_child_surface_id),
                                    {.stretch_content_to_fill_bounds = true}))
            .Build();
    root_sink_->SubmitCompositorFrame(root_surface_id_.local_surface_id(),
                                      std::move(frame));
  }

  EXPECT_CALL(
      aggregated_damage_callback,
      OnAggregatedDamage(root_surface_id_.local_surface_id(), kSurfaceSize,
                         gfx::Rect(kSurfaceSize), next_display_time()));
  auto frame = AggregateFrame(root_surface_id_);
  testing::Mock::VerifyAndClearExpectations(&aggregated_damage_callback);

  EXPECT_EQ(1u, frame.render_pass_list.size());
  auto* render_pass = frame.render_pass_list.back().get();
  EXPECT_EQ(1u, render_pass->quad_list.size());

  auto* output_quad = render_pass->quad_list.back();

  EXPECT_EQ(DrawQuad::Material::kSolidColor, output_quad->material);

  // SurfaceAggregator should stretch the SolidColorDrawQuad to fit the bounds
  // of the parent's SurfaceDrawQuad.
  gfx::RectF output_rect =
      output_quad->shared_quad_state->quad_to_target_transform.MapRect(
          gfx::RectF(100.f, 100.f));

  EXPECT_EQ(gfx::RectF(50.f, 25.f), output_rect);
}

// This test verifies that the appropriate transform will be applied to a
// surface embedded by a parent SurfaceDrawQuad marked as
// stretch_content_to_fill_bounds when the device_scale_factor is
// greater than 1.
TEST_F(SurfaceAggregatorValidSurfaceTest, StretchContentToFillStretchedBounds) {
  auto primary_child_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryFrameSinkId1, /*is_root=*/false);
  TestSurfaceIdAllocator primary_child_surface_id(
      primary_child_support->frame_sink_id());

  {
    auto pass = CompositorRenderPass::Create();
    pass->SetNew(CompositorRenderPassId{1}, gfx::Rect(0, 0, 20, 20),
                 gfx::Rect(), gfx::Transform());
    auto* sqs = pass->CreateAndAppendSharedQuadState();
    auto* solid_color_quad =
        pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();

    solid_color_quad->SetNew(sqs, gfx::Rect(0, 0, 20, 20),
                             gfx::Rect(0, 0, 20, 20), SkColors::kRed, false);

    CompositorFrame frame =
        CompositorFrameBuilder().AddRenderPass(std::move(pass)).Build();

    primary_child_support->SubmitCompositorFrame(
        primary_child_surface_id.local_surface_id(), std::move(frame));
  }

  constexpr gfx::Rect surface_quad_rect(10, 5);
  std::vector<Quad> root_quads = {
      Quad::SurfaceQuad(SurfaceRange(primary_child_surface_id),
                        SkColors::kWhite, surface_quad_rect,
                        /*stretch_content_to_fill_bounds=*/true)};
  std::vector<Pass> root_passes = {Pass(root_quads, kSurfaceSize)};

  MockAggregatedDamageCallback aggregated_damage_callback;
  root_sink_->SetAggregatedDamageCallbackForTesting(
      aggregated_damage_callback.GetCallback());

  SubmitCompositorFrame(root_sink_.get(), root_passes,
                        root_surface_id_.local_surface_id(), 2.0f);

  EXPECT_CALL(
      aggregated_damage_callback,
      OnAggregatedDamage(root_surface_id_.local_surface_id(), kSurfaceSize,
                         gfx::Rect(kSurfaceSize), next_display_time()));
  auto frame = AggregateFrame(root_surface_id_);

  testing::Mock::VerifyAndClearExpectations(&aggregated_damage_callback);

  EXPECT_EQ(1u, frame.render_pass_list.size());
  auto* render_pass = frame.render_pass_list.back().get();
  EXPECT_EQ(1u, render_pass->quad_list.size());

  auto* output_quad = render_pass->quad_list.back();

  EXPECT_EQ(DrawQuad::Material::kSolidColor, output_quad->material);

  // SurfaceAggregator should stretch the SolidColorDrawQuad to fit the bounds
  // of the parent's SurfaceDrawQuad.
  gfx::RectF output_rect =
      output_quad->shared_quad_state->quad_to_target_transform.MapRect(
          gfx::RectF(200.f, 200.f));

  EXPECT_EQ(gfx::RectF(100.f, 50.f), output_rect);
}

// This test verifies that the appropriate transform will be applied to a
// surface embedded by a parent SurfaceDrawQuad marked as
// stretch_content_to_fill_bounds when the device_scale_factor is
// less than 1.
TEST_F(SurfaceAggregatorValidSurfaceTest, StretchContentToFillSquashedBounds) {
  auto primary_child_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryFrameSinkId1, /*is_root=*/false);
  TestSurfaceIdAllocator primary_child_surface_id(
      primary_child_support->frame_sink_id());

  constexpr gfx::Rect child_surface_rect(20, 20);
  {
    CompositorFrame frame =
        CompositorFrameBuilder()
            .AddRenderPass(
                RenderPassBuilder(child_surface_rect)
                    .AddSolidColorQuad(child_surface_rect, SkColors::kRed))
            .SetDeviceScaleFactor(1.0f)
            .Build();

    primary_child_support->SubmitCompositorFrame(
        primary_child_surface_id.local_surface_id(), std::move(frame));
  }

  constexpr gfx::Rect surface_quad_rect(10, 5);
  {
    CompositorFrame frame =
        CompositorFrameBuilder()
            .AddRenderPass(
                RenderPassBuilder(kSurfaceSize)
                    .AddSurfaceQuad(surface_quad_rect,
                                    SurfaceRange(primary_child_surface_id),
                                    {.stretch_content_to_fill_bounds = true}))
            .SetDeviceScaleFactor(0.5f)
            .Build();
    root_sink_->SubmitCompositorFrame(root_surface_id_.local_surface_id(),
                                      std::move(frame));
  }

  MockAggregatedDamageCallback aggregated_damage_callback;
  root_sink_->SetAggregatedDamageCallbackForTesting(
      aggregated_damage_callback.GetCallback());

  EXPECT_CALL(
      aggregated_damage_callback,
      OnAggregatedDamage(root_surface_id_.local_surface_id(), kSurfaceSize,
                         gfx::Rect(kSurfaceSize), next_display_time()));
  auto frame = AggregateFrame(root_surface_id_);
  testing::Mock::VerifyAndClearExpectations(&aggregated_damage_callback);

  EXPECT_EQ(1u, frame.render_pass_list.size());
  auto* render_pass = frame.render_pass_list.back().get();

  // SurfaceAggregator should stretch the 20x20 SolidColorDrawQuad to fit the
  // bounds of the parent's 10x5 SurfaceDrawQuad.
  gfx::Transform expected_transform;
  expected_transform.Scale(0.5, 0.25);

  EXPECT_THAT(render_pass->quad_list,
              ElementsAre(AllOf(IsSolidColorQuad(), HasRect(child_surface_rect),
                                HasTransform(expected_transform))));
}

// Verify that a reflected SurfaceDrawQuad with scaling won't have the surfaces
// root RenderPass merged with the RenderPass that embeds it. This ensures the
// reflected pixels can be scaled with AA enabled.
TEST_F(SurfaceAggregatorValidSurfaceTest, ReflectedSurfaceDrawQuadScaled) {
  // Submit a CompositorFrame for the primary display. This will get mirrored
  // by the second display through surface embedding.
  const gfx::Rect display_rect(0, 0, 100, 100);
  {
    CompositorFrame frame =
        CompositorFrameBuilder()
            .AddRenderPass(RenderPassBuilder(display_rect)
                               .AddSolidColorQuad(display_rect, SkColors::kRed))
            .Build();
    root_sink_->SubmitCompositorFrame(root_surface_id_.local_surface_id(),
                                      std::move(frame));
  }

  auto mirror_display_sink = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryFrameSinkId1, true);

  TestSurfaceIdAllocator mirror_display_surface_id(
      mirror_display_sink->frame_sink_id());

  // The mirroring display size is smaller than the primary display. The
  // mirrored content would be scaled to fit.
  const gfx::Rect mirror_display_rect(80, 80);
  gfx::Transform scale_transform;
  scale_transform.Scale(0.8, 0.8);

  {
    CompositorFrame frame =
        CompositorFrameBuilder()
            .AddRenderPass(RenderPassBuilder(mirror_display_rect)
                               .AddSurfaceQuad(
                                   display_rect, SurfaceRange(root_surface_id_),
                                   {.stretch_content_to_fill_bounds = true,
                                    .is_reflection = true})
                               .SetQuadToTargetTransform(scale_transform))
            .Build();
    mirror_display_sink->SubmitCompositorFrame(
        mirror_display_surface_id.local_surface_id(), std::move(frame));
  }

  auto frame = AggregateFrame(mirror_display_surface_id);

  // The reflected surface should be a separate RenderPass as it's scaled. The
  // root RenderPass should have a single CompositorRenderPassDrawQuad.
  EXPECT_EQ(2u, frame.render_pass_list.size());

  auto* root_render_pass = frame.render_pass_list.back().get();
  EXPECT_THAT(root_render_pass->quad_list,
              ElementsAre(IsAggregatedRenderPassQuad()));

  // The CompositorRenderPassDrawQuad should have the same scale transform that
  // was applied to the SurfaceDrawQuad.
  auto* output_quad = root_render_pass->quad_list.back();
  EXPECT_EQ(output_quad->shared_quad_state->quad_to_target_transform,
            scale_transform);
}

// Verify that a reflected SurfaceDrawQuad with no scaling has the surfaces root
// RenderPass merged with the RenderPass that embeds it.
TEST_F(SurfaceAggregatorValidSurfaceTest, ReflectedSurfaceDrawQuadNotScaled) {
  // Submit a CompositorFrame for the primary display. This will get mirrored
  // by the second display through surface embedding.
  const gfx::Rect display_rect(0, 0, 100, 100);
  {
    CompositorFrame frame =
        CompositorFrameBuilder()
            .AddRenderPass(RenderPassBuilder(display_rect)
                               .AddSolidColorQuad(display_rect, SkColors::kRed))
            .Build();
    root_sink_->SubmitCompositorFrame(root_surface_id_.local_surface_id(),
                                      std::move(frame));
  }

  auto mirror_display_sink = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryFrameSinkId1, true);

  TestSurfaceIdAllocator mirror_display_surface_id(
      mirror_display_sink->frame_sink_id());

  // The mirroring display is the same width but different height. The mirrored
  // content would be letterboxed by translating it.
  const gfx::Rect mirror_display_rect(120, 100);
  gfx::Transform translate_transform;
  translate_transform.Translate(10, 0);

  {
    CompositorFrame frame =
        CompositorFrameBuilder()
            .AddRenderPass(RenderPassBuilder(mirror_display_rect)
                               .AddSurfaceQuad(
                                   display_rect,
                                   SurfaceRange(std::nullopt, root_surface_id_),
                                   {.stretch_content_to_fill_bounds = true,
                                    .is_reflection = true})
                               .SetQuadToTargetTransform(translate_transform))
            .Build();
    mirror_display_sink->SubmitCompositorFrame(
        mirror_display_surface_id.local_surface_id(), std::move(frame));
  }

  auto frame = AggregateFrame(mirror_display_surface_id);

  // The reflected surfaces RenderPass should be merged into the root RenderPass
  // since it's not being scaled.
  EXPECT_EQ(1u, frame.render_pass_list.size());

  auto* root_render_pass = frame.render_pass_list.back().get();

  // The quad from the embedded surface merged into the root RenderPass should
  // have the same translate transform that was applied to the SurfaceDrawQuad.
  EXPECT_THAT(root_render_pass->quad_list,
              ElementsAre(AllOf(IsSolidColorQuad(),
                                HasTransform(translate_transform))));
}

// This test verifies that in the presence of both primary Surface and fallback
// Surface, the fallback will not be used.
TEST_F(SurfaceAggregatorValidSurfaceTest, FallbackSurfaceReferenceWithPrimary) {
  auto primary_child_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryFrameSinkId1, /*is_root=*/false);
  TestSurfaceIdAllocator primary_child_surface_id(
      primary_child_support->frame_sink_id());
  std::vector<Quad> primary_child_quads = {
      Quad::SolidColorQuad(SkColors::kGreen, gfx::Rect(kSurfaceSize))};
  constexpr gfx::Size primary_size(50, 50);
  std::vector<Pass> primary_child_passes = {
      Pass(primary_child_quads, primary_size)};

  // Submit a CompositorFrame to the primary Surface containing a green
  // SolidColorDrawQuad.
  constexpr float device_scale_factor = 1.0f;
  SubmitCompositorFrame(primary_child_support.get(), primary_child_passes,
                        primary_child_surface_id.local_surface_id(),
                        device_scale_factor);

  auto fallback_child_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryFrameSinkId2, /*is_root=*/false);
  TestSurfaceIdAllocator fallback_child_surface_id(
      fallback_child_support->frame_sink_id());

  std::vector<Quad> fallback_child_quads = {
      Quad::SolidColorQuad(SkColors::kRed, gfx::Rect(kSurfaceSize))};
  std::vector<Pass> fallback_child_passes = {
      Pass(fallback_child_quads, kSurfaceSize)};

  // Submit a CompositorFrame to the fallback Surface containing a red
  // SolidColorDrawQuad.
  SubmitCompositorFrame(fallback_child_support.get(), fallback_child_passes,
                        fallback_child_surface_id.local_surface_id(),
                        device_scale_factor);

  // Try to embed |primary_child_surface_id| and if unavailabe, embed
  // |fallback_child_surface_id|.
  std::vector<Quad> root_quads = {Quad::SurfaceQuad(
      SurfaceRange(fallback_child_surface_id, primary_child_surface_id),
      SkColors::kWhite, gfx::Rect(kSurfaceSize),
      /*stretch_content_to_fill_bounds=*/false)};
  constexpr gfx::Size root_size(75, 75);
  std::vector<Pass> root_passes = {Pass(root_quads, root_size, kEmptyDamage)};

  MockAggregatedDamageCallback aggregated_damage_callback;
  root_sink_->SetAggregatedDamageCallbackForTesting(
      aggregated_damage_callback.GetCallback());

  SubmitCompositorFrame(root_sink_.get(), root_passes,
                        root_surface_id_.local_surface_id(),
                        device_scale_factor);

  // The CompositorFrame is submitted to |primary_child_surface_id|, so
  // |fallback_child_surface_id| will not be used and we should see a green
  // SolidColorDrawQuad.
  EXPECT_CALL(aggregated_damage_callback,
              OnAggregatedDamage(root_surface_id_.local_surface_id(), root_size,
                                 gfx::Rect(root_size), next_display_time()));

  auto aggregated_frame = AggregateFrame(root_surface_id_);

  ASSERT_EQ(aggregated_frame.render_pass_list.size(), 1u);
  EXPECT_THAT(aggregated_frame.render_pass_list[0]->quad_list,
              ElementsAre(IsSolidColorQuad(SkColors::kGreen)));

  // The fallback will not be contained within the aggregated frame.
  VerifyExpectedSurfaceIds({root_surface_id_, primary_child_surface_id});

  testing::Mock::VerifyAndClearExpectations(&aggregated_damage_callback);

  // Submit a new frame to the primary surface to cause some damage.
  SubmitCompositorFrame(primary_child_support.get(), primary_child_passes,
                        primary_child_surface_id.local_surface_id(),
                        device_scale_factor);

  // The size of the damage should be equal to the size of the primary surface.
  EXPECT_CALL(aggregated_damage_callback,
              OnAggregatedDamage(root_surface_id_.local_surface_id(), root_size,
                                 gfx::Rect(primary_size), next_display_time()));

  // Generate a new aggregated frame.
  aggregated_frame = AggregateFrame(root_surface_id_);

  ASSERT_EQ(aggregated_frame.render_pass_list.size(), 1u);
  EXPECT_THAT(aggregated_frame.render_pass_list[0]->quad_list,
              ElementsAre(IsSolidColorQuad(SkColors::kGreen)));

  // The fallback will not be contained within the aggregated frame.
  VerifyExpectedSurfaceIds({root_surface_id_, primary_child_surface_id});

  testing::Mock::VerifyAndClearExpectations(&aggregated_damage_callback);
}

TEST_F(SurfaceAggregatorValidSurfaceTest, CopyRequest) {
  auto embedded_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryFrameSinkId1, /*is_root=*/false);
  TestSurfaceIdAllocator embedded_surface_id(embedded_support->frame_sink_id());

  CompositorFrame embedded_frame =
      CompositorFrameBuilder()
          .AddRenderPass(
              RenderPassBuilder(CompositorRenderPassId{1}, kSurfaceSize)
                  .AddSolidColorQuad(gfx::Rect(5, 5), SkColors::kGreen))
          .Build();
  embedded_support->SubmitCompositorFrame(
      embedded_surface_id.local_surface_id(), std::move(embedded_frame));

  auto copy_request = CopyOutputRequest::CreateStubForTesting();
  auto* copy_request_ptr = copy_request.get();
  embedded_support->RequestCopyOfOutput({embedded_surface_id.local_surface_id(),
                                         SubtreeCaptureId(),
                                         std::move(copy_request)});

  CompositorFrame root_frame =
      CompositorFrameBuilder()
          .AddRenderPass(
              RenderPassBuilder(CompositorRenderPassId{1}, kSurfaceSize)
                  .AddSolidColorQuad(gfx::Rect(5, 5), SkColors::kWhite)
                  .AddSurfaceQuad(gfx::Rect(kSurfaceSize),
                                  SurfaceRange(embedded_surface_id))
                  .AddSolidColorQuad(gfx::Rect(5, 5), SkColors::kBlack)
                  .Build())
          .Build();
  root_sink_->SubmitCompositorFrame(root_surface_id_.local_surface_id(),
                                    std::move(root_frame));

  auto aggregated_frame = AggregateFrame(root_surface_id_);

  std::vector<Quad> embedded_quads = {
      Quad::SolidColorQuad(SkColors::kGreen, gfx::Rect(5, 5))};
  std::vector<Quad> root_quads = {
      Quad::SolidColorQuad(SkColors::kWhite, gfx::Rect(5, 5)),
      Quad::RenderPassQuad(CompositorRenderPassId{uint64_t{
                               aggregated_frame.render_pass_list[0]->id}},
                           gfx::Transform(), true),
      Quad::SolidColorQuad(SkColors::kBlack, gfx::Rect(5, 5))};
  std::vector<Pass> expected_passes = {Pass(embedded_quads, kSurfaceSize),
                                       Pass(root_quads, kSurfaceSize)};
  TestPassesMatchExpectations(expected_passes,
                              &aggregated_frame.render_pass_list);
  EXPECT_TRUE(aggregated_frame.has_copy_requests);
  ASSERT_EQ(2u, aggregated_frame.render_pass_list.size());
  ASSERT_EQ(1u, aggregated_frame.render_pass_list[0]->copy_requests.size());
  DCHECK_EQ(copy_request_ptr,
            aggregated_frame.render_pass_list[0]->copy_requests[0].get());

  VerifyExpectedSurfaceIds({root_surface_id_, embedded_surface_id});
}

TEST_F(SurfaceAggregatorValidSurfaceTest,
       ShouldNotTakeCopyRequestIfTakeCopyRequestIsFalse) {
  auto embedded_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryFrameSinkId1, /*is_root=*/false);
  TestSurfaceIdAllocator embedded_surface_id(embedded_support->frame_sink_id());

  CompositorFrame embedded_frame =
      CompositorFrameBuilder()
          .AddRenderPass(
              RenderPassBuilder(CompositorRenderPassId{1}, kSurfaceSize)
                  .AddSolidColorQuad(gfx::Rect(5, 5), SkColors::kGreen)
                  .Build())
          .Build();
  embedded_support->SubmitCompositorFrame(
      embedded_surface_id.local_surface_id(), std::move(embedded_frame));

  auto copy_request = CopyOutputRequest::CreateStubForTesting();
  embedded_support->RequestCopyOfOutput({embedded_surface_id.local_surface_id(),
                                         SubtreeCaptureId(),
                                         std::move(copy_request)});

  CompositorFrame root_frame =
      CompositorFrameBuilder()
          .AddRenderPass(
              RenderPassBuilder(CompositorRenderPassId{1}, kSurfaceSize)
                  .AddSurfaceQuad(gfx::Rect(kSurfaceSize),
                                  SurfaceRange(embedded_surface_id))
                  .Build())
          .Build();
  root_sink_->SubmitCompositorFrame(root_surface_id_.local_surface_id(),
                                    std::move(root_frame));

  aggregator_.set_take_copy_requests(false);
  auto aggregated_frame = AggregateFrame(root_surface_id_);

  // There should be no copy requests on the aggregated_frame.
  EXPECT_FALSE(aggregated_frame.has_copy_requests);
  ASSERT_EQ(1u, aggregated_frame.render_pass_list.size());
  ASSERT_EQ(0u, aggregated_frame.render_pass_list[0]->copy_requests.size());
}

// Check that a copy request does not prevent protected quads from being
// displayed. Protected quads must merge to the root render pass to be
// considered for overlay promotion, which is required for their display.
TEST_F(SurfaceAggregatorValidSurfaceTest,
       CopyRequestOnEmbeddedSurfaceWithProtectedQuads) {
  auto embedded_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryFrameSinkId2, /*is_root=*/false);
  TestSurfaceIdAllocator embedded_surface_id(embedded_support->frame_sink_id());

  {
    CompositorFrame frame =
        CompositorFrameBuilder()
            .AddRenderPass(
                RenderPassBuilder(kSurfaceSize)
                    .AddTextureQuad(
                        gfx::Rect(20, 20), ResourceId(1),
                        TextureQuadParams{
                            .protected_video_type =
                                gfx::ProtectedVideoType::kHardwareProtected})
                    .AddSolidColorQuad(gfx::Rect(kSurfaceSize),
                                       SkColors::kGreen))
            .Build();
    PopulateTransferableResources(frame);

    embedded_support->SubmitCompositorFrame(
        embedded_surface_id.local_surface_id(), std::move(frame));

    auto copy_request = CopyOutputRequest::CreateStubForTesting();
    embedded_support->RequestCopyOfOutput(
        {embedded_surface_id.local_surface_id(), SubtreeCaptureId(),
         std::move(copy_request)});
  }

  {
    CompositorFrame frame =
        CompositorFrameBuilder()
            .AddRenderPass(
                RenderPassBuilder(CompositorRenderPassId{1}, kSurfaceSize)
                    .AddSurfaceQuad(
                        gfx::Rect(kSurfaceSize),
                        SurfaceRange(embedded_surface_id),
                        {.default_background_color = SkColors::kYellow}))
            .Build();

    root_sink_->SubmitCompositorFrame(root_surface_id_.local_surface_id(),
                                      std::move(frame));
  }

  auto aggregated_frame = AggregateFrame(root_surface_id_);

  VerifyExpectedSurfaceIds({root_surface_id_, embedded_surface_id});

  EXPECT_TRUE(aggregated_frame.has_copy_requests);

  // We expect two render passes:
  //   - embedded surface's root pass with the copy request.
  //   - root pass with the embedded surface's quads merged, no copy requests.
  auto& render_pass_list = aggregated_frame.render_pass_list;
  ASSERT_EQ(2u, render_pass_list.size());

  // Embedded surface
  EXPECT_THAT(render_pass_list[0]->quad_list,
              ElementsAre(IsTextureQuad(), IsSolidColorQuad(SkColors::kGreen)));
  EXPECT_THAT(render_pass_list[0]->copy_requests, testing::SizeIs(1u));

  // Root pass
  EXPECT_THAT(render_pass_list[1]->quad_list,
              ElementsAre(IsTextureQuad(), IsSolidColorQuad(SkColors::kGreen)));
  EXPECT_THAT(render_pass_list[1]->copy_requests, testing::IsEmpty());

  // Ensure copy requests have been removed from the embedded surface.
  const CompositorFrame& original_frame =
      manager_.surface_manager()
          ->GetSurfaceForId(embedded_surface_id)
          ->GetActiveFrame();
  const auto& original_pass_list = original_frame.render_pass_list;
  ASSERT_EQ(1u, original_pass_list.size());
  EXPECT_THAT(original_pass_list[0]->copy_requests, testing::IsEmpty());
}

// This is the same test as CopyRequestOnEmbeddedSurfaceWithProtectedQuads, but
// ensures that we can still merge the render pass with the copy request even if
// it does not directly contain the protected quad and transitively embeds it.
TEST_F(SurfaceAggregatorValidSurfaceTest,
       CopyRequestOnSurfaceEmbeddingSurfaceWithProtectedQuads) {
  auto video_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryFrameSinkId2, /*is_root=*/false);
  TestSurfaceIdAllocator video_surface_id(video_support->frame_sink_id());

  auto embedded_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryFrameSinkId3, /*is_root=*/false);
  TestSurfaceIdAllocator embedded_surface_id(embedded_support->frame_sink_id());

  {
    CompositorFrame frame =
        CompositorFrameBuilder()
            .AddRenderPass(
                RenderPassBuilder(kSurfaceSize)
                    .AddTextureQuad(
                        gfx::Rect(20, 20), ResourceId(1),
                        TextureQuadParams{
                            .protected_video_type =
                                gfx::ProtectedVideoType::kHardwareProtected})
                    .AddSolidColorQuad(gfx::Rect(kSurfaceSize),
                                       SkColors::kGreen))
            .Build();
    PopulateTransferableResources(frame);

    video_support->SubmitCompositorFrame(video_surface_id.local_surface_id(),
                                         std::move(frame));
  }

  {
    CompositorFrame frame =
        CompositorFrameBuilder()
            .AddRenderPass(
                RenderPassBuilder(kSurfaceSize)
                    .AddSurfaceQuad(
                        gfx::Rect(kSurfaceSize), SurfaceRange(video_surface_id),
                        {.default_background_color = SkColors::kYellow}))
            .Build();

    embedded_support->SubmitCompositorFrame(
        embedded_surface_id.local_surface_id(), std::move(frame));

    auto copy_request = CopyOutputRequest::CreateStubForTesting();
    embedded_support->RequestCopyOfOutput(
        {embedded_surface_id.local_surface_id(), SubtreeCaptureId(),
         std::move(copy_request)});
  }

  {
    CompositorFrame frame =
        CompositorFrameBuilder()
            .AddRenderPass(
                RenderPassBuilder(CompositorRenderPassId{1}, kSurfaceSize)
                    .AddSurfaceQuad(
                        gfx::Rect(kSurfaceSize),
                        SurfaceRange(embedded_surface_id),
                        {.default_background_color = SkColors::kYellow}))
            .Build();

    root_sink_->SubmitCompositorFrame(root_surface_id_.local_surface_id(),
                                      std::move(frame));
  }

  auto aggregated_frame = AggregateFrame(root_surface_id_);

  VerifyExpectedSurfaceIds(
      {root_surface_id_, embedded_surface_id, video_surface_id});

  EXPECT_TRUE(aggregated_frame.has_copy_requests);

  // We expect two render passes:
  //   - embedded surface with the video surface merged and a copy request.
  //   - root pass with everything merged, no copy requests.
  auto& render_pass_list = aggregated_frame.render_pass_list;
  ASSERT_EQ(2u, render_pass_list.size());

  // Embedded surface pass, with the video surface merged
  EXPECT_THAT(render_pass_list[0]->quad_list,
              ElementsAre(IsTextureQuad(), IsSolidColorQuad(SkColors::kGreen)));
  EXPECT_THAT(render_pass_list[0]->copy_requests, testing::SizeIs(1u));

  // Root pass, with everything merged
  EXPECT_THAT(render_pass_list[1]->quad_list,
              ElementsAre(IsTextureQuad(), IsSolidColorQuad(SkColors::kGreen)));
  EXPECT_THAT(render_pass_list[1]->copy_requests, testing::IsEmpty());

  // Ensure copy requests have been removed from the embedded surface.
  const CompositorFrame& original_frame =
      manager_.surface_manager()
          ->GetSurfaceForId(root_surface_id_)
          ->GetActiveFrame();
  const auto& original_pass_list = original_frame.render_pass_list;
  ASSERT_EQ(1u, original_pass_list.size());
  EXPECT_THAT(original_pass_list[0]->copy_requests, testing::IsEmpty());
}

// Root surface may contain copy requests.
TEST_F(SurfaceAggregatorValidSurfaceTest, RootCopyRequest) {
  constexpr gfx::Rect quad_rect(5, 5);

  auto embedded_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryFrameSinkId2, /*is_root=*/false);
  TestSurfaceIdAllocator embedded_surface_id(embedded_support->frame_sink_id());

  {
    CompositorFrame frame =
        CompositorFrameBuilder()
            .AddRenderPass(RenderPassBuilder(kSurfaceSize)
                               .AddSolidColorQuad(quad_rect, SkColors::kGreen))
            .Build();

    embedded_support->SubmitCompositorFrame(
        embedded_surface_id.local_surface_id(), std::move(frame));
  }

  base::WeakPtr<CopyOutputRequest> child_copy_request_ptr;
  base::WeakPtr<CopyOutputRequest> root_copy_request_ptr;

  {
    CompositorFrame frame =
        CompositorFrameBuilder()
            .AddRenderPass(
                RenderPassBuilder(CompositorRenderPassId{1}, kSurfaceSize)
                    .AddStubCopyOutputRequest(&child_copy_request_ptr)
                    .AddSolidColorQuad(quad_rect, SkColors::kWhite)
                    .AddSurfaceQuad(
                        quad_rect, SurfaceRange(embedded_surface_id),
                        {.default_background_color = SkColors::kYellow})
                    .AddSolidColorQuad(quad_rect, SkColors::kBlack))
            .AddRenderPass(
                RenderPassBuilder(CompositorRenderPassId{2}, kSurfaceSize)
                    .AddStubCopyOutputRequest(&root_copy_request_ptr)
                    .AddSolidColorQuad(quad_rect, SkColors::kRed))
            .Build();

    root_sink_->SubmitCompositorFrame(root_surface_id_.local_surface_id(),
                                      std::move(frame));
  }

  auto aggregated_frame = AggregateFrame(root_surface_id_);
  EXPECT_TRUE(aggregated_frame.has_copy_requests);

  auto& render_pass_list = aggregated_frame.render_pass_list;
  ASSERT_EQ(2u, render_pass_list.size());

  EXPECT_THAT(render_pass_list[0]->quad_list,
              ElementsAre(IsSolidColorQuad(SkColors::kWhite),
                          IsSolidColorQuad(SkColors::kGreen),
                          IsSolidColorQuad(SkColors::kBlack)));
  EXPECT_THAT(render_pass_list[0]->copy_requests,
              ElementsAre(testing::Pointer(child_copy_request_ptr.get())));

  EXPECT_THAT(render_pass_list[1]->quad_list,
              ElementsAre(IsSolidColorQuad(SkColors::kRed)));
  EXPECT_THAT(render_pass_list[1]->copy_requests,
              ElementsAre(testing::Pointer(root_copy_request_ptr.get())));

  VerifyExpectedSurfaceIds({root_surface_id_, embedded_surface_id});

  // Ensure copy requests have been removed from root surface.
  const CompositorFrame& original_frame =
      manager_.surface_manager()
          ->GetSurfaceForId(root_surface_id_)
          ->GetActiveFrame();
  const auto& original_pass_list = original_frame.render_pass_list;
  ASSERT_EQ(2u, original_pass_list.size());
  EXPECT_THAT(original_pass_list[0]->copy_requests, testing::IsEmpty());
  EXPECT_THAT(original_pass_list[1]->copy_requests, testing::IsEmpty());
}

TEST_F(SurfaceAggregatorValidSurfaceTest,
       ShouldNotTakeRootCopyRequestIfTakeCopyRequestIsFalse) {
  auto embedded_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryFrameSinkId2, /*is_root=*/false);
  TestSurfaceIdAllocator embedded_surface_id(embedded_support->frame_sink_id());

  CompositorFrame root_frame =
      CompositorFrameBuilder()
          .AddRenderPass(
              RenderPassBuilder(CompositorRenderPassId{1}, kSurfaceSize)
                  .AddSolidColorQuad(gfx::Rect(5, 5), SkColors::kWhite)
                  .Build())
          .Build();
  root_sink_->SubmitCompositorFrame(root_surface_id_.local_surface_id(),
                                    std::move(root_frame));

  auto copy_request = CopyOutputRequest::CreateStubForTesting();
  auto* copy_request_ptr = copy_request.get();
  root_sink_->RequestCopyOfOutput({root_surface_id_.local_surface_id(),
                                   SubtreeCaptureId(),
                                   std::move(copy_request)});

  aggregator_.set_take_copy_requests(false);
  auto aggregated_frame = AggregateFrame(root_surface_id_);

  // Ensure no copy requests are added to the aggregated frame.
  EXPECT_FALSE(aggregated_frame.has_copy_requests);
  ASSERT_EQ(1u, aggregated_frame.render_pass_list.size());
  ASSERT_EQ(0u, aggregated_frame.render_pass_list[0]->copy_requests.size());

  // Ensure copy request remains on the root surface.
  const CompositorFrame& original_frame =
      manager_.surface_manager()
          ->GetSurfaceForId(root_surface_id_)
          ->GetActiveFrame();
  const auto& original_pass_list = original_frame.render_pass_list;
  ASSERT_EQ(1u, original_pass_list.size());
  ASSERT_EQ(1u, original_pass_list[0]->copy_requests.size());
  EXPECT_EQ(copy_request_ptr,
            original_frame.render_pass_list[0]->copy_requests[0].get());
}

TEST_F(SurfaceAggregatorValidSurfaceTest, VideoCapturePreventsMerge) {
  auto embedded_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryFrameSinkId1, /*is_root=*/false);
  TestSurfaceIdAllocator embedded_surface_id(embedded_support->frame_sink_id());

  CompositorFrame embedded_frame =
      CompositorFrameBuilder()
          .AddRenderPass(
              RenderPassBuilder(CompositorRenderPassId{1}, kSurfaceSize)
                  .AddSolidColorQuad(gfx::Rect(5, 5), SkColors::kGreen))
          .Build();
  embedded_support->SubmitCompositorFrame(
      embedded_surface_id.local_surface_id(), std::move(embedded_frame));

  CompositorFrame root_frame =
      CompositorFrameBuilder()
          .AddRenderPass(
              RenderPassBuilder(CompositorRenderPassId{1}, kSurfaceSize)
                  .AddSurfaceQuad(gfx::Rect(kSurfaceSize),
                                  SurfaceRange(embedded_surface_id))
                  .Build())
          .Build();
  root_sink_->SubmitCompositorFrame(root_surface_id_.local_surface_id(),
                                    std::move(root_frame));

  // Frame #1: Video capture is enabled with a copy request
  {
    embedded_support->OnClientCaptureStarted();

    auto copy_request = CopyOutputRequest::CreateStubForTesting();
    auto* copy_request_ptr = copy_request.get();
    embedded_support->RequestCopyOfOutput(
        {embedded_surface_id.local_surface_id(), SubtreeCaptureId(),
         std::move(copy_request)});

    auto aggregated_frame = AggregateFrame(root_surface_id_);

    // We expect the child pass to remain unmerged due to a copy request.
    ASSERT_EQ(2u, aggregated_frame.render_pass_list.size());

    EXPECT_TRUE(aggregated_frame.render_pass_list[0]->video_capture_enabled);

    // We don't expect video capture on the root pass, only the embedded pass.
    EXPECT_FALSE(
        aggregated_frame.render_pass_list.back()->video_capture_enabled);

    ASSERT_EQ(1u, aggregated_frame.render_pass_list[0]->copy_requests.size());
    EXPECT_EQ(copy_request_ptr,
              aggregated_frame.render_pass_list[0]->copy_requests[0].get());
  }

  // Frame #2: Video capture is still enabled, but no copy requests
  {
    auto aggregated_frame = AggregateFrame(root_surface_id_);

    // We expect the child pass to remain unmerged due to a copy request.
    ASSERT_EQ(2u, aggregated_frame.render_pass_list.size());

    EXPECT_TRUE(aggregated_frame.render_pass_list[0]->video_capture_enabled);
    EXPECT_FALSE(
        aggregated_frame.render_pass_list.back()->video_capture_enabled);

    EXPECT_TRUE(aggregated_frame.render_pass_list[0]->copy_requests.empty());
  }

  // Frame #3: Video capture is disabled
  {
    embedded_support->OnClientCaptureStopped();

    auto aggregated_frame = AggregateFrame(root_surface_id_);

    // No more video capture, so we expect the pass to merge.
    ASSERT_EQ(1u, aggregated_frame.render_pass_list.size());

    EXPECT_FALSE(
        aggregated_frame.render_pass_list.back()->video_capture_enabled);
  }
}

TEST_F(SurfaceAggregatorValidSurfaceTest, UnreferencedSurface) {
  auto embedded_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryFrameSinkId1, /*is_root=*/false);
  auto parent_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryFrameSinkId2, /*is_root=*/true);
  TestSurfaceIdAllocator embedded_surface_id(embedded_support->frame_sink_id());
  TestSurfaceIdAllocator nonexistent_surface_id(root_sink_->frame_sink_id());

  std::vector<Quad> embedded_quads = {
      Quad::SolidColorQuad(SkColors::kGreen, gfx::Rect(5, 5))};
  std::vector<Pass> embedded_passes = {Pass(embedded_quads, kSurfaceSize)};

  constexpr float device_scale_factor = 1.0f;
  SubmitCompositorFrame(embedded_support.get(), embedded_passes,
                        embedded_surface_id.local_surface_id(),
                        device_scale_factor);
  auto copy_request(CopyOutputRequest::CreateStubForTesting());
  auto* copy_request_ptr = copy_request.get();
  embedded_support->RequestCopyOfOutput({embedded_surface_id.local_surface_id(),
                                         SubtreeCaptureId(),
                                         std::move(copy_request)});

  TestSurfaceIdAllocator parent_surface_id(parent_support->frame_sink_id());

  std::vector<Quad> parent_quads = {
      Quad::SolidColorQuad(SkColors::kGray, gfx::Rect(5, 5)),
      Quad::SurfaceQuad(SurfaceRange(std::nullopt, embedded_surface_id),
                        SkColors::kWhite, gfx::Rect(5, 5),
                        /*stretch_content_to_fill_bounds=*/false),
      Quad::SolidColorQuad(SkColors::kLtGray, gfx::Rect(5, 5))};
  std::vector<Pass> parent_passes = {Pass(parent_quads, kSurfaceSize)};

  {
    CompositorFrame frame = MakeEmptyCompositorFrame();

    AddPasses(&frame.render_pass_list, parent_passes,
              &frame.metadata.referenced_surfaces);

    frame.metadata.referenced_surfaces.emplace_back(embedded_surface_id);

    parent_support->SubmitCompositorFrame(parent_surface_id.local_surface_id(),
                                          std::move(frame));
  }

  std::vector<Quad> root_quads = {
      Quad::SolidColorQuad(SkColors::kWhite, gfx::Rect(5, 5)),
      Quad::SolidColorQuad(SkColors::kBlack, gfx::Rect(5, 5))};
  std::vector<Pass> root_passes = {Pass(root_quads, kSurfaceSize)};

  {
    CompositorFrame frame = MakeEmptyCompositorFrame();
    AddPasses(&frame.render_pass_list, root_passes,
              &frame.metadata.referenced_surfaces);

    frame.metadata.referenced_surfaces.emplace_back(parent_surface_id);
    // Reference to Surface ID of a Surface that doesn't exist should be
    // included in previous_contained_surfaces, but otherwise ignored.
    frame.metadata.referenced_surfaces.emplace_back(nonexistent_surface_id);

    root_sink_->SubmitCompositorFrame(root_surface_id_.local_surface_id(),
                                      std::move(frame));
  }

  auto aggregated_frame = AggregateFrame(root_surface_id_);

  // First pass should come from surface that had a copy request but was not
  // referenced directly. The second pass comes from the root surface.
  // parent_quad should be ignored because it is neither referenced through a
  // SurfaceDrawQuad nor has a copy request on it.
  std::vector<Pass> expected_passes = {Pass(embedded_quads, kSurfaceSize),
                                       Pass(root_quads, kSurfaceSize)};
  TestPassesMatchExpectations(expected_passes,
                              &aggregated_frame.render_pass_list);
  EXPECT_TRUE(aggregated_frame.has_copy_requests);
  ASSERT_EQ(2u, aggregated_frame.render_pass_list.size());
  ASSERT_EQ(1u, aggregated_frame.render_pass_list[0]->copy_requests.size());
  DCHECK_EQ(copy_request_ptr,
            aggregated_frame.render_pass_list[0]->copy_requests[0].get());

  VerifyExpectedSurfaceIds(
      {root_surface_id_, parent_surface_id, embedded_surface_id});
}

// This tests referencing a surface that has multiple render passes.
TEST_F(SurfaceAggregatorValidSurfaceTest, MultiPassSurfaceReference) {
  TestSurfaceIdAllocator embedded_surface_id(child_sink_->frame_sink_id());

  CompositorRenderPassId pass_ids[] = {CompositorRenderPassId{1},
                                       CompositorRenderPassId{2},
                                       CompositorRenderPassId{3}};

  std::vector<Quad> embedded_quads[3] = {
      {Quad::SolidColorQuad({1.0, 0.0, 1.0, 1.0f / 255.0f}, gfx::Rect(5, 5)),
       Quad::SolidColorQuad({1.0, 0.0, 1.0, 2.0f / 255.0f}, gfx::Rect(5, 5))},
      {Quad::SolidColorQuad({1.0, 0.0, 1.0, 3.0f / 255.0f}, gfx::Rect(5, 5)),
       Quad::RenderPassQuad(pass_ids[0], gfx::Transform(), true)},
      {Quad::SolidColorQuad({1.0, 0.0, 1.0, 4.0f / 255.0f}, gfx::Rect(5, 5)),
       Quad::RenderPassQuad(pass_ids[1], gfx::Transform(), true)}};
  std::vector<Pass> embedded_passes = {
      Pass(embedded_quads[0], pass_ids[0], kSurfaceSize),
      Pass(embedded_quads[1], pass_ids[1], kSurfaceSize),
      Pass(embedded_quads[2], pass_ids[2], kSurfaceSize)};

  constexpr float device_scale_factor = 1.0f;
  SubmitCompositorFrame(child_sink_.get(), embedded_passes,
                        embedded_surface_id.local_surface_id(),
                        device_scale_factor);

  std::vector<Quad> root_quads[3] = {
      {Quad::SolidColorQuad({1.0, 0.0, 1.0, 5.0f / 255.0f}, gfx::Rect(5, 5)),
       Quad::SolidColorQuad({1.0, 0.0, 1.0, 6.0f / 255.0f}, gfx::Rect(5, 5))},
      {Quad::SurfaceQuad(SurfaceRange(std::nullopt, embedded_surface_id),
                         SkColors::kWhite, gfx::Rect(5, 5),
                         /*stretch_content_to_fill_bounds=*/false),
       Quad::RenderPassQuad(pass_ids[0], gfx::Transform(), true)},
      {Quad::SolidColorQuad({1.0, 0.0, 1.0, 7.0f / 255.0f}, gfx::Rect(5, 5)),
       Quad::RenderPassQuad(pass_ids[1], gfx::Transform(), true)}};
  std::vector<Pass> root_passes = {
      Pass(root_quads[0], pass_ids[0], kSurfaceSize),
      Pass(root_quads[1], pass_ids[1], kSurfaceSize),
      Pass(root_quads[2], pass_ids[2], kSurfaceSize)};

  SubmitCompositorFrame(root_sink_.get(), root_passes,
                        root_surface_id_.local_surface_id(),
                        device_scale_factor);

  auto aggregated_frame = AggregateFrame(root_surface_id_);

  const auto& aggregated_pass_list = aggregated_frame.render_pass_list;

  ASSERT_EQ(5u, aggregated_pass_list.size());
  AggregatedRenderPassId actual_pass_ids[] = {
      aggregated_pass_list[0]->id, aggregated_pass_list[1]->id,
      aggregated_pass_list[2]->id, aggregated_pass_list[3]->id,
      aggregated_pass_list[4]->id};
  for (size_t i = 0; i < 5; ++i) {
    for (size_t j = 0; j < i; ++j) {
      EXPECT_NE(actual_pass_ids[i], actual_pass_ids[j]);
    }
  }

  {
    SCOPED_TRACE("First pass");
    // The first pass will just be the first pass from the root surfaces quad
    // with no render pass quads to remap.
    TestPassMatchesExpectations(root_passes[0], aggregated_pass_list[0].get());
  }

  {
    SCOPED_TRACE("Second pass");
    // The next two passes will be from the embedded surface since we have to
    // draw those passes before they are referenced from the render pass draw
    // quad embedded into the root surface's second pass.
    // First, there's the first embedded pass which doesn't reference anything
    // else.
    TestPassMatchesExpectations(embedded_passes[0],
                                aggregated_pass_list[1].get());
  }

  {
    SCOPED_TRACE("Third pass");
    const auto& third_pass_quad_list = aggregated_pass_list[2]->quad_list;
    ASSERT_EQ(2u, third_pass_quad_list.size());
    TestQuadMatchesExpectations(embedded_quads[1][0],
                                third_pass_quad_list.ElementAt(0));

    // This render pass pass quad will reference the first pass from the
    // embedded surface, which is the second pass in the aggregated frame.
    ASSERT_EQ(DrawQuad::Material::kAggregatedRenderPass,
              third_pass_quad_list.ElementAt(1)->material);
    const auto* third_pass_render_pass_draw_quad =
        AggregatedRenderPassDrawQuad::MaterialCast(
            third_pass_quad_list.ElementAt(1));
    EXPECT_EQ(actual_pass_ids[1],
              third_pass_render_pass_draw_quad->render_pass_id);
  }

  {
    SCOPED_TRACE("Fourth pass");
    // The fourth pass will have aggregated quads from the root surface's second
    // pass and the embedded surface's first pass.
    const auto& fourth_pass_quad_list = aggregated_pass_list[3]->quad_list;
    ASSERT_EQ(3u, fourth_pass_quad_list.size());

    // The first quad will be the yellow quad from the embedded surface's last
    // pass.
    TestQuadMatchesExpectations(embedded_quads[2][0],
                                fourth_pass_quad_list.ElementAt(0));

    // The next quad will be a render pass quad referencing the second pass from
    // the embedded surface, which is the third pass in the aggregated frame.
    ASSERT_EQ(DrawQuad::Material::kAggregatedRenderPass,
              fourth_pass_quad_list.ElementAt(1)->material);
    const auto* fourth_pass_first_render_pass_draw_quad =
        AggregatedRenderPassDrawQuad::MaterialCast(
            fourth_pass_quad_list.ElementAt(1));
    EXPECT_EQ(actual_pass_ids[2],
              fourth_pass_first_render_pass_draw_quad->render_pass_id);

    // The last quad will be a render pass quad referencing the first pass from
    // the root surface, which is the first pass overall.
    ASSERT_EQ(DrawQuad::Material::kAggregatedRenderPass,
              fourth_pass_quad_list.ElementAt(2)->material);
    const auto* fourth_pass_second_render_pass_draw_quad =
        AggregatedRenderPassDrawQuad::MaterialCast(
            fourth_pass_quad_list.ElementAt(2));
    EXPECT_EQ(actual_pass_ids[0],
              fourth_pass_second_render_pass_draw_quad->render_pass_id);
  }

  {
    SCOPED_TRACE("Fifth pass");
    const auto& fifth_pass_quad_list = aggregated_pass_list[4]->quad_list;
    ASSERT_EQ(2u, fifth_pass_quad_list.size());

    TestQuadMatchesExpectations(root_quads[2][0],
                                fifth_pass_quad_list.ElementAt(0));

    // The last quad in the last pass will reference the second pass from the
    // root surface, which after aggregating is the fourth pass in the overall
    // list.
    ASSERT_EQ(DrawQuad::Material::kAggregatedRenderPass,
              fifth_pass_quad_list.ElementAt(1)->material);
    const auto* fifth_pass_render_pass_draw_quad =
        AggregatedRenderPassDrawQuad::MaterialCast(
            fifth_pass_quad_list.ElementAt(1));
    EXPECT_EQ(actual_pass_ids[3],
              fifth_pass_render_pass_draw_quad->render_pass_id);
  }
}

// Tests an invalid surface reference in a frame. The surface quad should just
// be dropped.
TEST_F(SurfaceAggregatorValidSurfaceTest, InvalidSurfaceReference) {
  const base::UnguessableToken token = base::UnguessableToken::Create();

  std::vector<Quad> quads = {
      Quad::SolidColorQuad(SkColors::kGreen, gfx::Rect(5, 5)),
      Quad::SurfaceQuad(
          SurfaceRange(SurfaceId(
              FrameSinkId(), LocalSurfaceId(0xdeadbeef, 0xdeadbeef, token))),
          SkColors::kWhite, gfx::Rect(5, 5),
          /*stretch_content_to_fill_bounds=*/false),
      Quad::SolidColorQuad(SkColors::kBlue, gfx::Rect(5, 5))};
  std::vector<Pass> passes = {Pass(quads, kSurfaceSize)};

  constexpr float device_scale_factor = 1.0f;
  SubmitCompositorFrame(root_sink_.get(), passes,
                        root_surface_id_.local_surface_id(),
                        device_scale_factor);

  std::vector<Quad> expected_quads = {
      Quad::SolidColorQuad(SkColors::kGreen, gfx::Rect(5, 5)),
      Quad::SolidColorQuad(SkColors::kBlue, gfx::Rect(5, 5))};
  std::vector<Pass> expected_passes = {Pass(expected_quads, kSurfaceSize)};
  AggregateAndVerify(expected_passes, {root_surface_id_});
}

// Tests a reference to a valid surface with no submitted frame. A
// SolidColorDrawQuad should be placed in lieu of a frame.
TEST_F(SurfaceAggregatorValidSurfaceTest, ValidSurfaceReferenceWithNoFrame) {
  TestSurfaceIdAllocator surface_with_no_frame_id(kArbitraryFrameSinkId1);

  std::vector<Quad> quads = {
      Quad::SolidColorQuad(SkColors::kGreen, gfx::Rect(5, 5)),
      Quad::SurfaceQuad(SurfaceRange(std::nullopt, surface_with_no_frame_id),
                        SkColors::kYellow, gfx::Rect(5, 5),
                        /*stretch_content_to_fill_bounds=*/false),
      Quad::SolidColorQuad(SkColors::kBlue, gfx::Rect(5, 5))};
  std::vector<Pass> passes = {Pass(quads, kSurfaceSize)};

  constexpr float device_scale_factor = 1.0f;
  SubmitCompositorFrame(root_sink_.get(), passes,
                        root_surface_id_.local_surface_id(),
                        device_scale_factor);

  std::vector<Quad> expected_quads = {
      Quad::SolidColorQuad(SkColors::kGreen, gfx::Rect(5, 5)),
      Quad::SolidColorQuad(SkColors::kYellow, gfx::Rect(5, 5)),
      Quad::SolidColorQuad(SkColors::kBlue, gfx::Rect(5, 5))};
  std::vector<Pass> expected_passes = {Pass(expected_quads, kSurfaceSize)};
  AggregateAndVerify(expected_passes, {root_surface_id_});
}

// Tests a reference to a valid primary surface and a fallback surface
// with no submitted frame. A SolidColorDrawQuad should be placed in lieu of a
// frame.
TEST_F(SurfaceAggregatorValidSurfaceTest, ValidFallbackWithNoFrame) {
  const TestSurfaceIdAllocator surface_with_no_frame_id(
      root_sink_->frame_sink_id());

  std::vector<Quad> quads = {
      Quad::SurfaceQuad(SurfaceRange(surface_with_no_frame_id),
                        SkColors::kYellow, gfx::Rect(5, 5),
                        /*stretch_content_to_fill_bounds=*/false)};
  std::vector<Pass> passes = {Pass(quads, kSurfaceSize)};

  constexpr float device_scale_factor = 1.0f;
  SubmitCompositorFrame(root_sink_.get(), passes,
                        root_surface_id_.local_surface_id(),
                        device_scale_factor);

  std::vector<Quad> expected_quads = {
      Quad::SolidColorQuad(SkColors::kYellow, gfx::Rect(5, 5)),
  };
  std::vector<Pass> expected_passes = {Pass(expected_quads, kSurfaceSize)};
  AggregateAndVerify(expected_passes, {root_surface_id_});
}

// Tests a surface quad referencing itself, generating a trivial cycle.
// The quad creating the cycle should be dropped from the final frame.
TEST_F(SurfaceAggregatorValidSurfaceTest, SimpleCyclicalReference) {
  std::vector<Quad> quads = {
      Quad::SurfaceQuad(SurfaceRange(std::nullopt, root_surface_id_),
                        SkColors::kWhite, gfx::Rect(5, 5),
                        /*stretch_content_to_fill_bounds=*/false),
      Quad::SolidColorQuad(SkColors::kYellow, gfx::Rect(5, 5))};
  std::vector<Pass> passes = {Pass(quads, kSurfaceSize)};

  constexpr float device_scale_factor = 1.0f;
  SubmitCompositorFrame(root_sink_.get(), passes,
                        root_surface_id_.local_surface_id(),
                        device_scale_factor);

  std::vector<Quad> expected_quads = {
      Quad::SolidColorQuad(SkColors::kYellow, gfx::Rect(5, 5))};
  std::vector<Pass> expected_passes = {Pass(expected_quads, kSurfaceSize)};
  AggregateAndVerify(expected_passes, {root_surface_id_});
}

// Tests a more complex cycle with one intermediate surface.
TEST_F(SurfaceAggregatorValidSurfaceTest, TwoSurfaceCyclicalReference) {
  TestSurfaceIdAllocator child_surface_id(child_sink_->frame_sink_id());

  std::vector<Quad> parent_quads = {
      Quad::SolidColorQuad(SkColors::kBlue, gfx::Rect(5, 5)),
      Quad::SurfaceQuad(SurfaceRange(std::nullopt, child_surface_id),
                        SkColors::kWhite, gfx::Rect(5, 5),
                        /*stretch_content_to_fill_bounds=*/false),
      Quad::SolidColorQuad(SkColors::kCyan, gfx::Rect(5, 5))};
  std::vector<Pass> parent_passes = {Pass(parent_quads, kSurfaceSize)};

  constexpr float device_scale_factor = 1.0f;
  SubmitCompositorFrame(root_sink_.get(), parent_passes,
                        root_surface_id_.local_surface_id(),
                        device_scale_factor);

  std::vector<Quad> child_quads = {
      Quad::SolidColorQuad(SkColors::kGreen, gfx::Rect(5, 5)),
      Quad::SurfaceQuad(SurfaceRange(std::nullopt, root_surface_id_),
                        SkColors::kWhite, gfx::Rect(5, 5),
                        /*stretch_content_to_fill_bounds=*/false),
      Quad::SolidColorQuad(SkColors::kMagenta, gfx::Rect(5, 5))};
  std::vector<Pass> child_passes = {Pass(child_quads, kSurfaceSize)};

  SubmitCompositorFrame(child_sink_.get(), child_passes,
                        child_surface_id.local_surface_id(),
                        device_scale_factor);

  // The child surface's reference to the root_surface_ will be dropped, so
  // we'll end up with:
  //   SkColors::kBlue from the parent
  //   SkColors::kGreen from the child
  //   SkColors::kMagenta from the child
  //   SkColors::kCyan from the parent
  std::vector<Quad> expected_quads = {
      Quad::SolidColorQuad(SkColors::kBlue, gfx::Rect(5, 5)),
      Quad::SolidColorQuad(SkColors::kGreen, gfx::Rect(5, 5)),
      Quad::SolidColorQuad(SkColors::kMagenta, gfx::Rect(5, 5)),
      Quad::SolidColorQuad(SkColors::kCyan, gfx::Rect(5, 5))};
  std::vector<Pass> expected_passes = {Pass(expected_quads, kSurfaceSize)};
  AggregateAndVerify(expected_passes, {root_surface_id_, child_surface_id});
}

// Tests that we map render pass IDs from different surfaces into a unified
// namespace and update CompositorRenderPassDrawQuad's id references to match.
TEST_F(SurfaceAggregatorValidSurfaceTest, RenderPassIdMapping) {
  TestSurfaceIdAllocator child_surface_id(child_sink_->frame_sink_id());

  CompositorRenderPassId child_pass_id[] = {CompositorRenderPassId{1u},
                                            CompositorRenderPassId{2u}};
  std::vector<Quad> child_quad[2] = {
      {Quad::SolidColorQuad(SkColors::kGreen, gfx::Rect(5, 5))},
      {Quad::RenderPassQuad(child_pass_id[0], gfx::Transform(), true)}};
  std::vector<Pass> surface_passes = {
      Pass(child_quad[0], child_pass_id[0], kSurfaceSize),
      Pass(child_quad[1], child_pass_id[1], kSurfaceSize)};

  constexpr float device_scale_factor = 1.0f;
  SubmitCompositorFrame(child_sink_.get(), surface_passes,
                        child_surface_id.local_surface_id(),
                        device_scale_factor);

  // Pass IDs from the parent surface may collide with ones from the child.
  CompositorRenderPassId parent_pass_id[] = {CompositorRenderPassId{3u},
                                             CompositorRenderPassId{2u}};
  std::vector<Quad> parent_quad[2] = {
      {Quad::SurfaceQuad(SurfaceRange(std::nullopt, child_surface_id),
                         SkColors::kWhite, gfx::Rect(5, 5),
                         /*stretch_content_to_fill_bounds=*/false)},
      {Quad::RenderPassQuad(parent_pass_id[0], gfx::Transform(), true)}};
  std::vector<Pass> parent_passes = {
      Pass(parent_quad[0], parent_pass_id[0], kSurfaceSize),
      Pass(parent_quad[1], parent_pass_id[1], kSurfaceSize)};

  SubmitCompositorFrame(root_sink_.get(), parent_passes,
                        root_surface_id_.local_surface_id(),
                        device_scale_factor);

  auto aggregated_frame = AggregateFrame(root_surface_id_);

  const auto& aggregated_pass_list = aggregated_frame.render_pass_list;

  ASSERT_EQ(3u, aggregated_pass_list.size());
  AggregatedRenderPassId actual_pass_ids[] = {aggregated_pass_list[0]->id,
                                              aggregated_pass_list[1]->id,
                                              aggregated_pass_list[2]->id};
  // Make sure the aggregated frame's pass IDs are all unique.
  for (size_t i = 0; i < 3; ++i) {
    for (size_t j = 0; j < i; ++j) {
      EXPECT_NE(actual_pass_ids[j], actual_pass_ids[i])
          << "pass ids " << i << " and " << j;
    }
  }

  // Make sure the render pass quads reference the remapped pass IDs.
  DrawQuad* render_pass_quads[] = {aggregated_pass_list[1]->quad_list.front(),
                                   aggregated_pass_list[2]->quad_list.front()};
  ASSERT_EQ(render_pass_quads[0]->material,
            DrawQuad::Material::kAggregatedRenderPass);
  EXPECT_EQ(actual_pass_ids[0],
            AggregatedRenderPassDrawQuad::MaterialCast(render_pass_quads[0])
                ->render_pass_id);

  ASSERT_EQ(render_pass_quads[1]->material,
            DrawQuad::Material::kAggregatedRenderPass);
  EXPECT_EQ(actual_pass_ids[1],
            AggregatedRenderPassDrawQuad::MaterialCast(render_pass_quads[1])
                ->render_pass_id);
}

// This tests that we update shared quad state pointers correctly within
// aggregated passes.  The shared quad state list on the aggregated pass will
// include the shared quad states from each pass in one list so the quads will
// end up pointed to shared quad state objects at different offsets. This test
// uses the blend_mode value stored on the shared quad state to track the shared
// quad state, but anything saved on the shared quad state would work.
//
// This test has 4 surfaces in the following structure:
// root_surface -> quad with kClear_Mode,
//                 [child_one_surface],
//                 quad with kDstOver_Mode,
//                 [child_two_surface],
//                 quad with kDstIn_Mode
// child_one_surface -> quad with kSrc_Mode,
//                      [grandchild_surface],
//                      quad with kSrcOver_Mode
// child_two_surface -> quad with kSrcIn_Mode
// grandchild_surface -> quad with kDst_Mode
//
// Resulting in the following aggregated pass:
//  quad_root_0       - blend_mode kClear_Mode
//  quad_child_one_0  - blend_mode kSrc_Mode
//  quad_grandchild_0 - blend_mode kDst_Mode
//  quad_child_one_1  - blend_mode kSrcOver_Mode
//  quad_root_1       - blend_mode kDstOver_Mode
//  quad_child_two_0  - blend_mode kSrcIn_Mode
//  quad_root_2       - blend_mode kDstIn_Mode
TEST_F(SurfaceAggregatorValidSurfaceTest, AggregateSharedQuadStateProperties) {
  const SkBlendMode blend_modes[] = {
      SkBlendMode::kClear,    // 0
      SkBlendMode::kSrc,      // 1
      SkBlendMode::kDst,      // 2
      SkBlendMode::kSrcOver,  // 3
      SkBlendMode::kDstOver,  // 4
      SkBlendMode::kSrcIn,    // 5
      SkBlendMode::kDstIn,    // 6
  };
  auto grandchild_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryFrameSinkId1, /*is_root=*/false);
  auto child_one_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryFrameSinkId2, /*is_root=*/false);
  auto child_two_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryFrameSinkId3, /*is_root=*/false);
  TestSurfaceIdAllocator grandchild_surface_id(
      grandchild_support->frame_sink_id());

  constexpr float device_scale_factor = 1.0f;

  auto grandchild_pass =
      RenderPassBuilder(kSurfaceSize)
          .AddSolidColorQuad(gfx::Rect(kSurfaceSize), SkColors::kGreen)
          .SetBlendMode(blend_modes[2])
          .Build();
  QueuePassAsFrame(std::move(grandchild_pass),
                   grandchild_surface_id.local_surface_id(),
                   device_scale_factor, grandchild_support.get());

  TestSurfaceIdAllocator child_one_surface_id(
      child_one_support->frame_sink_id());

  auto child_one_pass =
      RenderPassBuilder(kSurfaceSize)
          .AddSolidColorQuad(gfx::Rect(kSurfaceSize), SkColors::kGreen)
          .SetBlendMode(blend_modes[1])
          .AddSurfaceQuad(gfx::Rect(kSurfaceSize),
                          SurfaceRange(std::nullopt, grandchild_surface_id))
          .AddSolidColorQuad(gfx::Rect(kSurfaceSize), SkColors::kGreen)
          .SetBlendMode(blend_modes[3])
          .Build();
  QueuePassAsFrame(std::move(child_one_pass),
                   child_one_surface_id.local_surface_id(), device_scale_factor,
                   child_one_support.get());

  TestSurfaceIdAllocator child_two_surface_id(
      child_two_support->frame_sink_id());

  auto child_two_pass =
      RenderPassBuilder(kSurfaceSize)
          .AddSolidColorQuad(gfx::Rect(kSurfaceSize), SkColors::kGreen)
          .SetBlendMode(blend_modes[5])
          .Build();
  QueuePassAsFrame(std::move(child_two_pass),
                   child_two_surface_id.local_surface_id(), device_scale_factor,
                   child_two_support.get());

  auto root_pass =
      RenderPassBuilder(kSurfaceSize)
          .AddSolidColorQuad(gfx::Rect(kSurfaceSize), SkColors::kGreen)
          .SetBlendMode(blend_modes[0])
          .AddSurfaceQuad(gfx::Rect(kSurfaceSize),
                          SurfaceRange(std::nullopt, child_one_surface_id))
          .AddSolidColorQuad(gfx::Rect(kSurfaceSize), SkColors::kGreen)
          .SetBlendMode(blend_modes[4])
          .AddSurfaceQuad(gfx::Rect(kSurfaceSize),
                          SurfaceRange(std::nullopt, child_two_surface_id))
          .AddSolidColorQuad(gfx::Rect(kSurfaceSize), SkColors::kGreen)
          .SetBlendMode(blend_modes[6])
          .Build();
  QueuePassAsFrame(std::move(root_pass), root_surface_id_.local_surface_id(),
                   device_scale_factor, root_sink_.get());

  auto aggregated_frame = AggregateFrame(root_surface_id_);

  const auto& aggregated_pass_list = aggregated_frame.render_pass_list;

  ASSERT_EQ(1u, aggregated_pass_list.size());

  const auto& aggregated_quad_list = aggregated_pass_list[0]->quad_list;

  ASSERT_EQ(7u, aggregated_quad_list.size());

  for (auto iter = aggregated_quad_list.cbegin();
       iter != aggregated_quad_list.cend(); ++iter) {
    EXPECT_EQ(blend_modes[iter.index()], iter->shared_quad_state->blend_mode)
        << iter.index();
  }
}

// This tests that we update shared quad state pointers for rounded corner
// bounds correctly within aggregated passes. In case of fast rounded corners or
// rounded corners that fit parent pass' rounded corners, the surface aggregator
// tries to optimize by merging the the surface quads instead of keeping the
// surface render pass.
//
// This test has 4 surfaces in the following structure:
// root_surface         -> [child_root_surface] has fast rounded corner [1],
// child_root_surface   -> [child_one_surface],
//                         [child_two_surface],
//                         quad (a),
// child_one_surface    -> quad (b),
//                         [child three surface],
//                         [child four surface]
// child_two_surface    -> quad (c),
//                      -> quad (d) has fast rounded corner [2]
//                      -> [child_five_surface]
// child_three_surface  -> quad (e),
// child_four_surface   -> quad (f) has fast rounded corner [3]
// child_five_surface   -> quad (g) has rounded corner [4]
//
// Resulting in the following aggregated pass:
// Root Pass:
//  quad (b)          - fast rounded corner [1]
//  quad (e)          - fast rounded corner [1]
//  render pass quad  - fast rounded corner [1]
//  render pass quad  - fast rounded corner [1]
//  quad (c)          - fast rounded corner [1]
//  quad (d)          - rounded corner [2]
//  quad (a)          - fast rounded corner [1]
// Render pass for child_four_surface:
//  quad (f)          - fast rounded corner [3]
// Render pass for child_five_surface:
//  quad (g)          - rounded corner [4]
TEST_F(SurfaceAggregatorValidSurfaceTest,
       AggregateSharedQuadStateRoundedCornerBounds) {
  const gfx::MaskFilterInfo kMaskFilterInfoWithFastRoundedCorners1(
      gfx::RRectF(0, 0, 640, 480, 5));
  const gfx::MaskFilterInfo kMaskFilterInfoWithFastRoundedCorners2(
      gfx::RRectF(6, 7, 100, 100, 2));
  const gfx::MaskFilterInfo kMaskFilterInfoWithFastRoundedCorners3(
      gfx::RRectF(41, 50, 600, 100, 7));
  const gfx::MaskFilterInfo kMaskFilterInfoWithRoundedCorners4(
      gfx::RRectF(0, 1, 10, 10, 3));

  auto child_root_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryFrameSinkId1, /*is_root=*/false);
  auto child_one_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryFrameSinkId2, /*is_root=*/false);
  auto child_two_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryMiddleFrameSinkId, /*is_root=*/false);
  auto child_three_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryFrameSinkId3, /*is_root=*/false);
  auto child_four_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryFrameSinkId4, /*is_root=*/false);
  auto child_five_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryFrameSinkId5, /*is_root=*/false);

  constexpr float device_scale_factor = 1.0f;

  // Setup child five surface.
  TestSurfaceIdAllocator child_five_surface_id(
      child_five_support->frame_sink_id());

  auto child_five_pass =
      RenderPassBuilder(kSurfaceSize)
          .AddSolidColorQuad(gfx::Rect(kSurfaceSize), SkColors::kGreen)
          .SetMaskFilter(kMaskFilterInfoWithRoundedCorners4,
                         /*is_fast_rounded_corner=*/false)
          .Build();
  QueuePassAsFrame(std::move(child_five_pass),
                   child_five_surface_id.local_surface_id(),
                   device_scale_factor, child_five_support.get());

  // Setup child four surface.
  TestSurfaceIdAllocator child_four_surface_id(
      child_four_support->frame_sink_id());

  auto child_four_pass =
      RenderPassBuilder(kSurfaceSize)
          .AddSolidColorQuad(gfx::Rect(kSurfaceSize), SkColors::kGreen)
          .SetMaskFilter(kMaskFilterInfoWithFastRoundedCorners3,
                         /*is_fast_rounded_corner=*/true)
          .Build();
  QueuePassAsFrame(std::move(child_four_pass),
                   child_four_surface_id.local_surface_id(),
                   device_scale_factor, child_four_support.get());

  // Setup child three surface.
  TestSurfaceIdAllocator child_three_surface_id(
      child_three_support->frame_sink_id());

  auto child_three_pass =
      RenderPassBuilder(kSurfaceSize)
          .AddSolidColorQuad(gfx::Rect(kSurfaceSize), SkColors::kGreen)
          .Build();
  QueuePassAsFrame(std::move(child_three_pass),
                   child_three_surface_id.local_surface_id(),
                   device_scale_factor, child_three_support.get());

  // Setup child one surface
  TestSurfaceIdAllocator child_one_surface_id(
      child_one_support->frame_sink_id());

  auto child_one_pass =
      RenderPassBuilder(kSurfaceSize)
          .AddSolidColorQuad(gfx::Rect(kSurfaceSize), SkColors::kGreen)
          .AddSurfaceQuad(gfx::Rect(kSurfaceSize),
                          SurfaceRange(std::nullopt, child_three_surface_id))
          .AddSurfaceQuad(gfx::Rect(kSurfaceSize),
                          SurfaceRange(std::nullopt, child_four_surface_id))
          .Build();
  QueuePassAsFrame(std::move(child_one_pass),
                   child_one_surface_id.local_surface_id(), device_scale_factor,
                   child_one_support.get());

  // Setup child two surface
  TestSurfaceIdAllocator child_two_surface_id(
      child_two_support->frame_sink_id());

  auto child_two_pass =
      RenderPassBuilder(kSurfaceSize)
          .AddSolidColorQuad(gfx::Rect(kSurfaceSize), SkColors::kGreen)
          .AddSolidColorQuad(gfx::Rect(kSurfaceSize), SkColors::kGreen)
          .AddSurfaceQuad(gfx::Rect(kSurfaceSize),
                          SurfaceRange(std::nullopt, child_five_surface_id))
          .SetMaskFilter(kMaskFilterInfoWithFastRoundedCorners2,
                         /*is_fast_rounded_corner=*/true)
          .Build();
  QueuePassAsFrame(std::move(child_two_pass),
                   child_two_surface_id.local_surface_id(), device_scale_factor,
                   child_two_support.get());

  // Setup child root surface
  TestSurfaceIdAllocator child_root_surface_id(
      child_root_support->frame_sink_id());

  auto child_root_pass =
      RenderPassBuilder(kSurfaceSize)
          .AddSurfaceQuad(gfx::Rect(kSurfaceSize),
                          SurfaceRange(std::nullopt, child_one_surface_id))
          .AddSurfaceQuad(gfx::Rect(kSurfaceSize),
                          SurfaceRange(std::nullopt, child_two_surface_id))
          .AddSolidColorQuad(gfx::Rect(kSurfaceSize), SkColors::kGreen)
          .Build();
  QueuePassAsFrame(std::move(child_root_pass),
                   child_root_surface_id.local_surface_id(),
                   device_scale_factor, child_root_support.get());

  auto root_pass =
      RenderPassBuilder(kSurfaceSize)
          .AddSurfaceQuad(gfx::Rect(kSurfaceSize),
                          SurfaceRange(std::nullopt, child_root_surface_id))
          .SetMaskFilter(kMaskFilterInfoWithFastRoundedCorners1,
                         /*is_fast_rounded_corner=*/true)
          .Build();
  QueuePassAsFrame(std::move(root_pass), root_surface_id_.local_surface_id(),
                   device_scale_factor, root_sink_.get());

  auto aggregated_frame = AggregateFrame(root_surface_id_);

  const auto& aggregated_pass_list = aggregated_frame.render_pass_list;

  // There should be 3 render passes since one of the surface quads could reject
  // merging due to it having a quad with a rounded corner of its own that does
  // not fit rounded corners of a parent pass and another surface quad has mask
  // filter with not fast rounded corners that cannot merge.
  ASSERT_EQ(3u, aggregated_pass_list.size());

  // There was a mask filter with fast rounded corners, but they didn't fit
  // into the destination pass' rounded corners' rect. Thus, it couldn't be
  // merged.
  const auto& aggregated_quad_list_of_surface1 =
      aggregated_pass_list[0]->quad_list;
  EXPECT_THAT(
      aggregated_quad_list_of_surface1,
      ElementsAre(HasMaskFilterInfo(kMaskFilterInfoWithFastRoundedCorners3)));

  // There was a mask filter with non-fast rounded corners, which are not
  // allowed to be merged.
  const auto& aggregated_quad_list_of_surface2 =
      aggregated_pass_list[1]->quad_list;
  EXPECT_THAT(
      aggregated_quad_list_of_surface2,
      ElementsAre(HasMaskFilterInfo(kMaskFilterInfoWithRoundedCorners4)));

  // Non-root pass that contains the aggregated render pass.
  const auto& aggregated_quad_list_of_surface3 =
      aggregated_pass_list[2]->quad_list;
  EXPECT_THAT(
      aggregated_quad_list_of_surface3,
      ElementsAre(HasMaskFilterInfo(kMaskFilterInfoWithFastRoundedCorners1),
                  HasMaskFilterInfo(kMaskFilterInfoWithFastRoundedCorners1),
                  HasMaskFilterInfo(kMaskFilterInfoWithFastRoundedCorners1),
                  HasMaskFilterInfo(kMaskFilterInfoWithFastRoundedCorners1),
                  HasMaskFilterInfo(kMaskFilterInfoWithFastRoundedCorners1),
                  HasMaskFilterInfo(kMaskFilterInfoWithFastRoundedCorners2),
                  HasMaskFilterInfo(kMaskFilterInfoWithFastRoundedCorners1)));
}

// Same as above, but with clipping applied. The embedding render pass will have
// mask and clip that are either smaller, equal, or bigger when combined than
// the mask of a render pass where the SurfaceAggregator will try to merge that
// embedding pass in.
TEST_F(SurfaceAggregatorValidSurfaceTest,
       AggregateSharedQuadStateRoundedCornerBoundsClipping) {
  const gfx::MaskFilterInfo kMaskFilterInfoWithFastRoundedCorners1(
      gfx::RRectF(0, 0, 900, 800, 2.5));
  const gfx::MaskFilterInfo kMaskFilterInfoWithFastRoundedCorners2(
      gfx::RRectF(31, 319, 888, 743, 14));
  constexpr gfx::Size kSurfaceSize1(950, 875);

  struct AuxiliaryTestData {
    // Helps to set correct expectation.
    bool mask_will_merge = false;
    // Sets additional clipping.
    std::optional<gfx::Rect> clip_rect = std::nullopt;
    // Transform from parent to target that the second SurfaceQuad's SQS must
    // apply for correctness of its position.
    gfx::Transform parent_target_transform;
  } kAuxiliaryTestData[] = {
      {/*mask_will_merge=*/true, gfx::Rect(0, 0, 350, 750)},
      {/*mask_will_merge=*/true, gfx::Rect(0, 0, 900, 750)},
      {/*mask_will_merge=*/true, gfx::Rect(0, 0, 899, 799)},
      {/*mask_will_merge=*/false, gfx::Rect(0, 0, 900, 800)},
      {/*mask_will_merge=*/false, gfx::Rect(0, 0, 901, 799)},
      {/*mask_will_merge=*/false, std::nullopt},
      {/*mask_will_merge=*/false, gfx::Rect(0, 0, 899, 801)},
      {/*mask_will_merge=*/true, gfx::Rect(31, 319, 70, 80),
       gfx::Transform::MakeTranslation(0, 100)},
  };

  for (auto& test_data : kAuxiliaryTestData) {
    auto child_root_support = std::make_unique<CompositorFrameSinkSupport>(
        nullptr, &manager_, kArbitraryFrameSinkId1, /*is_root=*/false);
    auto child_one_support = std::make_unique<CompositorFrameSinkSupport>(
        nullptr, &manager_, kArbitraryFrameSinkId2, /*is_root=*/false);

    constexpr float device_scale_factor = 1.0f;

    TestSurfaceIdAllocator child_root_surface_id(
        child_root_support->frame_sink_id());
    TestSurfaceIdAllocator child_one_surface_id(
        child_one_support->frame_sink_id());

    auto child_one_pass =
        RenderPassBuilder(kSurfaceSize1)
            .AddSolidColorQuad(gfx::Rect(kSurfaceSize1), SkColors::kGreen)
            .SetQuadClipRect(test_data.clip_rect)
            .SetMaskFilter(kMaskFilterInfoWithFastRoundedCorners2,
                           /*is_fast_rounded_corner=*/true)
            .Build();
    QueuePassAsFrame(std::move(child_one_pass),
                     child_one_surface_id.local_surface_id(),
                     device_scale_factor, child_one_support.get());

    auto child_root_pass =
        RenderPassBuilder(kSurfaceSize1)
            .AddSurfaceQuad(gfx::Rect(kSurfaceSize1),
                            SurfaceRange(std::nullopt, child_one_surface_id))
            .AddSolidColorQuad(gfx::Rect(kSurfaceSize), SkColors::kGreen)
            .Build();
    QueuePassAsFrame(std::move(child_root_pass),
                     child_root_surface_id.local_surface_id(),
                     device_scale_factor, child_root_support.get());

    auto root_pass =
        RenderPassBuilder(kSurfaceSize1)
            .AddSurfaceQuad(gfx::Rect(kSurfaceSize1),
                            SurfaceRange(std::nullopt, child_root_surface_id))
            .SetQuadClipRect(gfx::Rect({0, 0}, kSurfaceSize1))
            .SetMaskFilter(kMaskFilterInfoWithFastRoundedCorners1,
                           /*is_fast_rounded_corner=*/true)
            .SetQuadToTargetTransform(test_data.parent_target_transform)
            .Build();
    QueuePassAsFrame(std::move(root_pass), root_surface_id_.local_surface_id(),
                     device_scale_factor, root_sink_.get());

    auto aggregated_frame = AggregateFrame(root_surface_id_);

    const auto& aggregated_pass_list = aggregated_frame.render_pass_list;

    if (test_data.mask_will_merge) {
      // Given clipping makes kMaskFilterInfoWithFastRoundedCorners2 fit
      // kMaskFilterInfoWithFastRoundedCorners1, there must be only a root
      // render pass.
      ASSERT_EQ(1u, aggregated_pass_list.size());

      const auto& root_aggregated_quad_list_of_surface =
          aggregated_pass_list[0]->quad_list;

      gfx::MaskFilterInfo expected_second_mask =
          kMaskFilterInfoWithFastRoundedCorners2;
      expected_second_mask.ApplyTransform(test_data.parent_target_transform);
      EXPECT_THAT(root_aggregated_quad_list_of_surface,
                  ElementsAre(HasMaskFilterInfo(expected_second_mask),
                              HasMaskFilterInfo(
                                  kMaskFilterInfoWithFastRoundedCorners1)));
    } else {
      // The kMaskFilterInfoWithFastRoundedCorners2 doesn't fit
      // kMaskFilterInfoWithFastRoundedCorners1 and there is no clipping that
      // could be applied. 2 render passes exist then.
      ASSERT_EQ(2u, aggregated_pass_list.size());

      const auto& aggregated_quad_list_of_surface1 =
          aggregated_pass_list[0]->quad_list;
      EXPECT_THAT(aggregated_quad_list_of_surface1,
                  ElementsAre(HasMaskFilterInfo(
                      kMaskFilterInfoWithFastRoundedCorners2)));

      const auto& root_aggregated_quad_list_of_surface =
          aggregated_pass_list[1]->quad_list;
      EXPECT_THAT(
          root_aggregated_quad_list_of_surface,
          ElementsAre(
              HasMaskFilterInfo(kMaskFilterInfoWithFastRoundedCorners1),
              HasMaskFilterInfo(kMaskFilterInfoWithFastRoundedCorners1)));
    }
  }
}

// Tests that transforms are properly handled.
TEST_F(SurfaceAggregatorValidSurfaceTest,
       AggregateSharedQuadStateRoundedCornerBounds2) {
  const gfx::MaskFilterInfo kMaskFilterInfoWithFastRoundedCorners1(
      gfx::RRectF(0, 0, 110, 180, 5));
  const gfx::MaskFilterInfo kMaskFilterInfoWithFastRoundedCorners2(
      gfx::RRectF(2, 3, 100, 100, 2));
  const gfx::MaskFilterInfo kMaskFilterInfoWithFastRoundedCorners3(
      gfx::RRectF(4, 5, 50, 50, 20));

  auto child_root_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryFrameSinkId1, /*is_root=*/false);
  auto child_one_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryFrameSinkId2, /*is_root=*/false);

  constexpr float device_scale_factor = 1.0f;

  // Setup Child one surface
  TestSurfaceIdAllocator child_one_surface_id(
      child_one_support->frame_sink_id());

  auto child_one_pass =
      RenderPassBuilder(kSurfaceSize)
          .AddSolidColorQuad(gfx::Rect(kSurfaceSize), SkColors::kCyan)
          .AddSolidColorQuad(gfx::Rect(kSurfaceSize), SkColors::kBlue)
          .SetMaskFilter(kMaskFilterInfoWithFastRoundedCorners3,
                         /*is_fast_rounded_corner=*/true)
          .AddSolidColorQuad(gfx::Rect(kSurfaceSize), SkColors::kBlue)
          .SetMaskFilter(kMaskFilterInfoWithFastRoundedCorners2,
                         /*is_fast_rounded_corner=*/true)
          .Build();
  QueuePassAsFrame(std::move(child_one_pass),
                   child_one_surface_id.local_surface_id(), device_scale_factor,
                   child_one_support.get());

  // Setup child root surface
  TestSurfaceIdAllocator child_root_surface_id(
      child_root_support->frame_sink_id());

  auto child_root_pass =
      RenderPassBuilder({kSurfaceSize})
          .AddSurfaceQuad(gfx::Rect(kSurfaceSize),
                          SurfaceRange(std::nullopt, child_one_surface_id))
          .AddSolidColorQuad(gfx::Rect(kSurfaceSize), SkColors::kGreen)
          .Build();
  auto* child_root_pass_sqs = child_root_pass->shared_quad_state_list.front();
  child_root_pass_sqs->quad_to_target_transform.Translate(5, 10);
  QueuePassAsFrame(std::move(child_root_pass),
                   child_root_surface_id.local_surface_id(),
                   device_scale_factor, child_root_support.get());

  auto root_pass =
      RenderPassBuilder(kSurfaceSize)
          .AddSurfaceQuad(gfx::Rect(kSurfaceSize),
                          SurfaceRange(std::nullopt, child_root_surface_id))
          .SetMaskFilter(kMaskFilterInfoWithFastRoundedCorners1,
                         /*is_fast_rounded_corner=*/true)
          .Build();
  auto* root_pass_sqs = root_pass->shared_quad_state_list.front();
  root_pass_sqs->quad_to_target_transform.Translate(0, 80);
  root_pass->transform_to_root_target.Translate(5, 10);
  QueuePassAsFrame(std::move(root_pass), root_surface_id_.local_surface_id(),
                   device_scale_factor, root_sink_.get());

  auto aggregated_frame = AggregateFrame(root_surface_id_);

  const auto& aggregated_pass_list = aggregated_frame.render_pass_list;

  ASSERT_EQ(2u, aggregated_pass_list.size());

  const auto& non_root_aggregated_quad_list_of_surface =
      aggregated_pass_list[0]->quad_list;
  EXPECT_THAT(
      non_root_aggregated_quad_list_of_surface,
      ElementsAre(HasMaskFilterInfo(gfx::MaskFilterInfo()),
                  HasMaskFilterInfo(kMaskFilterInfoWithFastRoundedCorners3),
                  HasMaskFilterInfo(kMaskFilterInfoWithFastRoundedCorners2)));

  const auto& root_aggregated_quad_list_of_surface =
      aggregated_pass_list[1]->quad_list;
  EXPECT_THAT(
      root_aggregated_quad_list_of_surface,
      ElementsAre(HasMaskFilterInfo(kMaskFilterInfoWithFastRoundedCorners1),
                  HasMaskFilterInfo(kMaskFilterInfoWithFastRoundedCorners1)));
}

// This tests that when aggregating a frame with multiple render passes that we
// map the transforms for the root pass but do not modify the transform on child
// passes.
//
// The root surface has one pass with a surface quad transformed by +10 in the y
// direction.
//
// The middle surface has one pass with a surface quad scaled by 2 in the x
// and 3 in the y directions.
//
// The child surface has two passes. The first pass has a quad with a transform
// of +5 in the x direction. The second pass has a reference to the first pass'
// pass id and a transform of +8 in the x direction.
//
// After aggregation, the child surface's root pass quad should have all
// transforms concatenated for a total transform of +23 x, +10 y. The
// contributing render pass' transform in the aggregate frame should not be
// affected.
TEST_F(SurfaceAggregatorValidSurfaceTest, AggregateMultiplePassWithTransform) {
  auto middle_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryMiddleFrameSinkId, /*is_root=*/false);
  // Innermost child surface.
  TestSurfaceIdAllocator child_surface_id(child_sink_->frame_sink_id());
  {
    CompositorRenderPassId child_pass_id[] = {CompositorRenderPassId{1},
                                              CompositorRenderPassId{2}};
    std::vector<Quad> child_quads[2] = {
        {Quad::SolidColorQuad(SkColors::kGreen, gfx::Rect(5, 5))},
        {Quad::RenderPassQuad(child_pass_id[0], gfx::Transform(), true)},
    };
    std::vector<Pass> child_passes = {
        Pass(child_quads[0], child_pass_id[0], kSurfaceSize),
        Pass(child_quads[1], child_pass_id[1], kSurfaceSize)};

    CompositorFrame child_frame = MakeEmptyCompositorFrame();
    AddPasses(&child_frame.render_pass_list, child_passes,
              &child_frame.metadata.referenced_surfaces);

    auto* child_nonroot_pass = child_frame.render_pass_list[0].get();
    child_nonroot_pass->transform_to_root_target.Translate(8, 0);
    auto* child_nonroot_pass_sqs =
        child_nonroot_pass->shared_quad_state_list.front();
    child_nonroot_pass_sqs->quad_to_target_transform.Translate(5, 0);

    auto* child_root_pass = child_frame.render_pass_list[1].get();
    auto* child_root_pass_sqs = child_root_pass->shared_quad_state_list.front();
    child_root_pass_sqs->quad_to_target_transform.Translate(8, 0);
    child_root_pass_sqs->clip_rect = gfx::Rect(0, 0, 5, 5);

    child_sink_->SubmitCompositorFrame(child_surface_id.local_surface_id(),
                                       std::move(child_frame));
  }

  // Middle child surface.
  TestSurfaceIdAllocator middle_surface_id(middle_support->frame_sink_id());
  {
    std::vector<Quad> middle_quads = {Quad::SurfaceQuad(
        SurfaceRange(std::nullopt, child_surface_id), SkColors::kWhite,
        gfx::Rect(5, 5), /*stretch_content_to_fill_bounds=*/false)};
    std::vector<Pass> middle_passes = {
        Pass(middle_quads, kSurfaceSize),
    };

    CompositorFrame middle_frame = MakeEmptyCompositorFrame();
    AddPasses(&middle_frame.render_pass_list, middle_passes,
              &middle_frame.metadata.referenced_surfaces);

    auto* middle_root_pass = middle_frame.render_pass_list[0].get();
    DrawQuad* middle_frame_quad = middle_root_pass->quad_list.ElementAt(0);
    middle_frame_quad->rect = gfx::Rect(0, 1, 100, 7);
    middle_frame_quad->visible_rect = gfx::Rect(0, 1, 100, 7);
    auto* middle_root_pass_sqs =
        middle_root_pass->shared_quad_state_list.front();
    middle_root_pass_sqs->quad_to_target_transform.Scale(2, 3);

    middle_support->SubmitCompositorFrame(middle_surface_id.local_surface_id(),
                                          std::move(middle_frame));
  }

  // Root surface.
  std::vector<Quad> secondary_quads = {
      Quad::SolidColorQuad({1.0, 0.0, 1.0, 1.0f / 255.0f}, gfx::Rect(5, 5)),
      Quad::SurfaceQuad(SurfaceRange(std::nullopt, middle_surface_id),
                        SkColors::kWhite, gfx::Rect(5, 5),
                        /*stretch_content_to_fill_bounds=*/false)};
  std::vector<Quad> root_quads = {
      Quad::SolidColorQuad({1.0, 0.0, 1.0, 1.0f / 255.0f}, gfx::Rect(5, 5))};
  std::vector<Pass> root_passes = {
      Pass(secondary_quads, CompositorRenderPassId(1), kSurfaceSize),
      Pass(root_quads, CompositorRenderPassId(2), kSurfaceSize)};

  CompositorFrame root_frame = MakeEmptyCompositorFrame();
  AddPasses(&root_frame.render_pass_list, root_passes,
            &root_frame.metadata.referenced_surfaces);

  root_frame.render_pass_list[0]
      ->shared_quad_state_list.front()
      ->quad_to_target_transform.Translate(0, 7);
  root_frame.render_pass_list[0]
      ->shared_quad_state_list.ElementAt(1)
      ->quad_to_target_transform.Translate(0, 10);
  DrawQuad* root_frame_quad =
      root_frame.render_pass_list[0]->quad_list.ElementAt(1);
  root_frame_quad->rect = gfx::Rect(8, 100);
  root_frame_quad->visible_rect = gfx::Rect(8, 100);
  root_frame.render_pass_list[0]->transform_to_root_target.Translate(10, 5);

  root_sink_->SubmitCompositorFrame(root_surface_id_.local_surface_id(),
                                    std::move(root_frame));

  auto aggregated_frame = AggregateFrame(root_surface_id_);

  const auto& aggregated_pass_list = aggregated_frame.render_pass_list;

  ASSERT_EQ(3u, aggregated_pass_list.size());

  ASSERT_EQ(1u, aggregated_pass_list[0]->shared_quad_state_list.size());

  // The first pass should have one shared quad state for the one solid color
  // quad.
  EXPECT_EQ(1u, aggregated_pass_list[0]->shared_quad_state_list.size());
  // The second pass should have just two shared quad states. We'll
  // verify the properties through the quads.
  EXPECT_EQ(2u, aggregated_pass_list[1]->shared_quad_state_list.size());

  EXPECT_EQ(1u, aggregated_pass_list[2]->shared_quad_state_list.size());

  auto* aggregated_first_pass_sqs =
      aggregated_pass_list[0]->shared_quad_state_list.front();

  // The first pass's transform should be unaffected by the embedding and still
  // be a translation by +5 in the x direction.
  gfx::Transform expected_aggregated_first_pass_sqs_transform;
  expected_aggregated_first_pass_sqs_transform.Translate(5, 0);
  EXPECT_EQ(expected_aggregated_first_pass_sqs_transform.ToString(),
            aggregated_first_pass_sqs->quad_to_target_transform.ToString());

  // The first pass's transform to the root target should include the aggregated
  // transform, including the transform from the child pass to the root.
  gfx::Transform expected_first_pass_transform_to_root_target;
  expected_first_pass_transform_to_root_target.Translate(10, 5);
  expected_first_pass_transform_to_root_target.Translate(0, 10);
  expected_first_pass_transform_to_root_target.Scale(2, 3);
  expected_first_pass_transform_to_root_target.Translate(8, 0);
  EXPECT_EQ(expected_first_pass_transform_to_root_target.ToString(),
            aggregated_pass_list[0]->transform_to_root_target.ToString());

  ASSERT_EQ(2u, aggregated_pass_list[1]->quad_list.size());

  gfx::Transform expected_root_pass_quad_transforms[2];
  // The first quad in the root pass is the solid color quad from the original
  // root surface. Its transform should be unaffected by the aggregation and
  // still be +7 in the y direction.
  expected_root_pass_quad_transforms[0].Translate(0, 7);
  // The second quad in the root pass is aggregated from the child surface so
  // its transform should be the combination of its original translation
  // (0, 10), the middle surface draw quad's scale of (2, 3), and the
  // child surface draw quad's translation (8, 0).
  expected_root_pass_quad_transforms[1].Translate(0, 10);
  expected_root_pass_quad_transforms[1].Scale(2, 3);
  expected_root_pass_quad_transforms[1].Translate(8, 0);

  for (auto iter = aggregated_pass_list[1]->quad_list.cbegin();
       iter != aggregated_pass_list[1]->quad_list.cend(); ++iter) {
    EXPECT_EQ(expected_root_pass_quad_transforms[iter.index()].ToString(),
              iter->shared_quad_state->quad_to_target_transform.ToString())
        << iter.index();
  }

  EXPECT_TRUE(aggregated_pass_list[1]
                  ->shared_quad_state_list.ElementAt(1)
                  ->clip_rect.has_value());

  // The second quad in the root pass is aggregated from the child, so its
  // clip rect must be transformed by the child's translation/scale and
  // clipped be the visible_rects for both children.
  EXPECT_EQ(
      gfx::Rect(0, 13, 8, 12),
      aggregated_pass_list[1]->shared_quad_state_list.ElementAt(1)->clip_rect);
}

// This test verifies that in the absence of a primary Surface,
// SurfaceAggregator will embed a fallback Surface, if available. If the primary
// surface is available, though, the fallback will not be used.
TEST_P(SurfaceAggregatorValidSurfaceWithMergingPassesTest,
       FallbackSurfaceReference) {
  auto primary_child_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryFrameSinkId1, /*is_root=*/false);

  auto fallback_child_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryFrameSinkId2, /*is_root=*/false);

  TestSurfaceIdAllocator fallback_child_surface_id(
      fallback_child_support->frame_sink_id());

  TestSurfaceIdAllocator primary_child_surface_id(
      primary_child_support->frame_sink_id());

  constexpr gfx::Size fallback_size(10, 10);
  std::vector<Quad> fallback_child_quads = {
      Quad::SolidColorQuad(SkColors::kRed, gfx::Rect(fallback_size))};
  std::vector<Pass> fallback_child_passes = {
      Pass(fallback_child_quads, fallback_size)};

  // Submit a CompositorFrame to the fallback Surface containing a red
  // SolidColorDrawQuad.
  constexpr float device_scale_factor_1 = 1.0f;
  constexpr float device_scale_factor_2 = 2.0f;
  SubmitCompositorFrame(fallback_child_support.get(), fallback_child_passes,
                        fallback_child_surface_id.local_surface_id(),
                        device_scale_factor_2);

  // Try to embed |primary_child_surface_id| and if unavailable, embed
  // |fallback_child_surface_id|. The |allow_merge| flag would be set to
  // true/false based on the parameter of the test.
  constexpr gfx::Rect surface_quad_rect(12, 15);
  std::vector<Quad> root_quads = {Quad::SurfaceQuad(
      SurfaceRange(fallback_child_surface_id, primary_child_surface_id),
      SkColors::kWhite, surface_quad_rect,
      /*stretch_content_to_fill_bounds=*/false, AllowMerge())};
  std::vector<Pass> root_passes = {Pass(root_quads, kSurfaceSize)};

  MockAggregatedDamageCallback aggregated_damage_callback;

  root_sink_->SetAggregatedDamageCallbackForTesting(
      aggregated_damage_callback.GetCallback());
  primary_child_support->SetAggregatedDamageCallbackForTesting(
      aggregated_damage_callback.GetCallback());
  fallback_child_support->SetAggregatedDamageCallbackForTesting(
      aggregated_damage_callback.GetCallback());

  SubmitCompositorFrame(root_sink_.get(), root_passes,
                        root_surface_id_.local_surface_id(),
                        device_scale_factor_1);

  // There is no CompositorFrame submitted to |primary_child_surface_id|
  // so |fallback_child_surface_id| will be embedded and we should see a red
  // SolidColorDrawQuad. These quads are in physical pixels.
  Quad right_gutter_quad =
      Quad::SolidColorQuad(SkColors::kWhite, gfx::Rect(5, 0, 7, 15));
  Quad bottom_gutter_quad =
      Quad::SolidColorQuad(SkColors::kWhite, gfx::Rect(0, 5, 5, 10));
  Quad render_pass_quad =
      Quad::RenderPassQuad(CompositorRenderPassId{2}, gfx::Transform(), true);
  Quad fallback_quad =
      Quad::SolidColorQuad(SkColors::kRed, gfx::Rect(fallback_size));

  // Without merging, a RPDQ will replace the fallback surface quad.
  // With merging, the solid color quad contained in the fallback surface will
  // replace the fallback surface quad.
  std::vector<Quad> expected_quads1{
      right_gutter_quad, bottom_gutter_quad,
      AllowMerge() ? fallback_quad : render_pass_quad};
  std::vector<Pass> expected_passes1;
  if (!AllowMerge()) {
    // Without merging, the root render pass of the fallback surface should be
    // added to the final render pass list.
    expected_passes1.emplace_back(fallback_child_quads, fallback_size);
  }
  expected_passes1.emplace_back(expected_quads1, kSurfaceSize);

  EXPECT_CALL(aggregated_damage_callback,
              OnAggregatedDamage(fallback_child_surface_id.local_surface_id(),
                                 fallback_size, gfx::Rect(fallback_size),
                                 next_display_time()))
      .Times(1);
  EXPECT_CALL(
      aggregated_damage_callback,
      OnAggregatedDamage(primary_child_surface_id.local_surface_id(), _, _, _))
      .Times(0);
  // The whole root surface should be damaged because this is the first
  // aggregation.
  EXPECT_CALL(
      aggregated_damage_callback,
      OnAggregatedDamage(root_surface_id_.local_surface_id(), kSurfaceSize,
                         gfx::Rect(kSurfaceSize), next_display_time()))
      .Times(1);

  // The primary_surface will not be listed in previously contained surfaces.
  AggregateAndVerify(expected_passes1,
                     {root_surface_id_, fallback_child_surface_id});

  testing::Mock::VerifyAndClearExpectations(&aggregated_damage_callback);

  // Submit the fallback again to create some damage then aggregate again.
  fallback_child_surface_id.Increment();

  SubmitCompositorFrame(fallback_child_support.get(), fallback_child_passes,
                        fallback_child_surface_id.local_surface_id(),
                        device_scale_factor_2);

  EXPECT_CALL(aggregated_damage_callback,
              OnAggregatedDamage(fallback_child_surface_id.local_surface_id(),
                                 fallback_size, gfx::Rect(fallback_size),
                                 next_display_time()))
      .Times(1);
  EXPECT_CALL(
      aggregated_damage_callback,
      OnAggregatedDamage(primary_child_surface_id.local_surface_id(), _, _, _))
      .Times(0);
  // The damage should be equal to whole size of the primary SurfaceDrawQuad.
  EXPECT_CALL(
      aggregated_damage_callback,
      OnAggregatedDamage(root_surface_id_.local_surface_id(), kSurfaceSize,
                         surface_quad_rect, testing::A<base::TimeTicks>()))
      .Times(1);

  std::vector<Quad> expected_quads2{
      right_gutter_quad, bottom_gutter_quad,
      AllowMerge() ? fallback_quad : render_pass_quad};
  std::vector<Pass> expected_passes2;
  if (!AllowMerge())
    expected_passes2.emplace_back(fallback_child_quads, fallback_size);
  expected_passes2.emplace_back(expected_quads2, kSurfaceSize);
  AggregateAndVerify(expected_passes2,
                     {root_surface_id_,
                      SurfaceId(fallback_child_support->frame_sink_id(),
                                fallback_child_surface_id.local_surface_id())});

  testing::Mock::VerifyAndClearExpectations(&aggregated_damage_callback);

  std::vector<Quad> primary_child_quads = {
      Quad::SolidColorQuad(SkColors::kGreen, gfx::Rect(5, 5))};
  constexpr gfx::Size primary_surface_size(5, 5);
  std::vector<Pass> primary_child_passes = {
      Pass(primary_child_quads, primary_surface_size)};

  // Submit a CompositorFrame to the primary Surface containing a green
  // SolidColorDrawQuad.
  SubmitCompositorFrame(primary_child_support.get(), primary_child_passes,
                        primary_child_surface_id.local_surface_id(),
                        device_scale_factor_2);

  // Now that the primary Surface has a CompositorFrame, we expect
  // SurfaceAggregator to embed the primary Surface, and drop the fallback
  // Surface.
  Quad primary_quad = Quad::SolidColorQuad(SkColors::kGreen, gfx::Rect(5, 5));
  // The primary surface is now available, so the RPDQ will point to a new pass
  // ID instead of the previous fallback pass ID.
  render_pass_quad.render_pass_id = CompositorRenderPassId{3};
  std::vector<Quad> expected_quads3{AllowMerge() ? primary_quad
                                                 : render_pass_quad};
  std::vector<Pass> expected_passes3;
  if (!AllowMerge())
    expected_passes3.emplace_back(primary_child_quads, primary_surface_size);
  expected_passes3.emplace_back(expected_quads3, kSurfaceSize);

  EXPECT_CALL(
      aggregated_damage_callback,
      OnAggregatedDamage(primary_child_surface_id.local_surface_id(),
                         primary_surface_size, gfx::Rect(primary_surface_size),
                         next_display_time()))
      .Times(1);
  EXPECT_CALL(
      aggregated_damage_callback,
      OnAggregatedDamage(fallback_child_surface_id.local_surface_id(), _, _, _))
      .Times(0);

  // The damage of the root should be equal to the damage of the primary surface
  // after scaling by 0.5 since the root surface has DSF=1 and primary surface
  // has DSF=2.
  gfx::Rect scaled_primary_surface_rect =
      gfx::ScaleToEnclosingRect(gfx::Rect(primary_surface_size), 0.5, 0.5);
  EXPECT_CALL(
      aggregated_damage_callback,
      OnAggregatedDamage(root_surface_id_.local_surface_id(), kSurfaceSize,
                         scaled_primary_surface_rect, next_display_time()))
      .Times(1);

  AggregateAndVerify(expected_passes3,
                     {root_surface_id_, primary_child_surface_id});
}

// Tests that damage rects are aggregated correctly when surfaces change.
TEST_P(SurfaceAggregatorValidSurfaceWithMergingPassesTest,
       AggregateDamageRect) {
  // Add a callback for when the surface is damaged.
  MockAggregatedDamageCallback aggregated_damage_callback;
  root_sink_->SetAggregatedDamageCallbackForTesting(
      aggregated_damage_callback.GetCallback());

  auto parent_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryMiddleFrameSinkId, /*is_root=*/false);

  CompositorRenderPassList child_passes;
  child_passes.push_back(
      RenderPassBuilder(CompositorRenderPassId{1}, kSurfaceSize)
          .AddSolidColorQuad(gfx::Rect(5, 5), SkColors::kGreen)
          .SetQuadToTargetTranslation(8, 0)
          .Build());

  TestSurfaceIdAllocator child_surface_id(child_sink_->frame_sink_id());
  {
    CompositorFrame child_frame =
        MakeCompositorFrame(CopyRenderPasses(child_passes));

    child_sink_->SubmitCompositorFrame(child_surface_id.local_surface_id(),
                                       std::move(child_frame));
  }

  // Parent surface is only used to test if the transform is applied correctly
  // to the child surface's damage.
  CompositorFrame parent_surface_frame =
      CompositorFrameBuilder()
          .AddRenderPass(
              RenderPassBuilder(kSurfaceSize)
                  .AddSurfaceQuad(gfx::Rect(kSurfaceSize),
                                  SurfaceRange(std::nullopt, child_surface_id),
                                  {.allow_merge = AllowMerge()}))
          .Build();

  TestSurfaceIdAllocator parent_surface_id(parent_support->frame_sink_id());
  parent_support->SubmitCompositorFrame(parent_surface_id.local_surface_id(),
                                        std::move(parent_surface_frame));

  CompositorRenderPassList root_passes;
  root_passes.push_back(
      RenderPassBuilder(CompositorRenderPassId{1}, kSurfaceSize)
          .AddSurfaceQuad(gfx::Rect(kSurfaceSize),
                          SurfaceRange(std::nullopt, parent_surface_id),
                          {.allow_merge = AllowMerge()})
          .SetQuadToTargetTranslation(0, 10)
          .SetDamageRect(gfx::Rect(5, 5, 95, 95))
          .Build());
  root_passes.push_back(
      RenderPassBuilder(CompositorRenderPassId{2}, kSurfaceSize)
          .AddRenderPassQuad(gfx::Rect(kSurfaceSize), CompositorRenderPassId{1})
          .Build());

  {
    root_sink_->SubmitCompositorFrame(
        root_surface_id_.local_surface_id(),
        MakeCompositorFrame(CopyRenderPasses(root_passes)));
  }

  size_t expected_num_passes_after_aggregation = AllowMerge() ? 2u : 4u;

  // Damage rect for first aggregation should contain entire root surface.
  {
    const gfx::Rect expected_root_damage(kSurfaceSize);
    EXPECT_CALL(
        aggregated_damage_callback,
        OnAggregatedDamage(root_surface_id_.local_surface_id(), kSurfaceSize,
                           expected_root_damage, next_display_time()));
    auto aggregated_frame = AggregateFrame(root_surface_id_);
    testing::Mock::VerifyAndClearExpectations(&aggregated_damage_callback);
    const auto& aggregated_pass_list = aggregated_frame.render_pass_list;
    ASSERT_EQ(expected_num_passes_after_aggregation,
              aggregated_pass_list.size());

    // The non-root render passes should have the root render pass damage_rect
    // translated into the appropriate coordinate space. When merging is not
    // allowed the first two render passes are translated 10px so the 100x100
    // root damage is only covers an area 100x90.
    if (AllowMerge()) {
      EXPECT_EQ(aggregated_pass_list[0]->damage_rect, expected_root_damage);
    } else {
      const gfx::Rect expected_clipped_damage(0, 0, 100, 90);
      EXPECT_EQ(aggregated_pass_list[0]->damage_rect, expected_clipped_damage);
      EXPECT_EQ(aggregated_pass_list[1]->damage_rect, expected_clipped_damage);
      EXPECT_EQ(aggregated_pass_list[2]->damage_rect, expected_root_damage);
    }
  }

  {
    CompositorFrame child_frame =
        MakeCompositorFrame(CopyRenderPasses(child_passes));

    child_frame.render_pass_list[0]->damage_rect = gfx::Rect(10, 10, 10, 10);

    child_sink_->SubmitCompositorFrame(child_surface_id.local_surface_id(),
                                       std::move(child_frame));

    // Outer surface didn't change, so a transformed inner damage rect is
    // expected.
    const gfx::Rect expected_damage_rect(10, 20, 10, 10);
    EXPECT_CALL(
        aggregated_damage_callback,
        OnAggregatedDamage(root_surface_id_.local_surface_id(), kSurfaceSize,
                           expected_damage_rect, next_display_time()));
    auto aggregated_frame = AggregateFrame(root_surface_id_);
    const auto& aggregated_pass_list = aggregated_frame.render_pass_list;
    ASSERT_EQ(expected_num_passes_after_aggregation,
              aggregated_pass_list.size());
    EXPECT_EQ(expected_damage_rect, aggregated_pass_list.back()->damage_rect);
  }

  {
    CompositorFrame root_frame =
        MakeCompositorFrame(CopyRenderPasses(root_passes));
    root_frame.render_pass_list[0]->damage_rect = gfx::Rect(0, 0, 1, 1);

    root_sink_->SubmitCompositorFrame(root_surface_id_.local_surface_id(),
                                      std::move(root_frame));
  }

  {
    CompositorFrame root_frame =
        MakeCompositorFrame(CopyRenderPasses(root_passes));
    root_frame.render_pass_list[0]->damage_rect = gfx::Rect(1, 1, 1, 1);

    root_sink_->SubmitCompositorFrame(root_surface_id_.local_surface_id(),
                                      std::move(root_frame));

    // The root surface was enqueued without being aggregated once, so it should
    // be treated as completely damaged.
    EXPECT_CALL(
        aggregated_damage_callback,
        OnAggregatedDamage(root_surface_id_.local_surface_id(), kSurfaceSize,
                           gfx::Rect(kSurfaceSize), next_display_time()));
    auto aggregated_frame = AggregateFrame(root_surface_id_);
    const auto& aggregated_pass_list = aggregated_frame.render_pass_list;
    ASSERT_EQ(expected_num_passes_after_aggregation,
              aggregated_pass_list.size());
    EXPECT_EQ(gfx::Rect(kSurfaceSize),
              aggregated_pass_list.back()->damage_rect);
  }

  // No Surface changed, so no damage should be given.
  {
    EXPECT_CALL(aggregated_damage_callback, OnAggregatedDamage(_, _, _, _))
        .Times(0);
    auto aggregated_frame = AggregateFrame(root_surface_id_);
    const auto& aggregated_pass_list = aggregated_frame.render_pass_list;
    ASSERT_EQ(expected_num_passes_after_aggregation,
              aggregated_pass_list.size());
    EXPECT_TRUE(aggregated_pass_list.back()->damage_rect.IsEmpty());
  }

  // SetFullDamageRectForSurface should cause the entire output to be
  // marked as damaged.
  {
    aggregator_.SetFullDamageForSurface(root_surface_id_);
    EXPECT_CALL(
        aggregated_damage_callback,
        OnAggregatedDamage(root_surface_id_.local_surface_id(), kSurfaceSize,
                           gfx::Rect(kSurfaceSize), next_display_time()));
    auto aggregated_frame = AggregateFrame(root_surface_id_);
    const auto& aggregated_pass_list = aggregated_frame.render_pass_list;
    ASSERT_EQ(expected_num_passes_after_aggregation,
              aggregated_pass_list.size());
    EXPECT_TRUE(aggregated_pass_list.back()->damage_rect.Contains(
        gfx::Rect(kSurfaceSize)));
  }
}

// Tests that damage rects are aggregated correctly when surfaces stretch to
// fit and device size is less than 1.
TEST_P(SurfaceAggregatorValidSurfaceWithMergingPassesTest,
       AggregateDamageRectWithSquashToFit) {
  // Add a callback for when the surface is damaged.
  MockAggregatedDamageCallback aggregated_damage_callback;
  root_sink_->SetAggregatedDamageCallbackForTesting(
      aggregated_damage_callback.GetCallback());

  auto parent_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryMiddleFrameSinkId, /*is_root=*/false);
  std::vector<Quad> child_quads = {
      Quad::SolidColorQuad(SkColors::kGreen, gfx::Rect(5, 5))};
  std::vector<Pass> child_passes = {
      Pass(child_quads, CompositorRenderPassId{1}, gfx::Size(kSurfaceSize))};

  TestSurfaceIdAllocator child_surface_id(child_sink_->frame_sink_id());
  {
    CompositorFrame child_frame = MakeEmptyCompositorFrame();
    AddPasses(&child_frame.render_pass_list, child_passes,
              &child_frame.metadata.referenced_surfaces);

    auto* child_root_pass = child_frame.render_pass_list[0].get();
    auto* child_root_pass_sqs = child_root_pass->shared_quad_state_list.front();
    child_root_pass_sqs->quad_to_target_transform.Translate(8, 0);

    child_sink_->SubmitCompositorFrame(child_surface_id.local_surface_id(),
                                       std::move(child_frame));
  }

  std::vector<Quad> parent_surface_quads = {Quad::SurfaceQuad(
      SurfaceRange(std::nullopt, child_surface_id), SkColors::kWhite,
      gfx::Rect(kSurfaceSize), /*stretch_content_to_fill_bounds=*/false,
      AllowMerge())};
  std::vector<Pass> parent_surface_passes = {
      Pass(parent_surface_quads, CompositorRenderPassId{1}, kSurfaceSize)};

  // Parent surface is only used to test if the transform is applied correctly
  // to the child surface's damage.
  CompositorFrame parent_surface_frame = MakeEmptyCompositorFrame();
  AddPasses(&parent_surface_frame.render_pass_list, parent_surface_passes,
            &parent_surface_frame.metadata.referenced_surfaces);

  TestSurfaceIdAllocator parent_surface_id(parent_support->frame_sink_id());
  parent_support->SubmitCompositorFrame(parent_surface_id.local_surface_id(),
                                        std::move(parent_surface_frame));

  CompositorFrame root_frame =
      CompositorFrameBuilder()
          .AddRenderPass(
              RenderPassBuilder(CompositorRenderPassId{1}, kSurfaceSize)
                  .AddSurfaceQuad(gfx::Rect(50, 50),
                                  SurfaceRange(std::nullopt, parent_surface_id),
                                  {.stretch_content_to_fill_bounds = true,
                                   .allow_merge = AllowMerge()}))
          .AddRenderPass(
              RenderPassBuilder(CompositorRenderPassId{2}, kSurfaceSize)
                  .AddRenderPassQuad(gfx::Rect(kSurfaceSize),
                                     CompositorRenderPassId{1}))
          .Build();

  root_sink_->SubmitCompositorFrame(root_surface_id_.local_surface_id(),
                                    std::move(root_frame));

  // Damage rect for first aggregation should be exactly the entire root
  // surface.

  size_t expected_num_passes_after_aggregation = AllowMerge() ? 2u : 4u;
  {
    EXPECT_CALL(
        aggregated_damage_callback,
        OnAggregatedDamage(root_surface_id_.local_surface_id(), kSurfaceSize,
                           gfx::Rect(kSurfaceSize), next_display_time()));
    auto aggregated_frame = AggregateFrame(root_surface_id_);
    testing::Mock::VerifyAndClearExpectations(&aggregated_damage_callback);
    const auto& aggregated_pass_list = aggregated_frame.render_pass_list;
    ASSERT_EQ(expected_num_passes_after_aggregation,
              aggregated_pass_list.size());
    EXPECT_EQ(gfx::Rect(kSurfaceSize),
              aggregated_pass_list.back()->damage_rect);
  }

  {
    CompositorFrame child_frame = MakeEmptyCompositorFrame();
    AddPasses(&child_frame.render_pass_list, child_passes,
              &child_frame.metadata.referenced_surfaces);

    auto* child_root_pass = child_frame.render_pass_list[0].get();
    child_root_pass->damage_rect = gfx::Rect(10, 20, 20, 30);

    child_sink_->SubmitCompositorFrame(child_surface_id.local_surface_id(),
                                       std::move(child_frame));

    // Outer surface didn't change, so transformed inner damage rect should be
    // used. Since the child surface is stretching to fit the outer surface
    // which is half the size, we end up with a damage rect that is half the
    // size of the child surface.
    const gfx::Rect expected_damage_rect(5, 10, 10, 15);
    EXPECT_CALL(
        aggregated_damage_callback,
        OnAggregatedDamage(root_surface_id_.local_surface_id(), kSurfaceSize,
                           expected_damage_rect, next_display_time()));
    auto aggregated_frame = AggregateFrame(root_surface_id_);
    const auto& aggregated_pass_list = aggregated_frame.render_pass_list;
    ASSERT_EQ(expected_num_passes_after_aggregation,
              aggregated_pass_list.size());
    EXPECT_EQ(expected_damage_rect, aggregated_pass_list.back()->damage_rect);
  }
}

// Tests that damage rects are aggregated correctly when surfaces stretch to
// fit and device size is greater than 1.
TEST_P(SurfaceAggregatorValidSurfaceWithMergingPassesTest,
       AggregateDamageRectWithStretchToFit) {
  // Add a callback for when the surface is damaged.
  MockAggregatedDamageCallback aggregated_damage_callback;
  root_sink_->SetAggregatedDamageCallbackForTesting(
      aggregated_damage_callback.GetCallback());

  auto parent_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryMiddleFrameSinkId, /*is_root=*/false);
  std::vector<Quad> child_quads = {
      Quad::SolidColorQuad(SkColors::kGreen, gfx::Rect(5, 5))};
  std::vector<Pass> child_passes = {
      Pass(child_quads, CompositorRenderPassId{1}, gfx::Size(100, 100))};

  TestSurfaceIdAllocator child_surface_id(child_sink_->frame_sink_id());
  {
    CompositorFrame child_frame = MakeEmptyCompositorFrame();
    AddPasses(&child_frame.render_pass_list, child_passes,
              &child_frame.metadata.referenced_surfaces);

    auto* child_root_pass = child_frame.render_pass_list[0].get();
    auto* child_root_pass_sqs = child_root_pass->shared_quad_state_list.front();
    child_root_pass_sqs->quad_to_target_transform.Translate(8, 0);

    child_sink_->SubmitCompositorFrame(child_surface_id.local_surface_id(),
                                       std::move(child_frame));
  }

  std::vector<Quad> parent_surface_quads = {Quad::SurfaceQuad(
      SurfaceRange(std::nullopt, child_surface_id), SkColors::kWhite,
      gfx::Rect(kSurfaceSize), /*stretch_content_to_fill_bounds=*/false,
      AllowMerge())};
  std::vector<Pass> parent_surface_passes = {
      Pass(parent_surface_quads, CompositorRenderPassId{1}, kSurfaceSize)};

  // Parent surface is only used to test if the transform is applied correctly
  // to the child surface's damage.
  CompositorFrame parent_surface_frame = MakeEmptyCompositorFrame();
  AddPasses(&parent_surface_frame.render_pass_list, parent_surface_passes,
            &parent_surface_frame.metadata.referenced_surfaces);

  TestSurfaceIdAllocator parent_surface_id(parent_support->frame_sink_id());
  parent_support->SubmitCompositorFrame(parent_surface_id.local_surface_id(),
                                        std::move(parent_surface_frame));

  CompositorFrame root_frame =
      CompositorFrameBuilder()
          .AddRenderPass(
              RenderPassBuilder(CompositorRenderPassId{1}, kSurfaceSize)
                  .AddSurfaceQuad(gfx::Rect(200, 200),
                                  SurfaceRange(std::nullopt, parent_surface_id),
                                  {.stretch_content_to_fill_bounds = true,
                                   .allow_merge = AllowMerge()}))
          .AddRenderPass(
              RenderPassBuilder(CompositorRenderPassId{2}, kSurfaceSize)
                  .AddRenderPassQuad(gfx::Rect(kSurfaceSize),
                                     CompositorRenderPassId{1}))
          .Build();

  root_sink_->SubmitCompositorFrame(root_surface_id_.local_surface_id(),
                                    std::move(root_frame));

  // Damage rect for first aggregation should contain entire root surface.
  size_t expected_num_passes_after_aggregation = AllowMerge() ? 2u : 4u;
  {
    EXPECT_CALL(
        aggregated_damage_callback,
        OnAggregatedDamage(root_surface_id_.local_surface_id(), kSurfaceSize,
                           gfx::Rect(kSurfaceSize), next_display_time()));
    auto aggregated_frame = AggregateFrame(root_surface_id_);
    testing::Mock::VerifyAndClearExpectations(&aggregated_damage_callback);
    const auto& aggregated_pass_list = aggregated_frame.render_pass_list;
    ASSERT_EQ(expected_num_passes_after_aggregation,
              aggregated_pass_list.size());
    EXPECT_EQ(gfx::Rect(kSurfaceSize),
              aggregated_pass_list.back()->damage_rect);
  }

  {
    CompositorFrame child_frame = MakeEmptyCompositorFrame();
    AddPasses(&child_frame.render_pass_list, child_passes,
              &child_frame.metadata.referenced_surfaces);

    auto* child_root_pass = child_frame.render_pass_list[0].get();
    child_root_pass->damage_rect = gfx::Rect(10, 15, 20, 30);

    child_sink_->SubmitCompositorFrame(child_surface_id.local_surface_id(),
                                       std::move(child_frame));

    // Outer surface didn't change, so transformed inner damage rect should be
    // used. Since the child surface is stretching to fit the outer surface
    // which is twice the size, we end up with a damage rect that is double the
    // size of the child surface.
    const gfx::Rect expected_damage_rect(20, 30, 40, 60);
    EXPECT_CALL(
        aggregated_damage_callback,
        OnAggregatedDamage(root_surface_id_.local_surface_id(), kSurfaceSize,
                           expected_damage_rect, next_display_time()));
    auto aggregated_frame = AggregateFrame(root_surface_id_);
    const auto& aggregated_pass_list = aggregated_frame.render_pass_list;
    ASSERT_EQ(expected_num_passes_after_aggregation,
              aggregated_pass_list.size());
    EXPECT_EQ(expected_damage_rect, aggregated_pass_list.back()->damage_rect);
  }
}

// Check that damage is correctly calculated for surfaces.
TEST_F(SurfaceAggregatorValidSurfaceTest, SwitchSurfaceDamage) {
  {
    std::vector<Quad> root_render_pass_quads = {
        Quad::SolidColorQuad({1.0, 0.0, 1.0, 1.0f / 255.0f}, gfx::Rect(5, 5))};

    std::vector<Pass> root_passes = {
        Pass(root_render_pass_quads, CompositorRenderPassId{2}, kSurfaceSize)};

    CompositorFrame root_frame = MakeEmptyCompositorFrame();
    AddPasses(&root_frame.render_pass_list, root_passes,
              &root_frame.metadata.referenced_surfaces);

    root_frame.render_pass_list[0]->damage_rect = gfx::Rect(5, 5, 100, 100);

    root_sink_->SubmitCompositorFrame(root_surface_id_.local_surface_id(),
                                      std::move(root_frame));
  }

  {
    auto aggregated_frame = AggregateFrame(root_surface_id_);

    const auto& aggregated_pass_list = aggregated_frame.render_pass_list;

    ASSERT_EQ(1u, aggregated_pass_list.size());

    // Damage rect for first aggregation should contain entire root surface.
    EXPECT_TRUE(
        aggregated_pass_list[0]->damage_rect.Contains(gfx::Rect(kSurfaceSize)));
  }

  TestSurfaceIdAllocator second_root_surface_id(root_sink_->frame_sink_id());
  {
    std::vector<Quad> root_render_pass_quads = {
        Quad::SolidColorQuad({1.0, 0.0, 1.0, 1.0f / 255.0f}, gfx::Rect(5, 5))};

    std::vector<Pass> root_passes = {
        Pass(root_render_pass_quads, CompositorRenderPassId{2}, kSurfaceSize)};

    CompositorFrame root_frame = MakeEmptyCompositorFrame();
    AddPasses(&root_frame.render_pass_list, root_passes,
              &root_frame.metadata.referenced_surfaces);

    root_frame.render_pass_list[0]->damage_rect = gfx::Rect(1, 2, 3, 4);

    root_sink_->SubmitCompositorFrame(second_root_surface_id.local_surface_id(),
                                      std::move(root_frame));
  }
  {
    auto aggregated_frame = AggregateFrame(second_root_surface_id);

    const auto& aggregated_pass_list = aggregated_frame.render_pass_list;

    ASSERT_EQ(1u, aggregated_pass_list.size());

    EXPECT_EQ(gfx::Rect(1, 2, 3, 4), aggregated_pass_list[0]->damage_rect);
  }
  {
    auto aggregated_frame = AggregateFrame(second_root_surface_id);

    const auto& aggregated_pass_list = aggregated_frame.render_pass_list;

    ASSERT_EQ(1u, aggregated_pass_list.size());

    // No new frame, so no new damage.
    EXPECT_TRUE(aggregated_pass_list[0]->damage_rect.IsEmpty());
  }
}

// Verifies that damage to any surface between primary and fallback damages the
// display if primary and fallback have the FrameSinkId.
TEST_F(SurfaceAggregatorValidSurfaceTest, SurfaceDamageSameFrameSinkId) {
  auto embedded_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryFrameSinkId1, /*is_root=*/true);
  ParentLocalSurfaceIdAllocator child_allocator;
  child_allocator.GenerateId();
  LocalSurfaceId id1 = child_allocator.GetCurrentLocalSurfaceId();
  child_allocator.GenerateId();
  LocalSurfaceId id2 = child_allocator.GetCurrentLocalSurfaceId();
  child_allocator.GenerateId();
  LocalSurfaceId id3 = child_allocator.GetCurrentLocalSurfaceId();
  child_allocator.GenerateId();
  LocalSurfaceId id4 = child_allocator.GetCurrentLocalSurfaceId();
  child_allocator.GenerateId();
  LocalSurfaceId id5 = child_allocator.GetCurrentLocalSurfaceId();
  SurfaceId fallback_surface_id(kArbitraryFrameSinkId1, id2);
  SurfaceId primary_surface_id(kArbitraryFrameSinkId1, id4);

  {
    CompositorFrame frame =
        CompositorFrameBuilder()
            .AddRenderPass(
                RenderPassBuilder(kSurfaceSize)
                    .AddSolidColorQuad(gfx::Rect(5, 5), SkColors::kGreen)
                    .AddSolidColorQuad(gfx::Rect(5, 5), SkColors::kBlue))
            .Build();
    embedded_support->SubmitCompositorFrame(id2, std::move(frame));
  }

  {
    CompositorFrame frame =
        CompositorFrameBuilder()
            .AddRenderPass(
                RenderPassBuilder(kSurfaceSize)
                    .AddSurfaceQuad(
                        gfx::Rect(5, 5),
                        SurfaceRange(fallback_surface_id, primary_surface_id)))
            .Build();
    root_sink_->SubmitCompositorFrame(root_surface_id_.local_surface_id(),
                                      std::move(frame));
  }

  auto aggregated_frame = AggregateFrame(root_surface_id_);

  // |id1| is before the fallback id so it shouldn't damage the display.
  EXPECT_FALSE(aggregator_.CheckForDisplayDamage(
      SurfaceId(kArbitraryFrameSinkId1, id1)));

  // |id2| is the fallback id so it should damage the display.
  EXPECT_TRUE(aggregator_.CheckForDisplayDamage(
      SurfaceId(kArbitraryFrameSinkId1, id2)));

  // |id3| is between fallback and primary so it should damage the display.
  EXPECT_TRUE(aggregator_.CheckForDisplayDamage(
      SurfaceId(kArbitraryFrameSinkId1, id3)));

  // |id4| is the primary id so it should damage the display.
  EXPECT_TRUE(aggregator_.CheckForDisplayDamage(
      SurfaceId(kArbitraryFrameSinkId1, id4)));

  // |id5| is newer than the primary surface so it shouldn't damage display.
  EXPECT_FALSE(aggregator_.CheckForDisplayDamage(
      SurfaceId(kArbitraryFrameSinkId1, id5)));

  // This FrameSinkId is not embedded at all so it shouldn't damage the display.
  EXPECT_FALSE(aggregator_.CheckForDisplayDamage(
      SurfaceId(kArbitraryFrameSinkId3, id3)));
}

// Verifies that only damage to primary and fallback surfaces and nothing in
// between damages the display if primary and fallback have different
// FrameSinkIds.
TEST_F(SurfaceAggregatorValidSurfaceTest, SurfaceDamageDifferentFrameSinkId) {
  auto embedded_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryFrameSinkId1, /*is_root=*/true);
  ParentLocalSurfaceIdAllocator sink1_allocator;
  ParentLocalSurfaceIdAllocator sink2_allocator;
  ParentLocalSurfaceIdAllocator sink3_allocator;
  sink1_allocator.GenerateId();
  LocalSurfaceId id1 = sink1_allocator.GetCurrentLocalSurfaceId();
  sink1_allocator.GenerateId();
  LocalSurfaceId id2 = sink1_allocator.GetCurrentLocalSurfaceId();
  sink2_allocator.GenerateId();
  LocalSurfaceId id3 = sink2_allocator.GetCurrentLocalSurfaceId();
  sink2_allocator.GenerateId();
  LocalSurfaceId id4 = sink2_allocator.GetCurrentLocalSurfaceId();
  SurfaceId fallback_surface_id(kArbitraryFrameSinkId1, id2);
  SurfaceId primary_surface_id(kArbitraryFrameSinkId2, id4);

  {
    CompositorFrame frame =
        CompositorFrameBuilder()
            .AddRenderPass(
                RenderPassBuilder(kSurfaceSize)
                    .AddSolidColorQuad(gfx::Rect(5, 5), SkColors::kGreen)
                    .AddSolidColorQuad(gfx::Rect(5, 5), SkColors::kBlue))
            .Build();
    embedded_support->SubmitCompositorFrame(id2, std::move(frame));
  }

  {
    CompositorFrame frame =
        CompositorFrameBuilder()
            .AddRenderPass(
                RenderPassBuilder(kSurfaceSize)
                    .AddSurfaceQuad(
                        gfx::Rect(5, 5),
                        SurfaceRange(fallback_surface_id, primary_surface_id)))
            .Build();
    root_sink_->SubmitCompositorFrame(root_surface_id_.local_surface_id(),
                                      std::move(frame));
  }

  auto aggregated_frame = AggregateFrame(root_surface_id_);

  // |id1| is before the fallback id so it shouldn't damage the display.
  EXPECT_FALSE(aggregator_.CheckForDisplayDamage(
      SurfaceId(kArbitraryFrameSinkId1, id1)));

  // |id2| is the fallback id so it should damage the display.
  EXPECT_TRUE(aggregator_.CheckForDisplayDamage(
      SurfaceId(kArbitraryFrameSinkId1, id2)));

  // |id3| is before the primary and fallback has a different FrameSinkId so it
  // should damage the display.
  EXPECT_TRUE(aggregator_.CheckForDisplayDamage(
      SurfaceId(kArbitraryFrameSinkId2, id3)));

  // |id4| is the primary id so it should damage the display.
  EXPECT_TRUE(aggregator_.CheckForDisplayDamage(
      SurfaceId(kArbitraryFrameSinkId2, id4)));

  // This FrameSinkId is not embedded at all so it shouldn't damage the display.
  EXPECT_FALSE(aggregator_.CheckForDisplayDamage(
      SurfaceId(kArbitraryFrameSinkId3, id4)));
}

// Verifies that when only a primary surface is provided any damage to primary
// surface damages the display.
TEST_F(SurfaceAggregatorValidSurfaceTest, SurfaceDamagePrimarySurfaceOnly) {
  ParentLocalSurfaceIdAllocator allocator;
  allocator.GenerateId();
  LocalSurfaceId id1 = allocator.GetCurrentLocalSurfaceId();
  allocator.GenerateId();
  LocalSurfaceId id2 = allocator.GetCurrentLocalSurfaceId();
  allocator.GenerateId();
  LocalSurfaceId id3 = allocator.GetCurrentLocalSurfaceId();
  SurfaceId primary_surface_id(kArbitraryFrameSinkId1, id2);

  ParentLocalSurfaceIdAllocator allocator2;
  allocator.GenerateId();
  LocalSurfaceId id4 = allocator2.GetCurrentLocalSurfaceId();

  CompositorFrame frame =
      CompositorFrameBuilder()
          .AddRenderPass(RenderPassBuilder(kSurfaceSize)
                             .AddSurfaceQuad(gfx::Rect(5, 5),
                                             SurfaceRange(std::nullopt,
                                                          primary_surface_id)))
          .Build();
  root_sink_->SubmitCompositorFrame(root_surface_id_.local_surface_id(),
                                    std::move(frame));

  auto aggregated_frame = AggregateFrame(root_surface_id_);

  // |id1| is inside the range so it should damage the display.
  EXPECT_TRUE(aggregator_.CheckForDisplayDamage(
      SurfaceId(kArbitraryFrameSinkId1, id1)));

  // |id2| is the primary id so it should damage the display.
  EXPECT_TRUE(aggregator_.CheckForDisplayDamage(
      SurfaceId(kArbitraryFrameSinkId1, id2)));

  // |id3| is after the primary id so it shouldn't damage the display.
  EXPECT_FALSE(aggregator_.CheckForDisplayDamage(
      SurfaceId(kArbitraryFrameSinkId1, id3)));

  // This FrameSinkId is not embedded at all so it shouldn't damage the display.
  EXPECT_FALSE(aggregator_.CheckForDisplayDamage(
      SurfaceId(kArbitraryFrameSinkId3, id4)));
}

// Verifies that when primary and fallback ids are equal, only damage to that
// particular surface causes damage to display.
TEST_F(SurfaceAggregatorValidSurfaceTest,
       SurfaceDamagePrimaryAndFallbackEqual) {
  auto embedded_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryFrameSinkId1, /*is_root=*/true);
  ParentLocalSurfaceIdAllocator allocator;
  allocator.GenerateId();
  LocalSurfaceId id1 = allocator.GetCurrentLocalSurfaceId();
  allocator.GenerateId();
  LocalSurfaceId id2 = allocator.GetCurrentLocalSurfaceId();
  allocator.GenerateId();
  LocalSurfaceId id3 = allocator.GetCurrentLocalSurfaceId();
  SurfaceId surface_id(kArbitraryFrameSinkId1, id2);

  ParentLocalSurfaceIdAllocator allocator2;
  allocator2.GenerateId();
  LocalSurfaceId id4 = allocator2.GetCurrentLocalSurfaceId();

  {
    CompositorFrame frame =
        CompositorFrameBuilder()
            .AddRenderPass(
                RenderPassBuilder(kSurfaceSize)
                    .AddSolidColorQuad(gfx::Rect(5, 5), SkColors::kGreen)
                    .AddSolidColorQuad(gfx::Rect(5, 5), SkColors::kBlue))
            .Build();
    embedded_support->SubmitCompositorFrame(id2, std::move(frame));
  }

  {
    CompositorFrame frame =
        CompositorFrameBuilder()
            .AddRenderPass(
                RenderPassBuilder(kSurfaceSize)
                    .AddSurfaceQuad(gfx::Rect(5, 5), SurfaceRange(surface_id)))
            .Build();
    root_sink_->SubmitCompositorFrame(root_surface_id_.local_surface_id(),
                                      std::move(frame));
  }

  auto aggregated_frame = AggregateFrame(root_surface_id_);

  // |id1| is before the fallback id so it shouldn't damage the display.
  EXPECT_FALSE(aggregator_.CheckForDisplayDamage(
      SurfaceId(kArbitraryFrameSinkId1, id1)));

  // |id2| is the embedded id so it should damage the display.
  EXPECT_TRUE(aggregator_.CheckForDisplayDamage(
      SurfaceId(kArbitraryFrameSinkId1, id2)));

  // |id3| is newer than primary id so it shouldn't damage the display.
  EXPECT_FALSE(aggregator_.CheckForDisplayDamage(
      SurfaceId(kArbitraryFrameSinkId1, id3)));

  // This FrameSinkId is not embedded at all so it shouldn't damage the display.
  EXPECT_FALSE(aggregator_.CheckForDisplayDamage(
      SurfaceId(kArbitraryFrameSinkId3, id4)));
}

// Tests the behavior of |intersects_damage_under| flag on a
// CompositorRenderPassDrawQuad, which should reset to false if the damage from
// quads below drawing to the same target intersects the RPDQ's rect, or
// otherwise remain unchanged.
TEST_P(SurfaceAggregatorValidSurfaceWithMergingPassesTest,
       RPDQBackdropFilterCacheFlagTest1) {
  // Add callbacks for when the root and child surfaces are damaged.
  MockAggregatedDamageCallback aggregated_damage_callback;
  root_sink_->SetAggregatedDamageCallbackForTesting(
      aggregated_damage_callback.GetCallback());
  child_sink_->SetAggregatedDamageCallbackForTesting(
      aggregated_damage_callback.GetCallback());

  gfx::Size child_surface_size(70, 70);
  // A simple child surface whose frame contains a single render pass with a
  // solid color quad.
  std::vector<Quad> child_quads = {
      Quad::SolidColorQuad(SkColors::kGreen, gfx::Rect(5, 5))};

  std::vector<Pass> child_passes = {
      Pass(child_quads, CompositorRenderPassId{1}, child_surface_size)};

  TestSurfaceIdAllocator child_surface_id(child_sink_->frame_sink_id());
  {
    CompositorFrame child_frame = MakeEmptyCompositorFrame();
    AddPasses(&child_frame.render_pass_list, child_passes,
              &child_frame.metadata.referenced_surfaces);

    child_sink_->SubmitCompositorFrame(child_surface_id.local_surface_id(),
                                       std::move(child_frame));
  }

  // The frame of the root surface has two render passes.
  // - the first pass has a solid color quad,
  // - the second pass has a render pass quad (RPDQ) with its
  // |intersects_damage_under| set to true and a surface quad embedding the
  // child surface.
  const gfx::Rect child_pass_rect(80, 80);
  CompositorRenderPassList root_passes;
  root_passes.push_back(
      RenderPassBuilder(CompositorRenderPassId{1}, child_pass_rect)
          .AddSolidColorQuad(gfx::Rect(5, 5), SkColors::kRed)
          .Build());
  root_passes.push_back(
      RenderPassBuilder(CompositorRenderPassId{2}, kSurfaceSize)
          .SetDamageRect(gfx::Rect())
          // We will verify the correctness of the |intersects_damage_under|
          // flag on this quad.
          .AddRenderPassQuad(child_pass_rect, CompositorRenderPassId{1},
                             {.intersects_damage_under = false})
          .SetQuadToTargetTranslation(20, 30)
          .AddSurfaceQuad(gfx::Rect(90, 90), SurfaceRange(child_surface_id),
                          {.allow_merge = AllowMerge()})
          .Build());

  root_sink_->SubmitCompositorFrame(
      root_surface_id_.local_surface_id(),
      MakeCompositorFrame(CopyRenderPasses(root_passes)));

  size_t expected_num_passes_after_aggregation = AllowMerge() ? 2u : 3u;

  // First aggregation.
  {
    EXPECT_CALL(
        aggregated_damage_callback,
        OnAggregatedDamage(root_surface_id_.local_surface_id(), kSurfaceSize,
                           gfx::Rect(kSurfaceSize), next_display_time()));
    EXPECT_CALL(aggregated_damage_callback,
                OnAggregatedDamage(
                    child_surface_id.local_surface_id(), child_surface_size,
                    gfx::Rect(child_surface_size), next_display_time()));
    auto aggregated_frame = AggregateFrame(root_surface_id_);
    testing::Mock::VerifyAndClearExpectations(&aggregated_damage_callback);
    const auto& aggregated_pass_list = aggregated_frame.render_pass_list;
    ASSERT_EQ(expected_num_passes_after_aggregation,
              aggregated_pass_list.size());

    // Root frame damage rect for the first aggregation should contain the
    // entire root rect.
    EXPECT_EQ(gfx::Rect(kSurfaceSize),
              aggregated_pass_list.back()->damage_rect);
    const auto* quad_to_test = aggregated_pass_list.back()->quad_list.front();
    const auto* rp_quad =
        AggregatedRenderPassDrawQuad::MaterialCast(quad_to_test);
    // The |quad_to_test| (20,30 80x80) intersects with damage below, which is
    // union of surface quad damage (0,0 70x70) and root surface damage (0,0
    // 100x100), so its |intersects_damage_under| is reset to false.
    EXPECT_TRUE(rp_quad->intersects_damage_under);
  }

  // Resubmit root frame and since there'll be no damage under the RPDQ with the
  // |intersects_damage_under|, the flag retains its original value (true).
  {
    root_sink_->SubmitCompositorFrame(
        root_surface_id_.local_surface_id(),
        MakeCompositorFrame(std::move(root_passes)));
    EXPECT_CALL(
        aggregated_damage_callback,
        OnAggregatedDamage(root_surface_id_.local_surface_id(), _, _, _))
        .Times(0);

    // No damage is expected from the child surface.
    EXPECT_CALL(
        aggregated_damage_callback,
        OnAggregatedDamage(child_surface_id.local_surface_id(), _, _, _))
        .Times(0);

    auto aggregated_frame = AggregateFrame(root_surface_id_);
    const auto& aggregated_pass_list = aggregated_frame.render_pass_list;
    ASSERT_EQ(expected_num_passes_after_aggregation,
              aggregated_pass_list.size());

    EXPECT_EQ(gfx::Rect(), aggregated_pass_list.back()->damage_rect);
    const auto* quad_to_test = aggregated_pass_list.back()->quad_list.front();
    const auto* rp_quad =
        AggregatedRenderPassDrawQuad::MaterialCast(quad_to_test);
    // No damage under |quad_to_test| and its |intersects_damage_under| retains
    // its value.
    EXPECT_FALSE(rp_quad->intersects_damage_under);
  }

  // Damage on the child surface doesn't intersect the RPDQ with
  // |intersects_damage_under|.
  {
    CompositorFrame child_frame = MakeEmptyCompositorFrame();
    AddPasses(&child_frame.render_pass_list, child_passes,
              &child_frame.metadata.referenced_surfaces);
    auto* child_root_pass = child_frame.render_pass_list[0].get();
    child_root_pass->damage_rect = gfx::Rect(10, 10, 10, 10);

    child_sink_->SubmitCompositorFrame(child_surface_id.local_surface_id(),
                                       std::move(child_frame));

    // Both the root and child surface should expect the damage from the child
    // surface (10,10 10x10)
    const gfx::Rect expected_damage_rect(10, 10, 10, 10);
    EXPECT_CALL(
        aggregated_damage_callback,
        OnAggregatedDamage(root_surface_id_.local_surface_id(), kSurfaceSize,
                           expected_damage_rect, next_display_time()));
    EXPECT_CALL(aggregated_damage_callback,
                OnAggregatedDamage(child_surface_id.local_surface_id(),
                                   child_surface_size, expected_damage_rect,
                                   next_display_time()));

    auto aggregated_frame = AggregateFrame(root_surface_id_);
    const auto& aggregated_pass_list = aggregated_frame.render_pass_list;
    ASSERT_EQ(expected_num_passes_after_aggregation,
              aggregated_pass_list.size());
    const auto* quad_to_test = aggregated_pass_list.back()->quad_list.front();
    const auto* rp_quad =
        AggregatedRenderPassDrawQuad::MaterialCast(quad_to_test);
    // The |quad_to_test| (20,30 80x80) doesn't intersect the damage from the
    // surface quad below (10,10 10x10) and the |intersects_damage_under|
    // retains its value of true.
    EXPECT_FALSE(rp_quad->intersects_damage_under);
  }

  // Damage on the child surface intersects the RPDQ with
  // |intersects_damage_under|.
  {
    CompositorFrame child_frame = MakeEmptyCompositorFrame();
    AddPasses(&child_frame.render_pass_list, child_passes,
              &child_frame.metadata.referenced_surfaces);
    auto* child_root_pass = child_frame.render_pass_list[0].get();
    child_root_pass->damage_rect = gfx::Rect(60, 60, 10, 10);

    child_sink_->SubmitCompositorFrame(child_surface_id.local_surface_id(),
                                       std::move(child_frame));

    // Both the root and child surface should expect the damage from the child
    // surface (60,60 10x10)
    const gfx::Rect expected_damage_rect(60, 60, 10, 10);
    EXPECT_CALL(
        aggregated_damage_callback,
        OnAggregatedDamage(root_surface_id_.local_surface_id(), kSurfaceSize,
                           expected_damage_rect, next_display_time()));
    EXPECT_CALL(aggregated_damage_callback,
                OnAggregatedDamage(child_surface_id.local_surface_id(),
                                   child_surface_size, expected_damage_rect,
                                   next_display_time()));

    auto aggregated_frame = AggregateFrame(root_surface_id_);
    const auto& aggregated_pass_list = aggregated_frame.render_pass_list;
    ASSERT_EQ(expected_num_passes_after_aggregation,
              aggregated_pass_list.size());
    const auto* quad_to_test = aggregated_pass_list.back()->quad_list.front();
    const auto* rp_quad =
        AggregatedRenderPassDrawQuad::MaterialCast(quad_to_test);
    // The |quad_to_test| (20,30 80x80) intersects the damage from the surface
    // quad below (60,60 10x10) and the |intersects_damage_under| resets to
    // false.
    EXPECT_TRUE(rp_quad->intersects_damage_under);
  }
}

// Tests the behavior of |intersects_damage_under| flag on a
// CompositorRenderPassDrawQuad. When damage under the RPDQ is coming from quads
// that draw to a different render target, it should not affect the
// |intersects_damage_under| flag. However, if merging happens, the RPDQ on root
// render pass of the surface being merged should take damage from all quads
// under it in the same final render target.
TEST_P(SurfaceAggregatorValidSurfaceWithMergingPassesTest,
       RPDQBackdropFilterCacheFlagTest2) {
  auto second_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryFrameSinkId1, /*is_root=*/false);

  // Add callbacks for when the surfaces are damaged.
  MockAggregatedDamageCallback aggregated_damage_callback;
  root_sink_->SetAggregatedDamageCallbackForTesting(
      aggregated_damage_callback.GetCallback());
  child_sink_->SetAggregatedDamageCallbackForTesting(
      aggregated_damage_callback.GetCallback());
  second_support->SetAggregatedDamageCallbackForTesting(
      aggregated_damage_callback.GetCallback());

  gfx::Size child_surface_size(60, 60);
  // The child surface has two passes:
  // - the first pass contains a color quad.
  // - the second pass contains an RPDQ referencing the first pass and having a
  // |intersects_damage_under| flag set to true. This flag is going to be
  // the subject of this test.
  std::vector<std::unique_ptr<CompositorRenderPass>> child_passes;
  child_passes.push_back(
      RenderPassBuilder(CompositorRenderPassId{1}, child_surface_size)
          .AddSolidColorQuad(gfx::Rect(5, 5), SkColors::kGreen)
          .Build());
  child_passes.push_back(
      RenderPassBuilder(CompositorRenderPassId{2}, child_surface_size)
          .AddRenderPassQuad(gfx::Rect(child_surface_size),
                             CompositorRenderPassId{1},
                             {.intersects_damage_under = false})
          .SetDamageRect(gfx::Rect())
          .Build());

  TestSurfaceIdAllocator child_surface_id(child_sink_->frame_sink_id());

  child_sink_->SubmitCompositorFrame(
      child_surface_id.local_surface_id(),
      MakeCompositorFrame(CopyRenderPasses(child_passes)));

  // The second surface will be embedded into a surface quad on the root pass of
  // the root frame, under the surface quad containing the child surface.
  gfx::Size second_surface_size(80, 80);
  // The root render pass of the second surface has a solid color quad.
  std::vector<Quad> second_surface_quads = {
      Quad::SolidColorQuad(SkColors::kGreen, gfx::Rect(5, 5))};

  std::vector<Pass> second_surface_passes = {Pass(
      second_surface_quads, CompositorRenderPassId{1}, second_surface_size)};

  TestSurfaceIdAllocator second_surface_id(second_support->frame_sink_id());
  {
    CompositorFrame second_surface_frame = MakeEmptyCompositorFrame();
    AddPasses(&second_surface_frame.render_pass_list, second_surface_passes,
              &second_surface_frame.metadata.referenced_surfaces);

    second_support->SubmitCompositorFrame(second_surface_id.local_surface_id(),
                                          std::move(second_surface_frame));
  }

  // The frame of the root surface has one single render pass with a surface
  // quad containing the child surface and a second surface quad containing
  // the second surface.
  std::vector<Quad> render_pass_quads = {
      // The |allow_merge| flag of the surface quad would be set to true/false
      // according to the parameter of the test.
      Quad::SurfaceQuad(SurfaceRange(std::nullopt, child_surface_id),
                        SkColors::kWhite, gfx::Rect(90, 90),
                        /*stretch_content_to_fill_bounds=*/false, AllowMerge()),
      Quad::SurfaceQuad(SurfaceRange(std::nullopt, second_surface_id),
                        SkColors::kWhite, gfx::Rect(second_surface_size),
                        /*stretch_content_to_fill_bounds=*/false,
                        /*allow_merge=*/false)};

  std::vector<Pass> root_passes{
      Pass(render_pass_quads, CompositorRenderPassId{1}, kSurfaceSize)};

  CompositorFrame root_frame = MakeEmptyCompositorFrame();
  AddPasses(&root_frame.render_pass_list, root_passes,
            &root_frame.metadata.referenced_surfaces);
  root_frame.render_pass_list.back()
      ->shared_quad_state_list.front()
      ->quad_to_target_transform.Translate(20, 30);
  root_sink_->SubmitCompositorFrame(root_surface_id_.local_surface_id(),
                                    std::move(root_frame));

  size_t expected_num_passes_after_aggregation = AllowMerge() ? 3u : 4u;
  // First aggregation.
  {
    EXPECT_CALL(
        aggregated_damage_callback,
        OnAggregatedDamage(root_surface_id_.local_surface_id(), kSurfaceSize,
                           gfx::Rect(kSurfaceSize), next_display_time()));
    // The damage for the first aggregation should contain
    // the entire child surface (0,0 60x60).
    gfx::Rect expected_child_damage_rect = gfx::Rect(child_surface_size);
    EXPECT_CALL(aggregated_damage_callback,
                OnAggregatedDamage(
                    child_surface_id.local_surface_id(), child_surface_size,
                    expected_child_damage_rect, next_display_time()));
    EXPECT_CALL(aggregated_damage_callback,
                OnAggregatedDamage(
                    second_surface_id.local_surface_id(), second_surface_size,
                    gfx::Rect(second_surface_size), next_display_time()));

    auto aggregated_frame = AggregateFrame(root_surface_id_);
    testing::Mock::VerifyAndClearExpectations(&aggregated_damage_callback);
    const auto& aggregated_pass_list = aggregated_frame.render_pass_list;
    ASSERT_EQ(expected_num_passes_after_aggregation,
              aggregated_pass_list.size());

    // Root frame damage rect for the first aggregation should contain the
    // entire root rect.
    EXPECT_EQ(gfx::Rect(kSurfaceSize),
              aggregated_pass_list.back()->damage_rect);
    const auto* quad_to_test =
        aggregated_pass_list[AllowMerge() ? 2 : 1]->quad_list.front();
    const auto* rp_quad =
        AggregatedRenderPassDrawQuad::MaterialCast(quad_to_test);
    // 1) Without merging, the |quad_to_test| (or more precisely, the
    // |output_rect| of the render pass referenced by the quad that's used for
    // damage intersection test) (0,0 60x60) has damage below from surface root
    // render pass (0,0 60x60), so its |intersects_damage_under| resets
    // to false.
    // 2) With merging, the |quad_to_test| would be merged to the root pass of
    // the root surface. The damage from below (0,0 100x100), which is the total
    // of the damage from second surface quad (0,0 80x80) and from root render
    // pass (0,0 100x100), is transformed into the local space of the child
    // surface as (-20,-30 100x100) and it intersects |quad_to_test|(0,0 60x60),
    // so its |intersects_damage_under| resets to false.
    EXPECT_TRUE(rp_quad->intersects_damage_under);
  }

  // Resubmit child frame and since there'll be no damage under the RPDQ with
  // the |intersects_damage_under|, the flag retains its original value (true).
  {
    child_sink_->SubmitCompositorFrame(
        child_surface_id.local_surface_id(),
        MakeCompositorFrame(std::move(child_passes)));
    EXPECT_CALL(
        aggregated_damage_callback,
        OnAggregatedDamage(root_surface_id_.local_surface_id(), _, _, _))
        .Times(0);

    // There should be no damage on any surface.
    EXPECT_CALL(
        aggregated_damage_callback,
        OnAggregatedDamage(child_surface_id.local_surface_id(), _, _, _))
        .Times(0);

    EXPECT_CALL(
        aggregated_damage_callback,
        OnAggregatedDamage(second_surface_id.local_surface_id(), _, _, _))
        .Times(0);

    auto aggregated_frame = AggregateFrame(root_surface_id_);
    const auto& aggregated_pass_list = aggregated_frame.render_pass_list;
    ASSERT_EQ(expected_num_passes_after_aggregation,
              aggregated_pass_list.size());

    EXPECT_EQ(gfx::Rect(), aggregated_pass_list.back()->damage_rect);
    const auto* quad_to_test =
        aggregated_pass_list[AllowMerge() ? 2 : 1]->quad_list.front();
    const auto* rp_quad =
        AggregatedRenderPassDrawQuad::MaterialCast(quad_to_test);
    // With or without merging, the |quad_to_test| (0,0 60x60) has no damage
    // from under it, so its |intersects_damage_under| remains unchanged.
    EXPECT_FALSE(rp_quad->intersects_damage_under);
  }

  // Damage on the second surface doesn't intersect the RPDQ with
  // |intersects_damage_under|.
  {
    CompositorFrame second_surface_frame = MakeEmptyCompositorFrame();
    AddPasses(&second_surface_frame.render_pass_list, second_surface_passes,
              &second_surface_frame.metadata.referenced_surfaces);
    auto* second_surface_root_pass =
        second_surface_frame.render_pass_list[0].get();
    second_surface_root_pass->damage_rect = gfx::Rect(10, 10, 10, 10);

    second_support->SubmitCompositorFrame(second_surface_id.local_surface_id(),
                                          std::move(second_surface_frame));

    const gfx::Rect expected_damage_rect(10, 10, 10, 10);
    EXPECT_CALL(
        aggregated_damage_callback,
        OnAggregatedDamage(root_surface_id_.local_surface_id(), kSurfaceSize,
                           expected_damage_rect, next_display_time()));
    // There is no damage on the child surface.
    EXPECT_CALL(
        aggregated_damage_callback,
        OnAggregatedDamage(child_surface_id.local_surface_id(), _, _, _))
        .Times(0);

    EXPECT_CALL(aggregated_damage_callback,
                OnAggregatedDamage(second_surface_id.local_surface_id(),
                                   second_surface_size, expected_damage_rect,
                                   next_display_time()));
    auto aggregated_frame = AggregateFrame(root_surface_id_);
    const auto& aggregated_pass_list = aggregated_frame.render_pass_list;
    ASSERT_EQ(expected_num_passes_after_aggregation,
              aggregated_pass_list.size());

    EXPECT_EQ(expected_damage_rect, aggregated_pass_list.back()->damage_rect);
    const auto* quad_to_test =
        aggregated_pass_list[AllowMerge() ? 2 : 1]->quad_list.front();
    const auto* rp_quad =
        AggregatedRenderPassDrawQuad::MaterialCast(quad_to_test);
    // 1) Without merging, the |quad_to_test| (0,0 60x60) doesn't have any
    // damage coming from under it and its |intersects_damage_under| flag
    // remains unchanged (true).
    // 2) With merging, the |quad_to_test| (0,0 60x60) doesn't intersect the
    // damage from under it transformed into its local space (-10,-20 10x10),
    // and its |intersects_damage_under| flag remains unchanged (true).
    EXPECT_FALSE(rp_quad->intersects_damage_under);
  }

  // Damage on the second surface intersects the RPDQ with
  // |intersects_damage_under|.
  {
    CompositorFrame second_surface_frame = MakeEmptyCompositorFrame();
    AddPasses(&second_surface_frame.render_pass_list, second_surface_passes,
              &second_surface_frame.metadata.referenced_surfaces);
    auto* second_surface_root_pass =
        second_surface_frame.render_pass_list[0].get();
    second_surface_root_pass->damage_rect = gfx::Rect(60, 60, 10, 10);

    second_support->SubmitCompositorFrame(second_surface_id.local_surface_id(),
                                          std::move(second_surface_frame));
    const gfx::Rect expected_damage_rect(60, 60, 10, 10);
    EXPECT_CALL(
        aggregated_damage_callback,
        OnAggregatedDamage(root_surface_id_.local_surface_id(), kSurfaceSize,
                           expected_damage_rect, next_display_time()));
    // There is no damage on the child surface.
    EXPECT_CALL(
        aggregated_damage_callback,
        OnAggregatedDamage(child_surface_id.local_surface_id(), _, _, _))
        .Times(0);

    EXPECT_CALL(aggregated_damage_callback,
                OnAggregatedDamage(second_surface_id.local_surface_id(),
                                   second_surface_size, expected_damage_rect,
                                   next_display_time()));

    auto aggregated_frame = AggregateFrame(root_surface_id_);
    const auto& aggregated_pass_list = aggregated_frame.render_pass_list;
    ASSERT_EQ(expected_num_passes_after_aggregation,
              aggregated_pass_list.size());

    EXPECT_EQ(expected_damage_rect, aggregated_pass_list.back()->damage_rect);
    const auto* quad_to_test =
        aggregated_pass_list[AllowMerge() ? 2 : 1]->quad_list.front();
    const auto* rp_quad =
        AggregatedRenderPassDrawQuad::MaterialCast(quad_to_test);
    // 1) Without merging, the |quad_to_test| (0,0 60x60) doesn't have any
    // damage from under it and its |intersects_damage_under| remains unchanged
    // (true).
    // 2) With merging, the |quad_to_test| (0,0 60x60) intersects the damage
    // passed on from second surface and transformed into the child local space
    // (40,30 10x10) and its |intersects_damage_under| resets to false.
    EXPECT_EQ(AllowMerge(), rp_quad->intersects_damage_under);
  }
}

// Tests that RenderPassDrawQud's |intersects_damage_under| flag is updated
// correctly with surface damage.
TEST_P(SurfaceAggregatorValidSurfaceWithMergingPassesTest,
       RPDQCanUseBackdropFilterCache) {
  auto parent_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryMiddleFrameSinkId, /*is_root=*/false);

  // Add a callback for when the surface is damaged.
  MockAggregatedDamageCallback aggregated_damage_callback;
  root_sink_->SetAggregatedDamageCallbackForTesting(
      aggregated_damage_callback.GetCallback());
  child_sink_->SetAggregatedDamageCallbackForTesting(
      aggregated_damage_callback.GetCallback());
  parent_support->SetAggregatedDamageCallbackForTesting(
      aggregated_damage_callback.GetCallback());

  constexpr gfx::Size child_surface_size(80, 80);
  std::vector<Quad> child_quads = {
      Quad::SolidColorQuad(SkColors::kGreen, gfx::Rect(5, 5))};
  std::vector<Pass> child_passes = {
      Pass(child_quads, CompositorRenderPassId{1}, child_surface_size)};

  TestSurfaceIdAllocator child_surface_id(child_sink_->frame_sink_id());
  {
    CompositorFrame child_frame = MakeEmptyCompositorFrame();
    AddPasses(&child_frame.render_pass_list, child_passes,
              &child_frame.metadata.referenced_surfaces);

    auto* child_root_pass = child_frame.render_pass_list[0].get();
    auto* child_root_pass_sqs = child_root_pass->shared_quad_state_list.front();
    child_root_pass_sqs->quad_to_target_transform.Translate(8, 0);

    child_sink_->SubmitCompositorFrame(child_surface_id.local_surface_id(),
                                       std::move(child_frame));
  }

  constexpr gfx::Size parent_surface_size(90, 90);
  std::vector<Quad> parent_surface_quads = {Quad::SurfaceQuad(
      SurfaceRange(std::nullopt, child_surface_id), SkColors::kWhite,
      gfx::Rect(kSurfaceSize), /*stretch_content_to_fill_bounds=*/false,
      AllowMerge())};
  std::vector<Pass> parent_surface_passes = {Pass(
      parent_surface_quads, CompositorRenderPassId{1}, parent_surface_size)};

  // Parent surface is only used to test if the transform is applied correctly
  // to the child surface's damage.
  CompositorFrame parent_surface_frame = MakeEmptyCompositorFrame();
  AddPasses(&parent_surface_frame.render_pass_list, parent_surface_passes,
            &parent_surface_frame.metadata.referenced_surfaces);

  TestSurfaceIdAllocator parent_surface_id(parent_support->frame_sink_id());
  parent_support->SubmitCompositorFrame(parent_surface_id.local_surface_id(),
                                        std::move(parent_surface_frame));

  std::vector<Quad> render_pass_1_quads = {
      Quad::SolidColorQuad(SkColors::kWhite, gfx::Rect(5, 5))};

  std::vector<Quad> render_pass_2_quads = {
      // Set the |intersects_damage_under| of this CompositorRenderPassDrawQuad
      // to be true. This is the quad that we are testing here. The
      // |intersects_damage_under| should be updated correctly based on the
      // damage of the SurfaceDrawQuad under it.
      Quad::RenderPassQuad(CompositorRenderPassId{1}, gfx::Transform(),
                           /*intersects_damage_under=*/false),
      Quad::SurfaceQuad(SurfaceRange(std::nullopt, parent_surface_id),
                        SkColors::kWhite, gfx::Rect(kSurfaceSize),
                        /*stretch_content_to_fill_bounds=*/false,
                        AllowMerge())};

  std::vector<Pass> root_passes{
      Pass(render_pass_1_quads, CompositorRenderPassId{1}, gfx::Size(50, 50)),
      Pass(render_pass_2_quads, CompositorRenderPassId{2}, kSurfaceSize)};

  {
    CompositorFrame root_frame = MakeEmptyCompositorFrame();
    AddPasses(&root_frame.render_pass_list, root_passes,
              &root_frame.metadata.referenced_surfaces);

    root_frame.render_pass_list[0]
        ->shared_quad_state_list.front()
        ->quad_to_target_transform.Translate(0, 10);
    root_frame.render_pass_list[0]->damage_rect = gfx::Rect(5, 5, 10, 10);
    root_frame.render_pass_list[1]->damage_rect = gfx::Rect(5, 5, 100, 100);

    root_sink_->SubmitCompositorFrame(root_surface_id_.local_surface_id(),
                                      std::move(root_frame));
  }
  size_t expected_num_passes_after_aggregation = AllowMerge() ? 2u : 4u;

  {
    // First aggregation.
    EXPECT_CALL(
        aggregated_damage_callback,
        OnAggregatedDamage(root_surface_id_.local_surface_id(), kSurfaceSize,
                           gfx::Rect(kSurfaceSize), next_display_time()));
    EXPECT_CALL(aggregated_damage_callback,
                OnAggregatedDamage(
                    child_surface_id.local_surface_id(), child_surface_size,
                    gfx::Rect(child_surface_size), next_display_time()));
    EXPECT_CALL(aggregated_damage_callback,
                OnAggregatedDamage(
                    parent_surface_id.local_surface_id(), parent_surface_size,
                    gfx::Rect(parent_surface_size), next_display_time()));
    auto aggregated_frame = AggregateFrame(root_surface_id_);
    testing::Mock::VerifyAndClearExpectations(&aggregated_damage_callback);
    const auto& aggregated_pass_list = aggregated_frame.render_pass_list;
    // After aggregation, there should be two render passes with merging
    // or four render passes without merging.
    ASSERT_EQ(expected_num_passes_after_aggregation,
              aggregated_pass_list.size());
    EXPECT_EQ(gfx::Rect(kSurfaceSize),
              aggregated_pass_list.back()->damage_rect);

    // The damage rect from under |quad_to_test| (0,0 100x100) intersects quad
    // render pass output rect (0,0 50x50).
    const auto* quad_to_test = aggregated_pass_list.back()->quad_list.front();
    const auto* rp_quad =
        AggregatedRenderPassDrawQuad::MaterialCast(quad_to_test);
    EXPECT_TRUE(rp_quad->intersects_damage_under);
  }

  // Resubmit root frame, |intersects_damage_under| retains its original value
  // (true).
  {
    CompositorFrame root_frame = MakeEmptyCompositorFrame();
    AddPasses(&root_frame.render_pass_list, root_passes,
              &root_frame.metadata.referenced_surfaces);
    root_frame.render_pass_list.back()->damage_rect = gfx::Rect();
    root_sink_->SubmitCompositorFrame(root_surface_id_.local_surface_id(),
                                      std::move(root_frame));
    EXPECT_CALL(
        aggregated_damage_callback,
        OnAggregatedDamage(root_surface_id_.local_surface_id(), _, _, _))
        .Times(0);
    EXPECT_CALL(
        aggregated_damage_callback,
        OnAggregatedDamage(child_surface_id.local_surface_id(), _, _, _))
        .Times(0);
    EXPECT_CALL(
        aggregated_damage_callback,
        OnAggregatedDamage(parent_surface_id.local_surface_id(), _, _, _))
        .Times(0);
    auto aggregated_frame = AggregateFrame(root_surface_id_);
    const auto& aggregated_pass_list = aggregated_frame.render_pass_list;
    ASSERT_EQ(expected_num_passes_after_aggregation,
              aggregated_pass_list.size());

    // There is no damage from under |quad_to_test| so |intersects_damage_under|
    // remains true.
    const auto* quad_to_test = aggregated_pass_list.back()->quad_list.front();
    const auto* rp_quad =
        AggregatedRenderPassDrawQuad::MaterialCast(quad_to_test);
    EXPECT_FALSE(rp_quad->intersects_damage_under);
  }

  // Change in inner surface that overlaps with the testing quad causes
  // |intersects_damage_under| to become false.
  {
    CompositorFrame child_frame = MakeEmptyCompositorFrame();
    AddPasses(&child_frame.render_pass_list, child_passes,
              &child_frame.metadata.referenced_surfaces);

    auto* child_root_pass = child_frame.render_pass_list[0].get();
    auto* child_root_pass_sqs = child_root_pass->shared_quad_state_list.front();
    child_root_pass_sqs->quad_to_target_transform.Translate(8, 0);
    child_root_pass->damage_rect = gfx::Rect(1, 1, 10, 10);

    child_sink_->SubmitCompositorFrame(child_surface_id.local_surface_id(),
                                       std::move(child_frame));

    // Outer surface didn't change, so a transformed inner damage rect is
    // expected.
    const gfx::Rect expected_damage_rect(1, 1, 10, 10);
    EXPECT_CALL(
        aggregated_damage_callback,
        OnAggregatedDamage(root_surface_id_.local_surface_id(), kSurfaceSize,
                           expected_damage_rect, next_display_time()));
    EXPECT_CALL(aggregated_damage_callback,
                OnAggregatedDamage(child_surface_id.local_surface_id(),
                                   child_surface_size, expected_damage_rect,
                                   next_display_time()));
    EXPECT_CALL(aggregated_damage_callback,
                OnAggregatedDamage(parent_surface_id.local_surface_id(),
                                   parent_surface_size, expected_damage_rect,
                                   next_display_time()));

    auto aggregated_frame = AggregateFrame(root_surface_id_);
    const auto& aggregated_pass_list = aggregated_frame.render_pass_list;
    ASSERT_EQ(expected_num_passes_after_aggregation,
              aggregated_pass_list.size());
    EXPECT_EQ(expected_damage_rect.ToString(),
              aggregated_pass_list.back()->damage_rect.ToString());

    // The damage rect from under |quad_to_test| (1,1 10x10) intersects quad
    // render pass output rect (0,0 50x50).
    const auto* quad_to_test = aggregated_pass_list.back()->quad_list.front();
    const auto* rp_quad =
        AggregatedRenderPassDrawQuad::MaterialCast(quad_to_test);
    EXPECT_TRUE(rp_quad->intersects_damage_under);
  }

  // No Surface changed, |intersects_damage_under| retains its original value
  // (true).
  {
    CompositorFrame root_frame = MakeEmptyCompositorFrame();
    AddPasses(&root_frame.render_pass_list, root_passes,
              &root_frame.metadata.referenced_surfaces);
    root_frame.render_pass_list.back()->damage_rect = gfx::Rect();
    root_sink_->SubmitCompositorFrame(root_surface_id_.local_surface_id(),
                                      std::move(root_frame));
    EXPECT_CALL(
        aggregated_damage_callback,
        OnAggregatedDamage(root_surface_id_.local_surface_id(), _, _, _))
        .Times(0);
    EXPECT_CALL(
        aggregated_damage_callback,
        OnAggregatedDamage(child_surface_id.local_surface_id(), _, _, _))
        .Times(0);
    EXPECT_CALL(
        aggregated_damage_callback,
        OnAggregatedDamage(parent_surface_id.local_surface_id(), _, _, _))
        .Times(0);

    auto aggregated_frame = AggregateFrame(root_surface_id_);
    const auto& aggregated_pass_list = aggregated_frame.render_pass_list;
    ASSERT_EQ(expected_num_passes_after_aggregation,
              aggregated_pass_list.size());

    // There is no damage from under |quad_to_test| so |intersects_damage_under|
    // remains true.
    const auto* quad_to_test = aggregated_pass_list.back()->quad_list.front();
    const auto* rp_quad =
        AggregatedRenderPassDrawQuad::MaterialCast(quad_to_test);
    EXPECT_FALSE(rp_quad->intersects_damage_under);
  }
}

// Verifies the |intersects_damage_under| flag on a CompositorRenderPassDrawQuad
// is updated correctly in cases involving a child surface that is embedded
// twice in the root surface and whose damage affects the flag.
TEST_P(SurfaceAggregatorValidSurfaceWithMergingPassesTest,
       RPDQCanUseBackdropFilterCacheTestWithMultiplyEmbeddedSurface) {
  // Add a callback for when the surface is damaged.
  MockAggregatedDamageCallback aggregated_damage_callback;
  root_sink_->SetAggregatedDamageCallbackForTesting(
      aggregated_damage_callback.GetCallback());
  child_sink_->SetAggregatedDamageCallbackForTesting(
      aggregated_damage_callback.GetCallback());

  // The child surface consists of a single render pass containing a single
  // solid color draw quad.
  const gfx::Size child_surface_size(20, 20);
  std::vector<Quad> child_quads = {
      Quad::SolidColorQuad(SkColors::kGreen, gfx::Rect(5, 5))};
  std::vector<Pass> child_passes = {Pass(child_quads, child_surface_size)};

  CompositorFrame child_frame = MakeEmptyCompositorFrame();
  AddPasses(&child_frame.render_pass_list, child_passes,
            &child_frame.metadata.referenced_surfaces);

  TestSurfaceIdAllocator child_surface_id(child_sink_->frame_sink_id());
  child_sink_->SubmitCompositorFrame(child_surface_id.local_surface_id(),
                                     std::move(child_frame));

  // The root surface consists of four render passes. In top-down order, they
  // are:
  //  1) The first one contains a color draw quad.
  //  2) The second one contains a surface draw quad referencing the child
  //     surface.
  //  3) The third one contains a render pass draw quad referencing the second
  //     render pass with a scale transform applied.
  //  4) The fourth one contains three render pass draw quads.
  //        - one referencing the first render pass with a translation, and its
  //           |intersects_damage_under| is the target of this test.
  //        - one referencing the third render pass with another translation
  //           transform applied
  //        - one referencing the second render pass with no transform.
  gfx::Transform scale;
  scale.Scale(2.f, 2.f);

  CompositorRenderPassList root_passes;
  root_passes.push_back(
      RenderPassBuilder(CompositorRenderPassId{4}, gfx::Rect(5, 5))
          .AddSolidColorQuad(gfx::Rect(5, 5), SkColors::kRed)
          .Build());
  root_passes.push_back(
      RenderPassBuilder(CompositorRenderPassId{3}, kSurfaceSize)
          .AddSurfaceQuad(gfx::Rect(child_surface_size),
                          SurfaceRange(std::nullopt, child_surface_id),
                          {.allow_merge = AllowMerge()})
          .Build());
  root_passes.push_back(
      RenderPassBuilder(CompositorRenderPassId{2}, kSurfaceSize)
          .AddRenderPassQuad(gfx::Rect(kSurfaceSize), CompositorRenderPassId{3})
          .SetQuadToTargetTransform(scale)
          .Build());
  root_passes.push_back(
      RenderPassBuilder(CompositorRenderPassId{1}, kSurfaceSize)
          .AddRenderPassQuad(gfx::Rect(5, 5), CompositorRenderPassId{4},
                             {.intersects_damage_under = false})
          .SetQuadToTargetTranslation(2, 2)
          .AddRenderPassQuad(gfx::Rect(kSurfaceSize), CompositorRenderPassId{2})
          .SetQuadToTargetTranslation(30, 50)
          .AddRenderPassQuad(gfx::Rect(kSurfaceSize), CompositorRenderPassId{3})
          .Build());

  size_t expected_num_passes_after_aggregation = AllowMerge() ? 4u : 5u;
  {
    // First aggregation.
    CompositorFrame root_frame =
        MakeCompositorFrame(CopyRenderPasses(root_passes));

    root_sink_->SubmitCompositorFrame(root_surface_id_.local_surface_id(),
                                      std::move(root_frame));

    // The first aggregation has full damage.
    EXPECT_CALL(
        aggregated_damage_callback,
        OnAggregatedDamage(root_surface_id_.local_surface_id(), kSurfaceSize,
                           gfx::Rect(kSurfaceSize), next_display_time()));
    // The child surface is embedded twice so the callback is called twice.
    EXPECT_CALL(aggregated_damage_callback,
                OnAggregatedDamage(
                    child_surface_id.local_surface_id(), child_surface_size,
                    gfx::Rect(child_surface_size), next_display_time()))
        .Times(2);

    auto aggregated_frame = AggregateFrame(root_surface_id_);
    testing::Mock::VerifyAndClearExpectations(&aggregated_damage_callback);
    ASSERT_EQ(expected_num_passes_after_aggregation,
              aggregated_frame.render_pass_list.size());

    // The damage of the two RenderPassDrawQuads under the |quad_to_test|
    // is (0,0 230x250). It intersects the quad's render pass output rect of
    // (2,2 5x5).
    const auto* quad_to_test =
        aggregated_frame.render_pass_list.back()->quad_list.front();
    const auto* rp_quad =
        AggregatedRenderPassDrawQuad::MaterialCast(quad_to_test);
    EXPECT_TRUE(rp_quad->intersects_damage_under);
  }

  // Resubmit root frame.
  {
    CompositorFrame root_frame =
        MakeCompositorFrame(CopyRenderPasses(root_passes));
    root_frame.render_pass_list.back()->damage_rect = gfx::Rect();
    root_sink_->SubmitCompositorFrame(root_surface_id_.local_surface_id(),
                                      std::move(root_frame));

    EXPECT_CALL(
        aggregated_damage_callback,
        OnAggregatedDamage(root_surface_id_.local_surface_id(), _, _, _))
        .Times(0);
    EXPECT_CALL(
        aggregated_damage_callback,
        OnAggregatedDamage(child_surface_id.local_surface_id(), _, _, _))
        .Times(0);
    auto aggregated_frame = AggregateFrame(root_surface_id_);
    const auto& aggregated_pass_list = aggregated_frame.render_pass_list;
    ASSERT_EQ(expected_num_passes_after_aggregation,
              aggregated_pass_list.size());

    // There's no damage under |quad_to_test| (2,2 5x5) and the
    // |intersects_damage_under| flag of |quad_to_test| remains true.
    const auto* quad_to_test =
        aggregated_frame.render_pass_list.back()->quad_list.front();
    const auto* rp_quad =
        AggregatedRenderPassDrawQuad::MaterialCast(quad_to_test);
    EXPECT_FALSE(rp_quad->intersects_damage_under);
  }

  {
    // Damage the child surface at (10,10 10x10).
    CompositorFrame child_frame_2 = MakeEmptyCompositorFrame();
    AddPasses(&child_frame_2.render_pass_list, child_passes,
              &child_frame_2.metadata.referenced_surfaces);

    child_frame_2.render_pass_list.back()->damage_rect =
        gfx::Rect(10, 10, 10, 10);

    child_sink_->SubmitCompositorFrame(child_surface_id.local_surface_id(),
                                       std::move(child_frame_2));

    // The child surface is embedded twice in the root surface, so its damage
    // rect would appear in two locations in the root surface:
    //  1) The first embedding has no transform, so its damage rect would
    //  simply be (10,10 10x10).
    //  2) The second embedding is scaled by a factor of 2 and translated by
    //  (30,50). So, its damage rect would be (10*2+30,10*2+50 10*2x10*2) =
    //  (50,70 20x20).
    //  The above two damage rects are from under |quad_to_test|, and the
    //  unioned damage rect (10,10 60x80) doesn't intersects the quad's rect
    //  of (2,2 5x5), so |intersects_damage_under| flag of |quad_to_test|
    //  remains true.

    // The aggregated damage rect would be union of the above damage rects
    // which is (10,10 60x80).
    constexpr gfx::Rect expected_damage_rect(10, 10, 60, 80);
    constexpr gfx::Rect expected_child_damage_rect(10, 10, 10, 10);
    EXPECT_CALL(
        aggregated_damage_callback,
        OnAggregatedDamage(root_surface_id_.local_surface_id(), kSurfaceSize,
                           expected_damage_rect, next_display_time()));
    EXPECT_CALL(aggregated_damage_callback,
                OnAggregatedDamage(
                    child_surface_id.local_surface_id(), child_surface_size,
                    gfx::Rect(expected_child_damage_rect), next_display_time()))
        .Times(2);
    auto aggregated_frame_2 = AggregateFrame(root_surface_id_);
    testing::Mock::VerifyAndClearExpectations(&aggregated_damage_callback);

    const auto* quad_to_test =
        aggregated_frame_2.render_pass_list.back()->quad_list.front();
    const auto* rp_quad =
        AggregatedRenderPassDrawQuad::MaterialCast(quad_to_test);
    EXPECT_FALSE(rp_quad->intersects_damage_under);
  }

  {
    // Damage the child surface at (10,10 10x10) and translate
    // the quad with |intersects_damage_under| to be tested by (12,12).
    CompositorFrame child_frame_2 = MakeEmptyCompositorFrame();
    AddPasses(&child_frame_2.render_pass_list, child_passes,
              &child_frame_2.metadata.referenced_surfaces);

    child_frame_2.render_pass_list.back()->damage_rect =
        gfx::Rect(10, 10, 10, 10);

    child_sink_->SubmitCompositorFrame(child_surface_id.local_surface_id(),
                                       std::move(child_frame_2));

    // The total translation of the quad with |intersects_damage_under| to be
    // tested is now (12,12)
    gfx::Transform& tr = const_cast<gfx::Transform&>(
        root_passes.back()
            ->quad_list.ElementAt(0)
            ->shared_quad_state->quad_to_target_transform);
    tr.Translate(10, 10);
    CompositorFrame root_frame =
        MakeCompositorFrame(CopyRenderPasses(root_passes));

    root_frame.render_pass_list.back()->damage_rect = gfx::Rect();

    root_sink_->SubmitCompositorFrame(root_surface_id_.local_surface_id(),
                                      std::move(root_frame));

    constexpr gfx::Rect expected_damage_rect(10, 10, 60, 80);
    constexpr gfx::Rect expected_child_damage_rect(10, 10, 10, 10);
    EXPECT_CALL(
        aggregated_damage_callback,
        OnAggregatedDamage(root_surface_id_.local_surface_id(), kSurfaceSize,
                           expected_damage_rect, next_display_time()));
    EXPECT_CALL(aggregated_damage_callback,
                OnAggregatedDamage(
                    child_surface_id.local_surface_id(), child_surface_size,
                    gfx::Rect(expected_child_damage_rect), next_display_time()))
        .Times(2);
    auto aggregated_frame_2 = AggregateFrame(root_surface_id_);
    testing::Mock::VerifyAndClearExpectations(&aggregated_damage_callback);

    //  The unioned damage rect from under |quad_to_test| (10,10 60x80)
    //  intersects the quad's rect of (12,12 5x5), so
    //  |intersects_damage_under| flag of |quad_to_test| becomes false.

    const auto* quad_to_test =
        aggregated_frame_2.render_pass_list.back()->quad_list.front();
    const auto* rp_quad =
        AggregatedRenderPassDrawQuad::MaterialCast(quad_to_test);
    EXPECT_TRUE(rp_quad->intersects_damage_under);
  }
}

// Tests the behavior of pixel moving backdrop filter damage expansion when
// passes are merge and the parent surface has damage.
TEST_P(SurfaceAggregatorValidSurfaceWithMergingPassesTest,
       PixelMovingBackdropFilterDamageExpansion) {
  // Add callbacks for when the surfaces are damaged.
  MockAggregatedDamageCallback aggregated_damage_callback;
  root_sink_->SetAggregatedDamageCallbackForTesting(
      aggregated_damage_callback.GetCallback());
  child_sink_->SetAggregatedDamageCallbackForTesting(
      aggregated_damage_callback.GetCallback());

  gfx::Size child_surface_size(60, 60);
  // The child surface has three passes:
  // - the first pass contains solid color quad.
  // - the second pass contains a render pass quad referencing the first pass
  // with a blur backdrop filter.
  // - the third pass contains a render pass quad referencing the second pass.
  CompositorFrame child_frame =
      CompositorFrameBuilder()
          .AddRenderPass(
              RenderPassBuilder(CompositorRenderPassId{1}, child_surface_size)
                  .AddSolidColorQuad(gfx::Rect(5, 5), SkColors::kGreen))
          .AddRenderPass(
              RenderPassBuilder(CompositorRenderPassId{2}, child_surface_size)
                  .AddRenderPassQuad(gfx::Rect(child_surface_size),
                                     CompositorRenderPassId{1})
                  .AddBackdropFilter(cc::FilterOperation::CreateBlurFilter(5)))
          .AddRenderPass(
              RenderPassBuilder(CompositorRenderPassId{3}, child_surface_size)
                  .AddRenderPassQuad(gfx::Rect(child_surface_size),
                                     CompositorRenderPassId{2})
                  .SetQuadToTargetTranslation(20, 30))
          .Build();

  TestSurfaceIdAllocator child_surface_id(child_sink_->frame_sink_id());
  child_sink_->SubmitCompositorFrame(child_surface_id.local_surface_id(),
                                     std::move(child_frame));

  CompositorRenderPassList root_render_passes;
  root_render_passes.push_back(
      RenderPassBuilder(CompositorRenderPassId{1}, kSurfaceSize)
          .SetDamageRect(gfx::Rect(0, 0, 10, 20))
          .AddSurfaceQuad(gfx::Rect(90, 90), SurfaceRange(child_surface_id),
                          {.allow_merge = AllowMerge()})
          .SetQuadToTargetTranslation(5, 5)
          .Build());

  root_sink_->SubmitCompositorFrame(
      root_surface_id_.local_surface_id(),
      MakeCompositorFrame(CopyRenderPasses(root_render_passes)));

  size_t expected_num_passes_after_aggregation = AllowMerge() ? 3u : 4u;
  // First aggregation.
  {
    EXPECT_CALL(
        aggregated_damage_callback,
        OnAggregatedDamage(root_surface_id_.local_surface_id(), kSurfaceSize,
                           gfx::Rect(kSurfaceSize), next_display_time()));
    // In the local space of the root pass of the child frame, the second render
    // pass (20,30 60x60) has a blur backdrop filter. The entire child root
    // render pass is damaged which is (0, 0 60x60).
    gfx::Rect expected_child_damage_rect(0, 0, 60, 60);
    EXPECT_CALL(aggregated_damage_callback,
                OnAggregatedDamage(
                    child_surface_id.local_surface_id(), child_surface_size,
                    expected_child_damage_rect, next_display_time()));

    auto aggregated_frame = AggregateFrame(root_surface_id_);
    testing::Mock::VerifyAndClearExpectations(&aggregated_damage_callback);
    const auto& aggregated_pass_list = aggregated_frame.render_pass_list;
    ASSERT_EQ(expected_num_passes_after_aggregation,
              aggregated_pass_list.size());

    // Root frame damage rect for the first aggregation should contain the
    // entire root rect.
    EXPECT_EQ(gfx::Rect(kSurfaceSize),
              aggregated_pass_list.back()->damage_rect);
  }

  // Resubmit the root frame.
  {
    root_sink_->SubmitCompositorFrame(
        root_surface_id_.local_surface_id(),
        MakeCompositorFrame(std::move(root_render_passes)));
    EXPECT_CALL(
        aggregated_damage_callback,
        OnAggregatedDamage(root_surface_id_.local_surface_id(), kSurfaceSize,
                           gfx::Rect(0, 0, 10, 20), next_display_time()));
    // 1) Without merging, there is no damage on the child surface.
    // 2) With merging, in the local space of the root pass of the child frame,
    // the second render pass (20,30 60x60) has a blur backdrop filter.
    // However, the damage passed from parent surface and transformed into the
    // same local space is (-5,-5 10x20), so this damage is not expanded. The
    // child surface doesn't have any damage.
    EXPECT_CALL(
        aggregated_damage_callback,
        OnAggregatedDamage(child_surface_id.local_surface_id(), _, _, _))
        .Times(0);

    auto aggregated_frame = AggregateFrame(root_surface_id_);
    const auto& aggregated_pass_list = aggregated_frame.render_pass_list;
    ASSERT_EQ(expected_num_passes_after_aggregation,
              aggregated_pass_list.size());
    EXPECT_EQ(gfx::Rect(0, 0, 10, 20),
              aggregated_pass_list.back()->damage_rect);
  }
}

// Tests the behavior of pixel moving backdrop filter damage expansion when
// passes are merged, the parent surface has damage, and the merged surface
// is set to stretch its contents to fill bounds.
TEST_P(SurfaceAggregatorValidSurfaceWithMergingPassesTest,
       PixelMovingBackdropFilterDamageExpansionWithSurfaceStretch) {
  // Add callbacks for when the surfaces are damaged.
  MockAggregatedDamageCallback aggregated_damage_callback;
  root_sink_->SetAggregatedDamageCallbackForTesting(
      aggregated_damage_callback.GetCallback());
  child_sink_->SetAggregatedDamageCallbackForTesting(
      aggregated_damage_callback.GetCallback());

  gfx::Size child_surface_size(60, 60);
  // The child surface has three passes:
  // - the first pass contains solid color quad.
  // - the second pass contains a render pass quad referencing the first pass
  // with a blur backdrop filter.
  // - the third pass contains a render pass quad referencing the second pass.
  CompositorFrame child_frame =
      CompositorFrameBuilder()
          .AddRenderPass(
              RenderPassBuilder(CompositorRenderPassId{1}, child_surface_size)
                  .AddSolidColorQuad(gfx::Rect(5, 5), SkColors::kGreen))
          .AddRenderPass(
              RenderPassBuilder(CompositorRenderPassId{2}, child_surface_size)
                  .AddRenderPassQuad(gfx::Rect(child_surface_size),
                                     CompositorRenderPassId{1})
                  .AddBackdropFilter(cc::FilterOperation::CreateBlurFilter(5)))
          .AddRenderPass(
              RenderPassBuilder(CompositorRenderPassId{3}, child_surface_size)
                  .AddRenderPassQuad(gfx::Rect(child_surface_size),
                                     CompositorRenderPassId{2})
                  .SetQuadToTargetTranslation(20, 30))
          .Build();

  TestSurfaceIdAllocator child_surface_id(child_sink_->frame_sink_id());
  child_sink_->SubmitCompositorFrame(child_surface_id.local_surface_id(),
                                     std::move(child_frame));

  // The frame of the root surface has one single render pass with a surface
  // quad containing the child surface.
  std::vector<Quad> render_pass_quads = {
      // The |allow_merge| flag of the surface quad would be set to true/false
      // according to the parameter of the test.
      Quad::SurfaceQuad(SurfaceRange(std::nullopt, child_surface_id),
                        SkColors::kWhite, gfx::Rect(90, 90),
                        /*stretch_content_to_fill_bounds=*/true, AllowMerge())};

  std::vector<Pass> root_passes{
      Pass(render_pass_quads, CompositorRenderPassId{1}, kSurfaceSize)};
  root_passes[0].damage_rect = gfx::Rect(0, 0, 10, 20);

  {
    CompositorFrame root_frame = MakeEmptyCompositorFrame();
    AddPasses(&root_frame.render_pass_list, root_passes,
              &root_frame.metadata.referenced_surfaces);
    root_frame.render_pass_list.back()
        ->shared_quad_state_list.front()
        ->quad_to_target_transform.Translate(5, 5);
    root_sink_->SubmitCompositorFrame(root_surface_id_.local_surface_id(),
                                      std::move(root_frame));
  }

  size_t expected_num_passes_after_aggregation = AllowMerge() ? 3u : 4u;
  // First aggregation.
  {
    // In the local space of the root pass of the child frame, the second render
    // pass (20,30 60x60) has a blur backdrop filter. The entire child root
    // render pass is damaged which is (0, 0 60x60).
    gfx::Rect expected_child_damage_rect = gfx::Rect(0, 0, 60, 60);
    EXPECT_CALL(aggregated_damage_callback,
                OnAggregatedDamage(
                    child_surface_id.local_surface_id(), child_surface_size,
                    expected_child_damage_rect, next_display_time()));

    // 1) Without merging, child surface damage (0,0 80x90) stretches to (0,0
    // 120x135), and transformed to root surface as (5,5 120x135), unions root
    // surface damage (0,0 100,100) to (0,0 125x140).
    // 2) With merging, child surface damage (-4,-4 84x94) stretches to (-6,-6
    // 126x141), and transformed to root surface as (-1,-1 126x141).
    // In both cases the damage is clipped to the output rect of the root
    // surface (0,0 100x100).
    gfx::Rect expected_root_damage_rect = gfx::Rect(kSurfaceSize);
    EXPECT_CALL(
        aggregated_damage_callback,
        OnAggregatedDamage(root_surface_id_.local_surface_id(), kSurfaceSize,
                           expected_root_damage_rect, next_display_time()));

    auto aggregated_frame = AggregateFrame(root_surface_id_);
    testing::Mock::VerifyAndClearExpectations(&aggregated_damage_callback);
    const auto& aggregated_pass_list = aggregated_frame.render_pass_list;
    ASSERT_EQ(expected_num_passes_after_aggregation,
              aggregated_pass_list.size());

    EXPECT_EQ(gfx::Rect(kSurfaceSize),
              aggregated_pass_list.back()->damage_rect);
  }

  // Resubmit the root frame.
  {
    CompositorFrame root_frame = MakeEmptyCompositorFrame();
    AddPasses(&root_frame.render_pass_list, root_passes,
              &root_frame.metadata.referenced_surfaces);
    root_frame.render_pass_list.back()
        ->shared_quad_state_list.front()
        ->quad_to_target_transform.Translate(5, 5);
    root_sink_->SubmitCompositorFrame(root_surface_id_.local_surface_id(),
                                      std::move(root_frame));
    EXPECT_CALL(
        aggregated_damage_callback,
        OnAggregatedDamage(root_surface_id_.local_surface_id(), kSurfaceSize,
                           gfx::Rect(0, 0, 10, 20), next_display_time()));
    // 1) Without merging, there is no damage on the child surface.
    // 2) With merging, in the local space of the root pass of the child frame,
    // the second render pass (20,30 60x60) has a blur backdrop filter.
    // However, the damage passed from parent surface and transformed into the
    // same local space is (-4,-4 8x14), so this damage is not expanded. The
    // child surface doesn't have any damage.
    EXPECT_CALL(
        aggregated_damage_callback,
        OnAggregatedDamage(child_surface_id.local_surface_id(), _, _, _))
        .Times(0);

    auto aggregated_frame = AggregateFrame(root_surface_id_);
    const auto& aggregated_pass_list = aggregated_frame.render_pass_list;
    ASSERT_EQ(expected_num_passes_after_aggregation,
              aggregated_pass_list.size());
    EXPECT_EQ(gfx::Rect(0, 0, 10, 20),
              aggregated_pass_list.back()->damage_rect);
  }
}

using SurfaceAggregatorPartialSwapTest = SurfaceAggregatorValidSurfaceTest;

TEST_F(SurfaceAggregatorPartialSwapTest, ExpandByTargetDamage) {
  TestSurfaceIdAllocator child_surface_id(child_sink_->frame_sink_id());
  constexpr float device_scale_factor = 1.0f;

  // The child surface has one quad.
  {
    CompositorRenderPassId child_pass_id{1};
    std::vector<Quad> child_quads1 = {
        Quad::SolidColorQuad(SkColors::kGreen, gfx::Rect(5, 5))};
    std::vector<Pass> child_passes = {
        Pass(child_quads1, child_pass_id, gfx::Rect(5, 5))};

    CompositorRenderPassList child_pass_list;
    std::vector<SurfaceRange> referenced_surfaces;
    AddPasses(&child_pass_list, child_passes, &referenced_surfaces);

    SubmitPassListAsFrame(child_sink_.get(),
                          child_surface_id.local_surface_id(), &child_pass_list,
                          std::move(referenced_surfaces), device_scale_factor);
  }

  {
    std::vector<Quad> root_quads = {Quad::SurfaceQuad(
        SurfaceRange(std::nullopt, child_surface_id), SkColors::kWhite,
        gfx::Rect(kSurfaceSize), /*stretch_content_to_fill_bounds=*/false)};

    std::vector<Pass> root_passes = {Pass(root_quads, kSurfaceSize)};

    CompositorRenderPassList root_pass_list;
    std::vector<SurfaceRange> referenced_surfaces;
    AddPasses(&root_pass_list, root_passes, &referenced_surfaces);
    // No damage, this is the first frame submitted, so all quads should be
    // produced.
    SubmitPassListAsFrame(root_sink_.get(), root_surface_id_.local_surface_id(),
                          &root_pass_list, std::move(referenced_surfaces),
                          device_scale_factor);
  }

  {
    auto aggregated_frame = AggregateFrame(root_surface_id_);

    const auto& aggregated_pass_list = aggregated_frame.render_pass_list;

    ASSERT_EQ(1u, aggregated_pass_list.size());

    // Damage rect for first aggregation should contain entire root surface.
    EXPECT_EQ(gfx::Rect(kSurfaceSize),
              aggregated_pass_list.back()->damage_rect);
    EXPECT_EQ(1u, aggregated_pass_list[0]->quad_list.size());
  }

  // Create a root surface with a smaller damage rect.
  // This time the damage should be smaller.
  {
    std::vector<Quad> root_quads = {Quad::SurfaceQuad(
        SurfaceRange(std::nullopt, child_surface_id), SkColors::kWhite,
        gfx::Rect(kSurfaceSize), /*stretch_content_to_fill_bounds=*/false)};

    std::vector<Pass> root_passes = {Pass(root_quads, kSurfaceSize)};

    CompositorRenderPassList root_pass_list;
    std::vector<SurfaceRange> referenced_surfaces;
    AddPasses(&root_pass_list, root_passes, &referenced_surfaces);

    auto* root_pass = root_pass_list[0].get();
    root_pass->damage_rect = gfx::Rect(10, 10, 2, 2);
    SubmitPassListAsFrame(root_sink_.get(), root_surface_id_.local_surface_id(),
                          &root_pass_list, std::move(referenced_surfaces),
                          device_scale_factor);
  }

  {
    auto aggregated_frame = AggregateFrame(root_surface_id_);

    const auto& aggregated_pass_list = aggregated_frame.render_pass_list;

    ASSERT_EQ(1u, aggregated_pass_list.size());
    EXPECT_EQ(gfx::Rect(10, 10, 2, 2),
              aggregated_pass_list.back()->damage_rect);
  }

  // This pass has damage that does not intersect the quad in the child
  // surface.
  {
    std::vector<Quad> root_quads = {
        Quad::SurfaceQuad(SurfaceRange(std::nullopt, child_surface_id),
                          SkColors::kWhite, gfx::Rect(kSurfaceSize), false)};

    std::vector<Pass> root_passes = {Pass(root_quads, kSurfaceSize)};

    CompositorRenderPassList root_pass_list;
    std::vector<SurfaceRange> referenced_surfaces;
    AddPasses(&root_pass_list, root_passes, &referenced_surfaces);

    auto* root_pass = root_pass_list[0].get();
    root_pass->damage_rect = gfx::Rect(10, 10, 2, 2);
    SubmitPassListAsFrame(root_sink_.get(), root_surface_id_.local_surface_id(),
                          &root_pass_list, std::move(referenced_surfaces),
                          device_scale_factor);
  }

  // The target surface invalidates one pixel in the top left, the quad in the
  // child surface should be added even if it's not causing damage nor in the
  // root render pass damage.
  {
    gfx::Rect target_damage(0, 0, 1, 1);
    auto aggregated_frame = AggregateFrame(root_surface_id_, target_damage);

    const auto& aggregated_pass_list = aggregated_frame.render_pass_list;
    ASSERT_EQ(1u, aggregated_pass_list.size());

    // The damage rect of the root render pass should not be changed.
    EXPECT_EQ(gfx::Rect(10, 10, 2, 2),
              aggregated_pass_list.back()->damage_rect);
    // We expect one quad
    ASSERT_EQ(1u, aggregated_pass_list[0]->quad_list.size());
  }
}

class SurfaceAggregatorWithResourcesTest : public SurfaceAggregatorTest {
 public:
  SurfaceAggregatorWithResourcesTest()
      : SurfaceAggregatorTest(
            SurfaceAggregator::ExtraPassForReadbackOption::kNone,
            false) {
    // BuildCompositorFrameWithResources() sets secure_output_only=true on
    // TextureDrawQuads so this will ensure they aren't dropped from the
    // AggregatedFrame.
    aggregator_.set_output_is_secure(true);
  }

  void SendBeginFrame(CompositorFrameSinkSupport* support, uint64_t id) {
    BeginFrameArgs args =
        CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, id);
    support->OnBeginFrame(args);
  }
};

CompositorFrame BuildCompositorFrameWithResources(
    const std::vector<ResourceId>& resource_ids,
    bool valid,
    SurfaceId child_id) {
  CompositorFrame frame = MakeEmptyCompositorFrame();
  auto pass = CompositorRenderPass::Create();
  pass->SetNew(CompositorRenderPassId{1}, gfx::Rect(0, 0, 20, 20), gfx::Rect(),
               gfx::Transform());
  auto* sqs = pass->CreateAndAppendSharedQuadState();
  sqs->opacity = 1.f;
  if (child_id.is_valid()) {
    auto* surface_quad = pass->CreateAndAppendDrawQuad<SurfaceDrawQuad>();
    surface_quad->SetNew(sqs, gfx::Rect(0, 0, 1, 1), gfx::Rect(0, 0, 1, 1),
                         SurfaceRange(std::nullopt, child_id), SkColors::kWhite,
                         /*stretch_content_to_fill_bounds=*/false);
  }

  for (ResourceId resource_id : resource_ids) {
    auto resource = TransferableResource::MakeSoftwareSharedBitmap(
        SharedBitmap::GenerateId(), gpu::SyncToken(), gfx::Size(1, 1),
        SinglePlaneFormat::kRGBA_8888);
    resource.id = resource_id;
    if (!valid) {
      // ResourceProvider is software, so only software resources are valid. Do
      // this to cause the resource to be rejected.
      resource.is_software = false;
    }
    frame.resource_list.push_back(resource);
    auto* quad = pass->CreateAndAppendDrawQuad<TextureDrawQuad>();
    const gfx::Rect rect;
    const gfx::Rect visible_rect;
    bool needs_blending = false;
    bool premultiplied_alpha = false;
    const gfx::PointF uv_top_left;
    const gfx::PointF uv_bottom_right;
    SkColor4f background_color = SkColors::kGreen;
    bool flipped = false;
    bool nearest_neighbor = false;
    bool secure_output_only = true;
    gfx::ProtectedVideoType protected_video_type =
        gfx::ProtectedVideoType::kClear;
    quad->SetAll(sqs, rect, visible_rect, needs_blending, resource_id,
                 gfx::Size(), premultiplied_alpha, uv_top_left, uv_bottom_right,
                 background_color, flipped, nearest_neighbor,
                 secure_output_only, protected_video_type);
  }
  frame.render_pass_list.push_back(std::move(pass));
  return frame;
}

void SubmitCompositorFrameWithResources(
    const std::vector<ResourceId>& resource_ids,
    bool valid,
    SurfaceId child_id,
    CompositorFrameSinkSupport* support,
    SurfaceId surface_id) {
  auto frame = BuildCompositorFrameWithResources(resource_ids, valid, child_id);
  support->SubmitCompositorFrame(surface_id.local_surface_id(),
                                 std::move(frame));
}

TEST_F(SurfaceAggregatorWithResourcesTest, TakeResourcesOneSurface) {
  LocalSurfaceId local_surface_id(7u, base::UnguessableToken::Create());
  SurfaceId surface_id(root_sink_->frame_sink_id(), local_surface_id);

  std::vector<ResourceId> ids = {ResourceId(11), ResourceId(12),
                                 ResourceId(13)};
  SubmitCompositorFrameWithResources(ids, true, SurfaceId(), root_sink_.get(),
                                     surface_id);

  auto frame = AggregateFrame(surface_id);

  // Nothing should be available to be returned yet.
  EXPECT_TRUE(fake_client_.returned_resources().empty());

  SubmitCompositorFrameWithResources({}, true, SurfaceId(), root_sink_.get(),
                                     surface_id);

  frame = AggregateFrame(surface_id);

  ASSERT_EQ(3u, fake_client_.returned_resources().size());
  ResourceId returned_ids[3];
  for (size_t i = 0; i < 3; ++i) {
    returned_ids[i] = fake_client_.returned_resources()[i].id;
  }
  EXPECT_THAT(returned_ids,
              testing::WhenSorted(testing::ElementsAreArray(ids)));
}

// This test verifies that when a CompositorFrame is submitted to a new surface
// ID, and a new display frame is generated, then the resources of the old
// surface are returned to the appropriate client.
TEST_F(SurfaceAggregatorWithResourcesTest, ReturnResourcesAsSurfacesChange) {
  LocalSurfaceId local_surface_id1(7u, base::UnguessableToken::Create());
  LocalSurfaceId local_surface_id2(8u, base::UnguessableToken::Create());
  SurfaceId surface_id1(root_sink_->frame_sink_id(), local_surface_id1);
  SurfaceId surface_id2(root_sink_->frame_sink_id(), local_surface_id2);

  std::vector<ResourceId> ids = {ResourceId(11), ResourceId(12),
                                 ResourceId(13)};
  SubmitCompositorFrameWithResources(ids, true, SurfaceId(), root_sink_.get(),
                                     surface_id1);

  auto frame = AggregateFrame(surface_id1);

  // Nothing should be available to be returned yet.
  EXPECT_TRUE(fake_client_.returned_resources().empty());

  // Submitting a CompositorFrame to |surface_id2| should cause the surface
  // associated with |surface_id1| to get garbage collected.
  SubmitCompositorFrameWithResources({}, true, SurfaceId(), root_sink_.get(),
                                     surface_id2);
  manager_.surface_manager()->GarbageCollectSurfaces();

  frame = AggregateFrame(surface_id2);

  ASSERT_EQ(3u, fake_client_.returned_resources().size());
  ResourceId returned_ids[3];
  for (size_t i = 0; i < 3; ++i) {
    returned_ids[i] = fake_client_.returned_resources()[i].id;
  }
  EXPECT_THAT(returned_ids,
              testing::WhenSorted(testing::ElementsAreArray(ids)));
}

TEST_F(SurfaceAggregatorWithResourcesTest, TakeInvalidResources) {
  LocalSurfaceId local_surface_id(7u, base::UnguessableToken::Create());
  SurfaceId surface_id(root_sink_->frame_sink_id(), local_surface_id);

  TransferableResource resource;
  resource.id = ResourceId(11);
  // ResourceProvider is software but resource is not, so it should be
  // ignored.
  resource.is_software = false;

  CompositorFrame frame = CompositorFrameBuilder()
                              .AddDefaultRenderPass()
                              .AddTransferableResource(resource)
                              .Build();
  root_sink_->SubmitCompositorFrame(local_surface_id, std::move(frame));

  auto returned_frame = AggregateFrame(surface_id);

  // Nothing should be available to be returned yet.
  EXPECT_TRUE(fake_client_.returned_resources().empty());

  SubmitCompositorFrameWithResources({}, true, SurfaceId(), root_sink_.get(),
                                     surface_id);
  ASSERT_EQ(1u, fake_client_.returned_resources().size());
  EXPECT_EQ(ResourceId(11u), fake_client_.returned_resources()[0].id);
}

TEST_F(SurfaceAggregatorWithResourcesTest, TwoSurfaces) {
  FakeCompositorFrameSinkClient client;
  auto support1 = std::make_unique<CompositorFrameSinkSupport>(
      &client, &manager_, FrameSinkId(3, 1), /*is_root=*/false);
  auto support2 = std::make_unique<CompositorFrameSinkSupport>(
      &client, &manager_, FrameSinkId(4, 2), /*is_root=*/false);
  LocalSurfaceId local_frame1_id(7u, base::UnguessableToken::Create());
  SurfaceId surface1_id(support1->frame_sink_id(), local_frame1_id);

  LocalSurfaceId local_frame2_id(8u, base::UnguessableToken::Create());
  SurfaceId surface2_id(support2->frame_sink_id(), local_frame2_id);

  std::vector<ResourceId> ids = {ResourceId(11), ResourceId(12),
                                 ResourceId(13)};
  SubmitCompositorFrameWithResources(ids, true, SurfaceId(), support1.get(),
                                     surface1_id);
  std::vector<ResourceId> ids2 = {ResourceId(14), ResourceId(15),
                                  ResourceId(16)};
  SubmitCompositorFrameWithResources(ids2, true, SurfaceId(), support2.get(),
                                     surface2_id);

  auto frame = AggregateFrame(surface1_id);

  SubmitCompositorFrameWithResources({}, true, SurfaceId(), support1.get(),
                                     surface1_id);

  // Nothing should be available to be returned yet.
  EXPECT_TRUE(client.returned_resources().empty());

  frame = AggregateFrame(surface2_id);

  // surface1_id wasn't referenced, so its resources should be returned.
  ASSERT_EQ(3u, client.returned_resources().size());
  ResourceId returned_ids[3];
  for (size_t i = 0; i < 3; ++i) {
    returned_ids[i] = client.returned_resources()[i].id;
  }
  EXPECT_THAT(returned_ids,
              testing::WhenSorted(testing::ElementsAreArray(ids)));
  EXPECT_EQ(3u, resource_provider_.num_resources());
}

// Ensure that aggregator completely ignores Surfaces that reference invalid
// resources.
TEST_F(SurfaceAggregatorWithResourcesTest, InvalidChildSurface) {
  auto middle_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryMiddleFrameSinkId, /*is_root=*/false);
  auto child_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryFrameSinkId1, /*is_root=*/false);
  TestSurfaceIdAllocator root_surface_id(root_sink_->frame_sink_id());
  TestSurfaceIdAllocator middle_surface_id(middle_support->frame_sink_id());
  TestSurfaceIdAllocator child_surface_id(child_support->frame_sink_id());

  std::vector<ResourceId> ids = {ResourceId(14), ResourceId(15),
                                 ResourceId(16)};
  SubmitCompositorFrameWithResources(ids, true, SurfaceId(),
                                     child_support.get(), child_surface_id);

  std::vector<ResourceId> ids2 = {ResourceId(17), ResourceId(18),
                                  ResourceId(19)};
  SubmitCompositorFrameWithResources(ids2, false, child_surface_id,
                                     middle_support.get(), middle_surface_id);

  std::vector<ResourceId> ids3 = {ResourceId(20), ResourceId(21),
                                  ResourceId(22)};
  SubmitCompositorFrameWithResources(ids3, true, middle_surface_id,
                                     root_sink_.get(), root_surface_id);

  auto frame = AggregateFrame(root_surface_id);

  auto* pass_list = &frame.render_pass_list;
  ASSERT_EQ(1u, pass_list->size());
  EXPECT_EQ(1u, pass_list->back()->shared_quad_state_list.size());
  EXPECT_EQ(3u, pass_list->back()->quad_list.size());
  SubmitCompositorFrameWithResources(ids2, true, child_surface_id,
                                     middle_support.get(), middle_surface_id);

  frame = AggregateFrame(root_surface_id);

  pass_list = &frame.render_pass_list;
  ASSERT_EQ(1u, pass_list->size());
  EXPECT_EQ(3u, pass_list->back()->shared_quad_state_list.size());
  EXPECT_EQ(9u, pass_list->back()->quad_list.size());
}

TEST_F(SurfaceAggregatorWithResourcesTest, SecureOutputTexture) {
  auto support1 = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, FrameSinkId(3, 1), /*is_root=*/false);
  auto support2 = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, FrameSinkId(4, 2), /*is_root=*/false);
  support2->set_allow_copy_output_requests_for_testing();
  LocalSurfaceId local_frame1_id(7u, base::UnguessableToken::Create());
  SurfaceId surface1_id(support1->frame_sink_id(), local_frame1_id);

  LocalSurfaceId local_frame2_id(8u, base::UnguessableToken::Create());
  SurfaceId surface2_id(support2->frame_sink_id(), local_frame2_id);

  std::vector<ResourceId> ids = {ResourceId(11), ResourceId(12),
                                 ResourceId(13)};
  SubmitCompositorFrameWithResources(ids, true, SurfaceId(), support1.get(),
                                     surface1_id);

  auto frame = AggregateFrame(surface1_id);

  auto* render_pass = frame.render_pass_list.back().get();

  EXPECT_EQ(DrawQuad::Material::kTextureContent,
            render_pass->quad_list.back()->material);

  {
    auto pass = CompositorRenderPass::Create();
    pass->SetNew(CompositorRenderPassId{1}, gfx::Rect(0, 0, 20, 20),
                 gfx::Rect(), gfx::Transform());
    auto* sqs = pass->CreateAndAppendSharedQuadState();
    sqs->opacity = 1.f;
    auto* surface_quad = pass->CreateAndAppendDrawQuad<SurfaceDrawQuad>();

    surface_quad->SetNew(sqs, gfx::Rect(0, 0, 1, 1), gfx::Rect(0, 0, 1, 1),
                         SurfaceRange(std::nullopt, surface1_id),
                         SkColors::kWhite,
                         /*stretch_content_to_fill_bounds=*/false);
    pass->copy_requests.push_back(CopyOutputRequest::CreateStubForTesting());

    CompositorFrame compositor_frame =
        CompositorFrameBuilder().AddRenderPass(std::move(pass)).Build();

    support2->SubmitCompositorFrame(local_frame2_id,
                                    std::move(compositor_frame));
  }

  frame = AggregateFrame(surface2_id);
  EXPECT_EQ(1u, frame.render_pass_list.size());
  render_pass = frame.render_pass_list.front().get();

  // Parent has copy request, so texture should not be drawn.
  EXPECT_EQ(DrawQuad::Material::kSolidColor,
            render_pass->quad_list.back()->material);

  frame = AggregateFrame(surface2_id);
  EXPECT_EQ(1u, frame.render_pass_list.size());
  render_pass = frame.render_pass_list.front().get();

  // Copy request has been executed earlier, so texture should be drawn.
  EXPECT_EQ(DrawQuad::Material::kTextureContent,
            render_pass->quad_list.front()->material);

  aggregator_.set_output_is_secure(false);

  frame = AggregateFrame(surface2_id);
  render_pass = frame.render_pass_list.back().get();

  // Output is insecure, so texture should be drawn.
  EXPECT_EQ(DrawQuad::Material::kSolidColor,
            render_pass->quad_list.back()->material);
}

// Ensure that the render passes have correct color spaces. This test
// simulates the Windows HDR behavior.
TEST_F(SurfaceAggregatorValidSurfaceTest, ColorSpaceTestWin) {
  constexpr float device_scale_factor = 1.0f;
  const gfx::Rect child_pass_damage_rect(10, 20, 30, 40);
  const gfx::Rect full_damage_rect(kSurfaceSize);
  const gfx::Rect partial_damage_rect(45, 45, 10, 10);

  std::vector<Quad> quads[2] = {
      {Quad::SolidColorQuad(SkColors::kWhite, gfx::Rect(5, 5)),
       Quad::SolidColorQuad(SkColors::kLtGray, gfx::Rect(5, 5))},
      {Quad::SolidColorQuad(SkColors::kGray, gfx::Rect(5, 5)),
       Quad::TransparentSolidColorQuad(SkColors::kDkGray, gfx::Rect(5, 5),
                                       0.5)}};

  gfx::DisplayColorSpaces display_color_spaces(gfx::ColorSpace::CreateSRGB());
  display_color_spaces.SetOutputColorSpaceAndBufferFormat(
      gfx::ContentColorUsage::kWideColorGamut, false /* needs_alpha */,
      gfx::ColorSpace(gfx::ColorSpace::PrimaryID::BT2020,
                      gfx::ColorSpace::TransferID::SRGB),
      gfx::BufferFormat::RGBA_8888);
  display_color_spaces.SetOutputColorSpaceAndBufferFormat(
      gfx::ContentColorUsage::kWideColorGamut, true /* needs_alpha */,
      gfx::ColorSpace::CreateSRGBLinear(), gfx::BufferFormat::RGBA_8888);
  display_color_spaces.SetOutputColorSpaceAndBufferFormat(
      gfx::ContentColorUsage::kHDR, false /* needs_alpha */,
      gfx::ColorSpace::CreateHDR10(), gfx::BufferFormat::BGRA_1010102);
  display_color_spaces.SetOutputColorSpaceAndBufferFormat(
      gfx::ContentColorUsage::kHDR, true /* needs_alpha */,
      gfx::ColorSpace::CreateSRGBLinear(), gfx::BufferFormat::RGBA_F16);

  std::vector<Pass> passes = {
      Pass(quads[0], CompositorRenderPassId{2}, kSurfaceSize),
      Pass(quads[1], CompositorRenderPassId{1}, kSurfaceSize)};
  passes[1].has_transparent_background = true;
  passes[1].damage_rect = partial_damage_rect;
  passes[0].damage_rect = child_pass_damage_rect;

  const bool has_color_conversion_pass =
      !base::FeatureList::IsEnabled(features::kColorConversionInRenderer);

  // The root pass of HDR content with a transparent background will get an
  // extra RenderPass converting to SCRGB-linear, if any content drawn to the
  // root pass requires blending.
  aggregator_.SetDisplayColorSpaces(display_color_spaces);
  {
    SubmitCompositorFrame(root_sink_.get(), passes,
                          root_surface_id_.local_surface_id(),
                          device_scale_factor);
    SurfaceId surface_id(root_sink_->frame_sink_id(),
                         root_surface_id_.local_surface_id());

    auto aggregated_frame = AggregateFrame(surface_id);

    EXPECT_EQ(has_color_conversion_pass ? 3u : 2u,
              aggregated_frame.render_pass_list.size());
    EXPECT_EQ(gfx::ContentColorUsage::kHDR,
              aggregated_frame.render_pass_list[0]->content_color_usage);
    EXPECT_EQ(gfx::ContentColorUsage::kHDR,
              aggregated_frame.render_pass_list[1]->content_color_usage);
    if (has_color_conversion_pass) {
      EXPECT_EQ(gfx::ContentColorUsage::kHDR,
                aggregated_frame.render_pass_list[2]->content_color_usage);
    }

    // All passes will have full damage for the first frame.
    if (has_color_conversion_pass) {
      EXPECT_EQ(full_damage_rect,
                aggregated_frame.render_pass_list[2]->damage_rect);
    }
    EXPECT_EQ(full_damage_rect,
              aggregated_frame.render_pass_list[1]->damage_rect);
    EXPECT_EQ(full_damage_rect,
              aggregated_frame.render_pass_list[0]->damage_rect);
  }

  // The root pass of HDR content with a transparent background will get an
  // extra RenderPass converting to HDR10, if any content drawn to the root pass
  // requires blending.
  passes[1].has_transparent_background = false;
  {
    SubmitCompositorFrame(root_sink_.get(), passes,
                          root_surface_id_.local_surface_id(),
                          device_scale_factor);
    SurfaceId surface_id(root_sink_->frame_sink_id(),
                         root_surface_id_.local_surface_id());

    auto aggregated_frame = AggregateFrame(surface_id);

    EXPECT_EQ(has_color_conversion_pass ? 3u : 2u,
              aggregated_frame.render_pass_list.size());
    EXPECT_EQ(gfx::ContentColorUsage::kHDR,
              aggregated_frame.render_pass_list[0]->content_color_usage);
    EXPECT_EQ(gfx::ContentColorUsage::kHDR,
              aggregated_frame.render_pass_list[1]->content_color_usage);
    if (has_color_conversion_pass) {
      EXPECT_EQ(gfx::ContentColorUsage::kHDR,
                aggregated_frame.render_pass_list[2]->content_color_usage);
    }

    if (has_color_conversion_pass) {
      // The root pass (drawn to the backbuffer) and the intermediate pass
      // (drawn to extended-sRGB) will now have partial damage. Note that the
      // root pass will end up getting full damage due to the
      // OutputSurface::Reshape call that will be made by DirectRenderer.
      EXPECT_EQ(partial_damage_rect,
                aggregated_frame.render_pass_list[2]->damage_rect);
    }
    EXPECT_EQ(partial_damage_rect,
              aggregated_frame.render_pass_list[1]->damage_rect);
  }

  // The root pass of HDR content with a transparent background won't get an
  // extra RenderPass, if all content drawn to the root pass doesn't require
  // blending.
  quads[1][1] = Quad::SolidColorQuad(SkColors::kDkGray, gfx::Rect(5, 5));
  passes[1] = Pass(quads[1], CompositorRenderPassId{1}, kSurfaceSize);
  passes[1].has_transparent_background = false;
  passes[1].damage_rect = partial_damage_rect;
  {
    SubmitCompositorFrame(root_sink_.get(), passes,
                          root_surface_id_.local_surface_id(),
                          device_scale_factor);
    SurfaceId surface_id(root_sink_->frame_sink_id(),
                         root_surface_id_.local_surface_id());

    auto aggregated_frame = AggregateFrame(surface_id);

    EXPECT_EQ(2u, aggregated_frame.render_pass_list.size());
    EXPECT_EQ(gfx::ContentColorUsage::kHDR,
              aggregated_frame.render_pass_list[0]->content_color_usage);
    EXPECT_EQ(gfx::ContentColorUsage::kHDR,
              aggregated_frame.render_pass_list[1]->content_color_usage);

    if (has_color_conversion_pass) {
      // The root pass has full damage because the intermediate pass was
      // removed.
      EXPECT_EQ(full_damage_rect,
                aggregated_frame.render_pass_list[1]->damage_rect);
    } else {
      EXPECT_EQ(partial_damage_rect,
                aggregated_frame.render_pass_list[1]->damage_rect);
    }
  }

  // This simulates the situation where we don't have HDR capabilities. Opaque
  // content can be drawn into a BT2020 buffer as 10-10-10-2, but transparent
  // content needs to bump up to 16-bit, and therefore (until we find a way
  // around this) linear color space.
  display_color_spaces.SetOutputColorSpaceAndBufferFormat(
      gfx::ContentColorUsage::kHDR, false /* needs_alpha */,
      gfx::ColorSpace(gfx::ColorSpace::PrimaryID::BT2020,
                      gfx::ColorSpace::TransferID::SRGB),
      gfx::BufferFormat::BGRA_1010102);
  display_color_spaces.SetOutputColorSpaceAndBufferFormat(
      gfx::ContentColorUsage::kHDR, true /* needs_alpha */,
      gfx::ColorSpace::CreateSRGBLinear(), gfx::BufferFormat::RGBA_F16);

  // Opaque content renders to the appropriate space directly.
  passes[1].has_transparent_background = false;
  aggregator_.SetDisplayColorSpaces(display_color_spaces);
  {
    SubmitCompositorFrame(root_sink_.get(), passes,
                          root_surface_id_.local_surface_id(),
                          device_scale_factor);
    SurfaceId surface_id(root_sink_->frame_sink_id(),
                         root_surface_id_.local_surface_id());

    auto aggregated_frame = AggregateFrame(surface_id);

    EXPECT_EQ(2u, aggregated_frame.render_pass_list.size());
    EXPECT_EQ(gfx::ContentColorUsage::kHDR,
              aggregated_frame.render_pass_list[0]->content_color_usage);
    EXPECT_EQ(gfx::ContentColorUsage::kHDR,
              aggregated_frame.render_pass_list[1]->content_color_usage);

    // The root pass has partial damage.
    EXPECT_EQ(partial_damage_rect,
              aggregated_frame.render_pass_list[1]->damage_rect);
  }

  // When the root pass has a transparent background and any content drawn to it
  // requires blending, we'll end up getting a color conversion pass.
  quads[1][1] =
      Quad::TransparentSolidColorQuad(SkColors::kDkGray, gfx::Rect(5, 5), 0.5);
  passes[1] = Pass(quads[1], CompositorRenderPassId{1}, kSurfaceSize);
  passes[1].has_transparent_background = true;
  passes[1].damage_rect = partial_damage_rect;
  {
    SubmitCompositorFrame(root_sink_.get(), passes,
                          root_surface_id_.local_surface_id(),
                          device_scale_factor);
    SurfaceId surface_id(root_sink_->frame_sink_id(),
                         root_surface_id_.local_surface_id());

    auto aggregated_frame = AggregateFrame(surface_id);

    EXPECT_EQ(has_color_conversion_pass ? 3u : 2u,
              aggregated_frame.render_pass_list.size());
    EXPECT_EQ(gfx::ContentColorUsage::kHDR,
              aggregated_frame.render_pass_list[0]->content_color_usage);
    EXPECT_EQ(gfx::ContentColorUsage::kHDR,
              aggregated_frame.render_pass_list[1]->content_color_usage);
    if (has_color_conversion_pass) {
      EXPECT_EQ(gfx::ContentColorUsage::kHDR,
                aggregated_frame.render_pass_list[2]->content_color_usage);
    }

    if (has_color_conversion_pass) {
      // The root (drawn to backbuffer) and intermediate (drawn to
      // extended-sRGB) passes have full damage because they were added this
      // frame.
      EXPECT_EQ(full_damage_rect,
                aggregated_frame.render_pass_list[2]->damage_rect);
      EXPECT_EQ(full_damage_rect,
                aggregated_frame.render_pass_list[1]->damage_rect);
    } else {
      EXPECT_EQ(partial_damage_rect,
                aggregated_frame.render_pass_list[1]->damage_rect);
    }
  }
}

// Ensure that the render passes have correct color spaces.
TEST_F(SurfaceAggregatorValidSurfaceTest, MetadataContentColorUsageTest) {
  auto test_content_color_usage_aggregation =
      [this](gfx::ContentColorUsage content_color_usage) {
        std::vector<Quad> child_quads = {
            Quad::SolidColorQuad(SkColors::kGreen, gfx::Rect(5, 5))};
        std::vector<Pass> child_passes = {
            Pass(child_quads, CompositorRenderPassId{1}, gfx::Size(100, 100))};

        CompositorFrame child_frame = MakeEmptyCompositorFrame();
        // Set the child's color space
        child_frame.metadata.content_color_usage = content_color_usage;
        AddPasses(&child_frame.render_pass_list, child_passes,
                  &child_frame.metadata.referenced_surfaces);

        TestSurfaceIdAllocator child_surface_id(child_sink_->frame_sink_id());
        child_sink_->SubmitCompositorFrame(child_surface_id.local_surface_id(),
                                           std::move(child_frame));

        std::vector<Quad> root_quads = {Quad::SurfaceQuad(
            SurfaceRange(std::nullopt, child_surface_id), SkColors::kWhite,
            gfx::Rect(5, 5), /*stretch_content_to_fill_bounds=*/false)};
        std::vector<Pass> root_passes = {Pass(root_quads, kSurfaceSize)};

        CompositorFrame root_frame = MakeEmptyCompositorFrame();
        root_frame.metadata.content_color_usage = content_color_usage;
        AddPasses(&root_frame.render_pass_list, root_passes,
                  &root_frame.metadata.referenced_surfaces);

        root_sink_->SubmitCompositorFrame(root_surface_id_.local_surface_id(),
                                          std::move(root_frame));

        auto aggregated_frame = AggregateFrame(root_surface_id_);
        const auto& aggregated_pass_list = aggregated_frame.render_pass_list;

        // Make sure the root render pass has a color space that matches
        // expected generalization.
        ASSERT_EQ(aggregated_frame.content_color_usage, content_color_usage);
        ASSERT_EQ(aggregated_pass_list[0]->content_color_usage,
                  content_color_usage);
      };

  test_content_color_usage_aggregation(gfx::ContentColorUsage::kSRGB);
  test_content_color_usage_aggregation(gfx::ContentColorUsage::kWideColorGamut);
  test_content_color_usage_aggregation(gfx::ContentColorUsage::kHDR);
}

// Tests that has_damage_from_contributing_content is aggregated correctly from
// child surface quads.
TEST_F(SurfaceAggregatorValidSurfaceTest, HasDamageByChangingChildSurface) {
  std::vector<Quad> child_surface_quads = {
      Quad::SolidColorQuad(SkColors::kGreen, gfx::Rect(5, 5))};
  std::vector<Pass> child_surface_passes = {
      Pass(child_surface_quads, CompositorRenderPassId{1}, kSurfaceSize)};

  TestSurfaceIdAllocator child_surface_id(child_sink_->frame_sink_id());
  {
    CompositorFrame child_surface_frame = MakeEmptyCompositorFrame();
    AddPasses(&child_surface_frame.render_pass_list, child_surface_passes,
              &child_surface_frame.metadata.referenced_surfaces);

    child_sink_->SubmitCompositorFrame(child_surface_id.local_surface_id(),
                                       std::move(child_surface_frame));
  }

  std::vector<Quad> root_surface_quads = {Quad::SurfaceQuad(
      SurfaceRange(std::nullopt, child_surface_id), SkColors::kWhite,
      gfx::Rect(5, 5), /*stretch_content_to_fill_bounds=*/false)};
  std::vector<Pass> root_passes = {
      Pass(root_surface_quads, CompositorRenderPassId{1}, kSurfaceSize)};

  CompositorFrame root_frame = MakeEmptyCompositorFrame();
  AddPasses(&root_frame.render_pass_list, root_passes,
            &root_frame.metadata.referenced_surfaces);

  root_sink_->SubmitCompositorFrame(root_surface_id_.local_surface_id(),
                                    std::move(root_frame));

  // On first frame there is no existing cache texture to worry about re-using,
  // so we don't worry what this bool is set to.
  { auto aggregated_frame = AggregateFrame(root_surface_id_); }

  // No Surface changed, so no damage should be given.
  {
    auto aggregated_frame = AggregateFrame(root_surface_id_);
    EXPECT_FALSE(aggregated_frame.render_pass_list[0]
                     ->has_damage_from_contributing_content);
  }

  // Change child_frame with damage should set the flag.
  {
    CompositorFrame child_surface_frame = MakeEmptyCompositorFrame();
    AddPasses(&child_surface_frame.render_pass_list, child_surface_passes,
              &child_surface_frame.metadata.referenced_surfaces);
    child_sink_->SubmitCompositorFrame(child_surface_id.local_surface_id(),
                                       std::move(child_surface_frame));

    auto aggregated_frame = AggregateFrame(root_surface_id_);
    // True for new child_frame with damage.
    EXPECT_TRUE(aggregated_frame.render_pass_list[0]
                    ->has_damage_from_contributing_content);
  }

  // Change child_frame without damage should not set the flag.
  {
    CompositorFrame child_surface_frame = MakeEmptyCompositorFrame();
    AddPasses(&child_surface_frame.render_pass_list, child_surface_passes,
              &child_surface_frame.metadata.referenced_surfaces);
    child_surface_frame.render_pass_list[0]->damage_rect = gfx::Rect();
    child_sink_->SubmitCompositorFrame(child_surface_id.local_surface_id(),
                                       std::move(child_surface_frame));

    auto aggregated_frame = AggregateFrame(root_surface_id_);
    // False for new child_frame without damage.
    EXPECT_FALSE(aggregated_frame.render_pass_list[0]
                     ->has_damage_from_contributing_content);
  }
}

// Tests that has_damage_from_contributing_content is aggregated correctly from
// grand child surface quads.
TEST_F(SurfaceAggregatorValidSurfaceTest,
       HasDamageByChangingGrandChildSurface) {
  auto grand_child_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryMiddleFrameSinkId, /*is_root=*/false);

  std::vector<Quad> child_surface_quads = {
      Quad::SolidColorQuad(SkColors::kGreen, gfx::Rect(5, 5))};
  std::vector<Pass> child_surface_passes = {
      Pass(child_surface_quads, CompositorRenderPassId{1}, kSurfaceSize)};

  CompositorFrame child_surface_frame = MakeEmptyCompositorFrame();
  AddPasses(&child_surface_frame.render_pass_list, child_surface_passes,
            &child_surface_frame.metadata.referenced_surfaces);

  TestSurfaceIdAllocator child_surface_id(child_sink_->frame_sink_id());
  child_sink_->SubmitCompositorFrame(child_surface_id.local_surface_id(),
                                     std::move(child_surface_frame));

  std::vector<Quad> root_surface_quads = {Quad::SurfaceQuad(
      SurfaceRange(std::nullopt, child_surface_id), SkColors::kWhite,
      gfx::Rect(5, 5), /*stretch_content_to_fill_bounds=*/false)};
  std::vector<Pass> root_passes = {
      Pass(root_surface_quads, CompositorRenderPassId{1}, kSurfaceSize)};

  CompositorFrame root_frame = MakeEmptyCompositorFrame();
  AddPasses(&root_frame.render_pass_list, root_passes,
            &root_frame.metadata.referenced_surfaces);

  root_sink_->SubmitCompositorFrame(root_surface_id_.local_surface_id(),
                                    std::move(root_frame));

  // On first frame there is no existing cache texture to worry about re-using,
  // so we don't worry what this bool is set to.
  { auto aggregated_frame = AggregateFrame(root_surface_id_); }

  // No Surface changed, so no damage should be given.
  {
    auto aggregated_frame = AggregateFrame(root_surface_id_);
    EXPECT_FALSE(aggregated_frame.render_pass_list[0]
                     ->has_damage_from_contributing_content);
  }

  // Add a grand_child_frame should cause damage.
  std::vector<Quad> grand_child_quads = {
      Quad::SolidColorQuad(SkColors::kGreen, gfx::Rect(5, 5))};
  std::vector<Pass> grand_child_passes = {
      Pass(grand_child_quads, CompositorRenderPassId{1}, kSurfaceSize)};
  TestSurfaceIdAllocator grand_child_surface_id(
      grand_child_support->frame_sink_id());
  {
    CompositorFrame grand_child_frame = MakeEmptyCompositorFrame();
    AddPasses(&grand_child_frame.render_pass_list, grand_child_passes,
              &grand_child_frame.metadata.referenced_surfaces);

    grand_child_support->SubmitCompositorFrame(
        grand_child_surface_id.local_surface_id(),
        std::move(grand_child_frame));

    std::vector<Quad> new_child_surface_quads = {
        child_surface_quads[0],
        Quad::SurfaceQuad(SurfaceRange(std::nullopt, grand_child_surface_id),
                          SkColors::kWhite, gfx::Rect(5, 5),
                          /*stretch_content_to_fill_bounds=*/false)};
    std::vector<Pass> new_child_surface_passes = {
        Pass(new_child_surface_quads, CompositorRenderPassId{1}, kSurfaceSize)};
    child_surface_frame = MakeEmptyCompositorFrame();
    AddPasses(&child_surface_frame.render_pass_list, new_child_surface_passes,
              &child_surface_frame.metadata.referenced_surfaces);
    child_sink_->SubmitCompositorFrame(child_surface_id.local_surface_id(),
                                       std::move(child_surface_frame));

    auto aggregated_frame = AggregateFrame(root_surface_id_);
    // True for new grand_child_frame.
    EXPECT_TRUE(aggregated_frame.render_pass_list[0]
                    ->has_damage_from_contributing_content);
  }

  // No Surface changed, so no damage should be given.
  {
    auto aggregated_frame = AggregateFrame(root_surface_id_);
    EXPECT_FALSE(aggregated_frame.render_pass_list[0]
                     ->has_damage_from_contributing_content);
  }

  // Change grand_child_frame with damage should set the flag.
  {
    CompositorFrame grand_child_frame = MakeEmptyCompositorFrame();
    AddPasses(&grand_child_frame.render_pass_list, grand_child_passes,
              &grand_child_frame.metadata.referenced_surfaces);
    grand_child_support->SubmitCompositorFrame(
        grand_child_surface_id.local_surface_id(),
        std::move(grand_child_frame));

    auto aggregated_frame = AggregateFrame(root_surface_id_);
    // True for new grand_child_frame with damage.
    EXPECT_TRUE(aggregated_frame.render_pass_list[0]
                    ->has_damage_from_contributing_content);
  }

  // Change grand_child_frame without damage should not set the flag.
  {
    CompositorFrame grand_child_frame = MakeEmptyCompositorFrame();
    AddPasses(&grand_child_frame.render_pass_list, grand_child_passes,
              &grand_child_frame.metadata.referenced_surfaces);
    grand_child_frame.render_pass_list[0]->damage_rect = gfx::Rect();
    grand_child_support->SubmitCompositorFrame(
        grand_child_surface_id.local_surface_id(),
        std::move(grand_child_frame));

    auto aggregated_frame = AggregateFrame(root_surface_id_);
    // False for new grand_child_frame without damage.
    EXPECT_FALSE(aggregated_frame.render_pass_list[0]
                     ->has_damage_from_contributing_content);
  }
}

// Tests that has_damage_from_contributing_content is aggregated correctly from
// grand child surface quads when render passes can't be merged.
TEST_F(SurfaceAggregatorValidSurfaceTest,
       HasDamageByChangingGrandChildSurfaceNoMerge) {
  auto grand_child_sink = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryMiddleFrameSinkId, /*is_root=*/false);
  TestSurfaceIdAllocator grand_child_surface_id(
      grand_child_sink->frame_sink_id());
  TestSurfaceIdAllocator child_surface_id(child_sink_->frame_sink_id());

  {
    CompositorFrame grandchild_frame =
        CompositorFrameBuilder()
            .AddRenderPass(
                RenderPassBuilder(kSurfaceSize)
                    .AddSolidColorQuad(gfx::Rect(5, 5), SkColors::kGreen))
            .Build();
    grand_child_sink->SubmitCompositorFrame(
        grand_child_surface_id.local_surface_id(), std::move(grandchild_frame));

    CompositorFrame child_frame =
        CompositorFrameBuilder()
            .AddRenderPass(
                RenderPassBuilder(CompositorRenderPassId{1}, kSurfaceSize)
                    .AddSurfaceQuad(gfx::Rect(5, 5),
                                    SurfaceRange(grand_child_surface_id),
                                    {.allow_merge = false}))
            .AddRenderPass(
                RenderPassBuilder(CompositorRenderPassId{2}, kSurfaceSize)
                    .AddRenderPassQuad(gfx::Rect(kSurfaceSize),
                                       CompositorRenderPassId{1}))
            .Build();
    child_sink_->SubmitCompositorFrame(child_surface_id.local_surface_id(),
                                       std::move(child_frame));

    CompositorFrame root_frame =
        CompositorFrameBuilder()
            .AddRenderPass(RenderPassBuilder(kSurfaceSize)
                               .AddSurfaceQuad(gfx::Rect(5, 5),
                                               SurfaceRange(child_surface_id),
                                               {.allow_merge = false}))
            .Build();
    root_sink_->SubmitCompositorFrame(root_surface_id_.local_surface_id(),
                                      std::move(root_frame));

    // On first frame there is no existing cache texture to worry about
    // re-using, so we don't worry what this bool is set to.
    auto aggregated_frame = AggregateFrame(root_surface_id_);
  }

  // No Surface changed, so no damage should be given.
  {
    auto aggregated_frame = AggregateFrame(root_surface_id_);

    ASSERT_EQ(4u, aggregated_frame.render_pass_list.size());
    EXPECT_FALSE(aggregated_frame.render_pass_list[3]
                     ->has_damage_from_contributing_content);
  }

  // A new grandchild frame should damage the root render pass.
  {
    CompositorFrame grandchild_frame =
        CompositorFrameBuilder()
            .AddRenderPass(
                RenderPassBuilder(kSurfaceSize)
                    .AddSolidColorQuad(gfx::Rect(5, 5), SkColors::kGreen))
            .Build();
    grand_child_sink->SubmitCompositorFrame(
        grand_child_surface_id.local_surface_id(), std::move(grandchild_frame));

    auto aggregated_frame = AggregateFrame(root_surface_id_);

    ASSERT_EQ(4u, aggregated_frame.render_pass_list.size());
    EXPECT_TRUE(aggregated_frame.render_pass_list[3]
                    ->has_damage_from_contributing_content);
  }
}

// Tests that has_damage_from_contributing_content is aggregated correctly when
// non-root pass has damage but root pass has no damage due to non-root damage
// being outside the root passes output_rect.
TEST_F(SurfaceAggregatorValidSurfaceTest, RootPassNoDamage) {
  constexpr gfx::Rect render_pass_rect(50, 0, 100, 100);
  {
    CompositorFrame root_frame =
        CompositorFrameBuilder()
            .AddRenderPass(
                RenderPassBuilder(CompositorRenderPassId{1}, kSurfaceSize)
                    .AddSolidColorQuad(gfx::Rect(kSurfaceSize),
                                       SkColors::kBlue))
            .AddRenderPass(
                RenderPassBuilder(CompositorRenderPassId{2}, kSurfaceSize)
                    .AddSolidColorQuad(gfx::Rect(kSurfaceSize),
                                       SkColors::kGreen)
                    .AddRenderPassQuad(render_pass_rect,
                                       CompositorRenderPassId{1}))
            .Build();
    root_sink_->SubmitCompositorFrame(root_surface_id_.local_surface_id(),
                                      std::move(root_frame));

    // On first frame there is full damage so just verify the render passes have
    // the expected quads.
    auto aggregated_frame = AggregateFrame(root_surface_id_);
    ASSERT_EQ(2u, aggregated_frame.render_pass_list.size());

    auto& pass_list = aggregated_frame.render_pass_list;
    ASSERT_THAT(pass_list[0]->quad_list,
                ElementsAre(IsSolidColorQuad(SkColors::kBlue)));
    ASSERT_THAT(pass_list[1]->quad_list,
                ElementsAre(IsSolidColorQuad(SkColors::kGreen),
                            IsAggregatedRenderPassQuad()));
  }

  {
    // Submit a new CompositorFrame where the non-root render pass has a new
    // quad and damage from it. This new quad is not going to end up in the
    // root render pass because of CompositorRenderPassDrawQuad having an
    // offset. The viz client sets `has_damage_from_contributing_content` false
    // on the root render pass as a result.
    gfx::Rect new_quad_rect(60, 60, 20, 20);
    CompositorFrame root_frame =
        CompositorFrameBuilder()
            .AddRenderPass(
                RenderPassBuilder(CompositorRenderPassId{1}, kSurfaceSize)
                    .AddSolidColorQuad(gfx::Rect(kSurfaceSize), SkColors::kBlue)
                    .AddSolidColorQuad(new_quad_rect, SkColors::kRed)
                    .SetHasDamageFromContributingContent(true)
                    .SetDamageRect(new_quad_rect))
            .AddRenderPass(
                RenderPassBuilder(CompositorRenderPassId{2}, kSurfaceSize)
                    .AddSolidColorQuad(gfx::Rect(kSurfaceSize),
                                       SkColors::kGreen)
                    .AddRenderPassQuad(render_pass_rect,
                                       CompositorRenderPassId{1})
                    .SetHasDamageFromContributingContent(false)
                    .SetDamageRect(gfx::Rect()))
            .Build();
    root_sink_->SubmitCompositorFrame(root_surface_id_.local_surface_id(),
                                      std::move(root_frame));

    auto aggregated_frame = AggregateFrame(root_surface_id_);
    ASSERT_EQ(2u, aggregated_frame.render_pass_list.size());

    // The non-root render pass has an extra quad now and it has damage.
    auto& pass_list = aggregated_frame.render_pass_list;
    ASSERT_THAT(pass_list[0]->quad_list,
                ElementsAre(IsSolidColorQuad(SkColors::kBlue),
                            IsSolidColorQuad(SkColors::kRed)));
    EXPECT_TRUE(pass_list[0]->has_damage_from_contributing_content);

    // Verify that the aggregated root render pass is marked as not having
    // any damage still.
    EXPECT_FALSE(pass_list[1]->has_damage_from_contributing_content);
  }
}

// Tests that has_damage_from_contributing_content is aggregated correctly from
// render pass quads.
TEST_F(SurfaceAggregatorValidSurfaceTest, HasDamageFromRenderPassQuads) {
  std::vector<Quad> child_quads = {
      Quad::SolidColorQuad(SkColors::kGreen, gfx::Rect(5, 5))};
  std::vector<Pass> child_passes = {
      Pass(child_quads, CompositorRenderPassId{1}, kSurfaceSize)};

  TestSurfaceIdAllocator child_surface_id(child_sink_->frame_sink_id());
  {
    CompositorFrame child_frame = MakeEmptyCompositorFrame();
    AddPasses(&child_frame.render_pass_list, child_passes,
              &child_frame.metadata.referenced_surfaces);

    child_sink_->SubmitCompositorFrame(child_surface_id.local_surface_id(),
                                       std::move(child_frame));
  }

  std::vector<Quad> root_surface_quads = {Quad::SurfaceQuad(
      SurfaceRange(std::nullopt, child_surface_id), SkColors::kWhite,
      gfx::Rect(5, 5), /*stretch_content_to_fill_bounds=*/false)};
  std::vector<Quad> root_render_pass_quads = {
      Quad::RenderPassQuad(CompositorRenderPassId{1}, gfx::Transform(), true)};

  std::vector<Pass> root_passes = {
      Pass(root_surface_quads, CompositorRenderPassId{1}, kSurfaceSize),
      Pass(root_render_pass_quads, CompositorRenderPassId{2}, kSurfaceSize)};

  CompositorFrame root_frame = MakeEmptyCompositorFrame();
  AddPasses(&root_frame.render_pass_list, root_passes,
            &root_frame.metadata.referenced_surfaces);

  root_sink_->SubmitCompositorFrame(root_surface_id_.local_surface_id(),
                                    std::move(root_frame));

  // Both CompositorRenderPass are built with
  // has_damage_from_contributing_content set to false.
  {
    auto aggregated_frame = AggregateFrame(root_surface_id_);

    // On first frame there is no existing cache texture to worry about
    // re-using, so we don't worry what this bool is set to.
    const auto& aggregated_pass_list = aggregated_frame.render_pass_list;

    ASSERT_EQ(2u, aggregated_pass_list.size());
  }

  // No Surface changed, so no damage should be given.
  {
    auto aggregated_frame = AggregateFrame(root_surface_id_);
    EXPECT_FALSE(aggregated_frame.render_pass_list[0]
                     ->has_damage_from_contributing_content);
    EXPECT_FALSE(aggregated_frame.render_pass_list[1]
                     ->has_damage_from_contributing_content);
  }

  // Changing child_frame should damage both render_pass.
  {
    CompositorFrame child_frame = MakeEmptyCompositorFrame();
    AddPasses(&child_frame.render_pass_list, child_passes,
              &child_frame.metadata.referenced_surfaces);
    child_sink_->SubmitCompositorFrame(child_surface_id.local_surface_id(),
                                       std::move(child_frame));

    auto aggregated_frame = AggregateFrame(root_surface_id_);

    // True for new child_frame.
    EXPECT_TRUE(aggregated_frame.render_pass_list[0]
                    ->has_damage_from_contributing_content);
    // The damage from the child frame will propagate to the root surface.
    EXPECT_TRUE(aggregated_frame.render_pass_list[1]
                    ->has_damage_from_contributing_content);
  }

  // Both CompositorRenderPass are built with
  // has_damage_from_contributing_content set to true.
  {
    CompositorFrame root_frame_2 = MakeEmptyCompositorFrame();
    root_passes[0].has_damage_from_contributing_content = true;
    AddPasses(&root_frame_2.render_pass_list, root_passes,
              &root_frame_2.metadata.referenced_surfaces);

    root_sink_->SubmitCompositorFrame(root_surface_id_.local_surface_id(),
                                      std::move(root_frame_2));

    CompositorFrame child_frame = MakeEmptyCompositorFrame();
    child_passes[0].has_damage_from_contributing_content = true;
    AddPasses(&child_frame.render_pass_list, child_passes,
              &child_frame.metadata.referenced_surfaces);
    child_sink_->SubmitCompositorFrame(child_surface_id.local_surface_id(),
                                       std::move(child_frame));

    auto aggregated_frame = AggregateFrame(root_surface_id_);

    EXPECT_TRUE(aggregated_frame.render_pass_list[0]
                    ->has_damage_from_contributing_content);
    EXPECT_TRUE(aggregated_frame.render_pass_list[1]
                    ->has_damage_from_contributing_content);
  }
  // No Surface changed, so no damage should be given even if
  // has_damage_from_contributing_content is true from CompositorRenderPass.
  {
    auto aggregated_frame = AggregateFrame(root_surface_id_);
    EXPECT_FALSE(aggregated_frame.render_pass_list[0]
                     ->has_damage_from_contributing_content);
    EXPECT_FALSE(aggregated_frame.render_pass_list[1]
                     ->has_damage_from_contributing_content);
  }
}

// Tests that the first frame damage_rect of a cached render pass should be
// fully damaged.
TEST_F(SurfaceAggregatorValidSurfaceTest, DamageRectOfCachedRenderPass) {
  CompositorRenderPassId pass_id[] = {CompositorRenderPassId{1},
                                      CompositorRenderPassId{2}};
  std::vector<Quad> root_quads[2] = {
      {Quad::SolidColorQuad(SkColors::kGreen, gfx::Rect(5, 5))},
      {Quad::RenderPassQuad(pass_id[0], gfx::Transform(), true)},
  };
  std::vector<Pass> root_passes = {
      Pass(root_quads[0], pass_id[0], kSurfaceSize),
      Pass(root_quads[1], pass_id[1], kSurfaceSize)};

  {
    CompositorFrame root_frame = MakeEmptyCompositorFrame();
    AddPasses(&root_frame.render_pass_list, root_passes,
              &root_frame.metadata.referenced_surfaces);

    root_sink_->SubmitCompositorFrame(root_surface_id_.local_surface_id(),
                                      std::move(root_frame));
  }

  {
    auto aggregated_frame = AggregateFrame(root_surface_id_);

    const auto& aggregated_pass_list = aggregated_frame.render_pass_list;

    ASSERT_EQ(2u, aggregated_pass_list.size());

    // The root surface was enqueued without being aggregated once, so it should
    // be treated as completely damaged.
    EXPECT_TRUE(
        aggregated_pass_list[0]->damage_rect.Contains(gfx::Rect(kSurfaceSize)));
    EXPECT_TRUE(
        aggregated_pass_list[1]->damage_rect.Contains(gfx::Rect(kSurfaceSize)));
  }

  // For offscreen render pass, only the visible area is damaged.
  {
    CompositorFrame root_frame = MakeEmptyCompositorFrame();
    AddPasses(&root_frame.render_pass_list, root_passes,
              &root_frame.metadata.referenced_surfaces);

    auto* nonroot_pass = root_frame.render_pass_list[0].get();
    nonroot_pass->transform_to_root_target.Translate(8, 0);

    gfx::Rect root_pass_damage = gfx::Rect(0, 0, 10, 10);
    auto* root_pass = root_frame.render_pass_list[1].get();
    root_pass->damage_rect = root_pass_damage;
    auto* root_pass_sqs = root_pass->shared_quad_state_list.front();
    root_pass_sqs->quad_to_target_transform.Translate(8, 0);

    root_sink_->SubmitCompositorFrame(root_surface_id_.local_surface_id(),
                                      std::move(root_frame));

    auto aggregated_frame = AggregateFrame(root_surface_id_);
    const auto& aggregated_pass_list = aggregated_frame.render_pass_list;

    // Only the visible area is damaged.
    EXPECT_EQ(gfx::Rect(0, 0, 2, 10), aggregated_pass_list[0]->damage_rect);
    EXPECT_EQ(root_pass_damage, aggregated_pass_list[1]->damage_rect);
  }

  // For offscreen cached render pass, should have full damage.
  {
    CompositorFrame root_frame = MakeEmptyCompositorFrame();
    AddPasses(&root_frame.render_pass_list, root_passes,
              &root_frame.metadata.referenced_surfaces);

    auto* nonroot_pass = root_frame.render_pass_list[0].get();
    nonroot_pass->transform_to_root_target.Translate(8, 0);
    nonroot_pass->cache_render_pass = true;

    gfx::Rect root_pass_damage = gfx::Rect(0, 0, 10, 10);
    auto* root_pass = root_frame.render_pass_list[1].get();
    root_pass->damage_rect = root_pass_damage;
    auto* root_pass_sqs = root_pass->shared_quad_state_list.front();
    root_pass_sqs->quad_to_target_transform.Translate(8, 0);

    root_sink_->SubmitCompositorFrame(root_surface_id_.local_surface_id(),
                                      std::move(root_frame));

    auto aggregated_frame = AggregateFrame(root_surface_id_);
    const auto& aggregated_pass_list = aggregated_frame.render_pass_list;

    // Should have full damage.
    EXPECT_EQ(gfx::Rect(kSurfaceSize), aggregated_pass_list[0]->damage_rect);
    EXPECT_EQ(root_pass_damage, aggregated_pass_list[1]->damage_rect);
  }
}

// Tests that the first frame damage_rect of cached render pass of a child
// surface should be fully damaged.
TEST_F(SurfaceAggregatorValidSurfaceTest,
       DamageRectOfCachedRenderPassInChildSurface) {
  CompositorRenderPassId pass_id[] = {CompositorRenderPassId{1},
                                      CompositorRenderPassId{2}};
  std::vector<Quad> child_quads[2] = {
      {Quad::SolidColorQuad(SkColors::kGreen, gfx::Rect(kSurfaceSize))},
      {Quad::RenderPassQuad(pass_id[0], gfx::Transform(), true)},
  };
  std::vector<Pass> child_passes = {
      Pass(child_quads[0], pass_id[0], kSurfaceSize),
      Pass(child_quads[1], pass_id[1], kSurfaceSize)};

  TestSurfaceIdAllocator child_surface_id(child_sink_->frame_sink_id());
  {
    CompositorFrame child_frame = MakeEmptyCompositorFrame();
    AddPasses(&child_frame.render_pass_list, child_passes,
              &child_frame.metadata.referenced_surfaces);

    child_sink_->SubmitCompositorFrame(child_surface_id.local_surface_id(),
                                       std::move(child_frame));
  }

  std::vector<Quad> root_surface_quads = {Quad::SurfaceQuad(
      SurfaceRange(std::nullopt, child_surface_id), SkColors::kWhite,
      gfx::Rect(kSurfaceSize), /*stretch_content_to_fill_bounds=*/false)};

  std::vector<Pass> root_passes = {
      Pass(root_surface_quads, CompositorRenderPassId{1}, kSurfaceSize)};

  CompositorFrame root_frame = MakeEmptyCompositorFrame();
  AddPasses(&root_frame.render_pass_list, root_passes,
            &root_frame.metadata.referenced_surfaces);

  root_sink_->SubmitCompositorFrame(root_surface_id_.local_surface_id(),
                                    std::move(root_frame));

  {
    auto aggregated_frame = AggregateFrame(root_surface_id_);

    const auto& aggregated_pass_list = aggregated_frame.render_pass_list;

    ASSERT_EQ(2u, aggregated_pass_list.size());

    // The root surface was enqueued without being aggregated once, so it should
    // be treated as completely damaged.
    EXPECT_TRUE(
        aggregated_pass_list[0]->damage_rect.Contains(gfx::Rect(kSurfaceSize)));
    EXPECT_TRUE(
        aggregated_pass_list[1]->damage_rect.Contains(gfx::Rect(kSurfaceSize)));
  }

  // For offscreen render pass, only the visible area is damaged.
  {
    CompositorFrame child_frame = MakeEmptyCompositorFrame();
    AddPasses(&child_frame.render_pass_list, child_passes,
              &child_frame.metadata.referenced_surfaces);

    auto* child_nonroot_pass = child_frame.render_pass_list[0].get();
    child_nonroot_pass->transform_to_root_target.Translate(8, 0);

    gfx::Rect child_root_pass_damage = gfx::Rect(0, 0, 10, 10);
    auto* child_root_pass = child_frame.render_pass_list[1].get();
    child_root_pass->damage_rect = child_root_pass_damage;
    auto* child_root_pass_sqs = child_root_pass->shared_quad_state_list.front();
    child_root_pass_sqs->quad_to_target_transform.Translate(8, 0);

    child_sink_->SubmitCompositorFrame(child_surface_id.local_surface_id(),
                                       std::move(child_frame));

    auto aggregated_frame = AggregateFrame(root_surface_id_);
    const auto& aggregated_pass_list = aggregated_frame.render_pass_list;

    // Only the visible area is damaged.
    EXPECT_EQ(gfx::Rect(0, 0, 2, 10), aggregated_pass_list[0]->damage_rect);
    EXPECT_EQ(child_root_pass_damage, aggregated_pass_list[1]->damage_rect);
  }

  // For offscreen cached render pass, should have full damage.
  {
    CompositorFrame child_frame = MakeEmptyCompositorFrame();
    AddPasses(&child_frame.render_pass_list, child_passes,
              &child_frame.metadata.referenced_surfaces);

    auto* child_nonroot_pass = child_frame.render_pass_list[0].get();
    child_nonroot_pass->transform_to_root_target.Translate(8, 0);
    child_nonroot_pass->cache_render_pass = true;

    gfx::Rect child_root_pass_damage = gfx::Rect(0, 0, 10, 10);
    auto* child_root_pass = child_frame.render_pass_list[1].get();
    child_root_pass->damage_rect = child_root_pass_damage;
    auto* child_root_pass_sqs = child_root_pass->shared_quad_state_list.front();
    child_root_pass_sqs->quad_to_target_transform.Translate(8, 0);

    child_sink_->SubmitCompositorFrame(child_surface_id.local_surface_id(),
                                       std::move(child_frame));

    auto aggregated_frame = AggregateFrame(root_surface_id_);
    const auto& aggregated_pass_list = aggregated_frame.render_pass_list;

    // Should have full damage.
    EXPECT_EQ(gfx::Rect(kSurfaceSize), aggregated_pass_list[0]->damage_rect);
    EXPECT_EQ(child_root_pass_damage, aggregated_pass_list[1]->damage_rect);
  }
}

// Tests that the damage rect from a child surface is clipped before
// aggregated with the parent damage rect when clipping is on
TEST_F(SurfaceAggregatorValidSurfaceTest, DamageRectWithClippedChildSurface) {
  std::vector<Quad> child_surface_quads = {
      Quad::SolidColorQuad(SkColors::kRed, gfx::Rect(kSurfaceSize))};
  std::vector<Pass> child_surface_passes = {
      Pass(child_surface_quads, CompositorRenderPassId{1}, kSurfaceSize)};

  TestSurfaceIdAllocator child_surface_id(child_sink_->frame_sink_id());
  {
    CompositorFrame child_surface_frame = MakeEmptyCompositorFrame();
    AddPasses(&child_surface_frame.render_pass_list, child_surface_passes,
              &child_surface_frame.metadata.referenced_surfaces);

    child_sink_->SubmitCompositorFrame(child_surface_id.local_surface_id(),
                                       std::move(child_surface_frame));
  }

  // root surface quads
  std::vector<Quad> root_surface_quads = {Quad::SurfaceQuad(
      SurfaceRange(std::nullopt, child_surface_id), SkColors::kWhite,
      gfx::Rect(kSurfaceSize), /*stretch_content_to_fill_bounds=*/false)};
  std::vector<Pass> root_passes = {
      Pass(root_surface_quads, CompositorRenderPassId{1}, kSurfaceSize)};

  {
    CompositorFrame root_frame = MakeEmptyCompositorFrame();
    AddPasses(&root_frame.render_pass_list, root_passes,
              &root_frame.metadata.referenced_surfaces);

    root_sink_->SubmitCompositorFrame(root_surface_id_.local_surface_id(),
                                      std::move(root_frame));
  }

  // The damage rect of the very first frame is always the full rect
  { auto aggregated_frame = AggregateFrame(root_surface_id_); }

  // Parameters used for damage rect testing
  auto transform = gfx::Transform::MakeTranslation(20, 0) *
                   gfx::Transform::MakeScale(0.5, 0.5);
  gfx::Rect clip_rect = gfx::Rect(30, 30, 40, 40);

  // Clipping is off
  {
    CompositorFrame child_surface_frame = MakeEmptyCompositorFrame();
    AddPasses(&child_surface_frame.render_pass_list, child_surface_passes,
              &child_surface_frame.metadata.referenced_surfaces);
    child_sink_->SubmitCompositorFrame(child_surface_id.local_surface_id(),
                                       std::move(child_surface_frame));

    CompositorFrame root_frame = MakeEmptyCompositorFrame();
    AddPasses(&root_frame.render_pass_list, root_passes,
              &root_frame.metadata.referenced_surfaces);
    auto* root_render_pass = root_frame.render_pass_list[0].get();
    auto* surface_quad_sqs = root_render_pass->shared_quad_state_list.front();
    surface_quad_sqs->quad_to_target_transform = transform;
    surface_quad_sqs->clip_rect.reset();
    // Set the root damage rect to empty. Only the child surface will be tested.
    root_render_pass->damage_rect = gfx::Rect();
    root_sink_->SubmitCompositorFrame(root_surface_id_.local_surface_id(),
                                      std::move(root_frame));

    auto aggregated_frame = AggregateFrame(root_surface_id_);

    // The root damage rect should be the size of the child surface damage rect
    gfx::Rect expected_damage_rect(20, 0, 50, 50);
    EXPECT_EQ(aggregated_frame.render_pass_list[0]->damage_rect,
              expected_damage_rect);
  }
  // Clipping is on
  {
    CompositorFrame child_surface_frame = MakeEmptyCompositorFrame();
    AddPasses(&child_surface_frame.render_pass_list, child_surface_passes,
              &child_surface_frame.metadata.referenced_surfaces);
    child_sink_->SubmitCompositorFrame(child_surface_id.local_surface_id(),
                                       std::move(child_surface_frame));

    CompositorFrame root_frame = MakeEmptyCompositorFrame();
    AddPasses(&root_frame.render_pass_list, root_passes,
              &root_frame.metadata.referenced_surfaces);
    auto* root_render_pass = root_frame.render_pass_list[0].get();
    auto* surface_quad_sqs = root_render_pass->shared_quad_state_list.front();
    surface_quad_sqs->quad_to_target_transform = transform;
    surface_quad_sqs->clip_rect = clip_rect;
    root_render_pass->damage_rect = gfx::Rect();
    root_sink_->SubmitCompositorFrame(root_surface_id_.local_surface_id(),
                                      std::move(root_frame));

    auto aggregated_frame = AggregateFrame(root_surface_id_);

    // The root damage rect should be the size of the clipped child surface
    // damage rect
    gfx::Rect expected_damage_rect(30, 30, 40, 20);
    EXPECT_EQ(aggregated_frame.render_pass_list[0]->damage_rect,
              expected_damage_rect);
  }
}

// Tests the damage rect with a invalid child frame
TEST_F(SurfaceAggregatorValidSurfaceTest, DamageRectWithInvalidChildFrame) {
  TestSurfaceIdAllocator child_surface_id(child_sink_->frame_sink_id());

  std::vector<Quad> root_surface_quads = {Quad::SurfaceQuad(
      SurfaceRange(std::nullopt, child_surface_id), SkColors::kWhite,
      gfx::Rect(0, 0, 100, 100), /*stretch_content_to_fill_bounds=*/false)};
  std::vector<Pass> root_passes = {
      Pass(root_surface_quads,
           /*size*/ gfx::Size(100, 100),
           /*damage_rect*/ gfx::Rect(10, 10, 20, 20))};

  {
    CompositorFrame root_frame = MakeEmptyCompositorFrame();
    AddPasses(&root_frame.render_pass_list, root_passes,
              &root_frame.metadata.referenced_surfaces);

    root_sink_->SubmitCompositorFrame(root_surface_id_.local_surface_id(),
                                      std::move(root_frame));
  }

  // Frame # 0 - The primary surface of the child frame is not available.
  // The child frame is not submitted.
  // The damage rect of the very first frame is always the full rect.
  {
    auto aggregated_frame = AggregateFrame(root_surface_id_);
    auto* output_root_pass = aggregated_frame.render_pass_list.back().get();
    EXPECT_EQ(gfx::Rect(gfx::Rect(0, 0, 100, 100)),
              output_root_pass->damage_rect);
    EXPECT_EQ(output_root_pass->damage_rect,
              DamageListUnion(aggregated_frame.surface_damage_rect_list_));
  }

  // Frame # 1 - The primary surface of the child frame is not available.
  {
    CompositorFrame root_frame = MakeEmptyCompositorFrame();
    AddPasses(&root_frame.render_pass_list, root_passes,
              &root_frame.metadata.referenced_surfaces);
    root_sink_->SubmitCompositorFrame(root_surface_id_.local_surface_id(),
                                      std::move(root_frame));

    auto aggregated_frame = AggregateFrame(root_surface_id_);
    auto* output_root_pass = aggregated_frame.render_pass_list.back().get();
    // The damage rect is the full display rect when the child surface is not
    // available.
    EXPECT_EQ(gfx::Rect(gfx::Rect(0, 0, 100, 100)),
              output_root_pass->damage_rect);
    // Make sure |surface_damage_rect_list_| is correct.
    EXPECT_EQ(output_root_pass->damage_rect,
              DamageListUnion(aggregated_frame.surface_damage_rect_list_));
  }

  // Frame # 2 - The primary surface is available now.
  // The child frame is submitted
  std::vector<Quad> child_surface_quads = {
      Quad::SolidColorQuad(SkColors::kRed, gfx::Rect(0, 0, 100, 100))};
  std::vector<Pass> child_surface_passes = {Pass(child_surface_quads,
                                                 CompositorRenderPassId{1},
                                                 gfx::Rect(20, 20, 50, 50))};
  {
    CompositorFrame child_surface_frame = MakeEmptyCompositorFrame();
    AddPasses(&child_surface_frame.render_pass_list, child_surface_passes,
              &child_surface_frame.metadata.referenced_surfaces);

    child_sink_->SubmitCompositorFrame(child_surface_id.local_surface_id(),
                                       std::move(child_surface_frame));

    CompositorFrame root_frame = MakeEmptyCompositorFrame();
    AddPasses(&root_frame.render_pass_list, root_passes,
              &root_frame.metadata.referenced_surfaces);
    root_sink_->SubmitCompositorFrame(root_surface_id_.local_surface_id(),
                                      std::move(root_frame));

    auto aggregated_frame = AggregateFrame(root_surface_id_);
    auto* output_root_pass = aggregated_frame.render_pass_list.back().get();
    // The damage rect is the union of root surface damage (10, 10, 20, 20) and
    // child surface (20, 20, 50, 50).
    EXPECT_EQ(gfx::Rect(gfx::Rect(10, 10, 60, 60)),
              output_root_pass->damage_rect);
    EXPECT_EQ(output_root_pass->damage_rect,
              DamageListUnion(aggregated_frame.surface_damage_rect_list_));
  }
  // Frame # 3 - The primary surface is not available, with a different id.
  {
    TestSurfaceIdAllocator child_surface_id2(child_sink_->frame_sink_id());
    std::vector<Quad> new_root_surface_quads = {Quad::SurfaceQuad(
        SurfaceRange(std::nullopt, child_surface_id2), SkColors::kWhite,
        gfx::Rect(0, 0, 100, 100), /*stretch_content_to_fill_bounds=*/false)};
    std::vector<Pass> new_root_passes = {
        Pass(new_root_surface_quads,
             /*size*/ gfx::Size(100, 100),
             /*damage_rect*/ gfx::Rect(10, 10, 20, 20))};

    CompositorFrame root_frame = MakeEmptyCompositorFrame();
    AddPasses(&root_frame.render_pass_list, new_root_passes,
              &root_frame.metadata.referenced_surfaces);
    root_sink_->SubmitCompositorFrame(root_surface_id_.local_surface_id(),
                                      std::move(root_frame));

    auto aggregated_frame = AggregateFrame(root_surface_id_);
    auto* output_root_pass = aggregated_frame.render_pass_list.back().get();
    // The damage rect is the full display rect when the primary child surface
    // is not available.
    EXPECT_EQ(gfx::Rect(gfx::Rect(0, 0, 100, 100)),
              output_root_pass->damage_rect);
    EXPECT_EQ(output_root_pass->damage_rect,
              DamageListUnion(aggregated_frame.surface_damage_rect_list_));
  }
}

// Tests the overlay occluding damage rect
TEST_F(SurfaceAggregatorValidSurfaceTest, OverlayOccludingDamageRect) {
  // Video quad
  std::vector<Quad> child_surface_quads = {
      Quad::TextureQuad(gfx::Rect(0, 0, 100, 100))};

  std::vector<Pass> child_surface_passes = {
      Pass(child_surface_quads, /*size*/ gfx::Size(100, 100),
           /*damage_rect*/ gfx::Rect(0, 0, 100, 100))};

  TestSurfaceIdAllocator child_surface_id(child_sink_->frame_sink_id());
  {
    CompositorFrame child_surface_frame = MakeEmptyCompositorFrame();
    AddPasses(&child_surface_frame.render_pass_list, child_surface_passes,
              &child_surface_frame.metadata.referenced_surfaces);
    PopulateTransferableResources(child_surface_frame);

    child_sink_->SubmitCompositorFrame(child_surface_id.local_surface_id(),
                                       std::move(child_surface_frame));
  }

  // Original video quad (0, 0, 100, 100) x this video_transform matrix ==
  // (10, 0, 80, 80).
  auto video_transform = gfx::Transform::MakeTranslation(10.f, 0) *
                         gfx::Transform::MakeScale(0.8f);

  // root surface quads
  std::vector<Quad> root_surface_quads = {
      Quad::SolidColorQuad(SkColors::kRed, gfx::Rect(60, 0, 40, 40)),
      Quad::SurfaceQuad(
          SurfaceRange(std::nullopt, child_surface_id), SkColors::kWhite,
          /*primary_surface_rect*/ gfx::Rect(0, 0, 100, 100),
          /*opacity*/ 1.f, video_transform,
          /*stretch_content_to_fill_bounds=*/false, gfx::MaskFilterInfo(),
          /*is_fast_rounded_corner=*/false)};

  std::vector<Pass> root_passes = {
      Pass(root_surface_quads,
           /*size*/ gfx::Size(200, 200),
           /*damage_rect*/ gfx::Rect(60, 0, 40, 40))};

  {
    CompositorFrame root_frame = MakeEmptyCompositorFrame();
    AddPasses(&root_frame.render_pass_list, root_passes,
              &root_frame.metadata.referenced_surfaces);

    root_sink_->SubmitCompositorFrame(root_surface_id_.local_surface_id(),
                                      std::move(root_frame));
  }

  {
    auto aggregated_frame = AggregateFrame(root_surface_id_);

    // Frame # 0 - Full occluding damage rect
    // The damage rect of the very first frame is always the full rect.
    auto* output_root_pass = aggregated_frame.render_pass_list.back().get();
    EXPECT_EQ(gfx::Rect(0, 0, 200, 200), output_root_pass->damage_rect);
    // Make sure |surface_damage_rect_list_| is correct.
    EXPECT_EQ(output_root_pass->damage_rect,
              DamageListUnion(aggregated_frame.surface_damage_rect_list_));

    const SharedQuadState* video_sqs =
        output_root_pass->quad_list.back()->shared_quad_state;

    // The whole root surface (0, 0, 200, 200) is damaged.
    EXPECT_EQ(gfx::Rect(0, 0, 200, 200),
              aggregated_frame.surface_damage_rect_list_[0]);

    // Video quad(10, 0, 80, 80) is damaged.
    ASSERT_TRUE(video_sqs->overlay_damage_index.has_value());
    auto index = video_sqs->overlay_damage_index.value();
    EXPECT_EQ(1U, index);
    EXPECT_EQ(gfx::Rect(10, 0, 80, 80),
              aggregated_frame.surface_damage_rect_list_[index]);
  }

  // Frame #1 - Has occluding damage
  {
    CompositorFrame child_surface_frame = MakeEmptyCompositorFrame();
    AddPasses(&child_surface_frame.render_pass_list, child_surface_passes,
              &child_surface_frame.metadata.referenced_surfaces);
    PopulateTransferableResources(child_surface_frame);
    child_sink_->SubmitCompositorFrame(child_surface_id.local_surface_id(),
                                       std::move(child_surface_frame));

    CompositorFrame root_frame = MakeEmptyCompositorFrame();
    AddPasses(&root_frame.render_pass_list, root_passes,
              &root_frame.metadata.referenced_surfaces);

    root_sink_->SubmitCompositorFrame(root_surface_id_.local_surface_id(),
                                      std::move(root_frame));

    auto aggregated_frame = AggregateFrame(root_surface_id_);

    auto* output_root_pass = aggregated_frame.render_pass_list.back().get();
    // The video quad (10, 0, 80, 80) unions the solid quad on top (60, 0, 40,
    // 40).
    EXPECT_EQ(gfx::Rect(10, 0, 90, 80), output_root_pass->damage_rect);
    EXPECT_EQ(output_root_pass->damage_rect,
              DamageListUnion(aggregated_frame.surface_damage_rect_list_));

    const SharedQuadState* video_sqs =
        output_root_pass->quad_list.back()->shared_quad_state;
    // The solid quad on top (60, 0, 40, 40) is damaged.
    EXPECT_EQ(gfx::Rect(60, 0, 40, 40),
              aggregated_frame.surface_damage_rect_list_[0]);

    // Video quad(10, 0, 80, 80) is damaged.
    EXPECT_TRUE(video_sqs->overlay_damage_index.has_value());
    auto index = video_sqs->overlay_damage_index.value();
    EXPECT_EQ(1U, index);
    EXPECT_EQ(gfx::Rect(10, 0, 80, 80),
              aggregated_frame.surface_damage_rect_list_[index]);
  }
  // Frame #2 - No occluding damage, the quad on top doesn't change
  {
    CompositorFrame child_surface_frame = MakeEmptyCompositorFrame();
    AddPasses(&child_surface_frame.render_pass_list, child_surface_passes,
              &child_surface_frame.metadata.referenced_surfaces);
    child_sink_->SubmitCompositorFrame(child_surface_id.local_surface_id(),
                                       std::move(child_surface_frame));

    // No change in root frame.
    auto aggregated_frame = AggregateFrame(root_surface_id_);

    auto* output_root_pass = aggregated_frame.render_pass_list.back().get();
    // Only the video quad (10, 0, 80, 80) is damaged.
    EXPECT_EQ(gfx::Rect(10, 0, 80, 80), output_root_pass->damage_rect);
    EXPECT_EQ(output_root_pass->damage_rect,
              DamageListUnion(aggregated_frame.surface_damage_rect_list_));

    const SharedQuadState* video_sqs =
        output_root_pass->quad_list.back()->shared_quad_state;

    // No occluding damage.
    // The solid quad on top (60, 0, 40, 40) is not damaged.
    EXPECT_TRUE(video_sqs->overlay_damage_index.has_value());
    auto index = video_sqs->overlay_damage_index.value();
    EXPECT_EQ(0U, index);

    // Video quad(10, 0, 80, 80) is damaged
    EXPECT_EQ(gfx::Rect(10, 0, 80, 80),
              aggregated_frame.surface_damage_rect_list_[index]);
  }
  // Frame #3 - The only quad on top is removed
  {
    CompositorFrame child_surface_frame = MakeEmptyCompositorFrame();
    AddPasses(&child_surface_frame.render_pass_list, child_surface_passes,
              &child_surface_frame.metadata.referenced_surfaces);
    child_sink_->SubmitCompositorFrame(child_surface_id.local_surface_id(),
                                       std::move(child_surface_frame));

    // root surface quads, the solid quad (60, 0, 40, 40) is removed.
    std::vector<Quad> new_root_surface_quads = {Quad::SurfaceQuad(
        SurfaceRange(std::nullopt, child_surface_id), SkColors::kWhite,
        /*primary_surface_rect*/ gfx::Rect(0, 0, 100, 100),
        /*opacity*/ 1.f, video_transform,
        /*stretch_content_to_fill_bounds=*/false, gfx::MaskFilterInfo(),
        /*is_fast_rounded_corner=*/false)};

    std::vector<Pass> new_root_passes = {
        Pass(new_root_surface_quads,
             /*size*/ gfx::Size(200, 200),
             /*damage_rect*/ gfx::Rect(60, 0, 40, 40))};

    CompositorFrame root_frame = MakeEmptyCompositorFrame();
    AddPasses(&root_frame.render_pass_list, new_root_passes,
              &root_frame.metadata.referenced_surfaces);

    root_sink_->SubmitCompositorFrame(root_surface_id_.local_surface_id(),
                                      std::move(root_frame));

    auto aggregated_frame = AggregateFrame(root_surface_id_);

    auto* output_root_pass = aggregated_frame.render_pass_list.back().get();
    // The video quad (10, 0, 80, 80) unions the expose damage from removing
    // the solid quad on top (60, 0, 40, 40).
    EXPECT_EQ(gfx::Rect(10, 0, 90, 80), output_root_pass->damage_rect);
    EXPECT_EQ(output_root_pass->damage_rect,
              DamageListUnion(aggregated_frame.surface_damage_rect_list_));

    const SharedQuadState* video_sqs =
        output_root_pass->quad_list.back()->shared_quad_state;

    // The expose damage (60, 0, 40, 40) on top.
    EXPECT_EQ(gfx::Rect(60, 0, 40, 40),
              aggregated_frame.surface_damage_rect_list_[0]);

    // Video quad(10, 0, 80, 80) is damaged.
    EXPECT_TRUE(video_sqs->overlay_damage_index.has_value());
    auto index = video_sqs->overlay_damage_index.value();
    EXPECT_EQ(1U, index);
    EXPECT_EQ(gfx::Rect(10, 0, 80, 80),
              aggregated_frame.surface_damage_rect_list_[index]);
  }
  // Frame #4 - Has occluding damage and clipping of the video quad is on
  {
    CompositorFrame child_surface_frame = MakeEmptyCompositorFrame();
    AddPasses(&child_surface_frame.render_pass_list, child_surface_passes,
              &child_surface_frame.metadata.referenced_surfaces);

    auto* render_pass = child_surface_frame.render_pass_list[0].get();
    auto* surface_quad_sqs = render_pass->shared_quad_state_list.front();
    surface_quad_sqs->clip_rect = gfx::Rect(20, 0, 60, 80);

    child_sink_->SubmitCompositorFrame(child_surface_id.local_surface_id(),
                                       std::move(child_surface_frame));

    CompositorFrame root_frame = MakeEmptyCompositorFrame();
    AddPasses(&root_frame.render_pass_list, root_passes,
              &root_frame.metadata.referenced_surfaces);

    root_sink_->SubmitCompositorFrame(root_surface_id_.local_surface_id(),
                                      std::move(root_frame));

    auto aggregated_frame = AggregateFrame(root_surface_id_);

    auto* output_root_pass = aggregated_frame.render_pass_list.back().get();
    // The video quad (10, 0, 80, 80) unions the solid quad on top (60, 0, 40,
    // 40).
    EXPECT_EQ(gfx::Rect(10, 0, 90, 80), output_root_pass->damage_rect);
    EXPECT_EQ(output_root_pass->damage_rect,
              DamageListUnion(aggregated_frame.surface_damage_rect_list_));

    const SharedQuadState* video_sqs =
        output_root_pass->quad_list.back()->shared_quad_state;

    // The damaged solid quad on top (60, 0, 40, 40).
    EXPECT_EQ(gfx::Rect(60, 0, 40, 40),
              aggregated_frame.surface_damage_rect_list_[0]);

    // Video quad(10, 0, 80, 80) is damaged.
    EXPECT_TRUE(video_sqs->overlay_damage_index.has_value());
    auto index = video_sqs->overlay_damage_index.value();
    EXPECT_EQ(1U, index);
    EXPECT_EQ(gfx::Rect(10, 0, 80, 80),
              aggregated_frame.surface_damage_rect_list_[index]);
  }
  // Frame #5 - Has occluding damage and clipping of surface on top is on
  {
    CompositorFrame child_surface_frame = MakeEmptyCompositorFrame();
    AddPasses(&child_surface_frame.render_pass_list, child_surface_passes,
              &child_surface_frame.metadata.referenced_surfaces);

    child_sink_->SubmitCompositorFrame(child_surface_id.local_surface_id(),
                                       std::move(child_surface_frame));

    // root surface quads
    std::vector<Quad> new_root_surface_quads = {
        Quad::SolidColorQuad(SkColors::kRed, gfx::Rect(60, 0, 100, 100)),
        Quad::SurfaceQuad(
            SurfaceRange(std::nullopt, child_surface_id), SkColors::kWhite,
            /*primary_surface_rect*/ gfx::Rect(0, 0, 100, 100),
            /*opacity*/ 1.f, video_transform,
            /*stretch_content_to_fill_bounds=*/false, gfx::MaskFilterInfo(),
            /*is_fast_rounded_corner=*/false)};

    std::vector<Pass> new_root_passes = {
        Pass(new_root_surface_quads,
             /*size*/ gfx::Size(200, 200),
             /*damage_rect*/ gfx::Rect(60, 0, 80, 70))};

    CompositorFrame root_frame = MakeEmptyCompositorFrame();
    AddPasses(&root_frame.render_pass_list, new_root_passes,
              &root_frame.metadata.referenced_surfaces);

    auto* last_pass = root_frame.render_pass_list.back().get();
    auto* solid_quad_sqs = last_pass->shared_quad_state_list.front();
    solid_quad_sqs->clip_rect = gfx::Rect(80, 0, 40, 30);

    root_sink_->SubmitCompositorFrame(root_surface_id_.local_surface_id(),
                                      std::move(root_frame));

    auto aggregated_frame = AggregateFrame(root_surface_id_);

    auto* output_root_pass = aggregated_frame.render_pass_list.back().get();
    // The video quad (10, 0, 80, 80) unions the clipped damage rect of the
    // solid quad on top (60, 0, 80, 70) where the clip rect (80, 0, 40, 30).
    EXPECT_EQ(gfx::Rect(10, 0, 130, 80), output_root_pass->damage_rect);
    EXPECT_EQ(output_root_pass->damage_rect,
              DamageListUnion(aggregated_frame.surface_damage_rect_list_));

    const SharedQuadState* video_sqs =
        output_root_pass->quad_list.back()->shared_quad_state;

    // Video quad(10, 0, 80, 80) is damaged.
    EXPECT_TRUE(video_sqs->overlay_damage_index.has_value());
    auto index = video_sqs->overlay_damage_index.value();
    EXPECT_EQ(1U, index);
    EXPECT_EQ(gfx::Rect(10, 0, 80, 80),
              aggregated_frame.surface_damage_rect_list_[index]);
  }

  // Add a quad on top of video quad.
  child_surface_quads = std::vector<Quad>(
      {Quad::SolidColorQuad(SkColors::kRed, gfx::Rect(0, 0, 50, 50)),
       Quad::TextureQuad(gfx::Rect(0, 0, 100, 100), true)});

  child_surface_passes =
      std::vector<Pass>({Pass(child_surface_quads, /*size*/ gfx::Size(100, 100),
                              /*damage_rect*/ gfx::Rect(0, 0, 100, 100))});

  // Frame #6 - Child surface contains a quad other than the video
  {
    CompositorFrame child_surface_frame = MakeEmptyCompositorFrame();
    AddPasses(&child_surface_frame.render_pass_list, child_surface_passes,
              &child_surface_frame.metadata.referenced_surfaces);
    child_sink_->SubmitCompositorFrame(child_surface_id.local_surface_id(),
                                       std::move(child_surface_frame));

    // No change in root frame.
    auto aggregated_frame = AggregateFrame(root_surface_id_);

    auto* output_root_pass = aggregated_frame.render_pass_list.back().get();
    // Only the video quad (10, 0, 80, 80) is damaged.
    EXPECT_EQ(gfx::Rect(10, 0, 80, 80), output_root_pass->damage_rect);
    EXPECT_EQ(output_root_pass->damage_rect,
              DamageListUnion(aggregated_frame.surface_damage_rect_list_));

    const SharedQuadState* video_sqs =
        output_root_pass->quad_list.back()->shared_quad_state;
    // The underlay optimization doesn't apply with multiple
    // possibly damaged quads.
    EXPECT_FALSE(video_sqs->overlay_damage_index.has_value());
  }
  // Frame #7 - Child surface contains an undamaged quad other than the video
  {
    CompositorFrame child_surface_frame = MakeEmptyCompositorFrame();
    AddPasses(&child_surface_frame.render_pass_list, child_surface_passes,
              &child_surface_frame.metadata.referenced_surfaces);
    auto* render_pass = child_surface_frame.render_pass_list[0].get();
    render_pass->has_per_quad_damage = true;

    child_sink_->SubmitCompositorFrame(child_surface_id.local_surface_id(),
                                       std::move(child_surface_frame));

    // No change in root frame.
    auto aggregated_frame = AggregateFrame(root_surface_id_);

    auto* output_root_pass = aggregated_frame.render_pass_list.back().get();
    // Only the video quad (10, 0, 80, 80) is damaged.
    EXPECT_EQ(gfx::Rect(10, 0, 80, 80), output_root_pass->damage_rect);
    EXPECT_EQ(output_root_pass->damage_rect,
              DamageListUnion(aggregated_frame.surface_damage_rect_list_));

    const SharedQuadState* video_sqs =
        output_root_pass->quad_list.back()->shared_quad_state;
    // No occluding damage.
    EXPECT_TRUE(video_sqs->overlay_damage_index.has_value());
    auto index = video_sqs->overlay_damage_index.value();
    EXPECT_EQ(1U, index);

    // Video quad(10, 0, 80, 80) is damaged.
    EXPECT_EQ(gfx::Rect(10, 0, 80, 80),
              aggregated_frame.surface_damage_rect_list_[index]);
  }
}

// Tests the |per_quad_damage| feature by adding a few quads, flagged with
// |per_quad_damage|, and then checking the output damage after surface
// aggregation. By placing these quads in a surface we also test that the
// correct relevant transforms have been applied by examining the
// |surface_damage_rect_list_|.
TEST_F(SurfaceAggregatorValidSurfaceTest, RenderPassHasPerQuadDamage) {
  // Video quad
  gfx::Rect surface_quad_rect = gfx::Rect(0, 0, 100, 100);
  std::vector<Quad> child_surface_quads = {
      Quad::TextureQuad(surface_quad_rect)};

  std::vector<Pass> child_surface_passes = {
      Pass(child_surface_quads, /*size*/ gfx::Size(100, 100),
           /*damage_rect*/ gfx::Rect(0, 0, 100, 100))};

  // Various rects configs that will be used to test per quad damage.
  gfx::Rect quad_rects[] = {gfx::Rect(60, 0, 40, 40), gfx::Rect(0, 0, 50, 50),
                            gfx::Rect(0, 0, 75, 25), gfx::Rect(10, 0, 30, 30),
                            gfx::Rect(0, 5, 50, 50)};

  TestSurfaceIdAllocator child_surface_id(child_sink_->frame_sink_id());
  {
    CompositorFrame child_surface_frame = MakeEmptyCompositorFrame();
    AddPasses(&child_surface_frame.render_pass_list, child_surface_passes,
              &child_surface_frame.metadata.referenced_surfaces);
    PopulateTransferableResources(child_surface_frame);

    child_sink_->SubmitCompositorFrame(child_surface_id.local_surface_id(),
                                       std::move(child_surface_frame));
  }
  gfx::PointF child_surface_offset(10.0f, 5.0f);
  gfx::Transform child_surface_transform = gfx::Transform::MakeTranslation(
      child_surface_offset.x(), child_surface_offset.y());

  auto apply_transform = [child_surface_offset](const gfx::Rect orig_rect) {
    auto rtn_rect = orig_rect;
    rtn_rect.set_x(static_cast<int>(child_surface_offset.x()) + rtn_rect.x());
    rtn_rect.set_y(static_cast<int>(child_surface_offset.y()) + rtn_rect.y());
    return rtn_rect;
  };

  // root surface quads
  std::vector<Quad> root_surface_quads = {
      Quad::SolidColorQuad(SkColors::kRed, quad_rects[0]),
      Quad::SurfaceQuad(
          SurfaceRange(std::nullopt, child_surface_id), SkColors::kWhite,
          /*primary_surface_rect*/ gfx::Rect(0, 0, 100, 100),
          /*opacity*/ 1.f, child_surface_transform,
          /*stretch_content_to_fill_bounds=*/false, gfx::MaskFilterInfo(),
          /*is_fast_rounded_corner=*/false)};

  std::vector<Pass> root_passes = {Pass(root_surface_quads,
                                        /*size*/ gfx::Size(200, 200),
                                        /*damage_rect*/ quad_rects[0])};

  CompositorFrame root_frame = MakeEmptyCompositorFrame();
  AddPasses(&root_frame.render_pass_list, root_passes,
            &root_frame.metadata.referenced_surfaces);

  root_sink_->SubmitCompositorFrame(root_surface_id_.local_surface_id(),
                                    std::move(root_frame));

  {
    auto aggregated_frame = AggregateFrame(root_surface_id_);

    // Initial test frame - Full occluding damage rect
    // The damage rect of the very first frame is always the full rect.
    auto* output_root_pass = aggregated_frame.render_pass_list.back().get();
    EXPECT_EQ(gfx::Rect(0, 0, 200, 200), output_root_pass->damage_rect);
    // Make sure |surface_damage_rect_list_| is correct.
    EXPECT_EQ(output_root_pass->damage_rect,
              DamageListUnion(aggregated_frame.surface_damage_rect_list_));

    const SharedQuadState* video_sqs =
        output_root_pass->quad_list.back()->shared_quad_state;

    // The whole root surface (0, 0, 200, 200) is damaged.
    EXPECT_EQ(gfx::Rect(0, 0, 200, 200),
              aggregated_frame.surface_damage_rect_list_[0]);

    ASSERT_TRUE(video_sqs->overlay_damage_index.has_value());
    auto index = video_sqs->overlay_damage_index.value();
    EXPECT_EQ(1U, index);
    EXPECT_EQ(apply_transform(surface_quad_rect),
              aggregated_frame.surface_damage_rect_list_[index]);
  }

  // Frame that has three quads that are flagged with per quad damage.
  // Add a quad on top of video quad.
  child_surface_quads = std::vector<Quad>({
      Quad::SolidColorQuad(SkColors::kRed, quad_rects[1]),
      Quad::TextureQuad(quad_rects[2], true),
      Quad::TextureQuad(quad_rects[3], true),
      Quad::TextureQuad(quad_rects[4], true),
  });

  child_surface_passes = std::vector<Pass>(
      {Pass(child_surface_quads, gfx::Size(100, 100), quad_rects[1])});

  {
    CompositorFrame child_surface_frame = MakeEmptyCompositorFrame();
    AddPasses(&child_surface_frame.render_pass_list, child_surface_passes,
              &child_surface_frame.metadata.referenced_surfaces);
    auto* render_pass = child_surface_frame.render_pass_list[0].get();
    render_pass->has_per_quad_damage = true;

    child_sink_->SubmitCompositorFrame(child_surface_id.local_surface_id(),
                                       std::move(child_surface_frame));
    // No change in root frame.
    auto aggregated_frame = AggregateFrame(root_surface_id_);
    auto* output_root_pass = aggregated_frame.render_pass_list.back().get();
    EXPECT_EQ(output_root_pass->damage_rect,
              DamageListUnion(aggregated_frame.surface_damage_rect_list_));
    EXPECT_EQ(apply_transform(quad_rects[1]),
              aggregated_frame.surface_damage_rect_list_[0]);
    EXPECT_EQ(4u, aggregated_frame.surface_damage_rect_list_.size());
    EXPECT_EQ(5u, output_root_pass->quad_list.size());
    uint32_t i = 0;
    // There should be 5 quads in total:
    // 0    - root color quad
    // 1    - surface color quad
    // 2-4  - Quads that have |per_quad_damage|
    for (auto* quad : output_root_pass->quad_list) {
      EXPECT_EQ(quad_rects[i], quad->rect);

      // Looking at only the quads with |per_quad_damage|.
      if (i >= 2) {
        const SharedQuadState* sqs = quad->shared_quad_state;
        EXPECT_TRUE(sqs->overlay_damage_index.has_value());
        auto index = sqs->overlay_damage_index.value();
        EXPECT_EQ(i - 1, index);
        EXPECT_EQ(apply_transform(quad_rects[i]),
                  aggregated_frame.surface_damage_rect_list_[i - 1]);
      }
      i++;
    }
  }
}

// Per quad damage can appear on quads that have the same 'shared_quad_state'.
// We need to make sure this will generate independent damage in the output
// listing.
TEST_F(SurfaceAggregatorValidSurfaceTest, PerQuadDamageSameSharedQuadState) {
  gfx::Rect quad_rects[] = {gfx::Rect(60, 0, 40, 40), gfx::Rect(0, 0, 50, 50)};

  gfx::Rect damage_rects[] = {gfx::Rect(60, 0, 30, 30),
                              gfx::Rect(0, 0, 20, 20)};

  auto pass = CompositorRenderPass::Create();
  pass->SetNew(CompositorRenderPassId{1}, gfx::Rect(0, 0, 200, 200),
               gfx::Rect(), gfx::Transform());

  auto* sqs = pass->CreateAndAppendSharedQuadState();
  pass->has_per_quad_damage = true;

  for (int i = 0; i < 2; i++) {
    auto* texure_quad = pass->CreateAndAppendDrawQuad<TextureDrawQuad>();

    const gfx::PointF kUVTopLeft(0.1f, 0.2f);
    const gfx::PointF kUVBottomRight(1.0f, 1.0f);
    texure_quad->SetNew(
        sqs, quad_rects[i], quad_rects[i], false /*needs_blending*/,
        ResourceId(1), false /*premultiplied_alpha*/, kUVTopLeft,
        kUVBottomRight, SkColors::kTransparent, false /*flipped*/,
        false /*nearest_neighbor*/, false /*secure_output_only*/,
        gfx::ProtectedVideoType::kClear);

    texure_quad->damage_rect = damage_rects[i];
  }

  CompositorFrame root_frame =
      CompositorFrameBuilder().AddRenderPass(std::move(pass)).Build();

  PopulateTransferableResources(root_frame);
  root_sink_->SubmitCompositorFrame(root_surface_id_.local_surface_id(),
                                    std::move(root_frame));
  auto aggregated_frame = AggregateFrame(root_surface_id_);
  auto* output_root_pass = aggregated_frame.render_pass_list.back().get();

  EXPECT_EQ(output_root_pass->quad_list.size(), 2u);
  EXPECT_GE(aggregated_frame.surface_damage_rect_list_.size(), 2u);

  int draw_rect_index = 0;
  for (auto* quad : output_root_pass->quad_list) {
    auto* quad_sqs = quad->shared_quad_state;
    EXPECT_TRUE(quad_sqs->overlay_damage_index.has_value());
    EXPECT_EQ(
        aggregated_frame
            .surface_damage_rect_list_[quad_sqs->overlay_damage_index.value()],
        damage_rects[draw_rect_index]);
    draw_rect_index++;
  }
}

TEST_F(SurfaceAggregatorValidSurfaceTest, QuadContainsSurfaceDamageRect) {
  // Video quad
  gfx::Rect surface_quad_rect = gfx::Rect(0, 0, 100, 100);
  gfx::Rect video_quad_rect = gfx::Rect(0, 0, 50, 50);
  auto video_quad = Quad::TextureQuad(video_quad_rect);

  std::vector<Pass> child_surface_passes = {
      Pass({video_quad}, /*size=*/surface_quad_rect.size(),
           /*damage_rect=*/video_quad_rect)};

  TestSurfaceIdAllocator child_surface_id(child_sink_->frame_sink_id());
  {
    CompositorFrame child_surface_frame = MakeEmptyCompositorFrame();
    AddPasses(&child_surface_frame.render_pass_list, child_surface_passes,
              &child_surface_frame.metadata.referenced_surfaces);
    PopulateTransferableResources(child_surface_frame);
    child_sink_->SubmitCompositorFrame(child_surface_id.local_surface_id(),
                                       std::move(child_surface_frame));
  }

  gfx::PointF child_surface_offset(10.0f, 5.0f);
  gfx::Transform child_surface_transform = gfx::Transform::MakeTranslation(
      child_surface_offset.x(), child_surface_offset.y());

  auto apply_offset = [child_surface_offset](const gfx::Rect orig_rect) {
    auto rtn_rect = orig_rect;
    rtn_rect.set_x(static_cast<int>(child_surface_offset.x()) + rtn_rect.x());
    rtn_rect.set_y(static_cast<int>(child_surface_offset.y()) + rtn_rect.y());
    return rtn_rect;
  };

  {  // First frame will have full damage for each surface.
    gfx::Rect red_rect = gfx::Rect(60, 0, 40, 40);
    // root surface quads
    std::vector<Quad> root_surface_quads = {
        Quad::SolidColorQuad(SkColors::kRed, red_rect),
        Quad::SurfaceQuad(
            SurfaceRange(std::nullopt, child_surface_id), SkColors::kWhite,
            /*primary_surface_rect*/ surface_quad_rect,
            /*opacity*/ 1.f, child_surface_transform,
            /*stretch_content_to_fill_bounds=*/false, gfx::MaskFilterInfo(),
            /*is_fast_rounded_corner=*/false)};
    std::vector<Pass> root_passes = {Pass(root_surface_quads,
                                          /*size=*/gfx::Size(200, 200),
                                          /*damage_rect=*/red_rect)};

    CompositorFrame root_frame = MakeEmptyCompositorFrame();
    AddPasses(&root_frame.render_pass_list, root_passes,
              &root_frame.metadata.referenced_surfaces);
    root_sink_->SubmitCompositorFrame(root_surface_id_.local_surface_id(),
                                      std::move(root_frame));
    auto aggregated_frame = AggregateFrame(root_surface_id_);
    auto* output_root_pass = aggregated_frame.render_pass_list.back().get();

    EXPECT_EQ(gfx::Rect(0, 0, 200, 200), output_root_pass->damage_rect);
    EXPECT_EQ(gfx::Rect(0, 0, 200, 200),
              aggregated_frame.surface_damage_rect_list_[0]);
    EXPECT_EQ(apply_offset(surface_quad_rect),
              aggregated_frame.surface_damage_rect_list_[1]);

    const SharedQuadState* video_sqs =
        output_root_pass->quad_list.back()->shared_quad_state;
    // Surface damage is larger than the video quad.
    ASSERT_FALSE(video_sqs->overlay_damage_index.has_value());
  }

  {  // Same frame submitted to child surface.
    CompositorFrame child_surface_frame = MakeEmptyCompositorFrame();
    AddPasses(&child_surface_frame.render_pass_list, child_surface_passes,
              &child_surface_frame.metadata.referenced_surfaces);
    child_sink_->SubmitCompositorFrame(child_surface_id.local_surface_id(),
                                       std::move(child_surface_frame));
    auto aggregated_frame = AggregateFrame(root_surface_id_);
    auto* output_root_pass = aggregated_frame.render_pass_list.back().get();

    EXPECT_EQ(apply_offset(video_quad_rect), output_root_pass->damage_rect);
    EXPECT_EQ(output_root_pass->damage_rect,
              DamageListUnion(aggregated_frame.surface_damage_rect_list_));

    const SharedQuadState* video_sqs =
        output_root_pass->quad_list.back()->shared_quad_state;

    // Surface damage can be assigned to the video quad.
    ASSERT_TRUE(video_sqs->overlay_damage_index.has_value());
    auto index = video_sqs->overlay_damage_index.value();
    EXPECT_EQ(0U, index);
    EXPECT_EQ(apply_offset(video_quad_rect),
              aggregated_frame.surface_damage_rect_list_[index]);
  }

  gfx::Rect scaled_video_rect = gfx::Rect(0, 0, 55, 55);

  {  // Video quad on child surface scales to 110%
    auto scaled_video_quad = Quad::TextureQuad(video_quad_rect);
    scaled_video_quad.to_target_transform.Scale(1.1f);

    child_surface_passes = std::vector<Pass>(
        {Pass({scaled_video_quad}, /*size=*/surface_quad_rect.size(),
              /*damage_rect=*/scaled_video_rect)});

    CompositorFrame child_surface_frame = MakeEmptyCompositorFrame();
    AddPasses(&child_surface_frame.render_pass_list, child_surface_passes,
              &child_surface_frame.metadata.referenced_surfaces);
    child_sink_->SubmitCompositorFrame(child_surface_id.local_surface_id(),
                                       std::move(child_surface_frame));
    auto aggregated_frame = AggregateFrame(root_surface_id_);
    auto& output_root_pass = aggregated_frame.render_pass_list.back();

    EXPECT_EQ(apply_offset(scaled_video_rect), output_root_pass->damage_rect);
    EXPECT_EQ(output_root_pass->damage_rect,
              DamageListUnion(aggregated_frame.surface_damage_rect_list_));

    const SharedQuadState* video_sqs =
        output_root_pass->quad_list.back()->shared_quad_state;

    ASSERT_TRUE(video_sqs->overlay_damage_index.has_value());
    auto index = video_sqs->overlay_damage_index.value();
    EXPECT_EQ(0U, index);
    EXPECT_EQ(apply_offset(scaled_video_rect),
              aggregated_frame.surface_damage_rect_list_[index]);
  }

  {  // Video quad scales back to 100%
    child_surface_passes =
        std::vector<Pass>({Pass({video_quad}, /*size=*/surface_quad_rect.size(),
                                /*damage_rect=*/scaled_video_rect)});

    CompositorFrame child_surface_frame = MakeEmptyCompositorFrame();
    AddPasses(&child_surface_frame.render_pass_list, child_surface_passes,
              &child_surface_frame.metadata.referenced_surfaces);
    child_sink_->SubmitCompositorFrame(child_surface_id.local_surface_id(),
                                       std::move(child_surface_frame));
    auto aggregated_frame = AggregateFrame(root_surface_id_);
    auto* output_root_pass = aggregated_frame.render_pass_list.back().get();

    EXPECT_EQ(apply_offset(scaled_video_rect), output_root_pass->damage_rect);
    EXPECT_EQ(output_root_pass->damage_rect,
              DamageListUnion(aggregated_frame.surface_damage_rect_list_));

    const SharedQuadState* video_sqs =
        output_root_pass->quad_list.back()->shared_quad_state;
    // The damage is still the size of the scale rect, which is larger than the
    // video quad this frame, so damage is not assigned to this quad.
    ASSERT_FALSE(video_sqs->overlay_damage_index.has_value());
  }

  auto moved_video_quad = Quad::TextureQuad(video_quad_rect);
  moved_video_quad.to_target_transform.Translate(gfx::Vector2dF(3, 0));

  {  // Video moves 3px right
    gfx::Rect moved_damage = gfx::Rect(0, 0, 53, 50);
    child_surface_passes = std::vector<Pass>(
        {Pass({moved_video_quad}, /*size=*/surface_quad_rect.size(),
              /*damage_rect=*/moved_damage)});

    CompositorFrame child_surface_frame = MakeEmptyCompositorFrame();
    AddPasses(&child_surface_frame.render_pass_list, child_surface_passes,
              &child_surface_frame.metadata.referenced_surfaces);
    child_sink_->SubmitCompositorFrame(child_surface_id.local_surface_id(),
                                       std::move(child_surface_frame));
    auto aggregated_frame = AggregateFrame(root_surface_id_);
    auto* output_root_pass = aggregated_frame.render_pass_list.back().get();

    EXPECT_EQ(apply_offset(moved_damage), output_root_pass->damage_rect);
    EXPECT_EQ(output_root_pass->damage_rect,
              DamageListUnion(aggregated_frame.surface_damage_rect_list_));

    const SharedQuadState* video_sqs =
        output_root_pass->quad_list.back()->shared_quad_state;
    // The damage from moving the video is wider than the video quad.
    ASSERT_FALSE(video_sqs->overlay_damage_index.has_value());
  }

  {  // Stays at 3px right
    gfx::Rect moved_video_rect = gfx::Rect(3, 0, 50, 50);
    child_surface_passes = std::vector<Pass>(
        {Pass({moved_video_quad}, /*size=*/surface_quad_rect.size(),
              /*damage_rect=*/moved_video_rect)});

    CompositorFrame child_surface_frame = MakeEmptyCompositorFrame();
    AddPasses(&child_surface_frame.render_pass_list, child_surface_passes,
              &child_surface_frame.metadata.referenced_surfaces);
    child_sink_->SubmitCompositorFrame(child_surface_id.local_surface_id(),
                                       std::move(child_surface_frame));
    auto aggregated_frame = AggregateFrame(root_surface_id_);
    auto* output_root_pass = aggregated_frame.render_pass_list.back().get();

    EXPECT_EQ(apply_offset(moved_video_rect), output_root_pass->damage_rect);
    EXPECT_EQ(output_root_pass->damage_rect,
              DamageListUnion(aggregated_frame.surface_damage_rect_list_));

    const SharedQuadState* video_sqs =
        output_root_pass->quad_list.back()->shared_quad_state;
    // The damage is now the same rect as the video, and can be assigned.
    ASSERT_TRUE(video_sqs->overlay_damage_index.has_value());
    auto index = video_sqs->overlay_damage_index.value();
    EXPECT_EQ(0U, index);
    EXPECT_EQ(apply_offset(moved_video_rect),
              aggregated_frame.surface_damage_rect_list_[index]);
  }
}

// Check that the overlay damage index is set for quads in non-root render
// passes. This can be useful e.g. if we want to do overlay processing even if
// the web contents surface does not merge.
TEST_F(SurfaceAggregatorValidSurfaceTest,
       OverlayDamageIndexFromNonRootSurface) {
  const gfx::Rect video_quad_rect = gfx::Rect(0, 0, 50, 50);
  const gfx::Rect child_surface_quad_rect = gfx::Rect(0, 0, 100, 100);
  const gfx::Rect root_surface_rect = gfx::Rect(200, 200);

  const gfx::Transform transform_child_surface_to_root =
      gfx::Transform::MakeTranslation(0.0f, 10.0f);
  const gfx::Transform transform_video_to_child_surface =
      gfx::Transform::MakeTranslation(10.0f, 0.0f);
  const gfx::Rect video_rect_in_root =
      (transform_child_surface_to_root * transform_video_to_child_surface)
          .MapRect(video_quad_rect);
  const gfx::Rect child_surface_rect_in_root =
      transform_child_surface_to_root.MapRect(child_surface_quad_rect);

  auto video_embedded_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryFrameSinkId1, /*is_root=*/false);

  TestSurfaceIdAllocator video_surface_id(
      video_embedded_support->frame_sink_id());
  auto video_quad = Quad::TextureQuad(video_quad_rect);

  std::vector<Pass> video_surface_passes = {
      Pass({video_quad}, /*size=*/video_quad_rect.size(),
           /*damage_rect=*/video_quad_rect)};
  {
    CompositorFrame video_surface_frame = MakeEmptyCompositorFrame();
    AddPasses(&video_surface_frame.render_pass_list, video_surface_passes,
              &video_surface_frame.metadata.referenced_surfaces);
    PopulateTransferableResources(video_surface_frame);

    video_embedded_support->SubmitCompositorFrame(
        video_surface_id.local_surface_id(), std::move(video_surface_frame));
  }

  TestSurfaceIdAllocator child_surface_id(child_sink_->frame_sink_id());
  {
    auto surface_quad = Quad::SurfaceQuad(
        SurfaceRange(std::nullopt, video_surface_id), SkColors::kWhite,
        /*primary_surface_rect*/ video_quad_rect,
        /*opacity*/ 1.f, transform_video_to_child_surface,
        /*stretch_content_to_fill_bounds=*/false, gfx::MaskFilterInfo(),
        /*is_fast_rounded_corner=*/false);
    // TODO doc
    surface_quad.allow_merge = false;

    std::vector<Pass> child_surface_passes = {
        Pass({surface_quad},
             /*size=*/child_surface_quad_rect.size())};
    CompositorFrame child_surface_frame = MakeEmptyCompositorFrame();
    AddPasses(&child_surface_frame.render_pass_list, child_surface_passes,
              &child_surface_frame.metadata.referenced_surfaces);
    PopulateTransferableResources(child_surface_frame);

    child_sink_->SubmitCompositorFrame(child_surface_id.local_surface_id(),
                                       std::move(child_surface_frame));
  }

  // First frame will have full damage for each surface.
  {
    std::vector<Quad> root_surface_quads = {Quad::SurfaceQuad(
        SurfaceRange(std::nullopt, child_surface_id), SkColors::kWhite,
        /*primary_surface_rect*/ child_surface_quad_rect,
        /*opacity*/ 1.f, transform_child_surface_to_root,
        /*stretch_content_to_fill_bounds=*/false, gfx::MaskFilterInfo(),
        /*is_fast_rounded_corner=*/false)};
    std::vector<Pass> root_passes = {Pass(root_surface_quads,
                                          /*size=*/root_surface_rect.size())};

    CompositorFrame root_frame = MakeEmptyCompositorFrame();
    AddPasses(&root_frame.render_pass_list, root_passes,
              &root_frame.metadata.referenced_surfaces);
    root_sink_->SubmitCompositorFrame(root_surface_id_.local_surface_id(),
                                      std::move(root_frame));
    auto aggregated_frame = AggregateFrame(root_surface_id_);
    auto* output_root_pass = aggregated_frame.render_pass_list.back().get();

    EXPECT_EQ(root_surface_rect, output_root_pass->damage_rect);
    EXPECT_THAT(aggregated_frame.surface_damage_rect_list_,
                testing::ElementsAreArray({
                    root_surface_rect,
                    child_surface_rect_in_root,
                    video_rect_in_root,
                }));
    EXPECT_EQ(DamageListUnion(aggregated_frame.surface_damage_rect_list_),
              output_root_pass->damage_rect);

    EXPECT_EQ(2u, aggregated_frame.render_pass_list.size())
        << "Test assumes surface does not merge";
    EXPECT_THAT(aggregated_frame.render_pass_list[0]->quad_list,
                ElementsAre(IsTextureQuad()));
    EXPECT_THAT(aggregated_frame.render_pass_list[1]->quad_list,
                ElementsAre(IsAggregatedRenderPassQuad()));
  }

  // Video surface is submitted again with damage, which will be the only thing
  // with damage. The video quad will have the overlay damage index referring to
  // this damage rect.
  {
    CompositorFrame video_surface_frame = MakeEmptyCompositorFrame();
    AddPasses(&video_surface_frame.render_pass_list, video_surface_passes,
              &video_surface_frame.metadata.referenced_surfaces);
    video_embedded_support->SubmitCompositorFrame(
        video_surface_id.local_surface_id(), std::move(video_surface_frame));
    auto aggregated_frame = AggregateFrame(root_surface_id_);
    auto* output_root_pass = aggregated_frame.render_pass_list.back().get();

    EXPECT_EQ(video_rect_in_root, output_root_pass->damage_rect);
    EXPECT_THAT(aggregated_frame.surface_damage_rect_list_,
                testing::ElementsAreArray({
                    video_rect_in_root,
                }));
    EXPECT_EQ(DamageListUnion(aggregated_frame.surface_damage_rect_list_),
              output_root_pass->damage_rect);

    EXPECT_EQ(2u, aggregated_frame.render_pass_list.size())
        << "Test assumes surface does not merge";
    EXPECT_THAT(aggregated_frame.render_pass_list[0]->quad_list,
                ElementsAre(IsTextureQuad()));
    EXPECT_THAT(aggregated_frame.render_pass_list[1]->quad_list,
                ElementsAre(IsAggregatedRenderPassQuad()));

    const SharedQuadState* video_sqs = aggregated_frame.render_pass_list[0]
                                           ->quad_list.back()
                                           ->shared_quad_state;

    // Surface damage can be assigned to the video quad.
    ASSERT_TRUE(video_sqs->overlay_damage_index.has_value());
    auto index = video_sqs->overlay_damage_index.value();
    EXPECT_EQ(0U, index);
    EXPECT_EQ(video_rect_in_root,
              aggregated_frame.surface_damage_rect_list_[index]);
  }
}

// Check GetRectDamage() handles per quad damage correctly.
TEST_F(SurfaceAggregatorValidSurfaceTest, NonRootRenderPassWithPerQuadDamage) {
  constexpr gfx::Rect root_damage_rect(70, 70, 10, 10);
  constexpr gfx::Rect quad_damage_rect(10, 10, 20, 20);
  constexpr gfx::Size child_pass_size(50, 50);

  gfx::Transform quad_transform;
  quad_transform.Scale(2.0, 2.0);
  quad_transform.Translate(10.0, 10.0);

  CompositorRenderPassList root_passes;
  root_passes.push_back(
      RenderPassBuilder(CompositorRenderPassId{1}, child_pass_size)
          .AddSolidColorQuad(gfx::Rect(child_pass_size), SkColors::kRed)
          .AddTextureQuad(gfx::Rect(20, 20), ResourceId(1))
          .SetQuadToTargetTransform(quad_transform)
          .SetQuadDamageRect(quad_damage_rect)
          .Build());
  root_passes.push_back(
      RenderPassBuilder(CompositorRenderPassId{2}, kSurfaceSize)
          .SetDamageRect(root_damage_rect)
          .AddSolidColorQuad(gfx::Rect(kSurfaceSize), SkColors::kRed)
          .AddRenderPassQuad(gfx::Rect(child_pass_size),
                             CompositorRenderPassId{1})
          .SetQuadToTargetTranslation(20, 20)
          .Build());
  {
    root_sink_->SubmitCompositorFrame(
        root_surface_id_.local_surface_id(),
        MakeCompositorFrame(CopyRenderPasses(root_passes)));
    auto aggregated_frame = AggregateFrame(root_surface_id_);

    // First aggregation always has full damage.
    ASSERT_EQ(aggregated_frame.render_pass_list.size(), 2u);
    EXPECT_EQ(aggregated_frame.render_pass_list[1]->damage_rect,
              gfx::Rect(kSurfaceSize));
  }

  {
    root_sink_->SubmitCompositorFrame(
        root_surface_id_.local_surface_id(),
        MakeCompositorFrame(CopyRenderPasses(root_passes)));
    auto aggregated_frame = AggregateFrame(root_surface_id_);

    // Second aggregation a new CompositorFrame was submitted. The final damage
    // is the quad damage (30, 30 20x20) unioned with surface damage (70,70
    // 10x10).
    ASSERT_EQ(aggregated_frame.render_pass_list.size(), 2u);
    EXPECT_EQ(aggregated_frame.render_pass_list[1]->damage_rect,
              gfx::Rect(30, 30, 50, 50));
  }

  {
    auto aggregated_frame = AggregateFrame(root_surface_id_);

    // Third aggregation the active CompositorFrame for the root surface hasn't
    // changed so both surface damage and per quad damage is empty.
    ASSERT_EQ(aggregated_frame.render_pass_list.size(), 2u);
    EXPECT_EQ(aggregated_frame.render_pass_list[1]->damage_rect, gfx::Rect());
  }
}

// Validates that while the display transform is applied to the aggregated frame
// and its damage, its not applied to the callback to the root frame sink.
TEST_F(SurfaceAggregatorValidSurfaceTest, DisplayTransformDamageCallback) {
  auto primary_child_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryFrameSinkId1, /*is_root=*/false);
  TestSurfaceIdAllocator primary_child_surface_id(
      primary_child_support->frame_sink_id());

  {
    auto pass = CompositorRenderPass::Create();
    pass->SetNew(CompositorRenderPassId{1}, gfx::Rect(0, 0, 20, 20),
                 gfx::Rect(), gfx::Transform());
    auto* sqs = pass->CreateAndAppendSharedQuadState();
    auto* solid_color_quad =
        pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();

    solid_color_quad->SetNew(sqs, gfx::Rect(0, 0, 20, 20),
                             gfx::Rect(0, 0, 20, 20), SkColors::kRed, false);

    CompositorFrame frame =
        CompositorFrameBuilder().AddRenderPass(std::move(pass)).Build();

    primary_child_support->SubmitCompositorFrame(
        primary_child_surface_id.local_surface_id(), std::move(frame));
  }

  constexpr gfx::Rect surface_quad_rect(10, 5);
  std::vector<Quad> root_quads = {
      Quad::SurfaceQuad(SurfaceRange(primary_child_surface_id),
                        SkColors::kWhite, surface_quad_rect,
                        /*stretch_content_to_fill_bounds=*/true)};

  constexpr gfx::Size surface_size(60, 100);
  std::vector<Pass> root_passes = {Pass(root_quads, surface_size)};

  MockAggregatedDamageCallback aggregated_damage_callback;
  root_sink_->SetAggregatedDamageCallbackForTesting(
      aggregated_damage_callback.GetCallback());

  SubmitCompositorFrame(root_sink_.get(), root_passes,
                        root_surface_id_.local_surface_id(), 0.5f);

  EXPECT_CALL(
      aggregated_damage_callback,
      OnAggregatedDamage(root_surface_id_.local_surface_id(), surface_size,
                         gfx::Rect(surface_size), next_display_time()));

  auto frame =
      aggregator_.Aggregate(root_surface_id_, GetNextDisplayTimeAndIncrement(),
                            gfx::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_90);
  gfx::Rect transformed_rect(surface_size.height(), surface_size.width());
  EXPECT_EQ(frame.render_pass_list.back()->output_rect, transformed_rect);
  EXPECT_EQ(frame.render_pass_list.back()->damage_rect, transformed_rect);
}

// Tests that a rounded_corner_bounds field on a quad in a child
// surface gets mapped up to the space of the parent surface, due to
// change of target render surface. (rounded corner bounds are in the space
// of the render surface).
TEST_F(SurfaceAggregatorValidSurfaceTest, RoundedCornerTransformChange) {
  auto middle_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryMiddleFrameSinkId, /*is_root=*/false);
  // Child surface.
  TestSurfaceIdAllocator child_surface_id(child_sink_->frame_sink_id());
  {
    std::vector<Quad> child_quads[1] = {
        {Quad::SolidColorQuad(SkColors::kGreen, gfx::Rect(5, 5))},
    };
    std::vector<Pass> child_passes = {
        Pass(child_quads[0], CompositorRenderPassId{1}, kSurfaceSize)};

    CompositorFrame child_frame = MakeEmptyCompositorFrame();
    AddPasses(&child_frame.render_pass_list, child_passes,
              &child_frame.metadata.referenced_surfaces);

    child_frame.render_pass_list[0]
        ->shared_quad_state_list.front()
        ->mask_filter_info = gfx::MaskFilterInfo(gfx::RRectF(0, 0, 100, 10, 5));

    child_sink_->SubmitCompositorFrame(child_surface_id.local_surface_id(),
                                       std::move(child_frame));
  }

  // Root surface.
  std::vector<Quad> surface_quads = {
      Quad::SurfaceQuad(SurfaceRange(std::nullopt, child_surface_id),
                        SkColors::kWhite, gfx::Rect(5, 5), false)};
  std::vector<Pass> root_passes = {Pass(surface_quads, kSurfaceSize)};

  CompositorFrame root_frame = MakeEmptyCompositorFrame();
  AddPasses(&root_frame.render_pass_list, root_passes,
            &root_frame.metadata.referenced_surfaces);

  root_frame.render_pass_list[0]
      ->shared_quad_state_list.front()
      ->quad_to_target_transform.Translate(0, 7);
  root_sink_->SubmitCompositorFrame(root_surface_id_.local_surface_id(),
                                    std::move(root_frame));

  auto aggregated_frame = AggregateFrame(root_surface_id_);
  auto* aggregated_first_pass_sqs =
      aggregated_frame.render_pass_list[0]->shared_quad_state_list.front();

  EXPECT_EQ(
      gfx::RRectF(0, 7, 100, 10, 5),
      aggregated_first_pass_sqs->mask_filter_info.rounded_corner_bounds());
}

// Tests that the rounded corner bounds of a surface quad that gets transformed
// when drawing into an ancestor surface get properly mapped to the new
// coordinate space of its final render surface. It also tests the specific case
// where the surface is embedded in a parent surface that itself can't be
// merged into the root surface (due to opacity).
TEST_F(SurfaceAggregatorValidSurfaceTest, RoundedCornerTransformedSurfaceQuad) {
  auto middle_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryMiddleFrameSinkId, /*is_root=*/false);
  // Grandchild surface.
  TestSurfaceIdAllocator grandchild_surface_id(child_sink_->frame_sink_id());
  {
    std::vector<Quad> child_quads[1] = {
        {Quad::SolidColorQuad(SkColors::kGreen, gfx::Rect(5, 5))},
    };
    std::vector<Pass> child_passes = {
        Pass(child_quads[0], CompositorRenderPassId{1}, kSurfaceSize)};

    CompositorFrame child_frame = MakeEmptyCompositorFrame();
    AddPasses(&child_frame.render_pass_list, child_passes,
              &child_frame.metadata.referenced_surfaces);

    child_sink_->SubmitCompositorFrame(grandchild_surface_id.local_surface_id(),
                                       std::move(child_frame));
  }

  // Child surface.
  TestSurfaceIdAllocator child_surface_id(child_sink_->frame_sink_id());
  {
    // Set an opacity in order to prevent merging into the root render pass.
    std::vector<Quad> child_quads = {Quad::SurfaceQuad(
        SurfaceRange(std::nullopt, grandchild_surface_id), SkColors::kWhite,
        gfx::Rect(5, 5), 0.5f, gfx::Transform(), false,
        gfx::MaskFilterInfo(gfx::RRectF(0, 0, 96, 10, 5)),
        /*is_fast_rounded_corner=*/false)};

    std::vector<Pass> child_passes = {
        Pass(child_quads, CompositorRenderPassId{1}, kSurfaceSize)};

    CompositorFrame child_frame = MakeEmptyCompositorFrame();
    AddPasses(&child_frame.render_pass_list, child_passes,
              &child_frame.metadata.referenced_surfaces);

    child_sink_->SubmitCompositorFrame(child_surface_id.local_surface_id(),
                                       std::move(child_frame));
  }

  // Root surface.
  gfx::Transform surface_transform;
  surface_transform.Translate(3, 4);
  std::vector<Quad> secondary_quads = {Quad::SurfaceQuad(
      SurfaceRange(std::nullopt, child_surface_id), SkColors::kWhite,
      gfx::Rect(5, 5), 1.f, surface_transform, false, gfx::MaskFilterInfo(),
      /*is_fast_rounded_corner=*/false)};

  std::vector<Pass> root_passes = {Pass(secondary_quads, kSurfaceSize)};

  CompositorFrame root_frame =
      CompositorFrameBuilder().SetDeviceScaleFactor(2.0f).Build();
  AddPasses(&root_frame.render_pass_list, root_passes,
            &root_frame.metadata.referenced_surfaces);

  root_frame.render_pass_list[0]
      ->shared_quad_state_list.front()
      ->quad_to_target_transform.Translate(0, 7);
  root_sink_->SubmitCompositorFrame(root_surface_id_.local_surface_id(),
                                    std::move(root_frame));

  auto aggregated_frame = AggregateFrame(root_surface_id_);
  auto* aggregated_first_pass_sqs =
      aggregated_frame.render_pass_list[1]->shared_quad_state_list.front();

  // Original rounded rect is (0, 0, 96, 10, 5). This then gets multiplied
  // by a device scale factor of 2 to (0, 0, 192, 20, 10), then moved
  // by a (3, 4) translation followed by a (0, 7) translation.
  EXPECT_EQ(
      gfx::RRectF(3, 11, 192, 20, 10),
      aggregated_first_pass_sqs->mask_filter_info.rounded_corner_bounds());
}

// This is a variant of RoundedCornerTransformedSurfaceQuad that does not
// have opacity, and therefore can be merged into the root render pass.
TEST_F(SurfaceAggregatorValidSurfaceTest,
       RoundedCornerTransformedMergedSurfaceQuad) {
  auto middle_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryMiddleFrameSinkId, /*is_root=*/false);
  // Grandchild surface.
  TestSurfaceIdAllocator grandchild_surface_id(child_sink_->frame_sink_id());
  {
    std::vector<Quad> child_quads[1] = {
        {Quad::SolidColorQuad(SkColors::kGreen, gfx::Rect(5, 5))},
    };
    std::vector<Pass> child_passes = {
        Pass(child_quads[0], CompositorRenderPassId{1}, kSurfaceSize)};

    CompositorFrame child_frame = MakeEmptyCompositorFrame();
    AddPasses(&child_frame.render_pass_list, child_passes,
              &child_frame.metadata.referenced_surfaces);

    child_sink_->SubmitCompositorFrame(grandchild_surface_id.local_surface_id(),
                                       std::move(child_frame));
  }

  // Child surface.
  TestSurfaceIdAllocator child_surface_id(child_sink_->frame_sink_id());
  {
    std::vector<Quad> child_quads = {Quad::SurfaceQuad(
        SurfaceRange(std::nullopt, grandchild_surface_id), SkColors::kWhite,
        gfx::Rect(5, 5), 1.f, gfx::Transform(), false,
        gfx::MaskFilterInfo(gfx::RRectF(0, 0, 96, 10, 5)),
        /*is_fast_rounded_corner=*/false)};

    std::vector<Pass> child_passes = {
        Pass(child_quads, CompositorRenderPassId{1}, kSurfaceSize)};

    CompositorFrame child_frame = MakeEmptyCompositorFrame();
    AddPasses(&child_frame.render_pass_list, child_passes,
              &child_frame.metadata.referenced_surfaces);

    child_sink_->SubmitCompositorFrame(child_surface_id.local_surface_id(),
                                       std::move(child_frame));
  }

  // Root surface.
  gfx::Transform surface_transform;
  surface_transform.Translate(3, 4);
  std::vector<Quad> secondary_quads = {Quad::SurfaceQuad(
      SurfaceRange(std::nullopt, child_surface_id), SkColors::kWhite,
      gfx::Rect(5, 5), 1.f, surface_transform, false, gfx::MaskFilterInfo(),
      /*is_fast_rounded_corner=*/false)};

  std::vector<Pass> root_passes = {Pass(secondary_quads, kSurfaceSize)};

  CompositorFrame root_frame =
      CompositorFrameBuilder().SetDeviceScaleFactor(2.0f).Build();
  AddPasses(&root_frame.render_pass_list, root_passes,
            &root_frame.metadata.referenced_surfaces);

  root_frame.render_pass_list[0]
      ->shared_quad_state_list.front()
      ->quad_to_target_transform.Translate(0, 7);
  root_sink_->SubmitCompositorFrame(root_surface_id_.local_surface_id(),
                                    std::move(root_frame));

  auto aggregated_frame = AggregateFrame(root_surface_id_);
  auto* aggregated_first_pass_sqs =
      aggregated_frame.render_pass_list[1]->shared_quad_state_list.front();

  // Original rounded rect is (0, 0, 96, 10, 5). This then gets multiplied
  // by a device scale factor of 2 to (0, 0, 192, 20, 10), then moved
  // by a (3, 4) translation followed by a (0, 7) translation.
  EXPECT_EQ(
      gfx::RRectF(3, 11, 192, 20, 10),
      aggregated_first_pass_sqs->mask_filter_info.rounded_corner_bounds());
}

TEST_F(SurfaceAggregatorValidSurfaceTest, TransformedRoundedSurfaceQuad) {
  auto middle_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryMiddleFrameSinkId, /*is_root=*/false);
  // Child surface.
  ParentLocalSurfaceIdAllocator child_allocator;
  child_allocator.GenerateId();

  // Child surface.
  TestSurfaceIdAllocator child_surface_id(child_sink_->frame_sink_id());
  {
    std::vector<Quad> child_quads[1] = {
        {Quad::SolidColorQuad(SkColors::kGreen, gfx::Rect(5, 5))},
    };
    std::vector<Pass> child_passes = {
        Pass(child_quads[0], CompositorRenderPassId{1}, kSurfaceSize)};

    CompositorFrame child_frame = MakeEmptyCompositorFrame();
    AddPasses(&child_frame.render_pass_list, child_passes,
              &child_frame.metadata.referenced_surfaces);

    child_sink_->SubmitCompositorFrame(child_surface_id.local_surface_id(),
                                       std::move(child_frame));
  }

  // Root surface.
  gfx::Transform surface_transform;
  surface_transform.Translate(3, 4);
  std::vector<Quad> secondary_quads = {Quad::SurfaceQuad(
      SurfaceRange(std::nullopt, child_surface_id), SkColors::kWhite,
      gfx::Rect(5, 5), 1.f, surface_transform, false,
      gfx::MaskFilterInfo(gfx::RRectF(0, 0, 96, 10, 5)),
      /*is_fast_rounded_corner=*/true)};

  std::vector<Pass> root_passes = {Pass(secondary_quads, kSurfaceSize)};

  CompositorFrame root_frame =
      CompositorFrameBuilder().SetDeviceScaleFactor(2.0f).Build();
  AddPasses(&root_frame.render_pass_list, root_passes,
            &root_frame.metadata.referenced_surfaces);

  root_frame.render_pass_list[0]
      ->shared_quad_state_list.front()
      ->quad_to_target_transform.Translate(0, 7);
  root_sink_->SubmitCompositorFrame(root_surface_id_.local_surface_id(),
                                    std::move(root_frame));

  auto aggregated_frame = AggregateFrame(root_surface_id_);
  // Only one aggregated quad will result, because the use of
  // is_fast_border_radius will result in the child surface being merged
  // into the parent.
  auto* aggregated_first_pass_sqs =
      aggregated_frame.render_pass_list[0]->shared_quad_state_list.front();

  // The rounded rect on the surface quad is already in the space of the root
  // surface, so the (3, 4) translation should not apply to it.
  EXPECT_EQ(
      gfx::RRectF(0, 0, 96, 10, 5),
      aggregated_first_pass_sqs->mask_filter_info.rounded_corner_bounds());
}

// Verifies that if a child surface is embedded twice in the root surface,
// SurfaceAggregator considers both occurrences in damage rect calculation.
TEST_P(SurfaceAggregatorValidSurfaceWithMergingPassesTest,
       AggregateDamageRectWithMultiplyEmbeddedSurface) {
  // Add a callback for when the surface is damaged.
  MockAggregatedDamageCallback aggregated_damage_callback;
  root_sink_->SetAggregatedDamageCallbackForTesting(
      aggregated_damage_callback.GetCallback());

  // The child surface consists of a single render pass containing a single
  // solid color draw quad.
  std::vector<Quad> child_quads = {
      Quad::SolidColorQuad(SkColors::kGreen, gfx::Rect(5, 5))};
  std::vector<Pass> child_passes = {Pass(child_quads, kSurfaceSize)};

  CompositorFrame child_frame = MakeEmptyCompositorFrame();
  AddPasses(&child_frame.render_pass_list, child_passes,
            &child_frame.metadata.referenced_surfaces);

  TestSurfaceIdAllocator child_surface_id(child_sink_->frame_sink_id());
  child_sink_->SubmitCompositorFrame(child_surface_id.local_surface_id(),
                                     std::move(child_frame));

  // The root surface consists of three render passes:
  //  1) The first one contains a surface draw quad referencing the child
  //     surface.
  //  2) The second one contains a render pass draw quad referencing the first
  //     render pass with a scale transform applied.
  //  3) The third one contains two render pass draw quads, one referencing the
  //     second render pass with a translation transform applied, the other
  //     referencing the first render pass with no transform.
  gfx::Transform scale;
  scale.Scale(2.f, 2.f);

  CompositorFrame root_frame =
      CompositorFrameBuilder()
          .AddRenderPass(
              RenderPassBuilder(CompositorRenderPassId{1}, kSurfaceSize)
                  .AddSurfaceQuad(gfx::Rect(kSurfaceSize),
                                  SurfaceRange(std::nullopt, child_surface_id),
                                  {.allow_merge = AllowMerge()}))
          .AddRenderPass(
              RenderPassBuilder(CompositorRenderPassId{2}, kSurfaceSize)
                  .AddRenderPassQuad(gfx::Rect(kSurfaceSize),
                                     CompositorRenderPassId{1})
                  .SetQuadToTargetTransform(scale))
          .AddRenderPass(
              RenderPassBuilder(CompositorRenderPassId{3}, kSurfaceSize)
                  .AddRenderPassQuad(gfx::Rect(kSurfaceSize),
                                     CompositorRenderPassId{2})
                  .SetQuadToTargetTranslation(30, 50)
                  .AddRenderPassQuad(gfx::Rect(kSurfaceSize),
                                     CompositorRenderPassId{1}))
          .Build();

  root_sink_->SubmitCompositorFrame(root_surface_id_.local_surface_id(),
                                    std::move(root_frame));

  // Damage rect for the first aggregation would contain entire root surface
  // which is union of (0,0 100x100) and (30,50 200x200); i.e. (0,0 230x250)
  // which is clipped to the root render pass output rect (0,0 100x100).
  EXPECT_CALL(
      aggregated_damage_callback,
      OnAggregatedDamage(root_surface_id_.local_surface_id(), kSurfaceSize,
                         gfx::Rect(kSurfaceSize), next_display_time()));
  auto aggregated_frame = AggregateFrame(root_surface_id_);
  testing::Mock::VerifyAndClearExpectations(&aggregated_damage_callback);

  // For the second aggregation we only damage the child surface at
  // (10,10 10x10). The aggregated damage rect should reflect that.
  CompositorFrame child_frame_2 = MakeEmptyCompositorFrame();
  AddPasses(&child_frame_2.render_pass_list, child_passes,
            &child_frame_2.metadata.referenced_surfaces);

  child_frame_2.render_pass_list.back()->damage_rect =
      gfx::Rect(10, 10, 10, 10);

  child_sink_->SubmitCompositorFrame(child_surface_id.local_surface_id(),
                                     std::move(child_frame_2));

  // The child surface is embedded twice in the root surface, so its damage rect
  // would appear in two locations in the root surface:
  //  1) The first embedding has no transform, so its damage rect would simply
  //     be (10,10 10x10).
  //  2) The second embedding is scaled by a factor of 2 and translated by
  //     (30,50). So, its damage rect would be (10*2+30,10*2+50 10*2x10*2) =
  //     (50,70 20x20).
  // The aggregated damage rect would be union of the above damage rects which
  // is (10,10 60x80).
  gfx::Rect expected_damage_rect(10, 10, 60, 80);
  EXPECT_CALL(
      aggregated_damage_callback,
      OnAggregatedDamage(root_surface_id_.local_surface_id(), kSurfaceSize,
                         expected_damage_rect, next_display_time()));
  auto aggregated_frame_2 = AggregateFrame(root_surface_id_);
  testing::Mock::VerifyAndClearExpectations(&aggregated_damage_callback);
}

// Verifies that if a CompositorFrame contains a render pass id cycle then the
// frame is rejected as invalid.
TEST_F(SurfaceAggregatorValidSurfaceTest,
       AggregateDamageRectWithRenderPassCycle) {
  // Add a callback for when the surface is damaged.
  MockAggregatedDamageCallback aggregated_damage_callback;
  root_sink_->SetAggregatedDamageCallbackForTesting(
      aggregated_damage_callback.GetCallback());

  // The root surface consists of two render passes:
  //  1) The first render pass contains a solid color draw quad and a render
  //     pass draw quad referencing the second render pass.
  //  2) The second render pass contains a render pass draw quad that is
  //     referencing the first render pass, creating a cycle.
  CompositorRenderPassId root_pass_ids[] = {CompositorRenderPassId{1},
                                            CompositorRenderPassId{2}};
  std::vector<Quad> root_quads_1 = {
      Quad::SolidColorQuad(SkColors::kGreen, gfx::Rect(5, 5)),
      Quad::RenderPassQuad(root_pass_ids[1], gfx::Transform(), true)};
  std::vector<Quad> root_quads_2 = {
      Quad::RenderPassQuad(root_pass_ids[0], gfx::Transform(), true)};
  std::vector<Pass> root_passes = {
      Pass(root_quads_2, root_pass_ids[1], kSurfaceSize),
      Pass(root_quads_1, root_pass_ids[0], kSurfaceSize)};

  CompositorFrame root_frame = MakeEmptyCompositorFrame();
  AddPasses(&root_frame.render_pass_list, root_passes,
            &root_frame.metadata.referenced_surfaces);

  root_sink_->SubmitCompositorFrame(root_surface_id_.local_surface_id(),
                                    std::move(root_frame));

  // Verify the CompositorFrame was rejected and there is no damage.
  EXPECT_CALL(aggregated_damage_callback, OnAggregatedDamage(_, _, _, _))
      .Times(0);
  auto aggregated_frame = AggregateFrame(root_surface_id_);
  EXPECT_TRUE(aggregated_frame.render_pass_list.empty());
}

// Verify that a SurfaceDrawQuad with !|allow_merge| won't be merged into
// the parent renderpass.
TEST_F(SurfaceAggregatorValidSurfaceTest, AllowMerge) {
  // Child surface.
  gfx::Rect child_rect(5, 5);
  TestSurfaceIdAllocator child_surface_id(child_sink_->frame_sink_id());
  {
    std::vector<Quad> child_quads = {
        Quad::SolidColorQuad(SkColors::kGreen, child_rect)};
    std::vector<Pass> child_passes = {
        Pass(child_quads, CompositorRenderPassId{1}, kSurfaceSize)};

    CompositorFrame child_frame = MakeEmptyCompositorFrame();
    AddPasses(&child_frame.render_pass_list, child_passes,
              &child_frame.metadata.referenced_surfaces);

    child_sink_->SubmitCompositorFrame(child_surface_id.local_surface_id(),
                                       std::move(child_frame));
  }

  gfx::Rect root_rect(kSurfaceSize);

  // Submit a SurfaceDrawQuad that allows merging.
  {
    auto pass = CompositorRenderPass::Create();
    pass->SetNew(CompositorRenderPassId{1}, root_rect, root_rect,
                 gfx::Transform());
    auto* sqs = pass->CreateAndAppendSharedQuadState();
    sqs->opacity = 1.f;

    auto* surface_quad = pass->CreateAndAppendDrawQuad<SurfaceDrawQuad>();
    surface_quad->SetAll(sqs, child_rect, child_rect,
                         /*needs_blending=*/false,
                         SurfaceRange(std::nullopt, child_surface_id),
                         SkColors::kWhite,
                         /*stretch_content_to_fill_bounds=*/false,
                         /*is_reflection=*/false,
                         /*allow_merge=*/true);

    CompositorFrame frame =
        CompositorFrameBuilder().AddRenderPass(std::move(pass)).Build();
    root_sink_->SubmitCompositorFrame(root_surface_id_.local_surface_id(),
                                      std::move(frame));

    auto aggregated_frame = AggregateFrame(root_surface_id_);
    // Merging allowed, so 1 pass should be present.
    EXPECT_EQ(1u, aggregated_frame.render_pass_list.size());
  }

  // Submit a SurfaceDrawQuad that does not allow merging
  {
    auto pass = CompositorRenderPass::Create();
    pass->SetNew(CompositorRenderPassId{1}, root_rect, root_rect,
                 gfx::Transform());
    auto* sqs = pass->CreateAndAppendSharedQuadState();
    sqs->opacity = 1.f;

    auto* surface_quad = pass->CreateAndAppendDrawQuad<SurfaceDrawQuad>();
    surface_quad->SetAll(sqs, child_rect, child_rect,
                         /*needs_blending=*/false,
                         SurfaceRange(std::nullopt, child_surface_id),
                         SkColors::kWhite,
                         /*stretch_content_to_fill_bounds=*/false,
                         /*is_reflection=*/false,
                         /*allow_merge=*/false);

    CompositorFrame frame =
        CompositorFrameBuilder().AddRenderPass(std::move(pass)).Build();
    root_sink_->SubmitCompositorFrame(root_surface_id_.local_surface_id(),
                                      std::move(frame));

    auto aggregated_frame = AggregateFrame(root_surface_id_);
    // Merging not allowed, so 2 passes should be present.
    EXPECT_EQ(2u, aggregated_frame.render_pass_list.size());
  }
}

// Check that if a non-merged surface is invisible, its entire render pass is
// skipped.
TEST_F(SurfaceAggregatorValidSurfaceTest, SkipInvisibleSurface) {
  // Child surface.
  gfx::Rect child_rect(5, 5);
  TestSurfaceIdAllocator child_surface_id(child_sink_->frame_sink_id());
  {
    std::vector<Quad> child_quads = {
        Quad::SolidColorQuad(SkColors::kGreen, child_rect)};
    // Offset child output rect so it's outside the root visible rect.
    gfx::Rect output_rect(kSurfaceSize);
    output_rect.Offset(output_rect.width(), output_rect.height());
    std::vector<Pass> child_passes = {
        Pass(child_quads, CompositorRenderPassId{1}, output_rect)};

    CompositorFrame child_frame = MakeEmptyCompositorFrame();
    AddPasses(&child_frame.render_pass_list, child_passes,
              &child_frame.metadata.referenced_surfaces);

    child_sink_->SubmitCompositorFrame(child_surface_id.local_surface_id(),
                                       std::move(child_frame));
  }

  gfx::Rect root_rect(kSurfaceSize);

  auto pass = CompositorRenderPass::Create();
  pass->SetNew(CompositorRenderPassId{1}, root_rect, root_rect,
               gfx::Transform());
  auto* sqs = pass->CreateAndAppendSharedQuadState();
  sqs->opacity = 1.f;

  // Disallow merge.
  auto* surface_quad = pass->CreateAndAppendDrawQuad<SurfaceDrawQuad>();
  surface_quad->SetAll(sqs, child_rect, child_rect,
                       /*needs_blending=*/false,
                       SurfaceRange(std::nullopt, child_surface_id),
                       SkColors::kWhite,
                       /*stretch_content_to_fill_bounds=*/false,
                       /*is_reflection=*/false,
                       /*allow_merge=*/false);

  CompositorFrame frame =
      CompositorFrameBuilder().AddRenderPass(std::move(pass)).Build();
  root_sink_->SubmitCompositorFrame(root_surface_id_.local_surface_id(),
                                    std::move(frame));

  auto aggregated_frame = AggregateFrame(root_surface_id_);
  // Merging not allowed, but child rect should be dropped.
  EXPECT_EQ(1u, aggregated_frame.render_pass_list.size());
}

// Verify that a SurfaceDrawQuad's root RenderPass has correct texture
// parameters if being drawn via RPDQ.
TEST_F(SurfaceAggregatorValidSurfaceTest, RenderPassDoesNotFillSurface) {
  // Child surface.
  gfx::Rect child_rect(5, 4, 5, 5);
  TestSurfaceIdAllocator child_surface_id(child_sink_->frame_sink_id());
  {
    std::vector<Quad> child_quads = {
        Quad::SolidColorQuad(SkColors::kGreen, child_rect)};
    std::vector<Pass> child_passes = {
        Pass(child_quads, CompositorRenderPassId{1}, child_rect)};

    CompositorFrame child_frame = MakeEmptyCompositorFrame();
    AddPasses(&child_frame.render_pass_list, child_passes,
              &child_frame.metadata.referenced_surfaces);

    child_sink_->SubmitCompositorFrame(child_surface_id.local_surface_id(),
                                       std::move(child_frame));
  }

  gfx::Rect root_rect(kSurfaceSize);
  gfx::Rect surface_size(10, 10);

  // Submit a SurfaceDrawQuad that does not allow merging.
  {
    auto pass = CompositorRenderPass::Create();
    pass->SetNew(CompositorRenderPassId{1}, root_rect, root_rect,
                 gfx::Transform());
    auto* sqs = pass->CreateAndAppendSharedQuadState();
    sqs->opacity = 1.f;

    auto* surface_quad = pass->CreateAndAppendDrawQuad<SurfaceDrawQuad>();
    surface_quad->SetAll(sqs, surface_size, surface_size,
                         /*needs_blending=*/false,
                         SurfaceRange(std::nullopt, child_surface_id),
                         SkColors::kWhite,
                         /*stretch_content_to_fill_bounds=*/false,
                         /*is_reflection=*/false,
                         /*allow_merge=*/false);

    CompositorFrame frame =
        CompositorFrameBuilder().AddRenderPass(std::move(pass)).Build();
    root_sink_->SubmitCompositorFrame(root_surface_id_.local_surface_id(),
                                      std::move(frame));

    auto aggregated_frame = AggregateFrame(root_surface_id_);

    // Merging not allowed, so 2 passes should be present.
    ASSERT_EQ(2u, aggregated_frame.render_pass_list.size());

    // The base pass should contain a single RPDQ with a |rect| matching
    // |child_rect|.
    ASSERT_EQ(1u, aggregated_frame.render_pass_list[1]->quad_list.size());
    const auto* rpdq = AggregatedRenderPassDrawQuad::MaterialCast(
        aggregated_frame.render_pass_list[1]->quad_list.front());
    EXPECT_EQ(child_rect, rpdq->rect);

    // Additionally, the visible rect should have been clipped.
    EXPECT_EQ(child_rect, rpdq->visible_rect);
  }
}

// Tests that damage rects are aggregated correctly when surfaces change.
TEST_P(SurfaceAggregatorValidSurfaceWithMergingPassesTest,
       AggregateDamageRectWithBackdropFilter) {
  // Add callbacks for when the surfaces are damaged.
  MockAggregatedDamageCallback aggregated_damage_callback;
  root_sink_->SetAggregatedDamageCallbackForTesting(
      aggregated_damage_callback.GetCallback());
  child_sink_->SetAggregatedDamageCallbackForTesting(
      aggregated_damage_callback.GetCallback());

  // A child surface which will be embedded into a surface quad under all the
  // RPDQs with backdrop filters
  gfx::Size child_surface_size(100, 15);
  std::vector<Quad> child_quads = {
      Quad::SolidColorQuad(SkColors::kGreen, gfx::Rect(5, 5))};

  std::vector<Pass> child_passes = {
      Pass(child_quads, CompositorRenderPassId{1}, child_surface_size)};

  CompositorFrame child_frame = MakeEmptyCompositorFrame();
  AddPasses(&child_frame.render_pass_list, child_passes,
            &child_frame.metadata.referenced_surfaces);

  TestSurfaceIdAllocator child_surface_id(child_sink_->frame_sink_id());
  child_sink_->SubmitCompositorFrame(child_surface_id.local_surface_id(),
                                     std::move(child_frame));

  {
    CompositorRenderPassList root_pass_list;
    // The root surface has five passes:
    // - Four 30x30 render passes that each contain one solid color quad and
    //   a pixel-moving backdrop filter.
    // - The root pass embeds each of the first four render passes and then
    //   underneath them embeds the child surface.
    const gfx::Rect render_pass_rect(30, 30);
    for (int i = 1; i < 5; ++i) {
      root_pass_list.push_back(
          RenderPassBuilder(CompositorRenderPassId{i}, render_pass_rect)
              .AddSolidColorQuad(render_pass_rect, SkColors::kGreen)
              .AddBackdropFilter(cc::FilterOperation::CreateBlurFilter(5))
              .Build());
    }
    root_pass_list.push_back(
        RenderPassBuilder(CompositorRenderPassId{5}, gfx::Rect(kSurfaceSize))
            .AddRenderPassQuad(render_pass_rect, CompositorRenderPassId{1})
            .SetQuadToTargetTranslation(70, 0)
            .AddRenderPassQuad(render_pass_rect, CompositorRenderPassId{2})
            .SetQuadToTargetTranslation(30, 30)
            .AddRenderPassQuad(render_pass_rect, CompositorRenderPassId{3})
            .SetQuadToTargetTranslation(10, 50)
            .AddRenderPassQuad(render_pass_rect, CompositorRenderPassId{4})
            .SetQuadToTargetTranslation(70, 70)
            .AddSurfaceQuad(gfx::Rect(100, 100), SurfaceRange(child_surface_id),
                            {.allow_merge = AllowMerge()})
            .SetQuadToTargetTranslation(0, 85)
            .Build());

    root_sink_->SubmitCompositorFrame(
        root_surface_id_.local_surface_id(),
        MakeCompositorFrame(std::move(root_pass_list)));
  }

  // Damage rect for first aggregation should contain entire root surface.
  size_t expected_num_passes_after_aggregation = AllowMerge() ? 5u : 6u;
  EXPECT_CALL(aggregated_damage_callback,
              OnAggregatedDamage(
                  child_surface_id.local_surface_id(), child_surface_size,
                  gfx::Rect(child_surface_size), next_display_time()));
  EXPECT_CALL(
      aggregated_damage_callback,
      OnAggregatedDamage(root_surface_id_.local_surface_id(), kSurfaceSize,
                         gfx::Rect(0, 0, 100, 100), next_display_time()));
  auto aggregated_frame = AggregateFrame(root_surface_id_);
  testing::Mock::VerifyAndClearExpectations(&aggregated_damage_callback);
  const auto& aggregated_pass_list = aggregated_frame.render_pass_list;
  EXPECT_EQ(expected_num_passes_after_aggregation, aggregated_pass_list.size());
  EXPECT_EQ(gfx::Rect(kSurfaceSize), aggregated_pass_list.back()->damage_rect);

  //   _____________________
  //  |               |     |
  //  |               |     |
  //  |       ____    |_____|
  //  |      |    |         |
  //  |   ___|    |         |
  //  |  |   ||___|         |
  //  |  |    |        _____|
  //  |  |____|       |     |
  //  |               |     |
  //  |_______________|_____|
  //

  child_frame = MakeEmptyCompositorFrame();
  AddPasses(&child_frame.render_pass_list, child_passes,
            &child_frame.metadata.referenced_surfaces);

  child_sink_->SubmitCompositorFrame(child_surface_id.local_surface_id(),
                                     std::move(child_frame));

  // The damage from the surface quad (0,85 100x15) is below all the four quads
  // with backdrop filters.
  // The expected damage rect should include all the other child render pass
  // output surface that would need to be updated. In this case, that would
  // be the bottom 3 render pass from the image.
  const gfx::Rect expected_damage_rect(0, 30, 100, 70);

  EXPECT_CALL(aggregated_damage_callback,
              OnAggregatedDamage(
                  child_surface_id.local_surface_id(), child_surface_size,
                  gfx::Rect(child_surface_size), next_display_time()));
  EXPECT_CALL(
      aggregated_damage_callback,
      OnAggregatedDamage(root_surface_id_.local_surface_id(), kSurfaceSize,
                         expected_damage_rect, next_display_time()));
  aggregated_frame = AggregateFrame(root_surface_id_);
  testing::Mock::VerifyAndClearExpectations(&aggregated_damage_callback);

  const auto& aggregated_pass_list2 = aggregated_frame.render_pass_list;
  EXPECT_EQ(expected_num_passes_after_aggregation,
            aggregated_pass_list2.size());
  EXPECT_EQ(expected_damage_rect, aggregated_pass_list2.back()->damage_rect);
}

TEST_F(SurfaceAggregatorValidSurfaceTest,
       ContainedFrameSinkChangeInvalidatesHitTestData) {
  auto embedded_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryFrameSinkId1, /*is_root=*/true);
  TestSurfaceIdAllocator embedded_surface_id(embedded_support->frame_sink_id());

  // First submit a root frame which doesn't reference the embedded frame
  // and aggregate.
  {
    std::vector<Quad> embedded_quads = {
        Quad::SolidColorQuad(SkColors::kGreen, gfx::Rect(5, 5)),
        Quad::SolidColorQuad(SkColors::kGray, gfx::Rect(5, 5))};
    std::vector<Pass> embedded_passes = {Pass(embedded_quads, kSurfaceSize)};
    SubmitCompositorFrame(embedded_support.get(), embedded_passes,
                          embedded_surface_id.local_surface_id(), 1.0f);

    std::vector<Quad> root_quads = {
        Quad::SolidColorQuad(SkColors::kRed, gfx::Rect(5, 5)),
        Quad::SolidColorQuad(SkColors::kBlue, gfx::Rect(5, 5))};
    std::vector<Pass> root_passes = {Pass(root_quads, kSurfaceSize)};
    SubmitCompositorFrame(root_sink_.get(), root_passes,
                          root_surface_id_.local_surface_id(), 1.0f);
    AggregateFrame(root_surface_id_);
  }

  const HitTestManager* hit_test_manager = manager_.hit_test_manager();
  uint64_t hit_test_region_index =
      hit_test_manager->submit_hit_test_region_list_index();

  // Now submit a root frame that *does* reference the embedded frame, and
  // aggregate.
  {
    std::vector<Quad> root_quads = {
        Quad::SurfaceQuad(SurfaceRange(std::nullopt, embedded_surface_id),
                          SkColors::kWhite, gfx::Rect(5, 5), false),
        Quad::SolidColorQuad(SkColors::kRed, gfx::Rect(5, 5)),
        Quad::SolidColorQuad(SkColors::kBlue, gfx::Rect(5, 5))};
    std::vector<Pass> root_passes = {Pass(root_quads, kSurfaceSize)};

    SubmitCompositorFrame(root_sink_.get(), root_passes,
                          root_surface_id_.local_surface_id(), 1.0);
    AggregateFrame(root_surface_id_);
  }

  // Check that the HitTestManager was marked as needing to re-aggregate hit
  // test data.
  EXPECT_GT(hit_test_manager->submit_hit_test_region_list_index(),
            hit_test_region_index);
}

void ExpectDelegatedInkMetadataIsEqual(const gfx::DelegatedInkMetadata& lhs,
                                       const gfx::DelegatedInkMetadata& rhs) {
  EXPECT_FLOAT_EQ(lhs.point().y(), rhs.point().y());
  EXPECT_FLOAT_EQ(lhs.point().x(), rhs.point().x());
  EXPECT_EQ(lhs.diameter(), rhs.diameter());
  EXPECT_EQ(lhs.color(), rhs.color());
  EXPECT_EQ(lhs.timestamp(), rhs.timestamp());
  EXPECT_FLOAT_EQ(lhs.presentation_area().y(), rhs.presentation_area().y());
  EXPECT_FLOAT_EQ(lhs.presentation_area().x(), rhs.presentation_area().x());
  EXPECT_FLOAT_EQ(lhs.presentation_area().width(),
                  rhs.presentation_area().width());
  EXPECT_FLOAT_EQ(lhs.presentation_area().height(),
                  rhs.presentation_area().height());
  EXPECT_EQ(lhs.frame_time(), rhs.frame_time());
  EXPECT_EQ(lhs.is_hovering(), rhs.is_hovering());
}

// Basic test to confirm that ink metadata on a child surface will be
// transformed by the parent and only used once.
TEST_F(SurfaceAggregatorValidSurfaceTest, DelegatedInkMetadataTest) {
  std::vector<Quad> child_quads = {
      Quad::SolidColorQuad(SkColors::kGreen, gfx::Rect(5, 5))};
  std::vector<Pass> child_passes = {
      Pass(child_quads, CompositorRenderPassId{1}, gfx::Size(100, 100))};

  CompositorFrame child_frame = MakeEmptyCompositorFrame();
  gfx::DelegatedInkMetadata metadata(
      gfx::PointF(100, 100), 1.5, SK_ColorRED, base::TimeTicks::Now(),
      gfx::RectF(10, 10, 200, 200), base::TimeTicks::Now(), /*hovering*/ true,
      /*render_pass_id=*/0);
  child_frame.metadata.delegated_ink_metadata =
      std::make_unique<gfx::DelegatedInkMetadata>(metadata);
  AddPasses(&child_frame.render_pass_list, child_passes,
            &child_frame.metadata.referenced_surfaces);

  TestSurfaceIdAllocator child_surface_id(child_sink_->frame_sink_id());
  child_sink_->SubmitCompositorFrame(child_surface_id.local_surface_id(),
                                     std::move(child_frame));

  std::vector<Quad> root_quads = {Quad::SurfaceQuad(
      SurfaceRange(std::nullopt, child_surface_id), SkColors::kWhite,
      gfx::Rect(5, 5), /*stretch_content_to_fill_bounds=*/false)};

  std::vector<Pass> root_passes = {
      Pass(root_quads, CompositorRenderPassId{1}, gfx::Size(30, 30))};

  CompositorFrame root_frame = MakeEmptyCompositorFrame();
  AddPasses(&root_frame.render_pass_list, root_passes,
            &root_frame.metadata.referenced_surfaces);

  root_frame.render_pass_list[0]
      ->shared_quad_state_list.ElementAt(0)
      ->quad_to_target_transform.Scale(1.5, 1.5);
  root_frame.render_pass_list[0]
      ->shared_quad_state_list.ElementAt(0)
      ->quad_to_target_transform.Translate(70, 240);

  // Update the expected metadata to reflect the transforms to point and area
  // that are expected to occur.
  gfx::PointF pt = root_frame.render_pass_list[0]
                       ->shared_quad_state_list.ElementAt(0)
                       ->quad_to_target_transform.MapPoint(metadata.point());
  gfx::RectF area =
      root_frame.render_pass_list[0]
          ->shared_quad_state_list.ElementAt(0)
          ->quad_to_target_transform.MapRect(metadata.presentation_area());
  metadata = gfx::DelegatedInkMetadata(
      pt, metadata.diameter(), metadata.color(), metadata.timestamp(), area,
      metadata.frame_time(), metadata.is_hovering(), /*render_pass_id=*/0);

  root_sink_->SubmitCompositorFrame(root_surface_id_.local_surface_id(),
                                    std::move(root_frame));

  auto aggregated_frame = AggregateFrame(root_surface_id_);

  std::unique_ptr<gfx::DelegatedInkMetadata> actual_metadata =
      std::move(aggregated_frame.delegated_ink_metadata);
  EXPECT_TRUE(actual_metadata);
  ExpectDelegatedInkMetadataIsEqual(*actual_metadata.get(), metadata);

  // Send a compositor frame with no delegated ink metadata.
  CompositorFrame blank_frame = MakeEmptyCompositorFrame();
  AddPasses(&blank_frame.render_pass_list, child_passes,
            &blank_frame.metadata.referenced_surfaces);
  child_sink_->SubmitCompositorFrame(child_surface_id.local_surface_id(),
                                     std::move(blank_frame));

  // Then confirm that the |delegated_ink_metadata| was reset and a new
  // aggregated frame does not contain any delegated ink metadata.
  auto new_aggregated_frame = AggregateFrame(root_surface_id_);
  EXPECT_FALSE(new_aggregated_frame.delegated_ink_metadata);
}

// Tests that consecutive aggregated frames will result in the duplicate
// delegated ink metadata being transferred to the aggregate frame until
// the `kMaxFramesWithIdenticalInkMetadata` frame limit is reached.
TEST_F(SurfaceAggregatorValidSurfaceTest, RepeatedDelegatedInkMetadataTest) {
  std::vector<Quad> child_quads = {
      Quad::SolidColorQuad(SkColors::kGreen, gfx::Rect(5, 5))};
  std::vector<Pass> child_passes = {
      Pass(child_quads, CompositorRenderPassId{1}, gfx::Size(100, 100))};

  CompositorFrame child_frame = MakeEmptyCompositorFrame();
  gfx::DelegatedInkMetadata metadata(
      gfx::PointF(100, 100), /*diameter=*/1.5, SK_ColorRED,
      base::TimeTicks::Now(), gfx::RectF(10, 10, 200, 200),
      base::TimeTicks::Now(), /*hovering=*/true, /*render_pass_id=*/1);
  child_frame.metadata.delegated_ink_metadata =
      std::make_unique<gfx::DelegatedInkMetadata>(metadata);
  AddPasses(&child_frame.render_pass_list, child_passes,
            &child_frame.metadata.referenced_surfaces);

  TestSurfaceIdAllocator child_surface_id(child_sink_->frame_sink_id());
  child_sink_->SubmitCompositorFrame(child_surface_id.local_surface_id(),
                                     std::move(child_frame));

  std::vector<Quad> root_quads = {Quad::SurfaceQuad(
      SurfaceRange(std::nullopt, child_surface_id), SkColors::kWhite,
      gfx::Rect(5, 5), /*stretch_content_to_fill_bounds=*/false)};

  std::vector<Pass> root_passes = {
      Pass(root_quads, CompositorRenderPassId{1}, gfx::Size(30, 30))};

  CompositorFrame root_frame = MakeEmptyCompositorFrame();
  AddPasses(&root_frame.render_pass_list, root_passes,
            &root_frame.metadata.referenced_surfaces);

  root_frame.render_pass_list[0]
      ->shared_quad_state_list.ElementAt(0)
      ->quad_to_target_transform.Scale(1.5, 1.5);
  root_frame.render_pass_list[0]
      ->shared_quad_state_list.ElementAt(0)
      ->quad_to_target_transform.Translate(70, 240);

  // Update the expected metadata to reflect the transforms to point and area
  // that are expected to occur.
  gfx::PointF pt = root_frame.render_pass_list[0]
                       ->shared_quad_state_list.ElementAt(0)
                       ->quad_to_target_transform.MapPoint(metadata.point());
  gfx::RectF area =
      root_frame.render_pass_list[0]
          ->shared_quad_state_list.ElementAt(0)
          ->quad_to_target_transform.MapRect(metadata.presentation_area());
  metadata = gfx::DelegatedInkMetadata(
      pt, metadata.diameter(), metadata.color(), metadata.timestamp(), area,
      metadata.frame_time(), metadata.is_hovering(), /*render_pass_id=*/1);

  root_sink_->SubmitCompositorFrame(root_surface_id_.local_surface_id(),
                                    std::move(root_frame));

  // In the scenario where a compositor frame misses deadline or is skipped,
  // ensure that the delegated ink metadata still gets put on to the aggregated
  // frame until the duplicate metadata count of 3 is reached. See
  // `kMaxFramesWithIdenticalInkMetadata`.
  for (int frame_count = 1; frame_count <= 3; frame_count++) {
    auto aggregated_frame = AggregateFrame(root_surface_id_);

    std::unique_ptr<gfx::DelegatedInkMetadata> actual_metadata =
        std::move(aggregated_frame.delegated_ink_metadata);
    EXPECT_TRUE(actual_metadata);
    EXPECT_EQ(*actual_metadata.get(), metadata);
  }

  // Ensure that the subsequent aggregated frame with no immediately prior
  // compositor frame does not have a delegated ink metadata.
  {
    auto aggregated_frame = AggregateFrame(root_surface_id_);

    std::unique_ptr<gfx::DelegatedInkMetadata> actual_metadata =
        std::move(aggregated_frame.delegated_ink_metadata);
    EXPECT_FALSE(actual_metadata);
  }
}

// Confirm that transforms are aggregated as the tree is walked and correctly
// applied to the ink metadata.
TEST_F(SurfaceAggregatorValidSurfaceTest,
       TransformDelegatedInkMetadataTallTree) {
  auto greatgrand_child_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryFrameSinkId2, /*is_root=*/false);
  std::vector<Quad> greatgrandchild_quads = {
      Quad::SolidColorQuad(SkColors::kGreen, gfx::Rect(5, 5))};
  std::vector<Pass> greatgrandchild_passes = {Pass(
      greatgrandchild_quads, CompositorRenderPassId{1}, gfx::Size(100, 100))};

  gfx::DelegatedInkMetadata metadata(
      gfx::PointF(100, 100), 1.5, SK_ColorRED, base::TimeTicks::Now(),
      gfx::RectF(10, 10, 200, 200), base::TimeTicks::Now(), /*hovering*/ false,
      /*render_pass_id=*/0);
  CompositorFrame greatgrandchild_frame = MakeEmptyCompositorFrame();
  greatgrandchild_frame.metadata.delegated_ink_metadata =
      std::make_unique<gfx::DelegatedInkMetadata>(metadata);
  AddPasses(&greatgrandchild_frame.render_pass_list, greatgrandchild_passes,
            &greatgrandchild_frame.metadata.referenced_surfaces);

  TestSurfaceIdAllocator great_grandchild_surface_id(
      greatgrand_child_support->frame_sink_id());
  greatgrand_child_support->SubmitCompositorFrame(
      great_grandchild_surface_id.local_surface_id(),
      std::move(greatgrandchild_frame));

  auto grand_child_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryMiddleFrameSinkId, /*is_root=*/false);
  std::vector<Quad> grandchild_quads = {
      Quad::SurfaceQuad(SurfaceRange(std::nullopt, great_grandchild_surface_id),
                        SkColors::kWhite, gfx::Rect(7, 7),
                        /*stretch_content_to_fill_bounds=*/false)};
  std::vector<Pass> grandchild_passes = {
      Pass(grandchild_quads, CompositorRenderPassId{1}, gfx::Size(100, 100))};

  CompositorFrame grandchild_frame = MakeEmptyCompositorFrame();

  AddPasses(&grandchild_frame.render_pass_list, grandchild_passes,
            &grandchild_frame.metadata.referenced_surfaces);

  grandchild_frame.render_pass_list[0]
      ->shared_quad_state_list.ElementAt(0)
      ->quad_to_target_transform.Scale(1.5, 1.5);
  grandchild_frame.render_pass_list[0]
      ->shared_quad_state_list.ElementAt(0)
      ->quad_to_target_transform.Translate(37, 82);

  // Update the expected metadata to reflect the transforms to point and area
  // that are expected to occur.
  gfx::PointF pt = grandchild_frame.render_pass_list[0]
                       ->shared_quad_state_list.ElementAt(0)
                       ->quad_to_target_transform.MapPoint(metadata.point());
  gfx::RectF area =
      grandchild_frame.render_pass_list[0]
          ->shared_quad_state_list.ElementAt(0)
          ->quad_to_target_transform.MapRect(metadata.presentation_area());

  TestSurfaceIdAllocator grandchild_surface_id(
      grand_child_support->frame_sink_id());
  grand_child_support->SubmitCompositorFrame(
      grandchild_surface_id.local_surface_id(), std::move(grandchild_frame));

  std::vector<Quad> child_quads = {Quad::SurfaceQuad(
      SurfaceRange(std::nullopt, grandchild_surface_id), SkColors::kWhite,
      gfx::Rect(7, 7), /*stretch_content_to_fill_bounds=*/false)};
  std::vector<Pass> child_passes = {
      Pass(child_quads, CompositorRenderPassId{1}, gfx::Size(30, 30))};

  CompositorFrame child_frame = MakeEmptyCompositorFrame();
  AddPasses(&child_frame.render_pass_list, child_passes,
            &child_frame.metadata.referenced_surfaces);

  child_frame.render_pass_list[0]
      ->shared_quad_state_list.ElementAt(0)
      ->quad_to_target_transform.Translate(36, 15);

  pt = child_frame.render_pass_list[0]
           ->shared_quad_state_list.ElementAt(0)
           ->quad_to_target_transform.MapPoint(pt);
  area = child_frame.render_pass_list[0]
             ->shared_quad_state_list.ElementAt(0)
             ->quad_to_target_transform.MapRect(area);

  TestSurfaceIdAllocator child_surface_id(child_sink_->frame_sink_id());
  child_sink_->SubmitCompositorFrame(child_surface_id.local_surface_id(),
                                     std::move(child_frame));

  std::vector<Quad> root_quads = {Quad::SurfaceQuad(
      SurfaceRange(std::nullopt, child_surface_id), SkColors::kWhite,
      gfx::Rect(5, 5), /*stretch_content_to_fill_bounds=*/false)};

  std::vector<Pass> root_passes = {
      Pass(root_quads, CompositorRenderPassId{1}, gfx::Size(30, 30))};

  CompositorFrame root_frame = MakeEmptyCompositorFrame();
  AddPasses(&root_frame.render_pass_list, root_passes,
            &root_frame.metadata.referenced_surfaces);

  root_frame.render_pass_list[0]
      ->shared_quad_state_list.ElementAt(0)
      ->quad_to_target_transform.Scale(0.7, 0.7);
  root_frame.render_pass_list[0]
      ->shared_quad_state_list.ElementAt(0)
      ->quad_to_target_transform.Translate(70, 240);

  pt = root_frame.render_pass_list[0]
           ->shared_quad_state_list.ElementAt(0)
           ->quad_to_target_transform.MapPoint(pt);
  area = root_frame.render_pass_list[0]
             ->shared_quad_state_list.ElementAt(0)
             ->quad_to_target_transform.MapRect(area);

  root_sink_->SubmitCompositorFrame(root_surface_id_.local_surface_id(),
                                    std::move(root_frame));

  auto aggregated_frame = AggregateFrame(root_surface_id_);

  metadata = gfx::DelegatedInkMetadata(
      pt, metadata.diameter(), metadata.color(), metadata.timestamp(), area,
      metadata.frame_time(), metadata.is_hovering(), /*render_pass_id=*/0);

  std::unique_ptr<gfx::DelegatedInkMetadata> actual_metadata =
      std::move(aggregated_frame.delegated_ink_metadata);
  EXPECT_TRUE(actual_metadata);
  ExpectDelegatedInkMetadataIsEqual(*actual_metadata.get(), metadata);

  // Send a compositor frame with no delegated ink metadata.
  CompositorFrame blank_frame = MakeEmptyCompositorFrame();
  AddPasses(&blank_frame.render_pass_list, greatgrandchild_passes,
            &blank_frame.metadata.referenced_surfaces);
  greatgrand_child_support->SubmitCompositorFrame(
      great_grandchild_surface_id.local_surface_id(), std::move(blank_frame));

  // Then confirm that the |delegated_ink_metadata| was reset and a new
  // aggregated frame does not contain any delegated ink metadata.
  auto new_aggregated_frame = AggregateFrame(root_surface_id_);
  EXPECT_FALSE(new_aggregated_frame.delegated_ink_metadata);
}

// Confirm the metadata is transformed correctly and makes it to the aggregated
// frame when there are multiple children.
TEST_F(SurfaceAggregatorValidSurfaceTest,
       DelegatedInkMetadataMultipleChildren) {
  auto child_2_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryMiddleFrameSinkId, /*is_root=*/false);
  auto child_3_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryFrameSinkId2, /*is_root=*/false);

  std::vector<Quad> child_1_quads = {
      Quad::SolidColorQuad(SkColors::kGreen, gfx::Rect(5, 5))};
  std::vector<Pass> child_1_passes = {
      Pass(child_1_quads, CompositorRenderPassId{1}, gfx::Size(100, 100))};

  CompositorFrame child_1_frame = MakeEmptyCompositorFrame();
  AddPasses(&child_1_frame.render_pass_list, child_1_passes,
            &child_1_frame.metadata.referenced_surfaces);

  TestSurfaceIdAllocator child_1_surface_id(child_sink_->frame_sink_id());
  child_sink_->SubmitCompositorFrame(child_1_surface_id.local_surface_id(),
                                     std::move(child_1_frame));

  std::vector<Quad> child_2_quads = {
      Quad::SolidColorQuad(SkColors::kMagenta, gfx::Rect(5, 5))};
  std::vector<Pass> child_2_passes = {
      Pass(child_2_quads, CompositorRenderPassId{1}, gfx::Size(100, 100))};

  gfx::DelegatedInkMetadata metadata(
      gfx::PointF(88, 34), 1.8, SK_ColorBLACK, base::TimeTicks::Now(),
      gfx::RectF(50, 50, 300, 300), base::TimeTicks::Now(), /*hovering*/ true,
      /*render_pass_id=*/0);
  CompositorFrame child_2_frame = MakeEmptyCompositorFrame();
  child_2_frame.metadata.delegated_ink_metadata =
      std::make_unique<gfx::DelegatedInkMetadata>(metadata);
  AddPasses(&child_2_frame.render_pass_list, child_2_passes,
            &child_2_frame.metadata.referenced_surfaces);

  TestSurfaceIdAllocator child_2_surface_id(child_2_support->frame_sink_id());
  child_2_support->SubmitCompositorFrame(child_2_surface_id.local_surface_id(),
                                         std::move(child_2_frame));

  std::vector<Quad> child_3_quads = {
      Quad::SolidColorQuad(SkColors::kCyan, gfx::Rect(5, 5))};
  std::vector<Pass> child_3_passes = {
      Pass(child_3_quads, CompositorRenderPassId{1}, gfx::Size(100, 100))};

  CompositorFrame child_3_frame = MakeEmptyCompositorFrame();
  AddPasses(&child_3_frame.render_pass_list, child_3_passes,
            &child_3_frame.metadata.referenced_surfaces);

  TestSurfaceIdAllocator child_3_surface_id(child_3_support->frame_sink_id());
  child_3_support->SubmitCompositorFrame(child_3_surface_id.local_surface_id(),
                                         std::move(child_3_frame));

  std::vector<Quad> root_quads = {
      Quad::SurfaceQuad(SurfaceRange(std::nullopt, child_1_surface_id),
                        SkColors::kWhite, gfx::Rect(5, 5),
                        /*stretch_content_to_fill_bounds=*/false),
      Quad::SurfaceQuad(SurfaceRange(std::nullopt, child_2_surface_id),
                        SkColors::kWhite, gfx::Rect(5, 5),
                        /*stretch_content_to_fill_bounds=*/false),
      Quad::SurfaceQuad(SurfaceRange(std::nullopt, child_3_surface_id),
                        SkColors::kWhite, gfx::Rect(5, 5),
                        /*stretch_content_to_fill_bounds=*/false)};

  std::vector<Pass> root_passes = {
      Pass(root_quads, CompositorRenderPassId{1}, gfx::Size(30, 30))};

  CompositorFrame root_frame = MakeEmptyCompositorFrame();
  AddPasses(&root_frame.render_pass_list, root_passes,
            &root_frame.metadata.referenced_surfaces);

  root_frame.render_pass_list[0]
      ->shared_quad_state_list.ElementAt(0)
      ->quad_to_target_transform.Translate(9, 87);
  root_frame.render_pass_list[0]
      ->shared_quad_state_list.ElementAt(1)
      ->quad_to_target_transform.Scale(0.7, 0.7);
  root_frame.render_pass_list[0]
      ->shared_quad_state_list.ElementAt(1)
      ->quad_to_target_transform.Translate(70, 240);
  root_frame.render_pass_list[0]
      ->shared_quad_state_list.ElementAt(2)
      ->quad_to_target_transform.Scale(2.7, 0.2);

  // Update the expected metadata to reflect the transforms to point and area
  // that are expected to occur.
  gfx::PointF pt = root_frame.render_pass_list[0]
                       ->shared_quad_state_list.ElementAt(1)
                       ->quad_to_target_transform.MapPoint(metadata.point());
  gfx::RectF area =
      root_frame.render_pass_list[0]
          ->shared_quad_state_list.ElementAt(1)
          ->quad_to_target_transform.MapRect(metadata.presentation_area());

  root_sink_->SubmitCompositorFrame(root_surface_id_.local_surface_id(),
                                    std::move(root_frame));

  auto aggregated_frame = AggregateFrame(root_surface_id_);

  metadata = gfx::DelegatedInkMetadata(
      pt, metadata.diameter(), metadata.color(), metadata.timestamp(), area,
      metadata.frame_time(), metadata.is_hovering(), /*render_pass_id=*/0);

  std::unique_ptr<gfx::DelegatedInkMetadata> actual_metadata =
      std::move(aggregated_frame.delegated_ink_metadata);
  EXPECT_TRUE(actual_metadata);
  ExpectDelegatedInkMetadataIsEqual(*actual_metadata.get(), metadata);

  // Send a compositor frame with no delegated ink metadata.
  CompositorFrame blank_frame = MakeEmptyCompositorFrame();
  AddPasses(&blank_frame.render_pass_list, child_2_passes,
            &blank_frame.metadata.referenced_surfaces);
  child_2_support->SubmitCompositorFrame(child_2_surface_id.local_surface_id(),
                                         std::move(blank_frame));

  // Then confirm that the |delegated_ink_metadata| was reset and a new
  // aggregated frame does not contain any delegated ink metadata.
  auto new_aggregated_frame = AggregateFrame(root_surface_id_);
  EXPECT_FALSE(new_aggregated_frame.delegated_ink_metadata);
}

// Confirm the the metadata with the most recent timestamp is used when
// multiple children have delegated ink metadata.
TEST_F(SurfaceAggregatorValidSurfaceTest,
       MultipleChildrenHaveDelegatedInkMetadata) {
  auto child_2_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryMiddleFrameSinkId, /*is_root=*/false);
  auto child_3_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryFrameSinkId2, /*is_root=*/false);

  std::vector<Quad> child_1_quads = {
      Quad::SolidColorQuad(SkColors::kGreen, gfx::Rect(5, 5))};
  std::vector<Pass> child_1_passes = {
      Pass(child_1_quads, CompositorRenderPassId{1}, gfx::Size(100, 100))};

  CompositorFrame child_1_frame = MakeEmptyCompositorFrame();
  AddPasses(&child_1_frame.render_pass_list, child_1_passes,
            &child_1_frame.metadata.referenced_surfaces);

  TestSurfaceIdAllocator child_1_surface_id(child_sink_->frame_sink_id());
  child_sink_->SubmitCompositorFrame(child_1_surface_id.local_surface_id(),
                                     std::move(child_1_frame));

  std::vector<Quad> child_2_quads = {
      Quad::SolidColorQuad(SkColors ::kMagenta, gfx::Rect(5, 5))};
  std::vector<Pass> child_2_passes = {
      Pass(child_2_quads, CompositorRenderPassId{1}, gfx::Size(100, 100))};

  // Making both metadatas here so that the one with a later timestamp can be
  // on child 2. This will cause the test to fail if we don't default to using
  // the metadata with the later timestamp. Specifically setting the
  // later_metadata timestamp to be 50 microseconds later than Now() to avoid
  // issues with both metadatas sometimes having the same time in Release.
  gfx::DelegatedInkMetadata early_metadata(
      gfx::PointF(88, 34), 1.8, SK_ColorBLACK, base::TimeTicks::Now(),
      gfx::RectF(50, 50, 300, 300), base::TimeTicks::Now(), /*hovering*/ false,
      /*render_pass_id=*/0);
  gfx::DelegatedInkMetadata later_metadata(
      gfx::PointF(92, 35), 0.08, SK_ColorYELLOW,
      base::TimeTicks::Now() + base::Microseconds(50),
      gfx::RectF(35, 55, 128, 256),
      base::TimeTicks::Now() + base::Microseconds(52),
      /*hovering*/ true, /*render_pass_id=*/0);

  CompositorFrame child_2_frame = MakeEmptyCompositorFrame();
  child_2_frame.metadata.delegated_ink_metadata =
      std::make_unique<gfx::DelegatedInkMetadata>(later_metadata);
  AddPasses(&child_2_frame.render_pass_list, child_2_passes,
            &child_2_frame.metadata.referenced_surfaces);

  TestSurfaceIdAllocator child_2_surface_id(child_2_support->frame_sink_id());
  child_2_support->SubmitCompositorFrame(child_2_surface_id.local_surface_id(),
                                         std::move(child_2_frame));

  std::vector<Quad> child_3_quads = {
      Quad::SolidColorQuad(SkColors ::kCyan, gfx::Rect(5, 5))};
  std::vector<Pass> child_3_passes = {
      Pass(child_3_quads, CompositorRenderPassId{1}, gfx::Size(100, 100))};

  CompositorFrame child_3_frame = MakeEmptyCompositorFrame();
  child_3_frame.metadata.delegated_ink_metadata =
      std::make_unique<gfx::DelegatedInkMetadata>(early_metadata);
  AddPasses(&child_3_frame.render_pass_list, child_3_passes,
            &child_3_frame.metadata.referenced_surfaces);

  TestSurfaceIdAllocator child_3_surface_id(child_3_support->frame_sink_id());
  child_3_support->SubmitCompositorFrame(child_3_surface_id.local_surface_id(),
                                         std::move(child_3_frame));

  std::vector<Quad> root_quads = {
      Quad::SurfaceQuad(SurfaceRange(std::nullopt, child_1_surface_id),
                        SkColors ::kWhite, gfx::Rect(5, 5),
                        /*stretch_content_to_fill_bounds=*/false),
      Quad::SurfaceQuad(SurfaceRange(std::nullopt, child_2_surface_id),
                        SkColors ::kWhite, gfx::Rect(5, 5),
                        /*stretch_content_to_fill_bounds=*/false),
      Quad::SurfaceQuad(SurfaceRange(std::nullopt, child_3_surface_id),
                        SkColors ::kWhite, gfx::Rect(5, 5),
                        /*stretch_content_to_fill_bounds=*/false)};

  std::vector<Pass> root_passes = {
      Pass(root_quads, CompositorRenderPassId{1}, gfx::Size(30, 30))};

  CompositorFrame root_frame = MakeEmptyCompositorFrame();
  AddPasses(&root_frame.render_pass_list, root_passes,
            &root_frame.metadata.referenced_surfaces);

  root_frame.render_pass_list[0]
      ->shared_quad_state_list.ElementAt(0)
      ->quad_to_target_transform.Translate(9, 87);
  root_frame.render_pass_list[0]
      ->shared_quad_state_list.ElementAt(1)
      ->quad_to_target_transform.Scale(1.4, 1.7);
  root_frame.render_pass_list[0]
      ->shared_quad_state_list.ElementAt(1)
      ->quad_to_target_transform.Translate(214, 144);
  root_frame.render_pass_list[0]
      ->shared_quad_state_list.ElementAt(2)
      ->quad_to_target_transform.Scale(2.7, 0.2);

  // Two surfaces have delegated ink metadata on them, and when this happens
  // on the metadata with the most recent timestamp should be used. Take this
  // metadata and transform it to what should be expected.
  gfx::PointF pt =
      root_frame.render_pass_list[0]
          ->shared_quad_state_list.ElementAt(1)
          ->quad_to_target_transform.MapPoint(later_metadata.point());
  gfx::RectF area = root_frame.render_pass_list[0]
                        ->shared_quad_state_list.ElementAt(1)
                        ->quad_to_target_transform.MapRect(
                            later_metadata.presentation_area());

  root_sink_->SubmitCompositorFrame(root_surface_id_.local_surface_id(),
                                    std::move(root_frame));

  auto aggregated_frame = AggregateFrame(root_surface_id_);

  gfx::DelegatedInkMetadata expected_metadata(
      pt, later_metadata.diameter(), later_metadata.color(),
      later_metadata.timestamp(), area, later_metadata.frame_time(),
      later_metadata.is_hovering(), later_metadata.render_pass_id());

  std::unique_ptr<gfx::DelegatedInkMetadata> actual_metadata =
      std::move(aggregated_frame.delegated_ink_metadata);
  EXPECT_TRUE(actual_metadata);
  ExpectDelegatedInkMetadataIsEqual(*actual_metadata.get(), expected_metadata);

  // Send a compositor frame for child_3 with no delegated ink metadata.
  CompositorFrame blank_frame = MakeEmptyCompositorFrame();
  AddPasses(&blank_frame.render_pass_list, child_3_passes,
            &blank_frame.metadata.referenced_surfaces);
  child_3_support->SubmitCompositorFrame(child_3_surface_id.local_surface_id(),
                                         std::move(blank_frame));

  // Then confirm that the |delegated_ink_metadata| was  not reset because the
  // compositor frame metadata for child_2 still contains delegated ink
  // metadata.
  auto new_aggregated_frame = AggregateFrame(root_surface_id_);
  EXPECT_TRUE(new_aggregated_frame.delegated_ink_metadata);

  // Send a compositor frame for child_2 with no delegated ink metadata.
  blank_frame = MakeEmptyCompositorFrame();
  AddPasses(&blank_frame.render_pass_list, child_2_passes,
            &blank_frame.metadata.referenced_surfaces);
  child_2_support->SubmitCompositorFrame(child_2_surface_id.local_surface_id(),
                                         std::move(blank_frame));

  // Now confirm that the |delegated_ink_metadata| was reset and a new
  // aggregated frame does not contain any delegated ink metadata.
  new_aggregated_frame = AggregateFrame(root_surface_id_);
  EXPECT_FALSE(new_aggregated_frame.delegated_ink_metadata);
}

// Confirm that delegated ink metadata on an undrawn surface is not on the
// aggregated surface unless the undrawn surface contains a CopyOutputRequest.
TEST_F(SurfaceAggregatorValidSurfaceTest,
       DelegatedInkMetadataOnUndrawnSurface) {
  std::vector<Quad> child_quads = {
      Quad::SolidColorQuad(SkColors ::kGreen, gfx::Rect(5, 5))};
  std::vector<Pass> child_passes = {
      Pass(child_quads, CompositorRenderPassId{1}, gfx::Size(100, 100))};

  CompositorFrame child_frame = MakeEmptyCompositorFrame();
  gfx::DelegatedInkMetadata metadata(
      gfx::PointF(34, 89), 1.597, SK_ColorBLUE, base::TimeTicks::Now(),
      gfx::RectF(2.3, 3.2, 177, 212), base::TimeTicks::Now(),
      /*hovering*/ false, /*render_pass_id=*/0);
  child_frame.metadata.delegated_ink_metadata =
      std::make_unique<gfx::DelegatedInkMetadata>(metadata);
  AddPasses(&child_frame.render_pass_list, child_passes,
            &child_frame.metadata.referenced_surfaces);

  TestSurfaceIdAllocator child_surface_id(child_sink_->frame_sink_id());
  child_sink_->SubmitCompositorFrame(child_surface_id.local_surface_id(),
                                     std::move(child_frame));

  // Do not put the child surface in a SurfaceDrawQuad so that it remains
  // undrawn.
  std::vector<Quad> root_quads = {
      Quad::SolidColorQuad(SkColors ::kMagenta, gfx::Rect(5, 5))};

  std::vector<Pass> root_passes = {
      Pass(root_quads, CompositorRenderPassId{1}, gfx::Size(30, 30))};

  CompositorFrame root_frame = MakeEmptyCompositorFrame();
  root_frame.metadata.referenced_surfaces.emplace_back(
      SurfaceRange(std::nullopt, child_surface_id));
  AddPasses(&root_frame.render_pass_list, root_passes,
            &root_frame.metadata.referenced_surfaces);

  root_frame.render_pass_list[0]
      ->shared_quad_state_list.ElementAt(0)
      ->quad_to_target_transform.Scale(1.5, 1.5);
  root_frame.render_pass_list[0]
      ->shared_quad_state_list.ElementAt(0)
      ->quad_to_target_transform.Translate(70, 240);

  root_sink_->SubmitCompositorFrame(root_surface_id_.local_surface_id(),
                                    std::move(root_frame));

  auto aggregated_frame = AggregateFrame(root_surface_id_);

  EXPECT_FALSE(aggregated_frame.delegated_ink_metadata);

  // Now add a CopyOutputRequest on the child surface, so that the delegated
  // ink metadata does get populated on the aggregated frame.
  auto copy_request = CopyOutputRequest::CreateStubForTesting();
  child_sink_->RequestCopyOfOutput({child_surface_id.local_surface_id(),
                                    SubtreeCaptureId(),
                                    std::move(copy_request)});

  aggregated_frame = AggregateFrame(root_surface_id_);

  std::unique_ptr<gfx::DelegatedInkMetadata> actual_metadata =
      std::move(aggregated_frame.delegated_ink_metadata);
  EXPECT_TRUE(actual_metadata);
  ExpectDelegatedInkMetadataIsEqual(*actual_metadata.get(), metadata);

  // Then confirm that the |delegated_ink_metadata| was reset and a new
  // aggregated frame does not contain any delegated ink metadata.
  auto new_aggregated_frame = AggregateFrame(root_surface_id_);
  EXPECT_FALSE(new_aggregated_frame.delegated_ink_metadata);
}

TEST_F(SurfaceAggregatorValidSurfaceTest, HasUnembeddedRenderPass) {
  constexpr gfx::Rect unembedded_rect(50, 50);
  constexpr gfx::Rect root_rect(kSurfaceSize);

  {
    CompositorFrame frame =
        CompositorFrameBuilder()
            // This render pass isn't embedded by the root so it doesn't need to
            // be drawn to draw the root render pass.
            .AddRenderPass(
                RenderPassBuilder(CompositorRenderPassId{1}, unembedded_rect)
                    .AddSolidColorQuad(root_rect, SkColors::kGreen)
                    .Build())
            .AddRenderPass(
                RenderPassBuilder(CompositorRenderPassId{2}, root_rect)
                    .AddSolidColorQuad(root_rect, SkColors::kBlue)
                    .Build())
            .Build();

    root_sink_->SubmitCompositorFrame(root_surface_id_.local_surface_id(),
                                      std::move(frame));
  }

  auto aggregated_frame = AggregateFrame(root_surface_id_);
  auto& render_pass_list = aggregated_frame.render_pass_list;
  ASSERT_EQ(2u, render_pass_list.size());

  // The unembedded render pass is included in the AggegatedFrame despite not
  // reachable from the root render pass. Both of them have damage from
  // contributing content since this is the first aggregation.
  auto& unembedded_pass = render_pass_list[0];
  EXPECT_THAT(unembedded_pass->quad_list,
              ElementsAre(IsSolidColorQuad(SkColors::kGreen)));
  EXPECT_EQ(unembedded_pass->output_rect, unembedded_rect);
  EXPECT_TRUE(unembedded_pass->has_damage_from_contributing_content);

  auto& root_pass = render_pass_list[1];
  EXPECT_THAT(root_pass->quad_list,
              ElementsAre(IsSolidColorQuad(SkColors::kBlue)));
  EXPECT_EQ(root_pass->output_rect, root_rect);
  EXPECT_TRUE(root_pass->has_damage_from_contributing_content);
}

TEST_F(SurfaceAggregatorValidSurfaceTest,
       HasDamageFromContributingPropagatedForUnembeddedRenderPass) {
  constexpr gfx::Rect unembedded_rect(50, 50);
  constexpr gfx::Rect root_rect(kSurfaceSize);
  TestSurfaceIdAllocator child_surface_id(child_sink_->frame_sink_id());

  {
    CompositorFrame frame =
        CompositorFrameBuilder()
            // This render pass isn't embedded by the root so it doesn't need to
            // be drawn to draw the root render pass.
            .AddRenderPass(
                RenderPassBuilder(CompositorRenderPassId{1}, unembedded_rect)
                    .AddSurfaceQuad(unembedded_rect,
                                    SurfaceRange(child_surface_id))
                    .Build())
            .AddRenderPass(
                RenderPassBuilder(CompositorRenderPassId{2}, root_rect)
                    .AddSolidColorQuad(root_rect, SkColors::kBlue)
                    .Build())
            .Build();

    root_sink_->SubmitCompositorFrame(root_surface_id_.local_surface_id(),
                                      std::move(frame));
  }

  {
    // This child frame is not reachable from root surface root render pass.
    CompositorFrame frame =
        CompositorFrameBuilder()
            .AddRenderPass(
                RenderPassBuilder(CompositorRenderPassId{1}, unembedded_rect)
                    .AddSolidColorQuad(unembedded_rect, SkColors::kGreen)
                    .Build())
            .Build();

    child_sink_->SubmitCompositorFrame(child_surface_id.local_surface_id(),
                                       std::move(frame));
  }

  {
    auto aggregated_frame = AggregateFrame(root_surface_id_);
    auto& render_pass_list = aggregated_frame.render_pass_list;
    ASSERT_EQ(2u, render_pass_list.size());

    // The child surface is merged into the non-embedded render pass so it has
    // a green draw quad. The root render pass has only the expected blue draw
    // quad. Both of them have damage from contributing content since this is
    // the first aggregation.
    auto& unembedded_pass = render_pass_list[0];
    EXPECT_THAT(unembedded_pass->quad_list,
                ElementsAre(IsSolidColorQuad(SkColors::kGreen)));
    EXPECT_EQ(unembedded_pass->output_rect, unembedded_rect);
    EXPECT_TRUE(unembedded_pass->has_damage_from_contributing_content);

    auto& root_pass = render_pass_list[1];
    EXPECT_THAT(root_pass->quad_list,
                ElementsAre(IsSolidColorQuad(SkColors::kBlue)));
    EXPECT_EQ(root_pass->output_rect, root_rect);
    EXPECT_TRUE(root_pass->has_damage_from_contributing_content);
  }

  {
    // Perform a second aggregation where nothing has changed. There is no
    // damage from either surface so both render passes will have no damage from
    // contributing content.
    auto aggregated_frame = AggregateFrame(root_surface_id_);
    auto& render_pass_list = aggregated_frame.render_pass_list;
    ASSERT_EQ(2u, render_pass_list.size());

    EXPECT_FALSE(render_pass_list[0]->has_damage_from_contributing_content);
    EXPECT_FALSE(render_pass_list[1]->has_damage_from_contributing_content);
  }

  {
    // Submit a new frame so child surface has damage and redo aggregation.
    CompositorFrame frame =
        CompositorFrameBuilder()
            .AddRenderPass(
                RenderPassBuilder(CompositorRenderPassId{1}, unembedded_rect)
                    .AddSolidColorQuad(unembedded_rect, SkColors::kGreen)
                    .Build())
            .Build();

    child_sink_->SubmitCompositorFrame(child_surface_id.local_surface_id(),
                                       std::move(frame));
    auto aggregated_frame = AggregateFrame(root_surface_id_);
    auto& render_pass_list = aggregated_frame.render_pass_list;
    ASSERT_EQ(2u, render_pass_list.size());

    // The root surface has no damage since it didn't submit a new frame so the
    // root render pass has no damage from contributing content. The unembedded
    // render pass from root surface embeds the child surface, so it should
    // have damage from unembedded passes.
    EXPECT_TRUE(render_pass_list[0]->has_damage_from_contributing_content);
    EXPECT_FALSE(render_pass_list[1]->has_damage_from_contributing_content);
  }
}

// Tests that a CopyOutputRequest on a render pass that's not embedded from the
// root pass is recognized when copying secure texture content.
TEST_F(SurfaceAggregatorValidSurfaceTest,
       CopyRequestWithSecureOutputForUnembeddedRenderPass) {
  aggregator_.set_output_is_secure(true);

  {
    constexpr gfx::Rect rect(kSurfaceSize);
    CompositorFrame frame =
        CompositorFrameBuilder()
            // This render pass has TextureDrawQuad with secure_output_only true
            // that needs to be removed if there is a CopyOutputRequest.
            .AddRenderPass(RenderPassBuilder(CompositorRenderPassId{1}, rect)
                               .AddTextureQuad(rect, ResourceId(1),
                                               {.secure_output_only = true})
                               .Build())
            // This render pass isn't embedded by the root but it embeds the
            // render pass with TextureDrawQuad.
            .AddRenderPass(
                RenderPassBuilder(CompositorRenderPassId{2}, rect)
                    .AddRenderPassQuad(rect, CompositorRenderPassId{1})
                    .AddStubCopyOutputRequest()
                    .Build())
            .AddRenderPass(RenderPassBuilder(CompositorRenderPassId{3}, rect)
                               .AddSolidColorQuad(rect, SkColors::kGreen)
                               .Build())
            .PopulateResources()
            .Build();

    root_sink_->SubmitCompositorFrame(root_surface_id_.local_surface_id(),
                                      std::move(frame));
  }

  auto aggregated_frame = AggregateFrame(root_surface_id_);
  EXPECT_TRUE(aggregated_frame.has_copy_requests);

  auto& render_pass_list = aggregated_frame.render_pass_list;
  ASSERT_EQ(3u, render_pass_list.size());

  // The first render pass had a TextureDrawQuad that contains secure output.
  // This render pass is embedded by the second render pass which has a copy
  // request. The TextureDrawQuad should be replaced by a black
  // SolidColorDrawQuad since it's included in a copy request.
  EXPECT_THAT(render_pass_list[0]->quad_list,
              ElementsAre(IsSolidColorQuad(SkColors::kBlack)));

  // The second render pass should have a single quad to embed the first and a
  // copy output request.
  EXPECT_EQ(render_pass_list[1]->copy_requests.size(), 1u);
  EXPECT_THAT(render_pass_list[1]->quad_list,
              ElementsAre(IsAggregatedRenderPassQuad()));

  // The root pass does not embed either of the first two passes and should just
  // contain a single SolidColorDrawQuad since the TextureDrawQuad was replaced.
  EXPECT_THAT(render_pass_list[2]->quad_list,
              ElementsAre(IsSolidColorQuad(SkColors::kGreen)));
}

// Tests that changing the color usage results in full-frame damage.
TEST_F(SurfaceAggregatorValidSurfaceTest, ColorUsageChangeFullFrameDamage) {
  constexpr float device_scale_factor = 1.0f;
  const gfx::Rect full_damage_rect(kSurfaceSize);
  const gfx::Rect partial_damage_rect(10, 10, 10, 10);
  std::vector<Quad> quads = {
      Quad::SolidColorQuad(SkColors ::kRed, gfx::Rect(kSurfaceSize))};
  std::vector<Pass> passes = {Pass(quads, kSurfaceSize)};
  passes[0].damage_rect = partial_damage_rect;

  // First frame has full damage.
  {
    SubmitCompositorFrame(root_sink_.get(), passes,
                          root_surface_id_.local_surface_id(),
                          device_scale_factor);
    auto aggregated_frame = AggregateFrame(root_surface_id_);

    EXPECT_EQ(gfx::ContentColorUsage::kHDR,
              aggregated_frame.render_pass_list[0]->content_color_usage);
    EXPECT_EQ(full_damage_rect,
              aggregated_frame.render_pass_list[0]->damage_rect);
  }
  // Second frame has partial damage.
  {
    SubmitCompositorFrame(root_sink_.get(), passes,
                          root_surface_id_.local_surface_id(),
                          device_scale_factor);
    auto aggregated_frame = AggregateFrame(root_surface_id_);

    EXPECT_EQ(gfx::ContentColorUsage::kHDR,
              aggregated_frame.render_pass_list[0]->content_color_usage);
    EXPECT_EQ(partial_damage_rect,
              aggregated_frame.render_pass_list[0]->damage_rect);
  }
  // Finally, change the content_color_usage from HDR to sRGB. The resulting
  // frame should have full damage.
  {
    CompositorFrame compositor_frame = MakeEmptyCompositorFrame();
    compositor_frame.metadata.content_color_usage =
        gfx::ContentColorUsage::kSRGB;
    AddPasses(&compositor_frame.render_pass_list, passes,
              &compositor_frame.metadata.referenced_surfaces);
    root_sink_->SubmitCompositorFrame(root_surface_id_.local_surface_id(),
                                      std::move(compositor_frame));
    auto aggregated_frame = AggregateFrame(root_surface_id_);

    EXPECT_EQ(gfx::ContentColorUsage::kSRGB,
              aggregated_frame.render_pass_list[0]->content_color_usage);
    EXPECT_EQ(full_damage_rect,
              aggregated_frame.render_pass_list[0]->damage_rect);
  }
}

// Test the Clip Rect of a non-merged pass from an embedded surface.
TEST_F(SurfaceAggregatorValidSurfaceTest, ClipRectNonMergedPass) {
  // A grand child surface is embedded into a child surface. This child surface
  // is then embedded into a root surface. Make the grandchild_rect the biggest
  // so it will be clipped after surface aggregation.
  const gfx::Rect grandchild_rect(0, 0, 200, 200);
  const gfx::Rect child_rect(10, 10, 150, 150);
  const gfx::Rect root_rect(0, 0, 100, 100);

  auto grandchild_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &this->manager_, kArbitraryFrameSinkId1, false);
  TestSurfaceIdAllocator grandchild_surface_id(
      grandchild_support->frame_sink_id());
  TestSurfaceIdAllocator child_surface_id(child_sink_->frame_sink_id());

  // The grandchild CompositorFrame contains a 200x200 surface with a 200x200
  // SolidColorDrawQuad. This surface is embedded into a child surface, but is
  // not allowed to merged into the root render pass of the root surface. As a
  // result, only 90x90 of the grandchild SolidColorDrawQuad can be drawn onto
  // the root frame. The rest will be clipped.
  {
    auto frame =
        CompositorFrameBuilder()
            .AddRenderPass(RenderPassBuilder(grandchild_rect)
                               .AddSolidColorQuad(gfx::Rect(grandchild_rect),
                                                  SkColors ::kBlue))
            .Build();
    grandchild_support->SubmitCompositorFrame(
        grandchild_surface_id.local_surface_id(), std::move(frame));
  }
  {
    // The grandchild surface is not allowed to merge.
    auto frame = CompositorFrameBuilder()
                     .AddRenderPass(RenderPassBuilder(child_rect)
                                        .AddSurfaceQuad(
                                            grandchild_rect,
                                            SurfaceRange(grandchild_surface_id),
                                            {.allow_merge = false}))
                     .Build();
    child_sink_->SubmitCompositorFrame(child_surface_id.local_surface_id(),
                                       std::move(frame));
  }

  {
    auto frame =
        CompositorFrameBuilder()
            .AddRenderPass(
                RenderPassBuilder(root_rect)
                    .AddSurfaceQuad(child_rect, SurfaceRange(child_surface_id))
                    .AddSolidColorQuad(gfx::Rect(root_rect), SkColors ::kWhite))
            .Build();
    root_sink_->SubmitCompositorFrame(root_surface_id_.local_surface_id(),
                                      std::move(frame));
  }

  // Since the render pass from the grandchild surface cannot be merged,
  // there will be total 2 render passes.
  auto aggregated_frame = AggregateFrame(root_surface_id_);
  EXPECT_EQ(2u, aggregated_frame.render_pass_list.size());

  // A blue SolidColorQuad in the non-root render pass.
  auto& nonroot_render_pass = aggregated_frame.render_pass_list[0];
  EXPECT_THAT(nonroot_render_pass->quad_list,
              ElementsAre(IsSolidColorQuad(SkColors ::kBlue)));
  auto* clipped_quad = nonroot_render_pass->quad_list.front();
  EXPECT_EQ(clipped_quad->rect, grandchild_rect);

  // A RenderPassDrawQuad and a SolidColorQuad in the root render pass.
  auto& root_render_pass = aggregated_frame.render_pass_list[1];
  EXPECT_THAT(root_render_pass->quad_list,
              ElementsAre(IsAggregatedRenderPassQuad(),
                          IsSolidColorQuad(SkColors ::kWhite)));

  // |clip_rect| of this RenderPassDrawQuad is bounded by the child render pass
  // output_rect (10, 10, 150, 150), which is then bounded by the root render
  // pass output_rect (0, 0, 100, 100). The intersection of both output_rects is
  // (10, 10, 90, 90).
  auto* rpdq = root_render_pass->quad_list.front();
  EXPECT_TRUE(rpdq->shared_quad_state->clip_rect);
  EXPECT_THAT(rpdq->shared_quad_state->clip_rect,
              testing::Optional(gfx::Rect(10, 10, 90, 90)));
}

INSTANTIATE_TEST_SUITE_P(,
                         SurfaceAggregatorValidSurfaceWithMergingPassesTest,
                         testing::Bool());

#if BUILDFLAG(IS_WIN)
// The flag |prevent_merging_surfaces_to_root_pass| prevents surfaces referenced
// by the root pass of the root surface (e.g. the web contents surface(s)) from
// merging during surface aggregation. This enables
// |kDelegatedCompositingLimitToUi| because in delegated compositing mode, those
// surfaces become RPDQ overlays.
class SurfaceAggregatorPreventMergeTest
    : public SurfaceAggregatorValidSurfaceTest {
 protected:
  SurfaceAggregatorPreventMergeTest()
      : SurfaceAggregatorValidSurfaceTest(
            SurfaceAggregator::ExtraPassForReadbackOption::kNone,
            /*prevent_merging_surfaces_to_root_pass=*/true) {}
};

// Check that surfaces in the root pass are not allowed to merge.
TEST_F(SurfaceAggregatorPreventMergeTest, PreventMerge) {
  const gfx::Rect child_rect(5, 5);

  TestVizClient child(this, &manager_, kArbitraryFrameSinkId1, child_rect);
  child.SubmitCompositorFrame(SkColors::kGreen);

  // Submit a SurfaceDrawQuad that allows merging, but will be prevented.
  {
    std::vector<Quad> root_quads = {
        Quad::SurfaceQuad(SurfaceRange(std::nullopt, child.surface_id()),
                          SkColors::kWhite, child_rect, false, true)};
    std::vector<Pass> root_passes = {
        Pass(root_quads, CompositorRenderPassId{1}, kSurfaceSize)};

    CompositorFrame frame = MakeEmptyCompositorFrame();
    AddPasses(&frame.render_pass_list, root_passes,
              &frame.metadata.referenced_surfaces);

    root_sink_->SubmitCompositorFrame(root_surface_id_.local_surface_id(),
                                      std::move(frame));
  }

  auto aggregated_frame = AggregateFrame(root_surface_id_);

  // We expect |child| to be prevented from merging.
  EXPECT_EQ(2u, aggregated_frame.render_pass_list.size());
}

TEST_F(SurfaceAggregatorPreventMergeTest, NonRootSurfacesCanMerge) {
  const gfx::Rect child_rect(5, 5);

  // Submit a leaf surface that does not contain other surfaces. This should be
  // merged into |child_surface_id| because |child_surface_id| is not the root.
  TestVizClient inner_child(this, &manager_, kArbitraryFrameSinkId1,
                            child_rect);
  inner_child.SubmitCompositorFrame(SkColors::kBlue);

  // Submit an intermediate surface that embeds the leaf and will be embedded by
  // the root.
  TestSurfaceIdAllocator child_surface_id(child_sink_->frame_sink_id());
  {
    std::vector<Quad> child_quads = {
        Quad::SurfaceQuad(SurfaceRange(std::nullopt, inner_child.surface_id()),
                          SkColors::kGreen, child_rect, false, true)};
    std::vector<Pass> child_passes = {
        Pass(child_quads, CompositorRenderPassId{1}, kSurfaceSize)};

    CompositorFrame child_frame = MakeEmptyCompositorFrame();
    AddPasses(&child_frame.render_pass_list, child_passes,
              &child_frame.metadata.referenced_surfaces);

    child_sink_->SubmitCompositorFrame(child_surface_id.local_surface_id(),
                                       std::move(child_frame));
  }

  // Submit a SurfaceDrawQuad that allows merging, but will be prevented.
  {
    std::vector<Quad> root_quads = {
        Quad::SurfaceQuad(SurfaceRange(std::nullopt, child_surface_id),
                          SkColors::kWhite, child_rect, false, true)};
    std::vector<Pass> root_passes = {
        Pass(root_quads, CompositorRenderPassId{1}, kSurfaceSize)};

    CompositorFrame frame = MakeEmptyCompositorFrame();
    AddPasses(&frame.render_pass_list, root_passes,
              &frame.metadata.referenced_surfaces);

    root_sink_->SubmitCompositorFrame(root_surface_id_.local_surface_id(),
                                      std::move(frame));
  }

  auto aggregated_frame = AggregateFrame(root_surface_id_);

  // We expect |inner_child| to merge into |child_surface_id|, but not for
  // |child_surface_id| to merge into |root_surface_id_|.
  EXPECT_EQ(2u, aggregated_frame.render_pass_list.size());
}
#endif

class SurfaceAggregatorVulkanSecondaryCB
    : public SurfaceAggregatorValidSurfaceTest {
 public:
  SurfaceAggregatorVulkanSecondaryCB()
      : SurfaceAggregatorValidSurfaceTest(
            SurfaceAggregator::ExtraPassForReadbackOption::kAddPassForReadback,
            false) {}
};

TEST_F(SurfaceAggregatorVulkanSecondaryCB, AppendPassForFrameWithFilter) {
  CompositorFrame frame =
      CompositorFrameBuilder()
          .AddRenderPass(
              RenderPassBuilder(CompositorRenderPassId{1}, kSurfaceSize)
                  .AddSolidColorQuad(gfx::Rect(5, 5), SkColors ::kGreen)
                  .AddBackdropFilter(cc::FilterOperation::CreateBlurFilter(5)))
          .AddRenderPass(
              RenderPassBuilder(CompositorRenderPassId{2}, kSurfaceSize)
                  .AddRenderPassQuad(gfx::Rect(kSurfaceSize),
                                     CompositorRenderPassId{1}))
          .Build();

  root_sink_->SubmitCompositorFrame(root_surface_id_.local_surface_id(),
                                    std::move(frame));

  SurfaceId surface_id(root_sink_->frame_sink_id(),
                       root_surface_id_.local_surface_id());
  auto aggregated_frame = AggregateFrame(surface_id);
  EXPECT_EQ(3u, aggregated_frame.render_pass_list.size());
}

TEST_F(SurfaceAggregatorVulkanSecondaryCB,
       DoNotAppendPassForFrameWithoutReadback) {
  CompositorFrame frame =
      CompositorFrameBuilder()
          .AddRenderPass(
              RenderPassBuilder(CompositorRenderPassId{1}, kSurfaceSize)
                  .AddSolidColorQuad(gfx::Rect(5, 5), SkColors ::kGreen))
          .AddRenderPass(
              RenderPassBuilder(CompositorRenderPassId{2}, kSurfaceSize)
                  .AddRenderPassQuad(gfx::Rect(kSurfaceSize),
                                     CompositorRenderPassId{1}))
          .Build();

  root_sink_->SubmitCompositorFrame(root_surface_id_.local_surface_id(),
                                    std::move(frame));

  SurfaceId surface_id(root_sink_->frame_sink_id(),
                       root_surface_id_.local_surface_id());
  auto aggregated_frame = AggregateFrame(surface_id);
  EXPECT_EQ(2u, aggregated_frame.render_pass_list.size());
}

namespace {

// Blocks until `OnScreenshotCaptured()` is called.
class OnScreenshotCapturedWaiter : public mojom::FrameSinkManagerClient {
 public:
  OnScreenshotCapturedWaiter() = default;
  ~OnScreenshotCapturedWaiter() override = default;
  OnScreenshotCapturedWaiter(const OnScreenshotCapturedWaiter&) = delete;
  OnScreenshotCapturedWaiter& operator=(const OnScreenshotCapturedWaiter&) =
      delete;

  // mojom::FrameSinkManagerClient:
  void OnFirstSurfaceActivation(const SurfaceInfo&) override {}
  void OnFrameTokenChanged(const FrameSinkId&,
                           uint32_t,
                           base::TimeTicks) override {}
  void OnAggregatedHitTestRegionListUpdated(
      const FrameSinkId& frame_sink_id,
      const std::vector<AggregatedHitTestRegion>& hit_test_data) override {}
#if BUILDFLAG(IS_ANDROID)
  void VerifyThreadIdsDoNotBelongToHost(
      const std::vector<int32_t>& thread_ids,
      VerifyThreadIdsDoNotBelongToHostCallback callback) override {}
#endif
  void OnScreenshotCaptured(
      const blink::SameDocNavigationScreenshotDestinationToken&
          destination_token,
      std::unique_ptr<CopyOutputResult> copy_output_result) override {
    observed_token_ = destination_token;
    run_loop_.Quit();
  }

  void Wait() { run_loop_.Run(); }

  const blink::SameDocNavigationScreenshotDestinationToken& observed_token() {
    return observed_token_;
  }

 private:
  blink::SameDocNavigationScreenshotDestinationToken observed_token_;
  base::RunLoop run_loop_;
};

class SurfaceAggregatorCopyRequestAgainstPreviousSurfaceTest
    : public SurfaceAggregatorValidSurfaceTest,
      public ::testing::WithParamInterface<bool> {
 public:
  SurfaceAggregatorCopyRequestAgainstPreviousSurfaceTest() = default;
  ~SurfaceAggregatorCopyRequestAgainstPreviousSurfaceTest() override = default;

  bool DestroyFrameSinkBeforeResult() { return GetParam(); }
};

std::string DescribeParam(const ::testing::TestParamInfo<bool>& info) {
  if (info.param) {
    return "CompositorFrameSinkSupportDestroyedBeforeResult";
  } else {
    return "CopyResultSent";
  }
}

}  // namespace

TEST_P(SurfaceAggregatorCopyRequestAgainstPreviousSurfaceTest,
       CopyAgainstPreviousSurface) {
  OnScreenshotCapturedWaiter waiter;
  manager_.SetLocalClient(&waiter);

  TestSurfaceIdAllocator child_allocator(child_sink_->frame_sink_id());
  SurfaceId prev_sid = child_allocator.Get();
  child_allocator.Increment();
  SurfaceId current_sid = child_allocator.Get();

  TestSurfaceIdAllocator root_allocator(root_sink_->frame_sink_id());
  SurfaceId root_sid = root_allocator.Get();

  // Submit one frame against the previous child surface.
  {
    SCOPED_TRACE("previous surface");
    CompositorFrame new_frame = MakeEmptyCompositorFrame();
    std::vector<Quad> quads = {
        Quad::SolidColorQuad(SkColors::kGreen, gfx::Rect(5, 5))};
    std::vector<Pass> passes = {
        Pass(quads, CompositorRenderPassId{1}, gfx::Size(100, 100))};
    AddPasses(&new_frame.render_pass_list, passes,
              &new_frame.metadata.referenced_surfaces);
    child_sink_->SubmitCompositorFrame(prev_sid.local_surface_id(),
                                       std::move(new_frame));
  }

  // Submit a frame against the root surface.
  {
    SCOPED_TRACE("root surface -> previous surface");
    CompositorFrame new_frame = MakeEmptyCompositorFrame();
    // The previous surface is reachable from the root surface.
    new_frame.metadata.referenced_surfaces = {SurfaceRange(prev_sid)};
    std::vector<Quad> quads = {
        Quad::SolidColorQuad(SkColors::kBlue, gfx::Rect(5, 5))};
    std::vector<Pass> passes = {
        Pass(quads, CompositorRenderPassId{2}, gfx::Size(100, 100))};
    AddPasses(&new_frame.render_pass_list, passes,
              &new_frame.metadata.referenced_surfaces);
    root_sink_->SubmitCompositorFrame(root_sid.local_surface_id(),
                                      std::move(new_frame));
  }

  // Activate the previous surface, and removes previous surface's temporary
  // reference.
  std::ignore = AggregateFrame(root_sid);

  // A new frame against the root surface and aggregate. This removes the
  // reference from root to the previous surface, but make the new surface
  // reachable.
  {
    SCOPED_TRACE("root surface -> current surface");
    CompositorFrame new_frame = MakeEmptyCompositorFrame();
    new_frame.metadata.referenced_surfaces = {SurfaceRange(current_sid)};
    std::vector<Quad> quads = {
        Quad::SolidColorQuad(SkColors::kBlue, gfx::Rect(5, 5))};
    std::vector<Pass> passes = {
        Pass(quads, CompositorRenderPassId{3}, gfx::Size(100, 100))};
    AddPasses(&new_frame.render_pass_list, passes,
              &new_frame.metadata.referenced_surfaces);
    root_sink_->SubmitCompositorFrame(root_sid.local_surface_id(),
                                      std::move(new_frame));
  }

  // Another frame against the current child surface, with the CopyOutputRequest
  // destination.
  const auto expected_token = base::UnguessableToken::Create();
  {
    SCOPED_TRACE("current surface with a COR");
    CompositorFrame new_frame = MakeEmptyCompositorFrame();
    new_frame.metadata.screenshot_destination =
        blink::SameDocNavigationScreenshotDestinationToken(expected_token);
    std::vector<Quad> quads = {
        Quad::SolidColorQuad(SkColors::kRed, gfx::Rect(5, 5))};
    std::vector<Pass> passes = {
        Pass(quads, CompositorRenderPassId{4}, gfx::Size(100, 100))};
    AddPasses(&new_frame.render_pass_list, passes,
              &new_frame.metadata.referenced_surfaces);
    child_sink_->SubmitCompositorFrame(current_sid.local_surface_id(),
                                       std::move(new_frame));
  }

  // Check that the current child surface has `pending_copy_surface_id_` set.
  ASSERT_EQ(manager_.surface_manager()
                ->GetSurfaceForId(current_sid)
                ->pending_copy_surface_id_for_testing(),
            prev_sid);

  // Check the references.
  ASSERT_THAT(
      manager_.surface_manager()->GetSurfacesReferencedByParent(current_sid),
      ::testing::UnorderedElementsAre(prev_sid));
  ASSERT_THAT(
      manager_.surface_manager()->GetSurfacesThatReferenceChildForTesting(
          prev_sid),
      ::testing::UnorderedElementsAre(current_sid));

  // Check that the CopyOutputRequest is taken during aggregation.
  auto result = AggregateFrame(root_sid);
  ASSERT_TRUE(result.has_copy_requests);
  ASSERT_EQ(result.render_pass_list.size(), 2U);
  ASSERT_EQ(result.render_pass_list[0]->copy_requests.size(), 1U);

  if (DestroyFrameSinkBeforeResult()) {
    child_sink_.reset();

    // The destruction of the frame sink doesn't remove the reference.
    ASSERT_EQ(manager_.surface_manager()
                  ->GetSurfaceForId(current_sid)
                  ->pending_copy_surface_id_for_testing(),
              prev_sid);
    ASSERT_THAT(
        manager_.surface_manager()->GetSurfacesReferencedByParent(current_sid),
        ::testing::UnorderedElementsAre(prev_sid));
    ASSERT_THAT(
        manager_.surface_manager()->GetSurfacesThatReferenceChildForTesting(
            prev_sid),
        ::testing::UnorderedElementsAre(current_sid));

    // The destruction of `current_sid` removes the reference.
    {
      SCOPED_TRACE("deref current surface from root");
      CompositorFrame new_frame = MakeEmptyCompositorFrame();
      std::vector<Quad> quads = {
          Quad::SolidColorQuad(SkColors::kBlue, gfx::Rect(5, 5))};
      std::vector<Pass> passes = {
          Pass(quads, CompositorRenderPassId{3}, gfx::Size(100, 100))};
      AddPasses(&new_frame.render_pass_list, passes,
                &new_frame.metadata.referenced_surfaces);
      root_sink_->SubmitCompositorFrame(root_sid.local_surface_id(),
                                        std::move(new_frame));
    }
    manager_.surface_manager()->GarbageCollectSurfaces();

    ASSERT_FALSE(manager_.surface_manager()->GetSurfaceForId(current_sid));
    ASSERT_FALSE(manager_.surface_manager()->GetSurfaceForId(prev_sid));
    ASSERT_TRUE(manager_.surface_manager()
                    ->GetSurfacesReferencedByParent(current_sid)
                    .empty());
    ASSERT_TRUE(manager_.surface_manager()
                    ->GetSurfacesThatReferenceChildForTesting(prev_sid)
                    .empty());
  } else {
    auto empty_result = std::make_unique<CopyOutputResult>(
        CopyOutputResult::Format::RGBA,
        CopyOutputResult::Destination::kSystemMemory, gfx::Rect(),
        /*needs_lock_for_bitmap=*/false);
    result.render_pass_list[0]->copy_requests[0]->SendResult(
        std::move(empty_result));
    {
      SCOPED_TRACE("Waiting for OnScreenshotCaptured()");
      waiter.Wait();
    }
    ASSERT_EQ(waiter.observed_token().value(), expected_token);
    ASSERT_FALSE(manager_.surface_manager()
                     ->GetSurfaceForId(current_sid)
                     ->pending_copy_surface_id_for_testing()
                     .is_valid());
    ASSERT_TRUE(manager_.surface_manager()
                    ->GetSurfacesReferencedByParent(current_sid)
                    .empty());
    ASSERT_TRUE(manager_.surface_manager()
                    ->GetSurfacesThatReferenceChildForTesting(prev_sid)
                    .empty());
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         SurfaceAggregatorCopyRequestAgainstPreviousSurfaceTest,
                         ::testing::Bool(),
                         &DescribeParam);

}  // namespace viz
