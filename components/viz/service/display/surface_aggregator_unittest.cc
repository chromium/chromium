// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/surface_aggregator.h"

#include <stddef.h>
#include <stdint.h>

#include <set>
#include <utility>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "cc/test/render_pass_test_utils.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/quads/draw_quad.h"
#include "components/viz/common/quads/render_pass.h"
#include "components/viz/common/quads/render_pass_draw_quad.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/surface_draw_quad.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "components/viz/service/display/display_resource_provider.h"
#include "components/viz/service/display_embedder/server_shared_bitmap_manager.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "components/viz/service/surfaces/surface.h"
#include "components/viz/service/surfaces/surface_manager.h"
#include "components/viz/test/compositor_frame_helpers.h"
#include "components/viz/test/fake_compositor_frame_sink_client.h"
#include "components/viz/test/fake_surface_observer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"

namespace viz {
namespace {

using ::testing::_;
constexpr FrameSinkId kArbitraryRootFrameSinkId(1, 1);
constexpr FrameSinkId kArbitraryFrameSinkId1(2, 2);
constexpr FrameSinkId kArbitraryFrameSinkId2(3, 3);
constexpr FrameSinkId kArbitraryMiddleFrameSinkId(4, 4);
constexpr FrameSinkId kArbitraryReservedFrameSinkId(5, 5);
constexpr FrameSinkId kArbitraryFrameSinkId3(6, 6);
const base::UnguessableToken kArbitraryToken = base::UnguessableToken::Create();
constexpr bool kRootIsRoot = true;
constexpr bool kChildIsRoot = false;
constexpr bool kNeedsSyncPoints = false;

gfx::Size SurfaceSize() {
  static gfx::Size size(100, 100);
  return size;
}

gfx::Rect NoDamage() {
  return gfx::Rect();
}

class MockAggregatedDamageCallback {
 public:
  MockAggregatedDamageCallback() : weak_ptr_factory_(this) {}
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
  base::WeakPtrFactory<MockAggregatedDamageCallback> weak_ptr_factory_;

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
        support_(std::make_unique<CompositorFrameSinkSupport>(
            &fake_client_,
            &manager_,
            kArbitraryRootFrameSinkId,
            kRootIsRoot,
            kNeedsSyncPoints)),
        aggregator_(manager_.surface_manager(), nullptr, use_damage_rect) {
    manager_.surface_manager()->AddObserver(&observer_);
  }

  SurfaceAggregatorTest() : SurfaceAggregatorTest(false) {}

  void TearDown() override {
    observer_.Reset();
    testing::Test::TearDown();
  }

  struct Quad {
    static Quad SolidColorQuad(SkColor color, const gfx::Rect& rect) {
      Quad quad;
      quad.material = DrawQuad::SOLID_COLOR;
      quad.color = color;
      quad.rect = rect;
      return quad;
    }

    // If |fallback_surface_id| is a valid surface Id then this will generate
    // two SurfaceDrawQuads.
    static Quad SurfaceQuad(const SurfaceRange& surface_range,
                            SkColor default_background_color,
                            const gfx::Rect& primary_surface_rect,
                            bool stretch_content_to_fill_bounds) {
      Quad quad;
      quad.material = DrawQuad::SURFACE_CONTENT;
      quad.primary_surface_rect = primary_surface_rect;
      quad.surface_range = surface_range;
      quad.default_background_color = default_background_color;
      quad.stretch_content_to_fill_bounds = stretch_content_to_fill_bounds;
      return quad;
    }

    static Quad SurfaceQuad(const SurfaceRange& surface_range,
                            SkColor default_background_color,
                            const gfx::Rect& primary_surface_rect,
                            float opacity,
                            const gfx::Transform& transform,
                            bool stretch_content_to_fill_bounds) {
      Quad quad;
      quad.material = DrawQuad::SURFACE_CONTENT;
      quad.primary_surface_rect = primary_surface_rect;
      quad.opacity = opacity;
      quad.to_target_transform = transform;
      quad.surface_range = surface_range;
      quad.default_background_color = default_background_color;
      quad.stretch_content_to_fill_bounds = stretch_content_to_fill_bounds;
      return quad;
    }

    static Quad RenderPassQuad(int id) {
      Quad quad;
      quad.material = DrawQuad::RENDER_PASS;
      quad.render_pass_id = id;
      return quad;
    }

    DrawQuad::Material material;
    // Set when material==DrawQuad::SURFACE_CONTENT.
    SurfaceRange surface_range;
    SkColor default_background_color;
    bool stretch_content_to_fill_bounds;
    gfx::Rect primary_surface_rect;
    float opacity;
    gfx::Transform to_target_transform;
    // Set when material==DrawQuad::SOLID_COLOR.
    SkColor color;
    gfx::Rect rect;
    // Set when material==DrawQuad::RENDER_PASS.
    RenderPassId render_pass_id;

   private:
    Quad() : material(DrawQuad::INVALID), opacity(1.f), color(SK_ColorWHITE) {}
  };

  struct Pass {
    Pass(const std::vector<Quad>& quads, int id, const gfx::Size& size)
        : quads(quads), id(id), size(size), damage_rect(size) {}
    Pass(const std::vector<Quad>& quads, const gfx::Size& size)
        : quads(quads), size(size), damage_rect(size) {}
    Pass(const std::vector<Quad>& quads,
         const gfx::Size& size,
         const gfx::Rect& damage_rect)
        : quads(quads), size(size), damage_rect(damage_rect) {}

    const std::vector<Quad>& quads;
    int id = 1;
    gfx::Size size;
    gfx::Rect damage_rect;
  };

  // |referenced_surfaces| refers to the SurfaceRanges of all the
  // SurfaceDrawQuads added to the provided |pass|.
  static void AddQuadInPass(const Quad& desc,
                            RenderPass* pass,
                            std::vector<SurfaceRange>* referenced_surfaces) {
    switch (desc.material) {
      case DrawQuad::SOLID_COLOR:
        cc::AddQuad(pass, gfx::Rect(0, 0, 5, 5), desc.color);
        break;
      case DrawQuad::SURFACE_CONTENT:
        referenced_surfaces->emplace_back(desc.surface_range);
        AddSurfaceQuad(pass, desc.primary_surface_rect, desc.opacity,
                       desc.to_target_transform, desc.surface_range,
                       desc.default_background_color,
                       desc.stretch_content_to_fill_bounds);
        break;
      case DrawQuad::RENDER_PASS:
        AddRenderPassQuad(pass, desc.render_pass_id);
        break;
      default:
        NOTREACHED();
    }
  }

  static void AddPasses(RenderPassList* pass_list,
                        const std::vector<Pass>& passes,
                        std::vector<SurfaceRange>* referenced_surfaces) {
    gfx::Transform root_transform;
    for (auto& pass : passes) {
      RenderPass* test_pass = AddRenderPassWithDamage(
          pass_list, pass.id, gfx::Rect(pass.size), pass.damage_rect,
          root_transform, cc::FilterOperations());
      for (size_t j = 0; j < pass.quads.size(); ++j)
        AddQuadInPass(pass.quads[j], test_pass, referenced_surfaces);
    }
  }

  static void TestQuadMatchesExpectations(Quad expected_quad,
                                          const DrawQuad* quad) {
    switch (expected_quad.material) {
      case DrawQuad::SOLID_COLOR: {
        ASSERT_EQ(DrawQuad::SOLID_COLOR, quad->material);

        const auto* solid_color_quad = SolidColorDrawQuad::MaterialCast(quad);

        EXPECT_EQ(expected_quad.color, solid_color_quad->color);
        EXPECT_EQ(expected_quad.rect, solid_color_quad->rect);
        break;
      }
      case DrawQuad::RENDER_PASS: {
        ASSERT_EQ(DrawQuad::RENDER_PASS, quad->material);

        const auto* render_pass_quad = RenderPassDrawQuad::MaterialCast(quad);

        EXPECT_EQ(expected_quad.render_pass_id,
                  render_pass_quad->render_pass_id);
        break;
      }
      default:
        NOTREACHED();
        break;
    }
  }

  static void TestPassMatchesExpectations(Pass expected_pass,
                                          const RenderPass* pass) {
    ASSERT_EQ(expected_pass.quads.size(), pass->quad_list.size());
    for (auto iter = pass->quad_list.cbegin(); iter != pass->quad_list.cend();
         ++iter) {
      SCOPED_TRACE(base::StringPrintf("Quad number %" PRIuS, iter.index()));
      TestQuadMatchesExpectations(expected_pass.quads[iter.index()], *iter);
    }
  }

  static void TestPassesMatchExpectations(
      const std::vector<Pass>& expected_passes,
      const RenderPassList* passes) {
    ASSERT_EQ(expected_passes.size(), passes->size());

    for (size_t i = 0; i < expected_passes.size(); ++i) {
      SCOPED_TRACE(base::StringPrintf("Pass number %" PRIuS, i));
      RenderPass* pass = (*passes)[i].get();
      TestPassMatchesExpectations(expected_passes[i], pass);
    }
  }

 private:
  static void AddSurfaceQuad(RenderPass* pass,
                             const gfx::Rect& primary_surface_rect,
                             float opacity,
                             const gfx::Transform& transform,
                             const SurfaceRange& surface_range,
                             SkColor default_background_color,
                             bool stretch_content_to_fill_bounds) {
    gfx::Transform layer_to_target_transform = transform;
    gfx::Rect layer_bounds(primary_surface_rect);
    gfx::Rect visible_layer_rect(primary_surface_rect);
    gfx::Rect clip_rect(primary_surface_rect);
    bool is_clipped = false;
    bool are_contents_opaque = false;
    SkBlendMode blend_mode = SkBlendMode::kSrcOver;

    auto* shared_quad_state = pass->CreateAndAppendSharedQuadState();
    shared_quad_state->SetAll(layer_to_target_transform, layer_bounds,
                              visible_layer_rect, clip_rect, is_clipped,
                              are_contents_opaque, opacity, blend_mode, 0);

    SurfaceDrawQuad* surface_quad =
        pass->CreateAndAppendDrawQuad<SurfaceDrawQuad>();
    surface_quad->SetNew(pass->shared_quad_state_list.back(),
                         primary_surface_rect, primary_surface_rect,
                         surface_range, default_background_color,
                         stretch_content_to_fill_bounds);
  }

  static void AddRenderPassQuad(RenderPass* pass, RenderPassId render_pass_id) {
    gfx::Rect output_rect = gfx::Rect(0, 0, 5, 5);
    auto* shared_state = pass->CreateAndAppendSharedQuadState();
    shared_state->SetAll(gfx::Transform(), output_rect, output_rect,
                         output_rect, false, false, 1, SkBlendMode::kSrcOver,
                         0);
    auto* quad = pass->CreateAndAppendDrawQuad<RenderPassDrawQuad>();
    quad->SetNew(shared_state, output_rect, output_rect, render_pass_id, 0,
                 gfx::RectF(), gfx::Size(), gfx::Vector2dF(), gfx::PointF(),
                 gfx::RectF(), false);
  }

 protected:
  ServerSharedBitmapManager shared_bitmap_manager_;
  FrameSinkManagerImpl manager_;
  FakeSurfaceObserver observer_;
  FakeCompositorFrameSinkClient fake_client_;
  std::unique_ptr<CompositorFrameSinkSupport> support_;
  SurfaceAggregator aggregator_;
};

class SurfaceAggregatorValidSurfaceTest : public SurfaceAggregatorTest {
 public:
  explicit SurfaceAggregatorValidSurfaceTest(bool use_damage_rect)
      : SurfaceAggregatorTest(use_damage_rect),
        child_support_(std::make_unique<CompositorFrameSinkSupport>(
            nullptr,
            &manager_,
            kArbitraryReservedFrameSinkId,
            kChildIsRoot,
            kNeedsSyncPoints)) {
    child_support_->set_allow_copy_output_requests_for_testing();
  }

  SurfaceAggregatorValidSurfaceTest()
      : SurfaceAggregatorValidSurfaceTest(false) {}

  void SetUp() override {
    SurfaceAggregatorTest::SetUp();
    root_local_surface_id_ = allocator_.GetCurrentLocalSurfaceId();
    root_surface_ = manager_.surface_manager()->GetSurfaceForId(
        SurfaceId(support_->frame_sink_id(), root_local_surface_id_));
  }

  void TearDown() override {
    SurfaceAggregatorTest::TearDown();
  }

  // Verifies that if the |SharedQuadState::quad_layer_rect| can be covered by
  // |DrawQuad::Rect| in the SharedQuadState.
  void VerifyQuadCoverSQS(CompositorFrame* aggregated_frame) {
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
    CompositorFrame aggregated_frame = aggregator_.Aggregate(
        SurfaceId(support_->frame_sink_id(), root_local_surface_id_),
        GetNextDisplayTimeAndIncrement());

    TestPassesMatchExpectations(expected_passes,
                                &aggregated_frame.render_pass_list);
    VerifyQuadCoverSQS(&aggregated_frame);

    // Ensure no duplicate pass ids output.
    std::set<RenderPassId> used_passes;
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
                             RenderPassList* pass_list,
                             std::vector<SurfaceRange> referenced_surfaces,
                             float device_scale_factor) {
    CompositorFrame frame =
        CompositorFrameBuilder()
            .SetRenderPassList(std::move(*pass_list))
            .SetDeviceScaleFactor(device_scale_factor)
            .SetReferencedSurfaces(std::move(referenced_surfaces))
            .Build();
    pass_list->clear();

    support->SubmitCompositorFrame(local_surface_id, std::move(frame));
  }

  void SubmitCompositorFrame(CompositorFrameSinkSupport* support,
                             const std::vector<Pass>& passes,
                             const LocalSurfaceId& local_surface_id,
                             float device_scale_factor) {
    RenderPassList pass_list;
    std::vector<SurfaceRange> referenced_surfaces;
    AddPasses(&pass_list, passes, &referenced_surfaces);
    SubmitPassListAsFrame(support, local_surface_id, &pass_list,
                          std::move(referenced_surfaces), device_scale_factor);
  }

  CompositorFrame MakeCompositorFrameFromSurfaceRanges(
      const std::vector<SurfaceRange>& ranges) {
    std::vector<Quad> quads;
    for (const SurfaceRange& range : ranges) {
      quads.push_back(Quad::SurfaceQuad(range, SK_ColorWHITE, gfx::Rect(5, 5),
                                        1.f, gfx::Transform(), false));
    }
    std::vector<Pass> passes = {Pass(quads, SurfaceSize())};
    RenderPassList pass_list;
    std::vector<SurfaceRange> referenced_surfaces;
    AddPasses(&pass_list, passes, &referenced_surfaces);
    return CompositorFrameBuilder()
        .SetRenderPassList(std::move(pass_list))
        .SetDeviceScaleFactor(1.f)
        .SetReferencedSurfaces(ranges)
        .Build();
  }

  void QueuePassAsFrame(std::unique_ptr<RenderPass> pass,
                        const LocalSurfaceId& local_surface_id,
                        float device_scale_factor,
                        CompositorFrameSinkSupport* support) {
    CompositorFrame child_frame = CompositorFrameBuilder()
                                      .AddRenderPass(std::move(pass))
                                      .SetDeviceScaleFactor(device_scale_factor)
                                      .Build();

    support->SubmitCompositorFrame(local_surface_id, std::move(child_frame));
  }

 protected:
  LocalSurfaceId root_local_surface_id_;
  Surface* root_surface_;
  ParentLocalSurfaceIdAllocator allocator_;
  std::unique_ptr<CompositorFrameSinkSupport> child_support_;
  ParentLocalSurfaceIdAllocator child_allocator_;
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
  support_->SetAggregatedDamageCallbackForTesting(
      aggregated_damage_callback.GetCallback());

  constexpr float device_scale_factor = 1.0f;
  SubmitCompositorFrame(support_.get(), passes, root_local_surface_id_,
                        device_scale_factor);

  SurfaceId root_surface_id(support_->frame_sink_id(), root_local_surface_id_);

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
      nullptr, &manager_, kArbitraryFrameSinkId1, kRootIsRoot,
      kNeedsSyncPoints);
  LocalSurfaceId embedded_local_surface_id = allocator_.GenerateId();
  SurfaceId embedded_surface_id(embedded_support->frame_sink_id(),
                                embedded_local_surface_id);

  std::vector<Quad> embedded_quads = {
      Quad::SolidColorQuad(SK_ColorGREEN, gfx::Rect(5, 5)),
      Quad::SolidColorQuad(SK_ColorBLUE, gfx::Rect(5, 5))};
  std::vector<Pass> embedded_passes = {Pass(embedded_quads, SurfaceSize())};

  constexpr float device_scale_factor = 1.0f;
  SubmitCompositorFrame(embedded_support.get(), embedded_passes,
                        embedded_local_surface_id, device_scale_factor);
  SurfaceId root_surface_id(support_->frame_sink_id(), root_local_surface_id_);
  {
    std::vector<Quad> quads = {Quad::SurfaceQuad(
        SurfaceRange(base::nullopt, embedded_surface_id), SK_ColorWHITE,
        gfx::Rect(5, 5), .5f, gfx::Transform(), false)};
    std::vector<Pass> passes = {Pass(quads, SurfaceSize())};

    SubmitCompositorFrame(support_.get(), passes, root_local_surface_id_,
                          device_scale_factor);

    CompositorFrame aggregated_frame = aggregator_.Aggregate(
        root_surface_id, GetNextDisplayTimeAndIncrement());

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
        gfx::Rect(5, 5), .9999f, gfx::Transform(), false)};
    std::vector<Pass> passes = {Pass(quads, SurfaceSize())};

