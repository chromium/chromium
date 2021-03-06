// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/surface_aggregator.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <map>
#include <set>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "cc/test/render_pass_test_utils.h"
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
#include "components/viz/common/quads/yuv_video_draw_quad.h"
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
#include "components/viz/test/compositor_frame_helpers.h"
#include "components/viz/test/fake_compositor_frame_sink_client.h"
#include "components/viz/test/fake_surface_observer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace viz {
namespace {

using ::testing::_;
constexpr FrameSinkId kArbitraryRootFrameSinkId(1, 1);
constexpr FrameSinkId kArbitraryFrameSinkId1(2, 2);
constexpr FrameSinkId kArbitraryFrameSinkId2(3, 3);
constexpr FrameSinkId kArbitraryMiddleFrameSinkId(4, 4);
constexpr FrameSinkId kArbitraryReservedFrameSinkId(5, 5);
constexpr FrameSinkId kArbitraryFrameSinkId3(6, 6);
const base::UnguessableToken kArbitraryToken =
    base::UnguessableToken::Deserialize(1, 2);
const base::UnguessableToken kArbitraryToken2 =
    base::UnguessableToken::Deserialize(3, 4);
const base::UnguessableToken kArbitraryToken3 =
    base::UnguessableToken::Deserialize(5, 6);
constexpr bool kRootIsRoot = true;
constexpr bool kChildIsRoot = false;

gfx::Size SurfaceSize() {
  static gfx::Size size(100, 100);
  return size;
}

gfx::Rect NoDamage() {
  return gfx::Rect();
}

class MockAggregatedDamageCallback {
 public:
  MockAggregatedDamageCallback() {}
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

  DISALLOW_COPY_AND_ASSIGN(MockAggregatedDamageCallback);
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
  base::TimeTicks next_display_time_ =
      base::TimeTicks() + base::TimeDelta::FromSeconds(1);
};

class SurfaceAggregatorTest : public testing::Test, public DisplayTimeSource {
 public:
  explicit SurfaceAggregatorTest(bool use_damage_rect)
      : manager_(&shared_bitmap_manager_),
        observer_(false),
        root_sink_(std::make_unique<CompositorFrameSinkSupport>(
            &fake_client_,
            &manager_,
            kArbitraryRootFrameSinkId,
            kRootIsRoot)),
        aggregator_(manager_.surface_manager(),
                    nullptr,
                    use_damage_rect,
                    true) {
    manager_.surface_manager()->AddObserver(&observer_);
  }

  SurfaceAggregatorTest() : SurfaceAggregatorTest(false) {}

  void TearDown() override {
    observer_.Reset();
    testing::Test::TearDown();
  }

  AggregatedFrame AggregateFrame(const SurfaceId& surface_id,
                                 gfx::Rect target_damage = gfx::Rect()) {
    return aggregator_.Aggregate(
        surface_id, GetNextDisplayTimeAndIncrement(),
        /*display_transform=*/gfx::OVERLAY_TRANSFORM_NONE, target_damage);
  }

  struct Quad {
    static Quad SolidColorQuad(SkColor color, const gfx::Rect& rect) {
      Quad quad;
      quad.material = DrawQuad::Material::kSolidColor;
      quad.color = color;
      quad.rect = rect;
      return quad;
    }

    static Quad YUVVideoQuad(const gfx::Rect& rect) {
      Quad quad;
      quad.material = DrawQuad::Material::kYuvVideoContent;
      quad.rect = rect;
      return quad;
    }

    // If |fallback_surface_id| is a valid surface Id then this will generate
    // two SurfaceDrawQuads.
    static Quad SurfaceQuad(const SurfaceRange& surface_range,
                            SkColor default_background_color,
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
                            SkColor default_background_color,
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

    DrawQuad::Material material;

    // Set when material==DrawQuad::Material::kSurfaceContent.
    SurfaceRange surface_range;
    SkColor default_background_color;
    bool stretch_content_to_fill_bounds;
    gfx::Rect primary_surface_rect;
    float opacity;
    gfx::Transform to_target_transform;
    gfx::MaskFilterInfo mask_filter_info;
    bool is_fast_rounded_corner = false;
    bool allow_merge = true;

    // Set when material==DrawQuad::Material::kSolidColor.
    SkColor color;
    gfx::Rect rect;

    // Set when material==DrawQuad::Material::kCompositorRenderPass.
    CompositorRenderPassId render_pass_id;
    gfx::Transform transform;
    bool intersects_damage_under = true;

   private:
    Quad()
        : material(DrawQuad::Material::kInvalid),
          opacity(1.f),
          color(SK_ColorWHITE) {}
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

    const std::vector<Quad>& quads;
    CompositorRenderPassId id{1};
    gfx::Rect output_rect;
    gfx::Rect damage_rect;
    bool has_transparent_background = true;
  };

  // |referenced_surfaces| refers to the SurfaceRanges of all the
  // SurfaceDrawQuads added to the provided |pass|.
  static void AddQuadInPass(const Quad& desc,
                            CompositorRenderPass* pass,
                            std::vector<SurfaceRange>* referenced_surfaces) {
    switch (desc.material) {
      case DrawQuad::Material::kSolidColor:
        cc::AddQuad(pass, desc.rect, desc.color);
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
      case DrawQuad::Material::kYuvVideoContent:
        AddYUVVideoQuad(pass, desc.rect);
        break;
      default:
        NOTREACHED();
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
      for (size_t j = 0; j < pass.quads.size(); ++j)
        AddQuadInPass(pass.quads[j], test_pass, referenced_surfaces);
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
        NOTREACHED();
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
                             SkColor default_background_color,
                             bool stretch_content_to_fill_bounds,
                             const gfx::MaskFilterInfo& mask_filter_info,
                             bool is_fast_rounded_corner,
                             bool allow_merge) {
    gfx::Transform layer_to_target_transform = transform;
    gfx::Rect layer_bounds(primary_surface_rect);
    gfx::Rect visible_layer_rect(primary_surface_rect);
    gfx::Rect clip_rect(primary_surface_rect);
    bool is_clipped = false;
    bool are_contents_opaque = false;
    SkBlendMode blend_mode = SkBlendMode::kSrcOver;

    auto* shared_quad_state = pass->CreateAndAppendSharedQuadState();
    shared_quad_state->SetAll(layer_to_target_transform, layer_bounds,
                              visible_layer_rect, mask_filter_info, clip_rect,
                              is_clipped, are_contents_opaque, opacity,
                              blend_mode, 0);
    shared_quad_state->is_fast_rounded_corner = is_fast_rounded_corner;

    SurfaceDrawQuad* surface_quad =
        pass->CreateAndAppendDrawQuad<SurfaceDrawQuad>();
    surface_quad->SetAll(
        pass->shared_quad_state_list.back(), primary_surface_rect,
        primary_surface_rect,
        /*needs_blending=*/true, surface_range, default_background_color,
        stretch_content_to_fill_bounds, /*is_reflection=*/false, allow_merge);
  }

  static void AddRenderPassQuad(CompositorRenderPass* pass,
                                CompositorRenderPassId render_pass_id,
                                const gfx::Transform& transform,
                                bool intersects_damage_under) {
    gfx::Rect output_rect = gfx::Rect(0, 0, 5, 5);
    auto* shared_state = pass->CreateAndAppendSharedQuadState();
    shared_state->SetAll(transform, output_rect, output_rect,
                         gfx::MaskFilterInfo(), output_rect, false, false, 1,
                         SkBlendMode::kSrcOver, 0);
    auto* quad = pass->CreateAndAppendDrawQuad<CompositorRenderPassDrawQuad>();
    quad->SetAll(shared_state, output_rect, output_rect,
                 /*needs_blending=*/true, render_pass_id, kInvalidResourceId,
                 gfx::RectF(), gfx::Size(), gfx::Vector2dF(), gfx::PointF(),
                 gfx::RectF(),
                 /*force_anti_aliasing_off=*/false,
                 /*backdrop_filter_quality=*/1.0f, intersects_damage_under);
  }

  static void AddYUVVideoQuad(CompositorRenderPass* pass,
                              const gfx::Rect& output_rect) {
    auto* shared_state = pass->CreateAndAppendSharedQuadState();
    shared_state->SetAll(gfx::Transform(), output_rect, output_rect,
                         gfx::MaskFilterInfo(), output_rect, false, false, 1,
                         SkBlendMode::kSrcOver, 0);
    auto* quad = pass->CreateAndAppendDrawQuad<YUVVideoDrawQuad>();
    quad->SetNew(shared_state, output_rect, output_rect, false,
                 gfx::RectF(output_rect), gfx::RectF(), output_rect.size(),
                 gfx::Size(), kInvalidResourceId, kInvalidResourceId,
                 kInvalidResourceId, kInvalidResourceId,
                 gfx::ColorSpace::CreateREC709(), 0, 1.0, 8);
  }

 protected:
  ServerSharedBitmapManager shared_bitmap_manager_;
  FrameSinkManagerImpl manager_;
  FakeSurfaceObserver observer_;
  FakeCompositorFrameSinkClient fake_client_;
  std::unique_ptr<CompositorFrameSinkSupport> root_sink_;
  SurfaceAggregator aggregator_;
};

class SurfaceAggregatorValidSurfaceTest : public SurfaceAggregatorTest {
 public:
  explicit SurfaceAggregatorValidSurfaceTest(bool use_damage_rect)
      : SurfaceAggregatorTest(use_damage_rect),
        child_sink_(std::make_unique<CompositorFrameSinkSupport>(
            nullptr,
            &manager_,
            kArbitraryReservedFrameSinkId,
            kChildIsRoot)) {
    child_sink_->set_allow_copy_output_requests_for_testing();
  }

  SurfaceAggregatorValidSurfaceTest()
      : SurfaceAggregatorValidSurfaceTest(false) {}

  void SetUp() override {
    SurfaceAggregatorTest::SetUp();
    root_allocator_.GenerateId();
    root_local_surface_id_ = root_allocator_.GetCurrentLocalSurfaceId();
    root_surface_ = manager_.surface_manager()->GetSurfaceForId(
        SurfaceId(root_sink_->frame_sink_id(), root_local_surface_id_));
  }

  void TearDown() override { SurfaceAggregatorTest::TearDown(); }

  // Verifies that if the |SharedQuadState::quad_layer_rect| can be covered by
  // |DrawQuad::Rect| in the SharedQuadState.
  void VerifyQuadCoverSQS(AggregatedFrame* aggregated_frame) {
    const SharedQuadState* shared_quad_state = nullptr;
    gfx::Rect draw_quad_coverage;
    for (size_t i = 0; i < aggregated_frame->render_pass_list.size(); ++i) {
      for (auto quad =
               aggregated_frame->render_pass_list[i]->quad_list.cbegin();
           quad != aggregated_frame->render_pass_list[i]->quad_list.cend();
           ++quad) {
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
    SurfaceId root_surface_id(root_sink_->frame_sink_id(),
                              root_local_surface_id_);
    auto aggregated_frame = AggregateFrame(root_surface_id);

    TestPassesMatchExpectations(expected_passes,
                                &aggregated_frame.render_pass_list);
    VerifyQuadCoverSQS(&aggregated_frame);

    // Ensure no duplicate pass ids output.
    std::set<AggregatedRenderPassId> used_passes;
    for (const auto& pass : aggregated_frame.render_pass_list)
      EXPECT_TRUE(used_passes.insert(pass->id).second);

    EXPECT_EQ(expected_surface_ids.size(),
              aggregator_.previous_contained_surfaces().size());
    for (const SurfaceId& surface_id : expected_surface_ids) {
      EXPECT_THAT(aggregator_.previous_contained_surfaces(),
                  testing::Contains(testing::Key(surface_id)));
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

  CompositorFrame MakeCompositorFrameFromSurfaceRanges(
      const std::vector<SurfaceRange>& ranges) {
    std::vector<Quad> quads;
    for (const SurfaceRange& range : ranges) {
      quads.push_back(Quad::SurfaceQuad(
          range, SK_ColorWHITE, gfx::Rect(5, 5), 1.f, gfx::Transform(),
          /*stretch_content_to_fill_bounds=*/false, gfx::MaskFilterInfo(),
          /*is_fast_rounded_corner=*/false));
    }
    std::vector<Pass> passes = {Pass(quads, SurfaceSize())};
    CompositorRenderPassList pass_list;
    std::vector<SurfaceRange> referenced_surfaces;
    AddPasses(&pass_list, passes, &referenced_surfaces);
    return CompositorFrameBuilder()
        .SetRenderPassList(std::move(pass_list))
        .SetDeviceScaleFactor(1.f)
        .SetReferencedSurfaces(ranges)
        .Build();
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
  LocalSurfaceId root_local_surface_id_;
  Surface* root_surface_;
  ParentLocalSurfaceIdAllocator root_allocator_;
  std::unique_ptr<CompositorFrameSinkSupport> child_sink_;
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
  std::vector<Quad> quads = {
      Quad::SolidColorQuad(SK_ColorRED, gfx::Rect(5, 5)),
      Quad::SolidColorQuad(SK_ColorBLUE, gfx::Rect(5, 5))};
  std::vector<Pass> passes = {Pass(quads, SurfaceSize())};

  // Add a callback for when the surface is damaged.
  MockAggregatedDamageCallback aggregated_damage_callback;
  root_sink_->SetAggregatedDamageCallbackForTesting(
      aggregated_damage_callback.GetCallback());

  constexpr float device_scale_factor = 1.0f;
  SubmitCompositorFrame(root_sink_.get(), passes, root_local_surface_id_,
                        device_scale_factor);

  SurfaceId root_surface_id(root_sink_->frame_sink_id(),
                            root_local_surface_id_);

  // Check that the AggregatedDamageCallback is called with the right arguments.
  EXPECT_CALL(
      aggregated_damage_callback,
      OnAggregatedDamage(root_local_surface_id_, SurfaceSize(),
                         gfx::Rect(SurfaceSize()), next_display_time()));

  AggregateAndVerify(passes, {root_surface_id});

  testing::Mock::VerifyAndClearExpectations(&aggregated_damage_callback);
}

// Test that when surface is translucent and we need the render surface to apply
// the opacity, we would keep the render surface.
TEST_F(SurfaceAggregatorValidSurfaceTest, OpacityCopied) {
  auto embedded_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryFrameSinkId1, kRootIsRoot);
  ParentLocalSurfaceIdAllocator embedded_allocator;
  embedded_allocator.GenerateId();
  LocalSurfaceId embedded_local_surface_id =
      embedded_allocator.GetCurrentLocalSurfaceId();
  SurfaceId embedded_surface_id(embedded_support->frame_sink_id(),
                                embedded_local_surface_id);

  std::vector<Quad> embedded_quads = {
      Quad::SolidColorQuad(SK_ColorGREEN, gfx::Rect(5, 5)),
      Quad::SolidColorQuad(SK_ColorBLUE, gfx::Rect(5, 5))};
  std::vector<Pass> embedded_passes = {Pass(embedded_quads, SurfaceSize())};

  constexpr float device_scale_factor = 1.0f;
  SubmitCompositorFrame(embedded_support.get(), embedded_passes,
                        embedded_local_surface_id, device_scale_factor);
  SurfaceId root_surface_id(root_sink_->frame_sink_id(),
                            root_local_surface_id_);
  {
    std::vector<Quad> quads = {Quad::SurfaceQuad(
        SurfaceRange(base::nullopt, embedded_surface_id), SK_ColorWHITE,
        gfx::Rect(5, 5), .5f, gfx::Transform(),
        /*stretch_content_to_fill_bounds=*/false, gfx::MaskFilterInfo(),
        /*is_fast_rounded_corner=*/false)};
    std::vector<Pass> passes = {Pass(quads, SurfaceSize())};

    SubmitCompositorFrame(root_sink_.get(), passes, root_local_surface_id_,
                          device_scale_factor);

    auto aggregated_frame = AggregateFrame(root_surface_id);

    auto& render_pass_list = aggregated_frame.render_pass_list;
    EXPECT_EQ(2u, render_pass_list.size());

    auto& shared_quad_state_list2 = render_pass_list[1]->shared_quad_state_list;
    ASSERT_EQ(1u, shared_quad_state_list2.size());
    EXPECT_EQ(.5f, shared_quad_state_list2.ElementAt(0)->opacity);
  }

  // For the case where opacity is close to 1.f, we treat it as opaque, and not
  // use a render surface.
  {
    std::vector<Quad> quads = {Quad::SurfaceQuad(
        SurfaceRange(base::nullopt, embedded_surface_id), SK_ColorWHITE,
        gfx::Rect(5, 5), .9999f, gfx::Transform(),
        /*stretch_content_to_fill_bounds=*/false, gfx::MaskFilterInfo(),
        /*is_fast_rounded_corner=*/false)};
    std::vector<Pass> passes = {Pass(quads, SurfaceSize())};

    SubmitCompositorFrame(root_sink_.get(), passes, root_local_surface_id_,
                          device_scale_factor);

    auto aggregated_frame = AggregateFrame(root_surface_id);

    auto& render_pass_list = aggregated_frame.render_pass_list;
    EXPECT_EQ(1u, render_pass_list.size());
  }
}

// Test that when surface is rotated and we need the render surface to apply the
// clip, we would keep the render surface.
TEST_F(SurfaceAggregatorValidSurfaceTest, RotatedClip) {
  auto embedded_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryFrameSinkId1, kRootIsRoot);
  ParentLocalSurfaceIdAllocator embedded_allocator;
  embedded_allocator.GenerateId();
  LocalSurfaceId embedded_local_surface_id =
      embedded_allocator.GetCurrentLocalSurfaceId();
  SurfaceId embedded_surface_id(embedded_support->frame_sink_id(),
                                embedded_local_surface_id);

  std::vector<Quad> embedded_quads = {
      Quad::SolidColorQuad(SK_ColorGREEN, gfx::Rect(5, 5)),
      Quad::SolidColorQuad(SK_ColorBLUE, gfx::Rect(5, 5))};
  std::vector<Pass> embedded_passes = {Pass(embedded_quads, SurfaceSize())};

  constexpr float device_scale_factor = 1.0f;
  SubmitCompositorFrame(embedded_support.get(), embedded_passes,
                        embedded_local_surface_id, device_scale_factor);
  gfx::Transform rotate;
  rotate.Rotate(30);
  std::vector<Quad> quads = {Quad::SurfaceQuad(
      SurfaceRange(base::nullopt, embedded_surface_id), SK_ColorWHITE,
      gfx::Rect(5, 5), 1.f, rotate,
      /*stretch_content_to_fill_bounds=*/false, gfx::MaskFilterInfo(),
      /*is_fast_rounded_corner=*/false)};
  std::vector<Pass> passes = {Pass(quads, SurfaceSize())};

  SubmitCompositorFrame(root_sink_.get(), passes, root_local_surface_id_,
                        device_scale_factor);

  SurfaceId root_surface_id(root_sink_->frame_sink_id(),
                            root_local_surface_id_);
  auto aggregated_frame = AggregateFrame(root_surface_id);

  auto& render_pass_list = aggregated_frame.render_pass_list;
  EXPECT_EQ(2u, render_pass_list.size());

  auto& shared_quad_state_list2 =
      render_pass_list.back()->shared_quad_state_list;
  EXPECT_EQ(rotate, shared_quad_state_list2.front()->quad_to_target_transform);
}

TEST_F(SurfaceAggregatorValidSurfaceTest, MultiPassSimpleFrame) {
  std::vector<Quad> quads[2] = {
      {Quad::SolidColorQuad(SK_ColorWHITE, gfx::Rect(5, 5)),
       Quad::SolidColorQuad(SK_ColorLTGRAY, gfx::Rect(5, 5))},
      {Quad::SolidColorQuad(SK_ColorGRAY, gfx::Rect(5, 5)),
       Quad::SolidColorQuad(SK_ColorDKGRAY, gfx::Rect(5, 5))}};
  std::vector<Pass> passes = {
      Pass(quads[0], CompositorRenderPassId{1}, SurfaceSize()),
      Pass(quads[1], CompositorRenderPassId{2}, SurfaceSize())};

  constexpr float device_scale_factor = 1.0f;

  SubmitCompositorFrame(root_sink_.get(), passes, root_local_surface_id_,
                        device_scale_factor);

  SurfaceId root_surface_id(root_sink_->frame_sink_id(),
                            root_local_surface_id_);

  AggregateAndVerify(passes, {root_surface_id});
}

// Ensure that the render pass ID map properly keeps and deletes entries.
TEST_F(SurfaceAggregatorValidSurfaceTest, MultiPassDeallocation) {
  std::vector<Quad> quads[2] = {
      {Quad::SolidColorQuad(SK_ColorWHITE, gfx::Rect(5, 5)),
       Quad::SolidColorQuad(SK_ColorLTGRAY, gfx::Rect(5, 5))},
      {Quad::SolidColorQuad(SK_ColorGRAY, gfx::Rect(5, 5)),
       Quad::SolidColorQuad(SK_ColorDKGRAY, gfx::Rect(5, 5))}};
  std::vector<Pass> passes = {
      Pass(quads[0], CompositorRenderPassId{2}, SurfaceSize()),
      Pass(quads[1], CompositorRenderPassId{1}, SurfaceSize())};

  constexpr float device_scale_factor = 1.0f;
  SubmitCompositorFrame(root_sink_.get(), passes, root_local_surface_id_,
                        device_scale_factor);

  SurfaceId surface_id(root_sink_->frame_sink_id(), root_local_surface_id_);

  auto aggregated_frame = AggregateFrame(surface_id);
  auto id0 = aggregated_frame.render_pass_list[0]->id;
  auto id1 = aggregated_frame.render_pass_list[1]->id;
  EXPECT_NE(id1, id0);

  // Aggregated RenderPass ids should remain the same between frames.
  aggregated_frame = AggregateFrame(surface_id);
  EXPECT_EQ(id0, aggregated_frame.render_pass_list[0]->id);
  EXPECT_EQ(id1, aggregated_frame.render_pass_list[1]->id);

  std::vector<Pass> passes2 = {
      Pass(quads[0], CompositorRenderPassId{3}, SurfaceSize()),
      Pass(quads[1], CompositorRenderPassId{1}, SurfaceSize())};

  SubmitCompositorFrame(root_sink_.get(), passes2, root_local_surface_id_,
                        device_scale_factor);

  // The RenderPass that still exists should keep the same ID.
  aggregated_frame = AggregateFrame(surface_id);
  auto id2 = aggregated_frame.render_pass_list[0]->id;
  EXPECT_NE(id2, id1);
  EXPECT_NE(id2, id0);
  EXPECT_EQ(id1, aggregated_frame.render_pass_list[1]->id);

  SubmitCompositorFrame(root_sink_.get(), passes, root_local_surface_id_,
                        device_scale_factor);

  // |id1| didn't exist in the previous frame, so it should be
  // mapped to a new ID.
  aggregated_frame = AggregateFrame(surface_id);
  auto id3 = aggregated_frame.render_pass_list[0]->id;
  EXPECT_NE(id3, id2);
  EXPECT_NE(id3, id1);
  EXPECT_NE(id3, id0);
  EXPECT_EQ(id1, aggregated_frame.render_pass_list[1]->id);
}

// This tests very simple embedding. root_surface has a frame containing a few
// solid color quads and a surface quad referencing embedded_surface.
// embedded_surface has a frame containing only a solid color quad. The solid
// color quad should be aggregated into the final frame.
TEST_F(SurfaceAggregatorValidSurfaceTest, SimpleSurfaceReference) {
  auto embedded_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryFrameSinkId1, kRootIsRoot);
  ParentLocalSurfaceIdAllocator embedded_allocator;
  embedded_allocator.GenerateId();
  LocalSurfaceId embedded_local_surface_id =
      embedded_allocator.GetCurrentLocalSurfaceId();
  SurfaceId embedded_surface_id(embedded_support->frame_sink_id(),
                                embedded_local_surface_id);

  std::vector<Quad> embedded_quads = {
      Quad::SolidColorQuad(SK_ColorGREEN, gfx::Rect(5, 5))};
  std::vector<Pass> embedded_passes = {Pass(embedded_quads, SurfaceSize())};

  constexpr float device_scale_factor = 1.0f;
  SubmitCompositorFrame(embedded_support.get(), embedded_passes,
                        embedded_local_surface_id, device_scale_factor);

  std::vector<Quad> root_quads = {
      Quad::SolidColorQuad(SK_ColorWHITE, gfx::Rect(5, 5)),
      Quad::SurfaceQuad(SurfaceRange(base::nullopt, embedded_surface_id),
                        SK_ColorWHITE, gfx::Rect(5, 5),
                        /*stretch_content_to_fill_bounds=*/false),
      Quad::SolidColorQuad(SK_ColorBLACK, gfx::Rect(5, 5))};
  std::vector<Pass> root_passes = {Pass(root_quads, SurfaceSize())};

  SubmitCompositorFrame(root_sink_.get(), root_passes, root_local_surface_id_,
                        device_scale_factor);

  std::vector<Quad> expected_quads = {
      Quad::SolidColorQuad(SK_ColorWHITE, gfx::Rect(5, 5)),
      Quad::SolidColorQuad(SK_ColorGREEN, gfx::Rect(5, 5)),
      Quad::SolidColorQuad(SK_ColorBLACK, gfx::Rect(5, 5))};
  std::vector<Pass> expected_passes = {Pass(expected_quads, SurfaceSize())};
  SurfaceId root_surface_id(root_sink_->frame_sink_id(),
                            root_local_surface_id_);
  AggregateAndVerify(expected_passes, {root_surface_id, embedded_surface_id});
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

  ~TestVizClient() = default;

  Surface* GetSurface() const {
    return manager_->surface_manager()->GetSurfaceForId(
        SurfaceId(frame_sink_id_, local_surface_id()));
  }

  void SubmitCompositorFrame(SkColor bgcolor) {
    using Quad = SurfaceAggregatorValidSurfaceTest::Quad;
    using Pass = SurfaceAggregatorValidSurfaceTest::Pass;

    std::vector<SurfaceRange> referenced_surfaces;
    std::vector<Quad> embedded_quads = {Quad::SolidColorQuad(bgcolor, bounds_)};
    for (const auto& embed : embedded_clients_) {
      if (embed.second) {
        embedded_quads.push_back(Quad::SurfaceQuad(
            SurfaceRange(base::nullopt, embed.first->surface_id()),
            SK_ColorWHITE, embed.first->bounds(),
            /*stretch_content_to_fill_bounds=*/false));
      } else {
        referenced_surfaces.emplace_back(
            SurfaceRange(base::nullopt, embed.first->surface_id()));
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
  SurfaceAggregatorValidSurfaceTest* const test_;
  FrameSinkManagerImpl* const manager_;
  std::unique_ptr<CompositorFrameSinkSupport> root_sink_;
  const FrameSinkId frame_sink_id_;
  const gfx::Rect bounds_;
  ParentLocalSurfaceIdAllocator allocator_;

  std::map<TestVizClient*, bool> embedded_clients_;

  DISALLOW_COPY_AND_ASSIGN(TestVizClient);
};

TEST_F(SurfaceAggregatorValidSurfaceTest, UndrawnSurfaces) {
  TestVizClient child(this, &manager_, kArbitraryFrameSinkId1,
                      gfx::Rect(10, 10));
  child.SubmitCompositorFrame(SK_ColorBLUE);

  // Parent first submits a CompositorFrame that renfereces |child|, but does
  // not provide a DrawQuad that embeds it.
  TestVizClient parent(this, &manager_, kArbitraryFrameSinkId2,
                       gfx::Rect(15, 15));
  parent.SetEmbeddedClient(&child, false);
  parent.SubmitCompositorFrame(SK_ColorGREEN);

  std::vector<Quad> root_quads = {
      Quad::SolidColorQuad(SK_ColorWHITE, gfx::Rect(5, 5)),
      Quad::SurfaceQuad(SurfaceRange(base::nullopt, parent.surface_id()),
                        SK_ColorWHITE, parent.bounds(),
                        /*stretch_content_to_fill_bounds=*/false),
      Quad::SolidColorQuad(SK_ColorBLACK, gfx::Rect(5, 5))};
  std::vector<Pass> root_passes = {Pass(root_quads, SurfaceSize())};

  constexpr float device_scale_factor = 1.0f;
  SubmitCompositorFrame(root_sink_.get(), root_passes, root_local_surface_id_,
                        device_scale_factor);

  std::vector<Quad> expected_quads = {
      Quad::SolidColorQuad(SK_ColorWHITE, gfx::Rect(5, 5)),
      Quad::SolidColorQuad(SK_ColorGREEN, parent.bounds()),
      Quad::SolidColorQuad(SK_ColorBLACK, gfx::Rect(5, 5))};
  std::vector<Pass> expected_passes = {Pass(expected_quads, SurfaceSize())};
  SurfaceId root_surface_id(root_sink_->frame_sink_id(),
                            root_local_surface_id_);
  AggregateAndVerify(expected_passes, {root_surface_id, parent.surface_id(),
                                       child.surface_id()});
  // |child| should not be drawn.
  EXPECT_TRUE(child.GetSurface()->HasUndrawnActiveFrame());
  EXPECT_FALSE(parent.GetSurface()->HasUndrawnActiveFrame());

  // Submit another CompositorFrame from |parent|, this time with a DrawQuad for
  // |child|.
  parent.SetEmbeddedClient(&child, true);
  parent.SubmitCompositorFrame(SK_ColorGREEN);

  expected_quads = {Quad::SolidColorQuad(SK_ColorWHITE, gfx::Rect(5, 5)),
                    Quad::SolidColorQuad(SK_ColorGREEN, parent.bounds()),
                    Quad::SolidColorQuad(SK_ColorBLUE, child.bounds()),
                    Quad::SolidColorQuad(SK_ColorBLACK, gfx::Rect(5, 5))};
  AggregateAndVerify(
      {Pass(expected_quads, SurfaceSize())},
      {root_surface_id, parent.surface_id(), child.surface_id()});
  EXPECT_FALSE(child.GetSurface()->HasUndrawnActiveFrame());
}

TEST_F(SurfaceAggregatorValidSurfaceTest, UndrawnSurfacesWithCopyRequests) {
  TestVizClient child(this, &manager_, kArbitraryFrameSinkId1,
                      gfx::Rect(10, 10));
  child.SubmitCompositorFrame(SK_ColorBLUE);
  child.RequestCopyOfOutput();

  // Parent first submits a CompositorFrame that renfereces |child|, but does
  // not provide a DrawQuad that embeds it.
  TestVizClient parent(this, &manager_, kArbitraryFrameSinkId2,
                       gfx::Rect(15, 15));
  parent.SetEmbeddedClient(&child, false);
  parent.SubmitCompositorFrame(SK_ColorGREEN);

  std::vector<Quad> root_quads = {
      Quad::SolidColorQuad(SK_ColorWHITE, gfx::Rect(5, 5)),
      Quad::SurfaceQuad(SurfaceRange(base::nullopt, parent.surface_id()),
                        SK_ColorWHITE, parent.bounds(),
                        /*stretch_content_to_fill_bounds=*/false),
      Quad::SolidColorQuad(SK_ColorBLACK, gfx::Rect(5, 5))};
  std::vector<Pass> root_passes = {Pass(root_quads, SurfaceSize())};

  constexpr float device_scale_factor = 1.0f;
  SubmitCompositorFrame(root_sink_.get(), root_passes, root_local_surface_id_,
                        device_scale_factor);

  std::vector<Quad> expected_quads = {
      Quad::SolidColorQuad(SK_ColorWHITE, gfx::Rect(5, 5)),
      Quad::SolidColorQuad(SK_ColorGREEN, parent.bounds()),
      Quad::SolidColorQuad(SK_ColorBLACK, gfx::Rect(5, 5))};
  std::vector<Quad> expected_copy_quads = {
      Quad::SolidColorQuad(SK_ColorBLUE, child.bounds())};
  std::vector<Pass> expected_passes = {Pass(expected_copy_quads, SurfaceSize()),
                                       Pass(expected_quads, SurfaceSize())};
  SurfaceId root_surface_id(root_sink_->frame_sink_id(),
                            root_local_surface_id_);
  AggregateAndVerify(expected_passes, {root_surface_id, parent.surface_id(),
                                       child.surface_id()});
  EXPECT_FALSE(child.GetSurface()->HasUndrawnActiveFrame());
  EXPECT_FALSE(parent.GetSurface()->HasUndrawnActiveFrame());
}

TEST_F(SurfaceAggregatorValidSurfaceTest,
       SurfacesWithMultipleEmbeddersBothVisibleAndInvisible) {
  TestVizClient child(this, &manager_, kArbitraryFrameSinkId1,
                      gfx::Rect(10, 10));
  child.SubmitCompositorFrame(SK_ColorBLUE);

  // First parent submits a CompositorFrame that renfereces |child|, but does
  // not provide a DrawQuad that embeds it.
  TestVizClient first_parent(this, &manager_, kArbitraryFrameSinkId2,
                             gfx::Rect(15, 15));
  first_parent.SetEmbeddedClient(&child, false);
  first_parent.SubmitCompositorFrame(SK_ColorGREEN);

  // Second parent submits a CompositorFrame referencing |child|, and also
  // includes a draw-quad for it.
  TestVizClient second_parent(this, &manager_, kArbitraryMiddleFrameSinkId,
                              gfx::Rect(25, 25));
  second_parent.SetEmbeddedClient(&child, true);
  second_parent.SubmitCompositorFrame(SK_ColorYELLOW);

  // Submit a root CompositorFrame that embeds both parents.
  std::vector<Quad> root_quads = {
      Quad::SolidColorQuad(SK_ColorWHITE, gfx::Rect(5, 5)),
      Quad::SurfaceQuad(SurfaceRange(base::nullopt, first_parent.surface_id()),
                        SK_ColorCYAN, first_parent.bounds(),
                        /*stretch_content_to_fill_bounds=*/false),
      Quad::SurfaceQuad(SurfaceRange(base::nullopt, second_parent.surface_id()),
                        SK_ColorMAGENTA, second_parent.bounds(),
                        /*stretch_content_to_fill_bounds=*/false),
      Quad::SolidColorQuad(SK_ColorBLACK, gfx::Rect(5, 5))};
  std::vector<Pass> root_passes = {Pass(root_quads, SurfaceSize())};

  constexpr float device_scale_factor = 1.0f;
  SubmitCompositorFrame(root_sink_.get(), root_passes, root_local_surface_id_,
                        device_scale_factor);

  EXPECT_TRUE(child.GetSurface()->HasUndrawnActiveFrame());
  EXPECT_TRUE(first_parent.GetSurface()->HasUndrawnActiveFrame());
  EXPECT_TRUE(second_parent.GetSurface()->HasUndrawnActiveFrame());

  std::vector<Quad> expected_quads = {
      Quad::SolidColorQuad(SK_ColorWHITE, gfx::Rect(5, 5)),
      Quad::SolidColorQuad(SK_ColorGREEN, first_parent.bounds()),
      Quad::SolidColorQuad(SK_ColorYELLOW, second_parent.bounds()),
      Quad::SolidColorQuad(SK_ColorBLUE, gfx::Rect(10, 10)),
      Quad::SolidColorQuad(SK_ColorBLACK, gfx::Rect(5, 5))};
  std::vector<Quad> expected_copy_quads = {};
  std::vector<Pass> expected_passes = {Pass(expected_quads, SurfaceSize())};
  SurfaceId root_surface_id(root_sink_->frame_sink_id(),
                            root_local_surface_id_);
  AggregateAndVerify(expected_passes,
                     {root_surface_id, first_parent.surface_id(),
                      second_parent.surface_id(), child.surface_id()});
  EXPECT_FALSE(child.GetSurface()->HasUndrawnActiveFrame());
  EXPECT_FALSE(first_parent.GetSurface()->HasUndrawnActiveFrame());
  EXPECT_FALSE(second_parent.GetSurface()->HasUndrawnActiveFrame());
}

// This test verifies that the appropriate transform will be applied to a
// surface embedded by a parent SurfaceDrawQuad marked as
// stretch_content_to_fill_bounds.
TEST_F(SurfaceAggregatorValidSurfaceTest, StretchContentToFillBounds) {
  auto primary_child_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryFrameSinkId1, kChildIsRoot);
  ParentLocalSurfaceIdAllocator child_allocator;
  child_allocator.GenerateId();
  LocalSurfaceId primary_child_local_surface_id =
      child_allocator.GetCurrentLocalSurfaceId();
  SurfaceId primary_child_surface_id(primary_child_support->frame_sink_id(),
                                     primary_child_local_surface_id);

  {
    auto pass = CompositorRenderPass::Create();
    pass->SetNew(CompositorRenderPassId{1}, gfx::Rect(0, 0, 20, 20),
                 gfx::Rect(), gfx::Transform());
    auto* sqs = pass->CreateAndAppendSharedQuadState();
    auto* solid_color_quad =
        pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();

    solid_color_quad->SetNew(sqs, gfx::Rect(0, 0, 20, 20),
                             gfx::Rect(0, 0, 20, 20), SK_ColorRED, false);

    CompositorFrame frame =
        CompositorFrameBuilder().AddRenderPass(std::move(pass)).Build();

    primary_child_support->SubmitCompositorFrame(primary_child_local_surface_id,
                                                 std::move(frame));
  }

  constexpr gfx::Rect surface_quad_rect(10, 5);
  std::vector<Quad> root_quads = {Quad::SurfaceQuad(
      SurfaceRange(primary_child_surface_id), SK_ColorWHITE, surface_quad_rect,
      /*stretch_content_to_fill_bounds=*/true)};
  std::vector<Pass> root_passes = {Pass(root_quads, SurfaceSize())};

  MockAggregatedDamageCallback aggregated_damage_callback;
  root_sink_->SetAggregatedDamageCallbackForTesting(
      aggregated_damage_callback.GetCallback());

  SurfaceId root_surface_id(root_sink_->frame_sink_id(),
                            root_local_surface_id_);
  SubmitCompositorFrame(root_sink_.get(), root_passes, root_local_surface_id_,
                        1.0f);

  EXPECT_CALL(
      aggregated_damage_callback,
      OnAggregatedDamage(root_local_surface_id_, SurfaceSize(),
                         gfx::Rect(SurfaceSize()), next_display_time()));
  auto frame = AggregateFrame(root_surface_id);
  testing::Mock::VerifyAndClearExpectations(&aggregated_damage_callback);

  EXPECT_EQ(1u, frame.render_pass_list.size());
  auto* render_pass = frame.render_pass_list.back().get();
  EXPECT_EQ(1u, render_pass->quad_list.size());

  auto* output_quad = render_pass->quad_list.back();

  EXPECT_EQ(DrawQuad::Material::kSolidColor, output_quad->material);
  gfx::RectF output_rect(100.f, 100.f);

  // SurfaceAggregator should stretch the SolidColorDrawQuad to fit the bounds
  // of the parent's SurfaceDrawQuad.
  output_quad->shared_quad_state->quad_to_target_transform.TransformRect(
      &output_rect);

  EXPECT_EQ(gfx::RectF(50.f, 25.f), output_rect);
}

// This test verifies that the appropriate transform will be applied to a
// surface embedded by a parent SurfaceDrawQuad marked as
// stretch_content_to_fill_bounds when the device_scale_factor is
// greater than 1.
TEST_F(SurfaceAggregatorValidSurfaceTest, StretchContentToFillStretchedBounds) {
  auto primary_child_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryFrameSinkId1, kChildIsRoot);
  ParentLocalSurfaceIdAllocator child_allocator;
  child_allocator.GenerateId();
  LocalSurfaceId primary_child_local_surface_id =
      child_allocator.GetCurrentLocalSurfaceId();
  SurfaceId primary_child_surface_id(primary_child_support->frame_sink_id(),
                                     primary_child_local_surface_id);

  {
    auto pass = CompositorRenderPass::Create();
    pass->SetNew(CompositorRenderPassId{1}, gfx::Rect(0, 0, 20, 20),
                 gfx::Rect(), gfx::Transform());
    auto* sqs = pass->CreateAndAppendSharedQuadState();
    auto* solid_color_quad =
        pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();

    solid_color_quad->SetNew(sqs, gfx::Rect(0, 0, 20, 20),
                             gfx::Rect(0, 0, 20, 20), SK_ColorRED, false);

    CompositorFrame frame =
        CompositorFrameBuilder().AddRenderPass(std::move(pass)).Build();

    primary_child_support->SubmitCompositorFrame(primary_child_local_surface_id,
                                                 std::move(frame));
  }

  constexpr gfx::Rect surface_quad_rect(10, 5);
  std::vector<Quad> root_quads = {Quad::SurfaceQuad(
      SurfaceRange(primary_child_surface_id), SK_ColorWHITE, surface_quad_rect,
      /*stretch_content_to_fill_bounds=*/true)};
  std::vector<Pass> root_passes = {Pass(root_quads, SurfaceSize())};

  MockAggregatedDamageCallback aggregated_damage_callback;
  root_sink_->SetAggregatedDamageCallbackForTesting(
      aggregated_damage_callback.GetCallback());

  SurfaceId root_surface_id(root_sink_->frame_sink_id(),
                            root_local_surface_id_);
  SubmitCompositorFrame(root_sink_.get(), root_passes, root_local_surface_id_,
                        2.0f);

  EXPECT_CALL(
      aggregated_damage_callback,
      OnAggregatedDamage(root_local_surface_id_, SurfaceSize(),
                         gfx::Rect(SurfaceSize()), next_display_time()));
  auto frame = AggregateFrame(root_surface_id);

  testing::Mock::VerifyAndClearExpectations(&aggregated_damage_callback);

  EXPECT_EQ(1u, frame.render_pass_list.size());
  auto* render_pass = frame.render_pass_list.back().get();
  EXPECT_EQ(1u, render_pass->quad_list.size());

  auto* output_quad = render_pass->quad_list.back();

  EXPECT_EQ(DrawQuad::Material::kSolidColor, output_quad->material);
  gfx::RectF output_rect(200.f, 200.f);

  // SurfaceAggregator should stretch the SolidColorDrawQuad to fit the bounds
  // of the parent's SurfaceDrawQuad.
  output_quad->shared_quad_state->quad_to_target_transform.TransformRect(
      &output_rect);

  EXPECT_EQ(gfx::RectF(100.f, 50.f), output_rect);
}

// This test verifies that the appropriate transform will be applied to a
// surface embedded by a parent SurfaceDrawQuad marked as
// stretch_content_to_fill_bounds when the device_scale_factor is
// less than 1.
TEST_F(SurfaceAggregatorValidSurfaceTest, StretchContentToFillSquashedBounds) {
  auto primary_child_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryFrameSinkId1, kChildIsRoot);
  ParentLocalSurfaceIdAllocator child_allocator;
  child_allocator.GenerateId();
  LocalSurfaceId primary_child_local_surface_id =
      child_allocator.GetCurrentLocalSurfaceId();
  SurfaceId primary_child_surface_id(primary_child_support->frame_sink_id(),
                                     primary_child_local_surface_id);

  {
    auto pass = CompositorRenderPass::Create();
    pass->SetNew(CompositorRenderPassId{1}, gfx::Rect(0, 0, 20, 20),
                 gfx::Rect(), gfx::Transform());
    auto* sqs = pass->CreateAndAppendSharedQuadState();
    auto* solid_color_quad =
        pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();

    solid_color_quad->SetNew(sqs, gfx::Rect(0, 0, 20, 20),
                             gfx::Rect(0, 0, 20, 20), SK_ColorRED, false);

    CompositorFrame frame =
        CompositorFrameBuilder().AddRenderPass(std::move(pass)).Build();

    primary_child_support->SubmitCompositorFrame(primary_child_local_surface_id,
                                                 std::move(frame));
  }

  constexpr gfx::Rect surface_quad_rect(10, 5);
  std::vector<Quad> root_quads = {Quad::SurfaceQuad(
      SurfaceRange(primary_child_surface_id), SK_ColorWHITE, surface_quad_rect,
      /*stretch_content_to_fill_bounds=*/true)};
  std::vector<Pass> root_passes = {Pass(root_quads, SurfaceSize())};

  MockAggregatedDamageCallback aggregated_damage_callback;
  root_sink_->SetAggregatedDamageCallbackForTesting(
      aggregated_damage_callback.GetCallback());

  SurfaceId root_surface_id(root_sink_->frame_sink_id(),
                            root_local_surface_id_);
  SubmitCompositorFrame(root_sink_.get(), root_passes, root_local_surface_id_,
                        0.5f);

  EXPECT_CALL(
      aggregated_damage_callback,
      OnAggregatedDamage(root_local_surface_id_, SurfaceSize(),
                         gfx::Rect(SurfaceSize()), next_display_time()));
  auto frame = AggregateFrame(root_surface_id);
  testing::Mock::VerifyAndClearExpectations(&aggregated_damage_callback);

  EXPECT_EQ(1u, frame.render_pass_list.size());
  auto* render_pass = frame.render_pass_list.back().get();
  EXPECT_EQ(1u, render_pass->quad_list.size());

  auto* output_quad = render_pass->quad_list.back();

  EXPECT_EQ(DrawQuad::Material::kSolidColor, output_quad->material);
  gfx::RectF output_rect(50.f, 50.f);

  // SurfaceAggregator should stretch the SolidColorDrawQuad to fit the bounds
  // of the parent's SurfaceDrawQuad.
  output_quad->shared_quad_state->quad_to_target_transform.TransformRect(
      &output_rect);

  EXPECT_EQ(gfx::RectF(25.f, 12.5f), output_rect);
}

// Verify that a reflected SurfaceDrawQuad with scaling won't have the surfaces
// root RenderPass merged with the RenderPass that embeds it. This ensures the
// reflected pixels can be scaled with AA enabled.
TEST_F(SurfaceAggregatorValidSurfaceTest, ReflectedSurfaceDrawQuadScaled) {
  const SurfaceId root_surface_id(root_sink_->frame_sink_id(),
                                  root_local_surface_id_);

  // Submit a CompositorFrame for the primary display. This will get mirrored
  // by the second display through surface embedding.
  const gfx::Rect display_rect(0, 0, 100, 100);
  {
    auto pass = CompositorRenderPass::Create();
    pass->SetNew(CompositorRenderPassId{1}, display_rect, display_rect,
                 gfx::Transform());
    auto* sqs = pass->CreateAndAppendSharedQuadState();
    sqs->opacity = 1.f;

    auto* solid_color_quad =
        pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
    solid_color_quad->SetNew(sqs, display_rect, display_rect, SK_ColorRED,
                             false);

    CompositorFrame frame =
        CompositorFrameBuilder().AddRenderPass(std::move(pass)).Build();
    root_sink_->SubmitCompositorFrame(root_surface_id.local_surface_id(),
                                      std::move(frame));
  }

  auto mirror_display_sink = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryFrameSinkId1, true);

  ParentLocalSurfaceIdAllocator lsi_allocator;
  lsi_allocator.GenerateId();
  LocalSurfaceId mirror_display_local_surface_id =
      lsi_allocator.GetCurrentLocalSurfaceId();

  // The mirroring display size is smaller than the primary display. The
  // mirrored content would be scaled to fit.
  const gfx::Rect mirror_display_rect(80, 80);
  gfx::Transform scale_transform;
  scale_transform.Scale(0.8, 0.8);

  {
    auto pass = CompositorRenderPass::Create();
    pass->SetNew(CompositorRenderPassId{1}, mirror_display_rect,
                 mirror_display_rect, gfx::Transform());
    auto* sqs = pass->CreateAndAppendSharedQuadState();
    sqs->quad_to_target_transform = scale_transform;
    sqs->opacity = 1.f;

    auto* surface_quad = pass->CreateAndAppendDrawQuad<SurfaceDrawQuad>();
    surface_quad->SetAll(sqs, display_rect, display_rect,
                         /*needs_blending=*/false,
                         SurfaceRange(base::nullopt, root_surface_id),
                         SK_ColorBLACK,
                         /*stretch_content_to_fill_bounds=*/true,
                         /*is_reflection=*/true,
                         /*allow_merge=*/true);

    CompositorFrame frame =
        CompositorFrameBuilder().AddRenderPass(std::move(pass)).Build();
    mirror_display_sink->SubmitCompositorFrame(mirror_display_local_surface_id,
                                               std::move(frame));
  }

  const SurfaceId mirror_display_surface_id(
      mirror_display_sink->frame_sink_id(), mirror_display_local_surface_id);
  auto frame = AggregateFrame(mirror_display_surface_id);

  // The reflected surface should be a separate RenderPass as it's scaled. The
  // root RenderPass should have a single CompositorRenderPassDrawQuad.
  EXPECT_EQ(2u, frame.render_pass_list.size());

  auto* root_render_pass = frame.render_pass_list.back().get();
  EXPECT_EQ(1u, root_render_pass->quad_list.size());

  auto* output_quad = root_render_pass->quad_list.back();
  EXPECT_EQ(DrawQuad::Material::kAggregatedRenderPass, output_quad->material);

  // The CompositorRenderPassDrawQuad should have the same scale transform that
  // was applied to the SurfaceDrawQuad.
  EXPECT_EQ(output_quad->shared_quad_state->quad_to_target_transform,
            scale_transform);
}

// Verify that a reflected SurfaceDrawQuad with no scaling has the surfaces root
// RenderPass merged with the RenderPass that embeds it.
TEST_F(SurfaceAggregatorValidSurfaceTest, ReflectedSurfaceDrawQuadNotScaled) {
  const SurfaceId root_surface_id(root_sink_->frame_sink_id(),
                                  root_local_surface_id_);

  // Submit a CompositorFrame for the primary display. This will get mirrored
  // by the second display through surface embedding.
  const gfx::Rect display_rect(0, 0, 100, 100);
  {
    auto pass = CompositorRenderPass::Create();
    pass->SetNew(CompositorRenderPassId{1}, display_rect, display_rect,
                 gfx::Transform());
    auto* sqs = pass->CreateAndAppendSharedQuadState();
    sqs->opacity = 1.f;

    auto* solid_color_quad =
        pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
    solid_color_quad->SetNew(sqs, display_rect, display_rect, SK_ColorRED,
                             false);

    CompositorFrame frame =
        CompositorFrameBuilder().AddRenderPass(std::move(pass)).Build();
    root_sink_->SubmitCompositorFrame(root_surface_id.local_surface_id(),
                                      std::move(frame));
  }

  auto mirror_display_sink = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryFrameSinkId1, true);

  ParentLocalSurfaceIdAllocator lsi_allocator;
  lsi_allocator.GenerateId();
  LocalSurfaceId mirror_display_local_surface_id =
      lsi_allocator.GetCurrentLocalSurfaceId();

  // The mirroring display is the same width but different height. The mirrored
  // content would be letterboxed by translating it.
  const gfx::Rect mirror_display_rect(120, 100);
  gfx::Transform translate_transform;
  translate_transform.Translate(10, 0);

  {
    auto pass = CompositorRenderPass::Create();
    pass->SetNew(CompositorRenderPassId{1}, mirror_display_rect,
                 mirror_display_rect, gfx::Transform());
    auto* sqs = pass->CreateAndAppendSharedQuadState();
    sqs->quad_to_target_transform = translate_transform;
    sqs->opacity = 1.f;

    auto* surface_quad = pass->CreateAndAppendDrawQuad<SurfaceDrawQuad>();
    surface_quad->SetAll(sqs, display_rect, display_rect,
                         /*needs_blending=*/false,
                         SurfaceRange(base::nullopt, root_surface_id),
                         SK_ColorBLACK,
                         /*stretch_content_to_fill_bounds=*/true,
                         /*is_reflection=*/true,
                         /*allow_merge=*/true);

    CompositorFrame frame =
        CompositorFrameBuilder().AddRenderPass(std::move(pass)).Build();
    mirror_display_sink->SubmitCompositorFrame(mirror_display_local_surface_id,
                                               std::move(frame));
  }

  const SurfaceId mirror_display_surface_id(
      mirror_display_sink->frame_sink_id(), mirror_display_local_surface_id);
  auto frame = AggregateFrame(mirror_display_surface_id);

  // The reflected surfaces RenderPass should be merged into the root RenderPass
  // since it's not being scaled.
  EXPECT_EQ(1u, frame.render_pass_list.size());

  auto* root_render_pass = frame.render_pass_list.back().get();
  EXPECT_EQ(1u, root_render_pass->quad_list.size());

  auto* output_quad = root_render_pass->quad_list.back();
  EXPECT_EQ(DrawQuad::Material::kSolidColor, output_quad->material);

  // The quad from the embedded surface merged into the root RenderPass should
  // have the same translate transform that was applied to the SurfaceDrawQuad.
  EXPECT_EQ(output_quad->shared_quad_state->quad_to_target_transform,
            translate_transform);
}

// This test verifies that in the presence of both primary Surface and fallback
// Surface, the fallback will not be used.
TEST_F(SurfaceAggregatorValidSurfaceTest, FallbackSurfaceReferenceWithPrimary) {
  auto primary_child_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryFrameSinkId1, kChildIsRoot);
  ParentLocalSurfaceIdAllocator primary_allocator;
  primary_allocator.GenerateId();
  LocalSurfaceId primary_child_local_surface_id =
      primary_allocator.GetCurrentLocalSurfaceId();
  SurfaceId primary_child_surface_id(primary_child_support->frame_sink_id(),
                                     primary_child_local_surface_id);
  std::vector<Quad> primary_child_quads = {
      Quad::SolidColorQuad(SK_ColorGREEN, gfx::Rect(5, 5))};
  constexpr gfx::Size primary_size(50, 50);
  std::vector<Pass> primary_child_passes = {
      Pass(primary_child_quads, primary_size)};

  // Submit a CompositorFrame to the primary Surface containing a green
  // SolidColorDrawQuad.
  constexpr float device_scale_factor = 1.0f;
  SubmitCompositorFrame(primary_child_support.get(), primary_child_passes,

                        primary_child_local_surface_id, device_scale_factor);

  auto fallback_child_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryFrameSinkId2, kChildIsRoot);
  ParentLocalSurfaceIdAllocator fallback_allocator;
  fallback_allocator.GenerateId();
  LocalSurfaceId fallback_child_local_surface_id =
      fallback_allocator.GetCurrentLocalSurfaceId();
  SurfaceId fallback_child_surface_id(fallback_child_support->frame_sink_id(),
                                      fallback_child_local_surface_id);

  std::vector<Quad> fallback_child_quads = {
      Quad::SolidColorQuad(SK_ColorRED, gfx::Rect(5, 5))};
  std::vector<Pass> fallback_child_passes = {
      Pass(fallback_child_quads, SurfaceSize())};

  // Submit a CompositorFrame to the fallback Surface containing a red
  // SolidColorDrawQuad.
  SubmitCompositorFrame(fallback_child_support.get(), fallback_child_passes,

                        fallback_child_local_surface_id, device_scale_factor);

  // Try to embed |primary_child_surface_id| and if unavailabe, embed
  // |fallback_child_surface_id|.
  std::vector<Quad> root_quads = {Quad::SurfaceQuad(
      SurfaceRange(fallback_child_surface_id, primary_child_surface_id),
      SK_ColorWHITE, gfx::Rect(5, 5),
      /*stretch_content_to_fill_bounds=*/false)};
  constexpr gfx::Size root_size(75, 75);
  std::vector<Pass> root_passes = {Pass(root_quads, root_size, NoDamage())};

  MockAggregatedDamageCallback aggregated_damage_callback;
  root_sink_->SetAggregatedDamageCallbackForTesting(
      aggregated_damage_callback.GetCallback());

  SubmitCompositorFrame(root_sink_.get(), root_passes, root_local_surface_id_,
                        device_scale_factor);

  // The CompositorFrame is submitted to |primary_child_surface_id|, so
  // |fallback_child_surface_id| will not be used and we should see a green
  // SolidColorDrawQuad.
  std::vector<Quad> expected_quads1 = {
      Quad::SolidColorQuad(SK_ColorGREEN, gfx::Rect(5, 5))};
  std::vector<Pass> expected_passes1 = {Pass(expected_quads1, SurfaceSize())};

  SurfaceId root_surface_id(root_sink_->frame_sink_id(),
                            root_local_surface_id_);

  EXPECT_CALL(aggregated_damage_callback,
              OnAggregatedDamage(root_local_surface_id_, root_size,
                                 gfx::Rect(root_size), next_display_time()));

  // The fallback will not be contained within the aggregated frame.
  AggregateAndVerify(expected_passes1,
                     {root_surface_id, primary_child_surface_id});

  testing::Mock::VerifyAndClearExpectations(&aggregated_damage_callback);

  // Submit a new frame to the primary surface to cause some damage.
  SubmitCompositorFrame(primary_child_support.get(), primary_child_passes,

                        primary_child_local_surface_id, device_scale_factor);

  // The size of the damage should be equal to the size of the primary surface.
  EXPECT_CALL(aggregated_damage_callback,
              OnAggregatedDamage(root_local_surface_id_, root_size,
                                 gfx::Rect(primary_size), next_display_time()));

  // Generate a new aggregated frame.
  AggregateAndVerify(expected_passes1,
                     {root_surface_id, primary_child_surface_id});

  testing::Mock::VerifyAndClearExpectations(&aggregated_damage_callback);
}

TEST_F(SurfaceAggregatorValidSurfaceTest, CopyRequest) {
  auto embedded_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryFrameSinkId1, kChildIsRoot);
  ParentLocalSurfaceIdAllocator embedded_allocator;
  embedded_allocator.GenerateId();
  LocalSurfaceId embedded_local_surface_id =
      embedded_allocator.GetCurrentLocalSurfaceId();
  SurfaceId embedded_surface_id(embedded_support->frame_sink_id(),
                                embedded_local_surface_id);

  std::vector<Quad> embedded_quads = {
      Quad::SolidColorQuad(SK_ColorGREEN, gfx::Rect(5, 5))};
  std::vector<Pass> embedded_passes = {Pass(embedded_quads, SurfaceSize())};

  constexpr float device_scale_factor = 1.0f;
  SubmitCompositorFrame(embedded_support.get(), embedded_passes,
                        embedded_local_surface_id, device_scale_factor);
  auto copy_request = CopyOutputRequest::CreateStubForTesting();
  auto* copy_request_ptr = copy_request.get();
  embedded_support->RequestCopyOfOutput(
      {embedded_local_surface_id, SubtreeCaptureId(), std::move(copy_request)});

  std::vector<Quad> root_quads = {
      Quad::SolidColorQuad(SK_ColorWHITE, gfx::Rect(5, 5)),
      Quad::SurfaceQuad(SurfaceRange(base::nullopt, embedded_surface_id),
                        SK_ColorWHITE, gfx::Rect(5, 5),
                        /*stretch_content_to_fill_bounds=*/false),
      Quad::SolidColorQuad(SK_ColorBLACK, gfx::Rect(5, 5))};
  std::vector<Pass> root_passes = {Pass(root_quads, SurfaceSize())};

  SubmitCompositorFrame(root_sink_.get(), root_passes, root_local_surface_id_,
                        device_scale_factor);

  SurfaceId root_surface_id(root_sink_->frame_sink_id(),
                            root_local_surface_id_);
  auto aggregated_frame = AggregateFrame(root_surface_id);

  std::vector<Quad> expected_quads = {
      Quad::SolidColorQuad(SK_ColorWHITE, gfx::Rect(5, 5)),
      Quad::RenderPassQuad(CompositorRenderPassId{uint64_t{
                               aggregated_frame.render_pass_list[0]->id}},
                           gfx::Transform(), true),
      Quad::SolidColorQuad(SK_ColorBLACK, gfx::Rect(5, 5))};
  std::vector<Pass> expected_passes = {Pass(embedded_quads, SurfaceSize()),
                                       Pass(expected_quads, SurfaceSize())};
  TestPassesMatchExpectations(expected_passes,
                              &aggregated_frame.render_pass_list);
  EXPECT_TRUE(aggregated_frame.has_copy_requests);
  ASSERT_EQ(2u, aggregated_frame.render_pass_list.size());
  ASSERT_EQ(1u, aggregated_frame.render_pass_list[0]->copy_requests.size());
  DCHECK_EQ(copy_request_ptr,
            aggregated_frame.render_pass_list[0]->copy_requests[0].get());

  SurfaceId surface_ids[] = {root_surface_id, embedded_surface_id};
  EXPECT_EQ(base::size(surface_ids),
            aggregator_.previous_contained_surfaces().size());
  for (size_t i = 0; i < base::size(surface_ids); i++) {
    EXPECT_TRUE(
        aggregator_.previous_contained_surfaces().find(surface_ids[i]) !=
        aggregator_.previous_contained_surfaces().end());
  }
}

// Root surface may contain copy requests.
TEST_F(SurfaceAggregatorValidSurfaceTest, RootCopyRequest) {
  auto embedded_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryFrameSinkId2, kChildIsRoot);
  ParentLocalSurfaceIdAllocator embedded_allocator;
  embedded_allocator.GenerateId();
  LocalSurfaceId embedded_local_surface_id =
      embedded_allocator.GetCurrentLocalSurfaceId();
  SurfaceId embedded_surface_id(embedded_support->frame_sink_id(),
                                embedded_local_surface_id);

  std::vector<Quad> embedded_quads = {
      Quad::SolidColorQuad(SK_ColorGREEN, gfx::Rect(5, 5))};
  std::vector<Pass> embedded_passes = {Pass(embedded_quads, SurfaceSize())};

  constexpr float device_scale_factor = 1.0f;
  SubmitCompositorFrame(embedded_support.get(), embedded_passes,
                        embedded_local_surface_id, device_scale_factor);
  auto copy_request(CopyOutputRequest::CreateStubForTesting());
  auto* copy_request_ptr = copy_request.get();
  auto copy_request2(CopyOutputRequest::CreateStubForTesting());
  auto* copy_request2_ptr = copy_request2.get();

  std::vector<Quad> root_quads = {
      Quad::SolidColorQuad(SK_ColorWHITE, gfx::Rect(5, 5)),
      Quad::SurfaceQuad(SurfaceRange(base::nullopt, embedded_surface_id),
                        SK_ColorWHITE, gfx::Rect(5, 5),
                        /*stretch_content_to_fill_bounds=*/false),
      Quad::SolidColorQuad(SK_ColorBLACK, gfx::Rect(5, 5))};
  std::vector<Quad> root_quads2 = {
      Quad::SolidColorQuad(SK_ColorRED, gfx::Rect(5, 5))};
  std::vector<Pass> root_passes = {
      Pass(root_quads, CompositorRenderPassId{1}, SurfaceSize()),
      Pass(root_quads2, CompositorRenderPassId{2}, SurfaceSize())};
  {
    CompositorFrame frame = MakeEmptyCompositorFrame();
    AddPasses(&frame.render_pass_list, root_passes,
              &frame.metadata.referenced_surfaces);
    frame.render_pass_list[0]->copy_requests.push_back(std::move(copy_request));
    frame.render_pass_list[1]->copy_requests.push_back(
        std::move(copy_request2));

    root_sink_->SubmitCompositorFrame(root_local_surface_id_, std::move(frame));
  }

  SurfaceId root_surface_id(root_sink_->frame_sink_id(),
                            root_local_surface_id_);
  auto aggregated_frame = AggregateFrame(root_surface_id);

  std::vector<Quad> expected_quads = {
      Quad::SolidColorQuad(SK_ColorWHITE, gfx::Rect(5, 5)),
      Quad::SolidColorQuad(SK_ColorGREEN, gfx::Rect(5, 5)),
      Quad::SolidColorQuad(SK_ColorBLACK, gfx::Rect(5, 5))};
  std::vector<Pass> expected_passes = {Pass(expected_quads, SurfaceSize()),
                                       Pass(root_quads2, SurfaceSize())};
  TestPassesMatchExpectations(expected_passes,
                              &aggregated_frame.render_pass_list);
  EXPECT_TRUE(aggregated_frame.has_copy_requests);
  ASSERT_EQ(2u, aggregated_frame.render_pass_list.size());
  ASSERT_EQ(1u, aggregated_frame.render_pass_list[0]->copy_requests.size());
  DCHECK_EQ(copy_request_ptr,
            aggregated_frame.render_pass_list[0]->copy_requests[0].get());
  ASSERT_EQ(1u, aggregated_frame.render_pass_list[1]->copy_requests.size());
  DCHECK_EQ(copy_request2_ptr,
            aggregated_frame.render_pass_list[1]->copy_requests[0].get());

  SurfaceId surface_ids[] = {root_surface_id, embedded_surface_id};
  EXPECT_EQ(base::size(surface_ids),
            aggregator_.previous_contained_surfaces().size());
  for (size_t i = 0; i < base::size(surface_ids); i++) {
    EXPECT_TRUE(
        aggregator_.previous_contained_surfaces().find(surface_ids[i]) !=
        aggregator_.previous_contained_surfaces().end());
  }

  // Ensure copy requests have been removed from root surface.
  const CompositorFrame& original_frame = manager_.surface_manager()
                                              ->GetSurfaceForId(root_surface_id)
                                              ->GetActiveFrame();
  const auto& original_pass_list = original_frame.render_pass_list;
  ASSERT_EQ(2u, original_pass_list.size());
  DCHECK(original_pass_list[0]->copy_requests.empty());
  DCHECK(original_pass_list[1]->copy_requests.empty());
}

TEST_F(SurfaceAggregatorValidSurfaceTest, UnreferencedSurface) {
  auto embedded_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryFrameSinkId1, kChildIsRoot);
  auto parent_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryFrameSinkId2, kRootIsRoot);
  ParentLocalSurfaceIdAllocator child_allocator;
  child_allocator.GenerateId();
  LocalSurfaceId embedded_local_surface_id =
      child_allocator.GetCurrentLocalSurfaceId();
  SurfaceId embedded_surface_id(embedded_support->frame_sink_id(),
                                embedded_local_surface_id);
  root_allocator_.GenerateId();
  SurfaceId nonexistent_surface_id(root_sink_->frame_sink_id(),
                                   root_allocator_.GetCurrentLocalSurfaceId());

  std::vector<Quad> embedded_quads = {
      Quad::SolidColorQuad(SK_ColorGREEN, gfx::Rect(5, 5))};
  std::vector<Pass> embedded_passes = {Pass(embedded_quads, SurfaceSize())};

  constexpr float device_scale_factor = 1.0f;
  SubmitCompositorFrame(embedded_support.get(), embedded_passes,
                        embedded_local_surface_id, device_scale_factor);
  auto copy_request(CopyOutputRequest::CreateStubForTesting());
  auto* copy_request_ptr = copy_request.get();
  embedded_support->RequestCopyOfOutput(
      {embedded_local_surface_id, SubtreeCaptureId(), std::move(copy_request)});

  ParentLocalSurfaceIdAllocator parent_allocator;
  parent_allocator.GenerateId();
  LocalSurfaceId parent_local_surface_id =
      parent_allocator.GetCurrentLocalSurfaceId();
  SurfaceId parent_surface_id(parent_support->frame_sink_id(),
                              parent_local_surface_id);

  std::vector<Quad> parent_quads = {
      Quad::SolidColorQuad(SK_ColorGRAY, gfx::Rect(5, 5)),
      Quad::SurfaceQuad(SurfaceRange(base::nullopt, embedded_surface_id),
                        SK_ColorWHITE, gfx::Rect(5, 5),
                        /*stretch_content_to_fill_bounds=*/false),
      Quad::SolidColorQuad(SK_ColorLTGRAY, gfx::Rect(5, 5))};
  std::vector<Pass> parent_passes = {Pass(parent_quads, SurfaceSize())};

  {
    CompositorFrame frame = MakeEmptyCompositorFrame();

    AddPasses(&frame.render_pass_list, parent_passes,
              &frame.metadata.referenced_surfaces);

    frame.metadata.referenced_surfaces.emplace_back(embedded_surface_id);

    parent_support->SubmitCompositorFrame(parent_local_surface_id,
                                          std::move(frame));
  }

  std::vector<Quad> root_quads = {
      Quad::SolidColorQuad(SK_ColorWHITE, gfx::Rect(5, 5)),
      Quad::SolidColorQuad(SK_ColorBLACK, gfx::Rect(5, 5))};
  std::vector<Pass> root_passes = {Pass(root_quads, SurfaceSize())};

  {
    CompositorFrame frame = MakeEmptyCompositorFrame();
    AddPasses(&frame.render_pass_list, root_passes,
              &frame.metadata.referenced_surfaces);

    frame.metadata.referenced_surfaces.emplace_back(parent_surface_id);
    // Reference to Surface ID of a Surface that doesn't exist should be
    // included in previous_contained_surfaces, but otherwise ignored.
    frame.metadata.referenced_surfaces.emplace_back(nonexistent_surface_id);

    root_sink_->SubmitCompositorFrame(root_local_surface_id_, std::move(frame));
  }

  SurfaceId root_surface_id(root_sink_->frame_sink_id(),
                            root_local_surface_id_);
  auto aggregated_frame = AggregateFrame(root_surface_id);

  // First pass should come from surface that had a copy request but was not
  // referenced directly. The second pass comes from the root surface.
  // parent_quad should be ignored because it is neither referenced through a
  // SurfaceDrawQuad nor has a copy request on it.
  std::vector<Pass> expected_passes = {Pass(embedded_quads, SurfaceSize()),
                                       Pass(root_quads, SurfaceSize())};
  TestPassesMatchExpectations(expected_passes,
                              &aggregated_frame.render_pass_list);
  EXPECT_TRUE(aggregated_frame.has_copy_requests);
  ASSERT_EQ(2u, aggregated_frame.render_pass_list.size());
  ASSERT_EQ(1u, aggregated_frame.render_pass_list[0]->copy_requests.size());
  DCHECK_EQ(copy_request_ptr,
            aggregated_frame.render_pass_list[0]->copy_requests[0].get());

  SurfaceId surface_ids[] = {
      SurfaceId(root_sink_->frame_sink_id(), root_local_surface_id_),
      parent_surface_id, embedded_surface_id};
  EXPECT_EQ(base::size(surface_ids),
            aggregator_.previous_contained_surfaces().size());
  for (size_t i = 0; i < base::size(surface_ids); i++) {
    EXPECT_TRUE(
        aggregator_.previous_contained_surfaces().find(surface_ids[i]) !=
        aggregator_.previous_contained_surfaces().end());
  }
}

// This tests referencing a surface that has multiple render passes.
TEST_F(SurfaceAggregatorValidSurfaceTest, MultiPassSurfaceReference) {
  ParentLocalSurfaceIdAllocator child_allocator;
  child_allocator.GenerateId();
  LocalSurfaceId embedded_local_surface_id =
      child_allocator.GetCurrentLocalSurfaceId();
  SurfaceId embedded_surface_id(child_sink_->frame_sink_id(),
                                embedded_local_surface_id);

  CompositorRenderPassId pass_ids[] = {CompositorRenderPassId{1},
                                       CompositorRenderPassId{2},
                                       CompositorRenderPassId{3}};

  std::vector<Quad> embedded_quads[3] = {
      {Quad::SolidColorQuad(1, gfx::Rect(5, 5)),
       Quad::SolidColorQuad(2, gfx::Rect(5, 5))},
      {Quad::SolidColorQuad(3, gfx::Rect(5, 5)),
       Quad::RenderPassQuad(pass_ids[0], gfx::Transform(), true)},
      {Quad::SolidColorQuad(4, gfx::Rect(5, 5)),
       Quad::RenderPassQuad(pass_ids[1], gfx::Transform(), true)}};
  std::vector<Pass> embedded_passes = {
      Pass(embedded_quads[0], pass_ids[0], SurfaceSize()),
      Pass(embedded_quads[1], pass_ids[1], SurfaceSize()),
      Pass(embedded_quads[2], pass_ids[2], SurfaceSize())};

  constexpr float device_scale_factor = 1.0f;
  SubmitCompositorFrame(child_sink_.get(), embedded_passes,
                        embedded_local_surface_id, device_scale_factor);

  std::vector<Quad> root_quads[3] = {
      {Quad::SolidColorQuad(5, gfx::Rect(5, 5)),
       Quad::SolidColorQuad(6, gfx::Rect(5, 5))},
      {Quad::SurfaceQuad(SurfaceRange(base::nullopt, embedded_surface_id),
                         SK_ColorWHITE, gfx::Rect(5, 5),
                         /*stretch_content_to_fill_bounds=*/false),
       Quad::RenderPassQuad(pass_ids[0], gfx::Transform(), true)},
      {Quad::SolidColorQuad(7, gfx::Rect(5, 5)),
       Quad::RenderPassQuad(pass_ids[1], gfx::Transform(), true)}};
  std::vector<Pass> root_passes = {
      Pass(root_quads[0], pass_ids[0], SurfaceSize()),
      Pass(root_quads[1], pass_ids[1], SurfaceSize()),
      Pass(root_quads[2], pass_ids[2], SurfaceSize())};

  SubmitCompositorFrame(root_sink_.get(), root_passes, root_local_surface_id_,
                        device_scale_factor);

  SurfaceId root_surface_id(root_sink_->frame_sink_id(),
                            root_local_surface_id_);
  auto aggregated_frame = AggregateFrame(root_surface_id);

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
  std::vector<Quad> quads = {
      Quad::SolidColorQuad(SK_ColorGREEN, gfx::Rect(5, 5)),
      Quad::SurfaceQuad(
          SurfaceRange(SurfaceId(
              FrameSinkId(),
              LocalSurfaceId(0xdeadbeef, 0xdeadbeef, kArbitraryToken))),
          SK_ColorWHITE, gfx::Rect(5, 5),
          /*stretch_content_to_fill_bounds=*/false),
      Quad::SolidColorQuad(SK_ColorBLUE, gfx::Rect(5, 5))};
  std::vector<Pass> passes = {Pass(quads, SurfaceSize())};

  constexpr float device_scale_factor = 1.0f;
  SubmitCompositorFrame(root_sink_.get(), passes, root_local_surface_id_,
                        device_scale_factor);

  std::vector<Quad> expected_quads = {
      Quad::SolidColorQuad(SK_ColorGREEN, gfx::Rect(5, 5)),
      Quad::SolidColorQuad(SK_ColorBLUE, gfx::Rect(5, 5))};
  std::vector<Pass> expected_passes = {Pass(expected_quads, SurfaceSize())};
  SurfaceId root_surface_id(root_sink_->frame_sink_id(),
                            root_local_surface_id_);
  AggregateAndVerify(expected_passes, {root_surface_id});
}

// Tests a reference to a valid surface with no submitted frame. A
// SolidColorDrawQuad should be placed in lieu of a frame.
TEST_F(SurfaceAggregatorValidSurfaceTest, ValidSurfaceReferenceWithNoFrame) {
  ParentLocalSurfaceIdAllocator child_allocator;
  child_allocator.GenerateId();
  LocalSurfaceId empty_local_surface_id =
      child_allocator.GetCurrentLocalSurfaceId();
  SurfaceId surface_with_no_frame_id(kArbitraryFrameSinkId1,
                                     empty_local_surface_id);

  std::vector<Quad> quads = {
      Quad::SolidColorQuad(SK_ColorGREEN, gfx::Rect(5, 5)),
      Quad::SurfaceQuad(SurfaceRange(base::nullopt, surface_with_no_frame_id),
                        SK_ColorYELLOW, gfx::Rect(5, 5),
                        /*stretch_content_to_fill_bounds=*/false),
      Quad::SolidColorQuad(SK_ColorBLUE, gfx::Rect(5, 5))};
  std::vector<Pass> passes = {Pass(quads, SurfaceSize())};

  constexpr float device_scale_factor = 1.0f;
  SubmitCompositorFrame(root_sink_.get(), passes, root_local_surface_id_,
                        device_scale_factor);

  std::vector<Quad> expected_quads = {
      Quad::SolidColorQuad(SK_ColorGREEN, gfx::Rect(5, 5)),
      Quad::SolidColorQuad(SK_ColorYELLOW, gfx::Rect(5, 5)),
      Quad::SolidColorQuad(SK_ColorBLUE, gfx::Rect(5, 5))};
  std::vector<Pass> expected_passes = {Pass(expected_quads, SurfaceSize())};
  SurfaceId root_surface_id(root_sink_->frame_sink_id(),
                            root_local_surface_id_);
  AggregateAndVerify(expected_passes, {root_surface_id});
}

// Tests a reference to a valid primary surface and a fallback surface
// with no submitted frame. A SolidColorDrawQuad should be placed in lieu of a
// frame.
TEST_F(SurfaceAggregatorValidSurfaceTest, ValidFallbackWithNoFrame) {
  root_allocator_.GenerateId();
  const LocalSurfaceId empty_local_surface_id =
      root_allocator_.GetCurrentLocalSurfaceId();
  const SurfaceId surface_with_no_frame_id(root_sink_->frame_sink_id(),
                                           empty_local_surface_id);

  std::vector<Quad> quads = {Quad::SurfaceQuad(
      SurfaceRange(surface_with_no_frame_id), SK_ColorYELLOW, gfx::Rect(5, 5),
      /*stretch_content_to_fill_bounds=*/false)};
  std::vector<Pass> passes = {Pass(quads, SurfaceSize())};

  constexpr float device_scale_factor = 1.0f;
  SubmitCompositorFrame(root_sink_.get(), passes, root_local_surface_id_,
                        device_scale_factor);

  std::vector<Quad> expected_quads = {
      Quad::SolidColorQuad(SK_ColorYELLOW, gfx::Rect(5, 5)),
  };
  std::vector<Pass> expected_passes = {Pass(expected_quads, SurfaceSize())};
  SurfaceId root_surface_id(root_sink_->frame_sink_id(),
                            root_local_surface_id_);
  AggregateAndVerify(expected_passes, {root_surface_id});
}

// Tests a surface quad referencing itself, generating a trivial cycle.
// The quad creating the cycle should be dropped from the final frame.
TEST_F(SurfaceAggregatorValidSurfaceTest, SimpleCyclicalReference) {
  SurfaceId root_surface_id(root_sink_->frame_sink_id(),
                            root_local_surface_id_);
  std::vector<Quad> quads = {
      Quad::SurfaceQuad(SurfaceRange(base::nullopt, root_surface_id),
                        SK_ColorWHITE, gfx::Rect(5, 5),
                        /*stretch_content_to_fill_bounds=*/false),
      Quad::SolidColorQuad(SK_ColorYELLOW, gfx::Rect(5, 5))};
  std::vector<Pass> passes = {Pass(quads, SurfaceSize())};

  constexpr float device_scale_factor = 1.0f;
  SubmitCompositorFrame(root_sink_.get(), passes, root_local_surface_id_,
                        device_scale_factor);

  std::vector<Quad> expected_quads = {
      Quad::SolidColorQuad(SK_ColorYELLOW, gfx::Rect(5, 5))};
  std::vector<Pass> expected_passes = {Pass(expected_quads, SurfaceSize())};
  AggregateAndVerify(expected_passes, {root_surface_id});
}

// Tests a more complex cycle with one intermediate surface.
TEST_F(SurfaceAggregatorValidSurfaceTest, TwoSurfaceCyclicalReference) {
  ParentLocalSurfaceIdAllocator child_allocator;
  child_allocator.GenerateId();
  LocalSurfaceId child_local_surface_id =
      child_allocator.GetCurrentLocalSurfaceId();
  SurfaceId child_surface_id(child_sink_->frame_sink_id(),
                             child_local_surface_id);

  std::vector<Quad> parent_quads = {
      Quad::SolidColorQuad(SK_ColorBLUE, gfx::Rect(5, 5)),
      Quad::SurfaceQuad(SurfaceRange(base::nullopt, child_surface_id),
                        SK_ColorWHITE, gfx::Rect(5, 5),
                        /*stretch_content_to_fill_bounds=*/false),
      Quad::SolidColorQuad(SK_ColorCYAN, gfx::Rect(5, 5))};
  std::vector<Pass> parent_passes = {Pass(parent_quads, SurfaceSize())};

  constexpr float device_scale_factor = 1.0f;
  SubmitCompositorFrame(root_sink_.get(), parent_passes, root_local_surface_id_,
                        device_scale_factor);

  SurfaceId root_surface_id(root_sink_->frame_sink_id(),
                            root_local_surface_id_);
  std::vector<Quad> child_quads = {
      Quad::SolidColorQuad(SK_ColorGREEN, gfx::Rect(5, 5)),
      Quad::SurfaceQuad(SurfaceRange(base::nullopt, root_surface_id),
                        SK_ColorWHITE, gfx::Rect(5, 5),
                        /*stretch_content_to_fill_bounds=*/false),
      Quad::SolidColorQuad(SK_ColorMAGENTA, gfx::Rect(5, 5))};
  std::vector<Pass> child_passes = {Pass(child_quads, SurfaceSize())};

  SubmitCompositorFrame(child_sink_.get(), child_passes, child_local_surface_id,
                        device_scale_factor);

  // The child surface's reference to the root_surface_ will be dropped, so
  // we'll end up with:
  //   SK_ColorBLUE from the parent
  //   SK_ColorGREEN from the child
  //   SK_ColorMAGENTA from the child
  //   SK_ColorCYAN from the parent
  std::vector<Quad> expected_quads = {
      Quad::SolidColorQuad(SK_ColorBLUE, gfx::Rect(5, 5)),
      Quad::SolidColorQuad(SK_ColorGREEN, gfx::Rect(5, 5)),
      Quad::SolidColorQuad(SK_ColorMAGENTA, gfx::Rect(5, 5)),
      Quad::SolidColorQuad(SK_ColorCYAN, gfx::Rect(5, 5))};
  std::vector<Pass> expected_passes = {Pass(expected_quads, SurfaceSize())};
  AggregateAndVerify(expected_passes, {root_surface_id, child_surface_id});
}

// Tests that we map render pass IDs from different surfaces into a unified
// namespace and update CompositorRenderPassDrawQuad's id references to match.
TEST_F(SurfaceAggregatorValidSurfaceTest, RenderPassIdMapping) {
  ParentLocalSurfaceIdAllocator child_allocator;
  child_allocator.GenerateId();
  LocalSurfaceId child_local_surface_id =
      child_allocator.GetCurrentLocalSurfaceId();
  SurfaceId child_surface_id(child_sink_->frame_sink_id(),
                             child_local_surface_id);

  CompositorRenderPassId child_pass_id[] = {CompositorRenderPassId{1u},
                                            CompositorRenderPassId{2u}};
  std::vector<Quad> child_quad[2] = {
      {Quad::SolidColorQuad(SK_ColorGREEN, gfx::Rect(5, 5))},
      {Quad::RenderPassQuad(child_pass_id[0], gfx::Transform(), true)}};
  std::vector<Pass> surface_passes = {
      Pass(child_quad[0], child_pass_id[0], SurfaceSize()),
      Pass(child_quad[1], child_pass_id[1], SurfaceSize())};

  constexpr float device_scale_factor = 1.0f;
  SubmitCompositorFrame(child_sink_.get(), surface_passes,
                        child_local_surface_id, device_scale_factor);

  // Pass IDs from the parent surface may collide with ones from the child.
  CompositorRenderPassId parent_pass_id[] = {CompositorRenderPassId{3u},
                                             CompositorRenderPassId{2u}};
  std::vector<Quad> parent_quad[2] = {
      {Quad::SurfaceQuad(SurfaceRange(base::nullopt, child_surface_id),
                         SK_ColorWHITE, gfx::Rect(5, 5),
                         /*stretch_content_to_fill_bounds=*/false)},
      {Quad::RenderPassQuad(parent_pass_id[0], gfx::Transform(), true)}};
  std::vector<Pass> parent_passes = {
      Pass(parent_quad[0], parent_pass_id[0], SurfaceSize()),
      Pass(parent_quad[1], parent_pass_id[1], SurfaceSize())};

  SubmitCompositorFrame(root_sink_.get(), parent_passes, root_local_surface_id_,
                        device_scale_factor);

  SurfaceId root_surface_id(root_sink_->frame_sink_id(),
                            root_local_surface_id_);
  auto aggregated_frame = AggregateFrame(root_surface_id);

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

void AddSolidColorQuadWithBlendMode(
    const gfx::Size& size,
    CompositorRenderPass* pass,
    const SkBlendMode blend_mode,
    const gfx::MaskFilterInfo& mask_filter_info) {
  const gfx::Transform layer_to_target_transform;
  const gfx::Rect layer_rect(size);
  const gfx::Rect visible_layer_rect(size);
  const gfx::Rect clip_rect(size);

  bool is_clipped = false;
  SkColor color = SK_ColorGREEN;
  bool are_contents_opaque = SkColorGetA(color) == 0xFF;
  float opacity = 1.f;

  bool force_anti_aliasing_off = false;
  auto* sqs = pass->CreateAndAppendSharedQuadState();
  sqs->SetAll(layer_to_target_transform, layer_rect, visible_layer_rect,
              mask_filter_info, clip_rect, is_clipped, are_contents_opaque,
              opacity, blend_mode, 0);

  auto* color_quad = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  color_quad->SetNew(pass->shared_quad_state_list.back(), visible_layer_rect,
                     visible_layer_rect, color, force_anti_aliasing_off);
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
  ParentLocalSurfaceIdAllocator grandchild_allocator;
  ParentLocalSurfaceIdAllocator child_one_allocator;
  ParentLocalSurfaceIdAllocator child_two_allocator;
  auto grandchild_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryFrameSinkId1, kChildIsRoot);
  auto child_one_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryFrameSinkId2, kChildIsRoot);
  auto child_two_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryFrameSinkId3, kChildIsRoot);
  CompositorRenderPassId pass_id{1};
  grandchild_allocator.GenerateId();
  LocalSurfaceId grandchild_local_surface_id =
      grandchild_allocator.GetCurrentLocalSurfaceId();
  SurfaceId grandchild_surface_id(grandchild_support->frame_sink_id(),
                                  grandchild_local_surface_id);

  auto grandchild_pass = CompositorRenderPass::Create();
  constexpr float device_scale_factor = 1.0f;
  gfx::Rect output_rect(SurfaceSize());
  gfx::Rect damage_rect(SurfaceSize());
  gfx::Transform transform_to_root_target;
  grandchild_pass->SetNew(pass_id, output_rect, damage_rect,
                          transform_to_root_target);
  AddSolidColorQuadWithBlendMode(SurfaceSize(), grandchild_pass.get(),
                                 blend_modes[2], gfx::MaskFilterInfo());
  QueuePassAsFrame(std::move(grandchild_pass), grandchild_local_surface_id,
                   device_scale_factor, grandchild_support.get());

  child_one_allocator.GenerateId();
  LocalSurfaceId child_one_local_surface_id =
      child_one_allocator.GetCurrentLocalSurfaceId();
  SurfaceId child_one_surface_id(child_one_support->frame_sink_id(),
                                 child_one_local_surface_id);

  auto child_one_pass = CompositorRenderPass::Create();
  child_one_pass->SetNew(pass_id, output_rect, damage_rect,
                         transform_to_root_target);
  AddSolidColorQuadWithBlendMode(SurfaceSize(), child_one_pass.get(),
                                 blend_modes[1], gfx::MaskFilterInfo());
  auto* grandchild_surface_quad =
      child_one_pass->CreateAndAppendDrawQuad<SurfaceDrawQuad>();
  grandchild_surface_quad->SetNew(
      child_one_pass->shared_quad_state_list.back(), gfx::Rect(SurfaceSize()),
      gfx::Rect(SurfaceSize()),
      SurfaceRange(base::nullopt, grandchild_surface_id), SK_ColorWHITE,
      /*stretch_content_to_fill_bounds=*/false);
  AddSolidColorQuadWithBlendMode(SurfaceSize(), child_one_pass.get(),
                                 blend_modes[3], gfx::MaskFilterInfo());
  QueuePassAsFrame(std::move(child_one_pass), child_one_local_surface_id,
                   device_scale_factor, child_one_support.get());

  child_two_allocator.GenerateId();
  LocalSurfaceId child_two_local_surface_id =
      child_two_allocator.GetCurrentLocalSurfaceId();
  SurfaceId child_two_surface_id(child_two_support->frame_sink_id(),
                                 child_two_local_surface_id);

  auto child_two_pass = CompositorRenderPass::Create();
  child_two_pass->SetNew(pass_id, output_rect, damage_rect,
                         transform_to_root_target);
  AddSolidColorQuadWithBlendMode(SurfaceSize(), child_two_pass.get(),
                                 blend_modes[5], gfx::MaskFilterInfo());
  QueuePassAsFrame(std::move(child_two_pass), child_two_local_surface_id,
                   device_scale_factor, child_two_support.get());

  auto root_pass = CompositorRenderPass::Create();
  root_pass->SetNew(pass_id, output_rect, damage_rect,
                    transform_to_root_target);

  AddSolidColorQuadWithBlendMode(SurfaceSize(), root_pass.get(), blend_modes[0],
                                 gfx::MaskFilterInfo());
  auto* child_one_surface_quad =
      root_pass->CreateAndAppendDrawQuad<SurfaceDrawQuad>();
  child_one_surface_quad->SetNew(
      root_pass->shared_quad_state_list.back(), gfx::Rect(SurfaceSize()),
      gfx::Rect(SurfaceSize()),
      SurfaceRange(base::nullopt, child_one_surface_id), SK_ColorWHITE,
      /*stretch_content_to_fill_bounds=*/false);
  AddSolidColorQuadWithBlendMode(SurfaceSize(), root_pass.get(), blend_modes[4],
                                 gfx::MaskFilterInfo());
  auto* child_two_surface_quad =
      root_pass->CreateAndAppendDrawQuad<SurfaceDrawQuad>();
  child_two_surface_quad->SetNew(
      root_pass->shared_quad_state_list.back(), gfx::Rect(SurfaceSize()),
      gfx::Rect(SurfaceSize()),
      SurfaceRange(base::nullopt, child_two_surface_id), SK_ColorWHITE,
      /*stretch_content_to_fill_bounds=*/false);
  AddSolidColorQuadWithBlendMode(SurfaceSize(), root_pass.get(), blend_modes[6],
                                 gfx::MaskFilterInfo());

  QueuePassAsFrame(std::move(root_pass), root_local_surface_id_,
                   device_scale_factor, root_sink_.get());

  SurfaceId root_surface_id(root_sink_->frame_sink_id(),
                            root_local_surface_id_);
  auto aggregated_frame = AggregateFrame(root_surface_id);

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
// bounds correctly within aggregated passes. In case of fast rounded corners,
// the surface aggregator tries to optimize by merging the the surface quads
// instead of keeping the surface render pass.
//
// This test has 4 surfaces in the following structure:
// root_surface         -> [child_root_surface] has fast rounded corner [1],
// child_root_surface   -> [child_one_surface],
//                         [child_two_surface],
//                         quad (a),
// child_one_surface    -> quad (b),
//                         [child three surface],
// child_two_surface    -> quad (c),
//                      -> quad (d) has rounded corner [2]
// child_three_surface  -> quad (e),
//
// Resulting in the following aggregated pass:
// Root Pass:
//  quad (b)          - rounded corner [1]
//  quad (e)          - rounded corner [1]
//  render pass quad  - rounded corner [1]
//  quad (a)          - rounded corner [1]
// Render pass for child two surface:
//  quad (c)          - no rounded corner on sqs
//  quad(d)           - rounded corner [2]
TEST_F(SurfaceAggregatorValidSurfaceTest,
       AggregateSharedQuadStateRoundedCornerBounds) {
  const gfx::MaskFilterInfo kMaskFilterInfoWithFastRoundedCorners(
      gfx::RRectF(0, 0, 640, 480, 5));
  const gfx::MaskFilterInfo kMaskFilterInfoWithRoundedCorners(
      gfx::RRectF(0, 0, 100, 100, 2));

  ParentLocalSurfaceIdAllocator child_root_allocator;
  ParentLocalSurfaceIdAllocator child_one_allocator;
  ParentLocalSurfaceIdAllocator child_two_allocator;
  ParentLocalSurfaceIdAllocator child_three_allocator;
  auto child_root_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryFrameSinkId1, kChildIsRoot);
  auto child_one_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryFrameSinkId2, kChildIsRoot);
  auto child_two_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryMiddleFrameSinkId, kChildIsRoot);
  auto child_three_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryFrameSinkId3, kChildIsRoot);
  CompositorRenderPassId pass_id{1};

  gfx::Rect output_rect(SurfaceSize());
  gfx::Rect damage_rect(SurfaceSize());
  constexpr float device_scale_factor = 1.0f;
  gfx::Transform transform_to_root_target;

  // Setup childe three surface.
  child_three_allocator.GenerateId();
  LocalSurfaceId child_three_local_surface_id =
      child_three_allocator.GetCurrentLocalSurfaceId();
  SurfaceId child_three_surface_id(child_three_support->frame_sink_id(),
                                   child_three_local_surface_id);

  auto child_three_pass = CompositorRenderPass::Create();
  child_three_pass->SetNew(pass_id, output_rect, damage_rect,
                           transform_to_root_target);
  AddSolidColorQuadWithBlendMode(SurfaceSize(), child_three_pass.get(),
                                 SkBlendMode::kSrcOver, gfx::MaskFilterInfo());
  QueuePassAsFrame(std::move(child_three_pass), child_three_local_surface_id,
                   device_scale_factor, child_three_support.get());

  // Setup Child one surface
  child_one_allocator.GenerateId();
  LocalSurfaceId child_one_local_surface_id =
      child_one_allocator.GetCurrentLocalSurfaceId();
  SurfaceId child_one_surface_id(child_one_support->frame_sink_id(),
                                 child_one_local_surface_id);

  auto child_one_pass = CompositorRenderPass::Create();
  child_one_pass->SetNew(pass_id, output_rect, damage_rect,
                         transform_to_root_target);
  AddSolidColorQuadWithBlendMode(SurfaceSize(), child_one_pass.get(),
                                 SkBlendMode::kSrcOver, gfx::MaskFilterInfo());

  // Add child three surface quad
  auto* child_three_surface_sqs =
      child_one_pass->CreateAndAppendSharedQuadState();
  child_three_surface_sqs->opacity = 1.f;
  auto* child_three_surface_quad =
      child_one_pass->CreateAndAppendDrawQuad<SurfaceDrawQuad>();
  child_three_surface_quad->SetNew(
      child_three_surface_sqs, gfx::Rect(SurfaceSize()),
      gfx::Rect(SurfaceSize()),
      SurfaceRange(base::nullopt, child_three_surface_id), SK_ColorWHITE,
      /*stretch_content_to_fill_bounds=*/false);

  QueuePassAsFrame(std::move(child_one_pass), child_one_local_surface_id,
                   device_scale_factor, child_one_support.get());

  // Setup child two surface
  child_two_allocator.GenerateId();
  LocalSurfaceId child_two_local_surface_id =
      child_two_allocator.GetCurrentLocalSurfaceId();
  SurfaceId child_two_surface_id(child_two_support->frame_sink_id(),
                                 child_two_local_surface_id);

  auto child_two_pass = CompositorRenderPass::Create();
  child_two_pass->SetNew(pass_id, output_rect, damage_rect,
                         transform_to_root_target);
  AddSolidColorQuadWithBlendMode(SurfaceSize(), child_two_pass.get(),
                                 SkBlendMode::kSrcOver, gfx::MaskFilterInfo());
  AddSolidColorQuadWithBlendMode(SurfaceSize(), child_two_pass.get(),
                                 SkBlendMode::kSrcOver,
                                 kMaskFilterInfoWithRoundedCorners);
  QueuePassAsFrame(std::move(child_two_pass), child_two_local_surface_id,
                   device_scale_factor, child_two_support.get());

  // Setup child root surface
  child_root_allocator.GenerateId();
  LocalSurfaceId child_root_local_surface_id =
      child_root_allocator.GetCurrentLocalSurfaceId();
  SurfaceId child_root_surface_id(child_root_support->frame_sink_id(),
                                  child_root_local_surface_id);

  auto child_root_pass = CompositorRenderPass::Create();
  child_root_pass->SetNew(pass_id, output_rect, damage_rect,
                          transform_to_root_target);

  // Add child one surface quad
  auto* child_one_surface_sqs =
      child_root_pass->CreateAndAppendSharedQuadState();
  child_one_surface_sqs->opacity = 1.f;
  auto* child_one_surface_quad =
      child_root_pass->CreateAndAppendDrawQuad<SurfaceDrawQuad>();
  child_one_surface_quad->SetNew(
      child_one_surface_sqs, gfx::Rect(SurfaceSize()), gfx::Rect(SurfaceSize()),
      SurfaceRange(base::nullopt, child_one_surface_id), SK_ColorWHITE,
      /*stretch_content_to_fill_bounds=*/false);

  // Add child two surface quad
  auto* child_two_surface_sqs =
      child_root_pass->CreateAndAppendSharedQuadState();
  child_two_surface_sqs->opacity = 1.f;
  auto* child_two_surface_quad =
      child_root_pass->CreateAndAppendDrawQuad<SurfaceDrawQuad>();
  child_two_surface_quad->SetNew(
      child_two_surface_sqs, gfx::Rect(SurfaceSize()), gfx::Rect(SurfaceSize()),
      SurfaceRange(base::nullopt, child_two_surface_id), SK_ColorWHITE,
      /*stretch_content_to_fill_bounds=*/false);

  // Add solid color quad
  AddSolidColorQuadWithBlendMode(SurfaceSize(), child_root_pass.get(),
                                 SkBlendMode::kSrcOver, gfx::MaskFilterInfo());
  QueuePassAsFrame(std::move(child_root_pass), child_root_local_surface_id,
                   device_scale_factor, child_root_support.get());

  auto root_pass = CompositorRenderPass::Create();
  root_pass->SetNew(pass_id, output_rect, damage_rect,
                    transform_to_root_target);

  auto* child_root_surface_sqs = root_pass->CreateAndAppendSharedQuadState();
  auto* child_root_surface_quad =
      root_pass->CreateAndAppendDrawQuad<SurfaceDrawQuad>();
  child_root_surface_sqs->opacity = 1.f;
  child_root_surface_sqs->mask_filter_info =
      kMaskFilterInfoWithFastRoundedCorners;
  child_root_surface_sqs->is_fast_rounded_corner = true;
  child_root_surface_quad->SetNew(
      child_root_surface_sqs, gfx::Rect(SurfaceSize()),
      gfx::Rect(SurfaceSize()),
      SurfaceRange(base::nullopt, child_root_surface_id), SK_ColorWHITE,
      /*stretch_content_to_fill_bounds=*/false);

  QueuePassAsFrame(std::move(root_pass), root_local_surface_id_,
                   device_scale_factor, root_sink_.get());

  SurfaceId root_surface_id(root_sink_->frame_sink_id(),
                            root_local_surface_id_);
  auto aggregated_frame = AggregateFrame(root_surface_id);

  const auto& aggregated_pass_list = aggregated_frame.render_pass_list;

  // There should be 2 render pass since one of the surface quad qould reject
  // merging due to it having a quad with a rounded corner of its own.
  EXPECT_EQ(2u, aggregated_pass_list.size());

  // The surface quad which has a render pass of its own, will have 2 quads.
  // One of them will have the rounded corner set on it.
  const auto& aggregated_quad_list_of_surface =
      aggregated_pass_list[0]->quad_list;
  EXPECT_EQ(2u, aggregated_quad_list_of_surface.size());
  EXPECT_EQ(kMaskFilterInfoWithRoundedCorners,
            aggregated_quad_list_of_surface.back()
                ->shared_quad_state->mask_filter_info);

  // The root render pass will have all the remaining quads with the rounded
  // corner set on them.
  const auto& aggregated_quad_list_of_root = aggregated_pass_list[1]->quad_list;
  EXPECT_EQ(4u, aggregated_quad_list_of_root.size());
  for (const auto* q : aggregated_quad_list_of_root) {
    EXPECT_EQ(q->shared_quad_state->mask_filter_info,
              kMaskFilterInfoWithFastRoundedCorners);
  }
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
      nullptr, &manager_, kArbitraryMiddleFrameSinkId, kChildIsRoot);
  // Innermost child surface.
  ParentLocalSurfaceIdAllocator child_allocator;
  child_allocator.GenerateId();
  LocalSurfaceId child_local_surface_id =
      child_allocator.GetCurrentLocalSurfaceId();
  SurfaceId child_surface_id(child_sink_->frame_sink_id(),
                             child_local_surface_id);
  {
    CompositorRenderPassId child_pass_id[] = {CompositorRenderPassId{1},
                                              CompositorRenderPassId{2}};
    std::vector<Quad> child_quads[2] = {
        {Quad::SolidColorQuad(SK_ColorGREEN, gfx::Rect(5, 5))},
        {Quad::RenderPassQuad(child_pass_id[0], gfx::Transform(), true)},
    };
    std::vector<Pass> child_passes = {
        Pass(child_quads[0], child_pass_id[0], SurfaceSize()),
        Pass(child_quads[1], child_pass_id[1], SurfaceSize())};

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
    child_root_pass_sqs->is_clipped = true;
    child_root_pass_sqs->clip_rect = gfx::Rect(0, 0, 5, 5);

    child_sink_->SubmitCompositorFrame(child_local_surface_id,
                                       std::move(child_frame));
  }

  // Middle child surface.
  ParentLocalSurfaceIdAllocator middle_allocator;
  middle_allocator.GenerateId();
  LocalSurfaceId middle_local_surface_id =
      middle_allocator.GetCurrentLocalSurfaceId();
  SurfaceId middle_surface_id(middle_support->frame_sink_id(),
                              middle_local_surface_id);
  {
    std::vector<Quad> middle_quads = {Quad::SurfaceQuad(
        SurfaceRange(base::nullopt, child_surface_id), SK_ColorWHITE,
        gfx::Rect(5, 5), /*stretch_content_to_fill_bounds=*/false)};
    std::vector<Pass> middle_passes = {
        Pass(middle_quads, SurfaceSize()),
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

    middle_support->SubmitCompositorFrame(middle_local_surface_id,
                                          std::move(middle_frame));
  }

  // Root surface.
  std::vector<Quad> secondary_quads = {
      Quad::SolidColorQuad(1, gfx::Rect(5, 5)),
      Quad::SurfaceQuad(SurfaceRange(base::nullopt, middle_surface_id),
                        SK_ColorWHITE, gfx::Rect(5, 5),
                        /*stretch_content_to_fill_bounds=*/false)};
  std::vector<Quad> root_quads = {Quad::SolidColorQuad(1, gfx::Rect(5, 5))};
  std::vector<Pass> root_passes = {Pass(secondary_quads, SurfaceSize()),
                                   Pass(root_quads, SurfaceSize())};

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

  root_sink_->SubmitCompositorFrame(root_local_surface_id_,
                                    std::move(root_frame));

  SurfaceId root_surface_id(root_sink_->frame_sink_id(),
                            root_local_surface_id_);
  auto aggregated_frame = AggregateFrame(root_surface_id);

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

  EXPECT_TRUE(
      aggregated_pass_list[1]->shared_quad_state_list.ElementAt(1)->is_clipped);

  // The second quad in the root pass is aggregated from the child, so its
  // clip rect must be transformed by the child's translation/scale and
  // clipped be the visible_rects for both children.
  EXPECT_EQ(gfx::Rect(0, 13, 8, 12).ToString(),
            aggregated_pass_list[1]
                ->shared_quad_state_list.ElementAt(1)
                ->clip_rect.ToString());
}

// This test verifies that in the absence of a primary Surface,
// SurfaceAggregator will embed a fallback Surface, if available. If the primary
// surface is available, though, the fallback will not be used.
TEST_P(SurfaceAggregatorValidSurfaceWithMergingPassesTest,
       FallbackSurfaceReference) {
  auto primary_child_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryFrameSinkId1, kChildIsRoot);

  auto fallback_child_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryFrameSinkId2, kChildIsRoot);

  ParentLocalSurfaceIdAllocator fallback_allocator;
  fallback_allocator.GenerateId();
  LocalSurfaceId fallback_child_local_surface_id =
      fallback_allocator.GetCurrentLocalSurfaceId();
  SurfaceId fallback_child_surface_id(fallback_child_support->frame_sink_id(),
                                      fallback_child_local_surface_id);

  ParentLocalSurfaceIdAllocator primary_allocator;
  primary_allocator.GenerateId();
  LocalSurfaceId primary_child_local_surface_id =
      primary_allocator.GetCurrentLocalSurfaceId();
  SurfaceId primary_child_surface_id(primary_child_support->frame_sink_id(),
                                     primary_child_local_surface_id);

  constexpr gfx::Size fallback_size(10, 10);
  std::vector<Quad> fallback_child_quads = {
      Quad::SolidColorQuad(SK_ColorRED, gfx::Rect(fallback_size))};
  std::vector<Pass> fallback_child_passes = {
      Pass(fallback_child_quads, fallback_size)};

  // Submit a CompositorFrame to the fallback Surface containing a red
  // SolidColorDrawQuad.
  constexpr float device_scale_factor_1 = 1.0f;
  constexpr float device_scale_factor_2 = 2.0f;
  SubmitCompositorFrame(fallback_child_support.get(), fallback_child_passes,
                        fallback_child_local_surface_id, device_scale_factor_2);

  // Try to embed |primary_child_surface_id| and if unavailable, embed
  // |fallback_child_surface_id|. The |allow_merge| flag would be set to
  // true/false based on the parameter of the test.
  constexpr gfx::Rect surface_quad_rect(12, 15);
  std::vector<Quad> root_quads = {Quad::SurfaceQuad(
      SurfaceRange(fallback_child_surface_id, primary_child_surface_id),
      SK_ColorWHITE, surface_quad_rect,
      /*stretch_content_to_fill_bounds=*/false, AllowMerge())};
  std::vector<Pass> root_passes = {Pass(root_quads, SurfaceSize())};

  MockAggregatedDamageCallback aggregated_damage_callback;

  root_sink_->SetAggregatedDamageCallbackForTesting(
      aggregated_damage_callback.GetCallback());
  primary_child_support->SetAggregatedDamageCallbackForTesting(
      aggregated_damage_callback.GetCallback());
  fallback_child_support->SetAggregatedDamageCallbackForTesting(
      aggregated_damage_callback.GetCallback());

  SubmitCompositorFrame(root_sink_.get(), root_passes, root_local_surface_id_,
                        device_scale_factor_1);

  // There is no CompositorFrame submitted to |primary_child_surface_id|
  // so |fallback_child_surface_id| will be embedded and we should see a red
  // SolidColorDrawQuad. These quads are in physical pixels.
  Quad right_gutter_quad =
      Quad::SolidColorQuad(SK_ColorWHITE, gfx::Rect(5, 0, 7, 15));
  Quad bottom_gutter_quad =
      Quad::SolidColorQuad(SK_ColorWHITE, gfx::Rect(0, 5, 5, 10));
  Quad render_pass_quad =
      Quad::RenderPassQuad(CompositorRenderPassId{2}, gfx::Transform(), true);
  Quad fallback_quad =
      Quad::SolidColorQuad(SK_ColorRED, gfx::Rect(fallback_size));

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
  expected_passes1.emplace_back(expected_quads1, SurfaceSize());

  SurfaceId root_surface_id(root_sink_->frame_sink_id(),
                            root_local_surface_id_);

  EXPECT_CALL(aggregated_damage_callback,
              OnAggregatedDamage(fallback_child_local_surface_id, fallback_size,
                                 gfx::Rect(fallback_size), next_display_time()))
      .Times(1);
  EXPECT_CALL(aggregated_damage_callback,
              OnAggregatedDamage(primary_child_local_surface_id, _, _, _))
      .Times(0);
  // The whole root surface should be damaged because this is the first
  // aggregation.
  EXPECT_CALL(aggregated_damage_callback,
              OnAggregatedDamage(root_local_surface_id_, SurfaceSize(),
                                 gfx::Rect(SurfaceSize()), next_display_time()))
      .Times(1);

  // The primary_surface will not be listed in previously contained surfaces.
  AggregateAndVerify(expected_passes1,
                     {root_surface_id, fallback_child_surface_id});

  testing::Mock::VerifyAndClearExpectations(&aggregated_damage_callback);

  // Submit the fallback again to create some damage then aggregate again.
  fallback_allocator.GenerateId();
  fallback_child_local_surface_id =
      fallback_allocator.GetCurrentLocalSurfaceId();

  SubmitCompositorFrame(fallback_child_support.get(), fallback_child_passes,
                        fallback_child_local_surface_id, device_scale_factor_2);

  EXPECT_CALL(aggregated_damage_callback,
              OnAggregatedDamage(fallback_child_local_surface_id, fallback_size,
                                 gfx::Rect(fallback_size), next_display_time()))
      .Times(1);
  EXPECT_CALL(aggregated_damage_callback,
              OnAggregatedDamage(primary_child_local_surface_id, _, _, _))
      .Times(0);
  // The damage should be equal to whole size of the primary SurfaceDrawQuad.
  EXPECT_CALL(
      aggregated_damage_callback,
      OnAggregatedDamage(root_local_surface_id_, SurfaceSize(),
                         surface_quad_rect, testing::A<base::TimeTicks>()))
      .Times(1);

  render_pass_quad.render_pass_id = CompositorRenderPassId{3};
  std::vector<Quad> expected_quads2{
      right_gutter_quad, bottom_gutter_quad,
      AllowMerge() ? fallback_quad : render_pass_quad};
  std::vector<Pass> expected_passes2;
  if (!AllowMerge())
    expected_passes2.emplace_back(fallback_child_quads, fallback_size);
  expected_passes2.emplace_back(expected_quads2, SurfaceSize());
  AggregateAndVerify(
      expected_passes2,
      {root_surface_id, SurfaceId(fallback_child_support->frame_sink_id(),
                                  fallback_child_local_surface_id)});

  testing::Mock::VerifyAndClearExpectations(&aggregated_damage_callback);

  std::vector<Quad> primary_child_quads = {
      Quad::SolidColorQuad(SK_ColorGREEN, gfx::Rect(5, 5))};
  constexpr gfx::Size primary_surface_size(5, 5);
  std::vector<Pass> primary_child_passes = {
      Pass(primary_child_quads, primary_surface_size)};

  // Submit a CompositorFrame to the primary Surface containing a green
  // SolidColorDrawQuad.
  SubmitCompositorFrame(primary_child_support.get(), primary_child_passes,
                        primary_child_local_surface_id, device_scale_factor_2);

  // Now that the primary Surface has a CompositorFrame, we expect
  // SurfaceAggregator to embed the primary Surface, and drop the fallback
  // Surface.
  Quad primary_quad = Quad::SolidColorQuad(SK_ColorGREEN, gfx::Rect(5, 5));
  render_pass_quad.render_pass_id = CompositorRenderPassId{4};
  std::vector<Quad> expected_quads3{AllowMerge() ? primary_quad
                                                 : render_pass_quad};
  std::vector<Pass> expected_passes3;
  if (!AllowMerge())
    expected_passes3.emplace_back(primary_child_quads, primary_surface_size);
  expected_passes3.emplace_back(expected_quads3, SurfaceSize());

  EXPECT_CALL(
      aggregated_damage_callback,
      OnAggregatedDamage(primary_child_local_surface_id, primary_surface_size,
                         gfx::Rect(primary_surface_size), next_display_time()))
      .Times(1);
  EXPECT_CALL(aggregated_damage_callback,
              OnAggregatedDamage(fallback_child_local_surface_id, _, _, _))
      .Times(0);
  // The damage of the root should be equal to the damage of the primary
  // surface.
  EXPECT_CALL(
      aggregated_damage_callback,
      OnAggregatedDamage(root_local_surface_id_, SurfaceSize(),
                         gfx::Rect(primary_surface_size), next_display_time()))
      .Times(1);

  AggregateAndVerify(expected_passes3,
                     {root_surface_id, primary_child_surface_id});

  testing::Mock::VerifyAndClearExpectations(&aggregated_damage_callback);
}

// Tests that damage rects are aggregated correctly when surfaces change.
TEST_P(SurfaceAggregatorValidSurfaceWithMergingPassesTest,
       AggregateDamageRect) {
  // Add a callback for when the surface is damaged.
  MockAggregatedDamageCallback aggregated_damage_callback;
  root_sink_->SetAggregatedDamageCallbackForTesting(
      aggregated_damage_callback.GetCallback());

  auto parent_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryMiddleFrameSinkId, kChildIsRoot);
  std::vector<Quad> child_quads = {
      Quad::SolidColorQuad(SK_ColorGREEN, gfx::Rect(5, 5))};
  std::vector<Pass> child_passes = {
      Pass(child_quads, CompositorRenderPassId{1}, SurfaceSize())};

  CompositorFrame child_frame = MakeEmptyCompositorFrame();
  AddPasses(&child_frame.render_pass_list, child_passes,
            &child_frame.metadata.referenced_surfaces);

  auto* child_root_pass = child_frame.render_pass_list[0].get();
  auto* child_root_pass_sqs = child_root_pass->shared_quad_state_list.front();
  child_root_pass_sqs->quad_to_target_transform.Translate(8, 0);

  ParentLocalSurfaceIdAllocator child_allocator;
  child_allocator.GenerateId();
  LocalSurfaceId child_local_surface_id =
      child_allocator.GetCurrentLocalSurfaceId();
  SurfaceId child_surface_id(child_sink_->frame_sink_id(),
                             child_local_surface_id);
  child_sink_->SubmitCompositorFrame(child_local_surface_id,
                                     std::move(child_frame));

  std::vector<Quad> parent_surface_quads = {Quad::SurfaceQuad(
      SurfaceRange(base::nullopt, child_surface_id), SK_ColorWHITE,
      gfx::Rect(5, 5), /*stretch_content_to_fill_bounds=*/false, AllowMerge())};
  std::vector<Pass> parent_surface_passes = {
      Pass(parent_surface_quads, CompositorRenderPassId{1}, SurfaceSize())};

  // Parent surface is only used to test if the transform is applied correctly
  // to the child surface's damage.
  CompositorFrame parent_surface_frame = MakeEmptyCompositorFrame();
  AddPasses(&parent_surface_frame.render_pass_list, parent_surface_passes,
            &parent_surface_frame.metadata.referenced_surfaces);

  ParentLocalSurfaceIdAllocator parent_allocator;
  parent_allocator.GenerateId();
  LocalSurfaceId parent_local_surface_id =
      parent_allocator.GetCurrentLocalSurfaceId();
  SurfaceId parent_surface_id(parent_support->frame_sink_id(),
                              parent_local_surface_id);
  parent_support->SubmitCompositorFrame(parent_local_surface_id,
                                        std::move(parent_surface_frame));

  std::vector<Quad> root_surface_quads = {Quad::SurfaceQuad(
      SurfaceRange(base::nullopt, parent_surface_id), SK_ColorWHITE,
      gfx::Rect(5, 5), /*stretch_content_to_fill_bounds=*/false, AllowMerge())};
  std::vector<Quad> root_render_pass_quads = {
      Quad::RenderPassQuad(CompositorRenderPassId{1}, gfx::Transform(), true)};

  std::vector<Pass> root_passes = {
      Pass(root_surface_quads, CompositorRenderPassId{1}, SurfaceSize()),
      Pass(root_render_pass_quads, CompositorRenderPassId{2}, SurfaceSize())};

  CompositorFrame root_frame = MakeEmptyCompositorFrame();
  AddPasses(&root_frame.render_pass_list, root_passes,
            &root_frame.metadata.referenced_surfaces);

  root_frame.render_pass_list[0]
      ->shared_quad_state_list.front()
      ->quad_to_target_transform.Translate(0, 10);
  root_frame.render_pass_list[0]->damage_rect = gfx::Rect(5, 5, 10, 10);
  root_frame.render_pass_list[1]->damage_rect = gfx::Rect(5, 5, 100, 100);

  root_sink_->SubmitCompositorFrame(root_local_surface_id_,
                                    std::move(root_frame));

  // Damage rect for first aggregation should contain entire root surface. The
  // damage rect reported to the callback is actually 10 pixels taller because
  // of the 10-pixel vertical translation of the first RenderPass.
  SurfaceId root_surface_id(root_sink_->frame_sink_id(),
                            root_local_surface_id_);

  size_t expected_num_passes_after_aggregation = AllowMerge() ? 2u : 4u;
  EXPECT_CALL(
      aggregated_damage_callback,
      OnAggregatedDamage(root_local_surface_id_, SurfaceSize(),
                         gfx::Rect(0, 0, 100, 110), next_display_time()));
  auto aggregated_frame = AggregateFrame(root_surface_id);
  testing::Mock::VerifyAndClearExpectations(&aggregated_damage_callback);
  const auto& aggregated_pass_list = aggregated_frame.render_pass_list;
  ASSERT_EQ(expected_num_passes_after_aggregation, aggregated_pass_list.size());
  EXPECT_EQ(gfx::Rect(SurfaceSize()), aggregated_pass_list[1]->damage_rect);

  {
    CompositorFrame child_frame = MakeEmptyCompositorFrame();
    AddPasses(&child_frame.render_pass_list, child_passes,
              &child_frame.metadata.referenced_surfaces);

    auto* child_root_pass = child_frame.render_pass_list[0].get();
    auto* child_root_pass_sqs = child_root_pass->shared_quad_state_list.front();
    child_root_pass_sqs->quad_to_target_transform.Translate(8, 0);
    child_root_pass->damage_rect = gfx::Rect(10, 10, 10, 10);

    child_sink_->SubmitCompositorFrame(child_local_surface_id,
                                       std::move(child_frame));

    // Outer surface didn't change, so a transformed inner damage rect is
    // expected.
    SurfaceId root_surface_id(root_sink_->frame_sink_id(),
                              root_local_surface_id_);
    const gfx::Rect expected_damage_rect(10, 20, 10, 10);
    EXPECT_CALL(aggregated_damage_callback,
                OnAggregatedDamage(root_local_surface_id_, SurfaceSize(),
                                   expected_damage_rect, next_display_time()));
    auto aggregated_frame = AggregateFrame(root_surface_id);
    const auto& aggregated_pass_list = aggregated_frame.render_pass_list;
    ASSERT_EQ(expected_num_passes_after_aggregation,
              aggregated_pass_list.size());
    EXPECT_EQ(expected_damage_rect.ToString(),
              aggregated_pass_list.back()->damage_rect.ToString());
  }

  {
    CompositorFrame root_frame = MakeEmptyCompositorFrame();
    AddPasses(&root_frame.render_pass_list, root_passes,
              &root_frame.metadata.referenced_surfaces);

    root_frame.render_pass_list[0]
        ->shared_quad_state_list.front()
        ->quad_to_target_transform.Translate(0, 10);
    root_frame.render_pass_list[0]->damage_rect = gfx::Rect(0, 0, 1, 1);

    root_sink_->SubmitCompositorFrame(root_local_surface_id_,
                                      std::move(root_frame));
  }

  {
    CompositorFrame root_frame = MakeEmptyCompositorFrame();
    AddPasses(&root_frame.render_pass_list, root_passes,
              &root_frame.metadata.referenced_surfaces);

    root_frame.render_pass_list[0]
        ->shared_quad_state_list.front()
        ->quad_to_target_transform.Translate(0, 10);
    root_frame.render_pass_list[0]->damage_rect = gfx::Rect(1, 1, 1, 1);

    root_sink_->SubmitCompositorFrame(root_local_surface_id_,
                                      std::move(root_frame));

    // The root surface was enqueued without being aggregated once, so it should
    // be treated as completely damaged.
    SurfaceId root_surface_id(root_sink_->frame_sink_id(),
                              root_local_surface_id_);
    EXPECT_CALL(
        aggregated_damage_callback,
        OnAggregatedDamage(root_local_surface_id_, SurfaceSize(),
                           gfx::Rect(SurfaceSize()), next_display_time()));
    auto aggregated_frame = AggregateFrame(root_surface_id);
    const auto& aggregated_pass_list = aggregated_frame.render_pass_list;
    ASSERT_EQ(expected_num_passes_after_aggregation,
              aggregated_pass_list.size());
    EXPECT_EQ(gfx::Rect(SurfaceSize()),
              aggregated_pass_list.back()->damage_rect);
  }

  // No Surface changed, so no damage should be given.
  {
    SurfaceId root_surface_id(root_sink_->frame_sink_id(),
                              root_local_surface_id_);
    EXPECT_CALL(aggregated_damage_callback, OnAggregatedDamage(_, _, _, _))
        .Times(0);
    auto aggregated_frame = AggregateFrame(root_surface_id);
    const auto& aggregated_pass_list = aggregated_frame.render_pass_list;
    ASSERT_EQ(expected_num_passes_after_aggregation,
              aggregated_pass_list.size());
    EXPECT_TRUE(aggregated_pass_list.back()->damage_rect.IsEmpty());
  }

  // SetFullDamageRectForSurface should cause the entire output to be
  // marked as damaged.
  {
    aggregator_.SetFullDamageForSurface(root_surface_id);
    EXPECT_CALL(
        aggregated_damage_callback,
        OnAggregatedDamage(root_local_surface_id_, SurfaceSize(),
                           gfx::Rect(SurfaceSize()), next_display_time()));
    auto aggregated_frame = AggregateFrame(root_surface_id);
    const auto& aggregated_pass_list = aggregated_frame.render_pass_list;
    ASSERT_EQ(expected_num_passes_after_aggregation,
              aggregated_pass_list.size());
    EXPECT_TRUE(aggregated_pass_list.back()->damage_rect.Contains(
        gfx::Rect(SurfaceSize())));
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
      nullptr, &manager_, kArbitraryMiddleFrameSinkId, kChildIsRoot);
  std::vector<Quad> child_quads = {
      Quad::SolidColorQuad(SK_ColorGREEN, gfx::Rect(5, 5))};
  std::vector<Pass> child_passes = {
      Pass(child_quads, CompositorRenderPassId{1}, gfx::Size(100, 100))};

  CompositorFrame child_frame = MakeEmptyCompositorFrame();
  AddPasses(&child_frame.render_pass_list, child_passes,
            &child_frame.metadata.referenced_surfaces);

  auto* child_root_pass = child_frame.render_pass_list[0].get();
  auto* child_root_pass_sqs = child_root_pass->shared_quad_state_list.front();
  child_root_pass_sqs->quad_to_target_transform.Translate(8, 0);

  ParentLocalSurfaceIdAllocator child_allocator;
  child_allocator.GenerateId();
  LocalSurfaceId child_local_surface_id =
      child_allocator.GetCurrentLocalSurfaceId();
  SurfaceId child_surface_id(child_sink_->frame_sink_id(),
                             child_local_surface_id);
  child_sink_->SubmitCompositorFrame(child_local_surface_id,
                                     std::move(child_frame));

  std::vector<Quad> parent_surface_quads = {Quad::SurfaceQuad(
      SurfaceRange(base::nullopt, child_surface_id), SK_ColorWHITE,
      gfx::Rect(5, 5), /*stretch_content_to_fill_bounds=*/false, AllowMerge())};
  std::vector<Pass> parent_surface_passes = {
      Pass(parent_surface_quads, CompositorRenderPassId{1}, SurfaceSize())};

  // Parent surface is only used to test if the transform is applied correctly
  // to the child surface's damage.
  CompositorFrame parent_surface_frame = MakeEmptyCompositorFrame();
  AddPasses(&parent_surface_frame.render_pass_list, parent_surface_passes,
            &parent_surface_frame.metadata.referenced_surfaces);

  ParentLocalSurfaceIdAllocator parent_allocator;
  parent_allocator.GenerateId();
  LocalSurfaceId parent_local_surface_id =
      parent_allocator.GetCurrentLocalSurfaceId();
  SurfaceId parent_surface_id(parent_support->frame_sink_id(),
                              parent_local_surface_id);
  parent_support->SubmitCompositorFrame(parent_local_surface_id,
                                        std::move(parent_surface_frame));

  std::vector<Quad> root_surface_quads = {
      Quad::SurfaceQuad(SurfaceRange(base::nullopt, parent_surface_id),
                        SK_ColorWHITE, gfx::Rect(50, 50),
                        /*stretch_content_to_fill_bounds=*/true, AllowMerge())};
  std::vector<Quad> root_render_pass_quads = {
      Quad::RenderPassQuad(CompositorRenderPassId{1}, gfx::Transform(), true)};

  std::vector<Pass> root_passes = {
      Pass(root_surface_quads, CompositorRenderPassId{1}, SurfaceSize()),
      Pass(root_render_pass_quads, CompositorRenderPassId{2}, SurfaceSize())};

  SubmitCompositorFrame(root_sink_.get(), root_passes, root_local_surface_id_,
                        1.0f);

  // Damage rect for first aggregation should be exactly the entire root
  // surface.
  SurfaceId root_surface_id(root_sink_->frame_sink_id(),
                            root_local_surface_id_);

  size_t expected_num_passes_after_aggregation = AllowMerge() ? 2u : 4u;
  EXPECT_CALL(
      aggregated_damage_callback,
      OnAggregatedDamage(root_local_surface_id_, SurfaceSize(),
                         gfx::Rect(SurfaceSize()), next_display_time()));
  auto aggregated_frame = AggregateFrame(root_surface_id);
  testing::Mock::VerifyAndClearExpectations(&aggregated_damage_callback);
  const auto& aggregated_pass_list = aggregated_frame.render_pass_list;
  ASSERT_EQ(expected_num_passes_after_aggregation, aggregated_pass_list.size());
  EXPECT_EQ(gfx::Rect(SurfaceSize()), aggregated_pass_list.back()->damage_rect);

  {
    CompositorFrame child_frame = MakeEmptyCompositorFrame();
    AddPasses(&child_frame.render_pass_list, child_passes,
              &child_frame.metadata.referenced_surfaces);

    auto* child_root_pass = child_frame.render_pass_list[0].get();
    child_root_pass->damage_rect = gfx::Rect(10, 20, 20, 30);

    child_sink_->SubmitCompositorFrame(child_local_surface_id,
                                       std::move(child_frame));

    // Outer surface didn't change, so transformed inner damage rect should be
    // used. Since the child surface is stretching to fit the outer surface
    // which is half the size, we end up with a damage rect that is half the
    // size of the child surface.
    SurfaceId root_surface_id(root_sink_->frame_sink_id(),
                              root_local_surface_id_);
    const gfx::Rect expected_damage_rect(5, 10, 10, 15);
    EXPECT_CALL(aggregated_damage_callback,
                OnAggregatedDamage(root_local_surface_id_, SurfaceSize(),
                                   expected_damage_rect, next_display_time()));
    auto aggregated_frame = AggregateFrame(root_surface_id);
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
      nullptr, &manager_, kArbitraryMiddleFrameSinkId, kChildIsRoot);
  std::vector<Quad> child_quads = {
      Quad::SolidColorQuad(SK_ColorGREEN, gfx::Rect(5, 5))};
  std::vector<Pass> child_passes = {
      Pass(child_quads, CompositorRenderPassId{1}, gfx::Size(100, 100))};

  CompositorFrame child_frame = MakeEmptyCompositorFrame();
  AddPasses(&child_frame.render_pass_list, child_passes,
            &child_frame.metadata.referenced_surfaces);

  auto* child_root_pass = child_frame.render_pass_list[0].get();
  auto* child_root_pass_sqs = child_root_pass->shared_quad_state_list.front();
  child_root_pass_sqs->quad_to_target_transform.Translate(8, 0);

  ParentLocalSurfaceIdAllocator child_allocator;
  child_allocator.GenerateId();
  LocalSurfaceId child_local_surface_id =
      child_allocator.GetCurrentLocalSurfaceId();
  SurfaceId child_surface_id(child_sink_->frame_sink_id(),
                             child_local_surface_id);
  child_sink_->SubmitCompositorFrame(child_local_surface_id,
                                     std::move(child_frame));

  std::vector<Quad> parent_surface_quads = {Quad::SurfaceQuad(
      SurfaceRange(base::nullopt, child_surface_id), SK_ColorWHITE,
      gfx::Rect(5, 5), /*stretch_content_to_fill_bounds=*/false, AllowMerge())};
  std::vector<Pass> parent_surface_passes = {
      Pass(parent_surface_quads, CompositorRenderPassId{1}, SurfaceSize())};

  // Parent surface is only used to test if the transform is applied correctly
  // to the child surface's damage.
  CompositorFrame parent_surface_frame = MakeEmptyCompositorFrame();
  AddPasses(&parent_surface_frame.render_pass_list, parent_surface_passes,
            &parent_surface_frame.metadata.referenced_surfaces);

  ParentLocalSurfaceIdAllocator parent_allocator;
  parent_allocator.GenerateId();
  LocalSurfaceId parent_local_surface_id =
      parent_allocator.GetCurrentLocalSurfaceId();
  SurfaceId parent_surface_id(parent_support->frame_sink_id(),
                              parent_local_surface_id);
  parent_support->SubmitCompositorFrame(parent_local_surface_id,
                                        std::move(parent_surface_frame));

  std::vector<Quad> root_surface_quads = {
      Quad::SurfaceQuad(SurfaceRange(base::nullopt, parent_surface_id),
                        SK_ColorWHITE, gfx::Rect(200, 200),
                        /*stretch_content_to_fill_bounds=*/true, AllowMerge())};
  std::vector<Quad> root_render_pass_quads = {
      Quad::RenderPassQuad(CompositorRenderPassId{1}, gfx::Transform(), true)};

  std::vector<Pass> root_passes = {
      Pass(root_surface_quads, CompositorRenderPassId{1}, SurfaceSize()),
      Pass(root_render_pass_quads, CompositorRenderPassId{2}, SurfaceSize())};

  SubmitCompositorFrame(root_sink_.get(), root_passes, root_local_surface_id_,
                        1.0f);

  // Damage rect for first aggregation should contain entire root surface. The
  // damage rect reported to the callback is actually 200x200, larger than the
  // root surface size, because the root's Quad is 200x200.
  SurfaceId root_surface_id(root_sink_->frame_sink_id(),
                            root_local_surface_id_);
  size_t expected_num_passes_after_aggregation = AllowMerge() ? 2u : 4u;
  EXPECT_CALL(
      aggregated_damage_callback,
      OnAggregatedDamage(root_local_surface_id_, SurfaceSize(),
                         gfx::Rect(0, 0, 200, 200), next_display_time()));
  auto aggregated_frame = AggregateFrame(root_surface_id);
  testing::Mock::VerifyAndClearExpectations(&aggregated_damage_callback);
  const auto& aggregated_pass_list = aggregated_frame.render_pass_list;
  ASSERT_EQ(expected_num_passes_after_aggregation, aggregated_pass_list.size());
  EXPECT_EQ(gfx::Rect(SurfaceSize()), aggregated_pass_list.back()->damage_rect);

  {
    CompositorFrame child_frame = MakeEmptyCompositorFrame();
    AddPasses(&child_frame.render_pass_list, child_passes,
              &child_frame.metadata.referenced_surfaces);

    auto* child_root_pass = child_frame.render_pass_list[0].get();
    child_root_pass->damage_rect = gfx::Rect(10, 15, 20, 30);

    child_sink_->SubmitCompositorFrame(child_local_surface_id,
                                       std::move(child_frame));

    // Outer surface didn't change, so transformed inner damage rect should be
    // used. Since the child surface is stretching to fit the outer surface
    // which is twice the size, we end up with a damage rect that is double the
    // size of the child surface.
    SurfaceId root_surface_id(root_sink_->frame_sink_id(),
                              root_local_surface_id_);
    const gfx::Rect expected_damage_rect(20, 30, 40, 60);
    EXPECT_CALL(aggregated_damage_callback,
                OnAggregatedDamage(root_local_surface_id_, SurfaceSize(),
                                   expected_damage_rect, next_display_time()));
    auto aggregated_frame = AggregateFrame(root_surface_id);
    const auto& aggregated_pass_list = aggregated_frame.render_pass_list;
    ASSERT_EQ(expected_num_passes_after_aggregation,
              aggregated_pass_list.size());
    EXPECT_EQ(expected_damage_rect, aggregated_pass_list.back()->damage_rect);
  }
}

// Check that damage is correctly calculated for surfaces.
TEST_F(SurfaceAggregatorValidSurfaceTest, SwitchSurfaceDamage) {
  std::vector<Quad> root_render_pass_quads = {
      Quad::SolidColorQuad(1, gfx::Rect(5, 5))};

  std::vector<Pass> root_passes = {
      Pass(root_render_pass_quads, CompositorRenderPassId{2}, SurfaceSize())};

  CompositorFrame root_frame = MakeEmptyCompositorFrame();
  AddPasses(&root_frame.render_pass_list, root_passes,
            &root_frame.metadata.referenced_surfaces);

  root_frame.render_pass_list[0]->damage_rect = gfx::Rect(5, 5, 100, 100);

  root_sink_->SubmitCompositorFrame(root_local_surface_id_,
                                    std::move(root_frame));

  {
    SurfaceId root_surface_id(root_sink_->frame_sink_id(),
                              root_local_surface_id_);
    auto aggregated_frame = AggregateFrame(root_surface_id);

    const auto& aggregated_pass_list = aggregated_frame.render_pass_list;

    ASSERT_EQ(1u, aggregated_pass_list.size());

    // Damage rect for first aggregation should contain entire root surface.
    EXPECT_TRUE(aggregated_pass_list[0]->damage_rect.Contains(
        gfx::Rect(SurfaceSize())));
  }

  LocalSurfaceId second_root_local_surface_id =
      root_allocator_.GetCurrentLocalSurfaceId();
  SurfaceId second_root_surface_id(root_sink_->frame_sink_id(),
                                   second_root_local_surface_id);
  {
    std::vector<Quad> root_render_pass_quads = {
        Quad::SolidColorQuad(1, gfx::Rect(5, 5))};

    std::vector<Pass> root_passes = {
        Pass(root_render_pass_quads, CompositorRenderPassId{2}, SurfaceSize())};

    CompositorFrame root_frame = MakeEmptyCompositorFrame();
    AddPasses(&root_frame.render_pass_list, root_passes,
              &root_frame.metadata.referenced_surfaces);

    root_frame.render_pass_list[0]->damage_rect = gfx::Rect(1, 2, 3, 4);

    root_sink_->SubmitCompositorFrame(second_root_local_surface_id,
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
      nullptr, &manager_, kArbitraryFrameSinkId1, kRootIsRoot);
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
  std::vector<Quad> embedded_quads = {
      Quad::SolidColorQuad(SK_ColorGREEN, gfx::Rect(5, 5)),
      Quad::SolidColorQuad(SK_ColorBLUE, gfx::Rect(5, 5))};
  std::vector<Pass> embedded_passes = {Pass(embedded_quads, SurfaceSize())};

  constexpr float device_scale_factor = 1.0f;
  SubmitCompositorFrame(embedded_support.get(), embedded_passes, id2,
                        device_scale_factor);

  CompositorFrame frame = MakeCompositorFrameFromSurfaceRanges(
      {SurfaceRange(fallback_surface_id, primary_surface_id)});
  root_sink_->SubmitCompositorFrame(root_local_surface_id_, std::move(frame));

  SurfaceId root_surface_id(root_sink_->frame_sink_id(),
                            root_local_surface_id_);
  auto aggregated_frame = AggregateFrame(root_surface_id);

  // |id1| is before the fallback id so it shouldn't damage the display.
  EXPECT_FALSE(aggregator_.NotifySurfaceDamageAndCheckForDisplayDamage(
      SurfaceId(kArbitraryFrameSinkId1, id1)));

  // |id2| is the fallback id so it should damage the display.
  EXPECT_TRUE(aggregator_.NotifySurfaceDamageAndCheckForDisplayDamage(
      SurfaceId(kArbitraryFrameSinkId1, id2)));

  // |id3| is between fallback and primary so it should damage the display.
  EXPECT_TRUE(aggregator_.NotifySurfaceDamageAndCheckForDisplayDamage(
      SurfaceId(kArbitraryFrameSinkId1, id3)));

  // |id4| is the primary id so it should damage the display.
  EXPECT_TRUE(aggregator_.NotifySurfaceDamageAndCheckForDisplayDamage(
      SurfaceId(kArbitraryFrameSinkId1, id4)));

  // |id5| is newer than the primary surface so it shouldn't damage display.
  EXPECT_FALSE(aggregator_.NotifySurfaceDamageAndCheckForDisplayDamage(
      SurfaceId(kArbitraryFrameSinkId1, id5)));

  // This FrameSinkId is not embedded at all so it shouldn't damage the display.
  EXPECT_FALSE(aggregator_.NotifySurfaceDamageAndCheckForDisplayDamage(
      SurfaceId(kArbitraryFrameSinkId3, id3)));
}

// Verifies that only damage to primary and fallback surfaces and nothing in
// between damages the display if primary and fallback have different
// FrameSinkIds.
TEST_F(SurfaceAggregatorValidSurfaceTest, SurfaceDamageDifferentFrameSinkId) {
  auto embedded_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryFrameSinkId1, kRootIsRoot);
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
  std::vector<Quad> embedded_quads = {
      Quad::SolidColorQuad(SK_ColorGREEN, gfx::Rect(5, 5)),
      Quad::SolidColorQuad(SK_ColorBLUE, gfx::Rect(5, 5))};
  std::vector<Pass> embedded_passes = {Pass(embedded_quads, SurfaceSize())};

  constexpr float device_scale_factor = 1.0f;
  SubmitCompositorFrame(embedded_support.get(), embedded_passes, id2,
                        device_scale_factor);

  CompositorFrame frame = MakeCompositorFrameFromSurfaceRanges(
      {SurfaceRange(fallback_surface_id, primary_surface_id)});
  root_sink_->SubmitCompositorFrame(root_local_surface_id_, std::move(frame));

  SurfaceId root_surface_id(root_sink_->frame_sink_id(),
                            root_local_surface_id_);
  auto aggregated_frame = AggregateFrame(root_surface_id);

  // |id1| is before the fallback id so it shouldn't damage the display.
  EXPECT_FALSE(aggregator_.NotifySurfaceDamageAndCheckForDisplayDamage(
      SurfaceId(kArbitraryFrameSinkId1, id1)));

  // |id2| is the fallback id so it should damage the display.
  EXPECT_TRUE(aggregator_.NotifySurfaceDamageAndCheckForDisplayDamage(
      SurfaceId(kArbitraryFrameSinkId1, id2)));

  // |id3| is before the primary and fallback has a different FrameSinkId so it
  // should damage the display.
  EXPECT_TRUE(aggregator_.NotifySurfaceDamageAndCheckForDisplayDamage(
      SurfaceId(kArbitraryFrameSinkId2, id3)));

  // |id4| is the primary id so it should damage the display.
  EXPECT_TRUE(aggregator_.NotifySurfaceDamageAndCheckForDisplayDamage(
      SurfaceId(kArbitraryFrameSinkId2, id4)));

  // This FrameSinkId is not embedded at all so it shouldn't damage the display.
  EXPECT_FALSE(aggregator_.NotifySurfaceDamageAndCheckForDisplayDamage(
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

  CompositorFrame frame = MakeCompositorFrameFromSurfaceRanges(
      {SurfaceRange(base::nullopt, primary_surface_id)});
  root_sink_->SubmitCompositorFrame(root_local_surface_id_, std::move(frame));

  SurfaceId root_surface_id(root_sink_->frame_sink_id(),
                            root_local_surface_id_);
  auto aggregated_frame = AggregateFrame(root_surface_id);

  // |id1| is inside the range so it should damage the display.
  EXPECT_TRUE(aggregator_.NotifySurfaceDamageAndCheckForDisplayDamage(
      SurfaceId(kArbitraryFrameSinkId1, id1)));

  // |id2| is the primary id so it should damage the display.
  EXPECT_TRUE(aggregator_.NotifySurfaceDamageAndCheckForDisplayDamage(
      SurfaceId(kArbitraryFrameSinkId1, id2)));

  // |id3| is after the primary id so it shouldn't damage the display.
  EXPECT_FALSE(aggregator_.NotifySurfaceDamageAndCheckForDisplayDamage(
      SurfaceId(kArbitraryFrameSinkId1, id3)));

  // This FrameSinkId is not embedded at all so it shouldn't damage the display.
  EXPECT_FALSE(aggregator_.NotifySurfaceDamageAndCheckForDisplayDamage(
      SurfaceId(kArbitraryFrameSinkId3, id4)));
}

// Verifies that when primary and fallback ids are equal, only damage to that
// particular surface causes damage to display.
TEST_F(SurfaceAggregatorValidSurfaceTest,
       SurfaceDamagePrimaryAndFallbackEqual) {
  auto embedded_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryFrameSinkId1, kRootIsRoot);
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

  std::vector<Quad> embedded_quads = {
      Quad::SolidColorQuad(SK_ColorGREEN, gfx::Rect(5, 5)),
      Quad::SolidColorQuad(SK_ColorBLUE, gfx::Rect(5, 5))};
  std::vector<Pass> embedded_passes = {Pass(embedded_quads, SurfaceSize())};
  constexpr float device_scale_factor = 1.0f;
  SubmitCompositorFrame(embedded_support.get(), embedded_passes, id2,
                        device_scale_factor);

  CompositorFrame frame =
      MakeCompositorFrameFromSurfaceRanges({SurfaceRange(surface_id)});
  root_sink_->SubmitCompositorFrame(root_local_surface_id_, std::move(frame));

  SurfaceId root_surface_id(root_sink_->frame_sink_id(),
                            root_local_surface_id_);
  auto aggregated_frame = AggregateFrame(root_surface_id);

  // |id1| is before the fallback id so it shouldn't damage the display.
  EXPECT_FALSE(aggregator_.NotifySurfaceDamageAndCheckForDisplayDamage(
      SurfaceId(kArbitraryFrameSinkId1, id1)));

  // |id2| is the embedded id so it should damage the display.
  EXPECT_TRUE(aggregator_.NotifySurfaceDamageAndCheckForDisplayDamage(
      SurfaceId(kArbitraryFrameSinkId1, id2)));

  // |id3| is newer than primary id so it shouldn't damage the display.
  EXPECT_FALSE(aggregator_.NotifySurfaceDamageAndCheckForDisplayDamage(
      SurfaceId(kArbitraryFrameSinkId1, id3)));

  // This FrameSinkId is not embedded at all so it shouldn't damage the display.
  EXPECT_FALSE(aggregator_.NotifySurfaceDamageAndCheckForDisplayDamage(
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
      Quad::SolidColorQuad(SK_ColorGREEN, gfx::Rect(5, 5))};

  std::vector<Pass> child_passes = {
      Pass(child_quads, CompositorRenderPassId{1}, child_surface_size)};

  CompositorFrame child_frame = MakeEmptyCompositorFrame();
  AddPasses(&child_frame.render_pass_list, child_passes,
            &child_frame.metadata.referenced_surfaces);

  ParentLocalSurfaceIdAllocator child_allocator;
  child_allocator.GenerateId();
  LocalSurfaceId child_local_surface_id =
      child_allocator.GetCurrentLocalSurfaceId();
  SurfaceId child_surface_id(child_sink_->frame_sink_id(),
                             child_local_surface_id);
  child_sink_->SubmitCompositorFrame(child_local_surface_id,
                                     std::move(child_frame));

  // The frame of the root surface has two render passes.
  // - the first pass has a solid color quad,
  // - the second pass has a render pass quad (RPDQ) with its
  // |intersects_damage_under| set to true and a surface quad embedding the
  // child surface.
  std::vector<Quad> render_pass_1_quads = {
      Quad::SolidColorQuad(SK_ColorRED, gfx::Rect(5, 5))};
  std::vector<Quad> render_pass_2_quads = {
      // We will verify the correctness of the |intersects_damage_under| flag
      // on this quad.
      Quad::RenderPassQuad(CompositorRenderPassId{1}, gfx::Transform(),
                           /*intersects_damage_under=*/false),
      Quad::SurfaceQuad(SurfaceRange(base::nullopt, child_surface_id),
                        SK_ColorWHITE, gfx::Rect(90, 90),
                        /*stretch_content_to_fill_bounds=*/false,
                        AllowMerge())};

  std::vector<Pass> root_passes{
      Pass(render_pass_1_quads, CompositorRenderPassId{1}, gfx::Size(80, 80)),
      Pass(render_pass_2_quads, CompositorRenderPassId{2}, SurfaceSize())};

  CompositorFrame root_frame = MakeEmptyCompositorFrame();
  AddPasses(&root_frame.render_pass_list, root_passes,
            &root_frame.metadata.referenced_surfaces);
  root_frame.render_pass_list.back()
      ->shared_quad_state_list.front()
      ->quad_to_target_transform.Translate(20, 30);
  root_frame.render_pass_list.back()->damage_rect = gfx::Rect();
  root_sink_->SubmitCompositorFrame(root_local_surface_id_,
                                    std::move(root_frame));
  SurfaceId root_surface_id(root_sink_->frame_sink_id(),
                            root_local_surface_id_);
  size_t expected_num_passes_after_aggregation = AllowMerge() ? 2u : 3u;

  // First aggregation.
  {
    EXPECT_CALL(
        aggregated_damage_callback,
        OnAggregatedDamage(root_local_surface_id_, SurfaceSize(),
                           gfx::Rect(SurfaceSize()), next_display_time()));
    EXPECT_CALL(
        aggregated_damage_callback,
        OnAggregatedDamage(child_local_surface_id, child_surface_size,
                           gfx::Rect(child_surface_size), next_display_time()));
    auto aggregated_frame = AggregateFrame(root_surface_id);
    testing::Mock::VerifyAndClearExpectations(&aggregated_damage_callback);
    const auto& aggregated_pass_list = aggregated_frame.render_pass_list;
    ASSERT_EQ(expected_num_passes_after_aggregation,
              aggregated_pass_list.size());

    // Root frame damage rect for the first aggregation should contain the
    // entire root rect.
    EXPECT_EQ(gfx::Rect(SurfaceSize()),
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
    CompositorFrame root_frame = MakeEmptyCompositorFrame();
    AddPasses(&root_frame.render_pass_list, root_passes,
              &root_frame.metadata.referenced_surfaces);
    root_frame.render_pass_list.back()
        ->shared_quad_state_list.front()
        ->quad_to_target_transform.Translate(20, 30);
    root_frame.render_pass_list.back()->damage_rect = gfx::Rect();

    root_sink_->SubmitCompositorFrame(root_local_surface_id_,
                                      std::move(root_frame));
    EXPECT_CALL(aggregated_damage_callback,
                OnAggregatedDamage(root_local_surface_id_, _, _, _))
        .Times(0);

    // No damage is expected from the child surface.
    EXPECT_CALL(aggregated_damage_callback,
                OnAggregatedDamage(child_local_surface_id, _, _, _))
        .Times(0);

    auto aggregated_frame = AggregateFrame(root_surface_id);
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

    child_sink_->SubmitCompositorFrame(child_local_surface_id,
                                       std::move(child_frame));

    // Both the root and child surface should expect the damage from the child
    // surface (10,10 10x10)
    const gfx::Rect expected_damage_rect(10, 10, 10, 10);
    EXPECT_CALL(aggregated_damage_callback,
                OnAggregatedDamage(root_local_surface_id_, SurfaceSize(),
                                   expected_damage_rect, next_display_time()));
    EXPECT_CALL(aggregated_damage_callback,
                OnAggregatedDamage(child_local_surface_id, child_surface_size,
                                   expected_damage_rect, next_display_time()));

    auto aggregated_frame = AggregateFrame(root_surface_id);
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

    child_sink_->SubmitCompositorFrame(child_local_surface_id,
                                       std::move(child_frame));

    // Both the root and child surface should expect the damage from the child
    // surface (60,60 10x10)
    const gfx::Rect expected_damage_rect(60, 60, 10, 10);
    EXPECT_CALL(aggregated_damage_callback,
                OnAggregatedDamage(root_local_surface_id_, SurfaceSize(),
                                   expected_damage_rect, next_display_time()));
    EXPECT_CALL(aggregated_damage_callback,
                OnAggregatedDamage(child_local_surface_id, child_surface_size,
                                   expected_damage_rect, next_display_time()));

    auto aggregated_frame = AggregateFrame(root_surface_id);
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
      nullptr, &manager_, kArbitraryFrameSinkId1, kChildIsRoot);

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
  std::vector<Quad> child_rp1_quads = {
      Quad::SolidColorQuad(SK_ColorGREEN, gfx::Rect(5, 5))};
  std::vector<Quad> child_rp2_quads = {
      Quad::RenderPassQuad(CompositorRenderPassId{1}, gfx::Transform(),
                           /*intersects_damage_under=*/false)};

  std::vector<Pass> child_passes = {
      Pass(child_rp1_quads, CompositorRenderPassId{1}, child_surface_size),
      Pass(child_rp2_quads, CompositorRenderPassId{2}, child_surface_size)};

  CompositorFrame child_frame = MakeEmptyCompositorFrame();
  AddPasses(&child_frame.render_pass_list, child_passes,
            &child_frame.metadata.referenced_surfaces);
  child_frame.render_pass_list.back()->damage_rect = gfx::Rect();

  ParentLocalSurfaceIdAllocator child_allocator;
  child_allocator.GenerateId();
  LocalSurfaceId child_local_surface_id =
      child_allocator.GetCurrentLocalSurfaceId();
  SurfaceId child_surface_id(child_sink_->frame_sink_id(),
                             child_local_surface_id);
  child_sink_->SubmitCompositorFrame(child_local_surface_id,
                                     std::move(child_frame));

  // The second surface will be embedded into a surface quad on the root pass of
  // the root frame, under the surface quad containing the child surface.
  gfx::Size second_surface_size(80, 80);
  // The root render pass of the second surface has a solid color quad.
  std::vector<Quad> second_surface_quads = {
      Quad::SolidColorQuad(SK_ColorGREEN, gfx::Rect(5, 5))};

  std::vector<Pass> second_surface_passes = {Pass(
      second_surface_quads, CompositorRenderPassId{1}, second_surface_size)};

  CompositorFrame second_surface_frame = MakeEmptyCompositorFrame();
  AddPasses(&second_surface_frame.render_pass_list, second_surface_passes,
            &second_surface_frame.metadata.referenced_surfaces);

  ParentLocalSurfaceIdAllocator second_allocator;
  second_allocator.GenerateId();
  LocalSurfaceId second_local_surface_id =
      second_allocator.GetCurrentLocalSurfaceId();
  SurfaceId second_surface_id(second_support->frame_sink_id(),
                              second_local_surface_id);
  second_support->SubmitCompositorFrame(second_local_surface_id,
                                        std::move(second_surface_frame));

  // The frame of the root surface has one single render pass with a surface
  // quad containing the child surface and a second surface quad containing
  // the second surface.
  std::vector<Quad> render_pass_quads = {
      // The |allow_merge| flag of the surface quad would be set to true/false
      // according to the parameter of the test.
      Quad::SurfaceQuad(SurfaceRange(base::nullopt, child_surface_id),
                        SK_ColorWHITE, gfx::Rect(90, 90),
                        /*stretch_content_to_fill_bounds=*/false, AllowMerge()),
      Quad::SurfaceQuad(SurfaceRange(base::nullopt, second_surface_id),
                        SK_ColorWHITE, gfx::Rect(second_surface_size),
                        /*stretch_content_to_fill_bounds=*/false,
                        /*allow_merge=*/false)};

  std::vector<Pass> root_passes{
      Pass(render_pass_quads, CompositorRenderPassId{1}, SurfaceSize())};

  CompositorFrame root_frame = MakeEmptyCompositorFrame();
  AddPasses(&root_frame.render_pass_list, root_passes,
            &root_frame.metadata.referenced_surfaces);
  root_frame.render_pass_list.back()
      ->shared_quad_state_list.front()
      ->quad_to_target_transform.Translate(20, 30);
  root_sink_->SubmitCompositorFrame(root_local_surface_id_,
                                    std::move(root_frame));

  SurfaceId root_surface_id(root_sink_->frame_sink_id(),
                            root_local_surface_id_);
  size_t expected_num_passes_after_aggregation = AllowMerge() ? 3u : 4u;
  // First aggregation.
  {
    EXPECT_CALL(
        aggregated_damage_callback,
        OnAggregatedDamage(root_local_surface_id_, SurfaceSize(),
                           gfx::Rect(SurfaceSize()), next_display_time()));
    // The damage for the first aggregation should contain
    // the entire child surface (0,0 60x60).
    gfx::Rect expected_child_damage_rect = gfx::Rect(child_surface_size);
    EXPECT_CALL(
        aggregated_damage_callback,
        OnAggregatedDamage(child_local_surface_id, child_surface_size,
                           expected_child_damage_rect, next_display_time()));
    EXPECT_CALL(aggregated_damage_callback,
                OnAggregatedDamage(second_local_surface_id, second_surface_size,
                                   gfx::Rect(second_surface_size),
                                   next_display_time()));

    auto aggregated_frame = AggregateFrame(root_surface_id);
    testing::Mock::VerifyAndClearExpectations(&aggregated_damage_callback);
    const auto& aggregated_pass_list = aggregated_frame.render_pass_list;
    ASSERT_EQ(expected_num_passes_after_aggregation,
              aggregated_pass_list.size());

    // Root frame damage rect for the first aggregation should contain the
    // entire root rect.
    EXPECT_EQ(gfx::Rect(SurfaceSize()),
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
    CompositorFrame child_frame = MakeEmptyCompositorFrame();
    AddPasses(&child_frame.render_pass_list, child_passes,
              &child_frame.metadata.referenced_surfaces);
    child_frame.render_pass_list.back()->damage_rect = gfx::Rect();
    child_sink_->SubmitCompositorFrame(child_local_surface_id,
                                       std::move(child_frame));
    EXPECT_CALL(aggregated_damage_callback,
                OnAggregatedDamage(root_local_surface_id_, _, _, _))
        .Times(0);

    // There should be no damage on any surface.
    EXPECT_CALL(aggregated_damage_callback,
                OnAggregatedDamage(child_local_surface_id, _, _, _))
        .Times(0);

    EXPECT_CALL(aggregated_damage_callback,
                OnAggregatedDamage(second_local_surface_id, _, _, _))
        .Times(0);

    auto aggregated_frame = AggregateFrame(root_surface_id);
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

    second_support->SubmitCompositorFrame(second_local_surface_id,
                                          std::move(second_surface_frame));

    const gfx::Rect expected_damage_rect(10, 10, 10, 10);
    EXPECT_CALL(aggregated_damage_callback,
                OnAggregatedDamage(root_local_surface_id_, SurfaceSize(),
                                   expected_damage_rect, next_display_time()));
    // There is no damage on the child surface.
    EXPECT_CALL(aggregated_damage_callback,
                OnAggregatedDamage(child_local_surface_id, _, _, _))
        .Times(0);

    EXPECT_CALL(aggregated_damage_callback,
                OnAggregatedDamage(second_local_surface_id, second_surface_size,
                                   expected_damage_rect, next_display_time()));
    auto aggregated_frame = AggregateFrame(root_surface_id);
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

    second_support->SubmitCompositorFrame(second_local_surface_id,
                                          std::move(second_surface_frame));
    const gfx::Rect expected_damage_rect(60, 60, 10, 10);
    EXPECT_CALL(aggregated_damage_callback,
                OnAggregatedDamage(root_local_surface_id_, SurfaceSize(),
                                   expected_damage_rect, next_display_time()));
    // There is no damage on the child surface.
    EXPECT_CALL(aggregated_damage_callback,
                OnAggregatedDamage(child_local_surface_id, _, _, _))
        .Times(0);

    EXPECT_CALL(aggregated_damage_callback,
                OnAggregatedDamage(second_local_surface_id, second_surface_size,
                                   expected_damage_rect, next_display_time()));

    auto aggregated_frame = AggregateFrame(root_surface_id);
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
      nullptr, &manager_, kArbitraryMiddleFrameSinkId, kChildIsRoot);

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
      Quad::SolidColorQuad(SK_ColorGREEN, gfx::Rect(5, 5))};
  std::vector<Pass> child_passes = {
      Pass(child_quads, CompositorRenderPassId{1}, child_surface_size)};

  CompositorFrame child_frame = MakeEmptyCompositorFrame();
  AddPasses(&child_frame.render_pass_list, child_passes,
            &child_frame.metadata.referenced_surfaces);

  auto* child_root_pass = child_frame.render_pass_list[0].get();
  auto* child_root_pass_sqs = child_root_pass->shared_quad_state_list.front();
  child_root_pass_sqs->quad_to_target_transform.Translate(8, 0);

  ParentLocalSurfaceIdAllocator child_allocator;
  child_allocator.GenerateId();
  LocalSurfaceId child_local_surface_id =
      child_allocator.GetCurrentLocalSurfaceId();
  SurfaceId child_surface_id(child_sink_->frame_sink_id(),
                             child_local_surface_id);
  child_sink_->SubmitCompositorFrame(child_local_surface_id,
                                     std::move(child_frame));

  constexpr gfx::Size parent_surface_size(90, 90);
  std::vector<Quad> parent_surface_quads = {Quad::SurfaceQuad(
      SurfaceRange(base::nullopt, child_surface_id), SK_ColorWHITE,
      gfx::Rect(5, 5), /*stretch_content_to_fill_bounds=*/false, AllowMerge())};
  std::vector<Pass> parent_surface_passes = {Pass(
      parent_surface_quads, CompositorRenderPassId{1}, parent_surface_size)};

  // Parent surface is only used to test if the transform is applied correctly
  // to the child surface's damage.
  CompositorFrame parent_surface_frame = MakeEmptyCompositorFrame();
  AddPasses(&parent_surface_frame.render_pass_list, parent_surface_passes,
            &parent_surface_frame.metadata.referenced_surfaces);

  ParentLocalSurfaceIdAllocator parent_allocator;
  parent_allocator.GenerateId();
  LocalSurfaceId parent_local_surface_id =
      parent_allocator.GetCurrentLocalSurfaceId();
  SurfaceId parent_surface_id(parent_support->frame_sink_id(),
                              parent_local_surface_id);
  parent_support->SubmitCompositorFrame(parent_local_surface_id,
                                        std::move(parent_surface_frame));

  std::vector<Quad> render_pass_1_quads = {
      Quad::SolidColorQuad(SK_ColorWHITE, gfx::Rect(5, 5))};

  std::vector<Quad> render_pass_2_quads = {
      // Set the |intersects_damage_under| of this CompositorRenderPassDrawQuad
      // to be true. This is the quad that we are testing here. The
      // |intersects_damage_under| should be updated correctly based on the
      // damage of the SurfaceDrawQuad under it.
      Quad::RenderPassQuad(CompositorRenderPassId{1}, gfx::Transform(),
                           /*intersects_damage_under=*/false),
      Quad::SurfaceQuad(SurfaceRange(base::nullopt, parent_surface_id),
                        SK_ColorWHITE, gfx::Rect(5, 5),
                        /*stretch_content_to_fill_bounds=*/false,
                        AllowMerge())};

  std::vector<Pass> root_passes{
      Pass(render_pass_1_quads, CompositorRenderPassId{1}, gfx::Size(50, 50)),
      Pass(render_pass_2_quads, CompositorRenderPassId{2}, SurfaceSize())};

  CompositorFrame root_frame = MakeEmptyCompositorFrame();
  AddPasses(&root_frame.render_pass_list, root_passes,
            &root_frame.metadata.referenced_surfaces);

  root_frame.render_pass_list[0]
      ->shared_quad_state_list.front()
      ->quad_to_target_transform.Translate(0, 10);
  root_frame.render_pass_list[0]->damage_rect = gfx::Rect(5, 5, 10, 10);
  root_frame.render_pass_list[1]->damage_rect = gfx::Rect(5, 5, 100, 100);

  root_sink_->SubmitCompositorFrame(root_local_surface_id_,
                                    std::move(root_frame));
  SurfaceId root_surface_id(root_sink_->frame_sink_id(),
                            root_local_surface_id_);
  size_t expected_num_passes_after_aggregation = AllowMerge() ? 2u : 4u;

  {
    // First aggregation.
    EXPECT_CALL(
        aggregated_damage_callback,
        OnAggregatedDamage(root_local_surface_id_, SurfaceSize(),
                           gfx::Rect(SurfaceSize()), next_display_time()));
    EXPECT_CALL(
        aggregated_damage_callback,
        OnAggregatedDamage(child_local_surface_id, child_surface_size,
                           gfx::Rect(child_surface_size), next_display_time()));
    EXPECT_CALL(aggregated_damage_callback,
                OnAggregatedDamage(parent_local_surface_id, parent_surface_size,
                                   gfx::Rect(parent_surface_size),
                                   next_display_time()));
    auto aggregated_frame = AggregateFrame(root_surface_id);
    testing::Mock::VerifyAndClearExpectations(&aggregated_damage_callback);
    const auto& aggregated_pass_list = aggregated_frame.render_pass_list;
    // After aggregation, there should be two render passes with merging
    // or four render passes without merging.
    ASSERT_EQ(expected_num_passes_after_aggregation,
              aggregated_pass_list.size());
    EXPECT_EQ(gfx::Rect(SurfaceSize()),
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
    root_sink_->SubmitCompositorFrame(root_local_surface_id_,
                                      std::move(root_frame));
    EXPECT_CALL(aggregated_damage_callback,
                OnAggregatedDamage(root_local_surface_id_, _, _, _))
        .Times(0);
    EXPECT_CALL(aggregated_damage_callback,
                OnAggregatedDamage(child_local_surface_id, _, _, _))
        .Times(0);
    EXPECT_CALL(aggregated_damage_callback,
                OnAggregatedDamage(parent_local_surface_id, _, _, _))
        .Times(0);
    auto aggregated_frame = AggregateFrame(root_surface_id);
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

    child_sink_->SubmitCompositorFrame(child_local_surface_id,
                                       std::move(child_frame));

    // Outer surface didn't change, so a transformed inner damage rect is
    // expected.
    const gfx::Rect expected_damage_rect(1, 1, 10, 10);
    EXPECT_CALL(aggregated_damage_callback,
                OnAggregatedDamage(root_local_surface_id_, SurfaceSize(),
                                   expected_damage_rect, next_display_time()));
    EXPECT_CALL(aggregated_damage_callback,
                OnAggregatedDamage(child_local_surface_id, child_surface_size,
                                   expected_damage_rect, next_display_time()));
    EXPECT_CALL(aggregated_damage_callback,
                OnAggregatedDamage(parent_local_surface_id, parent_surface_size,
                                   expected_damage_rect, next_display_time()));

    auto aggregated_frame = AggregateFrame(root_surface_id);
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
    root_sink_->SubmitCompositorFrame(root_local_surface_id_,
                                      std::move(root_frame));
    EXPECT_CALL(aggregated_damage_callback,
                OnAggregatedDamage(root_local_surface_id_, _, _, _))
        .Times(0);
    EXPECT_CALL(aggregated_damage_callback,
                OnAggregatedDamage(child_local_surface_id, _, _, _))
        .Times(0);
    EXPECT_CALL(aggregated_damage_callback,
                OnAggregatedDamage(parent_local_surface_id, _, _, _))
        .Times(0);

    auto aggregated_frame = AggregateFrame(root_surface_id);
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
  const gfx::Size child_surface_size(SurfaceSize());
  std::vector<Quad> child_quads = {
      Quad::SolidColorQuad(SK_ColorGREEN, gfx::Rect(5, 5))};
  std::vector<Pass> child_passes = {Pass(child_quads, child_surface_size)};

  CompositorFrame child_frame = MakeEmptyCompositorFrame();
  AddPasses(&child_frame.render_pass_list, child_passes,
            &child_frame.metadata.referenced_surfaces);

  ParentLocalSurfaceIdAllocator child_allocator;
  child_allocator.GenerateId();
  LocalSurfaceId child_local_surface_id =
      child_allocator.GetCurrentLocalSurfaceId();
  SurfaceId child_surface_id(child_sink_->frame_sink_id(),
                             child_local_surface_id);
  child_sink_->SubmitCompositorFrame(child_local_surface_id,
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
  gfx::Transform translation;
  translation.Translate(30.f, 50.f);
  gfx::Transform translation2;
  translation2.Translate(2.f, 2.f);
  std::vector<Quad> root_quads[] = {
      {Quad::SurfaceQuad(SurfaceRange(base::nullopt, child_surface_id),
                         SK_ColorWHITE, gfx::Rect(5, 5),
                         /*stretch_content_to_fill_bounds=*/false,
                         AllowMerge())},
      {Quad::RenderPassQuad(CompositorRenderPassId{1}, scale, true)},
      {Quad::RenderPassQuad(CompositorRenderPassId{4}, translation2, false),
       Quad::RenderPassQuad(CompositorRenderPassId{2}, translation, true),
       Quad::RenderPassQuad(CompositorRenderPassId{1}, gfx::Transform(), true)},
      {Quad::SolidColorQuad(SK_ColorGREEN, gfx::Rect(5, 5))}};
  std::vector<Pass> root_passes = {
      Pass(root_quads[3], CompositorRenderPassId{4}, gfx::Size(5, 5)),
      Pass(root_quads[0], CompositorRenderPassId{1}, SurfaceSize()),
      Pass(root_quads[1], CompositorRenderPassId{2}, SurfaceSize()),
      Pass(root_quads[2], CompositorRenderPassId{3}, SurfaceSize())};

  SurfaceId root_surface_id(root_sink_->frame_sink_id(),
                            root_local_surface_id_);
  size_t expected_num_passes_after_aggregation = AllowMerge() ? 4u : 5u;
  {
    // First aggregation.
    CompositorFrame root_frame = MakeEmptyCompositorFrame();
    AddPasses(&root_frame.render_pass_list, root_passes,
              &root_frame.metadata.referenced_surfaces);

    root_sink_->SubmitCompositorFrame(root_local_surface_id_,
                                      std::move(root_frame));

    // Damage rect for the first aggregation would contain entire root surface
    // which is union of (0,0 100x100) from RP1, (0,0 200x200) from RP2 and
    // (0,0 230x250) from RP3, a total of (0,0 230x250).
    EXPECT_CALL(
        aggregated_damage_callback,
        OnAggregatedDamage(root_local_surface_id_, SurfaceSize(),
                           gfx::Rect(0, 0, 230, 250), next_display_time()));
    // The child surface is embedded twice so the callback is called twice.
    EXPECT_CALL(
        aggregated_damage_callback,
        OnAggregatedDamage(child_local_surface_id, child_surface_size,
                           gfx::Rect(child_surface_size), next_display_time()))
        .Times(2);

    auto aggregated_frame = AggregateFrame(root_surface_id);
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
    CompositorFrame root_frame = MakeEmptyCompositorFrame();
    AddPasses(&root_frame.render_pass_list, root_passes,
              &root_frame.metadata.referenced_surfaces);
    root_frame.render_pass_list.back()->damage_rect = gfx::Rect();
    root_sink_->SubmitCompositorFrame(root_local_surface_id_,
                                      std::move(root_frame));

    EXPECT_CALL(aggregated_damage_callback,
                OnAggregatedDamage(root_local_surface_id_, _, _, _))
        .Times(0);
    EXPECT_CALL(aggregated_damage_callback,
                OnAggregatedDamage(child_local_surface_id, _, _, _))
        .Times(0);
    auto aggregated_frame = AggregateFrame(root_surface_id);
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

    child_sink_->SubmitCompositorFrame(child_local_surface_id,
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
    EXPECT_CALL(aggregated_damage_callback,
                OnAggregatedDamage(root_local_surface_id_, SurfaceSize(),
                                   expected_damage_rect, next_display_time()));
    EXPECT_CALL(aggregated_damage_callback,
                OnAggregatedDamage(child_local_surface_id, child_surface_size,
                                   gfx::Rect(expected_child_damage_rect),
                                   next_display_time()))
        .Times(2);
    auto aggregated_frame_2 = AggregateFrame(root_surface_id);
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

    child_sink_->SubmitCompositorFrame(child_local_surface_id,
                                       std::move(child_frame_2));

    // The total translation of the quad with |intersects_damage_under| to be
    // tested is now (12,12)
    gfx::Transform& tr =
        const_cast<gfx::Transform&>(root_passes.back().quads[0].transform);
    tr.Translate(10, 10);
    CompositorFrame root_frame = MakeEmptyCompositorFrame();
    AddPasses(&root_frame.render_pass_list, root_passes,
              &root_frame.metadata.referenced_surfaces);
    root_frame.render_pass_list.back()->damage_rect = gfx::Rect();

    root_sink_->SubmitCompositorFrame(root_local_surface_id_,
                                      std::move(root_frame));

    constexpr gfx::Rect expected_damage_rect(10, 10, 60, 80);
    constexpr gfx::Rect expected_child_damage_rect(10, 10, 10, 10);
    EXPECT_CALL(aggregated_damage_callback,
                OnAggregatedDamage(root_local_surface_id_, SurfaceSize(),
                                   expected_damage_rect, next_display_time()));
    EXPECT_CALL(aggregated_damage_callback,
                OnAggregatedDamage(child_local_surface_id, child_surface_size,
                                   gfx::Rect(expected_child_damage_rect),
                                   next_display_time()))
        .Times(2);
    auto aggregated_frame_2 = AggregateFrame(root_surface_id);
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
  std::vector<Quad> child_rp1_quads = {
      Quad::SolidColorQuad(SK_ColorGREEN, gfx::Rect(5, 5))};
  std::vector<Quad> child_rp2_quads = {
      Quad::RenderPassQuad(CompositorRenderPassId{1}, gfx::Transform(),
                           /*intersects_damage_under=*/true)};
  std::vector<Quad> child_rp3_quads = {
      Quad::RenderPassQuad(CompositorRenderPassId{2}, gfx::Transform(),
                           /*intersects_damage_under=*/true)};
  std::vector<Pass> child_passes = {
      Pass(child_rp1_quads, CompositorRenderPassId{1}, child_surface_size),
      Pass(child_rp2_quads, CompositorRenderPassId{2}, child_surface_size),
      Pass(child_rp3_quads, CompositorRenderPassId{3}, child_surface_size)};

  CompositorFrame child_frame = MakeEmptyCompositorFrame();
  AddPasses(&child_frame.render_pass_list, child_passes,
            &child_frame.metadata.referenced_surfaces);

  child_frame.render_pass_list[2]
      ->shared_quad_state_list.ElementAt(0)
      ->quad_to_target_transform.Translate(20, 30);

  child_frame.render_pass_list[1]->backdrop_filters.Append(
      cc::FilterOperation::CreateBlurFilter(5));

  ParentLocalSurfaceIdAllocator child_allocator;
  child_allocator.GenerateId();
  LocalSurfaceId child_local_surface_id =
      child_allocator.GetCurrentLocalSurfaceId();
  SurfaceId child_surface_id(child_sink_->frame_sink_id(),
                             child_local_surface_id);
  child_sink_->SubmitCompositorFrame(child_local_surface_id,
                                     std::move(child_frame));

  // The frame of the root surface has one single render pass with a surface
  // quad containing the child surface.
  std::vector<Quad> render_pass_quads = {
      // The |allow_merge| flag of the surface quad would be set to true/false
      // according to the parameter of the test.
      Quad::SurfaceQuad(SurfaceRange(base::nullopt, child_surface_id),
                        SK_ColorWHITE, gfx::Rect(90, 90),
                        /*stretch_content_to_fill_bounds=*/false,
                        AllowMerge())};

  std::vector<Pass> root_passes{
      Pass(render_pass_quads, CompositorRenderPassId{1}, SurfaceSize())};
  root_passes[0].damage_rect = gfx::Rect(0, 0, 10, 20);

  CompositorFrame root_frame = MakeEmptyCompositorFrame();
  AddPasses(&root_frame.render_pass_list, root_passes,
            &root_frame.metadata.referenced_surfaces);
  root_frame.render_pass_list.back()
      ->shared_quad_state_list.front()
      ->quad_to_target_transform.Translate(5, 5);
  root_sink_->SubmitCompositorFrame(root_local_surface_id_,
                                    std::move(root_frame));

  SurfaceId root_surface_id(root_sink_->frame_sink_id(),
                            root_local_surface_id_);
  size_t expected_num_passes_after_aggregation = AllowMerge() ? 3u : 4u;
  // First aggregation.
  {
    EXPECT_CALL(
        aggregated_damage_callback,
        OnAggregatedDamage(root_local_surface_id_, SurfaceSize(),
                           gfx::Rect(SurfaceSize()), next_display_time()));
    // In the local space of the root pass of the child frame, the second render
    // pass (20,30 60x60) has a blur backdrop filter. It expands the damage from
    // under it. 1) Without merging, the damage for the first aggregation of the
    // child surface is the entire child surface (0,0 60x60), expanded by the
    // backdrop filter, to be (0,0 80x90).
    // 2) With merging, the backdrop filter expands the damage passed from
    // parent surface and transformed into the root pass local space to (-5,-5
    // 100x100), so the child surface has a damage rect of (-5,-5 100x100).
    gfx::Rect expected_child_damage_rect =
        AllowMerge() ? gfx::Rect(-5, -5, 100, 100) : gfx::Rect(0, 0, 80, 90);
    EXPECT_CALL(
        aggregated_damage_callback,
        OnAggregatedDamage(child_local_surface_id, child_surface_size,
                           expected_child_damage_rect, next_display_time()));

    auto aggregated_frame = AggregateFrame(root_surface_id);
    testing::Mock::VerifyAndClearExpectations(&aggregated_damage_callback);
    const auto& aggregated_pass_list = aggregated_frame.render_pass_list;
    ASSERT_EQ(expected_num_passes_after_aggregation,
              aggregated_pass_list.size());

    // Root frame damage rect for the first aggregation should contain the
    // entire root rect.
    EXPECT_EQ(gfx::Rect(SurfaceSize()),
              aggregated_pass_list.back()->damage_rect);
  }

  // Resubmit the root frame.
  {
    CompositorFrame root_frame = MakeEmptyCompositorFrame();
    AddPasses(&root_frame.render_pass_list, root_passes,
              &root_frame.metadata.referenced_surfaces);
    root_sink_->SubmitCompositorFrame(root_local_surface_id_,
                                      std::move(root_frame));
    EXPECT_CALL(
        aggregated_damage_callback,
        OnAggregatedDamage(root_local_surface_id_, SurfaceSize(),
                           gfx::Rect(0, 0, 10, 20), next_display_time()));
    // 1) Without merging, there is no damage on the child surface.
    // 2) With merging, in the local space of the root pass of the child frame,
    // the second render pass (20,30 60x60) has a blur backdrop filter.
    // However, the damage passed from parent surface and transformed into the
    // same local space is (-5,-5 10x20), so this damage is not expanded. The
    // child surface doesn't have any damage.
    EXPECT_CALL(aggregated_damage_callback,
                OnAggregatedDamage(child_local_surface_id, _, _, _))
        .Times(0);

    auto aggregated_frame = AggregateFrame(root_surface_id);
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
  std::vector<Quad> child_rp1_quads = {
      Quad::SolidColorQuad(SK_ColorGREEN, gfx::Rect(5, 5))};
  std::vector<Quad> child_rp2_quads = {
      Quad::RenderPassQuad(CompositorRenderPassId{1}, gfx::Transform(),
                           /*intersects_damage_under=*/true)};
  std::vector<Quad> child_rp3_quads = {
      Quad::RenderPassQuad(CompositorRenderPassId{2}, gfx::Transform(),
                           /*intersects_damage_under=*/true)};
  std::vector<Pass> child_passes = {
      Pass(child_rp1_quads, CompositorRenderPassId{1}, child_surface_size),
      Pass(child_rp2_quads, CompositorRenderPassId{2}, child_surface_size),
      Pass(child_rp3_quads, CompositorRenderPassId{3}, child_surface_size)};

  CompositorFrame child_frame = MakeEmptyCompositorFrame();
  AddPasses(&child_frame.render_pass_list, child_passes,
            &child_frame.metadata.referenced_surfaces);

  child_frame.render_pass_list[2]
      ->shared_quad_state_list.ElementAt(0)
      ->quad_to_target_transform.Translate(20, 30);

  child_frame.render_pass_list[1]->backdrop_filters.Append(
      cc::FilterOperation::CreateBlurFilter(5));

  ParentLocalSurfaceIdAllocator child_allocator;
  child_allocator.GenerateId();
  LocalSurfaceId child_local_surface_id =
      child_allocator.GetCurrentLocalSurfaceId();
  SurfaceId child_surface_id(child_sink_->frame_sink_id(),
                             child_local_surface_id);
  child_sink_->SubmitCompositorFrame(child_local_surface_id,
                                     std::move(child_frame));

  // The frame of the root surface has one single render pass with a surface
  // quad containing the child surface.
  std::vector<Quad> render_pass_quads = {
      // The |allow_merge| flag of the surface quad would be set to true/false
      // according to the parameter of the test.
      Quad::SurfaceQuad(SurfaceRange(base::nullopt, child_surface_id),
                        SK_ColorWHITE, gfx::Rect(90, 90),
                        /*stretch_content_to_fill_bounds=*/true, AllowMerge())};

  std::vector<Pass> root_passes{
      Pass(render_pass_quads, CompositorRenderPassId{1}, SurfaceSize())};
  root_passes[0].damage_rect = gfx::Rect(0, 0, 10, 20);

  CompositorFrame root_frame = MakeEmptyCompositorFrame();
  AddPasses(&root_frame.render_pass_list, root_passes,
            &root_frame.metadata.referenced_surfaces);
  root_frame.render_pass_list.back()
      ->shared_quad_state_list.front()
      ->quad_to_target_transform.Translate(5, 5);
  root_sink_->SubmitCompositorFrame(root_local_surface_id_,
                                    std::move(root_frame));

  SurfaceId root_surface_id(root_sink_->frame_sink_id(),
                            root_local_surface_id_);
  size_t expected_num_passes_after_aggregation = AllowMerge() ? 3u : 4u;
  // First aggregation.
  {
    // In the local space of the root pass of the child frame, the second render
    // pass (20,30 60x60) has a blur backdrop filter. It expands the damage from
    // under it. 1) Without merging, the damage for the first aggregation of the
    // child surface is the entire child surface (0,0 60x60), expanded by the
    // backdrop filter, to be (0,0 80x90).
    // 2) With merging, the damage from parent surface is transformed into the
    // root pass local space to (-5,-5 100x100), and scaled by 2/3 to (-4,-4
    // 68x68). The backdrop filter expands this damage to (-4,-4, 84x94).
    gfx::Rect expected_child_damage_rect =
        AllowMerge() ? gfx::Rect(-4, -4, 84, 94) : gfx::Rect(0, 0, 80, 90);
    EXPECT_CALL(
        aggregated_damage_callback,
        OnAggregatedDamage(child_local_surface_id, child_surface_size,
                           expected_child_damage_rect, next_display_time()));

    // 1) Without merging, child surface damage (0,0 80x90) stretches to (0,0
    // 120x135), and transformed to root surface as (5,5 120x135), unions root
    // surface damage (0,0 100,100) to (0,0 125x140).
    // 2) With merging, child surface damage (-4,-4 84x94) stretches to (-6,-6
    // 126x141), and transformed to root surface as (-1,-1 126x141).
    gfx::Rect expected_root_damage_rect =
        AllowMerge() ? gfx::Rect(-1, -1, 126, 141) : gfx::Rect(0, 0, 125, 140);
    EXPECT_CALL(
        aggregated_damage_callback,
        OnAggregatedDamage(root_local_surface_id_, SurfaceSize(),
                           expected_root_damage_rect, next_display_time()));

    auto aggregated_frame = AggregateFrame(root_surface_id);
    testing::Mock::VerifyAndClearExpectations(&aggregated_damage_callback);
    const auto& aggregated_pass_list = aggregated_frame.render_pass_list;
    ASSERT_EQ(expected_num_passes_after_aggregation,
              aggregated_pass_list.size());

    EXPECT_EQ(gfx::Rect(SurfaceSize()),
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
    root_sink_->SubmitCompositorFrame(root_local_surface_id_,
                                      std::move(root_frame));
    EXPECT_CALL(
        aggregated_damage_callback,
        OnAggregatedDamage(root_local_surface_id_, SurfaceSize(),
                           gfx::Rect(0, 0, 10, 20), next_display_time()));
    // 1) Without merging, there is no damage on the child surface.
    // 2) With merging, in the local space of the root pass of the child frame,
    // the second render pass (20,30 60x60) has a blur backdrop filter.
    // However, the damage passed from parent surface and transformed into the
    // same local space is (-4,-4 8x14), so this damage is not expanded. The
    // child surface doesn't have any damage.
    EXPECT_CALL(aggregated_damage_callback,
                OnAggregatedDamage(child_local_surface_id, _, _, _))
        .Times(0);

    auto aggregated_frame = AggregateFrame(root_surface_id);
    const auto& aggregated_pass_list = aggregated_frame.render_pass_list;
    ASSERT_EQ(expected_num_passes_after_aggregation,
              aggregated_pass_list.size());
    EXPECT_EQ(gfx::Rect(0, 0, 10, 20),
              aggregated_pass_list.back()->damage_rect);
  }
}

class SurfaceAggregatorPartialSwapTest
    : public SurfaceAggregatorValidSurfaceTest {
 public:
  SurfaceAggregatorPartialSwapTest()
      : SurfaceAggregatorValidSurfaceTest(true) {}
};

// Tests that quads outside the damage rect are ignored.
TEST_F(SurfaceAggregatorPartialSwapTest, IgnoreOutside) {
  ParentLocalSurfaceIdAllocator allocator;
  allocator.GenerateId();
  LocalSurfaceId child_local_surface_id = allocator.GetCurrentLocalSurfaceId();
  SurfaceId child_surface_id(child_sink_->frame_sink_id(),
                             child_local_surface_id);
  constexpr float device_scale_factor = 1.0f;

  // The child surface has three quads, one with a visible rect of 13,13 4x4 and
  // the other other with a visible rect of 10,10 2x2 (relative to root target
  // space), and one with a non-invertible transform.
  {
    CompositorRenderPassId child_pass_ids[] = {CompositorRenderPassId{1},
                                               CompositorRenderPassId{2},
                                               CompositorRenderPassId{3}};
    std::vector<Quad> child_quads1 = {
        Quad::SolidColorQuad(SK_ColorGREEN, gfx::Rect(5, 5))};
    std::vector<Quad> child_quads2 = {
        Quad::SolidColorQuad(SK_ColorGREEN, gfx::Rect(5, 5))};
    std::vector<Quad> child_quads3 = {
        Quad::SolidColorQuad(SK_ColorGREEN, gfx::Rect(5, 5))};
    std::vector<Pass> child_passes = {
        Pass(child_quads1, child_pass_ids[0], SurfaceSize()),
        Pass(child_quads2, child_pass_ids[1], SurfaceSize()),
        Pass(child_quads3, child_pass_ids[2], SurfaceSize())};

    CompositorRenderPassList child_pass_list;
    std::vector<SurfaceRange> referenced_surfaces;
    AddPasses(&child_pass_list, child_passes, &referenced_surfaces);

    child_pass_list[0]->quad_list.ElementAt(0)->visible_rect =
        gfx::Rect(1, 1, 2, 2);
    auto* child_sqs = child_pass_list[0]->shared_quad_state_list.ElementAt(0u);
    child_sqs->quad_to_target_transform.Translate(1, 1);
    child_sqs->quad_to_target_transform.Scale(2, 2);

    child_pass_list[1]->quad_list.ElementAt(0)->visible_rect =
        gfx::Rect(0, 0, 2, 2);

    auto* child_noninvertible_sqs =
        child_pass_list[2]->shared_quad_state_list.ElementAt(0u);
    child_noninvertible_sqs->quad_to_target_transform.matrix().setDouble(0, 0,
                                                                         0.0);
    EXPECT_FALSE(
        child_noninvertible_sqs->quad_to_target_transform.IsInvertible());
    child_pass_list[2]->quad_list.ElementAt(0)->visible_rect =
        gfx::Rect(0, 0, 2, 2);

    SubmitPassListAsFrame(child_sink_.get(), child_local_surface_id,
                          &child_pass_list, std::move(referenced_surfaces),
                          device_scale_factor);
  }

  {
    std::vector<Quad> root_quads = {Quad::SurfaceQuad(
        SurfaceRange(base::nullopt, child_surface_id), SK_ColorWHITE,
        gfx::Rect(5, 5), /*stretch_content_to_fill_bounds=*/false)};

    std::vector<Pass> root_passes = {Pass(root_quads, SurfaceSize())};

    CompositorRenderPassList root_pass_list;
    std::vector<SurfaceRange> referenced_surfaces;
    AddPasses(&root_pass_list, root_passes, &referenced_surfaces);

    auto* root_pass = root_pass_list[0].get();
    root_pass->shared_quad_state_list.front()
        ->quad_to_target_transform.Translate(10, 10);
    root_pass->damage_rect = gfx::Rect(0, 0, 1, 1);

    SubmitPassListAsFrame(root_sink_.get(), root_local_surface_id_,
                          &root_pass_list, std::move(referenced_surfaces),
                          device_scale_factor);
  }

  SurfaceId root_surface_id(root_sink_->frame_sink_id(),
                            root_local_surface_id_);
  auto aggregated_frame = AggregateFrame(root_surface_id);

  const auto& aggregated_pass_list = aggregated_frame.render_pass_list;

  ASSERT_EQ(3u, aggregated_pass_list.size());

  // Damage rect for first aggregation should contain entire root surface.
  EXPECT_EQ(gfx::Rect(SurfaceSize()), aggregated_pass_list[2]->damage_rect);
  EXPECT_EQ(1u, aggregated_pass_list[0]->quad_list.size());
  EXPECT_EQ(1u, aggregated_pass_list[1]->quad_list.size());
  EXPECT_EQ(1u, aggregated_pass_list[2]->quad_list.size());

  // Create a root surface with a smaller damage rect.
  {
    std::vector<Quad> root_quads = {Quad::SurfaceQuad(
        SurfaceRange(base::nullopt, child_surface_id), SK_ColorWHITE,
        gfx::Rect(5, 5), /*stretch_content_to_fill_bounds=*/false)};

    std::vector<Pass> root_passes = {Pass(root_quads, SurfaceSize())};

    CompositorRenderPassList root_pass_list;
    std::vector<SurfaceRange> referenced_surfaces;
    AddPasses(&root_pass_list, root_passes, &referenced_surfaces);

    auto* root_pass = root_pass_list[0].get();
    root_pass->shared_quad_state_list.front()
        ->quad_to_target_transform.Translate(10, 10);
    root_pass->damage_rect = gfx::Rect(10, 10, 2, 2);
    SubmitPassListAsFrame(root_sink_.get(), root_local_surface_id_,
                          &root_pass_list, std::move(referenced_surfaces),
                          device_scale_factor);
  }

  {
    auto aggregated_frame = AggregateFrame(root_surface_id);

    const auto& aggregated_pass_list = aggregated_frame.render_pass_list;

    ASSERT_EQ(3u, aggregated_pass_list.size());

    // Only first quad from surface is inside damage rect and should be
    // included.
    EXPECT_EQ(gfx::Rect(10, 10, 2, 2), aggregated_pass_list[2]->damage_rect);
    EXPECT_EQ(0u, aggregated_pass_list[0]->quad_list.size());
    EXPECT_EQ(1u, aggregated_pass_list[1]->quad_list.size());
    EXPECT_EQ(gfx::Rect(0, 0, 2, 2),
              aggregated_pass_list[1]->quad_list.back()->visible_rect);
    EXPECT_EQ(1u, aggregated_pass_list[2]->quad_list.size());
  }

  // New child frame has same content and no damage, but has a
  // CopyOutputRequest.
  {
    CompositorRenderPassId child_pass_ids[] = {CompositorRenderPassId{1},
                                               CompositorRenderPassId{2}};
    std::vector<Quad> child_quads1 = {Quad::SolidColorQuad(1, gfx::Rect(5, 5))};
    std::vector<Quad> child_quads2 = {
        Quad::RenderPassQuad(child_pass_ids[0], gfx::Transform(), true)};
    std::vector<Pass> child_passes = {
        Pass(child_quads1, child_pass_ids[0], SurfaceSize()),
        Pass(child_quads2, child_pass_ids[1], SurfaceSize())};

    CompositorRenderPassList child_pass_list;
    std::vector<SurfaceRange> referenced_surfaces;
    AddPasses(&child_pass_list, child_passes, &referenced_surfaces);

    child_pass_list[0]->quad_list.ElementAt(0)->visible_rect =
        gfx::Rect(1, 1, 2, 2);
    auto* child_sqs = child_pass_list[0]->shared_quad_state_list.ElementAt(0u);
    child_sqs->quad_to_target_transform.Translate(1, 1);
    child_sqs->quad_to_target_transform.Scale(2, 2);

    child_pass_list[1]->quad_list.ElementAt(0)->visible_rect =
        gfx::Rect(0, 0, 2, 2);

    auto* child_root_pass = child_pass_list[1].get();

    child_root_pass->copy_requests.push_back(
        CopyOutputRequest::CreateStubForTesting());
    child_root_pass->damage_rect = gfx::Rect();
    SubmitPassListAsFrame(child_sink_.get(), child_local_surface_id,
                          &child_pass_list, std::move(referenced_surfaces),
                          device_scale_factor);
  }

  {
    auto aggregated_frame = AggregateFrame(root_surface_id);

    const auto& aggregated_pass_list = aggregated_frame.render_pass_list;

    // Output frame should have no damage, but all quads included.
    ASSERT_EQ(3u, aggregated_pass_list.size());

    EXPECT_EQ(gfx::Rect(SurfaceSize()), aggregated_pass_list[0]->damage_rect);
    EXPECT_EQ(gfx::Rect(SurfaceSize()), aggregated_pass_list[1]->damage_rect);
    EXPECT_TRUE(aggregated_pass_list[2]->damage_rect.IsEmpty());
    ASSERT_EQ(1u, aggregated_pass_list[0]->quad_list.size());
    ASSERT_EQ(1u, aggregated_pass_list[1]->quad_list.size());
    EXPECT_EQ(gfx::Rect(1, 1, 2, 2),
              aggregated_pass_list[0]->quad_list.ElementAt(0)->visible_rect);
    EXPECT_EQ(gfx::Rect(0, 0, 2, 2),
              aggregated_pass_list[1]->quad_list.ElementAt(0)->visible_rect);
    ASSERT_EQ(1u, aggregated_pass_list[2]->quad_list.size());
  }

  {
    auto aggregated_frame = AggregateFrame(root_surface_id);

    const auto& aggregated_pass_list = aggregated_frame.render_pass_list;
    // There were no changes since last aggregation, so output should be empty
    // and have no damage.
    ASSERT_EQ(1u, aggregated_pass_list.size());
    EXPECT_TRUE(aggregated_pass_list[0]->damage_rect.IsEmpty());
    ASSERT_EQ(0u, aggregated_pass_list[0]->quad_list.size());
  }

  // Render passes with pixel-moving foreground filters will increase the damage
  // only if the damage of the contents will overlap the expanded render pass
  // draw quad. Since the root surface damage does not overlap, the render pass
  // and its descendant passes should not be aggregated.
  {
    CompositorRenderPassId root_pass_ids[] = {CompositorRenderPassId{1},
                                              CompositorRenderPassId{2},
                                              CompositorRenderPassId{3}};
    std::vector<Quad> root_quads1 = {Quad::SurfaceQuad(
        SurfaceRange(base::nullopt, child_surface_id), SK_ColorWHITE,
        gfx::Rect(5, 5), /*stretch_content_to_fill_bounds=*/false)};
    std::vector<Quad> root_quads2 = {
        Quad::RenderPassQuad(root_pass_ids[0], gfx::Transform(), true)};
    std::vector<Quad> root_quads3 = {
        Quad::RenderPassQuad(root_pass_ids[1], gfx::Transform(), true)};
    std::vector<Pass> root_passes = {
        Pass(root_quads1, root_pass_ids[0], SurfaceSize()),
        Pass(root_quads2, root_pass_ids[1], SurfaceSize()),
        Pass(root_quads3, root_pass_ids[2], SurfaceSize())};

    CompositorRenderPassList root_pass_list;
    std::vector<SurfaceRange> referenced_surfaces;
    AddPasses(&root_pass_list, root_passes, &referenced_surfaces);

    auto* filter_pass = root_pass_list[1].get();
    filter_pass->shared_quad_state_list.front()
        ->quad_to_target_transform.Translate(10, 10);
    // Create 3 pixel-moving filters with the same max pixel movement.
    filter_pass->filters.Append(cc::FilterOperation::CreateBlurFilter(2));
    filter_pass->filters.Append(
        cc::FilterOperation::CreateDropShadowFilter(gfx::Point(0, 0), 2, 0));
    filter_pass->filters.Append(cc::FilterOperation::CreateZoomFilter(2, 4));
    auto* root_pass = root_pass_list[2].get();
    // Set the root damage rect which doesn't intersect with the expanded
    // filter_pass quad (-4, -4, 13, 13) (filter quad (0, 0, 5, 5) +
    // MaximumPixelMovement(2 * 3 = 6)), so we don't have to add more damage
    // from the filter_pass and the first render pass draw quad will not be
    // drawn.
    root_pass->damage_rect = gfx::Rect(20, 20, 2, 2);
    SubmitPassListAsFrame(root_sink_.get(), root_local_surface_id_,
                          &root_pass_list, std::move(referenced_surfaces),
                          device_scale_factor);

    auto aggregated_frame = AggregateFrame(root_surface_id);

    const auto& aggregated_pass_list = aggregated_frame.render_pass_list;

    ASSERT_EQ(4u, aggregated_pass_list.size());

    EXPECT_EQ(gfx::Rect(SurfaceSize()), aggregated_pass_list[0]->damage_rect);
    EXPECT_EQ(gfx::Rect(SurfaceSize()), aggregated_pass_list[1]->damage_rect);
    EXPECT_EQ(gfx::Rect(SurfaceSize()), aggregated_pass_list[2]->damage_rect);
    // The filter pass does not intersects with the other damages. The root
    // damage should not increase.
    EXPECT_EQ(gfx::Rect(20, 20, 2, 2), aggregated_pass_list[3]->damage_rect);
    EXPECT_EQ(1u, aggregated_pass_list[0]->quad_list.size());
    EXPECT_EQ(1u, aggregated_pass_list[1]->quad_list.size());
    EXPECT_EQ(1u, aggregated_pass_list[2]->quad_list.size());
    // First render pass draw quad with filterw is outside damage rect, so
    // shouldn't be drawn.
    EXPECT_EQ(0u, aggregated_pass_list[3]->quad_list.size());
  }

  // Render passes with pixel-moving foreground filters will increase the damage
  // if the damage of the contents will overlap the expanded render pass draw
  // quad (quad rect + maximum pixel movement). Since the root surface damage
  // overlaps, the render pass and its descendant passes should be aggregated.
  {
    CompositorRenderPassId root_pass_ids[] = {CompositorRenderPassId{1},
                                              CompositorRenderPassId{2},
                                              CompositorRenderPassId{3}};
    std::vector<Quad> root_quads1 = {Quad::SurfaceQuad(
        SurfaceRange(base::nullopt, child_surface_id), SK_ColorWHITE,
        gfx::Rect(5, 5), /*stretch_content_to_fill_bounds=*/false)};
    std::vector<Quad> root_quads2 = {
        Quad::RenderPassQuad(root_pass_ids[0], gfx::Transform(), true)};
    std::vector<Quad> root_quads3 = {
        Quad::RenderPassQuad(root_pass_ids[1], gfx::Transform(), true)};
    std::vector<Pass> root_passes = {
        Pass(root_quads1, root_pass_ids[0], SurfaceSize()),
        Pass(root_quads2, root_pass_ids[1], SurfaceSize()),
        Pass(root_quads3, root_pass_ids[2], SurfaceSize())};

    CompositorRenderPassList root_pass_list;
    std::vector<SurfaceRange> referenced_surfaces;
    AddPasses(&root_pass_list, root_passes, &referenced_surfaces);

    auto* filter_pass = root_pass_list[1].get();
    filter_pass->shared_quad_state_list.front()
        ->quad_to_target_transform.Translate(10, 10);
    // Create 3 pixel-moving filters with the same max pixel movement.
    filter_pass->filters.Append(cc::FilterOperation::CreateBlurFilter(10));
    filter_pass->filters.Append(
        cc::FilterOperation::CreateDropShadowFilter(gfx::Point(0, 0), 10, 0));
    filter_pass->filters.Append(cc::FilterOperation::CreateZoomFilter(2, 20));
    auto* root_pass = root_pass_list[2].get();
    // Make the root damage rect intersect with the expanded filter_pass
    // quad (filter quad (0, 0, 5, 5) + MaximumPixelMovement(10 * 3) = (-30,
    // -30, 65, 65)), but not with filter_pass quad itself (0, 0, 5, 5). The
    // first render pass will be drawn.
    root_pass->damage_rect = gfx::Rect(20, 20, 2, 2);
    SubmitPassListAsFrame(root_sink_.get(), root_local_surface_id_,
                          &root_pass_list, std::move(referenced_surfaces),
                          device_scale_factor);

    auto aggregated_frame = AggregateFrame(root_surface_id);

    const auto& aggregated_pass_list = aggregated_frame.render_pass_list;

    ASSERT_EQ(4u, aggregated_pass_list.size());

    EXPECT_EQ(gfx::Rect(SurfaceSize()), aggregated_pass_list[0]->damage_rect);
    EXPECT_EQ(gfx::Rect(SurfaceSize()), aggregated_pass_list[1]->damage_rect);
    EXPECT_EQ(gfx::Rect(SurfaceSize()), aggregated_pass_list[2]->damage_rect);
    // The filter pass intersects with the root surface damage, the root damage
    // should increase.
    // damage_rect = original root damage (0, 0, 5, 5) + MaximumPixelMovement(10
    // * 3) = (-30, -30, 65, 65). Then intersects with the root output_rect (0,
    // 0, 100, 100) = (0, 0, 35, 35).
    EXPECT_EQ(gfx::Rect(0, 0, 35, 35), aggregated_pass_list[3]->damage_rect);
    EXPECT_EQ(1u, aggregated_pass_list[0]->quad_list.size());
    EXPECT_EQ(1u, aggregated_pass_list[1]->quad_list.size());
    EXPECT_EQ(1u, aggregated_pass_list[2]->quad_list.size());
    // First render pass draw quad is damaged. It should be drawn.
    EXPECT_EQ(1u, aggregated_pass_list[3]->quad_list.size());
  }

  // Root surface has smaller damage rect. Opacity filter on render pass
  // means Surface quad under it should be aggregated.
  {
    CompositorRenderPassId root_pass_ids[] = {CompositorRenderPassId{1},
                                              CompositorRenderPassId{2}};
    std::vector<Quad> root_quads1 = {
        Quad::SolidColorQuad(1, gfx::Rect(5, 5)),
    };
    std::vector<Quad> root_quads2 = {
        Quad::RenderPassQuad(root_pass_ids[0], gfx::Transform(), true),
        Quad::SurfaceQuad(SurfaceRange(base::nullopt, child_surface_id),
                          SK_ColorWHITE, gfx::Rect(5, 5),
                          /*stretch_content_to_fill_bounds=*/false)};
    std::vector<Pass> root_passes = {
        Pass(root_quads1, root_pass_ids[0], SurfaceSize()),
        Pass(root_quads2, root_pass_ids[1], SurfaceSize())};

    CompositorRenderPassList root_pass_list;
    std::vector<SurfaceRange> referenced_surfaces;
    AddPasses(&root_pass_list, root_passes, &referenced_surfaces);

    auto* pass = root_pass_list[0].get();
    auto* root_pass = root_pass_list[1].get();
    root_pass->shared_quad_state_list.ElementAt(1)
        ->quad_to_target_transform.Translate(10, 10);
    pass->backdrop_filters.Append(
        cc::FilterOperation::CreateOpacityFilter(0.5f));
    root_pass->damage_rect = gfx::Rect(10, 10, 2, 2);
    SubmitPassListAsFrame(root_sink_.get(), root_local_surface_id_,
                          &root_pass_list, std::move(referenced_surfaces),
                          device_scale_factor);
  }

  {
    auto aggregated_frame = AggregateFrame(root_surface_id);
    const auto& aggregated_pass_list = aggregated_frame.render_pass_list;

    ASSERT_EQ(3u, aggregated_pass_list.size());

    // Pass 0 is solid color quad from root, but outside damage rect.
    EXPECT_EQ(gfx::Rect(10, 10, 2, 2), aggregated_pass_list[0]->damage_rect);
    EXPECT_EQ(0u, aggregated_pass_list[0]->quad_list.size());
    EXPECT_EQ(gfx::Rect(0, 0, 2, 2), aggregated_pass_list[1]->damage_rect);
    EXPECT_EQ(0u, aggregated_pass_list[1]->quad_list.size());

    // First render pass draw quad is outside damage rect, so shouldn't be
    // drawn. SurfaceDrawQuad is after opacity filter, so corresponding
    // CompositorRenderPassDrawQuad should be drawn.
    EXPECT_EQ(gfx::Rect(10, 10, 2, 2), aggregated_pass_list[2]->damage_rect);
    EXPECT_EQ(1u, aggregated_pass_list[2]->quad_list.size());
  }

  // Render passes with pixel-moving backdrop filters will increase the damage
  // only if the damage of the contents will overlap the render pass. Since one
  // of the render passes has a pixel-moving backdrop filter no quads are
  // ignored.
  {
    CompositorRenderPassId child_pass_ids[] = {CompositorRenderPassId{1},
                                               CompositorRenderPassId{2}};
    std::vector<Quad> child_quads1 = {Quad::SolidColorQuad(1, gfx::Rect(5, 5))};
    std::vector<Quad> child_quads2 = {
        Quad::RenderPassQuad(child_pass_ids[0], gfx::Transform(), true)};
    std::vector<Pass> child_passes = {
        Pass(child_quads1, child_pass_ids[0], SurfaceSize()),
        Pass(child_quads2, child_pass_ids[1], SurfaceSize())};

    CompositorRenderPassList child_pass_list;
    std::vector<SurfaceRange> child_referenced_surfaces;
    AddPasses(&child_pass_list, child_passes, &child_referenced_surfaces);

    child_pass_list[0]->quad_list.ElementAt(0)->visible_rect =
        gfx::Rect(1, 1, 2, 2);
    auto* child_sqs = child_pass_list[0]->shared_quad_state_list.ElementAt(0u);
    child_sqs->quad_to_target_transform.Translate(1, 1);
    child_sqs->quad_to_target_transform.Scale(2, 2);

    child_pass_list[1]->quad_list.ElementAt(0)->visible_rect =
        gfx::Rect(0, 0, 2, 2);

    child_pass_list[1]->damage_rect = gfx::Rect(-2, -2, 3, 3);
    SubmitPassListAsFrame(
        child_sink_.get(), child_local_surface_id, &child_pass_list,
        std::move(child_referenced_surfaces), device_scale_factor);

    CompositorRenderPassId root_pass_ids[] = {CompositorRenderPassId{1},
                                              CompositorRenderPassId{2}};
    const gfx::Size pass_with_filter_size(5, 5);
    std::vector<Quad> root_quads1 = {
        Quad::SolidColorQuad(1, gfx::Rect(pass_with_filter_size)),
    };
    std::vector<Quad> root_quads2 = {
        Quad::RenderPassQuad(root_pass_ids[0], gfx::Transform(), true),
        Quad::SurfaceQuad(SurfaceRange(base::nullopt, child_surface_id),
                          SK_ColorWHITE, gfx::Rect(5, 5),
                          /*stretch_content_to_fill_bounds=*/false)};
    std::vector<Pass> root_passes = {
        Pass(root_quads1, root_pass_ids[0], pass_with_filter_size),
        Pass(root_quads2, root_pass_ids[1], SurfaceSize())};

    CompositorRenderPassList root_pass_list;
    std::vector<SurfaceRange> referenced_surfaces;
    AddPasses(&root_pass_list, root_passes, &referenced_surfaces);

    auto* pass_with_filter = root_pass_list[0].get();
    auto* root_pass = root_pass_list[1].get();
    root_pass->shared_quad_state_list.ElementAt(1)
        ->quad_to_target_transform.Translate(5, 5);
    pass_with_filter->backdrop_filters.Append(
        cc::FilterOperation::CreateBlurFilter(2));
    root_pass->damage_rect = gfx::Rect();
    SubmitPassListAsFrame(root_sink_.get(), root_local_surface_id_,
                          &root_pass_list, std::move(referenced_surfaces),
                          device_scale_factor);
  }

  {
    auto aggregated_frame = AggregateFrame(root_surface_id);

    const auto& aggregated_pass_list = aggregated_frame.render_pass_list;

    ASSERT_EQ(3u, aggregated_pass_list.size());

    // Pass 0 has background blur filter and overlaps with damage rect,
    // therefore the whole render pass should be damaged.
    EXPECT_EQ(gfx::Rect(0, 0, 5, 5), aggregated_pass_list[0]->damage_rect);
    EXPECT_EQ(1u, aggregated_pass_list[0]->quad_list.size());

    EXPECT_EQ(gfx::Rect(1, 1), aggregated_pass_list[1]->damage_rect);
    EXPECT_EQ(1u, aggregated_pass_list[1]->quad_list.size());

    // First render pass draw quad overlaps with damage rect and has background
    // filter, so it should be damaged. SurfaceDrawQuad is after background
    // filter, so corresponding CompositorRenderPassDrawQuad should be drawn.
    EXPECT_EQ(gfx::Rect(0, 0, 6, 6), aggregated_pass_list[2]->damage_rect);
    EXPECT_EQ(2u, aggregated_pass_list[2]->quad_list.size());
  }

  // If the render pass with background filters does not intersect the damage
  // rect, the damage won't be expanded to cover the render pass. Since one of
  // the render passes has a pixel-moving backdrop filter no quads are ignored.
  {
    CompositorRenderPassId child_pass_ids[] = {CompositorRenderPassId{1},
                                               CompositorRenderPassId{2}};
    std::vector<Quad> child_quads1 = {Quad::SolidColorQuad(1, gfx::Rect(5, 5))};
    std::vector<Quad> child_quads2 = {
        Quad::RenderPassQuad(child_pass_ids[0], gfx::Transform(), true)};
    std::vector<Pass> child_passes = {
        Pass(child_quads1, child_pass_ids[0], SurfaceSize()),
        Pass(child_quads2, child_pass_ids[1], SurfaceSize())};

    CompositorRenderPassList child_pass_list;
    std::vector<SurfaceRange> child_referenced_surfaces;
    AddPasses(&child_pass_list, child_passes, &child_referenced_surfaces);

    child_pass_list[0]->quad_list.ElementAt(0)->visible_rect =
        gfx::Rect(1, 1, 2, 2);
    auto* child_sqs = child_pass_list[0]->shared_quad_state_list.ElementAt(0u);
    child_sqs->quad_to_target_transform.Translate(1, 1);
    child_sqs->quad_to_target_transform.Scale(2, 2);

    child_pass_list[1]->quad_list.ElementAt(0)->visible_rect =
        gfx::Rect(0, 0, 2, 2);

    child_pass_list[1]->damage_rect = gfx::Rect(1, 1, 3, 3);
    SubmitPassListAsFrame(
        child_sink_.get(), child_local_surface_id, &child_pass_list,
        std::move(child_referenced_surfaces), device_scale_factor);

    CompositorRenderPassId root_pass_ids[] = {CompositorRenderPassId{1},
                                              CompositorRenderPassId{2}};
    const gfx::Size pass_with_filter_size(5, 5);
    std::vector<Quad> root_quads1 = {
        Quad::SolidColorQuad(1, gfx::Rect(pass_with_filter_size)),
    };
    std::vector<Quad> root_quads2 = {
        Quad::RenderPassQuad(root_pass_ids[0], gfx::Transform(), true),
        Quad::SurfaceQuad(SurfaceRange(base::nullopt, child_surface_id),
                          SK_ColorWHITE, gfx::Rect(5, 5),
                          /*stretch_content_to_fill_bounds=*/false)};
    std::vector<Pass> root_passes = {
        Pass(root_quads1, root_pass_ids[0], pass_with_filter_size),
        Pass(root_quads2, root_pass_ids[1], SurfaceSize())};

    CompositorRenderPassList root_pass_list;
    std::vector<SurfaceRange> referenced_surfaces;
    AddPasses(&root_pass_list, root_passes, &referenced_surfaces);

    auto* pass_with_filter = root_pass_list[0].get();
    auto* root_pass = root_pass_list[1].get();
    root_pass->shared_quad_state_list.ElementAt(1)
        ->quad_to_target_transform.Translate(5, 5);
    pass_with_filter->backdrop_filters.Append(
        cc::FilterOperation::CreateBlurFilter(2));
    root_pass->damage_rect = gfx::Rect();
    SubmitPassListAsFrame(root_sink_.get(), root_local_surface_id_,
                          &root_pass_list, std::move(referenced_surfaces),
                          device_scale_factor);
  }

  {
    auto aggregated_frame = AggregateFrame(root_surface_id);

    const auto& aggregated_pass_list = aggregated_frame.render_pass_list;

    ASSERT_EQ(3u, aggregated_pass_list.size());

    // Pass 0 has background blur filter but does NOT overlap with damage rect.
    EXPECT_EQ(gfx::Rect(), aggregated_pass_list[0]->damage_rect);
    EXPECT_EQ(1u, aggregated_pass_list[0]->quad_list.size());

    EXPECT_EQ(gfx::Rect(1, 1, 3, 3), aggregated_pass_list[1]->damage_rect);
    EXPECT_EQ(1u, aggregated_pass_list[1]->quad_list.size());

    // First render pass draw quad is outside damage rect, so shouldn't be
    // drawn. SurfaceDrawQuad is after background filter, so corresponding
    // CompositorRenderPassDrawQuad should be drawn.
    EXPECT_EQ(gfx::Rect(6, 6, 3, 3), aggregated_pass_list[2]->damage_rect);
    EXPECT_EQ(2u, aggregated_pass_list[2]->quad_list.size());
  }
}

TEST_F(SurfaceAggregatorPartialSwapTest, ExpandByTargetDamage) {
  ParentLocalSurfaceIdAllocator allocator;
  allocator.GenerateId();
  LocalSurfaceId child_local_surface_id = allocator.GetCurrentLocalSurfaceId();
  SurfaceId child_surface_id(child_sink_->frame_sink_id(),
                             child_local_surface_id);
  constexpr float device_scale_factor = 1.0f;

  // The child surface has one quad.
  {
    CompositorRenderPassId child_pass_id{1};
    std::vector<Quad> child_quads1 = {
        Quad::SolidColorQuad(SK_ColorGREEN, gfx::Rect(5, 5))};
    std::vector<Pass> child_passes = {
        Pass(child_quads1, child_pass_id, gfx::Rect(5, 5))};

    CompositorRenderPassList child_pass_list;
    std::vector<SurfaceRange> referenced_surfaces;
    AddPasses(&child_pass_list, child_passes, &referenced_surfaces);

    SubmitPassListAsFrame(child_sink_.get(), child_local_surface_id,
                          &child_pass_list, std::move(referenced_surfaces),
                          device_scale_factor);
  }

  {
    std::vector<Quad> root_quads = {Quad::SurfaceQuad(
        SurfaceRange(base::nullopt, child_surface_id), SK_ColorWHITE,
        gfx::Rect(SurfaceSize()), /*stretch_content_to_fill_bounds=*/false)};

    std::vector<Pass> root_passes = {Pass(root_quads, SurfaceSize())};

    CompositorRenderPassList root_pass_list;
    std::vector<SurfaceRange> referenced_surfaces;
    AddPasses(&root_pass_list, root_passes, &referenced_surfaces);
    // No damage, this is the first frame submitted, so all quads should be
    // produced.
    SubmitPassListAsFrame(root_sink_.get(), root_local_surface_id_,
                          &root_pass_list, std::move(referenced_surfaces),
                          device_scale_factor);
  }

  SurfaceId root_surface_id(root_sink_->frame_sink_id(),
                            root_local_surface_id_);
  auto aggregated_frame = AggregateFrame(root_surface_id);

  const auto& aggregated_pass_list = aggregated_frame.render_pass_list;

  ASSERT_EQ(1u, aggregated_pass_list.size());

  // Damage rect for first aggregation should contain entire root surface.
  EXPECT_EQ(gfx::Rect(SurfaceSize()), aggregated_pass_list.back()->damage_rect);
  EXPECT_EQ(1u, aggregated_pass_list[0]->quad_list.size());

  // Create a root surface with a smaller damage rect.
  // This time the damage should be smaller.
  {
    std::vector<Quad> root_quads = {Quad::SurfaceQuad(
        SurfaceRange(base::nullopt, child_surface_id), SK_ColorWHITE,
        gfx::Rect(SurfaceSize()), /*stretch_content_to_fill_bounds=*/false)};

    std::vector<Pass> root_passes = {Pass(root_quads, SurfaceSize())};

    CompositorRenderPassList root_pass_list;
    std::vector<SurfaceRange> referenced_surfaces;
    AddPasses(&root_pass_list, root_passes, &referenced_surfaces);

    auto* root_pass = root_pass_list[0].get();
    root_pass->damage_rect = gfx::Rect(10, 10, 2, 2);
    SubmitPassListAsFrame(root_sink_.get(), root_local_surface_id_,
                          &root_pass_list, std::move(referenced_surfaces),
                          device_scale_factor);
  }

  {
    auto aggregated_frame = AggregateFrame(root_surface_id);

    const auto& aggregated_pass_list = aggregated_frame.render_pass_list;

    ASSERT_EQ(1u, aggregated_pass_list.size());

    // No quads inside the damage
    EXPECT_EQ(gfx::Rect(10, 10, 2, 2),
              aggregated_pass_list.back()->damage_rect);
    EXPECT_EQ(0u, aggregated_pass_list.back()->quad_list.size());
  }

  // This pass has damage that does not intersect the quad in the child
  // surface.
  {
    std::vector<Quad> root_quads = {
        Quad::SurfaceQuad(SurfaceRange(base::nullopt, child_surface_id),
                          SK_ColorWHITE, gfx::Rect(SurfaceSize()), false)};

    std::vector<Pass> root_passes = {Pass(root_quads, SurfaceSize())};

    CompositorRenderPassList root_pass_list;
    std::vector<SurfaceRange> referenced_surfaces;
    AddPasses(&root_pass_list, root_passes, &referenced_surfaces);

    auto* root_pass = root_pass_list[0].get();
    root_pass->damage_rect = gfx::Rect(10, 10, 2, 2);
    SubmitPassListAsFrame(root_sink_.get(), root_local_surface_id_,
                          &root_pass_list, std::move(referenced_surfaces),
                          device_scale_factor);
  }

  // The target surface invalidates one pixel in the top left, the quad in the
  // child surface should be added even if it's not causing damage nor in the
  // root render pass damage.
  {
    gfx::Rect target_damage(0, 0, 1, 1);
    auto aggregated_frame = AggregateFrame(root_surface_id, target_damage);

    const auto& aggregated_pass_list = aggregated_frame.render_pass_list;
    ASSERT_EQ(1u, aggregated_pass_list.size());

    // The damage rect of the root render pass should not be changed.
    EXPECT_EQ(gfx::Rect(10, 10, 2, 2),
              aggregated_pass_list.back()->damage_rect);
    // We expect one quad
    ASSERT_EQ(1u, aggregated_pass_list[0]->quad_list.size());
  }
}

class SurfaceAggregatorWithResourcesTest : public testing::Test,
                                           public DisplayTimeSource {
 public:
  SurfaceAggregatorWithResourcesTest() : manager_(&shared_bitmap_manager_) {}

  void SetUp() override {
    resource_provider_ = std::make_unique<DisplayResourceProviderSoftware>(
        &shared_bitmap_manager_);

    aggregator_ = std::make_unique<SurfaceAggregator>(
        manager_.surface_manager(), resource_provider_.get(), false, false);
    aggregator_->set_output_is_secure(true);
  }

  AggregatedFrame AggregateFrame(const SurfaceId& surface_id) {
    return aggregator_->Aggregate(surface_id, GetNextDisplayTimeAndIncrement(),
                                  gfx::OVERLAY_TRANSFORM_NONE);
  }

 protected:
  ServerSharedBitmapManager shared_bitmap_manager_;
  FrameSinkManagerImpl manager_;
  std::unique_ptr<DisplayResourceProvider> resource_provider_;
  std::unique_ptr<SurfaceAggregator> aggregator_;
};

void SubmitCompositorFrameWithResources(
    const std::vector<ResourceId>& resource_ids,
    bool valid,
    SurfaceId child_id,
    CompositorFrameSinkSupport* support,
    SurfaceId surface_id) {
  CompositorFrame frame = MakeEmptyCompositorFrame();
  auto pass = CompositorRenderPass::Create();
  pass->SetNew(CompositorRenderPassId{1}, gfx::Rect(0, 0, 20, 20), gfx::Rect(),
               gfx::Transform());
  auto* sqs = pass->CreateAndAppendSharedQuadState();
  sqs->opacity = 1.f;
  if (child_id.is_valid()) {
    auto* surface_quad = pass->CreateAndAppendDrawQuad<SurfaceDrawQuad>();
    surface_quad->SetNew(sqs, gfx::Rect(0, 0, 1, 1), gfx::Rect(0, 0, 1, 1),
                         SurfaceRange(base::nullopt, child_id), SK_ColorWHITE,
                         /*stretch_content_to_fill_bounds=*/false);
  }

  for (size_t i = 0u; i < resource_ids.size(); ++i) {
    auto resource = TransferableResource::MakeSoftware(
        SharedBitmap::GenerateId(), gfx::Size(1, 1), RGBA_8888);
    resource.id = resource_ids[i];
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
    SkColor background_color = SK_ColorGREEN;
    const float vertex_opacity[4] = {0.f, 0.f, 1.f, 1.f};
    bool flipped = false;
    bool nearest_neighbor = false;
    bool secure_output_only = true;
    gfx::ProtectedVideoType protected_video_type =
        gfx::ProtectedVideoType::kClear;
    quad->SetAll(sqs, rect, visible_rect, needs_blending, resource_ids[i],
                 gfx::Size(), premultiplied_alpha, uv_top_left, uv_bottom_right,
                 background_color, vertex_opacity, flipped, nearest_neighbor,
                 secure_output_only, protected_video_type);
  }
  frame.render_pass_list.push_back(std::move(pass));
  support->SubmitCompositorFrame(surface_id.local_surface_id(),
                                 std::move(frame));
}

TEST_F(SurfaceAggregatorWithResourcesTest, TakeResourcesOneSurface) {
  FakeCompositorFrameSinkClient client;
  auto support = std::make_unique<CompositorFrameSinkSupport>(
      &client, &manager_, kArbitraryRootFrameSinkId, kRootIsRoot);
  LocalSurfaceId local_surface_id(7u, base::UnguessableToken::Create());
  SurfaceId surface_id(support->frame_sink_id(), local_surface_id);

  std::vector<ResourceId> ids = {ResourceId(11), ResourceId(12),
                                 ResourceId(13)};
  SubmitCompositorFrameWithResources(ids, true, SurfaceId(), support.get(),
                                     surface_id);

  auto frame = AggregateFrame(surface_id);

  // Nothing should be available to be returned yet.
  EXPECT_TRUE(client.returned_resources().empty());

  SubmitCompositorFrameWithResources({}, true, SurfaceId(), support.get(),
                                     surface_id);

  frame = AggregateFrame(surface_id);

  ASSERT_EQ(3u, client.returned_resources().size());
  ResourceId returned_ids[3];
  for (size_t i = 0; i < 3; ++i) {
    returned_ids[i] = client.returned_resources()[i].id;
  }
  EXPECT_THAT(returned_ids,
              testing::WhenSorted(testing::ElementsAreArray(ids)));
}

// This test verifies that when a CompositorFrame is submitted to a new surface
// ID, and a new display frame is generated, then the resources of the old
// surface are returned to the appropriate client.
TEST_F(SurfaceAggregatorWithResourcesTest, ReturnResourcesAsSurfacesChange) {
  FakeCompositorFrameSinkClient client;
  auto support = std::make_unique<CompositorFrameSinkSupport>(
      &client, &manager_, kArbitraryRootFrameSinkId, kRootIsRoot);
  LocalSurfaceId local_surface_id1(7u, base::UnguessableToken::Create());
  LocalSurfaceId local_surface_id2(8u, base::UnguessableToken::Create());
  SurfaceId surface_id1(support->frame_sink_id(), local_surface_id1);
  SurfaceId surface_id2(support->frame_sink_id(), local_surface_id2);

  std::vector<ResourceId> ids = {ResourceId(11), ResourceId(12),
                                 ResourceId(13)};
  SubmitCompositorFrameWithResources(ids, true, SurfaceId(), support.get(),
                                     surface_id1);

  auto frame = AggregateFrame(surface_id1);

  // Nothing should be available to be returned yet.
  EXPECT_TRUE(client.returned_resources().empty());

  // Submitting a CompositorFrame to |surface_id2| should cause the surface
  // associated with |surface_id1| to get garbage collected.
  SubmitCompositorFrameWithResources({}, true, SurfaceId(), support.get(),
                                     surface_id2);
  manager_.surface_manager()->GarbageCollectSurfaces();

  frame = AggregateFrame(surface_id2);

  ASSERT_EQ(3u, client.returned_resources().size());
  ResourceId returned_ids[3];
  for (size_t i = 0; i < 3; ++i) {
    returned_ids[i] = client.returned_resources()[i].id;
  }
  EXPECT_THAT(returned_ids,
              testing::WhenSorted(testing::ElementsAreArray(ids)));
}

TEST_F(SurfaceAggregatorWithResourcesTest, TakeInvalidResources) {
  FakeCompositorFrameSinkClient client;
  auto support = std::make_unique<CompositorFrameSinkSupport>(
      &client, &manager_, kArbitraryRootFrameSinkId, kRootIsRoot);
  LocalSurfaceId local_surface_id(7u, base::UnguessableToken::Create());
  SurfaceId surface_id(support->frame_sink_id(), local_surface_id);

  TransferableResource resource;
  resource.id = ResourceId(11);
  // ResourceProvider is software but resource is not, so it should be
  // ignored.
  resource.is_software = false;

  CompositorFrame frame = CompositorFrameBuilder()
                              .AddDefaultRenderPass()
                              .AddTransferableResource(resource)
                              .Build();
  support->SubmitCompositorFrame(local_surface_id, std::move(frame));

  auto returned_frame = AggregateFrame(surface_id);

  // Nothing should be available to be returned yet.
  EXPECT_TRUE(client.returned_resources().empty());

  SubmitCompositorFrameWithResources({}, true, SurfaceId(), support.get(),
                                     surface_id);
  ASSERT_EQ(1u, client.returned_resources().size());
  EXPECT_EQ(ResourceId(11u), client.returned_resources()[0].id);
}

TEST_F(SurfaceAggregatorWithResourcesTest, TwoSurfaces) {
  FakeCompositorFrameSinkClient client;
  auto support1 = std::make_unique<CompositorFrameSinkSupport>(
      &client, &manager_, FrameSinkId(1, 1), kChildIsRoot);
  auto support2 = std::make_unique<CompositorFrameSinkSupport>(
      &client, &manager_, FrameSinkId(2, 2), kChildIsRoot);
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
  EXPECT_EQ(3u, resource_provider_->num_resources());
}

// Ensure that aggregator completely ignores Surfaces that reference invalid
// resources.
TEST_F(SurfaceAggregatorWithResourcesTest, InvalidChildSurface) {
  auto root_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryRootFrameSinkId, kRootIsRoot);
  auto middle_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryMiddleFrameSinkId, kChildIsRoot);
  auto child_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryFrameSinkId1, kChildIsRoot);
  LocalSurfaceId root_local_surface_id(7u, kArbitraryToken);
  SurfaceId root_surface_id(root_support->frame_sink_id(),
                            root_local_surface_id);
  LocalSurfaceId middle_local_surface_id(8u, kArbitraryToken2);
  SurfaceId middle_surface_id(middle_support->frame_sink_id(),
                              middle_local_surface_id);
  LocalSurfaceId child_local_surface_id(9u, kArbitraryToken3);
  SurfaceId child_surface_id(child_support->frame_sink_id(),
                             child_local_surface_id);

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
                                     root_support.get(), root_surface_id);

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
      nullptr, &manager_, FrameSinkId(1, 1), kChildIsRoot);
  auto support2 = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, FrameSinkId(2, 2), kChildIsRoot);
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
                         SurfaceRange(base::nullopt, surface1_id),
                         SK_ColorWHITE,
                         /*stretch_content_to_fill_bounds=*/false);
    pass->copy_requests.push_back(CopyOutputRequest::CreateStubForTesting());

    CompositorFrame frame =
        CompositorFrameBuilder().AddRenderPass(std::move(pass)).Build();

    support2->SubmitCompositorFrame(local_frame2_id, std::move(frame));
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

  aggregator_->set_output_is_secure(false);

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
  const gfx::Rect full_damage_rect(SurfaceSize());
  const gfx::Rect partial_damage_rect(45, 45, 10, 10);

  std::vector<Quad> quads[2] = {
      {Quad::SolidColorQuad(SK_ColorWHITE, gfx::Rect(5, 5)),
       Quad::SolidColorQuad(SK_ColorLTGRAY, gfx::Rect(5, 5))},
      {Quad::SolidColorQuad(SK_ColorGRAY, gfx::Rect(5, 5)),
       Quad::SolidColorQuad(SK_ColorDKGRAY, gfx::Rect(5, 5))}};

  gfx::DisplayColorSpaces display_color_spaces(gfx::ColorSpace::CreateSRGB());
  display_color_spaces.SetOutputColorSpaceAndBufferFormat(
      gfx::ContentColorUsage::kWideColorGamut, false /* needs_alpha */,
      gfx::ColorSpace(gfx::ColorSpace::PrimaryID::BT2020,
                      gfx::ColorSpace::TransferID::IEC61966_2_1),
      gfx::BufferFormat::RGBA_8888);
  display_color_spaces.SetOutputColorSpaceAndBufferFormat(
      gfx::ContentColorUsage::kWideColorGamut, true /* needs_alpha */,
      gfx::ColorSpace::CreateSCRGBLinear(), gfx::BufferFormat::RGBA_8888);
  display_color_spaces.SetOutputColorSpaceAndBufferFormat(
      gfx::ContentColorUsage::kHDR, false /* needs_alpha */,
      gfx::ColorSpace::CreateHDR10(), gfx::BufferFormat::BGRA_1010102);
  display_color_spaces.SetOutputColorSpaceAndBufferFormat(
      gfx::ContentColorUsage::kHDR, true /* needs_alpha */,
      gfx::ColorSpace::CreateSCRGBLinear(), gfx::BufferFormat::RGBA_F16);

  std::vector<Pass> passes = {
      Pass(quads[0], CompositorRenderPassId{2}, SurfaceSize()),
      Pass(quads[1], CompositorRenderPassId{1}, SurfaceSize())};
  passes[1].has_transparent_background = true;
  passes[1].damage_rect = partial_damage_rect;
  passes[0].damage_rect = child_pass_damage_rect;

  // HDR content with a transparent background will get an extra RenderPass
  // converting to SCRGB-linear.
  aggregator_.SetDisplayColorSpaces(display_color_spaces);
  {
    SubmitCompositorFrame(root_sink_.get(), passes, root_local_surface_id_,
                          device_scale_factor);
    SurfaceId surface_id(root_sink_->frame_sink_id(), root_local_surface_id_);

    auto aggregated_frame = AggregateFrame(surface_id);

    EXPECT_EQ(3u, aggregated_frame.render_pass_list.size());
    EXPECT_EQ(gfx::ContentColorUsage::kHDR,
              aggregated_frame.render_pass_list[0]->content_color_usage);
    EXPECT_EQ(gfx::ContentColorUsage::kHDR,
              aggregated_frame.render_pass_list[1]->content_color_usage);
    EXPECT_EQ(gfx::ContentColorUsage::kHDR,
              aggregated_frame.render_pass_list[2]->content_color_usage);

    // All passes will have full damage for the first frame.
    EXPECT_EQ(full_damage_rect,
              aggregated_frame.render_pass_list[2]->damage_rect);
    EXPECT_EQ(full_damage_rect,
              aggregated_frame.render_pass_list[1]->damage_rect);
    EXPECT_EQ(full_damage_rect,
              aggregated_frame.render_pass_list[0]->damage_rect);
  }

  // HDR content with an opaque background will get an extra RenderPass
  // converting to HDR10.
  passes[1].has_transparent_background = false;
  {
    SubmitCompositorFrame(root_sink_.get(), passes, root_local_surface_id_,
                          device_scale_factor);
    SurfaceId surface_id(root_sink_->frame_sink_id(), root_local_surface_id_);

    auto aggregated_frame = AggregateFrame(surface_id);

    EXPECT_EQ(3u, aggregated_frame.render_pass_list.size());
    EXPECT_EQ(gfx::ContentColorUsage::kHDR,
              aggregated_frame.render_pass_list[0]->content_color_usage);
    EXPECT_EQ(gfx::ContentColorUsage::kHDR,
              aggregated_frame.render_pass_list[1]->content_color_usage);
    EXPECT_EQ(gfx::ContentColorUsage::kHDR,
              aggregated_frame.render_pass_list[2]->content_color_usage);

    // The root pass (drawn to the backbuffer) and the intermediate pass (drawn
    // to extended-sRGB) will now have partial damage. Note that the root pass
    // will end up getting full damage due to the OutputSurface::Reshape call
    // that will be made by DirectRenderer.
    EXPECT_EQ(partial_damage_rect,
              aggregated_frame.render_pass_list[2]->damage_rect);
    EXPECT_EQ(partial_damage_rect,
              aggregated_frame.render_pass_list[1]->damage_rect);
  }

  // This simulates the situation where we don't have HDR capabilities. Opaque
  // content can be drawn into a BT2020 buffer as 10-10-10-2, but transparent
  // content needs to bump up to 16-bit, and therefore (until we find a way
  // around this) linear color space.
  display_color_spaces.SetOutputColorSpaceAndBufferFormat(
      gfx::ContentColorUsage::kHDR, false /* needs_alpha */,
      gfx::ColorSpace(gfx::ColorSpace::PrimaryID::BT2020,
                      gfx::ColorSpace::TransferID::IEC61966_2_1),
      gfx::BufferFormat::BGRA_1010102);
  display_color_spaces.SetOutputColorSpaceAndBufferFormat(
      gfx::ContentColorUsage::kHDR, true /* needs_alpha */,
      gfx::ColorSpace::CreateSCRGBLinear(), gfx::BufferFormat::RGBA_F16);

  // Opaque content renders to the appropriate space directly.
  passes[1].has_transparent_background = false;
  aggregator_.SetDisplayColorSpaces(display_color_spaces);
  {
    SubmitCompositorFrame(root_sink_.get(), passes, root_local_surface_id_,
                          device_scale_factor);
    SurfaceId surface_id(root_sink_->frame_sink_id(), root_local_surface_id_);

    auto aggregated_frame = AggregateFrame(surface_id);

    EXPECT_EQ(2u, aggregated_frame.render_pass_list.size());
    EXPECT_EQ(gfx::ContentColorUsage::kHDR,
              aggregated_frame.render_pass_list[0]->content_color_usage);
    EXPECT_EQ(gfx::ContentColorUsage::kHDR,
              aggregated_frame.render_pass_list[1]->content_color_usage);

    // The root pass has full damage because the intermediate pass was removed.
    EXPECT_EQ(full_damage_rect,
              aggregated_frame.render_pass_list[1]->damage_rect);
  }

  // When the root pass has a transparent background, we'll end up getting a
  // color conversion pass.
  passes[1].has_transparent_background = true;
  {
    SubmitCompositorFrame(root_sink_.get(), passes, root_local_surface_id_,
                          device_scale_factor);
    SurfaceId surface_id(root_sink_->frame_sink_id(), root_local_surface_id_);

    auto aggregated_frame = AggregateFrame(surface_id);

    EXPECT_EQ(3u, aggregated_frame.render_pass_list.size());
    EXPECT_EQ(gfx::ContentColorUsage::kHDR,
              aggregated_frame.render_pass_list[0]->content_color_usage);
    EXPECT_EQ(gfx::ContentColorUsage::kHDR,
              aggregated_frame.render_pass_list[1]->content_color_usage);
    EXPECT_EQ(gfx::ContentColorUsage::kHDR,
              aggregated_frame.render_pass_list[2]->content_color_usage);

    // The root (drawn to backbuffer) and intermediate (drawn to extended-sRGB)
    // passes have full damage because they were added this frame.
    EXPECT_EQ(full_damage_rect,
              aggregated_frame.render_pass_list[2]->damage_rect);
    EXPECT_EQ(full_damage_rect,
              aggregated_frame.render_pass_list[1]->damage_rect);
  }
}

// Ensure that the render passes have correct color spaces.
TEST_F(SurfaceAggregatorValidSurfaceTest, MetadataContentColorUsageTest) {
  auto test_content_color_usage_aggregation =
      [this](gfx::ContentColorUsage content_color_usage) {
        std::vector<Quad> child_quads = {
            Quad::SolidColorQuad(SK_ColorGREEN, gfx::Rect(5, 5))};
        std::vector<Pass> child_passes = {
            Pass(child_quads, CompositorRenderPassId{1}, gfx::Size(100, 100))};

        CompositorFrame child_frame = MakeEmptyCompositorFrame();
        // Set the child's color space
        child_frame.metadata.content_color_usage = content_color_usage;
        AddPasses(&child_frame.render_pass_list, child_passes,
                  &child_frame.metadata.referenced_surfaces);

        ParentLocalSurfaceIdAllocator child_allocator;
        child_allocator.GenerateId();
        LocalSurfaceId child_local_surface_id =
            child_allocator.GetCurrentLocalSurfaceId();
        SurfaceId child_surface_id(child_sink_->frame_sink_id(),
                                   child_local_surface_id);
        child_sink_->SubmitCompositorFrame(child_local_surface_id,
                                           std::move(child_frame));

        std::vector<Quad> root_quads = {Quad::SurfaceQuad(
            SurfaceRange(base::nullopt, child_surface_id), SK_ColorWHITE,
            gfx::Rect(5, 5), /*stretch_content_to_fill_bounds=*/false)};
        std::vector<Pass> root_passes = {Pass(root_quads, SurfaceSize())};

        CompositorFrame root_frame = MakeEmptyCompositorFrame();
        root_frame.metadata.content_color_usage = content_color_usage;
        AddPasses(&root_frame.render_pass_list, root_passes,
                  &root_frame.metadata.referenced_surfaces);

        root_sink_->SubmitCompositorFrame(root_local_surface_id_,
                                          std::move(root_frame));

        SurfaceId root_surface_id(root_sink_->frame_sink_id(),
                                  root_local_surface_id_);
        auto aggregated_frame = AggregateFrame(root_surface_id);
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
      Quad::SolidColorQuad(SK_ColorGREEN, gfx::Rect(5, 5))};
  std::vector<Pass> child_surface_passes = {
      Pass(child_surface_quads, CompositorRenderPassId{1}, SurfaceSize())};

  CompositorFrame child_surface_frame = MakeEmptyCompositorFrame();
  AddPasses(&child_surface_frame.render_pass_list, child_surface_passes,
            &child_surface_frame.metadata.referenced_surfaces);

  ParentLocalSurfaceIdAllocator allocator;
  allocator.GenerateId();
  LocalSurfaceId child_local_surface_id = allocator.GetCurrentLocalSurfaceId();
  SurfaceId child_surface_id(child_sink_->frame_sink_id(),
                             child_local_surface_id);
  child_sink_->SubmitCompositorFrame(child_local_surface_id,
                                     std::move(child_surface_frame));

  std::vector<Quad> root_surface_quads = {Quad::SurfaceQuad(
      SurfaceRange(base::nullopt, child_surface_id), SK_ColorWHITE,
      gfx::Rect(5, 5), /*stretch_content_to_fill_bounds=*/false)};
  std::vector<Pass> root_passes = {
      Pass(root_surface_quads, CompositorRenderPassId{1}, SurfaceSize())};

  CompositorFrame root_frame = MakeEmptyCompositorFrame();
  AddPasses(&root_frame.render_pass_list, root_passes,
            &root_frame.metadata.referenced_surfaces);

  SurfaceId root_surface_id(root_sink_->frame_sink_id(),
                            root_local_surface_id_);
  root_sink_->SubmitCompositorFrame(root_local_surface_id_,
                                    std::move(root_frame));

  // On first frame there is no existing cache texture to worry about re-using,
  // so we don't worry what this bool is set to.
  auto aggregated_frame = AggregateFrame(root_surface_id);

  // No Surface changed, so no damage should be given.
  {
    auto aggregated_frame = AggregateFrame(root_surface_id);
    EXPECT_FALSE(aggregated_frame.render_pass_list[0]
                     ->has_damage_from_contributing_content);
  }

  // Change child_frame with damage should set the flag.
  {
    CompositorFrame child_surface_frame = MakeEmptyCompositorFrame();
    AddPasses(&child_surface_frame.render_pass_list, child_surface_passes,
              &child_surface_frame.metadata.referenced_surfaces);
    child_sink_->SubmitCompositorFrame(child_local_surface_id,
                                       std::move(child_surface_frame));

    auto aggregated_frame = AggregateFrame(root_surface_id);
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
    child_sink_->SubmitCompositorFrame(child_local_surface_id,
                                       std::move(child_surface_frame));

    auto aggregated_frame = AggregateFrame(root_surface_id);
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
      nullptr, &manager_, kArbitraryMiddleFrameSinkId, kChildIsRoot);

  std::vector<Quad> child_surface_quads = {
      Quad::SolidColorQuad(SK_ColorGREEN, gfx::Rect(5, 5))};
  std::vector<Pass> child_surface_passes = {
      Pass(child_surface_quads, CompositorRenderPassId{1}, SurfaceSize())};

  CompositorFrame child_surface_frame = MakeEmptyCompositorFrame();
  AddPasses(&child_surface_frame.render_pass_list, child_surface_passes,
            &child_surface_frame.metadata.referenced_surfaces);

  ParentLocalSurfaceIdAllocator child_allocator;
  child_allocator.GenerateId();
  LocalSurfaceId child_local_surface_id =
      child_allocator.GetCurrentLocalSurfaceId();
  SurfaceId child_surface_id(child_sink_->frame_sink_id(),
                             child_local_surface_id);
  child_sink_->SubmitCompositorFrame(child_local_surface_id,
                                     std::move(child_surface_frame));

  std::vector<Quad> root_surface_quads = {Quad::SurfaceQuad(
      SurfaceRange(base::nullopt, child_surface_id), SK_ColorWHITE,
      gfx::Rect(5, 5), /*stretch_content_to_fill_bounds=*/false)};
  std::vector<Pass> root_passes = {
      Pass(root_surface_quads, CompositorRenderPassId{1}, SurfaceSize())};

  CompositorFrame root_frame = MakeEmptyCompositorFrame();
  AddPasses(&root_frame.render_pass_list, root_passes,
            &root_frame.metadata.referenced_surfaces);

  SurfaceId root_surface_id(root_sink_->frame_sink_id(),
                            root_local_surface_id_);
  root_sink_->SubmitCompositorFrame(root_local_surface_id_,
                                    std::move(root_frame));

  // On first frame there is no existing cache texture to worry about re-using,
  // so we don't worry what this bool is set to.
  auto aggregated_frame = AggregateFrame(root_surface_id);

  // No Surface changed, so no damage should be given.
  {
    auto aggregated_frame = AggregateFrame(root_surface_id);
    EXPECT_FALSE(aggregated_frame.render_pass_list[0]
                     ->has_damage_from_contributing_content);
  }

  // Add a grand_child_frame should cause damage.
  std::vector<Quad> grand_child_quads = {
      Quad::SolidColorQuad(SK_ColorGREEN, gfx::Rect(5, 5))};
  std::vector<Pass> grand_child_passes = {
      Pass(grand_child_quads, CompositorRenderPassId{1}, SurfaceSize())};
  ParentLocalSurfaceIdAllocator grandchild_allocator;
  grandchild_allocator.GenerateId();
  LocalSurfaceId grand_child_local_surface_id =
      grandchild_allocator.GetCurrentLocalSurfaceId();
  SurfaceId grand_child_surface_id(grand_child_support->frame_sink_id(),
                                   grand_child_local_surface_id);
  {
    CompositorFrame grand_child_frame = MakeEmptyCompositorFrame();
    AddPasses(&grand_child_frame.render_pass_list, grand_child_passes,
              &grand_child_frame.metadata.referenced_surfaces);

    grand_child_support->SubmitCompositorFrame(grand_child_local_surface_id,
                                               std::move(grand_child_frame));

    std::vector<Quad> new_child_surface_quads = {
        child_surface_quads[0],
        Quad::SurfaceQuad(SurfaceRange(base::nullopt, grand_child_surface_id),
                          SK_ColorWHITE, gfx::Rect(5, 5),
                          /*stretch_content_to_fill_bounds=*/false)};
    std::vector<Pass> new_child_surface_passes = {Pass(
        new_child_surface_quads, CompositorRenderPassId{1}, SurfaceSize())};
    child_surface_frame = MakeEmptyCompositorFrame();
    AddPasses(&child_surface_frame.render_pass_list, new_child_surface_passes,
              &child_surface_frame.metadata.referenced_surfaces);
    child_sink_->SubmitCompositorFrame(child_local_surface_id,
                                       std::move(child_surface_frame));

    auto aggregated_frame = AggregateFrame(root_surface_id);
    // True for new grand_child_frame.
    EXPECT_TRUE(aggregated_frame.render_pass_list[0]
                    ->has_damage_from_contributing_content);
  }

  // No Surface changed, so no damage should be given.
  {
    auto aggregated_frame = AggregateFrame(root_surface_id);
    EXPECT_FALSE(aggregated_frame.render_pass_list[0]
                     ->has_damage_from_contributing_content);
  }

  // Change grand_child_frame with damage should set the flag.
  {
    CompositorFrame grand_child_frame = MakeEmptyCompositorFrame();
    AddPasses(&grand_child_frame.render_pass_list, grand_child_passes,
              &grand_child_frame.metadata.referenced_surfaces);
    grand_child_support->SubmitCompositorFrame(grand_child_local_surface_id,
                                               std::move(grand_child_frame));

    auto aggregated_frame = AggregateFrame(root_surface_id);
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
    grand_child_support->SubmitCompositorFrame(grand_child_local_surface_id,
                                               std::move(grand_child_frame));

    auto aggregated_frame = AggregateFrame(root_surface_id);
    // False for new grand_child_frame without damage.
    EXPECT_FALSE(aggregated_frame.render_pass_list[0]
                     ->has_damage_from_contributing_content);
  }
}

// Tests that has_damage_from_contributing_content is aggregated correctly from
// render pass quads.
TEST_F(SurfaceAggregatorValidSurfaceTest, HasDamageFromRenderPassQuads) {
  std::vector<Quad> child_quads = {
      Quad::SolidColorQuad(SK_ColorGREEN, gfx::Rect(5, 5))};
  std::vector<Pass> child_passes = {
      Pass(child_quads, CompositorRenderPassId{1}, SurfaceSize())};

  CompositorFrame child_frame = MakeEmptyCompositorFrame();
  AddPasses(&child_frame.render_pass_list, child_passes,
            &child_frame.metadata.referenced_surfaces);

  ParentLocalSurfaceIdAllocator allocator;
  allocator.GenerateId();
  LocalSurfaceId child_local_surface_id = allocator.GetCurrentLocalSurfaceId();
  SurfaceId child_surface_id(child_sink_->frame_sink_id(),
                             child_local_surface_id);
  child_sink_->SubmitCompositorFrame(child_local_surface_id,
                                     std::move(child_frame));

  std::vector<Quad> root_surface_quads = {Quad::SurfaceQuad(
      SurfaceRange(base::nullopt, child_surface_id), SK_ColorWHITE,
      gfx::Rect(5, 5), /*stretch_content_to_fill_bounds=*/false)};
  std::vector<Quad> root_render_pass_quads = {
      Quad::RenderPassQuad(CompositorRenderPassId{1}, gfx::Transform(), true)};

  std::vector<Pass> root_passes = {
      Pass(root_surface_quads, CompositorRenderPassId{1}, SurfaceSize()),
      Pass(root_render_pass_quads, CompositorRenderPassId{2}, SurfaceSize())};

  CompositorFrame root_frame = MakeEmptyCompositorFrame();
  AddPasses(&root_frame.render_pass_list, root_passes,
            &root_frame.metadata.referenced_surfaces);

  root_sink_->SubmitCompositorFrame(root_local_surface_id_,
                                    std::move(root_frame));

  SurfaceId root_surface_id(root_sink_->frame_sink_id(),
                            root_local_surface_id_);
  auto aggregated_frame = AggregateFrame(root_surface_id);

  // On first frame there is no existing cache texture to worry about re-using,
  // so we don't worry what this bool is set to.
  const auto& aggregated_pass_list = aggregated_frame.render_pass_list;

  ASSERT_EQ(2u, aggregated_pass_list.size());

  // No Surface changed, so no damage should be given.
  {
    auto aggregated_frame = AggregateFrame(root_surface_id);
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
    child_sink_->SubmitCompositorFrame(child_local_surface_id,
                                       std::move(child_frame));

    auto aggregated_frame = AggregateFrame(root_surface_id);
    // True for new child_frame.
    EXPECT_TRUE(aggregated_frame.render_pass_list[0]
                    ->has_damage_from_contributing_content);
    EXPECT_TRUE(aggregated_frame.render_pass_list[1]
                    ->has_damage_from_contributing_content);
  }
}

// Tests that the first frame damage_rect of a cached render pass should be
// fully damaged.
TEST_F(SurfaceAggregatorValidSurfaceTest, DamageRectOfCachedRenderPass) {
  CompositorRenderPassId pass_id[] = {CompositorRenderPassId{1},
                                      CompositorRenderPassId{2}};
  std::vector<Quad> root_quads[2] = {
      {Quad::SolidColorQuad(SK_ColorGREEN, gfx::Rect(5, 5))},
      {Quad::RenderPassQuad(pass_id[0], gfx::Transform(), true)},
  };
  std::vector<Pass> root_passes = {
      Pass(root_quads[0], pass_id[0], SurfaceSize()),
      Pass(root_quads[1], pass_id[1], SurfaceSize())};

  CompositorFrame root_frame = MakeEmptyCompositorFrame();
  AddPasses(&root_frame.render_pass_list, root_passes,
            &root_frame.metadata.referenced_surfaces);

  root_sink_->SubmitCompositorFrame(root_local_surface_id_,
                                    std::move(root_frame));

  SurfaceId root_surface_id(root_sink_->frame_sink_id(),
                            root_local_surface_id_);
  auto aggregated_frame = AggregateFrame(root_surface_id);

  const auto& aggregated_pass_list = aggregated_frame.render_pass_list;

  ASSERT_EQ(2u, aggregated_pass_list.size());

  // The root surface was enqueued without being aggregated once, so it should
  // be treated as completely damaged.
  EXPECT_TRUE(
      aggregated_pass_list[0]->damage_rect.Contains(gfx::Rect(SurfaceSize())));
  EXPECT_TRUE(
      aggregated_pass_list[1]->damage_rect.Contains(gfx::Rect(SurfaceSize())));

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

    root_sink_->SubmitCompositorFrame(root_local_surface_id_,
                                      std::move(root_frame));

    auto aggregated_frame = AggregateFrame(root_surface_id);
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

    root_sink_->SubmitCompositorFrame(root_local_surface_id_,
                                      std::move(root_frame));

    auto aggregated_frame = AggregateFrame(root_surface_id);
    const auto& aggregated_pass_list = aggregated_frame.render_pass_list;

    // Should have full damage.
    EXPECT_EQ(gfx::Rect(SurfaceSize()), aggregated_pass_list[0]->damage_rect);
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
      {Quad::SolidColorQuad(SK_ColorGREEN, gfx::Rect(5, 5))},
      {Quad::RenderPassQuad(pass_id[0], gfx::Transform(), true)},
  };
  std::vector<Pass> child_passes = {
      Pass(child_quads[0], pass_id[0], SurfaceSize()),
      Pass(child_quads[1], pass_id[1], SurfaceSize())};

  CompositorFrame child_frame = MakeEmptyCompositorFrame();
  AddPasses(&child_frame.render_pass_list, child_passes,
            &child_frame.metadata.referenced_surfaces);

  ParentLocalSurfaceIdAllocator allocator;
  allocator.GenerateId();
  LocalSurfaceId child_local_surface_id = allocator.GetCurrentLocalSurfaceId();
  SurfaceId child_surface_id(child_sink_->frame_sink_id(),
                             child_local_surface_id);
  child_sink_->SubmitCompositorFrame(child_local_surface_id,
                                     std::move(child_frame));

  std::vector<Quad> root_surface_quads = {Quad::SurfaceQuad(
      SurfaceRange(base::nullopt, child_surface_id), SK_ColorWHITE,
      gfx::Rect(5, 5), /*stretch_content_to_fill_bounds=*/false)};

  std::vector<Pass> root_passes = {
      Pass(root_surface_quads, CompositorRenderPassId{1}, SurfaceSize())};

  CompositorFrame root_frame = MakeEmptyCompositorFrame();
  AddPasses(&root_frame.render_pass_list, root_passes,
            &root_frame.metadata.referenced_surfaces);

  root_sink_->SubmitCompositorFrame(root_local_surface_id_,
                                    std::move(root_frame));

  SurfaceId root_surface_id(root_sink_->frame_sink_id(),
                            root_local_surface_id_);
  auto aggregated_frame = AggregateFrame(root_surface_id);

  const auto& aggregated_pass_list = aggregated_frame.render_pass_list;

  ASSERT_EQ(2u, aggregated_pass_list.size());

  // The root surface was enqueued without being aggregated once, so it should
  // be treated as completely damaged.
  EXPECT_TRUE(
      aggregated_pass_list[0]->damage_rect.Contains(gfx::Rect(SurfaceSize())));
  EXPECT_TRUE(
      aggregated_pass_list[1]->damage_rect.Contains(gfx::Rect(SurfaceSize())));

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

    child_sink_->SubmitCompositorFrame(child_local_surface_id,
                                       std::move(child_frame));

    auto aggregated_frame = AggregateFrame(root_surface_id);
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

    child_sink_->SubmitCompositorFrame(child_local_surface_id,
                                       std::move(child_frame));

    auto aggregated_frame = AggregateFrame(root_surface_id);
    const auto& aggregated_pass_list = aggregated_frame.render_pass_list;

    // Should have full damage.
    EXPECT_EQ(gfx::Rect(SurfaceSize()), aggregated_pass_list[0]->damage_rect);
    EXPECT_EQ(child_root_pass_damage, aggregated_pass_list[1]->damage_rect);
  }
}

// Tests that the damage rect from a child surface is clipped before
// aggregated with the parent damage rect when clipping is on
TEST_F(SurfaceAggregatorValidSurfaceTest, DamageRectWithClippedChildSurface) {
  std::vector<Quad> child_surface_quads = {
      Quad::SolidColorQuad(SK_ColorRED, gfx::Rect(SurfaceSize()))};
  std::vector<Pass> child_surface_passes = {
      Pass(child_surface_quads, CompositorRenderPassId{1}, SurfaceSize())};

  CompositorFrame child_surface_frame = MakeEmptyCompositorFrame();
  AddPasses(&child_surface_frame.render_pass_list, child_surface_passes,
            &child_surface_frame.metadata.referenced_surfaces);

  ParentLocalSurfaceIdAllocator allocator;
  allocator.GenerateId();
  LocalSurfaceId child_local_surface_id = allocator.GetCurrentLocalSurfaceId();
  SurfaceId child_surface_id(child_sink_->frame_sink_id(),
                             child_local_surface_id);
  child_sink_->SubmitCompositorFrame(child_local_surface_id,
                                     std::move(child_surface_frame));

  // root surface quads
  std::vector<Quad> root_surface_quads = {Quad::SurfaceQuad(
      SurfaceRange(base::nullopt, child_surface_id), SK_ColorWHITE,
      gfx::Rect(SurfaceSize()), /*stretch_content_to_fill_bounds=*/false)};
  std::vector<Pass> root_passes = {
      Pass(root_surface_quads, CompositorRenderPassId{1}, SurfaceSize())};

  CompositorFrame root_frame = MakeEmptyCompositorFrame();
  AddPasses(&root_frame.render_pass_list, root_passes,
            &root_frame.metadata.referenced_surfaces);

  SurfaceId root_surface_id(root_sink_->frame_sink_id(),
                            root_local_surface_id_);
  root_sink_->SubmitCompositorFrame(root_local_surface_id_,
                                    std::move(root_frame));
  // The damage rect of the very first frame is always the full rect
  auto aggregated_frame = AggregateFrame(root_surface_id);

  // Parameters used for damage rect testing
  gfx::Transform transform(0.5, 0, 0, 0.5, 20, 0);
  gfx::Rect clip_rect = gfx::Rect(30, 30, 40, 40);

  // Clipping is off
  {
    CompositorFrame child_surface_frame = MakeEmptyCompositorFrame();
    AddPasses(&child_surface_frame.render_pass_list, child_surface_passes,
              &child_surface_frame.metadata.referenced_surfaces);
    child_sink_->SubmitCompositorFrame(child_local_surface_id,
                                       std::move(child_surface_frame));

    CompositorFrame root_frame = MakeEmptyCompositorFrame();
    AddPasses(&root_frame.render_pass_list, root_passes,
              &root_frame.metadata.referenced_surfaces);
    auto* root_render_pass = root_frame.render_pass_list[0].get();
    auto* surface_quad_sqs = root_render_pass->shared_quad_state_list.front();
    surface_quad_sqs->quad_to_target_transform = transform;
    surface_quad_sqs->is_clipped = false;
    // Set the root damage rect to empty. Only the child surface will be tested.
    root_render_pass->damage_rect = gfx::Rect();
    root_sink_->SubmitCompositorFrame(root_local_surface_id_,
                                      std::move(root_frame));

    auto aggregated_frame = AggregateFrame(root_surface_id);

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
    child_sink_->SubmitCompositorFrame(child_local_surface_id,
                                       std::move(child_surface_frame));

    CompositorFrame root_frame = MakeEmptyCompositorFrame();
    AddPasses(&root_frame.render_pass_list, root_passes,
              &root_frame.metadata.referenced_surfaces);
    auto* root_render_pass = root_frame.render_pass_list[0].get();
    auto* surface_quad_sqs = root_render_pass->shared_quad_state_list.front();
    surface_quad_sqs->quad_to_target_transform = transform;
    surface_quad_sqs->is_clipped = true;
    surface_quad_sqs->clip_rect = clip_rect;
    root_render_pass->damage_rect = gfx::Rect();
    root_sink_->SubmitCompositorFrame(root_local_surface_id_,
                                      std::move(root_frame));

    auto aggregated_frame = AggregateFrame(root_surface_id);

    // The root damage rect should be the size of the clipped child surface
    // damage rect
    gfx::Rect expected_damage_rect(30, 30, 40, 20);
    EXPECT_EQ(aggregated_frame.render_pass_list[0]->damage_rect,
              expected_damage_rect);
  }
}

// Tests the damage rect with a invalid child frame
TEST_F(SurfaceAggregatorValidSurfaceTest, DamageRectWithInvalidChildFrame) {
  ParentLocalSurfaceIdAllocator allocator;
  allocator.GenerateId();
  LocalSurfaceId child_local_surface_id = allocator.GetCurrentLocalSurfaceId();
  SurfaceId child_surface_id(child_sink_->frame_sink_id(),
                             child_local_surface_id);

  std::vector<Quad> root_surface_quads = {Quad::SurfaceQuad(
      SurfaceRange(base::nullopt, child_surface_id), SK_ColorWHITE,
      gfx::Rect(0, 0, 100, 100), /*stretch_content_to_fill_bounds=*/false)};
  std::vector<Pass> root_passes = {
      Pass(root_surface_quads,
           /*size*/ gfx::Size(100, 100),
           /*damage_rect*/ gfx::Rect(10, 10, 20, 20))};

  CompositorFrame root_frame = MakeEmptyCompositorFrame();
  AddPasses(&root_frame.render_pass_list, root_passes,
            &root_frame.metadata.referenced_surfaces);

  SurfaceId root_surface_id(root_sink_->frame_sink_id(),
                            root_local_surface_id_);
  root_sink_->SubmitCompositorFrame(root_local_surface_id_,
                                    std::move(root_frame));

  // Frame # 0 - The primary surface of the child frame is not available.
  // The child frame is not submitted.
  // The damage rect of the very first frame is always the full rect.
  auto aggregated_frame = AggregateFrame(root_surface_id);
  auto* output_root_pass = aggregated_frame.render_pass_list.back().get();
  EXPECT_EQ(gfx::Rect(gfx::Rect(0, 0, 100, 100)),
            output_root_pass->damage_rect);
  EXPECT_EQ(output_root_pass->damage_rect,
            DamageListUnion(aggregated_frame.surface_damage_rect_list_));

  // Frame # 1 - The primary surface of the child frame is not available.
  {
    CompositorFrame root_frame = MakeEmptyCompositorFrame();
    AddPasses(&root_frame.render_pass_list, root_passes,
              &root_frame.metadata.referenced_surfaces);
    root_sink_->SubmitCompositorFrame(root_local_surface_id_,
                                      std::move(root_frame));

    auto aggregated_frame = AggregateFrame(root_surface_id);
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
      Quad::SolidColorQuad(SK_ColorRED, gfx::Rect(0, 0, 100, 100))};
  std::vector<Pass> child_surface_passes = {Pass(child_surface_quads,
                                                 CompositorRenderPassId{1},
                                                 gfx::Rect(20, 20, 50, 50))};
  {
    CompositorFrame child_surface_frame = MakeEmptyCompositorFrame();
    AddPasses(&child_surface_frame.render_pass_list, child_surface_passes,
              &child_surface_frame.metadata.referenced_surfaces);

    child_sink_->SubmitCompositorFrame(child_local_surface_id,
                                       std::move(child_surface_frame));

    CompositorFrame root_frame = MakeEmptyCompositorFrame();
    AddPasses(&root_frame.render_pass_list, root_passes,
              &root_frame.metadata.referenced_surfaces);
    root_sink_->SubmitCompositorFrame(root_local_surface_id_,
                                      std::move(root_frame));

    auto aggregated_frame = AggregateFrame(root_surface_id);
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
    allocator.GenerateId();
    LocalSurfaceId child_local_surface_id2 =
        allocator.GetCurrentLocalSurfaceId();
    SurfaceId child_surface_id2(child_sink_->frame_sink_id(),
                                child_local_surface_id2);
    std::vector<Quad> root_surface_quads = {Quad::SurfaceQuad(
        SurfaceRange(base::nullopt, child_surface_id2), SK_ColorWHITE,
        gfx::Rect(0, 0, 100, 100), /*stretch_content_to_fill_bounds=*/false)};
    std::vector<Pass> root_passes = {
        Pass(root_surface_quads,
             /*size*/ gfx::Size(100, 100),
             /*damage_rect*/ gfx::Rect(10, 10, 20, 20))};

    CompositorFrame root_frame = MakeEmptyCompositorFrame();
    AddPasses(&root_frame.render_pass_list, root_passes,
              &root_frame.metadata.referenced_surfaces);
    root_sink_->SubmitCompositorFrame(root_local_surface_id_,
                                      std::move(root_frame));

    auto aggregated_frame = AggregateFrame(root_surface_id);
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
      Quad::YUVVideoQuad(gfx::Rect(0, 0, 100, 100))};

  std::vector<Pass> child_surface_passes = {
      Pass(child_surface_quads, /*size*/ gfx::Size(100, 100),
           /*damage_rect*/ gfx::Rect(0, 0, 100, 100))};

  CompositorFrame child_surface_frame = MakeEmptyCompositorFrame();
  AddPasses(&child_surface_frame.render_pass_list, child_surface_passes,
            &child_surface_frame.metadata.referenced_surfaces);

  ParentLocalSurfaceIdAllocator allocator;
  allocator.GenerateId();
  LocalSurfaceId child_local_surface_id = allocator.GetCurrentLocalSurfaceId();
  SurfaceId child_surface_id(child_sink_->frame_sink_id(),
                             child_local_surface_id);
  child_sink_->SubmitCompositorFrame(child_local_surface_id,
                                     std::move(child_surface_frame));

  // Original video quad (0, 0, 100, 100) x this video_transform matrix ==
  // (10, 0, 80, 80).
  gfx::Transform video_transform(0.8f, 0, 0, 0.8f, 10.0f, 0);

  // root surface quads
  std::vector<Quad> root_surface_quads = {
      Quad::SolidColorQuad(SK_ColorRED, gfx::Rect(60, 0, 40, 40)),
      Quad::SurfaceQuad(
          SurfaceRange(base::nullopt, child_surface_id), SK_ColorWHITE,
          /*primary_surface_rect*/ gfx::Rect(0, 0, 100, 100),
          /*opacity*/ 1.f, video_transform,
          /*stretch_content_to_fill_bounds=*/false, gfx::MaskFilterInfo(),
          /*is_fast_rounded_corner=*/false)};

  std::vector<Pass> root_passes = {
      Pass(root_surface_quads,
           /*size*/ gfx::Size(200, 200),
           /*damage_rect*/ gfx::Rect(60, 0, 40, 40))};

  CompositorFrame root_frame = MakeEmptyCompositorFrame();
  AddPasses(&root_frame.render_pass_list, root_passes,
            &root_frame.metadata.referenced_surfaces);

  SurfaceId root_surface_id(root_sink_->frame_sink_id(),
                            root_local_surface_id_);
  root_sink_->SubmitCompositorFrame(root_local_surface_id_,
                                    std::move(root_frame));

  auto aggregated_frame = AggregateFrame(root_surface_id);

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
  EXPECT_TRUE(video_sqs->overlay_damage_index.has_value());
  auto index = video_sqs->overlay_damage_index.value();
  EXPECT_EQ(1U, index);
  EXPECT_EQ(gfx::Rect(10, 0, 80, 80),
            aggregated_frame.surface_damage_rect_list_[index]);

  // Frame #1 - Has occluding damage
  {
    CompositorFrame child_surface_frame = MakeEmptyCompositorFrame();
    AddPasses(&child_surface_frame.render_pass_list, child_surface_passes,
              &child_surface_frame.metadata.referenced_surfaces);
    child_sink_->SubmitCompositorFrame(child_local_surface_id,
                                       std::move(child_surface_frame));

    CompositorFrame root_frame = MakeEmptyCompositorFrame();
    AddPasses(&root_frame.render_pass_list, root_passes,
              &root_frame.metadata.referenced_surfaces);

    root_sink_->SubmitCompositorFrame(root_local_surface_id_,
                                      std::move(root_frame));

    auto aggregated_frame = AggregateFrame(root_surface_id);

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
    child_sink_->SubmitCompositorFrame(child_local_surface_id,
                                       std::move(child_surface_frame));

    // No change in root frame.
    auto aggregated_frame = AggregateFrame(root_surface_id);

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
    child_sink_->SubmitCompositorFrame(child_local_surface_id,
                                       std::move(child_surface_frame));

    // root surface quads, the solid quad (60, 0, 40, 40) is removed.
    std::vector<Quad> root_surface_quads = {Quad::SurfaceQuad(
        SurfaceRange(base::nullopt, child_surface_id), SK_ColorWHITE,
        /*primary_surface_rect*/ gfx::Rect(0, 0, 100, 100),
        /*opacity*/ 1.f, video_transform,
        /*stretch_content_to_fill_bounds=*/false, gfx::MaskFilterInfo(),
        /*is_fast_rounded_corner=*/false)};

    std::vector<Pass> root_passes = {
        Pass(root_surface_quads,
             /*size*/ gfx::Size(200, 200),
             /*damage_rect*/ gfx::Rect(60, 0, 40, 40))};

    CompositorFrame root_frame = MakeEmptyCompositorFrame();
    AddPasses(&root_frame.render_pass_list, root_passes,
              &root_frame.metadata.referenced_surfaces);

    root_sink_->SubmitCompositorFrame(root_local_surface_id_,
                                      std::move(root_frame));

    auto aggregated_frame = AggregateFrame(root_surface_id);

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
    surface_quad_sqs->is_clipped = true;
    surface_quad_sqs->clip_rect = gfx::Rect(20, 0, 60, 80);

    child_sink_->SubmitCompositorFrame(child_local_surface_id,
                                       std::move(child_surface_frame));

    CompositorFrame root_frame = MakeEmptyCompositorFrame();
    AddPasses(&root_frame.render_pass_list, root_passes,
              &root_frame.metadata.referenced_surfaces);

    root_sink_->SubmitCompositorFrame(root_local_surface_id_,
                                      std::move(root_frame));

    auto aggregated_frame = AggregateFrame(root_surface_id);

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

    child_sink_->SubmitCompositorFrame(child_local_surface_id,
                                       std::move(child_surface_frame));

    // root surface quads
    std::vector<Quad> root_surface_quads = {
        Quad::SolidColorQuad(SK_ColorRED, gfx::Rect(60, 0, 100, 100)),
        Quad::SurfaceQuad(
            SurfaceRange(base::nullopt, child_surface_id), SK_ColorWHITE,
            /*primary_surface_rect*/ gfx::Rect(0, 0, 100, 100),
            /*opacity*/ 1.f, video_transform,
            /*stretch_content_to_fill_bounds=*/false, gfx::MaskFilterInfo(),
            /*is_fast_rounded_corner=*/false)};

    std::vector<Pass> root_passes = {
        Pass(root_surface_quads,
             /*size*/ gfx::Size(200, 200),
             /*damage_rect*/ gfx::Rect(60, 0, 80, 70))};

    CompositorFrame root_frame = MakeEmptyCompositorFrame();
    AddPasses(&root_frame.render_pass_list, root_passes,
              &root_frame.metadata.referenced_surfaces);

    auto* last_pass = root_frame.render_pass_list.back().get();
    auto* solid_quad_sqs = last_pass->shared_quad_state_list.front();
    solid_quad_sqs->is_clipped = true;
    solid_quad_sqs->clip_rect = gfx::Rect(80, 0, 40, 30);

    root_sink_->SubmitCompositorFrame(root_local_surface_id_,
                                      std::move(root_frame));

    auto aggregated_frame = AggregateFrame(root_surface_id);

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
      {Quad::SolidColorQuad(SK_ColorRED, gfx::Rect(0, 0, 50, 50)),
       Quad::YUVVideoQuad(gfx::Rect(0, 0, 100, 100))});

  child_surface_passes =
      std::vector<Pass>({Pass(child_surface_quads, /*size*/ gfx::Size(100, 100),
                              /*damage_rect*/ gfx::Rect(0, 0, 100, 100))});

  // Frame #6 - Child surface contains a quad other than the video
  {
    CompositorFrame child_surface_frame = MakeEmptyCompositorFrame();
    AddPasses(&child_surface_frame.render_pass_list, child_surface_passes,
              &child_surface_frame.metadata.referenced_surfaces);
    child_sink_->SubmitCompositorFrame(child_local_surface_id,
                                       std::move(child_surface_frame));

    // No change in root frame.
    auto aggregated_frame = AggregateFrame(root_surface_id);

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
    auto* surface_quad_sqs = render_pass->shared_quad_state_list.front();
    surface_quad_sqs->no_damage = true;

    child_sink_->SubmitCompositorFrame(child_local_surface_id,
                                       std::move(child_surface_frame));

    // No change in root frame.
    auto aggregated_frame = AggregateFrame(root_surface_id);

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
    EXPECT_EQ(0U, index);

    // Video quad(10, 0, 80, 80) is damaged.
    EXPECT_EQ(gfx::Rect(10, 0, 80, 80),
              aggregated_frame.surface_damage_rect_list_[index]);
  }
}

// Tests that quads outside the damage rect are not ignored for cached render
// pass.
TEST_F(SurfaceAggregatorPartialSwapTest, NotIgnoreOutsideForCachedRenderPass) {
  ParentLocalSurfaceIdAllocator allocator;
  allocator.GenerateId();
  LocalSurfaceId child_local_surface_id = allocator.GetCurrentLocalSurfaceId();
  SurfaceId child_surface_id(child_sink_->frame_sink_id(),
                             child_local_surface_id);
  // The child surface has two quads, one with a visible rect of 15,15 6x6 and
  // the other other with a visible rect of 10,10 2x2 (relative to root target
  // space).
  constexpr float device_scale_factor = 1.0f;
  {
    CompositorRenderPassId pass_id[] = {CompositorRenderPassId{1},
                                        CompositorRenderPassId{2}};
    std::vector<Quad> child_quads[2] = {
        {Quad::SolidColorQuad(SK_ColorGREEN, gfx::Rect(5, 5))},
        {Quad::RenderPassQuad(pass_id[0], gfx::Transform(), true)},
    };
    std::vector<Pass> child_passes = {
        Pass(child_quads[0], pass_id[0], SurfaceSize()),
        Pass(child_quads[1], pass_id[1], SurfaceSize())};

    CompositorRenderPassList child_pass_list;
    std::vector<SurfaceRange> referenced_surfaces;
    AddPasses(&child_pass_list, child_passes, &referenced_surfaces);

    child_pass_list[0]->quad_list.ElementAt(0)->visible_rect =
        gfx::Rect(1, 1, 3, 3);
    auto* child_sqs = child_pass_list[0]->shared_quad_state_list.ElementAt(0u);
    child_sqs->quad_to_target_transform.Translate(3, 3);
    child_sqs->quad_to_target_transform.Scale(2, 2);

    child_pass_list[0]->cache_render_pass = true;

    child_pass_list[1]->quad_list.ElementAt(0)->visible_rect =
        gfx::Rect(0, 0, 2, 2);

    SubmitPassListAsFrame(child_sink_.get(), child_local_surface_id,
                          &child_pass_list, std::move(referenced_surfaces),
                          device_scale_factor);
  }

  {
    CompositorRenderPassId pass_id[] = {CompositorRenderPassId{1},
                                        CompositorRenderPassId{2}};
    std::vector<Quad> root_quads[2] = {
        {Quad::SurfaceQuad(SurfaceRange(base::nullopt, child_surface_id),
                           SK_ColorWHITE, gfx::Rect(5, 5),
                           /*stretch_content_to_fill_bounds=*/false)},
        {Quad::RenderPassQuad(pass_id[0], gfx::Transform(), true)},
    };
    std::vector<Pass> root_passes = {
        Pass(root_quads[0], pass_id[0], SurfaceSize()),
        Pass(root_quads[1], pass_id[1], SurfaceSize())};

    CompositorRenderPassList root_pass_list;
    std::vector<SurfaceRange> referenced_surfaces;
    AddPasses(&root_pass_list, root_passes, &referenced_surfaces);

    auto* root_pass = root_pass_list[1].get();
    root_pass->shared_quad_state_list.front()
        ->quad_to_target_transform.Translate(10, 10);
    root_pass->damage_rect = gfx::Rect(0, 0, 1, 1);

    SubmitPassListAsFrame(root_sink_.get(), root_local_surface_id_,
                          &root_pass_list, std::move(referenced_surfaces),
                          device_scale_factor);
  }

  SurfaceId root_surface_id(root_sink_->frame_sink_id(),
                            root_local_surface_id_);
  auto aggregated_frame = AggregateFrame(root_surface_id);

  const auto& aggregated_pass_list = aggregated_frame.render_pass_list;

  ASSERT_EQ(3u, aggregated_pass_list.size());

  // Damage rect for first aggregation should contain entire root surface.
  EXPECT_EQ(gfx::Rect(SurfaceSize()), aggregated_pass_list[2]->damage_rect);
  EXPECT_EQ(1u, aggregated_pass_list[0]->quad_list.size());
  EXPECT_EQ(1u, aggregated_pass_list[1]->quad_list.size());
  EXPECT_EQ(1u, aggregated_pass_list[2]->quad_list.size());

  // Test should not ignore outside for cached render pass.
  // Create a root surface with a smaller damage rect.
  {
    CompositorRenderPassId pass_id[] = {CompositorRenderPassId{1},
                                        CompositorRenderPassId{2}};
    std::vector<Quad> root_quads[2] = {
        {Quad::SurfaceQuad(SurfaceRange(base::nullopt, child_surface_id),
                           SK_ColorWHITE, gfx::Rect(5, 5),
                           /*stretch_content_to_fill_bounds=*/false)},
        {Quad::RenderPassQuad(pass_id[0], gfx::Transform(), true)},
    };
    std::vector<Pass> root_passes = {
        Pass(root_quads[0], pass_id[0], SurfaceSize()),
        Pass(root_quads[1], pass_id[1], SurfaceSize())};

    CompositorRenderPassList root_pass_list;
    std::vector<SurfaceRange> referenced_surfaces;
    AddPasses(&root_pass_list, root_passes, &referenced_surfaces);

    auto* root_pass = root_pass_list[1].get();
    root_pass->shared_quad_state_list.front()
        ->quad_to_target_transform.Translate(10, 10);
    root_pass->damage_rect = gfx::Rect(10, 10, 2, 2);
    SubmitPassListAsFrame(root_sink_.get(), root_local_surface_id_,
                          &root_pass_list, std::move(referenced_surfaces),
                          device_scale_factor);
  }

  {
    auto aggregated_frame = AggregateFrame(root_surface_id);
    const auto& aggregated_pass_list = aggregated_frame.render_pass_list;

    ASSERT_EQ(3u, aggregated_pass_list.size());

    // The first quad is a cached render pass, should be included and fully
    // damaged.
    EXPECT_EQ(gfx::Rect(1, 1, 3, 3),
              aggregated_pass_list[0]->quad_list.back()->visible_rect);
    EXPECT_EQ(gfx::Rect(0, 0, 2, 2),
              aggregated_pass_list[1]->quad_list.back()->visible_rect);
    EXPECT_EQ(gfx::Rect(SurfaceSize()), aggregated_pass_list[0]->damage_rect);
    EXPECT_EQ(gfx::Rect(10, 10, 2, 2), aggregated_pass_list[1]->damage_rect);
    EXPECT_EQ(gfx::Rect(10, 10, 2, 2), aggregated_pass_list[2]->damage_rect);
    EXPECT_EQ(1u, aggregated_pass_list[0]->quad_list.size());
    EXPECT_EQ(1u, aggregated_pass_list[1]->quad_list.size());
    EXPECT_EQ(1u, aggregated_pass_list[2]->quad_list.size());
  }
}

// Validates that while the display transform is applied to the aggregated frame
// and its damage, its not applied to the callback to the root frame sink.
TEST_F(SurfaceAggregatorValidSurfaceTest, DisplayTransformDamageCallback) {
  auto primary_child_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryFrameSinkId1, kChildIsRoot);
  ParentLocalSurfaceIdAllocator child_allocator;
  child_allocator.GenerateId();
  LocalSurfaceId primary_child_local_surface_id =
      child_allocator.GetCurrentLocalSurfaceId();
  SurfaceId primary_child_surface_id(primary_child_support->frame_sink_id(),
                                     primary_child_local_surface_id);

  {
    auto pass = CompositorRenderPass::Create();
    pass->SetNew(CompositorRenderPassId{1}, gfx::Rect(0, 0, 20, 20),
                 gfx::Rect(), gfx::Transform());
    auto* sqs = pass->CreateAndAppendSharedQuadState();
    auto* solid_color_quad =
        pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();

    solid_color_quad->SetNew(sqs, gfx::Rect(0, 0, 20, 20),
                             gfx::Rect(0, 0, 20, 20), SK_ColorRED, false);

    CompositorFrame frame =
        CompositorFrameBuilder().AddRenderPass(std::move(pass)).Build();

    primary_child_support->SubmitCompositorFrame(primary_child_local_surface_id,
                                                 std::move(frame));
  }

  constexpr gfx::Rect surface_quad_rect(10, 5);
  std::vector<Quad> root_quads = {Quad::SurfaceQuad(
      SurfaceRange(primary_child_surface_id), SK_ColorWHITE, surface_quad_rect,
      /*stretch_content_to_fill_bounds=*/true)};

  constexpr gfx::Size surface_size(60, 100);
  std::vector<Pass> root_passes = {Pass(root_quads, surface_size)};

  MockAggregatedDamageCallback aggregated_damage_callback;
  root_sink_->SetAggregatedDamageCallbackForTesting(
      aggregated_damage_callback.GetCallback());

  SurfaceId root_surface_id(root_sink_->frame_sink_id(),
                            root_local_surface_id_);
  SubmitCompositorFrame(root_sink_.get(), root_passes, root_local_surface_id_,
                        0.5f);

  EXPECT_CALL(aggregated_damage_callback,
              OnAggregatedDamage(root_local_surface_id_, surface_size,
                                 gfx::Rect(surface_size), next_display_time()));

  auto frame =
      aggregator_.Aggregate(root_surface_id, GetNextDisplayTimeAndIncrement(),
                            gfx::OVERLAY_TRANSFORM_ROTATE_90);
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
      nullptr, &manager_, kArbitraryMiddleFrameSinkId, kChildIsRoot);
  // Child surface.
  ParentLocalSurfaceIdAllocator child_allocator;
  child_allocator.GenerateId();
  LocalSurfaceId child_local_surface_id =
      child_allocator.GetCurrentLocalSurfaceId();
  SurfaceId child_surface_id(child_sink_->frame_sink_id(),
                             child_local_surface_id);
  {
    std::vector<Quad> child_quads[1] = {
        {Quad::SolidColorQuad(SK_ColorGREEN, gfx::Rect(5, 5))},
    };
    std::vector<Pass> child_passes = {
        Pass(child_quads[0], CompositorRenderPassId{1}, SurfaceSize())};

    CompositorFrame child_frame = MakeEmptyCompositorFrame();
    AddPasses(&child_frame.render_pass_list, child_passes,
              &child_frame.metadata.referenced_surfaces);

    child_frame.render_pass_list[0]
        ->shared_quad_state_list.front()
        ->mask_filter_info = gfx::MaskFilterInfo(gfx::RRectF(0, 0, 100, 10, 5));

    child_sink_->SubmitCompositorFrame(child_local_surface_id,
                                       std::move(child_frame));
  }

  // Root surface.
  std::vector<Quad> surface_quads = {
      Quad::SurfaceQuad(SurfaceRange(base::nullopt, child_surface_id),
                        SK_ColorWHITE, gfx::Rect(5, 5), false)};
  std::vector<Pass> root_passes = {Pass(surface_quads, SurfaceSize())};

  CompositorFrame root_frame = MakeEmptyCompositorFrame();
  AddPasses(&root_frame.render_pass_list, root_passes,
            &root_frame.metadata.referenced_surfaces);

  root_frame.render_pass_list[0]
      ->shared_quad_state_list.front()
      ->quad_to_target_transform.Translate(0, 7);
  root_sink_->SubmitCompositorFrame(root_local_surface_id_,
                                    std::move(root_frame));

  SurfaceId root_surface_id(root_sink_->frame_sink_id(),
                            root_local_surface_id_);

  auto aggregated_frame = AggregateFrame(root_surface_id);
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
      nullptr, &manager_, kArbitraryMiddleFrameSinkId, kChildIsRoot);
  // Grandchild surface.
  ParentLocalSurfaceIdAllocator child_allocator;
  child_allocator.GenerateId();

  LocalSurfaceId grandchild_local_surface_id =
      child_allocator.GetCurrentLocalSurfaceId();
  SurfaceId grandchild_surface_id(child_sink_->frame_sink_id(),
                                  grandchild_local_surface_id);
  {
    std::vector<Quad> child_quads[1] = {
        {Quad::SolidColorQuad(SK_ColorGREEN, gfx::Rect(5, 5))},
    };
    std::vector<Pass> child_passes = {
        Pass(child_quads[0], CompositorRenderPassId{1}, SurfaceSize())};

    CompositorFrame child_frame = MakeEmptyCompositorFrame();
    AddPasses(&child_frame.render_pass_list, child_passes,
              &child_frame.metadata.referenced_surfaces);

    child_sink_->SubmitCompositorFrame(grandchild_local_surface_id,
                                       std::move(child_frame));
  }

  // Child surface.
  child_allocator.GenerateId();
  LocalSurfaceId child_local_surface_id =
      child_allocator.GetCurrentLocalSurfaceId();
  SurfaceId child_surface_id(child_sink_->frame_sink_id(),
                             child_local_surface_id);
  {
    // Set an opacity in order to prevent merging into the root render pass.
    std::vector<Quad> child_quads = {Quad::SurfaceQuad(
        SurfaceRange(base::nullopt, grandchild_surface_id), SK_ColorWHITE,
        gfx::Rect(5, 5), 0.5f, gfx::Transform(), false,
        gfx::MaskFilterInfo(gfx::RRectF(0, 0, 96, 10, 5)),
        /*is_fast_rounded_corner=*/false)};

    std::vector<Pass> child_passes = {
        Pass(child_quads, CompositorRenderPassId{1}, SurfaceSize())};

    CompositorFrame child_frame = MakeEmptyCompositorFrame();
    AddPasses(&child_frame.render_pass_list, child_passes,
              &child_frame.metadata.referenced_surfaces);

    child_sink_->SubmitCompositorFrame(child_local_surface_id,
                                       std::move(child_frame));
  }

  // Root surface.
  gfx::Transform surface_transform;
  surface_transform.Translate(3, 4);
  std::vector<Quad> secondary_quads = {Quad::SurfaceQuad(
      SurfaceRange(base::nullopt, child_surface_id), SK_ColorWHITE,
      gfx::Rect(5, 5), 1.f, surface_transform, false, gfx::MaskFilterInfo(),
      /*is_fast_rounded_corner=*/false)};

  std::vector<Pass> root_passes = {Pass(secondary_quads, SurfaceSize())};

  CompositorFrame root_frame =
      CompositorFrameBuilder().SetDeviceScaleFactor(2.0f).Build();
  AddPasses(&root_frame.render_pass_list, root_passes,
            &root_frame.metadata.referenced_surfaces);

  root_frame.render_pass_list[0]
      ->shared_quad_state_list.front()
      ->quad_to_target_transform.Translate(0, 7);
  root_sink_->SubmitCompositorFrame(root_local_surface_id_,
                                    std::move(root_frame));

  SurfaceId root_surface_id(root_sink_->frame_sink_id(),
                            root_local_surface_id_);

  auto aggregated_frame = AggregateFrame(root_surface_id);
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
      nullptr, &manager_, kArbitraryMiddleFrameSinkId, kChildIsRoot);
  // Grandchild surface.
  ParentLocalSurfaceIdAllocator child_allocator;
  child_allocator.GenerateId();

  LocalSurfaceId grandchild_local_surface_id =
      child_allocator.GetCurrentLocalSurfaceId();
  SurfaceId grandchild_surface_id(child_sink_->frame_sink_id(),
                                  grandchild_local_surface_id);
  {
    std::vector<Quad> child_quads[1] = {
        {Quad::SolidColorQuad(SK_ColorGREEN, gfx::Rect(5, 5))},
    };
    std::vector<Pass> child_passes = {
        Pass(child_quads[0], CompositorRenderPassId{1}, SurfaceSize())};

    CompositorFrame child_frame = MakeEmptyCompositorFrame();
    AddPasses(&child_frame.render_pass_list, child_passes,
              &child_frame.metadata.referenced_surfaces);

    child_sink_->SubmitCompositorFrame(grandchild_local_surface_id,
                                       std::move(child_frame));
  }

  // Child surface.
  child_allocator.GenerateId();
  LocalSurfaceId child_local_surface_id =
      child_allocator.GetCurrentLocalSurfaceId();
  SurfaceId child_surface_id(child_sink_->frame_sink_id(),
                             child_local_surface_id);
  {
    std::vector<Quad> child_quads = {Quad::SurfaceQuad(
        SurfaceRange(base::nullopt, grandchild_surface_id), SK_ColorWHITE,
        gfx::Rect(5, 5), 1.f, gfx::Transform(), false,
        gfx::MaskFilterInfo(gfx::RRectF(0, 0, 96, 10, 5)),
        /*is_fast_rounded_corner=*/false)};

    std::vector<Pass> child_passes = {
        Pass(child_quads, CompositorRenderPassId{1}, SurfaceSize())};

    CompositorFrame child_frame = MakeEmptyCompositorFrame();
    AddPasses(&child_frame.render_pass_list, child_passes,
              &child_frame.metadata.referenced_surfaces);

    child_sink_->SubmitCompositorFrame(child_local_surface_id,
                                       std::move(child_frame));
  }

  // Root surface.
  gfx::Transform surface_transform;
  surface_transform.Translate(3, 4);
  std::vector<Quad> secondary_quads = {Quad::SurfaceQuad(
      SurfaceRange(base::nullopt, child_surface_id), SK_ColorWHITE,
      gfx::Rect(5, 5), 1.f, surface_transform, false, gfx::MaskFilterInfo(),
      /*is_fast_rounded_corner=*/false)};

  std::vector<Pass> root_passes = {Pass(secondary_quads, SurfaceSize())};

  CompositorFrame root_frame =
      CompositorFrameBuilder().SetDeviceScaleFactor(2.0f).Build();
  AddPasses(&root_frame.render_pass_list, root_passes,
            &root_frame.metadata.referenced_surfaces);

  root_frame.render_pass_list[0]
      ->shared_quad_state_list.front()
      ->quad_to_target_transform.Translate(0, 7);
  root_sink_->SubmitCompositorFrame(root_local_surface_id_,
                                    std::move(root_frame));

  SurfaceId root_surface_id(root_sink_->frame_sink_id(),
                            root_local_surface_id_);

  auto aggregated_frame = AggregateFrame(root_surface_id);
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
      nullptr, &manager_, kArbitraryMiddleFrameSinkId, kChildIsRoot);
  // Child surface.
  ParentLocalSurfaceIdAllocator child_allocator;
  child_allocator.GenerateId();

  // Child surface.
  child_allocator.GenerateId();
  LocalSurfaceId child_local_surface_id =
      child_allocator.GetCurrentLocalSurfaceId();
  SurfaceId child_surface_id(child_sink_->frame_sink_id(),
                             child_local_surface_id);
  {
    std::vector<Quad> child_quads[1] = {
        {Quad::SolidColorQuad(SK_ColorGREEN, gfx::Rect(5, 5))},
    };
    std::vector<Pass> child_passes = {
        Pass(child_quads[0], CompositorRenderPassId{1}, SurfaceSize())};

    CompositorFrame child_frame = MakeEmptyCompositorFrame();
    AddPasses(&child_frame.render_pass_list, child_passes,
              &child_frame.metadata.referenced_surfaces);

    child_sink_->SubmitCompositorFrame(child_local_surface_id,
                                       std::move(child_frame));
  }

  // Root surface.
  gfx::Transform surface_transform;
  surface_transform.Translate(3, 4);
  std::vector<Quad> secondary_quads = {Quad::SurfaceQuad(
      SurfaceRange(base::nullopt, child_surface_id), SK_ColorWHITE,
      gfx::Rect(5, 5), 1.f, surface_transform, false,
      gfx::MaskFilterInfo(gfx::RRectF(0, 0, 96, 10, 5)),
      /*is_fast_rounded_corner=*/true)};

  std::vector<Pass> root_passes = {Pass(secondary_quads, SurfaceSize())};

  CompositorFrame root_frame =
      CompositorFrameBuilder().SetDeviceScaleFactor(2.0f).Build();
  AddPasses(&root_frame.render_pass_list, root_passes,
            &root_frame.metadata.referenced_surfaces);

  root_frame.render_pass_list[0]
      ->shared_quad_state_list.front()
      ->quad_to_target_transform.Translate(0, 7);
  root_sink_->SubmitCompositorFrame(root_local_surface_id_,
                                    std::move(root_frame));

  SurfaceId root_surface_id(root_sink_->frame_sink_id(),
                            root_local_surface_id_);

  auto aggregated_frame = AggregateFrame(root_surface_id);
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
      Quad::SolidColorQuad(SK_ColorGREEN, gfx::Rect(5, 5))};
  std::vector<Pass> child_passes = {Pass(child_quads, SurfaceSize())};

  CompositorFrame child_frame = MakeEmptyCompositorFrame();
  AddPasses(&child_frame.render_pass_list, child_passes,
            &child_frame.metadata.referenced_surfaces);

  ParentLocalSurfaceIdAllocator child_allocator;
  child_allocator.GenerateId();
  LocalSurfaceId child_local_surface_id =
      child_allocator.GetCurrentLocalSurfaceId();
  SurfaceId child_surface_id(child_sink_->frame_sink_id(),
                             child_local_surface_id);
  child_sink_->SubmitCompositorFrame(child_local_surface_id,
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
  gfx::Transform translation;
  translation.Translate(30.f, 50.f);
  std::vector<Quad> root_quads[] = {
      {Quad::SurfaceQuad(SurfaceRange(base::nullopt, child_surface_id),
                         SK_ColorWHITE, gfx::Rect(5, 5),
                         /*stretch_content_to_fill_bounds=*/false,
                         AllowMerge())},
      {Quad::RenderPassQuad(CompositorRenderPassId{1}, scale, true)},
      {Quad::RenderPassQuad(CompositorRenderPassId{2}, translation, true),
       Quad::RenderPassQuad(CompositorRenderPassId{1}, gfx::Transform(),
                            true)}};
  std::vector<Pass> root_passes = {
      Pass(root_quads[0], CompositorRenderPassId{1}, SurfaceSize()),
      Pass(root_quads[1], CompositorRenderPassId{2}, SurfaceSize()),
      Pass(root_quads[2], CompositorRenderPassId{3}, SurfaceSize())};

  CompositorFrame root_frame = MakeEmptyCompositorFrame();
  AddPasses(&root_frame.render_pass_list, root_passes,
            &root_frame.metadata.referenced_surfaces);

  root_sink_->SubmitCompositorFrame(root_local_surface_id_,
                                    std::move(root_frame));

  SurfaceId root_surface_id(root_sink_->frame_sink_id(),
                            root_local_surface_id_);

  // Damage rect for the first aggregation would contain entire root surface
  // which is union of (0,0 100x100) and (30,50 200x200); i.e. (0,0 230x250).
  EXPECT_CALL(
      aggregated_damage_callback,
      OnAggregatedDamage(root_local_surface_id_, SurfaceSize(),
                         gfx::Rect(0, 0, 230, 250), next_display_time()));
  auto aggregated_frame = AggregateFrame(root_surface_id);
  testing::Mock::VerifyAndClearExpectations(&aggregated_damage_callback);

  // For the second aggregation we only damage the child surface at
  // (10,10 10x10). The aggregated damage rect should reflect that.
  CompositorFrame child_frame_2 = MakeEmptyCompositorFrame();
  AddPasses(&child_frame_2.render_pass_list, child_passes,
            &child_frame_2.metadata.referenced_surfaces);

  child_frame_2.render_pass_list.back()->damage_rect =
      gfx::Rect(10, 10, 10, 10);

  child_sink_->SubmitCompositorFrame(child_local_surface_id,
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
  EXPECT_CALL(aggregated_damage_callback,
              OnAggregatedDamage(root_local_surface_id_, SurfaceSize(),
                                 expected_damage_rect, next_display_time()));
  auto aggregated_frame_2 = AggregateFrame(root_surface_id);
  testing::Mock::VerifyAndClearExpectations(&aggregated_damage_callback);
}

// Verifies that if a child surface is embedded in the root surface inside a
// render pass cycle, only the first embedding of the child surface is
// considered in the damage rect and its repeated embeddings are ignored.
TEST_F(SurfaceAggregatorValidSurfaceTest,
       AggregateDamageRectWithRenderPassCycle) {
  // Add a callback for when the surface is damaged.
  MockAggregatedDamageCallback aggregated_damage_callback;
  root_sink_->SetAggregatedDamageCallbackForTesting(
      aggregated_damage_callback.GetCallback());

  // The child surface consists of a single render pass containing a single
  // solid color draw quad.
  std::vector<Quad> child_quads = {
      Quad::SolidColorQuad(SK_ColorGREEN, gfx::Rect(5, 5))};
  std::vector<Pass> child_passes = {Pass(child_quads, SurfaceSize())};

  CompositorFrame child_frame = MakeEmptyCompositorFrame();
  AddPasses(&child_frame.render_pass_list, child_passes,
            &child_frame.metadata.referenced_surfaces);

  ParentLocalSurfaceIdAllocator child_allocator;
  child_allocator.GenerateId();
  LocalSurfaceId child_local_surface_id =
      child_allocator.GetCurrentLocalSurfaceId();
  SurfaceId child_surface_id(child_sink_->frame_sink_id(),
                             child_local_surface_id);
  child_sink_->SubmitCompositorFrame(child_local_surface_id,
                                     std::move(child_frame));

  // The root surface consists of two render passes:
  //  1) The first render pass contains a surface draw quad referencing the
  //     child surface and a render pass draw quad referencing the second
  //     render pass.
  //  2) The second render pass contains a render pass draw quad with a
  //     transform applied that is referencing the first render pass,
  //     creating a cycle.
  CompositorRenderPassId root_pass_ids[] = {CompositorRenderPassId{1},
                                            CompositorRenderPassId{2}};
  std::vector<Quad> root_quads_1 = {
      Quad::SurfaceQuad(SurfaceRange(base::nullopt, child_surface_id),
                        SK_ColorWHITE, gfx::Rect(5, 5),
                        /*stretch_content_to_fill_bounds=*/false),
      Quad::RenderPassQuad(root_pass_ids[1], gfx::Transform(), true)};
  std::vector<Quad> root_quads_2 = {
      Quad::RenderPassQuad(root_pass_ids[0], gfx::Transform(), true)};
  std::vector<Pass> root_passes = {
      Pass(root_quads_2, root_pass_ids[1], SurfaceSize()),
      Pass(root_quads_1, root_pass_ids[0], SurfaceSize())};

  CompositorFrame root_frame = MakeEmptyCompositorFrame();
  AddPasses(&root_frame.render_pass_list, root_passes,
            &root_frame.metadata.referenced_surfaces);

  auto& rpdq_2_transform = root_frame.render_pass_list.front()
                               ->shared_quad_state_list.back()
                               ->quad_to_target_transform;
  rpdq_2_transform.Translate(30.f, 50.f);
  rpdq_2_transform.Scale(2.f, 2.f);

  root_sink_->SubmitCompositorFrame(root_local_surface_id_,
                                    std::move(root_frame));

  SurfaceId root_surface_id(root_sink_->frame_sink_id(),
                            root_local_surface_id_);

  // Damage rect for the first aggregation would contain entire root surface
  // which is just (0,0 100x100). The child surface is only embedded once
  // and without any transform, since repeated embeddings caused by the
  // render pass cycle are ignored.
  EXPECT_CALL(
      aggregated_damage_callback,
      OnAggregatedDamage(root_local_surface_id_, SurfaceSize(),
                         gfx::Rect(0, 0, 100, 100), next_display_time()));
  auto aggregated_frame = AggregateFrame(root_surface_id);
  testing::Mock::VerifyAndClearExpectations(&aggregated_damage_callback);

  // For the second aggregation we only damage the child surface at
  // (10,10 10x10). The aggregated damage rect should reflect that only for
  // the first embedding.
  CompositorFrame child_frame_2 = MakeEmptyCompositorFrame();
  AddPasses(&child_frame_2.render_pass_list, child_passes,
            &child_frame_2.metadata.referenced_surfaces);

  child_frame_2.render_pass_list.back()->damage_rect =
      gfx::Rect(10, 10, 10, 10);

  child_sink_->SubmitCompositorFrame(child_local_surface_id,
                                     std::move(child_frame_2));

  gfx::Rect expected_damage_rect(10, 10, 10, 10);
  EXPECT_CALL(aggregated_damage_callback,
              OnAggregatedDamage(root_local_surface_id_, SurfaceSize(),
                                 expected_damage_rect, next_display_time()));
  auto aggregated_frame_2 = AggregateFrame(root_surface_id);
  testing::Mock::VerifyAndClearExpectations(&aggregated_damage_callback);
}

// Verify that a SurfaceDrawQuad with !|allow_merge| won't be merged into
// the parent renderpass.
TEST_F(SurfaceAggregatorValidSurfaceTest, AllowMerge) {
  // Child surface.
  gfx::Rect child_rect(5, 5);
  ParentLocalSurfaceIdAllocator child_allocator;
  child_allocator.GenerateId();

  LocalSurfaceId child_local_surface_id =
      child_allocator.GetCurrentLocalSurfaceId();
  SurfaceId child_surface_id(child_sink_->frame_sink_id(),
                             child_local_surface_id);
  {
    std::vector<Quad> child_quads = {
        Quad::SolidColorQuad(SK_ColorGREEN, child_rect)};
    std::vector<Pass> child_passes = {
        Pass(child_quads, CompositorRenderPassId{1}, SurfaceSize())};

    CompositorFrame child_frame = MakeEmptyCompositorFrame();
    AddPasses(&child_frame.render_pass_list, child_passes,
              &child_frame.metadata.referenced_surfaces);

    child_sink_->SubmitCompositorFrame(child_local_surface_id,
                                       std::move(child_frame));
  }

  gfx::Rect root_rect(SurfaceSize());

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
                         SurfaceRange(base::nullopt, child_surface_id),
                         SK_ColorWHITE,
                         /*stretch_content_to_fill_bounds=*/false,
                         /*is_reflection=*/false,
                         /*allow_merge=*/true);

    CompositorFrame frame =
        CompositorFrameBuilder().AddRenderPass(std::move(pass)).Build();
    root_sink_->SubmitCompositorFrame(root_local_surface_id_, std::move(frame));

    SurfaceId root_surface_id(root_sink_->frame_sink_id(),
                              root_local_surface_id_);

    auto aggregated_frame = AggregateFrame(root_surface_id);
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
                         SurfaceRange(base::nullopt, child_surface_id),
                         SK_ColorWHITE,
                         /*stretch_content_to_fill_bounds=*/false,
                         /*is_reflection=*/false,
                         /*allow_merge=*/false);

    CompositorFrame frame =
        CompositorFrameBuilder().AddRenderPass(std::move(pass)).Build();
    root_sink_->SubmitCompositorFrame(root_local_surface_id_, std::move(frame));

    SurfaceId root_surface_id(root_sink_->frame_sink_id(),
                              root_local_surface_id_);

    auto aggregated_frame = AggregateFrame(root_surface_id);
    // Merging not allowed, so 2 passes should be present.
    EXPECT_EQ(2u, aggregated_frame.render_pass_list.size());
  }
}

// Check that if a non-merged surface is invisible, its entire render pass is
// skipped.
TEST_F(SurfaceAggregatorValidSurfaceTest, SkipInvisibleSurface) {
  // Child surface.
  gfx::Rect child_rect(5, 5);
  ParentLocalSurfaceIdAllocator child_allocator;
  child_allocator.GenerateId();

  LocalSurfaceId child_local_surface_id =
      child_allocator.GetCurrentLocalSurfaceId();
  SurfaceId child_surface_id(child_sink_->frame_sink_id(),
                             child_local_surface_id);
  {
    std::vector<Quad> child_quads = {
        Quad::SolidColorQuad(SK_ColorGREEN, child_rect)};
    // Offset child output rect so it's outside the root visible rect.
    gfx::Rect output_rect(SurfaceSize());
    output_rect.Offset(output_rect.width(), output_rect.height());
    std::vector<Pass> child_passes = {
        Pass(child_quads, CompositorRenderPassId{1}, output_rect)};

    CompositorFrame child_frame = MakeEmptyCompositorFrame();
    AddPasses(&child_frame.render_pass_list, child_passes,
              &child_frame.metadata.referenced_surfaces);

    child_sink_->SubmitCompositorFrame(child_local_surface_id,
                                       std::move(child_frame));
  }

  gfx::Rect root_rect(SurfaceSize());

  auto pass = CompositorRenderPass::Create();
  pass->SetNew(CompositorRenderPassId{1}, root_rect, root_rect,
               gfx::Transform());
  auto* sqs = pass->CreateAndAppendSharedQuadState();
  sqs->opacity = 1.f;

  // Disallow merge.
  auto* surface_quad = pass->CreateAndAppendDrawQuad<SurfaceDrawQuad>();
  surface_quad->SetAll(sqs, child_rect, child_rect,
                       /*needs_blending=*/false,
                       SurfaceRange(base::nullopt, child_surface_id),
                       SK_ColorWHITE,
                       /*stretch_content_to_fill_bounds=*/false,
                       /*is_reflection=*/false,
                       /*allow_merge=*/false);

  CompositorFrame frame =
      CompositorFrameBuilder().AddRenderPass(std::move(pass)).Build();
  root_sink_->SubmitCompositorFrame(root_local_surface_id_, std::move(frame));

  SurfaceId root_surface_id(root_sink_->frame_sink_id(),
                            root_local_surface_id_);

  auto aggregated_frame = AggregateFrame(root_surface_id);
  // Merging not allowed, but child rect should be dropped.
  EXPECT_EQ(1u, aggregated_frame.render_pass_list.size());
}

// Verify that a SurfaceDrawQuad's root RenderPass has correct texture
// parameters if being drawn via RPDQ.
TEST_F(SurfaceAggregatorValidSurfaceTest, RenderPassDoesNotFillSurface) {
  // Child surface.
  gfx::Rect child_rect(5, 4, 5, 5);
  ParentLocalSurfaceIdAllocator child_allocator;
  child_allocator.GenerateId();

  LocalSurfaceId child_local_surface_id =
      child_allocator.GetCurrentLocalSurfaceId();
  SurfaceId child_surface_id(child_sink_->frame_sink_id(),
                             child_local_surface_id);
  {
    std::vector<Quad> child_quads = {
        Quad::SolidColorQuad(SK_ColorGREEN, child_rect)};
    std::vector<Pass> child_passes = {
        Pass(child_quads, CompositorRenderPassId{1}, child_rect)};

    CompositorFrame child_frame = MakeEmptyCompositorFrame();
    AddPasses(&child_frame.render_pass_list, child_passes,
              &child_frame.metadata.referenced_surfaces);

    child_sink_->SubmitCompositorFrame(child_local_surface_id,
                                       std::move(child_frame));
  }

  gfx::Rect root_rect(SurfaceSize());
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
                         SurfaceRange(base::nullopt, child_surface_id),
                         SK_ColorWHITE,
                         /*stretch_content_to_fill_bounds=*/false,
                         /*is_reflection=*/false,
                         /*allow_merge=*/false);

    CompositorFrame frame =
        CompositorFrameBuilder().AddRenderPass(std::move(pass)).Build();
    root_sink_->SubmitCompositorFrame(root_local_surface_id_, std::move(frame));

    SurfaceId root_surface_id(root_sink_->frame_sink_id(),
                              root_local_surface_id_);

    auto aggregated_frame = AggregateFrame(root_surface_id);

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
      Quad::SolidColorQuad(SK_ColorGREEN, gfx::Rect(5, 5))};

  std::vector<Pass> child_passes = {
      Pass(child_quads, CompositorRenderPassId{1}, child_surface_size)};

  CompositorFrame child_frame = MakeEmptyCompositorFrame();
  AddPasses(&child_frame.render_pass_list, child_passes,
            &child_frame.metadata.referenced_surfaces);

  ParentLocalSurfaceIdAllocator child_allocator;
  child_allocator.GenerateId();
  LocalSurfaceId child_local_surface_id =
      child_allocator.GetCurrentLocalSurfaceId();
  SurfaceId child_surface_id(child_sink_->frame_sink_id(),
                             child_local_surface_id);
  child_sink_->SubmitCompositorFrame(child_local_surface_id,
                                     std::move(child_frame));

  std::vector<Quad> solid_color_quad = {
      Quad::SolidColorQuad(SK_ColorGREEN, gfx::Rect(30, 30))};

  std::vector<Quad> render_pass_draw_quads = {
      Quad::RenderPassQuad(CompositorRenderPassId{1}, gfx::Transform(), true),
      Quad::RenderPassQuad(CompositorRenderPassId{2}, gfx::Transform(), true),
      Quad::RenderPassQuad(CompositorRenderPassId{3}, gfx::Transform(), true),
      Quad::RenderPassQuad(CompositorRenderPassId{4}, gfx::Transform(), true),
      Quad::SurfaceQuad(SurfaceRange(base::nullopt, child_surface_id),
                        SK_ColorWHITE, gfx::Rect(child_surface_size),
                        /*stretch_content_to_fill_bounds=*/false,
                        AllowMerge())};

  std::vector<Pass> root_passes = {
      Pass(solid_color_quad, CompositorRenderPassId{1}, gfx::Size(30, 30)),
      Pass(solid_color_quad, CompositorRenderPassId{2}, gfx::Size(30, 30)),
      Pass(solid_color_quad, CompositorRenderPassId{3}, gfx::Size(30, 30)),
      Pass(solid_color_quad, CompositorRenderPassId{4}, gfx::Size(30, 30)),
      Pass(render_pass_draw_quads, CompositorRenderPassId{5},
           gfx::Size(100, 100))};

  CompositorFrame root_frame = MakeEmptyCompositorFrame();
  AddPasses(&root_frame.render_pass_list, root_passes,
            &root_frame.metadata.referenced_surfaces);

  root_frame.render_pass_list[4]
      ->shared_quad_state_list.ElementAt(0)
      ->quad_to_target_transform.Translate(70, 0);
  root_frame.render_pass_list[4]
      ->shared_quad_state_list.ElementAt(1)
      ->quad_to_target_transform.Translate(30, 30);
  root_frame.render_pass_list[4]
      ->shared_quad_state_list.ElementAt(2)
      ->quad_to_target_transform.Translate(10, 50);
  root_frame.render_pass_list[4]
      ->shared_quad_state_list.ElementAt(3)
      ->quad_to_target_transform.Translate(70, 70);
  root_frame.render_pass_list[4]
      ->shared_quad_state_list.ElementAt(4)
      ->quad_to_target_transform.Translate(0, 85);

  // Add backdrop blur filter to all render passes.
  root_frame.render_pass_list[0]->backdrop_filters.Append(
      cc::FilterOperation::CreateBlurFilter(5));
  root_frame.render_pass_list[1]->backdrop_filters.Append(
      cc::FilterOperation::CreateBlurFilter(5));
  root_frame.render_pass_list[2]->backdrop_filters.Append(
      cc::FilterOperation::CreateBlurFilter(5));
  root_frame.render_pass_list[3]->backdrop_filters.Append(
      cc::FilterOperation::CreateBlurFilter(5));

  root_sink_->SubmitCompositorFrame(root_local_surface_id_,
                                    std::move(root_frame));

  // Damage rect for first aggregation should contain entire root surface.
  SurfaceId root_surface_id(root_sink_->frame_sink_id(),
                            root_local_surface_id_);
  size_t expected_num_passes_after_aggregation = AllowMerge() ? 5u : 6u;
  EXPECT_CALL(
      aggregated_damage_callback,
      OnAggregatedDamage(child_local_surface_id, child_surface_size,
                         gfx::Rect(child_surface_size), next_display_time()));
  EXPECT_CALL(
      aggregated_damage_callback,
      OnAggregatedDamage(root_local_surface_id_, SurfaceSize(),
                         gfx::Rect(0, 0, 100, 100), next_display_time()));
  auto aggregated_frame = AggregateFrame(root_surface_id);
  testing::Mock::VerifyAndClearExpectations(&aggregated_damage_callback);
  const auto& aggregated_pass_list = aggregated_frame.render_pass_list;
  EXPECT_EQ(expected_num_passes_after_aggregation, aggregated_pass_list.size());
  EXPECT_EQ(gfx::Rect(SurfaceSize()), aggregated_pass_list.back()->damage_rect);

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

  child_sink_->SubmitCompositorFrame(child_local_surface_id,
                                     std::move(child_frame));

  // The damage from the surface quad (0,85 100x15) is below all the four quads
  // with backdrop filters.
  // The expected damage rect should include all the other child render pass
  // output surface that would need to be updated. In this case, that would
  // be the bottom 3 render pass from the image.
  const gfx::Rect expected_damage_rect(0, 30, 100, 70);

  EXPECT_CALL(
      aggregated_damage_callback,
      OnAggregatedDamage(child_local_surface_id, child_surface_size,
                         gfx::Rect(child_surface_size), next_display_time()));
  EXPECT_CALL(aggregated_damage_callback,
              OnAggregatedDamage(root_local_surface_id_, SurfaceSize(),
                                 expected_damage_rect, next_display_time()));
  aggregated_frame = AggregateFrame(root_surface_id);
  testing::Mock::VerifyAndClearExpectations(&aggregated_damage_callback);

  const auto& aggregated_pass_list2 = aggregated_frame.render_pass_list;
  EXPECT_EQ(expected_num_passes_after_aggregation,
            aggregated_pass_list2.size());
  EXPECT_EQ(expected_damage_rect, aggregated_pass_list2.back()->damage_rect);
}

TEST_F(SurfaceAggregatorValidSurfaceTest,
       ContainedFrameSinkChangeInvalidatesHitTestData) {
  auto embedded_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryFrameSinkId1, kRootIsRoot);
  ParentLocalSurfaceIdAllocator embedded_allocator;
  embedded_allocator.GenerateId();
  LocalSurfaceId embedded_local_surface_id =
      embedded_allocator.GetCurrentLocalSurfaceId();
  SurfaceId embedded_surface_id(embedded_support->frame_sink_id(),
                                embedded_local_surface_id);
  SurfaceId root_surface_id(root_sink_->frame_sink_id(),
                            root_local_surface_id_);

  // First submit a root frame which doesn't reference the embedded frame
  // and aggregate.
  {
    std::vector<Quad> embedded_quads = {
        Quad::SolidColorQuad(SK_ColorGREEN, gfx::Rect(5, 5)),
        Quad::SolidColorQuad(SK_ColorGRAY, gfx::Rect(5, 5))};
    std::vector<Pass> embedded_passes = {Pass(embedded_quads, SurfaceSize())};
    SubmitCompositorFrame(embedded_support.get(), embedded_passes,
                          embedded_local_surface_id, 1.0f);

    std::vector<Quad> root_quads = {
        Quad::SolidColorQuad(SK_ColorRED, gfx::Rect(5, 5)),
        Quad::SolidColorQuad(SK_ColorBLUE, gfx::Rect(5, 5))};
    std::vector<Pass> root_passes = {Pass(root_quads, SurfaceSize())};
    SubmitCompositorFrame(root_sink_.get(), root_passes, root_local_surface_id_,
                          1.0f);
    AggregateFrame(root_surface_id);
  }

  const HitTestManager* hit_test_manager = manager_.hit_test_manager();
  uint64_t hit_test_region_index =
      hit_test_manager->submit_hit_test_region_list_index();

  // Now submit a root frame that *does* reference the embedded frame, and
  // aggregate.
  {
    std::vector<Quad> root_quads = {
        Quad::SurfaceQuad(SurfaceRange(base::nullopt, embedded_surface_id),
                          SK_ColorWHITE, gfx::Rect(5, 5), false),
        Quad::SolidColorQuad(SK_ColorRED, gfx::Rect(5, 5)),
        Quad::SolidColorQuad(SK_ColorBLUE, gfx::Rect(5, 5))};
    std::vector<Pass> root_passes = {Pass(root_quads, SurfaceSize())};

    SubmitCompositorFrame(root_sink_.get(), root_passes, root_local_surface_id_,
                          1.0);
    AggregateFrame(root_surface_id);
  }

  // Check that the HitTestManager was marked as needing to re-aggregate hit
  // test data.
  EXPECT_GT(hit_test_manager->submit_hit_test_region_list_index(),
            hit_test_region_index);
}

void ExpectDelegatedInkMetadataIsEqual(const DelegatedInkMetadata& lhs,
                                       const DelegatedInkMetadata& rhs) {
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
      Quad::SolidColorQuad(SK_ColorGREEN, gfx::Rect(5, 5))};
  std::vector<Pass> child_passes = {
      Pass(child_quads, CompositorRenderPassId{1}, gfx::Size(100, 100))};

  CompositorFrame child_frame = MakeEmptyCompositorFrame();
  DelegatedInkMetadata metadata(
      gfx::PointF(100, 100), 1.5, SK_ColorRED, base::TimeTicks::Now(),
      gfx::RectF(10, 10, 200, 200), base::TimeTicks::Now(), /*hovering*/ true);
  child_frame.metadata.delegated_ink_metadata =
      std::make_unique<DelegatedInkMetadata>(metadata);
  AddPasses(&child_frame.render_pass_list, child_passes,
            &child_frame.metadata.referenced_surfaces);

  ParentLocalSurfaceIdAllocator child_allocator;
  child_allocator.GenerateId();
  LocalSurfaceId child_local_surface_id =
      child_allocator.GetCurrentLocalSurfaceId();
  SurfaceId child_surface_id(child_sink_->frame_sink_id(),
                             child_local_surface_id);
  child_sink_->SubmitCompositorFrame(child_local_surface_id,
                                     std::move(child_frame));

  std::vector<Quad> root_quads = {Quad::SurfaceQuad(
      SurfaceRange(base::nullopt, child_surface_id), SK_ColorWHITE,
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
  gfx::PointF pt = metadata.point();
  gfx::RectF area = metadata.presentation_area();
  root_frame.render_pass_list[0]
      ->shared_quad_state_list.ElementAt(0)
      ->quad_to_target_transform.TransformPoint(&pt);
  root_frame.render_pass_list[0]
      ->shared_quad_state_list.ElementAt(0)
      ->quad_to_target_transform.TransformRect(&area);
  metadata = DelegatedInkMetadata(
      pt, metadata.diameter(), metadata.color(), metadata.timestamp(), area,
      metadata.frame_time(), metadata.is_hovering());

  root_sink_->SubmitCompositorFrame(root_local_surface_id_,
                                    std::move(root_frame));

  SurfaceId root_surface_id(root_sink_->frame_sink_id(),
                            root_local_surface_id_);
  auto aggregated_frame = AggregateFrame(root_surface_id);

  std::unique_ptr<DelegatedInkMetadata> actual_metadata =
      std::move(aggregated_frame.delegated_ink_metadata);
  EXPECT_TRUE(actual_metadata);
  ExpectDelegatedInkMetadataIsEqual(*actual_metadata.get(), metadata);

  // Then confirm that the |delegated_ink_metadata| was reset and a new
  // aggregated frame does not contain any delegated ink metadata.
  auto new_aggregated_frame = AggregateFrame(root_surface_id);
  EXPECT_FALSE(new_aggregated_frame.delegated_ink_metadata);
}

// Confirm that transforms are aggregated as the tree is walked and correctly
// applied to the ink metadata.
TEST_F(SurfaceAggregatorValidSurfaceTest,
       TransformDelegatedInkMetadataTallTree) {
  auto greatgrand_child_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryFrameSinkId2, kChildIsRoot);
  std::vector<Quad> greatgrandchild_quads = {
      Quad::SolidColorQuad(SK_ColorGREEN, gfx::Rect(5, 5))};
  std::vector<Pass> greatgrandchild_passes = {Pass(
      greatgrandchild_quads, CompositorRenderPassId{1}, gfx::Size(100, 100))};

  DelegatedInkMetadata metadata(
      gfx::PointF(100, 100), 1.5, SK_ColorRED, base::TimeTicks::Now(),
      gfx::RectF(10, 10, 200, 200), base::TimeTicks::Now(), /*hovering*/ false);
  CompositorFrame greatgrandchild_frame = MakeEmptyCompositorFrame();
  greatgrandchild_frame.metadata.delegated_ink_metadata =
      std::make_unique<DelegatedInkMetadata>(metadata);
  AddPasses(&greatgrandchild_frame.render_pass_list, greatgrandchild_passes,
            &greatgrandchild_frame.metadata.referenced_surfaces);

  ParentLocalSurfaceIdAllocator greatgrandchild_allocator;
  greatgrandchild_allocator.GenerateId();
  LocalSurfaceId greatgrandchild_local_surface_id =
      greatgrandchild_allocator.GetCurrentLocalSurfaceId();
  SurfaceId great_grandchild_surface_id(
      greatgrand_child_support->frame_sink_id(),
      greatgrandchild_local_surface_id);
  greatgrand_child_support->SubmitCompositorFrame(
      greatgrandchild_local_surface_id, std::move(greatgrandchild_frame));

  auto grand_child_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryMiddleFrameSinkId, kChildIsRoot);
  std::vector<Quad> grandchild_quads = {Quad::SurfaceQuad(
      SurfaceRange(base::nullopt, great_grandchild_surface_id), SK_ColorWHITE,
      gfx::Rect(7, 7), /*stretch_content_to_fill_bounds=*/false)};
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
  gfx::PointF pt = metadata.point();
  gfx::RectF area = metadata.presentation_area();
  grandchild_frame.render_pass_list[0]
      ->shared_quad_state_list.ElementAt(0)
      ->quad_to_target_transform.TransformPoint(&pt);
  grandchild_frame.render_pass_list[0]
      ->shared_quad_state_list.ElementAt(0)
      ->quad_to_target_transform.TransformRect(&area);

  ParentLocalSurfaceIdAllocator grandchild_allocator;
  grandchild_allocator.GenerateId();
  LocalSurfaceId grandchild_local_surface_id =
      grandchild_allocator.GetCurrentLocalSurfaceId();
  SurfaceId grandchild_surface_id(grand_child_support->frame_sink_id(),
                                  grandchild_local_surface_id);
  grand_child_support->SubmitCompositorFrame(grandchild_local_surface_id,
                                             std::move(grandchild_frame));

  std::vector<Quad> child_quads = {Quad::SurfaceQuad(
      SurfaceRange(base::nullopt, grandchild_surface_id), SK_ColorWHITE,
      gfx::Rect(7, 7), /*stretch_content_to_fill_bounds=*/false)};
  std::vector<Pass> child_passes = {
      Pass(child_quads, CompositorRenderPassId{1}, gfx::Size(30, 30))};

  CompositorFrame child_frame = MakeEmptyCompositorFrame();
  AddPasses(&child_frame.render_pass_list, child_passes,
            &child_frame.metadata.referenced_surfaces);

  child_frame.render_pass_list[0]
      ->shared_quad_state_list.ElementAt(0)
      ->quad_to_target_transform.Translate(36, 15);

  child_frame.render_pass_list[0]
      ->shared_quad_state_list.ElementAt(0)
      ->quad_to_target_transform.TransformPoint(&pt);
  child_frame.render_pass_list[0]
      ->shared_quad_state_list.ElementAt(0)
      ->quad_to_target_transform.TransformRect(&area);

  ParentLocalSurfaceIdAllocator child_allocator;
  child_allocator.GenerateId();
  LocalSurfaceId child_local_surface_id =
      child_allocator.GetCurrentLocalSurfaceId();
  SurfaceId child_surface_id(child_sink_->frame_sink_id(),
                             child_local_surface_id);
  child_sink_->SubmitCompositorFrame(child_local_surface_id,
                                     std::move(child_frame));

  std::vector<Quad> root_quads = {Quad::SurfaceQuad(
      SurfaceRange(base::nullopt, child_surface_id), SK_ColorWHITE,
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

  root_frame.render_pass_list[0]
      ->shared_quad_state_list.ElementAt(0)
      ->quad_to_target_transform.TransformPoint(&pt);
  root_frame.render_pass_list[0]
      ->shared_quad_state_list.ElementAt(0)
      ->quad_to_target_transform.TransformRect(&area);

  root_sink_->SubmitCompositorFrame(root_local_surface_id_,
                                    std::move(root_frame));

  SurfaceId root_surface_id(root_sink_->frame_sink_id(),
                            root_local_surface_id_);
  auto aggregated_frame = AggregateFrame(root_surface_id);

  metadata = DelegatedInkMetadata(
      pt, metadata.diameter(), metadata.color(), metadata.timestamp(), area,
      metadata.frame_time(), metadata.is_hovering());

  std::unique_ptr<DelegatedInkMetadata> actual_metadata =
      std::move(aggregated_frame.delegated_ink_metadata);
  EXPECT_TRUE(actual_metadata);
  ExpectDelegatedInkMetadataIsEqual(*actual_metadata.get(), metadata);

  // Then confirm that the |delegated_ink_metadata| was reset and a new
  // aggregated frame does not contain any delegated ink metadata.
  auto new_aggregated_frame = AggregateFrame(root_surface_id);
  EXPECT_FALSE(new_aggregated_frame.delegated_ink_metadata);
}

// Confirm the metadata is transformed correctly and makes it to the aggregated
// frame when there are multiple children.
TEST_F(SurfaceAggregatorValidSurfaceTest,
       DelegatedInkMetadataMultipleChildren) {
  auto child_2_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryMiddleFrameSinkId, kChildIsRoot);
  auto child_3_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryFrameSinkId2, kChildIsRoot);

  std::vector<Quad> child_1_quads = {
      Quad::SolidColorQuad(SK_ColorGREEN, gfx::Rect(5, 5))};
  std::vector<Pass> child_1_passes = {
      Pass(child_1_quads, CompositorRenderPassId{1}, gfx::Size(100, 100))};

  CompositorFrame child_1_frame = MakeEmptyCompositorFrame();
  AddPasses(&child_1_frame.render_pass_list, child_1_passes,
            &child_1_frame.metadata.referenced_surfaces);

  ParentLocalSurfaceIdAllocator child_1_allocator;
  child_1_allocator.GenerateId();
  LocalSurfaceId child_1_local_surface_id =
      child_1_allocator.GetCurrentLocalSurfaceId();
  SurfaceId child_1_surface_id(child_sink_->frame_sink_id(),
                               child_1_local_surface_id);
  child_sink_->SubmitCompositorFrame(child_1_local_surface_id,
                                     std::move(child_1_frame));

  std::vector<Quad> child_2_quads = {
      Quad::SolidColorQuad(SK_ColorMAGENTA, gfx::Rect(5, 5))};
  std::vector<Pass> child_2_passes = {
      Pass(child_2_quads, CompositorRenderPassId{1}, gfx::Size(100, 100))};

  DelegatedInkMetadata metadata = DelegatedInkMetadata(
      gfx::PointF(88, 34), 1.8, SK_ColorBLACK, base::TimeTicks::Now(),
      gfx::RectF(50, 50, 300, 300), base::TimeTicks::Now(), /*hovering*/ true);
  CompositorFrame child_2_frame = MakeEmptyCompositorFrame();
  child_2_frame.metadata.delegated_ink_metadata =
      std::make_unique<DelegatedInkMetadata>(metadata);
  AddPasses(&child_2_frame.render_pass_list, child_2_passes,
            &child_2_frame.metadata.referenced_surfaces);

  ParentLocalSurfaceIdAllocator child_2_allocator;
  child_2_allocator.GenerateId();
  LocalSurfaceId child_2_local_surface_id =
      child_2_allocator.GetCurrentLocalSurfaceId();
  SurfaceId child_2_surface_id(child_2_support->frame_sink_id(),
                               child_2_local_surface_id);
  child_2_support->SubmitCompositorFrame(child_2_local_surface_id,
                                         std::move(child_2_frame));

  std::vector<Quad> child_3_quads = {
      Quad::SolidColorQuad(SK_ColorCYAN, gfx::Rect(5, 5))};
  std::vector<Pass> child_3_passes = {
      Pass(child_3_quads, CompositorRenderPassId{1}, gfx::Size(100, 100))};

  CompositorFrame child_3_frame = MakeEmptyCompositorFrame();
  AddPasses(&child_3_frame.render_pass_list, child_3_passes,
            &child_3_frame.metadata.referenced_surfaces);

  ParentLocalSurfaceIdAllocator child_3_allocator;
  child_3_allocator.GenerateId();
  LocalSurfaceId child_3_local_surface_id =
      child_3_allocator.GetCurrentLocalSurfaceId();
  SurfaceId child_3_surface_id(child_3_support->frame_sink_id(),
                               child_3_local_surface_id);
  child_3_support->SubmitCompositorFrame(child_3_local_surface_id,
                                         std::move(child_3_frame));

  std::vector<Quad> root_quads = {
      Quad::SurfaceQuad(SurfaceRange(base::nullopt, child_1_surface_id),
                        SK_ColorWHITE, gfx::Rect(5, 5),
                        /*stretch_content_to_fill_bounds=*/false),
      Quad::SurfaceQuad(SurfaceRange(base::nullopt, child_2_surface_id),
                        SK_ColorWHITE, gfx::Rect(5, 5),
                        /*stretch_content_to_fill_bounds=*/false),
      Quad::SurfaceQuad(SurfaceRange(base::nullopt, child_3_surface_id),
                        SK_ColorWHITE, gfx::Rect(5, 5),
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
  gfx::PointF pt = metadata.point();
  gfx::RectF area = metadata.presentation_area();
  root_frame.render_pass_list[0]
      ->shared_quad_state_list.ElementAt(1)
      ->quad_to_target_transform.TransformPoint(&pt);
  root_frame.render_pass_list[0]
      ->shared_quad_state_list.ElementAt(1)
      ->quad_to_target_transform.TransformRect(&area);

  root_sink_->SubmitCompositorFrame(root_local_surface_id_,
                                    std::move(root_frame));

  SurfaceId root_surface_id(root_sink_->frame_sink_id(),
                            root_local_surface_id_);
  auto aggregated_frame = AggregateFrame(root_surface_id);

  metadata = DelegatedInkMetadata(
      pt, metadata.diameter(), metadata.color(), metadata.timestamp(), area,
      metadata.frame_time(), metadata.is_hovering());

  std::unique_ptr<DelegatedInkMetadata> actual_metadata =
      std::move(aggregated_frame.delegated_ink_metadata);
  EXPECT_TRUE(actual_metadata);
  ExpectDelegatedInkMetadataIsEqual(*actual_metadata.get(), metadata);

  // Then confirm that the |delegated_ink_metadata| was reset and a new
  // aggregated frame does not contain any delegated ink metadata.
  auto new_aggregated_frame = AggregateFrame(root_surface_id);
  EXPECT_FALSE(new_aggregated_frame.delegated_ink_metadata);
}

// Confirm the the metadata with the most recent timestamp is used when
// multiple children have delegated ink metadata.
TEST_F(SurfaceAggregatorValidSurfaceTest,
       MultipleChildrenHaveDelegatedInkMetadata) {
  auto child_2_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryMiddleFrameSinkId, kChildIsRoot);
  auto child_3_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryFrameSinkId2, kChildIsRoot);

  std::vector<Quad> child_1_quads = {
      Quad::SolidColorQuad(SK_ColorGREEN, gfx::Rect(5, 5))};
  std::vector<Pass> child_1_passes = {
      Pass(child_1_quads, CompositorRenderPassId{1}, gfx::Size(100, 100))};

  CompositorFrame child_1_frame = MakeEmptyCompositorFrame();
  AddPasses(&child_1_frame.render_pass_list, child_1_passes,
            &child_1_frame.metadata.referenced_surfaces);

  ParentLocalSurfaceIdAllocator child_1_allocator;
  child_1_allocator.GenerateId();
  LocalSurfaceId child_1_local_surface_id =
      child_1_allocator.GetCurrentLocalSurfaceId();
  SurfaceId child_1_surface_id(child_sink_->frame_sink_id(),
                               child_1_local_surface_id);
  child_sink_->SubmitCompositorFrame(child_1_local_surface_id,
                                     std::move(child_1_frame));

  std::vector<Quad> child_2_quads = {
      Quad::SolidColorQuad(SK_ColorMAGENTA, gfx::Rect(5, 5))};
  std::vector<Pass> child_2_passes = {
      Pass(child_2_quads, CompositorRenderPassId{1}, gfx::Size(100, 100))};

  // Making both metadatas here so that the one with a later timestamp can be
  // on child 2. This will cause the test to fail if we don't default to using
  // the metadata with the later timestamp. Specifically setting the
  // later_metadata timestamp to be 50 microseconds later than Now() to avoid
  // issues with both metadatas sometimes having the same time in Release.
  DelegatedInkMetadata early_metadata = DelegatedInkMetadata(
      gfx::PointF(88, 34), 1.8, SK_ColorBLACK, base::TimeTicks::Now(),
      gfx::RectF(50, 50, 300, 300), base::TimeTicks::Now(), /*hovering*/ false);
  DelegatedInkMetadata later_metadata = DelegatedInkMetadata(
      gfx::PointF(92, 35), 0.08, SK_ColorYELLOW,
      base::TimeTicks::Now() + base::TimeDelta::FromMicroseconds(50),
      gfx::RectF(35, 55, 128, 256),
      base::TimeTicks::Now() + base::TimeDelta::FromMicroseconds(52),
      /*hovering*/ true);

  CompositorFrame child_2_frame = MakeEmptyCompositorFrame();
  child_2_frame.metadata.delegated_ink_metadata =
      std::make_unique<DelegatedInkMetadata>(later_metadata);
  AddPasses(&child_2_frame.render_pass_list, child_2_passes,
            &child_2_frame.metadata.referenced_surfaces);

  ParentLocalSurfaceIdAllocator child_2_allocator;
  child_2_allocator.GenerateId();
  LocalSurfaceId child_2_local_surface_id =
      child_2_allocator.GetCurrentLocalSurfaceId();
  SurfaceId child_2_surface_id(child_2_support->frame_sink_id(),
                               child_2_local_surface_id);
  child_2_support->SubmitCompositorFrame(child_2_local_surface_id,
                                         std::move(child_2_frame));

  std::vector<Quad> child_3_quads = {
      Quad::SolidColorQuad(SK_ColorCYAN, gfx::Rect(5, 5))};
  std::vector<Pass> child_3_passes = {
      Pass(child_3_quads, CompositorRenderPassId{1}, gfx::Size(100, 100))};

  CompositorFrame child_3_frame = MakeEmptyCompositorFrame();
  child_3_frame.metadata.delegated_ink_metadata =
      std::make_unique<DelegatedInkMetadata>(early_metadata);
  AddPasses(&child_3_frame.render_pass_list, child_3_passes,
            &child_3_frame.metadata.referenced_surfaces);

  ParentLocalSurfaceIdAllocator child_3_allocator;
  child_3_allocator.GenerateId();
  LocalSurfaceId child_3_local_surface_id =
      child_3_allocator.GetCurrentLocalSurfaceId();
  SurfaceId child_3_surface_id(child_3_support->frame_sink_id(),
                               child_3_local_surface_id);
  child_3_support->SubmitCompositorFrame(child_3_local_surface_id,
                                         std::move(child_3_frame));

  std::vector<Quad> root_quads = {
      Quad::SurfaceQuad(SurfaceRange(base::nullopt, child_1_surface_id),
                        SK_ColorWHITE, gfx::Rect(5, 5),
                        /*stretch_content_to_fill_bounds=*/false),
      Quad::SurfaceQuad(SurfaceRange(base::nullopt, child_2_surface_id),
                        SK_ColorWHITE, gfx::Rect(5, 5),
                        /*stretch_content_to_fill_bounds=*/false),
      Quad::SurfaceQuad(SurfaceRange(base::nullopt, child_3_surface_id),
                        SK_ColorWHITE, gfx::Rect(5, 5),
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
  gfx::PointF pt = later_metadata.point();
  gfx::RectF area = later_metadata.presentation_area();
  root_frame.render_pass_list[0]
      ->shared_quad_state_list.ElementAt(1)
      ->quad_to_target_transform.TransformPoint(&pt);
  root_frame.render_pass_list[0]
      ->shared_quad_state_list.ElementAt(1)
      ->quad_to_target_transform.TransformRect(&area);

  root_sink_->SubmitCompositorFrame(root_local_surface_id_,
                                    std::move(root_frame));

  SurfaceId root_surface_id(root_sink_->frame_sink_id(),
                            root_local_surface_id_);
  auto aggregated_frame = AggregateFrame(root_surface_id);

  DelegatedInkMetadata expected_metadata = DelegatedInkMetadata(
      pt, later_metadata.diameter(), later_metadata.color(),
      later_metadata.timestamp(), area, later_metadata.frame_time(),
      later_metadata.is_hovering());

  std::unique_ptr<DelegatedInkMetadata> actual_metadata =
      std::move(aggregated_frame.delegated_ink_metadata);
  EXPECT_TRUE(actual_metadata);
  ExpectDelegatedInkMetadataIsEqual(*actual_metadata.get(), expected_metadata);

  // Then confirm that the |delegated_ink_metadata| was reset and a new
  // aggregated frame does not contain any delegated ink metadata.
  auto new_aggregated_frame = AggregateFrame(root_surface_id);
  EXPECT_FALSE(new_aggregated_frame.delegated_ink_metadata);
}

// Confirm that delegated ink metadata on an undrawn surface is not on the
// aggregated surface unless the undrawn surface contains a CopyOutputRequest.
TEST_F(SurfaceAggregatorValidSurfaceTest,
       DelegatedInkMetadataOnUndrawnSurface) {
  std::vector<Quad> child_quads = {
      Quad::SolidColorQuad(SK_ColorGREEN, gfx::Rect(5, 5))};
  std::vector<Pass> child_passes = {
      Pass(child_quads, CompositorRenderPassId{1}, gfx::Size(100, 100))};

  CompositorFrame child_frame = MakeEmptyCompositorFrame();
  DelegatedInkMetadata metadata(gfx::PointF(34, 89), 1.597, SK_ColorBLUE,
                                base::TimeTicks::Now(),
                                gfx::RectF(2.3, 3.2, 177, 212),
                                base::TimeTicks::Now(), /*hovering*/ false);
  child_frame.metadata.delegated_ink_metadata =
      std::make_unique<DelegatedInkMetadata>(metadata);
  AddPasses(&child_frame.render_pass_list, child_passes,
            &child_frame.metadata.referenced_surfaces);

  ParentLocalSurfaceIdAllocator child_allocator;
  child_allocator.GenerateId();
  LocalSurfaceId child_local_surface_id =
      child_allocator.GetCurrentLocalSurfaceId();
  SurfaceId child_surface_id(child_sink_->frame_sink_id(),
                             child_local_surface_id);
  child_sink_->SubmitCompositorFrame(child_local_surface_id,
                                     std::move(child_frame));

  // Do not put the child surface in a SurfaceDrawQuad so that it remains
  // undrawn.
  std::vector<Quad> root_quads = {
      Quad::SolidColorQuad(SK_ColorMAGENTA, gfx::Rect(5, 5))};

  std::vector<Pass> root_passes = {
      Pass(root_quads, CompositorRenderPassId{1}, gfx::Size(30, 30))};

  CompositorFrame root_frame = MakeEmptyCompositorFrame();
  root_frame.metadata.referenced_surfaces.emplace_back(
      SurfaceRange(base::nullopt, child_surface_id));
  AddPasses(&root_frame.render_pass_list, root_passes,
            &root_frame.metadata.referenced_surfaces);

  root_frame.render_pass_list[0]
      ->shared_quad_state_list.ElementAt(0)
      ->quad_to_target_transform.Scale(1.5, 1.5);
  root_frame.render_pass_list[0]
      ->shared_quad_state_list.ElementAt(0)
      ->quad_to_target_transform.Translate(70, 240);

  root_sink_->SubmitCompositorFrame(root_local_surface_id_,
                                    std::move(root_frame));

  SurfaceId root_surface_id(root_sink_->frame_sink_id(),
                            root_local_surface_id_);
  auto aggregated_frame = AggregateFrame(root_surface_id);

  EXPECT_FALSE(aggregated_frame.delegated_ink_metadata);

  // Now add a CopyOutputRequest on the child surface, so that the delegated
  // ink metadata does get populated on the aggregated frame.
  auto copy_request = CopyOutputRequest::CreateStubForTesting();
  child_sink_->RequestCopyOfOutput(
      {child_local_surface_id, SubtreeCaptureId(), std::move(copy_request)});

  aggregated_frame = AggregateFrame(root_surface_id);

  std::unique_ptr<DelegatedInkMetadata> actual_metadata =
      std::move(aggregated_frame.delegated_ink_metadata);
  EXPECT_TRUE(actual_metadata);
  ExpectDelegatedInkMetadataIsEqual(*actual_metadata.get(), metadata);

  // Then confirm that the |delegated_ink_metadata| was reset and a new
  // aggregated frame does not contain any delegated ink metadata.
  auto new_aggregated_frame = AggregateFrame(root_surface_id);
  EXPECT_FALSE(new_aggregated_frame.delegated_ink_metadata);
}

// Tests that changing the color usage results in full-frame damage.
TEST_F(SurfaceAggregatorValidSurfaceTest, ColorUsageChangeFullFrameDamage) {
  constexpr float device_scale_factor = 1.0f;
  const gfx::Rect full_damage_rect(SurfaceSize());
  const gfx::Rect partial_damage_rect(10, 10, 10, 10);
  std::vector<Quad> quads = {
      Quad::SolidColorQuad(SK_ColorRED, gfx::Rect(SurfaceSize()))};
  std::vector<Pass> passes = {Pass(quads, SurfaceSize())};
  passes[0].damage_rect = partial_damage_rect;

  // First frame has full damage.
  {
    SubmitCompositorFrame(root_sink_.get(), passes, root_local_surface_id_,
                          device_scale_factor);
    SurfaceId root_surface_id(root_sink_->frame_sink_id(),
                              root_local_surface_id_);
    auto aggregated_frame = AggregateFrame(root_surface_id);

    EXPECT_EQ(gfx::ContentColorUsage::kHDR,
              aggregated_frame.render_pass_list[0]->content_color_usage);
    EXPECT_EQ(full_damage_rect,
              aggregated_frame.render_pass_list[0]->damage_rect);
  }
  // Second frame has partial damage.
  {
    SubmitCompositorFrame(root_sink_.get(), passes, root_local_surface_id_,
                          device_scale_factor);
    SurfaceId root_surface_id(root_sink_->frame_sink_id(),
                              root_local_surface_id_);
    auto aggregated_frame = AggregateFrame(root_surface_id);

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
    root_sink_->SubmitCompositorFrame(root_local_surface_id_,
                                      std::move(compositor_frame));
    SurfaceId root_surface_id(root_sink_->frame_sink_id(),
                              root_local_surface_id_);
    auto aggregated_frame = AggregateFrame(root_surface_id);

    EXPECT_EQ(gfx::ContentColorUsage::kSRGB,
              aggregated_frame.render_pass_list[0]->content_color_usage);
    EXPECT_EQ(full_damage_rect,
              aggregated_frame.render_pass_list[0]->damage_rect);
  }
}

INSTANTIATE_TEST_SUITE_P(,
                         SurfaceAggregatorValidSurfaceWithMergingPassesTest,
                         testing::Bool());
}  // namespace
}  // namespace viz
