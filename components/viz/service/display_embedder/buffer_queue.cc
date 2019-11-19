// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/buffer_queue.h"

#include <utility>

#include "base/containers/adapters.h"
#include "build/build_config.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gl/buffer_format_utils.h"
#include "ui/gl/gl_image.h"

namespace viz {

BufferQueue::BufferQueue(gpu::gles2::GLES2Interface* gl,
                         gfx::BufferFormat format,
                         gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
                         gpu::SurfaceHandle surface_handle,
                         const gpu::Capabilities& capabilities)
    : gl_(gl),
      allocated_count_(0),
      texture_target_(gpu::GetBufferTextureTarget(gfx::BufferUsage::SCANOUT,
                                                  format,
                                                  capabilities)),
      internal_format_(base::strict_cast<uint32_t>(
          gl::BufferFormatToGLInternalFormat(format))),
      format_(format),
      gpu_memory_buffer_manager_(gpu_memory_buffer_manager),
      surface_handle_(surface_handle) {}

BufferQueue::~BufferQueue() {
  FreeAllSurfaces();
}

unsigned BufferQueue::GetCurrentBuffer(unsigned* stencil) {
  DCHECK(stencil);
  if (!current_surface_)
    current_surface_ = GetNextSurface();
  if (current_surface_) {
    *stencil = current_surface_->stencil;
    return current_surface_->texture;
  }
  *stencil = 0u;
  return 0u;
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
  DCHECK(current_surface_);
  return current_surface_->damage;
}

void BufferQueue::SwapBuffers(const gfx::Rect& damage) {
  UpdateBufferDamage(damage);
  if (current_surface_)
    current_surface_->damage = gfx::Rect();
  in_flight_surfaces_.push_back(std::move(current_surface_));
}

bool BufferQueue::Reshape(const gfx::Size& size,
                          float scale_factor,
                          const gfx::ColorSpace& color_space,
                          bool use_stencil) {
  if (size == size_ && color_space == color_space_ &&
      use_stencil == use_stencil_) {
    return false;
  }
#if !defined(OS_MACOSX)
  // TODO(ccameron): This assert is being hit on Mac try jobs. Determine if that
  // is cause for concern or if it is benign.
  // http://crbug.com/524624
  DCHECK(!current_surface_);
#endif
  size_ = size;
  color_space_ = color_space;
  use_stencil_ = use_stencil;

  FreeAllSurfaces();
  return true;
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
  displayed_surface_.reset();
  current_surface_.reset();
  // This is intentionally not emptied since the swap buffers acks are still
  // expected to arrive.
  for (auto& surface : in_flight_surfaces_)
    surface = nullptr;
  available_surfaces_.clear();
}

void BufferQueue::FreeSurfaceResources(AllocatedSurface* surface) {
  if (!surface->texture)
    return;

  gl_->BindTexture(texture_target_, surface->texture);
  gl_->ReleaseTexImage2DCHROMIUM(texture_target_, surface->image);
  gl_->DeleteTextures(1, &surface->texture);
  gl_->DestroyImageCHROMIUM(surface->image);
  if (surface->stencil)
    gl_->DeleteRenderbuffers(1, &surface->stencil);
  surface->buffer.reset();
  allocated_count_--;
}

std::unique_ptr<BufferQueue::AllocatedSurface> BufferQueue::GetNextSurface() {
  if (!available_surfaces_.empty()) {
    std::unique_ptr<AllocatedSurface> surface =
        std::move(available_surfaces_.back());
    available_surfaces_.pop_back();
    return surface;
  }

  unsigned texture;
  gl_->GenTextures(1, &texture);

  unsigned stencil = 0;
  if (use_stencil_) {
    gl_->GenRenderbuffers(1, &stencil);
    gl_->BindRenderbuffer(GL_RENDERBUFFER, stencil);
    gl_->RenderbufferStorage(GL_RENDERBUFFER, GL_STENCIL_INDEX8, size_.width(),
                             size_.height());
    gl_->BindRenderbuffer(GL_RENDERBUFFER, 0);
  }

  // We don't want to allow anything more than triple buffering.
  DCHECK_LT(allocated_count_, 3U);
  std::unique_ptr<gfx::GpuMemoryBuffer> buffer(
      gpu_memory_buffer_manager_->CreateGpuMemoryBuffer(
          size_, format_, gfx::BufferUsage::SCANOUT, surface_handle_));
  if (!buffer) {
    gl_->DeleteTextures(1, &texture);
    DLOG(ERROR) << "Failed to allocate GPU memory buffer";
    return nullptr;
  }
  buffer->SetColorSpace(color_space_);

  unsigned id =
      gl_->CreateImageCHROMIUM(buffer->AsClientBuffer(), size_.width(),
                               size_.height(), internal_format_);
  if (!id) {
    LOG(ERROR) << "Failed to allocate backing image surface";
    gl_->DeleteTextures(1, &texture);
    return nullptr;
  }

  allocated_count_++;
  gl_->BindTexture(texture_target_, texture);
  gl_->BindTexImage2DCHROMIUM(texture_target_, id);

  // The texture must be bound to the image before setting the color space.
  gl_->SetColorSpaceMetadataCHROMIUM(
      texture, reinterpret_cast<GLColorSpace>(&color_space_));

  return std::make_unique<AllocatedSurface>(this, std::move(buffer), texture,
                                            id, stencil, gfx::Rect(size_));
}

BufferQueue::AllocatedSurface::AllocatedSurface(
    BufferQueue* buffer_queue,
    std::unique_ptr<gfx::GpuMemoryBuffer> buffer,
    unsigned texture,
    unsigned image,
    unsigned stencil,
    const gfx::Rect& rect)
    : buffer_queue(buffer_queue),
      buffer(buffer.release()),
      texture(texture),
      image(image),
      stencil(stencil),
      damage(rect) {}

BufferQueue::AllocatedSurface::~AllocatedSurface() {
  buffer_queue->FreeSurfaceResources(this);
}

}  // namespace viz