    SubmitCompositorFrame(support_.get(), passes, root_local_surface_id_,
                          device_scale_factor);

    CompositorFrame aggregated_frame = aggregator_.Aggregate(
        root_surface_id, GetNextDisplayTimeAndIncrement());

    auto& render_pass_list = aggregated_frame.render_pass_list;
    EXPECT_EQ(1u, render_pass_list.size());
  }
}

// Test that when surface is rotated and we need the render surface to apply the
// clip, we would keep the render surface.
TEST_F(SurfaceAggregatorValidSurfaceTest, RotatedClip) {
  auto embedded_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryFrameSinkId1, kRootIsRoot,
      kNeedsSyncPoints);
  LocalSurfaceId embedded_local_surface_id = allocator_.GenerateId();
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
  std::vector<Quad> quads = {
      Quad::SurfaceQuad(SurfaceRange(base::nullopt, embedded_surface_id),
                        SK_ColorWHITE, gfx::Rect(5, 5), 1.f, rotate, false)};
  std::vector<Pass> passes = {Pass(quads, SurfaceSize())};

  SubmitCompositorFrame(support_.get(), passes, root_local_surface_id_,
                        device_scale_factor);

  SurfaceId root_surface_id(support_->frame_sink_id(), root_local_surface_id_);
  CompositorFrame aggregated_frame =
      aggregator_.Aggregate(root_surface_id, GetNextDisplayTimeAndIncrement());

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
  std::vector<Pass> passes = {Pass(quads[0], 1, SurfaceSize()),
                              Pass(quads[1], 2, SurfaceSize())};

  constexpr float device_scale_factor = 1.0f;

  SubmitCompositorFrame(support_.get(), passes, root_local_surface_id_,
                        device_scale_factor);

  SurfaceId root_surface_id(support_->frame_sink_id(), root_local_surface_id_);

  AggregateAndVerify(passes, {root_surface_id});
}

// Ensure that the render pass ID map properly keeps and deletes entries.
TEST_F(SurfaceAggregatorValidSurfaceTest, MultiPassDeallocation) {
  std::vector<Quad> quads[2] = {
      {Quad::SolidColorQuad(SK_ColorWHITE, gfx::Rect(5, 5)),
       Quad::SolidColorQuad(SK_ColorLTGRAY, gfx::Rect(5, 5))},
      {Quad::SolidColorQuad(SK_ColorGRAY, gfx::Rect(5, 5)),
       Quad::SolidColorQuad(SK_ColorDKGRAY, gfx::Rect(5, 5))}};
  std::vector<Pass> passes = {Pass(quads[0], 2, SurfaceSize()),
                              Pass(quads[1], 1, SurfaceSize())};

  constexpr float device_scale_factor = 1.0f;
  SubmitCompositorFrame(support_.get(), passes, root_local_surface_id_,
                        device_scale_factor);

  SurfaceId surface_id(support_->frame_sink_id(), root_local_surface_id_);

  CompositorFrame aggregated_frame;
  aggregated_frame =
      aggregator_.Aggregate(surface_id, GetNextDisplayTimeAndIncrement());
  auto id0 = aggregated_frame.render_pass_list[0]->id;
  auto id1 = aggregated_frame.render_pass_list[1]->id;
  EXPECT_NE(id1, id0);

  // Aggregated RenderPass ids should remain the same between frames.
  aggregated_frame =
      aggregator_.Aggregate(surface_id, GetNextDisplayTimeAndIncrement());
  EXPECT_EQ(id0, aggregated_frame.render_pass_list[0]->id);
  EXPECT_EQ(id1, aggregated_frame.render_pass_list[1]->id);

  std::vector<Pass> passes2 = {Pass(quads[0], 3, SurfaceSize()),
                               Pass(quads[1], 1, SurfaceSize())};

  SubmitCompositorFrame(support_.get(), passes2, root_local_surface_id_,
                        device_scale_factor);

  // The RenderPass that still exists should keep the same ID.
  aggregated_frame =
      aggregator_.Aggregate(surface_id, GetNextDisplayTimeAndIncrement());
  auto id2 = aggregated_frame.render_pass_list[0]->id;
  EXPECT_NE(id2, id1);
  EXPECT_NE(id2, id0);
  EXPECT_EQ(id1, aggregated_frame.render_pass_list[1]->id);

  SubmitCompositorFrame(support_.get(), passes, root_local_surface_id_,
                        device_scale_factor);

  // |id1| didn't exist in the previous frame, so it should be
  // mapped to a new ID.
  aggregated_frame =
      aggregator_.Aggregate(surface_id, GetNextDisplayTimeAndIncrement());
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
      nullptr, &manager_, kArbitraryFrameSinkId1, kRootIsRoot,
      kNeedsSyncPoints);
  LocalSurfaceId embedded_local_surface_id = allocator_.GenerateId();
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
                        SK_ColorWHITE, gfx::Rect(5, 5), false),
      Quad::SolidColorQuad(SK_ColorBLACK, gfx::Rect(5, 5))};
  std::vector<Pass> root_passes = {Pass(root_quads, SurfaceSize())};

  SubmitCompositorFrame(support_.get(), root_passes, root_local_surface_id_,
                        device_scale_factor);

  std::vector<Quad> expected_quads = {
      Quad::SolidColorQuad(SK_ColorWHITE, gfx::Rect(5, 5)),
      Quad::SolidColorQuad(SK_ColorGREEN, gfx::Rect(5, 5)),
      Quad::SolidColorQuad(SK_ColorBLACK, gfx::Rect(5, 5))};
  std::vector<Pass> expected_passes = {Pass(expected_quads, SurfaceSize())};
  SurfaceId root_surface_id(support_->frame_sink_id(), root_local_surface_id_);
  AggregateAndVerify(expected_passes, {root_surface_id, embedded_surface_id});
}

// This test verifies that in the absence of a primary Surface,
// SurfaceAggregator will embed a fallback Surface, if available. If the primary
// Surface is available, though, the fallback will not be used.
TEST_F(SurfaceAggregatorValidSurfaceTest, FallbackSurfaceReference) {
  auto primary_child_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryFrameSinkId1, kChildIsRoot,
      kNeedsSyncPoints);

  auto fallback_child_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryFrameSinkId2, kChildIsRoot,
      kNeedsSyncPoints);

  LocalSurfaceId fallback_child_local_surface_id =
      child_allocator_.GenerateId();
  SurfaceId fallback_child_surface_id(fallback_child_support->frame_sink_id(),
                                      fallback_child_local_surface_id);

  LocalSurfaceId primary_child_local_surface_id = child_allocator_.GenerateId();
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
  // |fallback_child_surface_id|.
  constexpr gfx::Rect surface_quad_rect(12, 15);
  std::vector<Quad> root_quads = {Quad::SurfaceQuad(
      SurfaceRange(fallback_child_surface_id, primary_child_surface_id),
      SK_ColorWHITE, surface_quad_rect, false)};
  std::vector<Pass> root_passes = {Pass(root_quads, SurfaceSize())};

  MockAggregatedDamageCallback aggregated_damage_callback;

  support_->SetAggregatedDamageCallbackForTesting(
      aggregated_damage_callback.GetCallback());
  primary_child_support->SetAggregatedDamageCallbackForTesting(
      aggregated_damage_callback.GetCallback());
  fallback_child_support->SetAggregatedDamageCallbackForTesting(
      aggregated_damage_callback.GetCallback());

  SubmitCompositorFrame(support_.get(), root_passes, root_local_surface_id_,
                        device_scale_factor_1);

  // There is no CompositorFrame submitted to |primary_child_surface_id| and
  // so |fallback_child_surface_id| will be embedded and we should see a red
  // SolidColorDrawQuad. These quads are in physical pixels.
  std::vector<Quad> expected_quads1 = {
      // Right gutter.
      Quad::SolidColorQuad(SK_ColorWHITE, gfx::Rect(5, 0, 7, 15)),
      // Bottom guttter.
      Quad::SolidColorQuad(SK_ColorWHITE, gfx::Rect(0, 5, 5, 10)),
      // Contents of the fallback surface.
      Quad::SolidColorQuad(SK_ColorRED, gfx::Rect(5, 5)),
  };
  std::vector<Pass> expected_passes1 = {Pass(expected_quads1, SurfaceSize())};

  SurfaceId root_surface_id(support_->frame_sink_id(), root_local_surface_id_);

  EXPECT_CALL(aggregated_damage_callback,
              OnAggregatedDamage(fallback_child_local_surface_id, fallback_size,
                                 gfx::Rect(fallback_size), next_display_time()))
      .Times(1);
  EXPECT_CALL(aggregated_damage_callback,
              OnAggregatedDamage(primary_child_local_surface_id, SurfaceSize(),
                                 gfx::Rect(SurfaceSize()), next_display_time()))
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
  fallback_child_local_surface_id = child_allocator_.GenerateId();

  SubmitCompositorFrame(fallback_child_support.get(), fallback_child_passes,

                        fallback_child_local_surface_id, device_scale_factor_2);

  EXPECT_CALL(aggregated_damage_callback,
              OnAggregatedDamage(fallback_child_local_surface_id, _, _, _));
  // The damage should be equal to whole size of the primary SurfaceDrawQuad.
  EXPECT_CALL(
      aggregated_damage_callback,
      OnAggregatedDamage(root_local_surface_id_, SurfaceSize(),
                         surface_quad_rect, testing::A<base::TimeTicks>()))
      .Times(1);

  AggregateAndVerify(
      expected_passes1,
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
  std::vector<Quad> expected_quads2 = {
      Quad::SolidColorQuad(SK_ColorGREEN, gfx::Rect(5, 5))};
  std::vector<Pass> expected_passes2 = {Pass(expected_quads2, SurfaceSize())};

  EXPECT_CALL(
      aggregated_damage_callback,
      OnAggregatedDamage(primary_child_local_surface_id, primary_surface_size,
                         gfx::Rect(primary_surface_size), next_display_time()))
      .Times(1);
  EXPECT_CALL(aggregated_damage_callback,
              OnAggregatedDamage(fallback_child_local_surface_id, fallback_size,
                                 gfx::Rect(fallback_size), next_display_time()))
      .Times(0);
  // The damage of the root should be equal to the damage of the primary
  // surface.
  EXPECT_CALL(
      aggregated_damage_callback,
      OnAggregatedDamage(root_local_surface_id_, SurfaceSize(),
                         gfx::Rect(primary_surface_size), next_display_time()))
      .Times(1);

  AggregateAndVerify(expected_passes2,
                     {root_surface_id, primary_child_surface_id});

  testing::Mock::VerifyAndClearExpectations(&aggregated_damage_callback);
}

// This test verifies that the appropriate transform will be applied to a
// surface embedded by a parent SurfaceDrawQuad marked as
// stretch_content_to_fill_bounds.
TEST_F(SurfaceAggregatorValidSurfaceTest, StretchContentToFillBounds) {
  auto primary_child_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryFrameSinkId1, kChildIsRoot,
      kNeedsSyncPoints);
  LocalSurfaceId primary_child_local_surface_id = allocator_.GenerateId();
  SurfaceId primary_child_surface_id(primary_child_support->frame_sink_id(),
                                     primary_child_local_surface_id);

  {
    auto pass = RenderPass::Create();
    pass->SetNew(1, gfx::Rect(0, 0, 20, 20), gfx::Rect(), gfx::Transform());
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
  std::vector<Quad> root_quads = {
      Quad::SurfaceQuad(SurfaceRange(primary_child_surface_id), SK_ColorWHITE,
                        surface_quad_rect, true)};
  std::vector<Pass> root_passes = {Pass(root_quads, SurfaceSize())};

  MockAggregatedDamageCallback aggregated_damage_callback;
  support_->SetAggregatedDamageCallbackForTesting(
      aggregated_damage_callback.GetCallback());

  SurfaceId root_surface_id(support_->frame_sink_id(), root_local_surface_id_);
  SubmitCompositorFrame(support_.get(), root_passes, root_local_surface_id_,
                        1.0f);

  EXPECT_CALL(
      aggregated_damage_callback,
      OnAggregatedDamage(root_local_surface_id_, SurfaceSize(),
                         gfx::Rect(SurfaceSize()), next_display_time()));
  CompositorFrame frame =
      aggregator_.Aggregate(root_surface_id, GetNextDisplayTimeAndIncrement());
  testing::Mock::VerifyAndClearExpectations(&aggregated_damage_callback);

  EXPECT_EQ(1u, frame.render_pass_list.size());
  auto* render_pass = frame.render_pass_list.back().get();
  EXPECT_EQ(1u, render_pass->quad_list.size());

  auto* output_quad = render_pass->quad_list.back();

  EXPECT_EQ(DrawQuad::SOLID_COLOR, output_quad->material);
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
      nullptr, &manager_, kArbitraryFrameSinkId1, kChildIsRoot,
      kNeedsSyncPoints);
  LocalSurfaceId primary_child_local_surface_id = allocator_.GenerateId();
  SurfaceId primary_child_surface_id(primary_child_support->frame_sink_id(),
                                     primary_child_local_surface_id);

  {
    auto pass = RenderPass::Create();
    pass->SetNew(1, gfx::Rect(0, 0, 20, 20), gfx::Rect(), gfx::Transform());
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
  std::vector<Quad> root_quads = {
      Quad::SurfaceQuad(SurfaceRange(primary_child_surface_id), SK_ColorWHITE,
                        surface_quad_rect, true)};
  std::vector<Pass> root_passes = {Pass(root_quads, SurfaceSize())};

  MockAggregatedDamageCallback aggregated_damage_callback;
  support_->SetAggregatedDamageCallbackForTesting(
      aggregated_damage_callback.GetCallback());

  SurfaceId root_surface_id(support_->frame_sink_id(), root_local_surface_id_);
  SubmitCompositorFrame(support_.get(), root_passes, root_local_surface_id_,
                        2.0f);

  EXPECT_CALL(
      aggregated_damage_callback,
      OnAggregatedDamage(root_local_surface_id_, SurfaceSize(),
                         gfx::Rect(SurfaceSize()), next_display_time()));
  CompositorFrame frame =
      aggregator_.Aggregate(root_surface_id, GetNextDisplayTimeAndIncrement());

  testing::Mock::VerifyAndClearExpectations(&aggregated_damage_callback);

  EXPECT_EQ(1u, frame.render_pass_list.size());
  auto* render_pass = frame.render_pass_list.back().get();
  EXPECT_EQ(1u, render_pass->quad_list.size());

  auto* output_quad = render_pass->quad_list.back();

  EXPECT_EQ(DrawQuad::SOLID_COLOR, output_quad->material);
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
      nullptr, &manager_, kArbitraryFrameSinkId1, kChildIsRoot,
      kNeedsSyncPoints);
  LocalSurfaceId primary_child_local_surface_id = allocator_.GenerateId();
  SurfaceId primary_child_surface_id(primary_child_support->frame_sink_id(),
                                     primary_child_local_surface_id);

  {
    auto pass = RenderPass::Create();
    pass->SetNew(1, gfx::Rect(0, 0, 20, 20), gfx::Rect(), gfx::Transform());
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
  std::vector<Quad> root_quads = {
      Quad::SurfaceQuad(SurfaceRange(primary_child_surface_id), SK_ColorWHITE,
                        surface_quad_rect, true)};
  std::vector<Pass> root_passes = {Pass(root_quads, SurfaceSize())};

  MockAggregatedDamageCallback aggregated_damage_callback;
  support_->SetAggregatedDamageCallbackForTesting(
      aggregated_damage_callback.GetCallback());

  SurfaceId root_surface_id(support_->frame_sink_id(), root_local_surface_id_);
  SubmitCompositorFrame(support_.get(), root_passes, root_local_surface_id_,
                        0.5f);

  EXPECT_CALL(
      aggregated_damage_callback,
      OnAggregatedDamage(root_local_surface_id_, SurfaceSize(),
                         gfx::Rect(SurfaceSize()), next_display_time()));
  CompositorFrame frame =
      aggregator_.Aggregate(root_surface_id, GetNextDisplayTimeAndIncrement());
  testing::Mock::VerifyAndClearExpectations(&aggregated_damage_callback);

  EXPECT_EQ(1u, frame.render_pass_list.size());
  auto* render_pass = frame.render_pass_list.back().get();
  EXPECT_EQ(1u, render_pass->quad_list.size());

  auto* output_quad = render_pass->quad_list.back();

  EXPECT_EQ(DrawQuad::SOLID_COLOR, output_quad->material);
  gfx::RectF output_rect(50.f, 50.f);

  // SurfaceAggregator should stretch the SolidColorDrawQuad to fit the bounds
  // of the parent's SurfaceDrawQuad.
  output_quad->shared_quad_state->quad_to_target_transform.TransformRect(
      &output_rect);

  EXPECT_EQ(gfx::RectF(25.f, 12.5f), output_rect);
}

