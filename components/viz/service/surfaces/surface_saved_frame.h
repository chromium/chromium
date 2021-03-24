// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_SURFACES_SURFACE_SAVED_FRAME_H_
#define COMPONENTS_VIZ_SERVICE_SURFACES_SURFACE_SAVED_FRAME_H_

#include <memory>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "components/viz/common/quads/compositor_frame_transition_directive.h"
#include "components/viz/common/resources/single_release_callback.h"
#include "components/viz/service/viz_service_export.h"

namespace viz {

class Surface;

class VIZ_SERVICE_EXPORT SurfaceSavedFrame {
 public:
  using TransitionDirectiveCompleteCallback =
      base::OnceCallback<void(uint32_t)>;

  struct OutputCopyResult {
    OutputCopyResult();
    OutputCopyResult(OutputCopyResult&& other);
    ~OutputCopyResult();

    OutputCopyResult& operator=(OutputCopyResult&& other);

    // Texture representation.
    gpu::Mailbox mailbox;
    gpu::SyncToken sync_token;

    // This represents the region for the pixel output.
    gfx::Rect rect;
    // This is a transform that takes `rect` into a root render pass space. Note
    // that this makes this result dependent on the structure of the compositor
    // frame render pass list used to request the copy output.
    gfx::Transform target_transform;

    // Is this a software or a GPU copy result?
    bool is_software = false;

    // Release callback used to return a GPU texture.
    std::unique_ptr<SingleReleaseCallback> release_callback;
  };

  struct FrameResult {
    FrameResult();
    FrameResult(FrameResult&& other);
    ~FrameResult();

    FrameResult& operator=(FrameResult&& other);

    OutputCopyResult root_result;
    std::vector<base::Optional<OutputCopyResult>> shared_results;
  };

  SurfaceSavedFrame(CompositorFrameTransitionDirective directive,
                    TransitionDirectiveCompleteCallback finished_callback);
  ~SurfaceSavedFrame();

  // Returns true iff the frame is valid and complete.
  bool IsValid() const;

  const CompositorFrameTransitionDirective& directive() { return directive_; }

  // Appends copy output requests to the needed render passes in the active
  // frame.
  void RequestCopyOfOutput(Surface* surface);

  base::Optional<FrameResult> TakeResult() WARN_UNUSED_RESULT;

  // For testing functionality that ensures that we have a valid frame.
  void CompleteSavedFrameForTesting(
      base::OnceCallback<void(const gpu::SyncToken&, bool)> release_callback);

 private:
  enum class ResultType { kRoot, kShared };

  void NotifyCopyOfOutputComplete(ResultType type,
                                  size_t shared_index,
                                  const gfx::Rect& rect,
                                  const gfx::Transform& target_transform,
                                  std::unique_ptr<CopyOutputResult> result);

  size_t ExpectedResultCount() const;

  CompositorFrameTransitionDirective directive_;
  TransitionDirectiveCompleteCallback directive_finished_callback_;

  base::Optional<FrameResult> frame_result_;

  // This is the number of copy requests we requested. We decrement this value
  // anytime we get a result back. When it reaches 0, we notify that this frame
  // is complete.
  size_t copy_request_count_ = 0;

  // This counts the total number of valid results. For example, if one of
  // several requests is not valid (e.g. it's empty) then this count will be
  // smaller than the number of requests we made. This is used to determine
  // whether the SurfaceSavedFrame is "valid".
  size_t valid_result_count_ = 0;

  base::WeakPtrFactory<SurfaceSavedFrame> weak_factory_{this};
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_SURFACES_SURFACE_SAVED_FRAME_H_
