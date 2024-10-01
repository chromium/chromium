// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_TEST_COMPOSITOR_FRAME_HELPERS_H_
#define COMPONENTS_VIZ_TEST_COMPOSITOR_FRAME_HELPERS_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/quads/compositor_render_pass.h"
#include "components/viz/common/quads/frame_deadline.h"
#include "components/viz/common/quads/offset_tag.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "components/viz/service/display/aggregated_frame.h"
#include "third_party/skia/include/core/SkBlendMode.h"
#include "ui/gfx/video_types.h"
#include "ui/latency/latency_info.h"

namespace viz {

class CopyOutputRequest;

// Note that QuadParam structs should not have a user defined constructor to
// allow the use designated initializers.

struct SolidColorQuadParms {
  bool force_anti_aliasing_off = false;
};

struct SurfaceQuadParams {
  SkColor4f default_background_color = SkColors::kWhite;
  bool stretch_content_to_fill_bounds = false;
  bool is_reflection = false;
  bool allow_merge = true;
};

struct RenderPassQuadParams {
  bool needs_blending = true;
  bool force_anti_aliasing_off = false;
  bool intersects_damage_under = true;
};

struct TextureQuadParams {
  bool needs_blending = false;
  bool premultiplied_alpha = false;
  SkColor4f background_color = SkColors::kGreen;
  bool flipped = false;
  bool nearest_neighbor = false;
  bool secure_output_only = false;
  gfx::ProtectedVideoType protected_video_type =
      gfx::ProtectedVideoType::kClear;
};

// Helper to build a CompositorRenderPass and add quads to it. By default the
// CompositorRenderPass will have full damage. Functionality is broken down into
// methods to modify render pass attributes, methods to add new quads and
// methods to modify SharedQuadState for the last quad added.
class RenderPassBuilder {
 public:
  RenderPassBuilder(CompositorRenderPassId id, const gfx::Size& output_size);
  RenderPassBuilder(CompositorRenderPassId id, const gfx::Rect& output_rect);

  // Constructors for use with CompositorFrameBuilder when the specific render
  // pass id doesn't matter. CompositorFrameBuilder will auto assign valid ids.
  explicit RenderPassBuilder(const gfx::Size& output_size);
  explicit RenderPassBuilder(const gfx::Rect& output_rect);

  RenderPassBuilder(const RenderPassBuilder& other) = delete;
  RenderPassBuilder& operator=(const RenderPassBuilder& other) = delete;
  ~RenderPassBuilder();

  // Checks basic validity, like the render pass hasn't already been built and
  // there is at least one quad.
  bool IsValid() const;

  // Returns the CompositorRenderPass and leaves |this| in an invalid state.
  std::unique_ptr<CompositorRenderPass> Build();

  // Methods to modify the CompositorRenderPass start here.
  RenderPassBuilder& SetDamageRect(const gfx::Rect& damage_rect);
  RenderPassBuilder& SetCacheRenderPass(bool val);
  RenderPassBuilder& SetHasDamageFromContributingContent(bool val);
  RenderPassBuilder& AddFilter(const cc::FilterOperation& filter);
  RenderPassBuilder& AddBackdropFilter(const cc::FilterOperation& filter);
  RenderPassBuilder& SetTransformToRootTarget(const gfx::Transform& transform);

  // Creates a new stub CopyOutputRequest and adds it to the render pass. If
  // |request_out| is not null the pointer will set to the newly created
  // request.
  //
  // Note that |request_out| is a WeakPtr because the CopyOutputRequest lifetime
  // is difficult to reason about as ownership can be transferred in many
  // places.
  RenderPassBuilder& AddStubCopyOutputRequest(
      base::WeakPtr<CopyOutputRequest>* request_out = nullptr);

  // Methods to add DrawQuads start here. The methods are structured so that the
  // most important attributes on the quad are function parameters. Less
  // important attributes are stored in an optional struct parameter. The
  // optional params struct is POD so that designated initializers can be used
  // to construct a new object with specified parameters overridden.
  RenderPassBuilder& AddSharedElementQuad(
      const gfx::Rect& rect,
      const ViewTransitionElementResourceId& id);
  RenderPassBuilder& AddSolidColorQuad(const gfx::Rect& rect,
                                       SkColor4f color,
                                       SolidColorQuadParms params = {});
  RenderPassBuilder& AddSolidColorQuad(const gfx::Rect& rect,
                                       const gfx::Rect& visible_rect,
                                       SkColor4f color,
                                       SolidColorQuadParms params = {});