// This test verifies that in the presence of both primary Surface and fallback
// Surface, the fallback will not be used.
TEST_F(SurfaceAggregatorValidSurfaceTest, FallbackSurfaceReferenceWithPrimary) {
  auto primary_child_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryFrameSinkId1, kChildIsRoot,
      kNeedsSyncPoints);
  LocalSurfaceId primary_child_local_surface_id = allocator_.GenerateId();
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
      nullptr, &manager_, kArbitraryFrameSinkId2, kChildIsRoot,
      kNeedsSyncPoints);
  LocalSurfaceId fallback_child_local_surface_id = allocator_.GenerateId();
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
      SK_ColorWHITE, gfx::Rect(5, 5), false)};
  constexpr gfx::Size root_size(75, 75);
  std::vector<Pass> root_passes = {Pass(root_quads, root_size, NoDamage())};

  MockAggregatedDamageCallback aggregated_damage_callback;
  support_->SetAggregatedDamageCallbackForTesting(
      aggregated_damage_callback.GetCallback());

  SubmitCompositorFrame(support_.get(), root_passes, root_local_surface_id_,
                        device_scale_factor);

  // The CompositorFrame is submitted to |primary_child_surface_id|, so
  // |fallback_child_surface_id| will not be used and we should see a green
  // SolidColorDrawQuad.
  std::vector<Quad> expected_quads1 = {
      Quad::SolidColorQuad(SK_ColorGREEN, gfx::Rect(5, 5))};
  std::vector<Pass> expected_passes1 = {Pass(expected_quads1, SurfaceSize())};

  SurfaceId root_surface_id(support_->frame_sink_id(), root_local_surface_id_);

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
      nullptr, &manager_, kArbitraryFrameSinkId1, kChildIsRoot,
      kNeedsSyncPoints);
  LocalSurfaceId embedded_local_surface_id = allocator_.GenerateId();
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
  embedded_support->RequestCopyOfOutput(embedded_local_surface_id,
                                        std::move(copy_request));

  std::vector<Quad> root_quads = {
      Quad::SolidColorQuad(SK_ColorWHITE, gfx::Rect(5, 5)),
      Quad::SurfaceQuad(SurfaceRange(base::nullopt, embedded_surface_id),
                        SK_ColorWHITE, gfx::Rect(5, 5), false),
      Quad::SolidColorQuad(SK_ColorBLACK, gfx::Rect(5, 5))};
  std::vector<Pass> root_passes = {Pass(root_quads, SurfaceSize())};

  SubmitCompositorFrame(support_.get(), root_passes, root_local_surface_id_,
                        device_scale_factor);

  SurfaceId root_surface_id(support_->frame_sink_id(), root_local_surface_id_);
  CompositorFrame aggregated_frame =
      aggregator_.Aggregate(root_surface_id, GetNextDisplayTimeAndIncrement());

  std::vector<Quad> expected_quads = {
      Quad::SolidColorQuad(SK_ColorWHITE, gfx::Rect(5, 5)),
      Quad::RenderPassQuad(aggregated_frame.render_pass_list[0]->id),
      Quad::SolidColorQuad(SK_ColorBLACK, gfx::Rect(5, 5))};
  std::vector<Pass> expected_passes = {Pass(embedded_quads, SurfaceSize()),
                                       Pass(expected_quads, SurfaceSize())};
  TestPassesMatchExpectations(expected_passes,
                              &aggregated_frame.render_pass_list);
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
      nullptr, &manager_, kArbitraryFrameSinkId2, kChildIsRoot,
      kNeedsSyncPoints);
  LocalSurfaceId embedded_local_surface_id = allocator_.GenerateId();
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
                        SK_ColorWHITE, gfx::Rect(5, 5), false),
      Quad::SolidColorQuad(SK_ColorBLACK, gfx::Rect(5, 5))};
  std::vector<Quad> root_quads2 = {
      Quad::SolidColorQuad(SK_ColorRED, gfx::Rect(5, 5))};
  std::vector<Pass> root_passes = {Pass(root_quads, 1, SurfaceSize()),
                                   Pass(root_quads2, 2, SurfaceSize())};
  {
    CompositorFrame frame = MakeEmptyCompositorFrame();
    AddPasses(&frame.render_pass_list, root_passes,
              &frame.metadata.referenced_surfaces);
    frame.render_pass_list[0]->copy_requests.push_back(std::move(copy_request));
    frame.render_pass_list[1]->copy_requests.push_back(
        std::move(copy_request2));

    support_->SubmitCompositorFrame(root_local_surface_id_, std::move(frame));
  }

  SurfaceId root_surface_id(support_->frame_sink_id(), root_local_surface_id_);
  CompositorFrame aggregated_frame =
      aggregator_.Aggregate(root_surface_id, GetNextDisplayTimeAndIncrement());

  std::vector<Quad> expected_quads = {
      Quad::SolidColorQuad(SK_ColorWHITE, gfx::Rect(5, 5)),
      Quad::SolidColorQuad(SK_ColorGREEN, gfx::Rect(5, 5)),
      Quad::SolidColorQuad(SK_ColorBLACK, gfx::Rect(5, 5))};
  std::vector<Pass> expected_passes = {Pass(expected_quads, SurfaceSize()),
                                       Pass(root_quads2, SurfaceSize())};
  TestPassesMatchExpectations(expected_passes,
                              &aggregated_frame.render_pass_list);
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
      nullptr, &manager_, kArbitraryFrameSinkId1, kChildIsRoot,
      kNeedsSyncPoints);
  auto parent_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryFrameSinkId2, kRootIsRoot,
      kNeedsSyncPoints);
  LocalSurfaceId embedded_local_surface_id = allocator_.GenerateId();
  SurfaceId embedded_surface_id(embedded_support->frame_sink_id(),
                                embedded_local_surface_id);
  SurfaceId nonexistent_surface_id(support_->frame_sink_id(),
                                   allocator_.GenerateId());

  std::vector<Quad> embedded_quads = {
      Quad::SolidColorQuad(SK_ColorGREEN, gfx::Rect(5, 5))};
  std::vector<Pass> embedded_passes = {Pass(embedded_quads, SurfaceSize())};

  constexpr float device_scale_factor = 1.0f;
  SubmitCompositorFrame(embedded_support.get(), embedded_passes,
                        embedded_local_surface_id, device_scale_factor);
  auto copy_request(CopyOutputRequest::CreateStubForTesting());
  auto* copy_request_ptr = copy_request.get();
  embedded_support->RequestCopyOfOutput(embedded_local_surface_id,
                                        std::move(copy_request));

  LocalSurfaceId parent_local_surface_id = allocator_.GenerateId();
  SurfaceId parent_surface_id(parent_support->frame_sink_id(),
                              parent_local_surface_id);

  std::vector<Quad> parent_quads = {
      Quad::SolidColorQuad(SK_ColorGRAY, gfx::Rect(5, 5)),
      Quad::SurfaceQuad(SurfaceRange(base::nullopt, embedded_surface_id),
                        SK_ColorWHITE, gfx::Rect(5, 5), false),
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

    support_->SubmitCompositorFrame(root_local_surface_id_, std::move(frame));
  }

  SurfaceId root_surface_id(support_->frame_sink_id(), root_local_surface_id_);
  CompositorFrame aggregated_frame =
      aggregator_.Aggregate(root_surface_id, GetNextDisplayTimeAndIncrement());

  // First pass should come from surface that had a copy request but was not
  // referenced directly. The second pass comes from the root surface.
  // parent_quad should be ignored because it is neither referenced through a
  // SurfaceDrawQuad nor has a copy request on it.
  std::vector<Pass> expected_passes = {Pass(embedded_quads, SurfaceSize()),
                                       Pass(root_quads, SurfaceSize())};
  TestPassesMatchExpectations(expected_passes,
                              &aggregated_frame.render_pass_list);
  ASSERT_EQ(2u, aggregated_frame.render_pass_list.size());
  ASSERT_EQ(1u, aggregated_frame.render_pass_list[0]->copy_requests.size());
  DCHECK_EQ(copy_request_ptr,
            aggregated_frame.render_pass_list[0]->copy_requests[0].get());

  SurfaceId surface_ids[] = {
      SurfaceId(support_->frame_sink_id(), root_local_surface_id_),
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
  LocalSurfaceId embedded_local_surface_id = child_allocator_.GenerateId();
  SurfaceId embedded_surface_id(child_support_->frame_sink_id(),
                                embedded_local_surface_id);

  int pass_ids[] = {1, 2, 3};

  std::vector<Quad> embedded_quads[3] = {
      {Quad::SolidColorQuad(1, gfx::Rect(5, 5)),
       Quad::SolidColorQuad(2, gfx::Rect(5, 5))},
      {Quad::SolidColorQuad(3, gfx::Rect(5, 5)),
       Quad::RenderPassQuad(pass_ids[0])},
      {Quad::SolidColorQuad(4, gfx::Rect(5, 5)),
       Quad::RenderPassQuad(pass_ids[1])}};
  std::vector<Pass> embedded_passes = {
      Pass(embedded_quads[0], pass_ids[0], SurfaceSize()),
      Pass(embedded_quads[1], pass_ids[1], SurfaceSize()),
      Pass(embedded_quads[2], pass_ids[2], SurfaceSize())};

  constexpr float device_scale_factor = 1.0f;
  SubmitCompositorFrame(child_support_.get(), embedded_passes,
                        embedded_local_surface_id, device_scale_factor);

  std::vector<Quad> root_quads[3] = {
      {Quad::SolidColorQuad(5, gfx::Rect(5, 5)),
       Quad::SolidColorQuad(6, gfx::Rect(5, 5))},
      {Quad::SurfaceQuad(SurfaceRange(base::nullopt, embedded_surface_id),
                         SK_ColorWHITE, gfx::Rect(5, 5), false),
       Quad::RenderPassQuad(pass_ids[0])},
      {Quad::SolidColorQuad(7, gfx::Rect(5, 5)),
       Quad::RenderPassQuad(pass_ids[1])}};
  std::vector<Pass> root_passes = {
      Pass(root_quads[0], pass_ids[0], SurfaceSize()),
      Pass(root_quads[1], pass_ids[1], SurfaceSize()),
      Pass(root_quads[2], pass_ids[2], SurfaceSize())};

  SubmitCompositorFrame(support_.get(), root_passes, root_local_surface_id_,
                        device_scale_factor);

  SurfaceId root_surface_id(support_->frame_sink_id(), root_local_surface_id_);
  CompositorFrame aggregated_frame =
      aggregator_.Aggregate(root_surface_id, GetNextDisplayTimeAndIncrement());

  const auto& aggregated_pass_list = aggregated_frame.render_pass_list;

  ASSERT_EQ(5u, aggregated_pass_list.size());
  RenderPassId actual_pass_ids[] = {
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
    ASSERT_EQ(DrawQuad::RENDER_PASS,
              third_pass_quad_list.ElementAt(1)->material);
    const auto* third_pass_render_pass_draw_quad =
        RenderPassDrawQuad::MaterialCast(third_pass_quad_list.ElementAt(1));
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
    ASSERT_EQ(DrawQuad::RENDER_PASS,
              fourth_pass_quad_list.ElementAt(1)->material);
    const auto* fourth_pass_first_render_pass_draw_quad =
        RenderPassDrawQuad::MaterialCast(fourth_pass_quad_list.ElementAt(1));
    EXPECT_EQ(actual_pass_ids[2],
              fourth_pass_first_render_pass_draw_quad->render_pass_id);

    // The last quad will be a render pass quad referencing the first pass from
    // the root surface, which is the first pass overall.
    ASSERT_EQ(DrawQuad::RENDER_PASS,
              fourth_pass_quad_list.ElementAt(2)->material);
    const auto* fourth_pass_second_render_pass_draw_quad =
        RenderPassDrawQuad::MaterialCast(fourth_pass_quad_list.ElementAt(2));
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
    ASSERT_EQ(DrawQuad::RENDER_PASS,
              fifth_pass_quad_list.ElementAt(1)->material);
    const auto* fifth_pass_render_pass_draw_quad =
        RenderPassDrawQuad::MaterialCast(fifth_pass_quad_list.ElementAt(1));
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
          SK_ColorWHITE, gfx::Rect(5, 5), false),
      Quad::SolidColorQuad(SK_ColorBLUE, gfx::Rect(5, 5))};
  std::vector<Pass> passes = {Pass(quads, SurfaceSize())};

  constexpr float device_scale_factor = 1.0f;
  SubmitCompositorFrame(support_.get(), passes, root_local_surface_id_,
                        device_scale_factor);

  std::vector<Quad> expected_quads = {
      Quad::SolidColorQuad(SK_ColorGREEN, gfx::Rect(5, 5)),
      Quad::SolidColorQuad(SK_ColorBLUE, gfx::Rect(5, 5))};
  std::vector<Pass> expected_passes = {Pass(expected_quads, SurfaceSize())};
  SurfaceId root_surface_id(support_->frame_sink_id(), root_local_surface_id_);
  AggregateAndVerify(expected_passes, {root_surface_id});
}

// Tests a reference to a valid surface with no submitted frame. A
// SolidColorDrawQuad should be placed in lieu of a frame.
TEST_F(SurfaceAggregatorValidSurfaceTest, ValidSurfaceReferenceWithNoFrame) {
  LocalSurfaceId empty_local_surface_id = allocator_.GenerateId();
  SurfaceId surface_with_no_frame_id(kArbitraryFrameSinkId1,
                                     empty_local_surface_id);

  std::vector<Quad> quads = {
      Quad::SolidColorQuad(SK_ColorGREEN, gfx::Rect(5, 5)),
      Quad::SurfaceQuad(SurfaceRange(base::nullopt, surface_with_no_frame_id),
                        SK_ColorYELLOW, gfx::Rect(5, 5), false),
      Quad::SolidColorQuad(SK_ColorBLUE, gfx::Rect(5, 5))};
  std::vector<Pass> passes = {Pass(quads, SurfaceSize())};

  constexpr float device_scale_factor = 1.0f;
  SubmitCompositorFrame(support_.get(), passes, root_local_surface_id_,
                        device_scale_factor);

  std::vector<Quad> expected_quads = {
      Quad::SolidColorQuad(SK_ColorGREEN, gfx::Rect(5, 5)),
      Quad::SolidColorQuad(SK_ColorYELLOW, gfx::Rect(5, 5)),
      Quad::SolidColorQuad(SK_ColorBLUE, gfx::Rect(5, 5))};
  std::vector<Pass> expected_passes = {Pass(expected_quads, SurfaceSize())};
  SurfaceId root_surface_id(support_->frame_sink_id(), root_local_surface_id_);
  AggregateAndVerify(expected_passes, {root_surface_id});
}

// Tests a reference to a valid primary surface and a fallback surface
// with no submitted frame. A SolidColorDrawQuad should be placed in lieu of a
// frame.
TEST_F(SurfaceAggregatorValidSurfaceTest, ValidFallbackWithNoFrame) {
  const LocalSurfaceId empty_local_surface_id = allocator_.GenerateId();
  const SurfaceId surface_with_no_frame_id(support_->frame_sink_id(),
                                           empty_local_surface_id);

  std::vector<Quad> quads = {
      Quad::SurfaceQuad(SurfaceRange(surface_with_no_frame_id), SK_ColorYELLOW,
                        gfx::Rect(5, 5), false)};
  std::vector<Pass> passes = {Pass(quads, SurfaceSize())};

  constexpr float device_scale_factor = 1.0f;
  SubmitCompositorFrame(support_.get(), passes, root_local_surface_id_,
                        device_scale_factor);

  std::vector<Quad> expected_quads = {
      Quad::SolidColorQuad(SK_ColorYELLOW, gfx::Rect(5, 5)),
  };
  std::vector<Pass> expected_passes = {Pass(expected_quads, SurfaceSize())};
  SurfaceId root_surface_id(support_->frame_sink_id(), root_local_surface_id_);
  AggregateAndVerify(expected_passes, {root_surface_id});
}

// Tests a surface quad referencing itself, generating a trivial cycle.
// The quad creating the cycle should be dropped from the final frame.
TEST_F(SurfaceAggregatorValidSurfaceTest, SimpleCyclicalReference) {
  SurfaceId root_surface_id(support_->frame_sink_id(), root_local_surface_id_);
  std::vector<Quad> quads = {
      Quad::SurfaceQuad(SurfaceRange(base::nullopt, root_surface_id),
                        SK_ColorWHITE, gfx::Rect(5, 5), false),
      Quad::SolidColorQuad(SK_ColorYELLOW, gfx::Rect(5, 5))};
  std::vector<Pass> passes = {Pass(quads, SurfaceSize())};

  constexpr float device_scale_factor = 1.0f;
  SubmitCompositorFrame(support_.get(), passes, root_local_surface_id_,
                        device_scale_factor);

  std::vector<Quad> expected_quads = {
      Quad::SolidColorQuad(SK_ColorYELLOW, gfx::Rect(5, 5))};
  std::vector<Pass> expected_passes = {Pass(expected_quads, SurfaceSize())};
  AggregateAndVerify(expected_passes, {root_surface_id});
}

