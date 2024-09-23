// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/quads/render_pass_io.h"

#include <array>
#include <memory>
#include <string>

#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/path_service.h"
#include "base/values.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/quads/compositor_render_pass_draw_quad.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/surface_draw_quad.h"
#include "components/viz/common/quads/tile_draw_quad.h"
#include "components/viz/common/quads/video_hole_draw_quad.h"
#include "components/viz/test/paths.h"
#include "components/viz/test/test_surface_id_allocator.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/modules/skcms/skcms.h"

namespace gfx {
struct HDRMetadata;
}

namespace viz {
namespace {

TEST(RenderPassIOTest, Default) {
  auto render_pass0 = CompositorRenderPass::Create();
  base::Value::Dict dict0 = CompositorRenderPassToDict(*render_pass0);
  auto render_pass1 = CompositorRenderPassFromDict(dict0);
  EXPECT_TRUE(render_pass1);
  base::Value::Dict dict1 = CompositorRenderPassToDict(*render_pass1);
  EXPECT_EQ(dict0, dict1);
}

TEST(RenderPassIOTest, FilterOperations) {
  auto render_pass0 = CompositorRenderPass::Create();
  {
    // Add two filters.
    cc::FilterOperation grayscale =
        cc::FilterOperation::CreateGrayscaleFilter(0.25f);
    render_pass0->filters.Append(grayscale);
    cc::FilterOperation opacity =
        cc::FilterOperation::CreateOpacityFilter(0.8f);
    render_pass0->filters.Append(opacity);
  }
  {
    // Add three backdrop filters.
    cc::FilterOperation drop_shadow =
        cc::FilterOperation::CreateDropShadowFilter(gfx::Point(1.0f, 2.0f),
                                                    0.8f, SkColors::kYellow);
    render_pass0->backdrop_filters.Append(drop_shadow);
    cc::FilterOperation invert = cc::FilterOperation::CreateInvertFilter(0.64f);
    render_pass0->backdrop_filters.Append(invert);
    cc::FilterOperation zoom = cc::FilterOperation::CreateZoomFilter(2.1f, 10);
    render_pass0->backdrop_filters.Append(zoom);
  }
  {
    // Set backdrop filter bounds.
    gfx::RRectF rrect(gfx::RectF(2.f, 3.f, 4.f, 5.f), 1.5f);
    ASSERT_EQ(gfx::RRectF::Type::kSingle, rrect.GetType());
    render_pass0->backdrop_filter_bounds = rrect;
  }
  base::Value::Dict dict0 = CompositorRenderPassToDict(*render_pass0);
  auto render_pass1 = CompositorRenderPassFromDict(dict0);
  EXPECT_TRUE(render_pass1);
  {
    // Verify two filters are as expected.
    EXPECT_EQ(render_pass0->filters, render_pass1->filters);
    EXPECT_EQ(2u, render_pass1->filters.size());
    EXPECT_EQ(cc::FilterOperation::GRAYSCALE,
              render_pass1->filters.at(0).type());
    EXPECT_EQ(0.25f, render_pass1->filters.at(0).amount());
    EXPECT_EQ(cc::FilterOperation::OPACITY, render_pass1->filters.at(1).type());
    EXPECT_EQ(0.8f, render_pass1->filters.at(1).amount());
  }
  {
    // Verify three backdrop filters are as expected.
    EXPECT_EQ(render_pass0->backdrop_filters, render_pass1->backdrop_filters);
    EXPECT_EQ(3u, render_pass1->backdrop_filters.size());
    EXPECT_EQ(cc::FilterOperation::DROP_SHADOW,
              render_pass1->backdrop_filters.at(0).type());
    EXPECT_EQ(0.8f, render_pass1->backdrop_filters.at(0).amount());
    EXPECT_EQ(SkColors::kYellow,
              render_pass1->backdrop_filters.at(0).drop_shadow_color());
    EXPECT_EQ(gfx::Point(1.0f, 2.0f),
              render_pass1->backdrop_filters.at(0).offset());
    EXPECT_EQ(cc::FilterOperation::INVERT,
              render_pass1->backdrop_filters.at(1).type());
    EXPECT_EQ(0.64f, render_pass1->backdrop_filters.at(1).amount());
    EXPECT_EQ(cc::FilterOperation::ZOOM,
              render_pass1->backdrop_filters.at(2).type());
    EXPECT_EQ(2.1f, render_pass1->backdrop_filters.at(2).amount());
    EXPECT_EQ(10, render_pass1->backdrop_filters.at(2).zoom_inset());
  }
  {
    // Verify backdrop filter bounds are as expected.
    EXPECT_TRUE(render_pass1->backdrop_filter_bounds.has_value());
    EXPECT_TRUE(render_pass0->backdrop_filter_bounds->Equals(
        render_pass1->backdrop_filter_bounds.value()));
    EXPECT_EQ(gfx::RRectF::Type::kSingle,
              render_pass1->backdrop_filter_bounds->GetType());
    EXPECT_EQ(1.5f, render_pass1->backdrop_filter_bounds->GetSimpleRadius());
    EXPECT_EQ(gfx::RectF(2.f, 3.f, 4.f, 5.f),
              render_pass1->backdrop_filter_bounds->rect());
  }
  base::Value::Dict dict1 = CompositorRenderPassToDict(*render_pass1);
  EXPECT_EQ(dict0, dict1);
}

TEST(RenderPassIOTest, SharedQuadStateList) {
  auto render_pass0 = CompositorRenderPass::Create();
  {
    // Add two SQS.
    SharedQuadState* sqs0 = render_pass0->CreateAndAppendSharedQuadState();
    ASSERT_TRUE(sqs0);
    SharedQuadState* sqs1 = render_pass0->CreateAndAppendSharedQuadState();
    ASSERT_TRUE(sqs1);
    gfx::Transform transform;
    transform.MakeIdentity();
    gfx::LinearGradient gradient_mask(40);
    gradient_mask.AddStep(/*fraction=*/0, /*alpha=*/0);
    gradient_mask.AddStep(1, 255);
    sqs1->SetAll(
        transform, gfx::Rect(0, 0, 640, 480), gfx::Rect(10, 10, 600, 400),
        gfx::MaskFilterInfo(gfx::RRectF(gfx::RectF(2.f, 3.f, 4.f, 5.f), 1.5f),
                            gradient_mask),
        gfx::Rect(5, 20, 1000, 200), /*contents_opaque=*/false,
        /*opacity_f=*/0.5f, SkBlendMode::kDstOver, /*sorting_context=*/101,
        /*layer_id=*/0u, /*fast_rounded_corner=*/true);
  }
  base::Value::Dict dict0 = CompositorRenderPassToDict(*render_pass0);
  auto render_pass1 = CompositorRenderPassFromDict(dict0);
  ASSERT_TRUE(render_pass1);
  {
    // Verify two SQS.
    EXPECT_EQ(2u, render_pass1->shared_quad_state_list.size());
    const SharedQuadState* sqs0 =
        render_pass1->shared_quad_state_list.ElementAt(0);
    EXPECT_TRUE(sqs0);
    EXPECT_TRUE(sqs0->quad_to_target_transform.IsIdentity());
    EXPECT_EQ(gfx::Rect(), sqs0->quad_layer_rect);
    EXPECT_EQ(gfx::Rect(), sqs0->visible_quad_layer_rect);
    EXPECT_FALSE(sqs0->mask_filter_info.HasRoundedCorners());
    EXPECT_FALSE(sqs0->mask_filter_info.HasGradientMask());
    EXPECT_EQ(std::nullopt, sqs0->clip_rect);
    EXPECT_TRUE(sqs0->are_contents_opaque);
    EXPECT_EQ(1.0f, sqs0->opacity);
    EXPECT_EQ(SkBlendMode::kSrcOver, sqs0->blend_mode);
    EXPECT_EQ(0, sqs0->sorting_context_id);
    EXPECT_FALSE(sqs0->is_fast_rounded_corner);

    const SharedQuadState* sqs1 =
        render_pass1->shared_quad_state_list.ElementAt(1);
    EXPECT_TRUE(sqs1);
    EXPECT_TRUE(sqs1->quad_to_target_transform.IsIdentity());
    EXPECT_EQ(gfx::Rect(0, 0, 640, 480), sqs1->quad_layer_rect);
    EXPECT_EQ(gfx::Rect(10, 10, 600, 400), sqs1->visible_quad_layer_rect);
    EXPECT_EQ(gfx::RRectF::Type::kSingle,
              sqs1->mask_filter_info.rounded_corner_bounds().GetType());
    EXPECT_EQ(1.5f,
              sqs1->mask_filter_info.rounded_corner_bounds().GetSimpleRadius());
    ASSERT_TRUE(sqs1->mask_filter_info.HasGradientMask());
    EXPECT_EQ(40, sqs1->mask_filter_info.gradient_mask()->angle());
    EXPECT_EQ(2u, sqs1->mask_filter_info.gradient_mask()->step_count());
    EXPECT_EQ(gfx::LinearGradient::Step({0, 0}),
              sqs1->mask_filter_info.gradient_mask()->steps()[0]);
    EXPECT_EQ(gfx::LinearGradient::Step({1, 255}),
              sqs1->mask_filter_info.gradient_mask()->steps()[1]);
    EXPECT_EQ(gfx::RectF(2.f, 3.f, 4.f, 5.f), sqs1->mask_filter_info.bounds());
    EXPECT_EQ(gfx::Rect(5, 20, 1000, 200), sqs1->clip_rect);
    EXPECT_FALSE(sqs1->are_contents_opaque);
    EXPECT_EQ(0.5f, sqs1->opacity);
    EXPECT_EQ(SkBlendMode::kDstOver, sqs1->blend_mode);
    EXPECT_EQ(101, sqs1->sorting_context_id);
    EXPECT_TRUE(sqs1->is_fast_rounded_corner);
  }
  base::Value::Dict dict1 = CompositorRenderPassToDict(*render_pass1);
  EXPECT_EQ(dict0, dict1);
}

TEST(RenderPassIOTest, QuadList) {
  const size_t kSharedQuadStateCount = 4;
  size_t quad_count = 0;
  const std::array<DrawQuad::Material, 8> kQuadMaterials = {
      DrawQuad::Material::kSolidColor,
      DrawQuad::Material::kTextureContent,  // is_stream_video set to true.
      DrawQuad::Material::kVideoHole,
      DrawQuad::Material::kTextureContent,
      DrawQuad::Material::kCompositorRenderPass,
      DrawQuad::Material::kTiledContent,
      DrawQuad::Material::kSurfaceContent,
      DrawQuad::Material::kSurfaceContent,
  };
  TestSurfaceIdAllocator kSurfaceId1(FrameSinkId(1, 1));
  TestSurfaceIdAllocator kSurfaceId2(FrameSinkId(2, 2));
  auto render_pass0 = CompositorRenderPass::Create();
  {
    // Add to shared_quad_state_list.
    for (size_t ii = 0; ii < kSharedQuadStateCount; ++ii) {
      SharedQuadState* sqs = render_pass0->CreateAndAppendSharedQuadState();
      ASSERT_TRUE(sqs);
    }

    size_t sqs_index = 0;

    // Add to quad_list.
    {
      // 1. SolidColorDrawQuad
      SolidColorDrawQuad* quad =
          render_pass0->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
      quad->SetAll(render_pass0->shared_quad_state_list.ElementAt(sqs_index),
                   gfx::Rect(0, 0, 30, 40), gfx::Rect(1, 2, 20, 30), true,
                   SkColors::kRed, false);
      ++quad_count;
    }
    {
      // 2. TextureDrawQuad with is_stream_video set to true.
      TextureDrawQuad* quad =
          render_pass0->CreateAndAppendDrawQuad<TextureDrawQuad>();
      quad->SetAll(render_pass0->shared_quad_state_list.ElementAt(sqs_index),
                   gfx::Rect(10, 10, 300, 400), gfx::Rect(10, 10, 200, 400),
                   false, ResourceId(100), gfx::Size(600, 800), false,
                   gfx::PointF(0.f, 0.f), gfx::PointF(1.f, 1.f),
                   SkColors::kTransparent, false, false, false,
                   gfx::ProtectedVideoType::kHardwareProtected);
      quad->is_stream_video = true;
      ++sqs_index;
      ++quad_count;
    }
    {
      // 3. VideoHoleDrawQuad
      VideoHoleDrawQuad* quad =
          render_pass0->CreateAndAppendDrawQuad<VideoHoleDrawQuad>();
      quad->SetAll(render_pass0->shared_quad_state_list.ElementAt(sqs_index),
                   gfx::Rect(5, 5, 305, 405), gfx::Rect(15, 15, 205, 305),
                   false, base::UnguessableToken::Create());
      ++quad_count;
    }
    {
      // 4. TextureDrawQuad
      TextureDrawQuad* quad =
          render_pass0->CreateAndAppendDrawQuad<TextureDrawQuad>();
      quad->SetAll(render_pass0->shared_quad_state_list.ElementAt(sqs_index),
                   gfx::Rect(0, 0, 100, 50), gfx::Rect(0, 0, 100, 50), false,
                   ResourceId(9u), gfx::Size(100, 50), false,
                   gfx::PointF(0.f, 0.f), gfx::PointF(1.f, 1.f),
                   SkColors::kBlue, false, true, false,
                   gfx::ProtectedVideoType::kHardwareProtected);

      ++sqs_index;
      ++quad_count;
    }
    {
      // 5. CompositorRenderPassDrawQuad
      CompositorRenderPassDrawQuad* quad =
          render_pass0->CreateAndAppendDrawQuad<CompositorRenderPassDrawQuad>();
      quad->SetAll(render_pass0->shared_quad_state_list.ElementAt(sqs_index),
                   gfx::Rect(2, 3, 100, 50), gfx::Rect(2, 3, 100, 50), true,
                   CompositorRenderPassId{198u}, ResourceId(81u),
                   gfx::RectF(0.1f, 0.2f, 0.5f, 0.6f), gfx::Size(800, 600),
                   gfx::Vector2dF(1.1f, 0.9f), gfx::PointF(0.01f, 0.02f),
                   gfx::RectF(0.2f, 0.3f, 0.3f, 0.4f), true, 0.88f, true);
      ++sqs_index;
      ++quad_count;
    }
    {
      // 6. TileDrawQuad
      TileDrawQuad* quad =
          render_pass0->CreateAndAppendDrawQuad<TileDrawQuad>();
      quad->SetAll(render_pass0->shared_quad_state_list.ElementAt(sqs_index),
                   gfx::Rect(0, 0, 256, 512), gfx::Rect(2, 2, 250, 500), true,
                   ResourceId(512u), gfx::RectF(0.0f, 0.0f, 0.9f, 0.8f),
                   gfx::Size(256, 512), true, true, true);
      ++quad_count;
    }
    {
      // 7. SurfaceDrawQuad
      SurfaceDrawQuad* quad =
          render_pass0->CreateAndAppendDrawQuad<SurfaceDrawQuad>();
      quad->SetAll(render_pass0->shared_quad_state_list.ElementAt(sqs_index),
                   gfx::Rect(0, 0, 512, 256), gfx::Rect(2, 2, 500, 250), true,
                   SurfaceRange(kSurfaceId1, kSurfaceId2), SkColors::kWhite,
                   false, false, true);
      ++quad_count;
    }
    {
      // 8. SurfaceDrawQuad with no starting SurfaceId
      SurfaceDrawQuad* quad =
          render_pass0->CreateAndAppendDrawQuad<SurfaceDrawQuad>();
      quad->SetAll(render_pass0->shared_quad_state_list.ElementAt(sqs_index),
                   gfx::Rect(10, 10, 512, 256), gfx::Rect(12, 12, 500, 250),
                   true, SurfaceRange(std::nullopt, kSurfaceId1),
                   SkColors::kBlack, true, true, false);
      ++quad_count;
    }
    DCHECK_EQ(kSharedQuadStateCount, sqs_index + 1);
  }
  base::Value::Dict dict0 = CompositorRenderPassToDict(*render_pass0);
  auto render_pass1 = CompositorRenderPassFromDict(dict0);
  EXPECT_TRUE(render_pass1);
  EXPECT_EQ(kSharedQuadStateCount, render_pass1->shared_quad_state_list.size());
  EXPECT_EQ(quad_count, render_pass1->quad_list.size());
  for (size_t ii = 0; ii < quad_count; ++ii) {
    EXPECT_EQ(kQuadMaterials[ii],
              render_pass1->quad_list.ElementAt(ii)->material);
  }
  base::Value::Dict dict1 = CompositorRenderPassToDict(*render_pass1);
  EXPECT_EQ(dict0, dict1);
}

TEST(RenderPassIOTest, CompositorRenderPassList) {
  // Validate recorded render pass list data from https://www.espn.com/.
  base::FilePath test_data_dir;
  ASSERT_TRUE(base::PathService::Get(Paths::DIR_TEST_DATA, &test_data_dir));
  base::FilePath json_path =
      test_data_dir.Append(FILE_PATH_LITERAL("render_pass_data"))
          .Append(FILE_PATH_LITERAL("top_real_world_desktop"))
          .Append(FILE_PATH_LITERAL("espn_2018"))
          .Append(FILE_PATH_LITERAL("0463.json"));
  ASSERT_TRUE(base::PathExists(json_path));
  std::string json_text;
  ASSERT_TRUE(base::ReadFileToString(json_path, &json_text));

  std::optional<base::Value> dict0 = base::JSONReader::Read(json_text);
  EXPECT_TRUE(dict0.has_value());
  CompositorRenderPassList render_pass_list;
  EXPECT_TRUE(
      CompositorRenderPassListFromDict(dict0->GetDict(), &render_pass_list));
  base::Value::Dict dict1 = CompositorRenderPassListToDict(render_pass_list);
  // Since the test file doesn't contain the field
  // 'intersects_damage_under' in its CompositorRenderPassDrawQuad, I'm
  // removing the field on dict1 for the exact comparison to work.
  base::Value::List* list = dict1.FindList("render_pass_list");
  for (auto& entry : *list) {
    base::Value::List* quad_list = entry.GetDict().FindList("quad_list");

    for (auto& quad_entry : *quad_list) {
      if (base::Value* extra_value =
              quad_entry.GetDict().Find("intersects_damage_under")) {
        EXPECT_FALSE(extra_value->GetBool());
        ASSERT_TRUE(quad_entry.GetDict().Remove("intersects_damage_under"));
      }
    }
  }

  EXPECT_EQ(dict0, dict1);
}

TEST(RenderPassIOTest, CompositorFrameData) {
  // Validate recorded multi-surface compositor frame data from a tab with
  // https://www.youtube.com/ focused, and 4 other tabs in the background.
  base::FilePath test_data_dir;
  ASSERT_TRUE(base::PathService::Get(Paths::DIR_TEST_DATA, &test_data_dir));
  base::FilePath json_path =
      test_data_dir.Append(FILE_PATH_LITERAL("render_pass_data"))
          .Append(FILE_PATH_LITERAL("multi_surface_test"))
          .Append(FILE_PATH_LITERAL("youtube_tab_focused"))
          .Append(FILE_PATH_LITERAL("1641.json"));
  ASSERT_TRUE(base::PathExists(json_path));
  std::string json_text;
  ASSERT_TRUE(base::ReadFileToString(json_path, &json_text));

  std::optional<base::Value> list0 = base::JSONReader::Read(json_text);
  EXPECT_TRUE(list0.has_value());
  std::vector<FrameData> frame_data_list;
  EXPECT_TRUE(FrameDataFromList(list0->GetList(), &frame_data_list));
  base::Value::List list1 = FrameDataToList(frame_data_list);

  EXPECT_EQ(list0->GetList(), list1);
}

}  // namespace
}  // namespace viz