  RenderPassBuilder& AddSurfaceQuad(const gfx::Rect& rect,
                                    const SurfaceRange& surface_range,
                                    const SurfaceQuadParams& params = {});
  RenderPassBuilder& AddSurfaceQuad(const gfx::Rect& rect,
                                    const gfx::Rect& visible_rect,
                                    const SurfaceRange& surface_range,
                                    const SurfaceQuadParams& params = {});

  RenderPassBuilder& AddRenderPassQuad(const gfx::Rect& rect,
                                       CompositorRenderPassId id,
                                       const RenderPassQuadParams& params = {});
  RenderPassBuilder& AddRenderPassQuad(const gfx::Rect& rect,
                                       const gfx::Rect& visible_rect,
                                       CompositorRenderPassId id,
                                       const RenderPassQuadParams& params = {});

  RenderPassBuilder& AddTextureQuad(const gfx::Rect& rect,
                                    ResourceId resource_id,
                                    const TextureQuadParams& params = {});
  RenderPassBuilder& AddTextureQuad(const gfx::Rect& rect,
                                    const gfx::Rect& visible_rect,
                                    ResourceId resource_id,
                                    const TextureQuadParams& params = {});

  // Methods to modify the last DrawQuad's SharedQuadState start here. Note that
  // at least one quad must have been added to the render pass before calling
  // these.

  // Sets SharedQuadState::quad_to_target_transform for the last quad.
  RenderPassBuilder& SetQuadToTargetTransform(const gfx::Transform& transform);

  // Sets SharedQuadState::quad_to_target_transform for the last quad with a
  // transform that has the specified translation components.
  RenderPassBuilder& SetQuadToTargetTranslation(int translate_x,
                                                int translate_y);

  // Sets the SharedQuadState::opacity for the last quad.
  RenderPassBuilder& SetQuadOpacity(float opacity);

  // Sets SharedQuadState::clip_rect for the last quad.
  RenderPassBuilder& SetQuadClipRect(std::optional<gfx::Rect> clip_rect);

  // Sets the damage_rect for the last quad. This is only valid to call if the
  // last quad has a `damage_rect` member.
  RenderPassBuilder& SetQuadDamageRect(const gfx::Rect& damage_rect);

  // Sets SharedQuadState::blend_mode for the last quad.
  RenderPassBuilder& SetBlendMode(SkBlendMode blend_mode);

  // Sets SharedQuadState::mask_filter_info and
  // SharedQuadState::is_fast_rounded_corner for the last quad.
  RenderPassBuilder& SetMaskFilter(const gfx::MaskFilterInfo& mask_filter_info,
                                   bool is_fast_rounded_corner);

  // Sets SharedQuadState::layer_id for the last quad.
  RenderPassBuilder& SetQuadLayerId(uint32_t layer_id);

  // Sets SharedQuadState::offset_tag for the last quad.
  RenderPassBuilder& SetQuadOffsetTag(const OffsetTag& tag);

  // Sets SharedQuadState::mask_filter_info for the last quad.
  RenderPassBuilder& SetQuadMaskFilterInfo(
      const gfx::MaskFilterInfo& mask_filter_info);

 private:
  // Appends and returns a new SharedQuadState for quad.
  SharedQuadState* AppendDefaultSharedQuadState(const gfx::Rect rect,
                                                const gfx::Rect visible_rect);
  SharedQuadState* GetLastQuadSharedQuadState();

  std::unique_ptr<CompositorRenderPass> pass_;
};

// A builder class for constructing CompositorFrames in tests. The initial
// CompositorFrame will have a valid BeginFrameAck and device_scale_factor of 1.
// At least one RenderPass must be added for the CompositorFrame to be valid.
class CompositorFrameBuilder {
 public:
  CompositorFrameBuilder();

  CompositorFrameBuilder(const CompositorFrameBuilder&) = delete;
  CompositorFrameBuilder& operator=(const CompositorFrameBuilder&) = delete;

  ~CompositorFrameBuilder();

  // Builds the CompositorFrame and leaves |this| in an invalid state. This can
  // only be called once.
  CompositorFrame Build();