// Tests a more complex cycle with one intermediate surface.
TEST_F(SurfaceAggregatorValidSurfaceTest, TwoSurfaceCyclicalReference) {
  LocalSurfaceId child_local_surface_id = allocator_.GenerateId();
  SurfaceId child_surface_id(child_support_->frame_sink_id(),
                             child_local_surface_id);

  std::vector<Quad> parent_quads = {
      Quad::SolidColorQuad(SK_ColorBLUE, gfx::Rect(5, 5)),
      Quad::SurfaceQuad(SurfaceRange(base::nullopt, child_surface_id),
                        SK_ColorWHITE, gfx::Rect(5, 5), false),
      Quad::SolidColorQuad(SK_ColorCYAN, gfx::Rect(5, 5))};
  std::vector<Pass> parent_passes = {Pass(parent_quads, SurfaceSize())};

  constexpr float device_scale_factor = 1.0f;
  SubmitCompositorFrame(support_.get(), parent_passes, root_local_surface_id_,
                        device_scale_factor);

  SurfaceId root_surface_id(support_->frame_sink_id(), root_local_surface_id_);
  std::vector<Quad> child_quads = {
      Quad::SolidColorQuad(SK_ColorGREEN, gfx::Rect(5, 5)),
      Quad::SurfaceQuad(SurfaceRange(base::nullopt, root_surface_id),
                        SK_ColorWHITE, gfx::Rect(5, 5), false),
      Quad::SolidColorQuad(SK_ColorMAGENTA, gfx::Rect(5, 5))};
  std::vector<Pass> child_passes = {Pass(child_quads, SurfaceSize())};

  SubmitCompositorFrame(child_support_.get(), child_passes,
                        child_local_surface_id, device_scale_factor);

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
// namespace and update RenderPassDrawQuad's id references to match.
TEST_F(SurfaceAggregatorValidSurfaceTest, RenderPassIdMapping) {
  LocalSurfaceId child_local_surface_id = allocator_.GenerateId();
  SurfaceId child_surface_id(child_support_->frame_sink_id(),
                             child_local_surface_id);

  RenderPassId child_pass_id[] = {1u, 2u};
  std::vector<Quad> child_quad[2] = {
      {Quad::SolidColorQuad(SK_ColorGREEN, gfx::Rect(5, 5))},
      {Quad::RenderPassQuad(child_pass_id[0])}};
  std::vector<Pass> surface_passes = {
      Pass(child_quad[0], child_pass_id[0], SurfaceSize()),
      Pass(child_quad[1], child_pass_id[1], SurfaceSize())};

  constexpr float device_scale_factor = 1.0f;
  SubmitCompositorFrame(child_support_.get(), surface_passes,
                        child_local_surface_id, device_scale_factor);

  // Pass IDs from the parent surface may collide with ones from the child.
  RenderPassId parent_pass_id[] = {3u, 2u};
  std::vector<Quad> parent_quad[2] = {
      {Quad::SurfaceQuad(SurfaceRange(base::nullopt, child_surface_id),
                         SK_ColorWHITE, gfx::Rect(5, 5), false)},
      {Quad::RenderPassQuad(parent_pass_id[0])}};
  std::vector<Pass> parent_passes = {
      Pass(parent_quad[0], parent_pass_id[0], SurfaceSize()),
      Pass(parent_quad[1], parent_pass_id[1], SurfaceSize())};

  SubmitCompositorFrame(support_.get(), parent_passes, root_local_surface_id_,
                        device_scale_factor);

  SurfaceId root_surface_id(support_->frame_sink_id(), root_local_surface_id_);
  CompositorFrame aggregated_frame =
      aggregator_.Aggregate(root_surface_id, GetNextDisplayTimeAndIncrement());

  const auto& aggregated_pass_list = aggregated_frame.render_pass_list;

  ASSERT_EQ(3u, aggregated_pass_list.size());
  RenderPassId actual_pass_ids[] = {aggregated_pass_list[0]->id,
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
  ASSERT_EQ(render_pass_quads[0]->material, DrawQuad::RENDER_PASS);
  EXPECT_EQ(
      actual_pass_ids[0],
      RenderPassDrawQuad::MaterialCast(render_pass_quads[0])->render_pass_id);

  ASSERT_EQ(render_pass_quads[1]->material, DrawQuad::RENDER_PASS);
  EXPECT_EQ(
      actual_pass_ids[1],
      RenderPassDrawQuad::MaterialCast(render_pass_quads[1])->render_pass_id);
}

void AddSolidColorQuadWithBlendMode(const gfx::Size& size,
                                    RenderPass* pass,
                                    const SkBlendMode blend_mode) {
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
              clip_rect, is_clipped, are_contents_opaque, opacity, blend_mode,
              0);

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
  auto grandchild_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryFrameSinkId1, kChildIsRoot,
      kNeedsSyncPoints);
  auto child_one_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryFrameSinkId2, kChildIsRoot,
      kNeedsSyncPoints);
  auto child_two_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryFrameSinkId3, kChildIsRoot,
      kNeedsSyncPoints);
  int pass_id = 1;
  LocalSurfaceId grandchild_local_surface_id = allocator_.GenerateId();
  SurfaceId grandchild_surface_id(grandchild_support->frame_sink_id(),
                                  grandchild_local_surface_id);

  auto grandchild_pass = RenderPass::Create();
  constexpr float device_scale_factor = 1.0f;
  gfx::Rect output_rect(SurfaceSize());
  gfx::Rect damage_rect(SurfaceSize());
  gfx::Transform transform_to_root_target;
  grandchild_pass->SetNew(pass_id, output_rect, damage_rect,
                          transform_to_root_target);
  AddSolidColorQuadWithBlendMode(SurfaceSize(), grandchild_pass.get(),
                                 blend_modes[2]);
  QueuePassAsFrame(std::move(grandchild_pass), grandchild_local_surface_id,
                   device_scale_factor, grandchild_support.get());

  LocalSurfaceId child_one_local_surface_id = allocator_.GenerateId();
  SurfaceId child_one_surface_id(child_one_support->frame_sink_id(),
                                 child_one_local_surface_id);

  auto child_one_pass = RenderPass::Create();
  child_one_pass->SetNew(pass_id, output_rect, damage_rect,
                         transform_to_root_target);
  AddSolidColorQuadWithBlendMode(SurfaceSize(), child_one_pass.get(),
                                 blend_modes[1]);
  auto* grandchild_surface_quad =
      child_one_pass->CreateAndAppendDrawQuad<SurfaceDrawQuad>();
  grandchild_surface_quad->SetNew(
      child_one_pass->shared_quad_state_list.back(), gfx::Rect(SurfaceSize()),
      gfx::Rect(SurfaceSize()),
      SurfaceRange(base::nullopt, grandchild_surface_id), SK_ColorWHITE, false);
  AddSolidColorQuadWithBlendMode(SurfaceSize(), child_one_pass.get(),
                                 blend_modes[3]);
  QueuePassAsFrame(std::move(child_one_pass), child_one_local_surface_id,
                   device_scale_factor, child_one_support.get());

  LocalSurfaceId child_two_local_surface_id = allocator_.GenerateId();
  SurfaceId child_two_surface_id(child_two_support->frame_sink_id(),
                                 child_two_local_surface_id);

  auto child_two_pass = RenderPass::Create();
  child_two_pass->SetNew(pass_id, output_rect, damage_rect,
                         transform_to_root_target);
  AddSolidColorQuadWithBlendMode(SurfaceSize(), child_two_pass.get(),
                                 blend_modes[5]);
  QueuePassAsFrame(std::move(child_two_pass), child_two_local_surface_id,
                   device_scale_factor, child_two_support.get());

  auto root_pass = RenderPass::Create();
  root_pass->SetNew(pass_id, output_rect, damage_rect,
                    transform_to_root_target);

  AddSolidColorQuadWithBlendMode(SurfaceSize(), root_pass.get(),
                                 blend_modes[0]);
  auto* child_one_surface_quad =
      root_pass->CreateAndAppendDrawQuad<SurfaceDrawQuad>();
  child_one_surface_quad->SetNew(
      root_pass->shared_quad_state_list.back(), gfx::Rect(SurfaceSize()),
      gfx::Rect(SurfaceSize()),
      SurfaceRange(base::nullopt, child_one_surface_id), SK_ColorWHITE, false);
  AddSolidColorQuadWithBlendMode(SurfaceSize(), root_pass.get(),
                                 blend_modes[4]);
  auto* child_two_surface_quad =
      root_pass->CreateAndAppendDrawQuad<SurfaceDrawQuad>();
  child_two_surface_quad->SetNew(
      root_pass->shared_quad_state_list.back(), gfx::Rect(SurfaceSize()),
      gfx::Rect(SurfaceSize()),
      SurfaceRange(base::nullopt, child_two_surface_id), SK_ColorWHITE, false);
  AddSolidColorQuadWithBlendMode(SurfaceSize(), root_pass.get(),
                                 blend_modes[6]);

  QueuePassAsFrame(std::move(root_pass), root_local_surface_id_,
                   device_scale_factor, support_.get());

  SurfaceId root_surface_id(support_->frame_sink_id(), root_local_surface_id_);
  CompositorFrame aggregated_frame =
      aggregator_.Aggregate(root_surface_id, GetNextDisplayTimeAndIncrement());

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
      nullptr, &manager_, kArbitraryMiddleFrameSinkId, kChildIsRoot,
      kNeedsSyncPoints);
  // Innermost child surface.
  LocalSurfaceId child_local_surface_id = allocator_.GenerateId();
  SurfaceId child_surface_id(child_support_->frame_sink_id(),
                             child_local_surface_id);
  {
    int child_pass_id[] = {1, 2};
    std::vector<Quad> child_quads[2] = {
        {Quad::SolidColorQuad(SK_ColorGREEN, gfx::Rect(5, 5))},
        {Quad::RenderPassQuad(child_pass_id[0])},
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

    child_support_->SubmitCompositorFrame(child_local_surface_id,
                                          std::move(child_frame));
  }

  // Middle child surface.
  LocalSurfaceId middle_local_surface_id = allocator_.GenerateId();
  SurfaceId middle_surface_id(middle_support->frame_sink_id(),
                              middle_local_surface_id);
  {
    std::vector<Quad> middle_quads = {
        Quad::SurfaceQuad(SurfaceRange(base::nullopt, child_surface_id),
                          SK_ColorWHITE, gfx::Rect(5, 5), false)};
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
                        SK_ColorWHITE, gfx::Rect(5, 5), false)};
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

  support_->SubmitCompositorFrame(root_local_surface_id_,
                                  std::move(root_frame));

  SurfaceId root_surface_id(support_->frame_sink_id(), root_local_surface_id_);
  CompositorFrame aggregated_frame =
      aggregator_.Aggregate(root_surface_id, GetNextDisplayTimeAndIncrement());

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

// Tests that damage rects are aggregated correctly when surfaces change.
TEST_F(SurfaceAggregatorValidSurfaceTest, AggregateDamageRect) {
  // Add a callback for when the surface is damaged.
  MockAggregatedDamageCallback aggregated_damage_callback;
  support_->SetAggregatedDamageCallbackForTesting(
      aggregated_damage_callback.GetCallback());

  auto parent_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryMiddleFrameSinkId, kChildIsRoot,
      kNeedsSyncPoints);
  std::vector<Quad> child_quads = {Quad::RenderPassQuad(1)};
  std::vector<Pass> child_passes = {Pass(child_quads, 1, SurfaceSize())};

  CompositorFrame child_frame = MakeEmptyCompositorFrame();
  AddPasses(&child_frame.render_pass_list, child_passes,
            &child_frame.metadata.referenced_surfaces);

  auto* child_root_pass = child_frame.render_pass_list[0].get();
  auto* child_root_pass_sqs = child_root_pass->shared_quad_state_list.front();
  child_root_pass_sqs->quad_to_target_transform.Translate(8, 0);

  LocalSurfaceId child_local_surface_id = allocator_.GenerateId();
  SurfaceId child_surface_id(child_support_->frame_sink_id(),
                             child_local_surface_id);
  child_support_->SubmitCompositorFrame(child_local_surface_id,
                                        std::move(child_frame));

  std::vector<Quad> parent_surface_quads = {
      Quad::SurfaceQuad(SurfaceRange(base::nullopt, child_surface_id),
                        SK_ColorWHITE, gfx::Rect(5, 5), false)};
  std::vector<Pass> parent_surface_passes = {
      Pass(parent_surface_quads, 1, SurfaceSize())};

  // Parent surface is only used to test if the transform is applied correctly
  // to the child surface's damage.
  CompositorFrame parent_surface_frame = MakeEmptyCompositorFrame();
  AddPasses(&parent_surface_frame.render_pass_list, parent_surface_passes,
            &parent_surface_frame.metadata.referenced_surfaces);

  LocalSurfaceId parent_local_surface_id = allocator_.GenerateId();
  SurfaceId parent_surface_id(parent_support->frame_sink_id(),
                              parent_local_surface_id);
  parent_support->SubmitCompositorFrame(parent_local_surface_id,
                                        std::move(parent_surface_frame));

  std::vector<Quad> root_surface_quads = {
      Quad::SurfaceQuad(SurfaceRange(base::nullopt, parent_surface_id),
                        SK_ColorWHITE, gfx::Rect(5, 5), false)};
  std::vector<Quad> root_render_pass_quads = {Quad::RenderPassQuad(1)};

  std::vector<Pass> root_passes = {
      Pass(root_surface_quads, 1, SurfaceSize()),
      Pass(root_render_pass_quads, 2, SurfaceSize())};

  CompositorFrame root_frame = MakeEmptyCompositorFrame();
  AddPasses(&root_frame.render_pass_list, root_passes,
            &root_frame.metadata.referenced_surfaces);

  root_frame.render_pass_list[0]
      ->shared_quad_state_list.front()
      ->quad_to_target_transform.Translate(0, 10);
  root_frame.render_pass_list[0]->damage_rect = gfx::Rect(5, 5, 10, 10);
  root_frame.render_pass_list[1]->damage_rect = gfx::Rect(5, 5, 100, 100);

  support_->SubmitCompositorFrame(root_local_surface_id_,
                                  std::move(root_frame));

  // Damage rect for first aggregation should contain entire root surface. The
  // damage rect reported to the callback is actually 10 pixels taller because
  // of the 10-pixel vertical translation of the first RenderPass.
  SurfaceId root_surface_id(support_->frame_sink_id(), root_local_surface_id_);
  EXPECT_CALL(
      aggregated_damage_callback,
      OnAggregatedDamage(root_local_surface_id_, SurfaceSize(),
                         gfx::Rect(0, 0, 100, 110), next_display_time()));
  CompositorFrame aggregated_frame =
      aggregator_.Aggregate(root_surface_id, GetNextDisplayTimeAndIncrement());
  testing::Mock::VerifyAndClearExpectations(&aggregated_damage_callback);
  const auto& aggregated_pass_list = aggregated_frame.render_pass_list;
  ASSERT_EQ(2u, aggregated_pass_list.size());
  EXPECT_EQ(gfx::Rect(SurfaceSize()), aggregated_pass_list[1]->damage_rect);

  {
    CompositorFrame child_frame = MakeEmptyCompositorFrame();
    AddPasses(&child_frame.render_pass_list, child_passes,
              &child_frame.metadata.referenced_surfaces);

    auto* child_root_pass = child_frame.render_pass_list[0].get();
    auto* child_root_pass_sqs = child_root_pass->shared_quad_state_list.front();
    child_root_pass_sqs->quad_to_target_transform.Translate(8, 0);
    child_root_pass->damage_rect = gfx::Rect(10, 10, 10, 10);

    child_support_->SubmitCompositorFrame(child_local_surface_id,
                                          std::move(child_frame));

    // Outer surface didn't change, so a transformed inner damage rect is
    // expected.
    SurfaceId root_surface_id(support_->frame_sink_id(),
                              root_local_surface_id_);
    const gfx::Rect expected_damage_rect(10, 20, 10, 10);
    EXPECT_CALL(aggregated_damage_callback,
                OnAggregatedDamage(root_local_surface_id_, SurfaceSize(),
                                   expected_damage_rect, next_display_time()));
    CompositorFrame aggregated_frame = aggregator_.Aggregate(
        root_surface_id, GetNextDisplayTimeAndIncrement());
    const auto& aggregated_pass_list = aggregated_frame.render_pass_list;
    ASSERT_EQ(2u, aggregated_pass_list.size());
    EXPECT_EQ(expected_damage_rect.ToString(),
              aggregated_pass_list[1]->damage_rect.ToString());
  }

  {
    CompositorFrame root_frame = MakeEmptyCompositorFrame();
    AddPasses(&root_frame.render_pass_list, root_passes,
              &root_frame.metadata.referenced_surfaces);

    root_frame.render_pass_list[0]
        ->shared_quad_state_list.front()
        ->quad_to_target_transform.Translate(0, 10);
    root_frame.render_pass_list[0]->damage_rect = gfx::Rect(0, 0, 1, 1);

    support_->SubmitCompositorFrame(root_local_surface_id_,
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

    support_->SubmitCompositorFrame(root_local_surface_id_,
                                    std::move(root_frame));

    // The root surface was enqueued without being aggregated once, so it should
    // be treated as completely damaged.
    SurfaceId root_surface_id(support_->frame_sink_id(),
                              root_local_surface_id_);
    EXPECT_CALL(
        aggregated_damage_callback,
        OnAggregatedDamage(root_local_surface_id_, SurfaceSize(),
                           gfx::Rect(SurfaceSize()), next_display_time()));
    CompositorFrame aggregated_frame = aggregator_.Aggregate(
        root_surface_id, GetNextDisplayTimeAndIncrement());
    const auto& aggregated_pass_list = aggregated_frame.render_pass_list;
    ASSERT_EQ(2u, aggregated_pass_list.size());
    EXPECT_EQ(gfx::Rect(SurfaceSize()), aggregated_pass_list[1]->damage_rect);
  }

  // No Surface changed, so no damage should be given.
  {
    SurfaceId root_surface_id(support_->frame_sink_id(),
                              root_local_surface_id_);
    EXPECT_CALL(aggregated_damage_callback, OnAggregatedDamage(_, _, _, _))
        .Times(0);
    CompositorFrame aggregated_frame = aggregator_.Aggregate(
        root_surface_id, GetNextDisplayTimeAndIncrement());
    const auto& aggregated_pass_list = aggregated_frame.render_pass_list;
    ASSERT_EQ(2u, aggregated_pass_list.size());
    EXPECT_TRUE(aggregated_pass_list[1]->damage_rect.IsEmpty());
  }

  // SetFullDamageRectForSurface should cause the entire output to be
  // marked as damaged.
  {
    aggregator_.SetFullDamageForSurface(root_surface_id);
    EXPECT_CALL(
        aggregated_damage_callback,
        OnAggregatedDamage(root_local_surface_id_, SurfaceSize(),
                           gfx::Rect(SurfaceSize()), next_display_time()));
    CompositorFrame aggregated_frame = aggregator_.Aggregate(
        root_surface_id, GetNextDisplayTimeAndIncrement());
    const auto& aggregated_pass_list = aggregated_frame.render_pass_list;
    ASSERT_EQ(2u, aggregated_pass_list.size());
    EXPECT_TRUE(aggregated_pass_list[1]->damage_rect.Contains(
        gfx::Rect(SurfaceSize())));
  }
}

