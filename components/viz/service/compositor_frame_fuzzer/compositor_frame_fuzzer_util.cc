// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/compositor_frame_fuzzer/compositor_frame_fuzzer_util.h"

#include <limits>
#include <utility>

#include "base/logging.h"
#include "cc/base/math_util.h"
#include "components/viz/common/quads/compositor_render_pass_draw_quad.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/tile_draw_quad.h"
#include "components/viz/common/resources/bitmap_allocation.h"
#include "components/viz/common/resources/resource_sizes.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/mask_filter_info.h"

namespace viz {
namespace {

// Limit on number of bytes of memory to allocate (e.g. for referenced
// bitmaps) in association with a single CompositorFrame. Currently 0.5 GiB;
// reduce this if bots are running out of memory.
constexpr uint64_t kMaxTextureMemory = 1 << 29;

// Handles inf / NaN by setting to 0.
double MakeNormal(double x) {
  return isnormal(x) ? x : 0;
}
float MakeNormal(float x) {
  return isnormal(x) ? x : 0;
}

// Normalizes value to a float in [0, 1]. Use to convert a fuzzed
// uint32 into a percentage.
float Normalize(uint32_t x) {
  return static_cast<float>(x) /
         static_cast<float>(std::numeric_limits<uint32_t>::max());
}

gfx::Size GetSizeFromProtobuf(const proto::Size& proto_size) {
  return gfx::Size(proto_size.width(), proto_size.height());
}

gfx::Rect GetRectFromProtobuf(const proto::Rect& proto_rect) {
  return gfx::Rect(proto_rect.x(), proto_rect.y(), proto_rect.width(),
                   proto_rect.height());
}

gfx::Transform GetTransformFromProtobuf(
    const proto::Transform& proto_transform) {
  gfx::Transform transform;

  // Note: There are no checks here that disallow a non-invertible transform
  // (for instance, if |scale_x| or |scale_y| is 0).
  transform.Scale(MakeNormal(proto_transform.scale_x()),
                  MakeNormal(proto_transform.scale_y()));

  transform.Rotate(MakeNormal(proto_transform.rotate()));

  transform.Translate(MakeNormal(proto_transform.translate_x()),
                      MakeNormal(proto_transform.translate_y()));

  return transform;
}

SkColor4f GetColorFromProtobuf(const proto::Color& color) {
  return SkColor4f{color.r(), color.g(), color.b(), color.a()};
}

// Mutates a gfx::Rect to ensure width and height are both at least min_size.
// Use in case e.g. a 0-width/height Rect would cause a validation error on
// deserialization.
void ExpandToMinSize(gfx::Rect* rect, int min_size) {
  if (rect->width() < min_size) {
    // grow width to min_size in +x direction
    // (may be clamped if x + min_size overflows)
    rect->set_width(min_size);
  }

  // if previous attempt failed due to overflow
  if (rect->width() < min_size) {
    // grow width to min_size in -x direction
    rect->Offset(-(min_size - rect->width()), 0);
    rect->set_width(min_size);
  }

  if (rect->height() < min_size) {
    // grow height to min_size in +y direction
    // (may be clamped if y + min_size overflows)
    rect->set_height(min_size);
  }

  // if previous attempt failed due to overflow
  if (rect->height() < min_size) {
    // grow height to min_size in -y direction
    rect->Offset(0, -(min_size - rect->height()));
    rect->set_height(min_size);
  }
}

class FuzzedCompositorFrameBuilder {
 public:
  FuzzedCompositorFrameBuilder() = default;

  FuzzedCompositorFrameBuilder(const FuzzedCompositorFrameBuilder&) = delete;
  FuzzedCompositorFrameBuilder& operator=(const FuzzedCompositorFrameBuilder&) =
      delete;

  ~FuzzedCompositorFrameBuilder() = default;

  FuzzedData Build(const proto::CompositorRenderPass& render_pass_spec);

 private:
  CompositorRenderPassId AddRenderPass(
      const proto::CompositorRenderPass& render_pass_spec);

  // Helper methods for AddRenderPass. Try* methods may return before
  // creating the quad in order to adhere to memory limits.
  void AddSolidColorDrawQuad(CompositorRenderPass* pass,
                             const gfx::Rect& rect,
                             const gfx::Rect& visible_rect,
                             const proto::DrawQuad& quad_spec);
  void TryAddTileDrawQuad(CompositorRenderPass* pass,
                          const gfx::Rect& rect,
                          const gfx::Rect& visible_rect,
                          const proto::DrawQuad& quad_spec);
  void TryAddRenderPassDrawQuad(CompositorRenderPass* pass,
                                const gfx::Rect& rect,
                                const gfx::Rect& visible_rect,
                                const proto::DrawQuad& quad_spec);

