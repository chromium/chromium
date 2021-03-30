// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/surfaces/surface_saved_frame.h"

#include <utility>

#include "base/threading/thread_task_runner_handle.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/quads/compositor_frame_transition_directive.h"
#include "components/viz/service/surfaces/surface.h"

namespace viz {

namespace {
constexpr gfx::Size kDefaultTextureSizeForTesting = gfx::Size(20, 20);

struct RenderPassGeometry {
  gfx::Rect rect;
  gfx::Transform target_transform;
};

RenderPassGeometry GetRootRenderPassGeometry(Surface* surface) {
  const auto& frame = surface->GetActiveFrame();
  DCHECK(!frame.render_pass_list.empty());
  const auto& root_render_pass = frame.render_pass_list.back();
  return {root_render_pass->output_rect,
          root_render_pass->transform_to_root_target};
}

RenderPassGeometry GetRenderPassGeometryInRootSpace(
    Surface* surface,
    const CompositorRenderPassId& render_pass_id) {
  const auto& frame = surface->GetActiveFrame();
  for (const auto& render_pass : frame.render_pass_list) {
    if (render_pass_id != render_pass->id)
      continue;
    return {render_pass->output_rect, render_pass->transform_to_root_target};
  }
  NOTREACHED();
  return {gfx::Rect(), gfx::Transform()};
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
    std::move(directive_finished_callback_).Run(directive_.sequence_id());
}

bool SurfaceSavedFrame::IsValid() const {
  bool result = valid_result_count_ == ExpectedResultCount();
  // If this saved frame is valid, then we should have a frame result.
  DCHECK(!result || frame_result_);
  return result;
}

void SurfaceSavedFrame::RequestCopyOfOutput(Surface* surface) {
  DCHECK(surface->HasActiveFrame());
  const auto& root_geometry = GetRootRenderPassGeometry(surface);
  // Bind kRoot and root geometry information to the callback.
  auto request = std::make_unique<CopyOutputRequest>(
      CopyOutputRequest::ResultFormat::RGBA_TEXTURE,
      base::BindOnce(&SurfaceSavedFrame::NotifyCopyOfOutputComplete,
                     weak_factory_.GetWeakPtr(), ResultType::kRoot, 0,
                     root_geometry.rect, root_geometry.target_transform));
  request->set_result_task_runner(base::ThreadTaskRunnerHandle::Get());
  surface->RequestCopyOfOutputOnRootRenderPass(std::move(request));
  copy_request_count_ = 1;

  const auto& shared_pass_ids = directive_.shared_render_pass_ids();
  for (size_t i = 0; i < shared_pass_ids.size(); ++i) {
    if (shared_pass_ids[i].is_null())
      continue;

    const auto& geometry =
        GetRenderPassGeometryInRootSpace(surface, shared_pass_ids[i]);
    // Shared callbacks bind kShared with an index, and geometry information on
    // the callbacks.
    auto request = std::make_unique<CopyOutputRequest>(
        CopyOutputRequest::ResultFormat::RGBA_TEXTURE,
        base::BindOnce(&SurfaceSavedFrame::NotifyCopyOfOutputComplete,
                       weak_factory_.GetWeakPtr(), ResultType::kShared, i,
                       geometry.rect, geometry.target_transform));
    request->set_result_task_runner(base::ThreadTaskRunnerHandle::Get());
    bool success = surface->RequestCopyOfOutputOnActiveFrameRenderPassId(
        std::move(request), shared_pass_ids[i]);
    DCHECK(success) << "PassId: " << shared_pass_ids[i];

    ++copy_request_count_;
  }

  DCHECK_EQ(copy_request_count_, ExpectedResultCount());
}

size_t SurfaceSavedFrame::ExpectedResultCount() const {
  // Start with 1 for the root render pass.
  size_t count = 1;
  for (auto& pass_id : directive_.shared_render_pass_ids())
    count += !pass_id.is_null();
  return count;
}

void SurfaceSavedFrame::NotifyCopyOfOutputComplete(
    ResultType type,
    size_t shared_index,
    const gfx::Rect& rect,
    const gfx::Transform& target_transform,
    std::unique_ptr<CopyOutputResult> output_copy) {
  DCHECK_GT(copy_request_count_, 0u);
  // Even if we early out, we update the count since we are no longer waiting
  // for this result.
  if (--copy_request_count_ == 0)
    std::move(directive_finished_callback_).Run(directive_.sequence_id());

  // Return if the result is empty.
  // TODO(vmpstr): We should log / trace this.
  if (output_copy->IsEmpty())
    return;

  // TODO(crbug.com/1174129): We need to support SoftwareRenderer, which would
  // return a bitmap result here.
  if (!output_copy->GetTextureResult())
    return;

  ++valid_result_count_;
  if (!frame_result_) {
    frame_result_.emplace();
    // Resize to the number of shared elements, even if some will be nullopts.
    frame_result_->shared_results.resize(
        directive_.shared_render_pass_ids().size());
  }

  auto output_copy_texture = *output_copy->GetTextureResult();
  OutputCopyResult* slot = nullptr;
  if (type == ResultType::kRoot) {
    slot = &frame_result_->root_result;
  } else {
    DCHECK(type == ResultType::kShared);
    CHECK_LT(shared_index, frame_result_->shared_results.size());
    DCHECK(!frame_result_->shared_results[shared_index]);
    slot = &frame_result_->shared_results[shared_index].emplace();
  }

  DCHECK(slot);
  DCHECK_EQ(output_copy->size(), rect.size());
  slot->mailbox = output_copy_texture.mailbox;
  slot->sync_token = output_copy_texture.sync_token;
  slot->release_callback = output_copy->TakeTextureOwnership();
  slot->rect = rect;
  slot->target_transform = target_transform;
  slot->is_software = false;
}

base::Optional<SurfaceSavedFrame::FrameResult> SurfaceSavedFrame::TakeResult() {
  return std::exchange(frame_result_, base::nullopt);
}

void SurfaceSavedFrame::CompleteSavedFrameForTesting(
    base::OnceCallback<void(const gpu::SyncToken&, bool)> release_callback) {
  frame_result_.emplace();
  frame_result_->root_result.mailbox = gpu::Mailbox::GenerateForSharedImage();
  frame_result_->root_result.release_callback =
      SingleReleaseCallback::Create(std::move(release_callback));
  frame_result_->root_result.rect = gfx::Rect(kDefaultTextureSizeForTesting);
  frame_result_->root_result.target_transform.MakeIdentity();
  frame_result_->root_result.is_software = true;

  frame_result_->shared_results.resize(
      directive_.shared_render_pass_ids().size());
  copy_request_count_ = 0;
  valid_result_count_ = ExpectedResultCount();
  weak_factory_.InvalidateWeakPtrs();
  DCHECK(IsValid());
}

SurfaceSavedFrame::OutputCopyResult::OutputCopyResult() = default;
SurfaceSavedFrame::OutputCopyResult::OutputCopyResult(
    OutputCopyResult&& other) {
  *this = std::move(other);
}

SurfaceSavedFrame::OutputCopyResult::~OutputCopyResult() {
  if (release_callback)
    release_callback->Run(sync_token, /*is_lost=*/false);
}

SurfaceSavedFrame::OutputCopyResult&
SurfaceSavedFrame::OutputCopyResult::operator=(OutputCopyResult&& other) {
  mailbox = std::move(other.mailbox);
  other.mailbox = gpu::Mailbox();

  sync_token = std::move(other.sync_token);
  other.sync_token = gpu::SyncToken();

  rect = std::move(other.rect);
  target_transform = std::move(other.target_transform);

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