// Tests that damage rects are aggregated correctly when surfaces stretch to
// fit and device size is less than 1.
TEST_F(SurfaceAggregatorValidSurfaceTest, AggregateDamageRectWithSquashToFit) {
  // Add a callback for when the surface is damaged.
  MockAggregatedDamageCallback aggregated_damage_callback;
  support_->SetAggregatedDamageCallbackForTesting(
      aggregated_damage_callback.GetCallback());

  auto parent_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryMiddleFrameSinkId, kChildIsRoot,
      kNeedsSyncPoints);
  std::vector<Quad> child_quads = {Quad::RenderPassQuad(1)};
  std::vector<Pass> child_passes = {Pass(child_quads, 1, gfx::Size(100, 100))};

  CompositorFrame child_frame = MakeEmptyCompositorFrame();
  AddPasses(&child_frame.render_pass_list, child_passes,
            &child_frame.metadata.referenced_surfaces);

  auto* child_root_pass = child_frame.render_pass_list[0].get();
  auto* child_root_pass_sqs = child_root_pass->shared_quad_state_list.front();
  child_root_pass_sqs->quad_to_target_transform.Translate(8, 0);

  LocalSurfaceId child_local_surface_id = allocator_.GenerateId();
  SurfaceId child_surface_id(child_support_->frame_sink_id(),
                             child_local_surface_id);
  child_support_->SubmitCompositorFrame(child_local_surface_id,
                                        std::move(child_frame));

  std::vector<Quad> parent_surface_quads = {
      Quad::SurfaceQuad(SurfaceRange(base::nullopt, child_surface_id),
                        SK_ColorWHITE, gfx::Rect(5, 5), false)};
  std::vector<Pass> parent_surface_passes = {
      Pass(parent_surface_quads, 1, SurfaceSize())};

  // Parent surface is only used to test if the transform is applied correctly
  // to the child surface's damage.
  CompositorFrame parent_surface_frame = MakeEmptyCompositorFrame();
  AddPasses(&parent_surface_frame.render_pass_list, parent_surface_passes,
            &parent_surface_frame.metadata.referenced_surfaces);

  LocalSurfaceId parent_local_surface_id = allocator_.GenerateId();
  SurfaceId parent_surface_id(parent_support->frame_sink_id(),
                              parent_local_surface_id);
  parent_support->SubmitCompositorFrame(parent_local_surface_id,
                                        std::move(parent_surface_frame));

  std::vector<Quad> root_surface_quads = {
      Quad::SurfaceQuad(SurfaceRange(base::nullopt, parent_surface_id),
                        SK_ColorWHITE, gfx::Rect(50, 50), true)};
  std::vector<Quad> root_render_pass_quads = {Quad::RenderPassQuad(1)};

  std::vector<Pass> root_passes = {
      Pass(root_surface_quads, 1, SurfaceSize()),
      Pass(root_render_pass_quads, 2, SurfaceSize())};

  SubmitCompositorFrame(support_.get(), root_passes, root_local_surface_id_,
                        1.0f);

  // Damage rect for first aggregation should be exactly the entire root
  // surface.
  SurfaceId root_surface_id(support_->frame_sink_id(), root_local_surface_id_);
  EXPECT_CALL(
      aggregated_damage_callback,
      OnAggregatedDamage(root_local_surface_id_, SurfaceSize(),
                         gfx::Rect(SurfaceSize()), next_display_time()));
  CompositorFrame aggregated_frame =
      aggregator_.Aggregate(root_surface_id, GetNextDisplayTimeAndIncrement());
  testing::Mock::VerifyAndClearExpectations(&aggregated_damage_callback);
  const auto& aggregated_pass_list = aggregated_frame.render_pass_list;
  ASSERT_EQ(2u, aggregated_pass_list.size());
  EXPECT_EQ(gfx::Rect(SurfaceSize()), aggregated_pass_list[1]->damage_rect);

  {
    CompositorFrame child_frame = MakeEmptyCompositorFrame();
    AddPasses(&child_frame.render_pass_list, child_passes,
              &child_frame.metadata.referenced_surfaces);

    auto* child_root_pass = child_frame.render_pass_list[0].get();
    child_root_pass->damage_rect = gfx::Rect(10, 20, 20, 30);

    child_support_->SubmitCompositorFrame(child_local_surface_id,
                                          std::move(child_frame));

    // Outer surface didn't change, so transformed inner damage rect should be
    // used. Since the child surface is stretching to fit the outer surface
    // which is half the size, we end up with a damage rect that is half the
    // size of the child surface.
    SurfaceId root_surface_id(support_->frame_sink_id(),
                              root_local_surface_id_);
    const gfx::Rect expected_damage_rect(5, 10, 10, 15);
    EXPECT_CALL(aggregated_damage_callback,
                OnAggregatedDamage(root_local_surface_id_, SurfaceSize(),
                                   expected_damage_rect, next_display_time()));
    CompositorFrame aggregated_frame = aggregator_.Aggregate(
        root_surface_id, GetNextDisplayTimeAndIncrement());
    const auto& aggregated_pass_list = aggregated_frame.render_pass_list;
    ASSERT_EQ(2u, aggregated_pass_list.size());
    EXPECT_EQ(expected_damage_rect.ToString(),
              aggregated_pass_list[1]->damage_rect.ToString());
  }
}

// Tests that damage rects are aggregated correctly when surfaces stretch to
// fit and device size is greater than 1.
TEST_F(SurfaceAggregatorValidSurfaceTest, AggregateDamageRectWithStretchToFit) {
  // Add a callback for when the surface is damaged.
  MockAggregatedDamageCallback aggregated_damage_callback;
  support_->SetAggregatedDamageCallbackForTesting(
      aggregated_damage_callback.GetCallback());

  auto parent_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryMiddleFrameSinkId, kChildIsRoot,
      kNeedsSyncPoints);
  std::vector<Quad> child_quads = {Quad::RenderPassQuad(1)};
  std::vector<Pass> child_passes = {Pass(child_quads, 1, gfx::Size(100, 100))};

  CompositorFrame child_frame = MakeEmptyCompositorFrame();
  AddPasses(&child_frame.render_pass_list, child_passes,
            &child_frame.metadata.referenced_surfaces);

  auto* child_root_pass = child_frame.render_pass_list[0].get();
  auto* child_root_pass_sqs = child_root_pass->shared_quad_state_list.front();
  child_root_pass_sqs->quad_to_target_transform.Translate(8, 0);

  LocalSurfaceId child_local_surface_id = allocator_.GenerateId();
  SurfaceId child_surface_id(child_support_->frame_sink_id(),
                             child_local_surface_id);
  child_support_->SubmitCompositorFrame(child_local_surface_id,
                                        std::move(child_frame));

  std::vector<Quad> parent_surface_quads = {
      Quad::SurfaceQuad(SurfaceRange(base::nullopt, child_surface_id),
                        SK_ColorWHITE, gfx::Rect(5, 5), false)};
  std::vector<Pass> parent_surface_passes = {
      Pass(parent_surface_quads, 1, SurfaceSize())};

  // Parent surface is only used to test if the transform is applied correctly
  // to the child surface's damage.
  CompositorFrame parent_surface_frame = MakeEmptyCompositorFrame();
  AddPasses(&parent_surface_frame.render_pass_list, parent_surface_passes,
            &parent_surface_frame.metadata.referenced_surfaces);

  LocalSurfaceId parent_local_surface_id = allocator_.GenerateId();
  SurfaceId parent_surface_id(parent_support->frame_sink_id(),
                              parent_local_surface_id);
  parent_support->SubmitCompositorFrame(parent_local_surface_id,
                                        std::move(parent_surface_frame));

  std::vector<Quad> root_surface_quads = {
      Quad::SurfaceQuad(SurfaceRange(base::nullopt, parent_surface_id),
                        SK_ColorWHITE, gfx::Rect(200, 200), true)};
  std::vector<Quad> root_render_pass_quads = {Quad::RenderPassQuad(1)};

  std::vector<Pass> root_passes = {
      Pass(root_surface_quads, 1, SurfaceSize()),
      Pass(root_render_pass_quads, 2, SurfaceSize())};

  SubmitCompositorFrame(support_.get(), root_passes, root_local_surface_id_,
                        1.0f);

  // Damage rect for first aggregation should contain entire root surface. The
  // damage rect reported to the callback is actually 200x200, larger than the
  // root surface size, because the root's Quad is 200x200.
  SurfaceId root_surface_id(support_->frame_sink_id(), root_local_surface_id_);
  EXPECT_CALL(
      aggregated_damage_callback,
      OnAggregatedDamage(root_local_surface_id_, SurfaceSize(),
                         gfx::Rect(0, 0, 200, 200), next_display_time()));
  CompositorFrame aggregated_frame =
      aggregator_.Aggregate(root_surface_id, GetNextDisplayTimeAndIncrement());
  testing::Mock::VerifyAndClearExpectations(&aggregated_damage_callback);
  const auto& aggregated_pass_list = aggregated_frame.render_pass_list;
  ASSERT_EQ(2u, aggregated_pass_list.size());
  EXPECT_EQ(gfx::Rect(SurfaceSize()), aggregated_pass_list[1]->damage_rect);

  {
    CompositorFrame child_frame = MakeEmptyCompositorFrame();
    AddPasses(&child_frame.render_pass_list, child_passes,
              &child_frame.metadata.referenced_surfaces);

    auto* child_root_pass = child_frame.render_pass_list[0].get();
    child_root_pass->damage_rect = gfx::Rect(10, 15, 20, 30);

    child_support_->SubmitCompositorFrame(child_local_surface_id,
                                          std::move(child_frame));

    // Outer surface didn't change, so transformed inner damage rect should be
    // used. Since the child surface is stretching to fit the outer surface
    // which is twice the size, we end up with a damage rect that is double the
    // size of the child surface.
    SurfaceId root_surface_id(support_->frame_sink_id(),
                              root_local_surface_id_);
    const gfx::Rect expected_damage_rect(20, 30, 40, 60);
    EXPECT_CALL(aggregated_damage_callback,
                OnAggregatedDamage(root_local_surface_id_, SurfaceSize(),
                                   expected_damage_rect, next_display_time()));
    CompositorFrame aggregated_frame = aggregator_.Aggregate(
        root_surface_id, GetNextDisplayTimeAndIncrement());
    const auto& aggregated_pass_list = aggregated_frame.render_pass_list;
    ASSERT_EQ(2u, aggregated_pass_list.size());
    EXPECT_EQ(expected_damage_rect.ToString(),
              aggregated_pass_list[1]->damage_rect.ToString());
  }
}

// Check that damage is correctly calculated for surfaces.
TEST_F(SurfaceAggregatorValidSurfaceTest, SwitchSurfaceDamage) {
  std::vector<Quad> root_render_pass_quads = {
      Quad::SolidColorQuad(1, gfx::Rect(5, 5))};

  std::vector<Pass> root_passes = {
      Pass(root_render_pass_quads, 2, SurfaceSize())};

  CompositorFrame root_frame = MakeEmptyCompositorFrame();
  AddPasses(&root_frame.render_pass_list, root_passes,
            &root_frame.metadata.referenced_surfaces);

  root_frame.render_pass_list[0]->damage_rect = gfx::Rect(5, 5, 100, 100);

  support_->SubmitCompositorFrame(root_local_surface_id_,
                                  std::move(root_frame));

  {
    SurfaceId root_surface_id(support_->frame_sink_id(),
                              root_local_surface_id_);
    CompositorFrame aggregated_frame = aggregator_.Aggregate(
        root_surface_id, GetNextDisplayTimeAndIncrement());

    const auto& aggregated_pass_list = aggregated_frame.render_pass_list;

    ASSERT_EQ(1u, aggregated_pass_list.size());

    // Damage rect for first aggregation should contain entire root surface.
    EXPECT_TRUE(aggregated_pass_list[0]->damage_rect.Contains(
        gfx::Rect(SurfaceSize())));
  }

  LocalSurfaceId second_root_local_surface_id = allocator_.GenerateId();
  SurfaceId second_root_surface_id(support_->frame_sink_id(),
                                   second_root_local_surface_id);
  {
    std::vector<Quad> root_render_pass_quads = {
        Quad::SolidColorQuad(1, gfx::Rect(5, 5))};

    std::vector<Pass> root_passes = {
        Pass(root_render_pass_quads, 2, SurfaceSize())};

    CompositorFrame root_frame = MakeEmptyCompositorFrame();
    AddPasses(&root_frame.render_pass_list, root_passes,
              &root_frame.metadata.referenced_surfaces);

    root_frame.render_pass_list[0]->damage_rect = gfx::Rect(1, 2, 3, 4);

    support_->SubmitCompositorFrame(second_root_local_surface_id,
                                    std::move(root_frame));
  }
  {
    CompositorFrame aggregated_frame = aggregator_.Aggregate(
        second_root_surface_id, GetNextDisplayTimeAndIncrement());

    const auto& aggregated_pass_list = aggregated_frame.render_pass_list;

    ASSERT_EQ(1u, aggregated_pass_list.size());

    EXPECT_EQ(gfx::Rect(1, 2, 3, 4), aggregated_pass_list[0]->damage_rect);
  }
  {
    CompositorFrame aggregated_frame = aggregator_.Aggregate(
        second_root_surface_id, GetNextDisplayTimeAndIncrement());

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
      nullptr, &manager_, kArbitraryFrameSinkId1, kRootIsRoot,
      kNeedsSyncPoints);
  LocalSurfaceId id1 = allocator_.GenerateId();
  LocalSurfaceId id2 = allocator_.GenerateId();
  LocalSurfaceId id3 = allocator_.GenerateId();
  LocalSurfaceId id4 = allocator_.GenerateId();
  LocalSurfaceId id5 = allocator_.GenerateId();
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
  support_->SubmitCompositorFrame(root_local_surface_id_, std::move(frame));

  SurfaceId root_surface_id(support_->frame_sink_id(), root_local_surface_id_);
  CompositorFrame aggregated_frame =
      aggregator_.Aggregate(root_surface_id, GetNextDisplayTimeAndIncrement());

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
      nullptr, &manager_, kArbitraryFrameSinkId1, kRootIsRoot,
      kNeedsSyncPoints);
  LocalSurfaceId id1 = allocator_.GenerateId();
  LocalSurfaceId id2 = allocator_.GenerateId();
  LocalSurfaceId id3 = allocator_.GenerateId();
  LocalSurfaceId id4 = allocator_.GenerateId();
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
  support_->SubmitCompositorFrame(root_local_surface_id_, std::move(frame));

  SurfaceId root_surface_id(support_->frame_sink_id(), root_local_surface_id_);
  CompositorFrame aggregated_frame =
      aggregator_.Aggregate(root_surface_id, GetNextDisplayTimeAndIncrement());

  // |id1| is before the fallback id so it shouldn't damage the display.
  EXPECT_FALSE(aggregator_.NotifySurfaceDamageAndCheckForDisplayDamage(
      SurfaceId(kArbitraryFrameSinkId1, id1)));

  // |id2| is the fallback id so it should damage the display.
  EXPECT_TRUE(aggregator_.NotifySurfaceDamageAndCheckForDisplayDamage(
      SurfaceId(kArbitraryFrameSinkId1, id2)));

  // |id3| is after the fallback and primary has a different FrameSinkId so it
  // should damage the display.
  EXPECT_TRUE(aggregator_.NotifySurfaceDamageAndCheckForDisplayDamage(
      SurfaceId(kArbitraryFrameSinkId1, id3)));

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
  LocalSurfaceId id1 = allocator_.GenerateId();
  LocalSurfaceId id2 = allocator_.GenerateId();
  LocalSurfaceId id3 = allocator_.GenerateId();
  SurfaceId primary_surface_id(kArbitraryFrameSinkId1, id2);

  CompositorFrame frame = MakeCompositorFrameFromSurfaceRanges(
      {SurfaceRange(base::nullopt, primary_surface_id)});
  support_->SubmitCompositorFrame(root_local_surface_id_, std::move(frame));

  SurfaceId root_surface_id(support_->frame_sink_id(), root_local_surface_id_);
  CompositorFrame aggregated_frame =
      aggregator_.Aggregate(root_surface_id, GetNextDisplayTimeAndIncrement());

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
      SurfaceId(kArbitraryFrameSinkId3, id2)));
}

