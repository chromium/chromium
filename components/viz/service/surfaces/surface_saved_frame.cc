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

SurfaceSavedFrame::SurfaceSavedFrame(
    const CompositorFrameTransitionDirective& directive)
    : directive_(directive) {
  // We should only be constructing a saved frame from a save directive.
  DCHECK_EQ(directive.type(), CompositorFrameTransitionDirective::Type::kSave);
}

SurfaceSavedFrame::~SurfaceSavedFrame() = default;

bool SurfaceSavedFrame::IsValid() const {
  // TODO(crbug.com/1174129): This needs to be updated with software copies as
  // well.
  return HasTextureResult();
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
  // Return if the result is empty.
  // TODO(vmpstr): We should log / trace this.
  if (result->IsEmpty())
    return;

  // TODO(crbug.com/1174129): We need to support SoftwareRenderer, which would
  // return a bitmap result here.
  if (!result->GetTextureResult())
    return;

  auto copy_output_texture = *result->GetTextureResult();
  texture_result_.mailbox = copy_output_texture.mailbox;
  texture_result_.sync_token = copy_output_texture.sync_token;
  texture_result_.size = result->size();
  texture_result_.release_callback = result->TakeTextureOwnership();
}

bool SurfaceSavedFrame::HasTextureResult() const {
  return texture_result_.release_callback && !texture_result_.mailbox.IsZero();
}

SurfaceSavedFrame::TextureResult SurfaceSavedFrame::TakeTextureResult() {
  DCHECK(HasTextureResult());
  // Note that the TextureResult move constructor resets sufficient state in the
  // member so that HasTextureResult() returns false afterwards, effectively
  // clearing the member variable.
  return std::move(texture_result_);
}

void SurfaceSavedFrame::CompleteSavedFrameForTesting(
    base::OnceCallback<void(const gpu::SyncToken&, bool)> release_callback) {
  texture_result_.mailbox = gpu::Mailbox::GenerateForSharedImage();
  texture_result_.release_callback =
      SingleReleaseCallback::Create(std::move(release_callback));
  DCHECK(IsValid());
}

SurfaceSavedFrame::TextureResult::TextureResult() = default;
SurfaceSavedFrame::TextureResult::TextureResult(TextureResult&& other) {
  *this = std::move(other);
}

SurfaceSavedFrame::TextureResult::~TextureResult() {
  if (release_callback)
    release_callback->Run(sync_token, /*is_lost=*/false);
}

SurfaceSavedFrame::TextureResult& SurfaceSavedFrame::TextureResult::operator=(
    TextureResult&& other) {
  mailbox = std::move(other.mailbox);
  other.mailbox = gpu::Mailbox();

  sync_token = std::move(other.sync_token);
  other.sync_token = gpu::SyncToken();

  size = std::move(other.size);

  release_callback = std::move(other.release_callback);
  return *this;
}

}  // namespace viz
