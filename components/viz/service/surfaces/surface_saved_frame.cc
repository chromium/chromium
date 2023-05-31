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
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/quads/compositor_frame_transition_directive.h"
#include "components/viz/common/quads/compositor_render_pass.h"
#include "components/viz/common/quads/compositor_render_pass_draw_quad.h"
#include "components/viz/common/quads/draw_quad.h"
#include "components/viz/common/transition_utils.h"
#include "components/viz/service/surfaces/surface.h"
#include "services/viz/public/mojom/compositing/compositor_render_pass_id.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/point.h"

namespace viz {

namespace {
constexpr gfx::Size kDefaultTextureSizeForTesting = gfx::Size(20, 20);

constexpr auto kResultFormat = CopyOutputRequest::ResultFormat::RGBA;
constexpr auto kResultDestination =
    CopyOutputRequest::ResultDestination::kNativeTextures;

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

SurfaceSavedFrame::SurfaceSavedFrame(
    CompositorFrameTransitionDirective directive,
    TransitionDirectiveCompleteCallback directive_finished_callback)
    : directive_(std::move(directive)),
      directive_finished_callback_(std::move(directive_finished_callback)) {
  // We should only be constructing a saved frame from a save directive.
  DCHECK_EQ(directive_.type(), CompositorFrameTransitionDirective::Type::kSave);
}

SurfaceSavedFrame::~SurfaceSavedFrame() {
  if (directive_finished_callback_)
    std::move(directive_finished_callback_).Run(directive_);
}

base::flat_set<ViewTransitionElementResourceId>
SurfaceSavedFrame::GetEmptyResourceIds() const {
  base::flat_set<ViewTransitionElementResourceId> result;
  for (auto& shared_element : directive_.shared_elements())
    if (shared_element.render_pass_id.is_null())
      result.insert(shared_element.view_transition_element_resource_id);
  return result;
}

bool SurfaceSavedFrame::IsValid() const {
  bool result = valid_result_count_ == ExpectedResultCount();
  // If this saved frame is valid, then we should have a frame result.
  DCHECK(!result || frame_result_);
  return result;
}

void SurfaceSavedFrame::RequestCopyOfOutput(Surface* surface) {
  DCHECK(surface->HasActiveFrame());

  const auto& active_frame = surface->GetActiveFrame();
  for (const auto& render_pass : active_frame.render_pass_list) {
    if (auto request = CreateCopyRequestIfNeeded(
            *render_pass, active_frame.render_pass_list)) {
      surface->RequestCopyOfOutputOnActiveFrameRenderPassId(std::move(request),
                                                            render_pass->id);
      copy_request_count_++;
    }
  }

  DCHECK_EQ(copy_request_count_, ExpectedResultCount());

  if (copy_request_count_ == 0) {
    InitFrameResult();
    std::move(directive_finished_callback_).Run(directive_);
  }
}

std::unique_ptr<CopyOutputRequest> SurfaceSavedFrame::CreateCopyRequestIfNeeded(
    const CompositorRenderPass& render_pass,
    const CompositorRenderPassList& render_pass_list) const {
  auto shared_pass_index =
      GetSharedPassIndex(directive_.shared_elements(), render_pass.id);
  if (shared_pass_index >= directive_.shared_elements().size())
    return nullptr;

  RenderPassDrawData draw_data(render_pass);
  auto request = std::make_unique<CopyOutputRequest>(
      kResultFormat, kResultDestination,
      base::BindOnce(&SurfaceSavedFrame::NotifyCopyOfOutputComplete,
                     weak_factory_.GetMutableWeakPtr(), shared_pass_index,
                     draw_data));
  request->set_result_task_runner(
      base::SingleThreadTaskRunner::GetCurrentDefault());
  return request;
}

bool SurfaceSavedFrame::IsSharedElementRenderPass(
    CompositorRenderPassId pass_id) const {
  const auto& shared_elements = directive_.shared_elements();
  return GetSharedPassIndex(shared_elements, pass_id) < shared_elements.size();
}

size_t SurfaceSavedFrame::ExpectedResultCount() const {
  base::flat_set<CompositorRenderPassId> ids;
  for (auto& shared_element : directive_.shared_elements())
    if (!shared_element.render_pass_id.is_null())
      ids.insert(shared_element.render_pass_id);
  return ids.size();
}

void SurfaceSavedFrame::NotifyCopyOfOutputComplete(
    size_t shared_index,
    const RenderPassDrawData& data,
    std::unique_ptr<CopyOutputResult> output_copy) {
  DCHECK_GT(copy_request_count_, 0u);
  // Even if we early out, we update the count since we are no longer waiting
  // for this result.
  if (--copy_request_count_ == 0) {
    std::move(directive_finished_callback_).Run(directive_);
  }

  // Return if the result is empty.
  if (output_copy->IsEmpty()) {
    LOG(ERROR) << "SurfaceSavedFrame copy output result for shared index "
               << shared_index << " is empty.";
    return;
  }

  ++valid_result_count_;
  if (!frame_result_) {
    InitFrameResult();
    // Resize to the number of shared elements, even if some will be nullopts.
    frame_result_->shared_results.resize(directive_.shared_elements().size());
  }

  CHECK_LT(shared_index, frame_result_->shared_results.size());
  DCHECK(!frame_result_->shared_results[shared_index]);
  OutputCopyResult* slot =
      &frame_result_->shared_results[shared_index].emplace();

  DCHECK(slot);
  DCHECK_EQ(output_copy->size(), data.size);
  DCHECK_EQ(output_copy->format(), CopyOutputResult::Format::RGBA);
  if (output_copy->destination() ==
      CopyOutputResult::Destination::kSystemMemory) {
    slot->bitmap = output_copy->ScopedAccessSkBitmap().GetOutScopedBitmap();
    slot->is_software = true;
  } else {
    auto output_copy_texture = *output_copy->GetTextureResult();
    slot->mailbox = output_copy_texture.mailbox_holders[0].mailbox;
    slot->sync_token = output_copy_texture.mailbox_holders[0].sync_token;
    slot->color_space = output_copy_texture.color_space;

    CopyOutputResult::ReleaseCallbacks release_callbacks =
        output_copy->TakeTextureOwnership();
    // CopyOutputResults carrying RGBA format contain a single texture, there
    // should be only one release callback:
    DCHECK_EQ(1u, release_callbacks.size());
    slot->release_callback = std::move(release_callbacks[0]);
    slot->is_software = false;
  }
  slot->draw_data = data;
}

absl::optional<SurfaceSavedFrame::FrameResult> SurfaceSavedFrame::TakeResult() {
  return std::exchange(frame_result_, absl::nullopt);
}

void SurfaceSavedFrame::CompleteSavedFrameForTesting() {
  SkBitmap bitmap;
  bitmap.allocPixels(
      SkImageInfo::MakeN32Premul(kDefaultTextureSizeForTesting.width(),
                                 kDefaultTextureSizeForTesting.height()));

  InitFrameResult();
  frame_result_->shared_results.resize(directive_.shared_elements().size());
  for (auto& result : frame_result_->shared_results) {
    result.emplace();
    result->bitmap = std::move(bitmap);
    result->draw_data.size = kDefaultTextureSizeForTesting;
    result->is_software = true;
  }

  copy_request_count_ = 0;
  valid_result_count_ = ExpectedResultCount();
  weak_factory_.InvalidateWeakPtrs();
  DCHECK(IsValid());
}

void SurfaceSavedFrame::InitFrameResult() {
  frame_result_.emplace();
  frame_result_->empty_resource_ids = GetEmptyResourceIds();
}

SurfaceSavedFrame::RenderPassDrawData::RenderPassDrawData() = default;
SurfaceSavedFrame::RenderPassDrawData::RenderPassDrawData(
    const CompositorRenderPass& render_pass)
    : size(render_pass.output_rect.size()) {}

SurfaceSavedFrame::OutputCopyResult::OutputCopyResult() = default;
SurfaceSavedFrame::OutputCopyResult::OutputCopyResult(
    OutputCopyResult&& other) {
  *this = std::move(other);
}

SurfaceSavedFrame::OutputCopyResult::~OutputCopyResult() {
  if (release_callback)
    std::move(release_callback).Run(sync_token, /*is_lost=*/false);
}

SurfaceSavedFrame::OutputCopyResult&
SurfaceSavedFrame::OutputCopyResult::operator=(OutputCopyResult&& other) {
  mailbox = std::move(other.mailbox);
  other.mailbox = gpu::Mailbox();

  sync_token = std::move(other.sync_token);
  other.sync_token = gpu::SyncToken();

  color_space = std::move(other.color_space);
  other.color_space = gfx::ColorSpace();

  bitmap = std::move(other.bitmap);
  other.bitmap = SkBitmap();

  draw_data = std::move(other.draw_data);

  release_callback = std::move(other.release_callback);

  is_software = other.is_software;
  other.is_software = false;

  return *this;
}

SurfaceSavedFrame::FrameResult::FrameResult() = default;

SurfaceSavedFrame::FrameResult::FrameResult(FrameResult&& other) = default;

SurfaceSavedFrame::FrameResult::~FrameResult() = default;

SurfaceSavedFrame::FrameResult& SurfaceSavedFrame::FrameResult::operator=(
    FrameResult&& other) = default;

}  // namespace viz