// Verifies that when primary and fallback ids are equal, only damage to that
// particular surface causee damage to display.
TEST_F(SurfaceAggregatorValidSurfaceTest,
       SurfaceDamagePrimaryAndFallbackEqual) {
  auto embedded_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryFrameSinkId1, kRootIsRoot,
      kNeedsSyncPoints);
  LocalSurfaceId id1 = allocator_.GenerateId();
  LocalSurfaceId id2 = allocator_.GenerateId();
  LocalSurfaceId id3 = allocator_.GenerateId();
  SurfaceId surface_id(kArbitraryFrameSinkId1, id2);

  std::vector<Quad> embedded_quads = {
      Quad::SolidColorQuad(SK_ColorGREEN, gfx::Rect(5, 5)),
      Quad::SolidColorQuad(SK_ColorBLUE, gfx::Rect(5, 5))};
  std::vector<Pass> embedded_passes = {Pass(embedded_quads, SurfaceSize())};
  constexpr float device_scale_factor = 1.0f;
  SubmitCompositorFrame(embedded_support.get(), embedded_passes, id2,
                        device_scale_factor);

  CompositorFrame frame =
      MakeCompositorFrameFromSurfaceRanges({SurfaceRange(surface_id)});
  support_->SubmitCompositorFrame(root_local_surface_id_, std::move(frame));

  SurfaceId root_surface_id(support_->frame_sink_id(), root_local_surface_id_);
  CompositorFrame aggregated_frame =
      aggregator_.Aggregate(root_surface_id, GetNextDisplayTimeAndIncrement());

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
      SurfaceId(kArbitraryFrameSinkId3, id2)));
}

class SurfaceAggregatorPartialSwapTest
    : public SurfaceAggregatorValidSurfaceTest {
 public:
  SurfaceAggregatorPartialSwapTest()
      : SurfaceAggregatorValidSurfaceTest(true) {}
};

// Tests that quads outside the damage rect are ignored.
TEST_F(SurfaceAggregatorPartialSwapTest, IgnoreOutside) {
  LocalSurfaceId child_local_surface_id = allocator_.GenerateId();
  SurfaceId child_surface_id(child_support_->frame_sink_id(),
                             child_local_surface_id);
  constexpr float device_scale_factor = 1.0f;

  // The child surface has three quads, one with a visible rect of 13,13 4x4 and
  // the other other with a visible rect of 10,10 2x2 (relative to root target
  // space), and one with a non-invertible transform.
  {
    int child_pass_id = 1;
    std::vector<Quad> child_quads1 = {Quad::RenderPassQuad(child_pass_id)};
    std::vector<Quad> child_quads2 = {Quad::RenderPassQuad(child_pass_id)};
    std::vector<Quad> child_quads3 = {Quad::RenderPassQuad(child_pass_id)};
    std::vector<Pass> child_passes = {
        Pass(child_quads1, child_pass_id, SurfaceSize()),
        Pass(child_quads2, child_pass_id, SurfaceSize()),
        Pass(child_quads3, child_pass_id, SurfaceSize())};

    RenderPassList child_pass_list;
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

    SubmitPassListAsFrame(child_support_.get(), child_local_surface_id,
                          &child_pass_list, std::move(referenced_surfaces),
                          device_scale_factor);
  }

  {
    std::vector<Quad> root_quads = {
        Quad::SurfaceQuad(SurfaceRange(base::nullopt, child_surface_id),
                          SK_ColorWHITE, gfx::Rect(5, 5), false)};

    std::vector<Pass> root_passes = {Pass(root_quads, SurfaceSize())};

    RenderPassList root_pass_list;
    std::vector<SurfaceRange> referenced_surfaces;
    AddPasses(&root_pass_list, root_passes, &referenced_surfaces);

    auto* root_pass = root_pass_list[0].get();
    root_pass->shared_quad_state_list.front()
        ->quad_to_target_transform.Translate(10, 10);
    root_pass->damage_rect = gfx::Rect(0, 0, 1, 1);

    SubmitPassListAsFrame(support_.get(), root_local_surface_id_,
                          &root_pass_list, std::move(referenced_surfaces),
                          device_scale_factor);
  }

  SurfaceId root_surface_id(support_->frame_sink_id(), root_local_surface_id_);
  CompositorFrame aggregated_frame =
      aggregator_.Aggregate(root_surface_id, GetNextDisplayTimeAndIncrement());

  const auto& aggregated_pass_list = aggregated_frame.render_pass_list;

  ASSERT_EQ(3u, aggregated_pass_list.size());

  // Damage rect for first aggregation should contain entire root surface.
  EXPECT_EQ(gfx::Rect(SurfaceSize()), aggregated_pass_list[2]->damage_rect);
  EXPECT_EQ(1u, aggregated_pass_list[0]->quad_list.size());
  EXPECT_EQ(1u, aggregated_pass_list[1]->quad_list.size());
  EXPECT_EQ(1u, aggregated_pass_list[2]->quad_list.size());

  // Create a root surface with a smaller damage rect.
  {
    std::vector<Quad> root_quads = {
        Quad::SurfaceQuad(SurfaceRange(base::nullopt, child_surface_id),
                          SK_ColorWHITE, gfx::Rect(5, 5), false)};

    std::vector<Pass> root_passes = {Pass(root_quads, SurfaceSize())};

    RenderPassList root_pass_list;
    std::vector<SurfaceRange> referenced_surfaces;
    AddPasses(&root_pass_list, root_passes, &referenced_surfaces);

    auto* root_pass = root_pass_list[0].get();
    root_pass->shared_quad_state_list.front()
        ->quad_to_target_transform.Translate(10, 10);
    root_pass->damage_rect = gfx::Rect(10, 10, 2, 2);
    SubmitPassListAsFrame(support_.get(), root_local_surface_id_,
                          &root_pass_list, std::move(referenced_surfaces),
                          device_scale_factor);
  }

  {
    CompositorFrame aggregated_frame = aggregator_.Aggregate(
        root_surface_id, GetNextDisplayTimeAndIncrement());

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
    int child_pass_ids[] = {1, 2};
    std::vector<Quad> child_quads1 = {Quad::SolidColorQuad(1, gfx::Rect(5, 5))};
    std::vector<Quad> child_quads2 = {Quad::RenderPassQuad(child_pass_ids[0])};
    std::vector<Pass> child_passes = {
        Pass(child_quads1, child_pass_ids[0], SurfaceSize()),
        Pass(child_quads2, child_pass_ids[1], SurfaceSize())};

    RenderPassList child_pass_list;
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
    SubmitPassListAsFrame(child_support_.get(), child_local_surface_id,
                          &child_pass_list, std::move(referenced_surfaces),
                          device_scale_factor);
  }

  {
    CompositorFrame aggregated_frame = aggregator_.Aggregate(
        root_surface_id, GetNextDisplayTimeAndIncrement());

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
    CompositorFrame aggregated_frame = aggregator_.Aggregate(
        root_surface_id, GetNextDisplayTimeAndIncrement());

    const auto& aggregated_pass_list = aggregated_frame.render_pass_list;
    // There were no changes since last aggregation, so output should be empty
    // and have no damage.
    ASSERT_EQ(1u, aggregated_pass_list.size());
    EXPECT_TRUE(aggregated_pass_list[0]->damage_rect.IsEmpty());
    ASSERT_EQ(0u, aggregated_pass_list[0]->quad_list.size());
  }

  // Root surface has smaller damage rect, but filter on render pass means all
  // of it and its descendant passes should be aggregated.
  {
    int root_pass_ids[] = {1, 2, 3};
    std::vector<Quad> root_quads1 = {
        Quad::SurfaceQuad(SurfaceRange(base::nullopt, child_surface_id),
                          SK_ColorWHITE, gfx::Rect(5, 5), false)};
    std::vector<Quad> root_quads2 = {Quad::RenderPassQuad(root_pass_ids[0])};
    std::vector<Quad> root_quads3 = {Quad::RenderPassQuad(root_pass_ids[1])};
    std::vector<Pass> root_passes = {
        Pass(root_quads1, root_pass_ids[0], SurfaceSize()),
        Pass(root_quads2, root_pass_ids[1], SurfaceSize()),
        Pass(root_quads3, root_pass_ids[2], SurfaceSize())};

    RenderPassList root_pass_list;
    std::vector<SurfaceRange> referenced_surfaces;
    AddPasses(&root_pass_list, root_passes, &referenced_surfaces);

    auto* filter_pass = root_pass_list[1].get();
    filter_pass->shared_quad_state_list.front()
        ->quad_to_target_transform.Translate(10, 10);
    auto* root_pass = root_pass_list[2].get();
    filter_pass->filters.Append(cc::FilterOperation::CreateBlurFilter(2));
    root_pass->damage_rect = gfx::Rect(10, 10, 2, 2);
    SubmitPassListAsFrame(support_.get(), root_local_surface_id_,
                          &root_pass_list, std::move(referenced_surfaces),
                          device_scale_factor);
  }

  {
    CompositorFrame aggregated_frame = aggregator_.Aggregate(
        root_surface_id, GetNextDisplayTimeAndIncrement());

    const auto& aggregated_pass_list = aggregated_frame.render_pass_list;

    ASSERT_EQ(4u, aggregated_pass_list.size());

    EXPECT_EQ(gfx::Rect(SurfaceSize()), aggregated_pass_list[0]->damage_rect);
    EXPECT_EQ(gfx::Rect(SurfaceSize()), aggregated_pass_list[1]->damage_rect);
    EXPECT_EQ(gfx::Rect(SurfaceSize()), aggregated_pass_list[2]->damage_rect);
    EXPECT_EQ(gfx::Rect(10, 10, 2, 2), aggregated_pass_list[3]->damage_rect);
    EXPECT_EQ(1u, aggregated_pass_list[0]->quad_list.size());
    EXPECT_EQ(1u, aggregated_pass_list[1]->quad_list.size());
    EXPECT_EQ(1u, aggregated_pass_list[2]->quad_list.size());
    // First render pass draw quad is outside damage rect, so shouldn't be
    // drawn.
    EXPECT_EQ(0u, aggregated_pass_list[3]->quad_list.size());
  }

  // Root surface has smaller damage rect. Opacity filter on render pass
  // means Surface quad under it should be aggregated.
  {
    int root_pass_ids[] = {1, 2};
    std::vector<Quad> root_quads1 = {
        Quad::SolidColorQuad(1, gfx::Rect(5, 5)),
    };
    std::vector<Quad> root_quads2 = {
        Quad::RenderPassQuad(root_pass_ids[0]),
        Quad::SurfaceQuad(SurfaceRange(base::nullopt, child_surface_id),
                          SK_ColorWHITE, gfx::Rect(5, 5), false)};
    std::vector<Pass> root_passes = {
        Pass(root_quads1, root_pass_ids[0], SurfaceSize()),
        Pass(root_quads2, root_pass_ids[1], SurfaceSize())};

    RenderPassList root_pass_list;
    std::vector<SurfaceRange> referenced_surfaces;
    AddPasses(&root_pass_list, root_passes, &referenced_surfaces);

    auto* pass = root_pass_list[0].get();
    auto* root_pass = root_pass_list[1].get();
    root_pass->shared_quad_state_list.ElementAt(1)
        ->quad_to_target_transform.Translate(10, 10);
    pass->backdrop_filters.Append(
        cc::FilterOperation::CreateOpacityFilter(0.5f));
    root_pass->damage_rect = gfx::Rect(10, 10, 2, 2);
    SubmitPassListAsFrame(support_.get(), root_local_surface_id_,
                          &root_pass_list, std::move(referenced_surfaces),
                          device_scale_factor);
  }

  {
    CompositorFrame aggregated_frame = aggregator_.Aggregate(
        root_surface_id, GetNextDisplayTimeAndIncrement());
    const auto& aggregated_pass_list = aggregated_frame.render_pass_list;

    ASSERT_EQ(3u, aggregated_pass_list.size());

    // Pass 0 is solid color quad from root, but outside damage rect.
    EXPECT_EQ(gfx::Rect(10, 10, 2, 2), aggregated_pass_list[0]->damage_rect);
    EXPECT_EQ(0u, aggregated_pass_list[0]->quad_list.size());
    EXPECT_EQ(gfx::Rect(0, 0, 2, 2), aggregated_pass_list[1]->damage_rect);
    EXPECT_EQ(0u, aggregated_pass_list[1]->quad_list.size());

    // First render pass draw quad is outside damage rect, so shouldn't be
    // drawn. SurfaceDrawQuad is after opacity filter, so corresponding
    // RenderPassDrawQuad should be drawn.
    EXPECT_EQ(gfx::Rect(10, 10, 2, 2), aggregated_pass_list[2]->damage_rect);
    EXPECT_EQ(1u, aggregated_pass_list[2]->quad_list.size());
  }

  // Render passes with pixel-moving filters will increase the damage only if
  // the damage of the contents will overlap the render pass.
  {
    int root_pass_ids[] = {1, 2};
    const gfx::Size pass_with_filter_size(5, 5);
    std::vector<Quad> root_quads1 = {
        Quad::SolidColorQuad(1, gfx::Rect(pass_with_filter_size)),
    };
    std::vector<Quad> root_quads2 = {
        Quad::RenderPassQuad(root_pass_ids[0]),
        Quad::SurfaceQuad(SurfaceRange(base::nullopt, child_surface_id),
                          SK_ColorWHITE, gfx::Rect(5, 5), false)};
    std::vector<Pass> root_passes = {
        Pass(root_quads1, root_pass_ids[0], pass_with_filter_size),
        Pass(root_quads2, root_pass_ids[1], SurfaceSize())};

    RenderPassList root_pass_list;
    std::vector<SurfaceRange> referenced_surfaces;
    AddPasses(&root_pass_list, root_passes, &referenced_surfaces);

    auto* pass_with_filter = root_pass_list[0].get();
    auto* root_pass = root_pass_list[1].get();
    root_pass->shared_quad_state_list.ElementAt(1)
        ->quad_to_target_transform.Translate(5, 5);
    pass_with_filter->backdrop_filters.Append(
        cc::FilterOperation::CreateBlurFilter(2));
    // Damage rect intersects with render passes of |pass_with_filter| and
    // |root_pass|.
    root_pass->damage_rect = gfx::Rect(3, 3, 3, 3);
    SubmitPassListAsFrame(support_.get(), root_local_surface_id_,
                          &root_pass_list, std::move(referenced_surfaces),
                          device_scale_factor);
  }

  {
    CompositorFrame aggregated_frame = aggregator_.Aggregate(
        root_surface_id, GetNextDisplayTimeAndIncrement());

    const auto& aggregated_pass_list = aggregated_frame.render_pass_list;

    ASSERT_EQ(3u, aggregated_pass_list.size());

    // Pass 0 has background blur filter and overlaps with damage rect,
    // therefore the whole render pass should be damaged.
    EXPECT_EQ(gfx::Rect(0, 0, 5, 5), aggregated_pass_list[0]->damage_rect);
    EXPECT_EQ(1u, aggregated_pass_list[0]->quad_list.size());
    EXPECT_EQ(gfx::Rect(SurfaceSize()), aggregated_pass_list[1]->damage_rect);
    EXPECT_EQ(1u, aggregated_pass_list[1]->quad_list.size());

    // First render pass draw quad overlaps with damage rect and has background
    // filter, so it should be damaged. SurfaceDrawQuad is after background
    // filter, so corresponding RenderPassDrawQuad should be drawn.
    EXPECT_EQ(gfx::Rect(0, 0, 6, 6), aggregated_pass_list[2]->damage_rect);
    EXPECT_EQ(2u, aggregated_pass_list[2]->quad_list.size());
  }

  // If the render pass with background filters does not intersect the damage
  // rect, the damage won't be expanded to cover the render pass.
  {
    int root_pass_ids[] = {1, 2};
    const gfx::Size pass_with_filter_size(5, 5);
    std::vector<Quad> root_quads1 = {
        Quad::SolidColorQuad(1, gfx::Rect(pass_with_filter_size)),
    };
    std::vector<Quad> root_quads2 = {
        Quad::RenderPassQuad(root_pass_ids[0]),
        Quad::SurfaceQuad(SurfaceRange(base::nullopt, child_surface_id),
                          SK_ColorWHITE, gfx::Rect(5, 5), false)};
    std::vector<Pass> root_passes = {
        Pass(root_quads1, root_pass_ids[0], pass_with_filter_size),
        Pass(root_quads2, root_pass_ids[1], SurfaceSize())};

    RenderPassList root_pass_list;
    std::vector<SurfaceRange> referenced_surfaces;
    AddPasses(&root_pass_list, root_passes, &referenced_surfaces);

    auto* pass_with_filter = root_pass_list[0].get();
    auto* root_pass = root_pass_list[1].get();
    root_pass->shared_quad_state_list.ElementAt(1)
        ->quad_to_target_transform.Translate(5, 5);
    pass_with_filter->backdrop_filters.Append(
        cc::FilterOperation::CreateBlurFilter(2));
    // Damage rect does not intersect with render pass.
    root_pass->damage_rect = gfx::Rect(6, 6, 3, 3);
    SubmitPassListAsFrame(support_.get(), root_local_surface_id_,
                          &root_pass_list, std::move(referenced_surfaces),
                          device_scale_factor);
  }

  {
    CompositorFrame aggregated_frame = aggregator_.Aggregate(
        root_surface_id, GetNextDisplayTimeAndIncrement());

    const auto& aggregated_pass_list = aggregated_frame.render_pass_list;

    ASSERT_EQ(3u, aggregated_pass_list.size());

    // Pass 0 has background blur filter but does NOT overlap with damage rect.
    EXPECT_EQ(gfx::Rect(), aggregated_pass_list[0]->damage_rect);
    EXPECT_EQ(0u, aggregated_pass_list[0]->quad_list.size());
    EXPECT_EQ(gfx::Rect(SurfaceSize()), aggregated_pass_list[1]->damage_rect);
    EXPECT_EQ(1u, aggregated_pass_list[1]->quad_list.size());

    // First render pass draw quad is outside damage rect, so shouldn't be
    // drawn. SurfaceDrawQuad is after background filter, so corresponding
    // RenderPassDrawQuad should be drawn.
    EXPECT_EQ(gfx::Rect(6, 6, 3, 3), aggregated_pass_list[2]->damage_rect);
    EXPECT_EQ(1u, aggregated_pass_list[2]->quad_list.size());
  }
}

