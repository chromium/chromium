// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/surfaces/surface_saved_frame.h"

#include <algorithm>
#include <iterator>
#include <utility>

#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "base/types/pass_key.h"
#include "components/viz/common/color_space_utils.h"
#include "components/viz/common/features.h"
#include "components/viz/common/frame_sinks/blit_request.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/quads/compositor_frame_transition_directive.h"
#include "components/viz/common/quads/compositor_render_pass.h"
#include "components/viz/common/quads/compositor_render_pass_draw_quad.h"
#include "components/viz/common/quads/draw_quad.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "components/viz/common/transition_utils.h"
#include "components/viz/service/surfaces/surface.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/ipc/common/surface_handle.h"
#include "services/viz/public/mojom/compositing/compositor_render_pass_id.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gl/gl_bindings.h"

namespace viz {

namespace {
constexpr gfx::Size kDefaultTextureSizeForTesting = gfx::Size(20, 20);

constexpr auto kResultFormat = CopyOutputRequest::ResultFormat::RGBA;
constexpr auto kResultDestination =
    CopyOutputRequest::ResultDestination::kSharedImage;

// Returns the index of |render_pass_id| in |shared_elements| if the id
// corresponds to an element in the given list. Otherwise returns the size of
// |shared_elements| vector.
size_t GetSharedPassIndex(
    const std::vector<CompositorFrameTransitionDirective::SharedElement>&
        shared_elements,
    CompositorRenderPassId render_pass_id) {
  size_t shared_element_index = 0;
  for (; shared_element_index < shared_elements.size();
       ++shared_element_index) {
    if (shared_elements[shared_element_index].render_pass_id ==
        render_pass_id) {
      break;
    }
  }
  return shared_element_index;
}

}  // namespace

// static
std::unique_ptr<SurfaceSavedFrame> SurfaceSavedFrame::CreateForTesting(
    CompositorFrameTransitionDirective directive,
    gpu::SharedImageInterface* shared_image_interface) {
  return base::WrapUnique(
      new SurfaceSavedFrame(base::PassKey<SurfaceSavedFrame>(),
                            std::move(directive), shared_image_interface));
}

SurfaceSavedFrame::SurfaceSavedFrame(
    CompositorFrameTransitionDirective directive,
    gpu::SharedImageInterface* shared_image_interface)
    : directive_(std::move(directive)),
      shared_image_interface_(shared_image_interface) {
  // If we're using BlitRequests, then we better have a shared image interface.
  CHECK(shared_image_interface_);

  // We should only be constructing a saved frame from a save directive.
  DCHECK_EQ(directive_.type(), CompositorFrameTransitionDirective::Type::kSave);
}

SurfaceSavedFrame::SurfaceSavedFrame(
    base::PassKey<SurfaceSavedFrame>,
    CompositorFrameTransitionDirective directive,
    gpu::SharedImageInterface* shared_image_interface)
    : directive_(std::move(directive)),
      shared_image_interface_(shared_image_interface) {
  frame_result_.emplace();
}

SurfaceSavedFrame::~SurfaceSavedFrame() {
  if (directive_finished_callback_)
    std::move(directive_finished_callback_).Run(directive_);
}

base::flat_set<ViewTransitionElementResourceId>
SurfaceSavedFrame::GetEmptyResourceIds(
    const CompositorRenderPassList& render_pass_list) const {
  base::flat_set<ViewTransitionElementResourceId> result;
  for (auto& shared_element : directive_.shared_elements()) {
    if (shared_element.render_pass_id.is_null()) {
      result.insert(shared_element.view_transition_element_resource_id);
    }
  }
  for (auto& render_pass : render_pass_list) {
    if (render_pass->output_rect.IsEmpty() &&
        render_pass->view_transition_element_resource_id.IsValid()) {
      result.insert(render_pass->view_transition_element_resource_id);
    }
  }
  return result;
}

bool SurfaceSavedFrame::IsValid() const {
  return frame_result_.has_value();
}

void SurfaceSavedFrame::RequestCopyOfOutput(
    Surface* surface,
    CopyFinishedCallback finished_callback) {
  CHECK(!directive_finished_callback_);
  directive_finished_callback_ = std::move(finished_callback);

  DCHECK(surface->HasActiveFrame());

  const auto& active_frame = surface->GetActiveFrame();
  bool is_software = active_frame.metadata.is_software;
  for (const auto& render_pass : active_frame.render_pass_list) {
    if (active_frame.render_pass_list.back() == render_pass) {
      continue;
    }

    if (auto request = CreateCopyRequestIfNeeded(
            *render_pass, is_software,
            active_frame.metadata.content_color_usage)) {
      surface->RequestCopyOfOutputOnActiveFrameRenderPassId(std::move(request),
                                                            render_pass->id);
      copy_request_count_++;
    }
  }

  DCHECK_EQ(copy_request_count_,
            ExpectedResultCount(active_frame.render_pass_list));

  frame_result_.emplace();
  frame_result_->empty_resource_ids =
      GetEmptyResourceIds(active_frame.render_pass_list);
  frame_result_->shared_results.resize(directive_.shared_elements().size());

  // If we're using BlitRequests, then we need to create the result bundle
  // immediately, since it can be imported before the copy output results
  // arrive.
  for (auto& [index, shared_image] : blit_shared_images_) {
    OutputCopyResult* slot = &frame_result_->shared_results[index].emplace();

    slot->sync_token = shared_image->creation_sync_token();
    slot->shared_image = shared_image;
    slot->release_callback = base::BindOnce(
        [](scoped_refptr<gpu::ClientSharedImage> image,
           const gpu::SyncToken& sync_token,
           bool is_lost) { image->UpdateDestructionSyncToken(sync_token); },
        std::move(shared_image));
  }

  // DispatchCopyDoneCallback early for cross frame sink view transitions.
  if ((features::ShouldAckCOREarlyForViewTransition() &&
       directive_.maybe_cross_frame_sink()) ||
      copy_request_count_ == 0) {
    DispatchCopyDoneCallback();
  }
}

void SurfaceSavedFrame::DispatchCopyDoneCallback() {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(directive_finished_callback_), directive_));
}

