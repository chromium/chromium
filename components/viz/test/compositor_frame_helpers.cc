// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/test/compositor_frame_helpers.h"

#include <memory>
#include <set>
#include <utility>

#include "base/functional/bind.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/quads/compositor_render_pass.h"
#include "components/viz/common/quads/compositor_render_pass_draw_quad.h"
#include "components/viz/common/quads/shared_element_draw_quad.h"
#include "components/viz/common/quads/shared_quad_state.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/surface_draw_quad.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/resources/resource_id.h"

namespace viz {
namespace {

constexpr gfx::Rect kDefaultOutputRect(20, 20);
constexpr gfx::Rect kDefaultDamageRect(0, 0);

constexpr CompositorRenderPassId kInvalidRenderPassId;

// A stub CopyOutputRequest for testing that ignores the result.
class StubCopyOutputRequest : public CopyOutputRequest {
 public:
  StubCopyOutputRequest()
      : CopyOutputRequest(
            ResultFormat::RGBA,
            ResultDestination::kSystemMemory,
            base::BindOnce([](std::unique_ptr<CopyOutputResult>) {})) {}
  ~StubCopyOutputRequest() override = default;

  base::WeakPtr<CopyOutputRequest> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<CopyOutputRequest> weak_factory_{this};
};

}  // namespace

RenderPassBuilder::RenderPassBuilder(CompositorRenderPassId id,
                                     const gfx::Size& output_size)
    : RenderPassBuilder(id, gfx::Rect(output_size)) {}

RenderPassBuilder::RenderPassBuilder(CompositorRenderPassId id,
                                     const gfx::Rect& output_rect)
    : pass_(CompositorRenderPass::Create()) {
  CHECK(!output_rect.IsEmpty());
  pass_->id = id;
  pass_->output_rect = output_rect;
  pass_->damage_rect = output_rect;  // Full damage by default.
}

RenderPassBuilder::RenderPassBuilder(const gfx::Size& output_size)
    : RenderPassBuilder(kInvalidRenderPassId, output_size) {}

RenderPassBuilder::RenderPassBuilder(const gfx::Rect& output_rect)
    : RenderPassBuilder(kInvalidRenderPassId, output_rect) {}

RenderPassBuilder::~RenderPassBuilder() = default;

bool RenderPassBuilder::IsValid() const {
  return pass_ && !pass_->quad_list.empty();
}

std::unique_ptr<CompositorRenderPass> RenderPassBuilder::Build() {
  return std::move(pass_);
}

RenderPassBuilder& RenderPassBuilder::SetDamageRect(
    const gfx::Rect& damage_rect) {
  CHECK(pass_->output_rect.Contains(damage_rect));
  pass_->damage_rect = damage_rect;
  return *this;
}

RenderPassBuilder& RenderPassBuilder::SetCacheRenderPass(bool val) {
  pass_->cache_render_pass = val;
  return *this;
}

RenderPassBuilder& RenderPassBuilder::SetHasDamageFromContributingContent(
    bool val) {
  pass_->has_damage_from_contributing_content = val;
  return *this;
}

RenderPassBuilder& RenderPassBuilder::AddFilter(
    const cc::FilterOperation& filter) {
  pass_->filters.Append(filter);
  return *this;
}

RenderPassBuilder& RenderPassBuilder::AddBackdropFilter(
    const cc::FilterOperation& filter) {
  pass_->backdrop_filters.Append(filter);
  return *this;
}

RenderPassBuilder& RenderPassBuilder::SetTransformToRootTarget(
    const gfx::Transform& transform) {
  pass_->transform_to_root_target = transform;
  return *this;
}

RenderPassBuilder& RenderPassBuilder::AddStubCopyOutputRequest(
    base::WeakPtr<CopyOutputRequest>* request_out) {
  auto request = std::make_unique<StubCopyOutputRequest>();
  if (request_out)
    *request_out = request->GetWeakPtr();
  pass_->copy_requests.push_back(std::move(request));
  return *this;
}

RenderPassBuilder& RenderPassBuilder::AddSharedElementQuad(
    const gfx::Rect& rect,
    const ViewTransitionElementResourceId& id) {
  auto* sqs = AppendDefaultSharedQuadState(rect, rect);
  auto* quad = pass_->CreateAndAppendDrawQuad<SharedElementDrawQuad>();
  quad->SetNew(sqs, rect, rect, id);

  return *this;
}

RenderPassBuilder& RenderPassBuilder::AddSolidColorQuad(
    const gfx::Rect& rect,
    SkColor4f color,
    SolidColorQuadParms params) {
  return AddSolidColorQuad(rect, rect, color, params);
}

RenderPassBuilder& RenderPassBuilder::AddSolidColorQuad(
    const gfx::Rect& rect,
    const gfx::Rect& visible_rect,
    SkColor4f color,
    SolidColorQuadParms params) {
  auto* sqs = AppendDefaultSharedQuadState(rect, visible_rect);
  auto* quad = pass_->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  quad->SetNew(sqs, rect, visible_rect, color, params.force_anti_aliasing_off);

  return *this;
}

RenderPassBuilder& RenderPassBuilder::AddSurfaceQuad(
    const gfx::Rect& rect,
    const SurfaceRange& surface_range,
    const SurfaceQuadParams& params) {
  return AddSurfaceQuad(rect, rect, surface_range, params);
}

RenderPassBuilder& RenderPassBuilder::AddSurfaceQuad(
    const gfx::Rect& rect,
    const gfx::Rect& visible_rect,
    const SurfaceRange& surface_range,
    const SurfaceQuadParams& params) {
  auto* sqs = AppendDefaultSharedQuadState(rect, visible_rect);
  auto* quad = pass_->CreateAndAppendDrawQuad<SurfaceDrawQuad>();
  // TODO (crbug.com/1308932) Change SurfaceQuadParams to use SKColor4f
  quad->SetNew(sqs, rect, visible_rect, surface_range,
               params.default_background_color,
               params.stretch_content_to_fill_bounds);
  quad->is_reflection = params.is_reflection;
  quad->allow_merge = params.allow_merge;

  return *this;
}

RenderPassBuilder& RenderPassBuilder::AddRenderPassQuad(
    const gfx::Rect& rect,
    CompositorRenderPassId id,
    const RenderPassQuadParams& params) {
  return AddRenderPassQuad(rect, rect, id, params);
}

RenderPassBuilder& RenderPassBuilder::AddRenderPassQuad(
    const gfx::Rect& rect,
    const gfx::Rect& visible_rect,
    CompositorRenderPassId id,
    const RenderPassQuadParams& params) {
  auto* sqs = AppendDefaultSharedQuadState(rect, visible_rect);
  auto* quad = pass_->CreateAndAppendDrawQuad<CompositorRenderPassDrawQuad>();
  quad->SetAll(
      sqs, rect, visible_rect, params.needs_blending, id, kInvalidResourceId,
      gfx::RectF(), gfx::Size(), gfx::Vector2dF(1.0f, 1.0f), gfx::PointF(),
      gfx::RectF(), params.force_anti_aliasing_off,
      /*backdrop_filter_quality=*/1.0f, params.intersects_damage_under);

  return *this;
}

RenderPassBuilder& RenderPassBuilder::AddTextureQuad(
    const gfx::Rect& rect,
    ResourceId resource_id,
    const TextureQuadParams& params) {
  return AddTextureQuad(rect, rect, resource_id, params);
}

RenderPassBuilder& RenderPassBuilder::AddTextureQuad(
    const gfx::Rect& rect,
    const gfx::Rect& visible_rect,
    ResourceId resource_id,
    const TextureQuadParams& params) {
  auto* sqs = AppendDefaultSharedQuadState(rect, visible_rect);
  auto* quad = pass_->CreateAndAppendDrawQuad<TextureDrawQuad>();
  quad->SetAll(sqs, rect, visible_rect, params.needs_blending, resource_id,
               rect.size(), params.premultiplied_alpha, gfx::PointF(0.0f, 0.0f),
               gfx::PointF(1.0f, 1.0f), params.background_color, params.flipped,
               params.nearest_neighbor, params.secure_output_only,
               params.protected_video_type);

  return *this;
}

RenderPassBuilder& RenderPassBuilder::SetQuadToTargetTransform(
    const gfx::Transform& transform) {
  GetLastQuadSharedQuadState()->quad_to_target_transform = transform;
  return *this;
}

RenderPassBuilder& RenderPassBuilder::SetQuadToTargetTranslation(
    int translate_x,
    int translate_y) {
  gfx::Transform transform;
  transform.Translate(translate_x, translate_y);
  return SetQuadToTargetTransform(transform);
}

RenderPassBuilder& RenderPassBuilder::SetQuadOpacity(float opacity) {
  CHECK_GE(opacity, 0.0f);
  CHECK_LE(opacity, 1.0f);
  GetLastQuadSharedQuadState()->opacity = opacity;
  return *this;
}

RenderPassBuilder& RenderPassBuilder::SetQuadClipRect(
    std::optional<gfx::Rect> clip_rect) {
  CHECK(!clip_rect || pass_->output_rect.Contains(*clip_rect));
  GetLastQuadSharedQuadState()->clip_rect = clip_rect;
  return *this;
}

RenderPassBuilder& RenderPassBuilder::SetQuadDamageRect(
    const gfx::Rect& damage_rect) {
  CHECK(!pass_->quad_list.empty());
  DrawQuad* quad = pass_->quad_list.back();

  if (quad->material == DrawQuad::Material::kTextureContent) {
    auto* texture_quad = static_cast<TextureDrawQuad*>(quad);
    texture_quad->damage_rect = damage_rect;
  } else {
    NOTREACHED_IN_MIGRATION();
  }

  pass_->has_per_quad_damage = true;
  return *this;
}

RenderPassBuilder& RenderPassBuilder::SetBlendMode(SkBlendMode blend_mode) {
  GetLastQuadSharedQuadState()->blend_mode = blend_mode;
  return *this;
}

RenderPassBuilder& RenderPassBuilder::SetMaskFilter(
    const gfx::MaskFilterInfo& mask_filter_info,
    bool is_fast_rounded_corner) {
  auto* sqs = GetLastQuadSharedQuadState();
  sqs->mask_filter_info = mask_filter_info;
  sqs->is_fast_rounded_corner = is_fast_rounded_corner;
  return *this;
}

RenderPassBuilder& RenderPassBuilder::SetQuadLayerId(uint32_t layer_id) {
  auto* sqs = GetLastQuadSharedQuadState();
  sqs->layer_id = layer_id;
  return *this;
}

RenderPassBuilder& RenderPassBuilder::SetQuadOffsetTag(const OffsetTag& tag) {
  auto* sqs = GetLastQuadSharedQuadState();
  sqs->offset_tag = tag;
  return *this;
}

RenderPassBuilder& RenderPassBuilder::SetQuadMaskFilterInfo(
    const gfx::MaskFilterInfo& mask_filter_info) {
  auto* sqs = GetLastQuadSharedQuadState();
  sqs->mask_filter_info = mask_filter_info;
  return *this;
}

SharedQuadState* RenderPassBuilder::AppendDefaultSharedQuadState(
    const gfx::Rect rect,
    const gfx::Rect visible_rect) {
  SharedQuadState* sqs = pass_->CreateAndAppendSharedQuadState();
  sqs->SetAll(gfx::Transform(), rect, visible_rect, gfx::MaskFilterInfo(),
              /*clip=*/std::nullopt, /*contents_opaque=*/false,
              /*opacity_f=*/1.0f, SkBlendMode::kSrcOver, /*sorting_context=*/0,
              /*layer_id=*/0u, /*fast_rounded_corner=*/false);
  return sqs;
}

SharedQuadState* RenderPassBuilder::GetLastQuadSharedQuadState() {
  CHECK(!pass_->quad_list.empty());
  CHECK(!pass_->shared_quad_state_list.empty());
  CHECK(pass_->shared_quad_state_list.back() ==
        pass_->quad_list.back()->shared_quad_state);

  return pass_->shared_quad_state_list.back();
}

CompositorFrameBuilder::CompositorFrameBuilder() {
  frame_ = MakeInitCompositorFrame();
}

CompositorFrameBuilder::~CompositorFrameBuilder() = default;

CompositorFrame CompositorFrameBuilder::Build() {
  CompositorFrame temp_frame(std::move(frame_.value()));
  frame_ = MakeInitCompositorFrame();
  return temp_frame;
}

CompositorFrameBuilder& CompositorFrameBuilder::AddDefaultRenderPass() {
  return AddRenderPass(kDefaultOutputRect, kDefaultDamageRect);
}

CompositorFrameBuilder& CompositorFrameBuilder::AddRenderPass(
    const gfx::Rect& output_rect,
    const gfx::Rect& damage_rect) {
  auto pass = CompositorRenderPass::Create();
  pass->SetNew(render_pass_id_generator_.GenerateNextId(), output_rect,
               damage_rect, gfx::Transform());
  frame_->render_pass_list.push_back(std::move(pass));
  return *this;
}

CompositorFrameBuilder& CompositorFrameBuilder::AddRenderPass(
    std::unique_ptr<CompositorRenderPass> render_pass) {
  // Give the render pass a unique id if one hasn't been assigned.
  if (render_pass->id.is_null())
    render_pass->id = render_pass_id_generator_.GenerateNextId();

  // Populate referenced_surfaces if there are any SurfaceDrawQuads.
  for (auto* quad : render_pass->quad_list) {
    if (quad->material == DrawQuad::Material::kSurfaceContent) {
      auto* surface_quad = SurfaceDrawQuad::MaterialCast(quad);
      frame_->metadata.referenced_surfaces.push_back(
          surface_quad->surface_range);
    }
  }

  frame_->render_pass_list.push_back(std::move(render_pass));
  return *this;
}

CompositorFrameBuilder& CompositorFrameBuilder::AddRenderPass(
    RenderPassBuilder& builder) {
  CHECK(builder.IsValid());
  AddRenderPass(builder.Build());
  return *this;
}

CompositorFrameBuilder& CompositorFrameBuilder::SetRenderPassList(
    CompositorRenderPassList render_pass_list) {
  CHECK(frame_->render_pass_list.empty());

  // Call AddRenderPass() for each pass as it contains additional logic.
  for (auto& render_pass : render_pass_list)
    AddRenderPass(std::move(render_pass));

  return *this;
}

CompositorFrameBuilder& CompositorFrameBuilder::AddTransferableResource(
    TransferableResource resource) {
  frame_->resource_list.push_back(std::move(resource));
  return *this;
}

CompositorFrameBuilder& CompositorFrameBuilder::SetTransferableResources(
    std::vector<TransferableResource> resource_list) {
  DCHECK(frame_->resource_list.empty());
  frame_->resource_list = std::move(resource_list);
  return *this;
}

CompositorFrameBuilder& CompositorFrameBuilder::PopulateResources() {
  PopulateTransferableResources(frame_.value());
  return *this;
}

CompositorFrameBuilder& CompositorFrameBuilder::SetBeginFrameAck(
    const BeginFrameAck& ack) {
  frame_->metadata.begin_frame_ack = ack;
  return *this;
}

CompositorFrameBuilder& CompositorFrameBuilder::SetBeginFrameSourceId(
    uint64_t source_id) {
  frame_->metadata.begin_frame_ack.frame_id.source_id = source_id;
  return *this;
}

CompositorFrameBuilder& CompositorFrameBuilder::SetDeviceScaleFactor(
    float device_scale_factor) {
  frame_->metadata.device_scale_factor = device_scale_factor;
  return *this;
}

CompositorFrameBuilder& CompositorFrameBuilder::AddLatencyInfo(
    ui::LatencyInfo latency_info) {
  frame_->metadata.latency_info.push_back(std::move(latency_info));
  return *this;
}

CompositorFrameBuilder& CompositorFrameBuilder::AddLatencyInfos(
    std::vector<ui::LatencyInfo> latency_info) {
  if (frame_->metadata.latency_info.empty()) {
    frame_->metadata.latency_info.swap(latency_info);
  } else {
    for (auto& latency : latency_info) {
      frame_->metadata.latency_info.push_back(std::move(latency));
    }
  }
  return *this;
}

CompositorFrameBuilder& CompositorFrameBuilder::SetActivationDependencies(
    std::vector<SurfaceId> activation_dependencies) {
  frame_->metadata.activation_dependencies = std::move(activation_dependencies);
  return *this;
}

CompositorFrameBuilder& CompositorFrameBuilder::SetDeadline(
    const FrameDeadline& deadline) {
  frame_->metadata.deadline = deadline;
  return *this;
}

CompositorFrameBuilder& CompositorFrameBuilder::SetReferencedSurfaces(
    std::vector<SurfaceRange> referenced_surfaces) {
  frame_->metadata.referenced_surfaces = std::move(referenced_surfaces);
  return *this;
}

CompositorFrameBuilder& CompositorFrameBuilder::SetSendFrameTokenToEmbedder(
    bool send) {
  DCHECK(frame_->metadata.frame_token);
  frame_->metadata.send_frame_token_to_embedder = send;
  return *this;
}

CompositorFrameBuilder& CompositorFrameBuilder::SetIsHandlingInteraction(
    bool is_handling_interaction) {
  frame_->metadata.is_handling_interaction = is_handling_interaction;
  return *this;
}

CompositorFrameBuilder& CompositorFrameBuilder::AddDelegatedInkMetadata(
    const gfx::DelegatedInkMetadata& metadata) {
  frame_->metadata.delegated_ink_metadata =
      std::make_unique<gfx::DelegatedInkMetadata>(metadata);
  return *this;
}

CompositorFrameBuilder& CompositorFrameBuilder::AddOffsetTagDefinition(
    const OffsetTagDefinition& definition) {
  frame_->metadata.offset_tag_definitions.push_back(definition);
  return *this;
}

CompositorFrame CompositorFrameBuilder::MakeInitCompositorFrame() const {
  static FrameTokenGenerator next_token;
  CompositorFrame frame;
  frame.metadata.begin_frame_ack = BeginFrameAck::CreateManualAckWithDamage();
  frame.metadata.device_scale_factor = 1.f;
  frame.metadata.frame_token = ++next_token;
  return frame;
}

CompositorRenderPassList CopyRenderPasses(
    const CompositorRenderPassList& render_pass_list) {
  CompositorRenderPassList copy_list;
  for (auto& render_pass : render_pass_list)
    copy_list.push_back(render_pass->DeepCopy());
  return copy_list;
}

CompositorFrame MakeDefaultCompositorFrame(uint64_t source_id) {
  return CompositorFrameBuilder()
      .AddDefaultRenderPass()
      .SetBeginFrameSourceId(source_id)
      .Build();
}

CompositorFrame MakeCompositorFrame(
    std::unique_ptr<CompositorRenderPass> render_pass) {
  return CompositorFrameBuilder()
      .AddRenderPass(std::move(render_pass))
      .PopulateResources()
      .Build();
}

CompositorFrame MakeCompositorFrame(CompositorRenderPassList render_pass_list) {
  return CompositorFrameBuilder()
      .SetRenderPassList(std::move(render_pass_list))
      .PopulateResources()
      .Build();
}

AggregatedFrame MakeDefaultAggregatedFrame(size_t num_render_passes) {
  static AggregatedRenderPassId::Generator id_generator;
  AggregatedFrame frame;
  for (size_t i = 0; i < num_render_passes; ++i) {
    frame.render_pass_list.push_back(std::make_unique<AggregatedRenderPass>());
    frame.render_pass_list.back()->SetNew(id_generator.GenerateNextId(),
                                          kDefaultOutputRect,
                                          kDefaultDamageRect, gfx::Transform());
  }
  return frame;
}

CompositorFrame MakeDefaultInteractiveCompositorFrame(uint64_t source_id) {
  return CompositorFrameBuilder()
      .AddDefaultRenderPass()
      .SetBeginFrameSourceId(source_id)
      .SetIsHandlingInteraction(true)
      .Build();
}

CompositorFrame MakeEmptyCompositorFrame() {
  return CompositorFrameBuilder().Build();
}

void PopulateTransferableResources(CompositorFrame& frame) {
  DCHECK(frame.resource_list.empty());

  std::set<ResourceId> resources_added;
  for (auto& render_pass : frame.render_pass_list) {
    for (auto* quad : render_pass->quad_list) {
      for (ResourceId resource_id : quad->resources) {
        if (resource_id == kInvalidResourceId)
          continue;

        // Adds a TransferableResource the first time seeing a ResourceId.
        if (resources_added.insert(resource_id).second) {
          frame.resource_list.push_back(
              TransferableResource::MakeSoftwareSharedBitmap(
                  SharedBitmap::GenerateId(), gpu::SyncToken(),
                  quad->rect.size(), SinglePlaneFormat::kRGBA_8888));
          frame.resource_list.back().id = resource_id;
        }
      }
    }
  }
}

}  // namespace viz
