// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/output_presenter.h"

#include <utility>

#include "components/viz/service/display_embedder/skia_output_surface_dependency.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/shared_image_factory.h"
#include "third_party/skia/include/gpu/GrBackendSemaphore.h"
#include "third_party/skia/include/gpu/GrDirectContext.h"

namespace viz {

OutputPresenter::Image::Image(
    gpu::SharedImageFactory* factory,
    gpu::SharedImageRepresentationFactory* representation_factory,
    SkiaOutputSurfaceDependency* deps)
    : factory_(factory),
      representation_factory_(representation_factory),
      deps_(deps) {}

OutputPresenter::Image::~Image() {
  // TODO(vasilyt): As we are going to delete image anyway we should be able
  // to abort write to avoid unnecessary flush to submit semaphores.
  if (scoped_skia_write_access_) {
    EndWriteSkia();
  }
  DCHECK(!scoped_skia_write_access_);
  factory_->DestroySharedImage(mailbox_);
}

bool OutputPresenter::Image::Initialize(const gfx::Size& size,
                                        const gfx::ColorSpace& color_space,
                                        SharedImageFormat format,
                                        uint32_t shared_image_usage) {
  auto mailbox = gpu::Mailbox::GenerateForSharedImage();

  if (!factory_->CreateSharedImage(
          mailbox, format, size, color_space, kTopLeft_GrSurfaceOrigin,
          kPremul_SkAlphaType, deps_->GetSurfaceHandle(), shared_image_usage)) {
    DLOG(ERROR) << "CreateSharedImage failed.";
    return false;
  }
  mailbox_ = mailbox;

  skia_representation_ = representation_factory_->ProduceSkia(
      mailbox_, deps_->GetSharedContextState());
  if (!skia_representation_) {
    DLOG(ERROR) << "ProduceSkia() failed.";
    return false;
  }

  overlay_representation_ = representation_factory_->ProduceOverlay(mailbox_);
  if (!overlay_representation_) {
    DLOG(ERROR) << "ProduceOverlay() failed";
    return false;
  }

  return true;
}

void OutputPresenter::Image::BeginWriteSkia(int sample_count) {
  DCHECK(!scoped_skia_write_access_);
  DCHECK(!GetPresentCount());
  DCHECK(end_semaphores_.empty());

  SetNotPurgeable();

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

bool OutputPresenter::Image::SetPurgeable() {
  if (is_purgeable_)
    return false;
  is_purgeable_ = true;

  // It is possible that `scoped_skia_write_access_` has been created
  // (pre-emptively, but never used). In that case, remove the write access.
  if (scoped_skia_write_access_) {
    EndWriteSkia(/*force_flush=*/false);
  }

  deps_->GetSharedImageManager()->SetPurgeable(mailbox_, true);
  return true;
}

void OutputPresenter::Image::SetNotPurgeable() {
  if (!is_purgeable_)
    return;
  is_purgeable_ = false;
  deps_->GetSharedImageManager()->SetPurgeable(mailbox_, false);
}

std::unique_ptr<OutputPresenter::Image> OutputPresenter::AllocateSingleImage(
    gfx::ColorSpace color_space,
    gfx::Size image_size) {
  return nullptr;
}

bool OutputPresenter::SupportsGpuVSync() const {
  return false;
}

}  // namespace viz
