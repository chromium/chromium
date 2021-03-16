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
    directive_finished_callback_.Run(directive_.sequence_id());
}

bool SurfaceSavedFrame::IsValid() const {
  return result_.has_value();
}

void SurfaceSavedFrame::RequestCopyOfOutput(Surface* surface) {
  auto request = std::make_unique<CopyOutputRequest>(
      CopyOutputRequest::ResultFormat::RGBA_TEXTURE,
      base::BindOnce(&SurfaceSavedFrame::NotifyCopyOfOutputComplete,
                     weak_factory_.GetWeakPtr()));
  request->set_result_task_runner(base::ThreadTaskRunnerHandle::Get());
  surface->RequestCopyOfOutputOnRootRenderPass(std::move(request));
}

void SurfaceSavedFrame::NotifyCopyOfOutputComplete(
    std::unique_ptr<CopyOutputResult> result) {
  // Even if we early out, we can notify that the save directive is completed.
  directive_finished_callback_.Run(directive_.sequence_id());
  directive_finished_callback_.Reset();

  // Return if the result is empty.
  // TODO(vmpstr): We should log / trace this.
  if (result->IsEmpty())
    return;

  // TODO(crbug.com/1174129): We need to support SoftwareRenderer, which would
  // return a bitmap result here.
  if (!result->GetTextureResult())
    return;

  auto copy_output_texture = *result->GetTextureResult();
  DCHECK(!result_.has_value());
  result_.emplace();
  result_->mailbox = copy_output_texture.mailbox;
  result_->sync_token = copy_output_texture.sync_token;
  result_->size = result->size();
  result_->release_callback = result->TakeTextureOwnership();
  result_->is_software = false;
}

base::Optional<SurfaceSavedFrame::OutputCopyResult>
SurfaceSavedFrame::TakeResult() {
  return std::move(result_);
}

void SurfaceSavedFrame::CompleteSavedFrameForTesting(
    base::OnceCallback<void(const gpu::SyncToken&, bool)> release_callback) {
  result_.emplace();
  result_->mailbox = gpu::Mailbox::GenerateForSharedImage();
  result_->release_callback =
      SingleReleaseCallback::Create(std::move(release_callback));
  result_->size = kDefaultTextureSizeForTesting;
  result_->is_software = true;
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

  size = std::move(other.size);

  release_callback = std::move(other.release_callback);

  is_software = other.is_software;
  other.is_software = false;
  return *this;
}

}  // namespace viz
