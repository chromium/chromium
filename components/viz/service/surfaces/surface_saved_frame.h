// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_SURFACES_SURFACE_SAVED_FRAME_H_
#define COMPONENTS_VIZ_SERVICE_SURFACES_SURFACE_SAVED_FRAME_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/types/pass_key.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/quads/compositor_frame_transition_directive.h"
#include "components/viz/common/quads/compositor_render_pass.h"
#include "components/viz/common/resources/release_callback.h"
#include "components/viz/service/viz_service_export.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "ui/gfx/display_color_spaces.h"

namespace gpu {
class SharedImageInterface;
}

namespace viz {

class Surface;

class VIZ_SERVICE_EXPORT SurfaceSavedFrame {
 public:
  using CopyFinishedCallback =
      base::OnceCallback<void(const CompositorFrameTransitionDirective&)>;

  struct RenderPassDrawData {
    RenderPassDrawData();
    explicit RenderPassDrawData(const CompositorRenderPass& render_pass);

    // This represents the size of the copied texture.
    gfx::Size size;
  };

  struct OutputCopyResult {
    OutputCopyResult();
    OutputCopyResult(OutputCopyResult&& other);
    ~OutputCopyResult();

    OutputCopyResult& operator=(OutputCopyResult&& other);

    gpu::SyncToken sync_token;
    gfx::ColorSpace color_space;

    // Texture representation.
    gpu::Mailbox mailbox;

    // Software bitmap representation.
    SkBitmap bitmap;

    // Software image representation.
    scoped_refptr<gpu::ClientSharedImage> shared_image;

    // This is information needed to draw the texture as if it was a part of the
    // original frame.
    RenderPassDrawData draw_data;

    // Is this a software or a GPU copy result?
    bool is_software = false;

    // Release callback used to return a GPU texture.
    ReleaseCallback release_callback;
  };

  struct FrameResult {
    FrameResult();
    FrameResult(FrameResult&& other);
    ~FrameResult();

    FrameResult& operator=(FrameResult&& other);

    std::vector<std::optional<OutputCopyResult>> shared_results;
    base::flat_set<ViewTransitionElementResourceId> empty_resource_ids;
  };

  static std::unique_ptr<SurfaceSavedFrame> CreateForTesting(
      CompositorFrameTransitionDirective directive);

  SurfaceSavedFrame(CompositorFrameTransitionDirective directive,
                    gpu::SharedImageInterface* shared_image_interface);
  ~SurfaceSavedFrame();

  // Returns true iff the frame is valid and complete.
  bool IsValid() const;

  const CompositorFrameTransitionDirective& directive() { return directive_; }

  // Appends copy output requests to the needed render passes in the active
  // frame.
  void RequestCopyOfOutput(Surface* surface,
                           CopyFinishedCallback finished_callback);

  [[nodiscard]] FrameResult TakeResult();

  // For testing functionality that ensures that we have a valid frame.
  void CompleteSavedFrameForTesting();

  base::flat_set<ViewTransitionElementResourceId> GetEmptyResourceIds() const;

 private:
  explicit SurfaceSavedFrame(base::PassKey<SurfaceSavedFrame>,
                             CompositorFrameTransitionDirective directive);

  std::unique_ptr<CopyOutputRequest> CreateCopyRequestIfNeeded(
      const CompositorRenderPass& render_pass,
      bool is_software,
      gfx::ContentColorUsage content_color_usage);

  void NotifyCopyOfOutputComplete(size_t shared_index,
                                  std::unique_ptr<CopyOutputResult> result);

  // The `directive_finished_callback_` is dispatched asynchronously since the
  // callback can access *and* delete this object.
  void DispatchCopyDoneCallback();

  size_t ExpectedResultCount() const;

  // Collects metadata to create a copy of the source CompositorFrame for shared
  // element snapshots.
  // |render_passes| is the render pass list from the source frame.
  // |max_id| returns the maximum render pass id in the list above.
  // |tainted_to_clean_pass_ids| populates the set of render passes which
  // include shared elements and need clean render passes for snapshots.
  void PrepareForCopy(
      const CompositorRenderPassList& render_passes,
      CompositorRenderPassId& max_id,
      base::flat_map<CompositorRenderPassId, CompositorRenderPassId>&
          tainted_to_clean_pass_ids) const;

  // Used to filter render pass draw quads when copying render passes for shared
  // element snapshots.
  // |tained_to_clean_pass_ids| is used to replace tainted quads with the
  // equivalent clean render passes.
  // |pass_quad| is the quad from the source pass being copied.
  // |copy_pass| is the new clean pass being created.
  bool FilterSharedElementAndTaintedQuads(
      const base::flat_map<CompositorRenderPassId, CompositorRenderPassId>*
          tainted_to_clean_pass_ids,
      const DrawQuad& quad,
      CompositorRenderPass& copy_pass) const;

  // Returns true if |pass_id|'s content is 1:1 with a shared element.
  bool IsSharedElementRenderPass(CompositorRenderPassId pass_id) const;

  CompositorFrameTransitionDirective directive_;
  raw_ptr<gpu::SharedImageInterface> shared_image_interface_;
  CopyFinishedCallback directive_finished_callback_;

  // Store the blit images while the copy output request is ongoing.
  base::flat_map<size_t, scoped_refptr<gpu::ClientSharedImage>>
      blit_shared_images_;

  // Stored draw data for the shared index.
  base::flat_map<size_t, RenderPassDrawData> draw_data_;

  std::optional<FrameResult> frame_result_;

  // This is the number of copy requests we requested. We decrement this value
  // anytime we get a result back. When it reaches 0, we notify that this frame
  // is complete.
  size_t copy_request_count_ = 0;

  // This counts the total number of valid results. For example, if one of
  // several requests is not valid (e.g. it's empty) then this count will be
  // smaller than the number of requests we made. This is used to determine
  // whether the SurfaceSavedFrame is "valid".
  size_t valid_result_count_ = 0;

  // This indicates whether or not to use blit requests.
  const bool use_blit_requests_;

  base::WeakPtrFactory<SurfaceSavedFrame> weak_factory_{this};
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_SURFACES_SURFACE_SAVED_FRAME_H_