  // Adds a render pass with 20x20 output_rect and empty damage_rect.
  CompositorFrameBuilder& AddDefaultRenderPass();
  // Adds a render pass with specified |output_rect| and |damage_rect|.
  CompositorFrameBuilder& AddRenderPass(const gfx::Rect& output_rect,
                                        const gfx::Rect& damage_rect);

  // Add a new render pass. If the render pass has an invalid id then a new id
  // will be assigned.
  //
  // This also populates CompositorFrameMetadata::referenced_surfaces if the
  // render pass contains any SurfaceDrawQuads.
  CompositorFrameBuilder& AddRenderPass(
      std::unique_ptr<CompositorRenderPass> render_pass);
  CompositorFrameBuilder& AddRenderPass(RenderPassBuilder& builder);

  // Sets list of render passes. The list of render passes must be empty when
  // this is called.
  CompositorFrameBuilder& SetRenderPassList(
      CompositorRenderPassList render_pass_list);

  CompositorFrameBuilder& AddTransferableResource(
      TransferableResource resource);
  // Sets list of transferable resources. The list of transferable resources
  // must be empty when this is called.
  CompositorFrameBuilder& SetTransferableResources(
      std::vector<TransferableResource> resource_list);
  // Populate valid looking TransferableResources based on DrawQuad ResourceIds.
  // The list of transferable resources must be empty when this is called.
  CompositorFrameBuilder& PopulateResources();

  // Sets the BeginFrameAck. This replaces the default BeginFrameAck.
  CompositorFrameBuilder& SetBeginFrameAck(const BeginFrameAck& ack);
  CompositorFrameBuilder& SetBeginFrameSourceId(uint64_t source_id);
  CompositorFrameBuilder& SetDeviceScaleFactor(float device_scale_factor);
  CompositorFrameBuilder& AddLatencyInfo(ui::LatencyInfo latency_info);
  CompositorFrameBuilder& AddLatencyInfos(
      std::vector<ui::LatencyInfo> latency_info);
  CompositorFrameBuilder& SetReferencedSurfaces(
      std::vector<SurfaceRange> referenced_surfaces);
  CompositorFrameBuilder& SetActivationDependencies(
      std::vector<SurfaceId> activation_dependencies);
  CompositorFrameBuilder& SetDeadline(const FrameDeadline& deadline);
  CompositorFrameBuilder& SetSendFrameTokenToEmbedder(bool send);
  CompositorFrameBuilder& SetIsHandlingInteraction(
      bool is_handling_interaction);

  CompositorFrameBuilder& AddDelegatedInkMetadata(
      const gfx::DelegatedInkMetadata& metadata);
  CompositorFrameBuilder& AddOffsetTagDefinition(
      const OffsetTagDefinition& definition);

 private:
  CompositorFrame MakeInitCompositorFrame() const;

  std::optional<CompositorFrame> frame_;
  CompositorRenderPassId::Generator render_pass_id_generator_;
};

// Duplicates a list of render passes by calling DeepCopy() on each.
CompositorRenderPassList CopyRenderPasses(
    const CompositorRenderPassList& render_pass_list);

// Creates a CompositorFrame that has a render pass with 20x20 output_rect and
// empty damage_rect. This CompositorFrame is valid and can be sent over IPC.
CompositorFrame MakeDefaultCompositorFrame(
    uint64_t source_id = BeginFrameArgs::kManualSourceId);

// Creates a CompositorFrame with provided render pass.
CompositorFrame MakeCompositorFrame(
    std::unique_ptr<CompositorRenderPass> render_pass);

// Creates a CompositorFrame with provided list of render passes.
CompositorFrame MakeCompositorFrame(CompositorRenderPassList render_pass_list);

// Makes an aggregated frame out of the default compositor frame.
AggregatedFrame MakeDefaultAggregatedFrame(size_t num_render_passes = 1);

CompositorFrame MakeDefaultInteractiveCompositorFrame(
    uint64_t source_id = BeginFrameArgs::kManualSourceId);

// Creates a CompositorFrame that will be valid once its render_pass_list is
// initialized.
CompositorFrame MakeEmptyCompositorFrame();

// Populate valid looking TransferableResources for `frame` based on DrawQuad
// ResourceIds.
void PopulateTransferableResources(CompositorFrame& frame);

}  // namespace viz

#endif  // COMPONENTS_VIZ_TEST_COMPOSITOR_FRAME_HELPERS_H_
