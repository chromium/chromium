// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/buffer_queue.h"

#include <utility>

#include "base/containers/adapters.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace viz {

BufferQueue::BufferQueue(gpu::SharedImageInterface* sii,
                         gpu::SurfaceHandle surface_handle)
    : sii_(sii),
      allocated_count_(0),
      surface_handle_(surface_handle) {}

BufferQueue::~BufferQueue() {
  FreeAllSurfaces();
}

void BufferQueue::SetSyncTokenProvider(SyncTokenProvider* sync_token_provider) {
  DCHECK(!sync_token_provider_);
  sync_token_provider_ = sync_token_provider;
}

gpu::Mailbox BufferQueue::GetCurrentBuffer(
    gpu::SyncToken* creation_sync_token) {
  DCHECK(creation_sync_token);
  if (!current_surface_)
    current_surface_ = GetNextSurface(creation_sync_token);
  return current_surface_ ? current_surface_->mailbox : gpu::Mailbox();
}

void BufferQueue::UpdateBufferDamage(const gfx::Rect& damage) {
  if (displayed_surface_)
    displayed_surface_->damage.Union(damage);
  for (auto& surface : available_surfaces_)
    surface->damage.Union(damage);
  for (auto& surface : in_flight_surfaces_) {
    if (surface)
      surface->damage.Union(damage);
  }
}

gfx::Rect BufferQueue::CurrentBufferDamage() const {
  if (current_surface_)
    return current_surface_->damage;

  // In case there is no current_surface_, we get the damage from the surface
  // that will be set as current_surface_ by the next call to GetNextSurface.
  if (!available_surfaces_.empty()) {
    return available_surfaces_.back()->damage;
  }

  // If we can't determine which surface will be the next current_surface_, we
  // conservatively invalidate the whole buffer.
  return gfx::Rect(size_);
}

void BufferQueue::SwapBuffers(const gfx::Rect& damage) {
  UpdateBufferDamage(damage);
  if (current_surface_)
    current_surface_->damage = gfx::Rect();
  in_flight_surfaces_.push_back(std::move(current_surface_));
}

bool BufferQueue::Reshape(const gfx::Size& size,
                          const gfx::ColorSpace& color_space,
                          gfx::BufferFormat format) {
  if (size == size_ && color_space == color_space_ && format == format_)
    return false;

#if !defined(OS_APPLE)
  // TODO(ccameron): This assert is being hit on Mac try jobs. Determine if that
  // is cause for concern or if it is benign.
  // http://crbug.com/524624
  DCHECK(!current_surface_);
#endif
  size_ = size;
  color_space_ = color_space;
  format_ = format;

  FreeAllSurfaces();
  return true;
}

void BufferQueue::SetMaxBuffers(size_t max) {
  max_buffers_ = max;
}

void BufferQueue::PageFlipComplete() {
  DCHECK(!in_flight_surfaces_.empty());
  if (in_flight_surfaces_.front()) {
    if (displayed_surface_)
      available_surfaces_.push_back(std::move(displayed_surface_));
    displayed_surface_ = std::move(in_flight_surfaces_.front());
  }

  in_flight_surfaces_.pop_front();
}

void BufferQueue::FreeAllSurfaces() {
  DCHECK(sync_token_provider_);
  const gpu::SyncToken destruction_sync_token =
      sync_token_provider_->GenSyncToken();
  FreeSurface(std::move(displayed_surface_), destruction_sync_token);
  FreeSurface(std::move(current_surface_), destruction_sync_token);

  // This is intentionally not emptied since the swap buffers acks are still
  // expected to arrive.
  for (auto& surface : in_flight_surfaces_) {
    FreeSurface(std::move(surface), destruction_sync_token);
  }

  for (auto& surface : available_surfaces_) {
    FreeSurface(std::move(surface), destruction_sync_token);
  }
  available_surfaces_.clear();
}

void BufferQueue::FreeSurface(std::unique_ptr<AllocatedSurface> surface,
                              const gpu::SyncToken& sync_token) {
  if (!surface)
    return;
  DCHECK(!surface->mailbox.IsZero());
  sii_->DestroySharedImage(sync_token, surface->mailbox);
  allocated_count_--;
}

std::unique_ptr<BufferQueue::AllocatedSurface> BufferQueue::GetNextSurface(
    gpu::SyncToken* creation_sync_token) {
  DCHECK(creation_sync_token);
  if (!available_surfaces_.empty()) {
    std::unique_ptr<AllocatedSurface> surface =
        std::move(available_surfaces_.back());
    available_surfaces_.pop_back();
    return surface;
  }

  // We don't want to allow anything more than triple buffering.
  DCHECK_LT(allocated_count_, max_buffers_);

  DCHECK(format_);
  const ResourceFormat format = GetResourceFormat(format_.value());
  const gpu::Mailbox mailbox = sii_->CreateSharedImage(
      format, size_, color_space_, kTopLeft_GrSurfaceOrigin,
      kPremul_SkAlphaType,
      gpu::SHARED_IMAGE_USAGE_SCANOUT |
          gpu::SHARED_IMAGE_USAGE_GLES2_FRAMEBUFFER_HINT,
      surface_handle_);

  if (mailbox.IsZero()) {
    LOG(ERROR) << "Failed to create SharedImage";
    return nullptr;
  }

  allocated_count_++;
  *creation_sync_token = sii_->GenUnverifiedSyncToken();
  return std::make_unique<AllocatedSurface>(mailbox, gfx::Rect(size_));
}

BufferQueue::AllocatedSurface::AllocatedSurface(const gpu::Mailbox& mailbox,
                                                const gfx::Rect& rect)
    : mailbox(mailbox), damage(rect) {}

BufferQueue::AllocatedSurface::~AllocatedSurface() = default;

}  // namespace viz