std::unique_ptr<CopyOutputRequest> SurfaceSavedFrame::CreateCopyRequestIfNeeded(
    const CompositorRenderPass& render_pass,
    bool is_software,
    gfx::ContentColorUsage content_color_usage) {
  if (render_pass.output_rect.IsEmpty()) {
    return nullptr;
  }

  size_t shared_pass_index =
      GetSharedPassIndex(directive_.shared_elements(), render_pass.id);
  if (shared_pass_index >= directive_.shared_elements().size()) {
    return nullptr;
  }

  const gfx::Size size = render_pass.output_rect.size();

  auto request = std::make_unique<CopyOutputRequest>(
      kResultFormat, kResultDestination,
      base::BindOnce(&SurfaceSavedFrame::NotifyCopyOfOutputComplete,
                     weak_factory_.GetMutableWeakPtr(), shared_pass_index));
  request->set_result_task_runner(
      base::SingleThreadTaskRunner::GetCurrentDefault());
  scoped_refptr<gpu::ClientSharedImage>& shared_image =
      blit_shared_images_[shared_pass_index];

  const auto& display_color_spaces = directive_.display_color_spaces();
  bool has_transparent_background = render_pass.has_transparent_background;

  auto image_format = display_color_spaces.GetOutputFormat(
      content_color_usage, has_transparent_background);
  auto color_space =
      display_color_spaces.GetRasterAndCompositeColorSpace(content_color_usage);

  if (is_software) {
    gpu::SharedImageUsageSet flags = gpu::SHARED_IMAGE_USAGE_CPU_WRITE_ONLY;
    shared_image =
        shared_image_interface_->CreateSharedImageForSoftwareCompositor(
            {image_format, size, color_space, flags, "ViewTransitionTexture"});
  } else {
    gpu::SharedImageUsageSet flags = gpu::SHARED_IMAGE_USAGE_DISPLAY_READ |
                                     gpu::SHARED_IMAGE_USAGE_DISPLAY_WRITE;
    shared_image = shared_image_interface_->CreateSharedImage(
        {image_format, size, color_space, flags, "ViewTransitionTexture"},
        gpu::kNullSurfaceHandle);
  }
  request->set_result_selection(gfx::Rect(size));
  request->set_blit_request(
      BlitRequest(gfx::Point(), LetterboxingBehavior::kDoNotLetterbox,
                  shared_image, shared_image->creation_sync_token(),
                  /*populates_gpu_memory_buffer=*/false));

  return request;
}