class SurfaceAggregatorWithResourcesTest : public testing::Test,
                                           public DisplayTimeSource {
 public:
  SurfaceAggregatorWithResourcesTest() : manager_(&shared_bitmap_manager_) {}

  void SetUp() override {
    resource_provider_ = std::make_unique<DisplayResourceProvider>(
        DisplayResourceProvider::kSoftware, nullptr, &shared_bitmap_manager_);

    aggregator_ = std::make_unique<SurfaceAggregator>(
        manager_.surface_manager(), resource_provider_.get(), false);
    aggregator_->set_output_is_secure(true);
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
  auto pass = RenderPass::Create();
  pass->SetNew(1, gfx::Rect(0, 0, 20, 20), gfx::Rect(), gfx::Transform());
  auto* sqs = pass->CreateAndAppendSharedQuadState();
  sqs->opacity = 1.f;
  if (child_id.is_valid()) {
    auto* surface_quad = pass->CreateAndAppendDrawQuad<SurfaceDrawQuad>();
    surface_quad->SetNew(sqs, gfx::Rect(0, 0, 1, 1), gfx::Rect(0, 0, 1, 1),
                         SurfaceRange(base::nullopt, child_id), SK_ColorWHITE,
                         false);
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
    quad->SetAll(sqs, rect, visible_rect, needs_blending, resource_ids[i],
                 gfx::Size(), premultiplied_alpha, uv_top_left, uv_bottom_right,
                 background_color, vertex_opacity, flipped, nearest_neighbor,
                 secure_output_only);
  }
  frame.render_pass_list.push_back(std::move(pass));
  support->SubmitCompositorFrame(surface_id.local_surface_id(),
                                 std::move(frame));
}

TEST_F(SurfaceAggregatorWithResourcesTest, TakeResourcesOneSurface) {
  FakeCompositorFrameSinkClient client;
  auto support = std::make_unique<CompositorFrameSinkSupport>(
      &client, &manager_, kArbitraryRootFrameSinkId, kRootIsRoot,
      kNeedsSyncPoints);
  LocalSurfaceId local_surface_id(7u, base::UnguessableToken::Create());
  SurfaceId surface_id(support->frame_sink_id(), local_surface_id);

  std::vector<ResourceId> ids = {11, 12, 13};
  SubmitCompositorFrameWithResources(ids, true, SurfaceId(), support.get(),
                                     surface_id);

  CompositorFrame frame =
      aggregator_->Aggregate(surface_id, GetNextDisplayTimeAndIncrement());

  // Nothing should be available to be returned yet.
  EXPECT_TRUE(client.returned_resources().empty());

  SubmitCompositorFrameWithResources({}, true, SurfaceId(), support.get(),
                                     surface_id);

  frame = aggregator_->Aggregate(surface_id, GetNextDisplayTimeAndIncrement());

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
      &client, &manager_, kArbitraryRootFrameSinkId, kRootIsRoot,
      kNeedsSyncPoints);
  LocalSurfaceId local_surface_id1(7u, base::UnguessableToken::Create());
  LocalSurfaceId local_surface_id2(8u, base::UnguessableToken::Create());
  SurfaceId surface_id1(support->frame_sink_id(), local_surface_id1);
  SurfaceId surface_id2(support->frame_sink_id(), local_surface_id2);

  std::vector<ResourceId> ids = {11, 12, 13};
  SubmitCompositorFrameWithResources(ids, true, SurfaceId(), support.get(),
                                     surface_id1);

  CompositorFrame frame =
      aggregator_->Aggregate(surface_id1, GetNextDisplayTimeAndIncrement());

  // Nothing should be available to be returned yet.
  EXPECT_TRUE(client.returned_resources().empty());

  // Submitting a CompositorFrame to |surface_id2| should cause the surface
  // associated with |surface_id1| to get garbage collected.
  SubmitCompositorFrameWithResources({}, true, SurfaceId(), support.get(),
                                     surface_id2);
  manager_.surface_manager()->GarbageCollectSurfaces();

  frame = aggregator_->Aggregate(surface_id2, GetNextDisplayTimeAndIncrement());

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
      &client, &manager_, kArbitraryRootFrameSinkId, kRootIsRoot,
      kNeedsSyncPoints);
  LocalSurfaceId local_surface_id(7u, base::UnguessableToken::Create());
  SurfaceId surface_id(support->frame_sink_id(), local_surface_id);

  TransferableResource resource;
  resource.id = 11;
  // ResourceProvider is software but resource is not, so it should be
  // ignored.
  resource.is_software = false;

  CompositorFrame frame = CompositorFrameBuilder()
                              .AddDefaultRenderPass()
                              .AddTransferableResource(resource)
                              .Build();
  support->SubmitCompositorFrame(local_surface_id, std::move(frame));

  CompositorFrame returned_frame =
      aggregator_->Aggregate(surface_id, GetNextDisplayTimeAndIncrement());

  // Nothing should be available to be returned yet.
  EXPECT_TRUE(client.returned_resources().empty());

  SubmitCompositorFrameWithResources({}, true, SurfaceId(), support.get(),
                                     surface_id);
  ASSERT_EQ(1u, client.returned_resources().size());
  EXPECT_EQ(11u, client.returned_resources()[0].id);
}

TEST_F(SurfaceAggregatorWithResourcesTest, TwoSurfaces) {
  FakeCompositorFrameSinkClient client;
  auto support1 = std::make_unique<CompositorFrameSinkSupport>(
      &client, &manager_, FrameSinkId(1, 1), kChildIsRoot, kNeedsSyncPoints);
  auto support2 = std::make_unique<CompositorFrameSinkSupport>(
      &client, &manager_, FrameSinkId(2, 2), kChildIsRoot, kNeedsSyncPoints);
  LocalSurfaceId local_frame1_id(7u, base::UnguessableToken::Create());
  SurfaceId surface1_id(support1->frame_sink_id(), local_frame1_id);

  LocalSurfaceId local_frame2_id(8u, base::UnguessableToken::Create());
  SurfaceId surface2_id(support2->frame_sink_id(), local_frame2_id);

  std::vector<ResourceId> ids = {11, 12, 13};
  SubmitCompositorFrameWithResources(ids, true, SurfaceId(), support1.get(),
                                     surface1_id);
  std::vector<ResourceId> ids2 = {14, 15, 16};
  SubmitCompositorFrameWithResources(ids2, true, SurfaceId(), support2.get(),
                                     surface2_id);

  CompositorFrame frame =
      aggregator_->Aggregate(surface1_id, GetNextDisplayTimeAndIncrement());

  SubmitCompositorFrameWithResources({}, true, SurfaceId(), support1.get(),
                                     surface1_id);

  // Nothing should be available to be returned yet.
  EXPECT_TRUE(client.returned_resources().empty());

  frame = aggregator_->Aggregate(surface2_id, GetNextDisplayTimeAndIncrement());

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
      nullptr, &manager_, kArbitraryRootFrameSinkId, kRootIsRoot,
      kNeedsSyncPoints);
  auto middle_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryMiddleFrameSinkId, kChildIsRoot,
      kNeedsSyncPoints);
  auto child_support = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, kArbitraryFrameSinkId1, kChildIsRoot,
      kNeedsSyncPoints);
  LocalSurfaceId root_local_surface_id(7u, kArbitraryToken);
  SurfaceId root_surface_id(root_support->frame_sink_id(),
                            root_local_surface_id);
  LocalSurfaceId middle_local_surface_id(8u, kArbitraryToken);
  SurfaceId middle_surface_id(middle_support->frame_sink_id(),
                              middle_local_surface_id);
  LocalSurfaceId child_local_surface_id(9u, kArbitraryToken);
  SurfaceId child_surface_id(child_support->frame_sink_id(),
                             child_local_surface_id);

  std::vector<ResourceId> ids = {14, 15, 16};
  SubmitCompositorFrameWithResources(ids, true, SurfaceId(),
                                     child_support.get(), child_surface_id);

  std::vector<ResourceId> ids2 = {17, 18, 19};
  SubmitCompositorFrameWithResources(ids2, false, child_surface_id,
                                     middle_support.get(), middle_surface_id);

  std::vector<ResourceId> ids3 = {20, 21, 22};
  SubmitCompositorFrameWithResources(ids3, true, middle_surface_id,
                                     root_support.get(), root_surface_id);

  CompositorFrame frame;
  frame =
      aggregator_->Aggregate(root_surface_id, GetNextDisplayTimeAndIncrement());

  auto* pass_list = &frame.render_pass_list;
  ASSERT_EQ(1u, pass_list->size());
  EXPECT_EQ(1u, pass_list->back()->shared_quad_state_list.size());
  EXPECT_EQ(3u, pass_list->back()->quad_list.size());
  SubmitCompositorFrameWithResources(ids2, true, child_surface_id,
                                     middle_support.get(), middle_surface_id);

  frame =
      aggregator_->Aggregate(root_surface_id, GetNextDisplayTimeAndIncrement());

  pass_list = &frame.render_pass_list;
  ASSERT_EQ(1u, pass_list->size());
  EXPECT_EQ(3u, pass_list->back()->shared_quad_state_list.size());
  EXPECT_EQ(9u, pass_list->back()->quad_list.size());
}

TEST_F(SurfaceAggregatorWithResourcesTest, SecureOutputTexture) {
  auto support1 = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, FrameSinkId(1, 1), kChildIsRoot, kNeedsSyncPoints);
  auto support2 = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, &manager_, FrameSinkId(2, 2), kChildIsRoot, kNeedsSyncPoints);
  support2->set_allow_copy_output_requests_for_testing();
  LocalSurfaceId local_frame1_id(7u, base::UnguessableToken::Create());
  SurfaceId surface1_id(support1->frame_sink_id(), local_frame1_id);

  LocalSurfaceId local_frame2_id(8u, base::UnguessableToken::Create());
  SurfaceId surface2_id(support2->frame_sink_id(), local_frame2_id);

  std::vector<ResourceId> ids = {11, 12, 13};
  SubmitCompositorFrameWithResources(ids, true, SurfaceId(), support1.get(),
                                     surface1_id);

  CompositorFrame frame =
      aggregator_->Aggregate(surface1_id, GetNextDisplayTimeAndIncrement());

  auto* render_pass = frame.render_pass_list.back().get();

  EXPECT_EQ(DrawQuad::TEXTURE_CONTENT, render_pass->quad_list.back()->material);

  {
    auto pass = RenderPass::Create();
    pass->SetNew(1, gfx::Rect(0, 0, 20, 20), gfx::Rect(), gfx::Transform());
    auto* sqs = pass->CreateAndAppendSharedQuadState();
    sqs->opacity = 1.f;
    auto* surface_quad = pass->CreateAndAppendDrawQuad<SurfaceDrawQuad>();

    surface_quad->SetNew(sqs, gfx::Rect(0, 0, 1, 1), gfx::Rect(0, 0, 1, 1),
                         SurfaceRange(base::nullopt, surface1_id),
                         SK_ColorWHITE, false);
    pass->copy_requests.push_back(CopyOutputRequest::CreateStubForTesting());

    CompositorFrame frame =
        CompositorFrameBuilder().AddRenderPass(std::move(pass)).Build();

    support2->SubmitCompositorFrame(local_frame2_id, std::move(frame));
  }

  frame = aggregator_->Aggregate(surface2_id, GetNextDisplayTimeAndIncrement());
  EXPECT_EQ(1u, frame.render_pass_list.size());
  render_pass = frame.render_pass_list.front().get();

  // Parent has copy request, so texture should not be drawn.
  EXPECT_EQ(DrawQuad::SOLID_COLOR, render_pass->quad_list.back()->material);

  frame = aggregator_->Aggregate(surface2_id, GetNextDisplayTimeAndIncrement());
  EXPECT_EQ(1u, frame.render_pass_list.size());
  render_pass = frame.render_pass_list.front().get();

  // Copy request has been executed earlier, so texture should be drawn.
  EXPECT_EQ(DrawQuad::TEXTURE_CONTENT,
            render_pass->quad_list.front()->material);

  aggregator_->set_output_is_secure(false);

  frame = aggregator_->Aggregate(surface2_id, GetNextDisplayTimeAndIncrement());
  render_pass = frame.render_pass_list.back().get();

  // Output is insecure, so texture should be drawn.
  EXPECT_EQ(DrawQuad::SOLID_COLOR, render_pass->quad_list.back()->material);
}

// Ensure that the render passes have correct color spaces.
TEST_F(SurfaceAggregatorValidSurfaceTest, ColorSpaceTest) {
  std::vector<Quad> quads[2] = {
      {Quad::SolidColorQuad(SK_ColorWHITE, gfx::Rect(5, 5)),
       Quad::SolidColorQuad(SK_ColorLTGRAY, gfx::Rect(5, 5))},
      {Quad::SolidColorQuad(SK_ColorGRAY, gfx::Rect(5, 5)),
       Quad::SolidColorQuad(SK_ColorDKGRAY, gfx::Rect(5, 5))}};
  std::vector<Pass> passes = {Pass(quads[0], 2, SurfaceSize()),
                              Pass(quads[1], 1, SurfaceSize())};
  gfx::ColorSpace color_space1 = gfx::ColorSpace::CreateXYZD50();
  gfx::ColorSpace color_space2 = gfx::ColorSpace::CreateSRGB();
  gfx::ColorSpace color_space3 = gfx::ColorSpace::CreateSCRGBLinear();

  constexpr float device_scale_factor = 1.0f;
  SubmitCompositorFrame(support_.get(), passes, root_local_surface_id_,
                        device_scale_factor);

  SurfaceId surface_id(support_->frame_sink_id(), root_local_surface_id_);

  CompositorFrame aggregated_frame;
  aggregator_.SetOutputColorSpace(color_space1, color_space1);
  aggregated_frame =
      aggregator_.Aggregate(surface_id, GetNextDisplayTimeAndIncrement());
  EXPECT_EQ(2u, aggregated_frame.render_pass_list.size());
  EXPECT_EQ(color_space1, aggregated_frame.render_pass_list[0]->color_space);
  EXPECT_EQ(color_space1, aggregated_frame.render_pass_list[1]->color_space);

  aggregator_.SetOutputColorSpace(color_space2, color_space2);
  aggregated_frame =
      aggregator_.Aggregate(surface_id, GetNextDisplayTimeAndIncrement());
  EXPECT_EQ(2u, aggregated_frame.render_pass_list.size());
  EXPECT_EQ(color_space2, aggregated_frame.render_pass_list[0]->color_space);
  EXPECT_EQ(color_space2, aggregated_frame.render_pass_list[1]->color_space);

  aggregator_.SetOutputColorSpace(color_space1, color_space3);
  aggregated_frame =
      aggregator_.Aggregate(surface_id, GetNextDisplayTimeAndIncrement());
  EXPECT_EQ(3u, aggregated_frame.render_pass_list.size());
  EXPECT_EQ(color_space1, aggregated_frame.render_pass_list[0]->color_space);
  EXPECT_EQ(color_space1, aggregated_frame.render_pass_list[1]->color_space);
  EXPECT_EQ(color_space3, aggregated_frame.render_pass_list[2]->color_space);
}