  // Configure the SharedQuadState to match the specifications in the protobuf,
  // if they are defined for this quad. Otherwise, use sensible defaults: match
  // the |rect| and |visible_rect| of the corresponding quad, and apply an
  // identity transform.
  void ConfigureSharedQuadState(SharedQuadState* shared_quad_state,
                                const proto::DrawQuad& quad_spec);

  // Records the intention to allocate enough memory for a bitmap of size
  // |size|, or returns false if the allocation would not be possible (the size
  // is 0 or the allocation would exceed kMaxMappedMemory).
  //
  // Should be called before AllocateFuzzedBitmap.
  bool TryReserveBitmapBytes(const gfx::Size& size);

  // Allocate and map memory for a bitmap filled with |color| and appends it to
  // |allocated_bitmaps|. Performs no checks to ensure that the bitmap will fit
  // in memory (see TryReserveBitmapBytes).
  FuzzedBitmap* AllocateFuzzedBitmap(const gfx::Size& size, SkColor4f color);

  // Number of bytes that have already been reserved for the allocation of
  // specific bitmaps/textures.
  uint64_t reserved_bytes_ = 0;

  CompositorRenderPassId::Generator pass_id_generator_;

  // Frame and data being built.
  FuzzedData data_;
};

FuzzedData FuzzedCompositorFrameBuilder::Build(
    const proto::CompositorRenderPass& render_pass_spec) {
  static FrameTokenGenerator next_frame_token;

  data_.frame.metadata.begin_frame_ack.frame_id = BeginFrameId(
      BeginFrameArgs::kManualSourceId, BeginFrameArgs::kStartingFrameNumber);
  data_.frame.metadata.begin_frame_ack.has_damage = true;
  data_.frame.metadata.frame_token = ++next_frame_token;
  data_.frame.metadata.device_scale_factor = 1;

  AddRenderPass(render_pass_spec);

  return std::move(data_);
}

CompositorRenderPassId FuzzedCompositorFrameBuilder::AddRenderPass(
    const proto::CompositorRenderPass& render_pass_spec) {
  auto pass = CompositorRenderPass::Create();
  gfx::Rect rp_output_rect =
      GetRectFromProtobuf(render_pass_spec.output_rect());
  gfx::Rect rp_damage_rect =
      GetRectFromProtobuf(render_pass_spec.damage_rect());

  // Handle constraints on RenderPass:
  // Ensure that |rp_output_rect| has non-zero area and that |rp_damage_rect|
  // is contained in |rp_output_rect|.
  ExpandToMinSize(&rp_output_rect, 1);
  rp_damage_rect.AdjustToFit(rp_output_rect);

  // Use an identity transform if |transform_to_root_target| is not defined.
  gfx::Transform transform_to_root_target =
      render_pass_spec.has_transform_to_root_target()
          ? GetTransformFromProtobuf(
                render_pass_spec.transform_to_root_target())
          : gfx::Transform();
  pass->SetNew(pass_id_generator_.GenerateNextId(), rp_output_rect,
               rp_damage_rect, transform_to_root_target);

  for (const proto::DrawQuad& quad_spec : render_pass_spec.quad_list()) {
    if (quad_spec.quad_case() == proto::DrawQuad::QUAD_NOT_SET) {
      continue;
    }

    gfx::Rect quad_rect = GetRectFromProtobuf(quad_spec.rect());
    gfx::Rect quad_visible_rect = GetRectFromProtobuf(quad_spec.visible_rect());

    // Handle constraints on DrawQuad:
    // Ensure that |quad_rect| has non-zero area and that |quad_visible_rect|
    // is contained in |quad_rect|.
    ExpandToMinSize(&quad_rect, 1);
    quad_visible_rect.AdjustToFit(quad_rect);

    switch (quad_spec.quad_case()) {
      case proto::DrawQuad::kSolidColorQuad: {
        AddSolidColorDrawQuad(pass.get(), quad_rect, quad_visible_rect,
                              quad_spec);
        break;
      }
      case proto::DrawQuad::kTileQuad: {
        TryAddTileDrawQuad(pass.get(), quad_rect, quad_visible_rect, quad_spec);
        break;
      }
      case proto::DrawQuad::kRenderPassQuad: {
        TryAddRenderPassDrawQuad(pass.get(), quad_rect, quad_visible_rect,
                                 quad_spec);
        break;
      }
      case proto::DrawQuad::QUAD_NOT_SET: {
        NOTREACHED_IN_MIGRATION();
      }
    }
  }
  data_.frame.render_pass_list.push_back(std::move(pass));
  return data_.frame.render_pass_list.back()->id;
}

// TODO(crbug.com/40219248): Move proto::DrawQuad to SkColor4f
void FuzzedCompositorFrameBuilder::AddSolidColorDrawQuad(
    CompositorRenderPass* pass,
    const gfx::Rect& rect,
    const gfx::Rect& visible_rect,
    const proto::DrawQuad& quad_spec) {
  auto* shared_quad_state = pass->CreateAndAppendSharedQuadState();
  ConfigureSharedQuadState(shared_quad_state, quad_spec);
  auto* quad = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  quad->SetNew(shared_quad_state, rect, visible_rect,
               GetColorFromProtobuf(quad_spec.solid_color_quad().color()),
               quad_spec.solid_color_quad().force_anti_aliasing_off());
}

void FuzzedCompositorFrameBuilder::TryAddTileDrawQuad(
    CompositorRenderPass* pass,
    const gfx::Rect& rect,
    const gfx::Rect& visible_rect,
    const proto::DrawQuad& quad_spec) {
  gfx::Size tile_size =
      GetSizeFromProtobuf(quad_spec.tile_quad().texture_size());

  // Skip TileDrawQuads with bitmaps that cannot be allocated (size is 0
  // or would overflow our limit on allocated memory for this
  // CompositorFrame.)
  if (!TryReserveBitmapBytes(tile_size)) {
    VLOG(1) << "Skipping TileDrawQuad: bitmap of size " << tile_size.ToString()
            << " can't be allocated.";
    return;
  }

  FuzzedBitmap* fuzzed_bitmap = AllocateFuzzedBitmap(
      tile_size, GetColorFromProtobuf(quad_spec.tile_quad().texture_color()));
  TransferableResource transferable_resource =
      TransferableResource::MakeSoftwareSharedBitmap(
          fuzzed_bitmap->id, gpu::SyncToken(), fuzzed_bitmap->size,
          SinglePlaneFormat::kRGBA_8888);

  auto* shared_quad_state = pass->CreateAndAppendSharedQuadState();
  ConfigureSharedQuadState(shared_quad_state, quad_spec);
  auto* quad = pass->CreateAndAppendDrawQuad<TileDrawQuad>();
  quad->SetNew(
      shared_quad_state, rect, visible_rect,
      quad_spec.tile_quad().needs_blending(), transferable_resource.id,
      gfx::RectF(GetRectFromProtobuf(quad_spec.tile_quad().tex_coord_rect())),
      tile_size, quad_spec.tile_quad().is_premultiplied(),
      quad_spec.tile_quad().nearest_neighbor(),
      quad_spec.tile_quad().force_anti_aliasing_off());

  data_.frame.resource_list.push_back(transferable_resource);
}

void FuzzedCompositorFrameBuilder::TryAddRenderPassDrawQuad(
    CompositorRenderPass* pass,
    const gfx::Rect& rect,
    const gfx::Rect& visible_rect,
    const proto::DrawQuad& quad_spec) {
  // Since child RenderPasses are allocated as textures, skip
  // RenderPasses that would overflow our limit on allocated memory for
  // this CompositorFrame.
  gfx::Size render_pass_size =
      GetRectFromProtobuf(
          quad_spec.render_pass_quad().render_pass().output_rect())
          .size();

  // RenderPass texture dimensions are rounded up to a
  // multiple of 64 pixels to reduce fragmentation.
  constexpr int multiple = 64;
  if (!cc::MathUtil::VerifyRoundup(render_pass_size.width(), multiple) ||
      !cc::MathUtil::VerifyRoundup(render_pass_size.height(), multiple)) {
    VLOG(1) << "Skipping CompositorRenderPassDrawQuad: bitmap of size "
            << render_pass_size.ToString() << " can't be allocated.";
    return;
  }
  render_pass_size = gfx::Size(
      cc::MathUtil::UncheckedRoundUp(render_pass_size.width(), multiple),
      cc::MathUtil::UncheckedRoundUp(render_pass_size.height(), multiple));

  if (!TryReserveBitmapBytes(render_pass_size)) {
    VLOG(1) << "Skipping CompositorRenderPassDrawQuad: bitmap of size "
            << render_pass_size.ToString() << " can't be allocated.";
    return;
  }

  // Build the child RenderPass and add it to the frame's
  // CompositorRenderPassList.
  CompositorRenderPassId child_pass_id =
      AddRenderPass(quad_spec.render_pass_quad().render_pass());

  // Unless a tex_coord_rect is defined in the protobuf specification,
  // a good default is a rectangle covering the entire quad.
  gfx::RectF tex_coord_rect = gfx::RectF(
      quad_spec.render_pass_quad().has_tex_coord_rect()
          ? GetRectFromProtobuf(quad_spec.render_pass_quad().tex_coord_rect())
          : rect);

  auto* shared_quad_state = pass->CreateAndAppendSharedQuadState();
  ConfigureSharedQuadState(shared_quad_state, quad_spec);
  auto* quad = pass->CreateAndAppendDrawQuad<CompositorRenderPassDrawQuad>();
  quad->SetNew(shared_quad_state, rect, visible_rect, child_pass_id,
               /*mask_resource_id=*/ResourceId(),
               /*mask_uv_rect=*/gfx::RectF(),
               /*mask_texture_size=*/gfx::Size(),
               /*filters_scale=*/gfx::Vector2dF(),
               /*filters_origin=*/gfx::PointF(),
               /*tex_coord_rect=*/tex_coord_rect,
               /*force_anti_aliasing_off=*/false,
               /*backdrop_filter_quality=*/1.0);
}

void FuzzedCompositorFrameBuilder::ConfigureSharedQuadState(
    SharedQuadState* shared_quad_state,
    const proto::DrawQuad& quad_spec) {
  if (quad_spec.has_sqs()) {
    std::optional<gfx::Rect> clip_rect;
    if (quad_spec.sqs().is_clipped()) {
      clip_rect = GetRectFromProtobuf(quad_spec.sqs().clip_rect());
    }
    shared_quad_state->SetAll(
        GetTransformFromProtobuf(quad_spec.sqs().transform()),
        GetRectFromProtobuf(quad_spec.sqs().layer_rect()),
        GetRectFromProtobuf(quad_spec.sqs().visible_rect()),
        gfx::MaskFilterInfo(), clip_rect, quad_spec.sqs().are_contents_opaque(),
        Normalize(quad_spec.sqs().opacity()), SkBlendMode::kSrcOver,
        quad_spec.sqs().sorting_context_id(), /*layer_id=*/0u,
        /*fast_rounded_corner*/ false);
  } else {
    gfx::Transform transform;

    if (quad_spec.quad_case() == proto::DrawQuad::kRenderPassQuad &&
        quad_spec.render_pass_quad()
            .render_pass()
            .has_transform_to_root_target()) {
      transform = GetTransformFromProtobuf(quad_spec.render_pass_quad()
                                               .render_pass()
                                               .transform_to_root_target());
    }

    shared_quad_state->SetAll(
        transform, GetRectFromProtobuf(quad_spec.rect()),
        GetRectFromProtobuf(quad_spec.visible_rect()), gfx::MaskFilterInfo(),
        /*clip_rect=*/std::nullopt, /*are_contents_opaque=*/true,
        /*opacity=*/1.0, SkBlendMode::kSrcOver, /*sorting_context_id=*/0,
        /*layer_id=*/0u, /*fast_rounded_corner*/ false);
  }
}

bool FuzzedCompositorFrameBuilder::TryReserveBitmapBytes(
    const gfx::Size& size) {
  uint64_t bitmap_bytes;
  if (!ResourceSizes::MaybeSizeInBytes<uint64_t>(
          size, SinglePlaneFormat::kRGBA_8888, &bitmap_bytes) ||
      bitmap_bytes > kMaxTextureMemory - reserved_bytes_) {
    return false;
  }

  reserved_bytes_ += bitmap_bytes;
  return true;
}

FuzzedBitmap* FuzzedCompositorFrameBuilder::AllocateFuzzedBitmap(
    const gfx::Size& size,
    SkColor4f color) {
  SharedBitmapId shared_bitmap_id = SharedBitmap::GenerateId();
  base::MappedReadOnlyRegion shm = bitmap_allocation::AllocateSharedBitmap(
      size, SinglePlaneFormat::kRGBA_8888);

  SkBitmap bitmap;
  SkImageInfo info = SkImageInfo::MakeN32Premul(size.width(), size.height());
  bitmap.installPixels(info, shm.mapping.memory(), info.minRowBytes());
  bitmap.eraseColor(color);

  data_.allocated_bitmaps.push_back(
      {shared_bitmap_id, size, std::move(shm.region)});

  return &data_.allocated_bitmaps.back();
}

}  // namespace

FuzzedBitmap::FuzzedBitmap(const SharedBitmapId& id,
                           const gfx::Size& size,
                           base::ReadOnlySharedMemoryRegion shared_region)
    : id(id), size(size), shared_region(std::move(shared_region)) {}
FuzzedBitmap::~FuzzedBitmap() = default;
FuzzedBitmap::FuzzedBitmap(FuzzedBitmap&& other) noexcept = default;

FuzzedData::FuzzedData() = default;
FuzzedData::~FuzzedData() = default;
FuzzedData::FuzzedData(FuzzedData&& other) noexcept = default;

FuzzedData BuildFuzzedCompositorFrame(
    const proto::CompositorRenderPass& render_pass_spec) {
  return FuzzedCompositorFrameBuilder().Build(render_pass_spec);
}

}  // namespace viz
