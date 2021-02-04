// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/surfaces/surface_saved_frame.h"

#include <utility>

#include "base/bind.h"
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

SurfaceSavedFrame::~SurfaceSavedFrame() {
  if (texture_release_callback_) {
    texture_release_callback_->Run(texture_result_.sync_token,
                                   /*is_lost=*/false);
  }
}

bool SurfaceSavedFrame::IsValid() const {
  // TODO(crbug.com/1174129): This needs to be updated with software copies as
  // well.
  return !texture_result_.mailbox.IsZero();
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

  texture_result_ = *result->GetTextureResult();
  texture_release_callback_ = result->TakeTextureOwnership();
}

void SurfaceSavedFrame::CompleteSavedFrameForTesting() {
  texture_result_.mailbox = gpu::Mailbox::GenerateForSharedImage();
  DCHECK(IsValid());
}

}  // namespace viz