bool SurfaceSavedFrame::IsSharedElementRenderPass(
    CompositorRenderPassId pass_id) const {
  const auto& shared_elements = directive_.shared_elements();
  return GetSharedPassIndex(shared_elements, pass_id) < shared_elements.size();
}

size_t SurfaceSavedFrame::ExpectedResultCount(
    const CompositorRenderPassList& render_pass_list) const {
  base::flat_set<CompositorRenderPassId> ids;
  for (auto& shared_element : directive_.shared_elements()) {
    if (!shared_element.render_pass_id.is_null()) {
      ids.insert(shared_element.render_pass_id);
    }
  }
  for (auto& render_pass : render_pass_list) {
    if (render_pass->output_rect.IsEmpty()) {
      ids.erase(render_pass->id);
    }
  }
  return ids.size();
}

void SurfaceSavedFrame::NotifyCopyOfOutputComplete(
    size_t shared_index,
    std::unique_ptr<CopyOutputResult> output_copy) {
  DCHECK_GT(copy_request_count_, 0u);
  // Even if we early out, we update the count since we are no longer waiting
  // for this result.
  --copy_request_count_;
  // Callback is run already for cross frame view transitions.
  if (!(features::ShouldAckCOREarlyForViewTransition() &&
        directive_.maybe_cross_frame_sink()) &&
      copy_request_count_ == 0) {
    DispatchCopyDoneCallback();
  }

  // Return if the result is empty.
  if (output_copy->IsEmpty()) {
    LOG(ERROR) << "SurfaceSavedFrame copy output result for shared index "
               << shared_index << " is empty.";
    return;
  }

  ++valid_result_count_;
}

SurfaceSavedFrame::FrameResult SurfaceSavedFrame::TakeResult() {
  CHECK(frame_result_);
  auto result = std::move(*frame_result_);
  frame_result_.reset();
  return result;
}

void SurfaceSavedFrame::CompleteSavedFrameForTesting() {
  frame_result_->shared_results.resize(directive_.shared_elements().size());
  for (auto& result : frame_result_->shared_results) {
    result.emplace();
    result->shared_image =
        shared_image_interface_->CreateSharedImageForSoftwareCompositor(
            {SinglePlaneFormat::kBGRA_8888, kDefaultTextureSizeForTesting,
             gfx::ColorSpace(), gpu::SHARED_IMAGE_USAGE_CPU_WRITE_ONLY,
             "SurfaceSavedFrameForTesting"});
    result->sync_token = shared_image_interface_->GenVerifiedSyncToken();
    result->release_callback =
        base::DoNothingWithBoundArgs(result->shared_image);
  }

  copy_request_count_ = 0;
  // TODO(vmpstr): Note that we also count passes that have an empty
  // `output_rect` here, but in testing situations this is not currently the
  // case. If we need to unittest empty render pass cases, then this value needs
  // to be changed.
  valid_result_count_ = [this]() {
    base::flat_set<CompositorRenderPassId> ids;
    for (auto& shared_element : directive_.shared_elements()) {
      if (!shared_element.render_pass_id.is_null()) {
        ids.insert(shared_element.render_pass_id);
      }
    }
    return ids.size();
  }();
  weak_factory_.InvalidateWeakPtrs();
  DCHECK(IsValid());
}

SurfaceSavedFrame::OutputCopyResult::OutputCopyResult() = default;
SurfaceSavedFrame::OutputCopyResult::OutputCopyResult(
    OutputCopyResult&& other) {
  *this = std::move(other);
}

SurfaceSavedFrame::OutputCopyResult::~OutputCopyResult() {
  if (release_callback) {
    std::move(release_callback).Run(sync_token, /*is_lost=*/false);
  }
}

SurfaceSavedFrame::OutputCopyResult&
SurfaceSavedFrame::OutputCopyResult::operator=(OutputCopyResult&& other) {
  sync_token = std::move(other.sync_token);
  other.sync_token = gpu::SyncToken();

  shared_image = std::move(other.shared_image);

  release_callback = std::move(other.release_callback);

  return *this;
}

SurfaceSavedFrame::FrameResult::FrameResult() = default;

SurfaceSavedFrame::FrameResult::FrameResult(FrameResult&& other) = default;

SurfaceSavedFrame::FrameResult::~FrameResult() = default;

SurfaceSavedFrame::FrameResult& SurfaceSavedFrame::FrameResult::operator=(
    FrameResult&& other) = default;

}  // namespace viz