// Tests that has_damage_from_contributing_content is aggregated correctly from
// child surface quads.
TEST_F(SurfaceAggregatorValidSurfaceTest, HasDamageByChangingChildSurface) {
  std::vector<Quad> child_surface_quads = {Quad::RenderPassQuad(1)};
  std::vector<Pass> child_surface_passes = {
      Pass(child_surface_quads, 1, SurfaceSize())};

  CompositorFrame child_surface_frame = MakeEmptyCompositorFrame();
  AddPasses(&child_surface_frame.render_pass_list, child_surface_passes,
            &child_surface_frame.metadata.referenced_surfaces);

  LocalSurfaceId child_local_surface_id = allocator_.GenerateId();
  SurfaceId child_surface_id(child_support_->frame_sink_id(),
                             child_local_surface_id);
  child_support_->SubmitCompositorFrame(child_local_surface_id,
                                        std::move(child_surface_frame));

  std::vector<Quad> root_surface_quads = {
      Quad::SurfaceQuad(SurfaceRange(base::nullopt, child_surface_id),
                        SK_ColorWHITE, gfx::Rect(5, 5), false)};
  std::vector<Pass> root_passes = {Pass(root_surface_quads, 1, SurfaceSize())};

  CompositorFrame root_frame = MakeEmptyCompositorFrame();
  AddPasses(&root_frame.render_pass_list, root_passes,
            &root_frame.metadata.referenced_surfaces);

  SurfaceId root_surface_id(support_->frame_sink_id(), root_local_surface_id_);
  support_->SubmitCompositorFrame(root_local_surface_id_,
                                  std::move(root_frame));

  // On first frame there is no existing cache texture to worry about re-using,
  // so we don't worry what this bool is set to.
  CompositorFrame aggregated_frame =
      aggregator_.Aggregate(root_surface_id, GetNextDisplayTimeAndIncrement());

  // No Surface changed, so no damage should be given.
  {
    CompositorFrame aggregated_frame = aggregator_.Aggregate(
        root_surface_id, GetNextDisplayTimeAndIncrement());
    EXPECT_FALSE(aggregated_frame.render_pass_list[0]
                     ->has_damage_from_contributing_content);
  }

  // Change child_frame with damage should set the flag.
  {
    CompositorFrame child_surface_frame = MakeEmptyCompositorFrame();
    AddPasses(&child_surface_frame.render_pass_list, child_surface_passes,
              &child_surface_frame.metadata.referenced_surfaces);
    child_support_->SubmitCompositorFrame(child_local_surface_id,
                                          std::move(child_surface_frame));

    CompositorFrame aggregated_frame = aggregator_.Aggregate(
        root_surface_id, GetNextDisplayTimeAndIncrement());
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
    child_support_->SubmitCompositorFrame(child_local_surface_id,
                                          std::move(child_surface_frame));

    CompositorFrame aggregated_frame = aggregator_.Aggregate(
        root_surface_id, GetNextDisplayTimeAndIncrement());
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
      nullptr, &manager_, kArbitraryMiddleFrameSinkId, kChildIsRoot,
      kNeedsSyncPoints);

  std::vector<Quad> child_surface_quads = {Quad::RenderPassQuad(1)};
  std::vector<Pass> child_surface_passes = {
      Pass(child_surface_quads, 1, SurfaceSize())};

  CompositorFrame child_surface_frame = MakeEmptyCompositorFrame();
  AddPasses(&child_surface_frame.render_pass_list, child_surface_passes,
            &child_surface_frame.metadata.referenced_surfaces);

  LocalSurfaceId child_local_surface_id = allocator_.GenerateId();
  SurfaceId child_surface_id(child_support_->frame_sink_id(),
                             child_local_surface_id);
  child_support_->SubmitCompositorFrame(child_local_surface_id,
                                        std::move(child_surface_frame));

  std::vector<Quad> root_surface_quads = {
      Quad::SurfaceQuad(SurfaceRange(base::nullopt, child_surface_id),
                        SK_ColorWHITE, gfx::Rect(5, 5), false)};
  std::vector<Pass> root_passes = {Pass(root_surface_quads, 1, SurfaceSize())};

  CompositorFrame root_frame = MakeEmptyCompositorFrame();
  AddPasses(&root_frame.render_pass_list, root_passes,
            &root_frame.metadata.referenced_surfaces);

  SurfaceId root_surface_id(support_->frame_sink_id(), root_local_surface_id_);
  support_->SubmitCompositorFrame(root_local_surface_id_,
                                  std::move(root_frame));

  // On first frame there is no existing cache texture to worry about re-using,
  // so we don't worry what this bool is set to.
  CompositorFrame aggregated_frame =
      aggregator_.Aggregate(root_surface_id, GetNextDisplayTimeAndIncrement());

  // No Surface changed, so no damage should be given.
  {
    CompositorFrame aggregated_frame = aggregator_.Aggregate(
        root_surface_id, GetNextDisplayTimeAndIncrement());
    EXPECT_FALSE(aggregated_frame.render_pass_list[0]
                     ->has_damage_from_contributing_content);
  }

  // Add a grand_child_frame should cause damage.
  std::vector<Quad> grand_child_quads = {Quad::RenderPassQuad(1)};
  std::vector<Pass> grand_child_passes = {
      Pass(grand_child_quads, 1, SurfaceSize())};
  LocalSurfaceId grand_child_local_surface_id = allocator_.GenerateId();
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
                          SK_ColorWHITE, gfx::Rect(5, 5), false)};
    std::vector<Pass> new_child_surface_passes = {
        Pass(new_child_surface_quads, 1, SurfaceSize())};
    child_surface_frame = MakeEmptyCompositorFrame();
    AddPasses(&child_surface_frame.render_pass_list, new_child_surface_passes,
              &child_surface_frame.metadata.referenced_surfaces);
    child_support_->SubmitCompositorFrame(child_local_surface_id,
                                          std::move(child_surface_frame));

    CompositorFrame aggregated_frame = aggregator_.Aggregate(
        root_surface_id, GetNextDisplayTimeAndIncrement());
    // True for new grand_child_frame.
    EXPECT_TRUE(aggregated_frame.render_pass_list[0]
                    ->has_damage_from_contributing_content);
  }

  // No Surface changed, so no damage should be given.
  {
    CompositorFrame aggregated_frame = aggregator_.Aggregate(
        root_surface_id, GetNextDisplayTimeAndIncrement());
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

    CompositorFrame aggregated_frame = aggregator_.Aggregate(
        root_surface_id, GetNextDisplayTimeAndIncrement());
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

    CompositorFrame aggregated_frame = aggregator_.Aggregate(
        root_surface_id, GetNextDisplayTimeAndIncrement());
    // False for new grand_child_frame without damage.
    EXPECT_FALSE(aggregated_frame.render_pass_list[0]
                     ->has_damage_from_contributing_content);
  }
}

// Tests that has_damage_from_contributing_content is aggregated correctly from
// render pass quads.
TEST_F(SurfaceAggregatorValidSurfaceTest, HasDamageFromRenderPassQuads) {
  std::vector<Quad> child_quads = {Quad::RenderPassQuad(1)};
  std::vector<Pass> child_passes = {Pass(child_quads, 1, SurfaceSize())};

  CompositorFrame child_frame = MakeEmptyCompositorFrame();
  AddPasses(&child_frame.render_pass_list, child_passes,
            &child_frame.metadata.referenced_surfaces);

  LocalSurfaceId child_local_surface_id = allocator_.GenerateId();
  SurfaceId child_surface_id(child_support_->frame_sink_id(),
                             child_local_surface_id);
  child_support_->SubmitCompositorFrame(child_local_surface_id,
                                        std::move(child_frame));

  std::vector<Quad> root_surface_quads = {
      Quad::SurfaceQuad(SurfaceRange(base::nullopt, child_surface_id),
                        SK_ColorWHITE, gfx::Rect(5, 5), false)};
  std::vector<Quad> root_render_pass_quads = {Quad::RenderPassQuad(1)};

  std::vector<Pass> root_passes = {
      Pass(root_surface_quads, 1, SurfaceSize()),
      Pass(root_render_pass_quads, 2, SurfaceSize())};

  CompositorFrame root_frame = MakeEmptyCompositorFrame();
  AddPasses(&root_frame.render_pass_list, root_passes,
            &root_frame.metadata.referenced_surfaces);

  support_->SubmitCompositorFrame(root_local_surface_id_,
                                  std::move(root_frame));

  SurfaceId root_surface_id(support_->frame_sink_id(), root_local_surface_id_);
  CompositorFrame aggregated_frame =
      aggregator_.Aggregate(root_surface_id, GetNextDisplayTimeAndIncrement());

  // On first frame there is no existing cache texture to worry about re-using,
  // so we don't worry what this bool is set to.
  const auto& aggregated_pass_list = aggregated_frame.render_pass_list;

  ASSERT_EQ(2u, aggregated_pass_list.size());

  // No Surface changed, so no damage should be given.
  {
    CompositorFrame aggregated_frame = aggregator_.Aggregate(
        root_surface_id, GetNextDisplayTimeAndIncrement());
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
    child_support_->SubmitCompositorFrame(child_local_surface_id,
                                          std::move(child_frame));

    CompositorFrame aggregated_frame = aggregator_.Aggregate(
        root_surface_id, GetNextDisplayTimeAndIncrement());
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
  int pass_id[] = {1, 2};
  std::vector<Quad> root_quads[2] = {
      {Quad::SolidColorQuad(SK_ColorGREEN, gfx::Rect(5, 5))},
      {Quad::RenderPassQuad(pass_id[0])},
  };
  std::vector<Pass> root_passes = {
      Pass(root_quads[0], pass_id[0], SurfaceSize()),
      Pass(root_quads[1], pass_id[1], SurfaceSize())};

  CompositorFrame root_frame = MakeEmptyCompositorFrame();
  AddPasses(&root_frame.render_pass_list, root_passes,
            &root_frame.metadata.referenced_surfaces);

  support_->SubmitCompositorFrame(root_local_surface_id_,
                                  std::move(root_frame));

  SurfaceId root_surface_id(support_->frame_sink_id(), root_local_surface_id_);
  CompositorFrame aggregated_frame =
      aggregator_.Aggregate(root_surface_id, GetNextDisplayTimeAndIncrement());

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

    support_->SubmitCompositorFrame(root_local_surface_id_,
                                    std::move(root_frame));

    CompositorFrame aggregated_frame = aggregator_.Aggregate(
        root_surface_id, GetNextDisplayTimeAndIncrement());
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

    support_->SubmitCompositorFrame(root_local_surface_id_,
                                    std::move(root_frame));

    CompositorFrame aggregated_frame = aggregator_.Aggregate(
        root_surface_id, GetNextDisplayTimeAndIncrement());
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
  int pass_id[] = {1, 2};
  std::vector<Quad> child_quads[2] = {
      {Quad::SolidColorQuad(SK_ColorGREEN, gfx::Rect(5, 5))},
      {Quad::RenderPassQuad(pass_id[0])},
  };
  std::vector<Pass> child_passes = {
      Pass(child_quads[0], pass_id[0], SurfaceSize()),
      Pass(child_quads[1], pass_id[1], SurfaceSize())};

  CompositorFrame child_frame = MakeEmptyCompositorFrame();
  AddPasses(&child_frame.render_pass_list, child_passes,
            &child_frame.metadata.referenced_surfaces);

  LocalSurfaceId child_local_surface_id = allocator_.GenerateId();
  SurfaceId child_surface_id(child_support_->frame_sink_id(),
                             child_local_surface_id);
  child_support_->SubmitCompositorFrame(child_local_surface_id,
                                        std::move(child_frame));

  std::vector<Quad> root_surface_quads = {
      Quad::SurfaceQuad(SurfaceRange(base::nullopt, child_surface_id),
                        SK_ColorWHITE, gfx::Rect(5, 5), false)};

  std::vector<Pass> root_passes = {Pass(root_surface_quads, 1, SurfaceSize())};

  CompositorFrame root_frame = MakeEmptyCompositorFrame();
  AddPasses(&root_frame.render_pass_list, root_passes,
            &root_frame.metadata.referenced_surfaces);

  support_->SubmitCompositorFrame(root_local_surface_id_,
                                  std::move(root_frame));

  SurfaceId root_surface_id(support_->frame_sink_id(), root_local_surface_id_);
  CompositorFrame aggregated_frame =
      aggregator_.Aggregate(root_surface_id, GetNextDisplayTimeAndIncrement());

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

    child_support_->SubmitCompositorFrame(child_local_surface_id,
                                          std::move(child_frame));

    CompositorFrame aggregated_frame = aggregator_.Aggregate(
        root_surface_id, GetNextDisplayTimeAndIncrement());
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

    child_support_->SubmitCompositorFrame(child_local_surface_id,
                                          std::move(child_frame));

    CompositorFrame aggregated_frame = aggregator_.Aggregate(
        root_surface_id, GetNextDisplayTimeAndIncrement());
    const auto& aggregated_pass_list = aggregated_frame.render_pass_list;

    // Should have full damage.
    EXPECT_EQ(gfx::Rect(SurfaceSize()), aggregated_pass_list[0]->damage_rect);
    EXPECT_EQ(child_root_pass_damage, aggregated_pass_list[1]->damage_rect);
  }
}

// Tests that quads outside the damage rect are not ignored for cached render
// pass.
TEST_F(SurfaceAggregatorPartialSwapTest, NotIgnoreOutsideForCachedRenderPass) {
  LocalSurfaceId child_local_surface_id = allocator_.GenerateId();
  SurfaceId child_surface_id(child_support_->frame_sink_id(),
                             child_local_surface_id);
  // The child surface has two quads, one with a visible rect of 15,15 6x6 and
  // the other other with a visible rect of 10,10 2x2 (relative to root target
  // space).
  constexpr float device_scale_factor = 1.0f;
  {
    int pass_id[] = {1, 2};
    std::vector<Quad> child_quads[2] = {
        {Quad::SolidColorQuad(SK_ColorGREEN, gfx::Rect(5, 5))},
        {Quad::RenderPassQuad(pass_id[0])},
    };
    std::vector<Pass> child_passes = {
        Pass(child_quads[0], pass_id[0], SurfaceSize()),
        Pass(child_quads[1], pass_id[1], SurfaceSize())};

    RenderPassList child_pass_list;
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

    SubmitPassListAsFrame(child_support_.get(), child_local_surface_id,
                          &child_pass_list, std::move(referenced_surfaces),
                          device_scale_factor);
  }

  {
    int pass_id[] = {1, 2};
    std::vector<Quad> root_quads[2] = {
        {Quad::SurfaceQuad(SurfaceRange(base::nullopt, child_surface_id),
                           SK_ColorWHITE, gfx::Rect(5, 5), false)},
        {Quad::RenderPassQuad(pass_id[0])},
    };
    std::vector<Pass> root_passes = {
        Pass(root_quads[0], pass_id[0], SurfaceSize()),
        Pass(root_quads[1], pass_id[1], SurfaceSize())};

    RenderPassList root_pass_list;
    std::vector<SurfaceRange> referenced_surfaces;
    AddPasses(&root_pass_list, root_passes, &referenced_surfaces);

    auto* root_pass = root_pass_list[1].get();
    root_pass->shared_quad_state_list.front()
        ->quad_to_target_transform.Translate(10, 10);
    root_pass->damage_rect = gfx::Rect(0, 0, 1, 1);

    SubmitPassListAsFrame(support_.get(), root_local_surface_id_,
                          &root_pass_list, std::move(referenced_surfaces),
                          device_scale_factor);
  }

  SurfaceId root_surface_id(support_->frame_sink_id(), root_local_surface_id_);
  CompositorFrame aggregated_frame =
      aggregator_.Aggregate(root_surface_id, GetNextDisplayTimeAndIncrement());

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
    int pass_id[] = {1, 2};
    std::vector<Quad> root_quads[2] = {
        {Quad::SurfaceQuad(SurfaceRange(base::nullopt, child_surface_id),
                           SK_ColorWHITE, gfx::Rect(5, 5), false)},
        {Quad::RenderPassQuad(pass_id[0])},
    };
    std::vector<Pass> root_passes = {
        Pass(root_quads[0], pass_id[0], SurfaceSize()),
        Pass(root_quads[1], pass_id[1], SurfaceSize())};

    RenderPassList root_pass_list;
    std::vector<SurfaceRange> referenced_surfaces;
    AddPasses(&root_pass_list, root_passes, &referenced_surfaces);

    auto* root_pass = root_pass_list[1].get();
    root_pass->shared_quad_state_list.front()
        ->quad_to_target_transform.Translate(10, 10);
    root_pass->damage_rect = gfx::Rect(10, 10, 2, 2);
    SubmitPassListAsFrame(support_.get(), root_local_surface_id_,
                          &root_pass_list, std::move(referenced_surfaces),
                          device_scale_factor);
  }

  {
    CompositorFrame aggregated_frame = aggregator_.Aggregate(
        root_surface_id, GetNextDisplayTimeAndIncrement());
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

}  // namespace
}  // namespace viz
