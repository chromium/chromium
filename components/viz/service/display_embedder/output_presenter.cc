// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/output_presenter.h"

#include <utility>

#include "components/viz/service/display_embedder/skia_output_surface_dependency.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image_factory.h"
#include "third_party/skia/include/gpu/GrBackendSemaphore.h"
#include "third_party/skia/include/gpu/GrDirectContext.h"

namespace viz {

OutputPresenter::Image::Image() = default;

OutputPresenter::Image::~Image() {
  // TODO(vasilyt): As we are going to delete image anyway we should be able
  // to abort write to avoid unnecessary flush to submit semaphores.
  if (scoped_skia_write_access_) {
    EndWriteSkia();
  }
  DCHECK(!scoped_skia_write_access_);
}

bool OutputPresenter::Image::Initialize(
    gpu::SharedImageFactory* factory,
    gpu::SharedImageRepresentationFactory* representation_factory,
    const gpu::Mailbox& mailbox,
    SkiaOutputSurfaceDependency* deps) {
  skia_representation_ = representation_factory->ProduceSkia(
      mailbox, deps->GetSharedContextState());
  if (!skia_representation_) {
    DLOG(ERROR) << "ProduceSkia() failed.";
    return false;
  }

  // Initialize |shared_image_deleter_| to make sure the shared image backing
  // will be released with the Image.
  shared_image_deleter_.ReplaceClosure(base::BindOnce(
      base::IgnoreResult(&gpu::SharedImageFactory::DestroySharedImage),
      base::Unretained(factory), mailbox));

  return true;
}

void OutputPresenter::Image::BeginWriteSkia(int sample_count) {
  DCHECK(!scoped_skia_write_access_);
  DCHECK(!GetPresentCount());
  DCHECK(end_semaphores_.empty());

  std::vector<GrBackendSemaphore> begin_semaphores;
  SkSurfaceProps surface_props{0, kUnknown_SkPixelGeometry};

  // Buffer queue is internal to GPU proc and handles texture initialization,
  // so allow uncleared access.
  scoped_skia_write_access_ = skia_representation_->BeginScopedWriteAccess(
      sample_count, surface_props, &begin_semaphores, &end_semaphores_,
      gpu::SharedImageRepresentation::AllowUnclearedAccess::kYes);
  DCHECK(scoped_skia_write_access_);
  if (!begin_semaphores.empty()) {
    scoped_skia_write_access_->surface()->wait(
        begin_semaphores.size(),
        begin_semaphores.data(),
        /*deleteSemaphoresAfterWait=*/false);
  }
}

SkSurface* OutputPresenter::Image::sk_surface() {
  return scoped_skia_write_access_ ? scoped_skia_write_access_->surface()
                                   : nullptr;
}

std::vector<GrBackendSemaphore>
OutputPresenter::Image::TakeEndWriteSkiaSemaphores() {
  std::vector<GrBackendSemaphore> result;
  result.swap(end_semaphores_);
  return result;
}

void OutputPresenter::Image::EndWriteSkia(bool force_flush) {
  // The Flush now takes place in finishPaintCurrentBuffer on the CPU side.
  // check if end_semaphores is not empty then flush here
  DCHECK(scoped_skia_write_access_);
  auto end_state = scoped_skia_write_access_->TakeEndState();
  if (!end_semaphores_.empty() || end_state || force_flush) {
    GrFlushInfo flush_info = {
        .fNumSemaphores = end_semaphores_.size(),
        .fSignalSemaphores = end_semaphores_.data(),
    };
    scoped_skia_write_access_->surface()->flush(flush_info, end_state.get());
    auto* direct_context = scoped_skia_write_access_->surface()
                               ->recordingContext()
                               ->asDirectContext();
    DCHECK(direct_context);
    direct_context->submit();
  }
  scoped_skia_write_access_.reset();
  end_semaphores_.clear();

  // SkiaRenderer always draws the full frame.
  skia_representation_->SetCleared();
}

void OutputPresenter::Image::PreGrContextSubmit() {
  DCHECK(scoped_skia_write_access_);
  if (auto end_state = scoped_skia_write_access_->TakeEndState()) {
    scoped_skia_write_access_->surface()->flush({}, end_state.get());
  }
}

std::unique_ptr<OutputPresenter::Image> OutputPresenter::AllocateSingleImage(
    gfx::ColorSpace color_space,
    gfx::Size image_size) {
  return nullptr;
}

void OutputPresenter::ScheduleOneOverlay(const OverlayCandidate& overlay,
                                         ScopedOverlayAccess* access) {
  NOTREACHED();
}

}  // namespace viz
