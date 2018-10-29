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
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/skia_util.h"

namespace viz {

BufferQueue::BufferQueue(gpu::gles2::GLES2Interface* gl,
                         uint32_t texture_target,
                         uint32_t internal_format,
                         gfx::BufferFormat format,
                         gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
                         gpu::SurfaceHandle surface_handle)
    : gl_(gl),
      fbo_(0),
      allocated_count_(0),
      texture_target_(texture_target),
      internal_format_(internal_format),
      format_(format),
      gpu_memory_buffer_manager_(gpu_memory_buffer_manager),
      surface_handle_(surface_handle) {
  DCHECK_EQ(internal_format,
            gpu::InternalFormatForGpuMemoryBufferFormat(format_));
}

BufferQueue::~BufferQueue() {
  FreeAllSurfaces();

  if (fbo_)
    gl_->DeleteFramebuffers(1, &fbo_);
}

void BufferQueue::Initialize() {
  gl_->GenFramebuffers(1, &fbo_);
}

void BufferQueue::BindFramebuffer() {
  gl_->BindFramebuffer(GL_FRAMEBUFFER, fbo_);

  if (!current_surface_)
    current_surface_ = GetNextSurface();

  if (current_surface_) {
    gl_->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                              texture_target_, current_surface_->texture, 0);
    if (current_surface_->stencil) {
      gl_->FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT,
                                   GL_RENDERBUFFER, current_surface_->stencil);
    }
  }
}

void BufferQueue::CopyBufferDamage(int texture,
                                   int source_texture,
                                   const gfx::Rect& new_damage,
                                   const gfx::Rect& old_damage) {
  SkRegion region(gfx::RectToSkIRect(old_damage));
  if (!region.op(gfx::RectToSkIRect(new_damage), SkRegion::kDifference_Op))
    return;

  GLuint dst_framebuffer = 0;
  gl_->GenFramebuffers(1, &dst_framebuffer);
  DCHECK(dst_framebuffer);
  gl_->BindFramebuffer(GL_FRAMEBUFFER, dst_framebuffer);
  gl_->BindTexture(texture_target_, texture);
  gl_->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                            texture_target_, source_texture, 0);
  for (SkRegion::Iterator it(region); !it.done(); it.next()) {
    const SkIRect& rect = it.rect();
    gl_->CopyTexSubImage2D(texture_target_, 0, rect.x(), rect.y(), rect.x(),
                           rect.y(), rect.width(), rect.height());
  }
  gl_->BindTexture(texture_target_, 0);
  gl_->Flush();
  gl_->DeleteFramebuffers(1, &dst_framebuffer);
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

void BufferQueue::SwapBuffers(const gfx::Rect& damage) {
  if (damage.IsEmpty()) {
    in_flight_surfaces_.push_back(std::move(current_surface_));
    return;
  }

  if (current_surface_) {
    if (damage != gfx::Rect(size_)) {
      // Copy damage from the most recently swapped buffer. In the event that
      // the buffer was destroyed and failed to recreate, pick from the most
      // recently available buffer.
      uint32_t texture_id = 0;
      for (auto& surface : base::Reversed(in_flight_surfaces_)) {
        if (surface) {
          texture_id = surface->texture;
          break;
        }
      }
      if (!texture_id && displayed_surface_)
        texture_id = displayed_surface_->texture;

      if (texture_id) {
        CopyBufferDamage(current_surface_->texture, texture_id, damage,
                         current_surface_->damage);
      }
    }
    current_surface_->damage = gfx::Rect();
  }
  UpdateBufferDamage(damage);
  in_flight_surfaces_.push_back(std::move(current_surface_));
  // Some things reset the framebuffer (CopyBufferDamage, some GLRenderer
  // paths), so ensure we restore it here.
  gl_->BindFramebuffer(GL_FRAMEBUFFER, fbo_);
}

void BufferQueue::Reshape(const gfx::Size& size,
                          float scale_factor,
                          const gfx::ColorSpace& color_space,
                          bool use_stencil) {
  if (size == size_ && color_space == color_space_ &&
      use_stencil == use_stencil_)
    return;
#if !defined(OS_MACOSX)
  // TODO(ccameron): This assert is being hit on Mac try jobs. Determine if that
  // is cause for concern or if it is benign.
  // http://crbug.com/524624
  DCHECK(!current_surface_);
#endif
  size_ = size;
  color_space_ = color_space;
  use_stencil_ = use_stencil;

  gl_->BindFramebuffer(GL_FRAMEBUFFER, fbo_);
  gl_->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                            texture_target_, 0, 0);
  gl_->FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT,
                               GL_RENDERBUFFER, 0);

  FreeAllSurfaces();
}

void BufferQueue::RecreateBuffers() {
  // We need to recreate the buffers, for whatever reason the old ones are not
  // presentable on the device anymore.
  // Unused buffers can be freed directly, they will be re-allocated as needed.
  // Any in flight, current or displayed surface must be replaced.
  available_surfaces_.clear();

  // Recreate all in-flight surfaces and put the recreated copies in the queue.
  for (auto& surface : in_flight_surfaces_)
    surface = RecreateBuffer(std::move(surface));

  current_surface_ = RecreateBuffer(std::move(current_surface_));
  displayed_surface_ = RecreateBuffer(std::move(displayed_surface_));

  if (current_surface_) {
    // If we have a texture bound, we will need to re-bind it.
    gl_->BindFramebuffer(GL_FRAMEBUFFER, fbo_);
    gl_->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                              texture_target_, current_surface_->texture, 0);
  }
}

std::unique_ptr<BufferQueue::AllocatedSurface> BufferQueue::RecreateBuffer(
    std::unique_ptr<AllocatedSurface> surface) {
  if (!surface)
    return nullptr;

  std::unique_ptr<AllocatedSurface> new_surface(GetNextSurface());
  if (!new_surface)
    return nullptr;

  new_surface->damage = surface->damage;

  // Copy the entire texture.
  CopyBufferDamage(new_surface->texture, surface->texture, gfx::Rect(),
                   gfx::Rect(size_));
  return new_surface;
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

uint32_t BufferQueue::GetCurrentTextureId() const {
  if (current_surface_)
    return current_surface_->texture;

  // Return in-flight or displayed surface texture if no surface is
  // currently bound. This can happen when using overlays and surface
  // damage is empty. Note: |in_flight_surfaces_| entries can be null
  // as a result of calling FreeAllSurfaces().
  for (auto& surface : base::Reversed(in_flight_surfaces_)) {
    if (surface)
      return surface->texture;
  }

  if (displayed_surface_)
    return displayed_surface_->texture;

  return 0;
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

  GLuint texture;
  gl_->GenTextures(1, &texture);

  GLuint stencil = 0;
  if (use_stencil_) {
    gl_->GenRenderbuffers(1, &stencil);
    gl_->BindRenderbuffer(GL_RENDERBUFFER, stencil);
    gl_->RenderbufferStorage(GL_RENDERBUFFER, GL_STENCIL_INDEX8, size_.width(),
                             size_.height());
    gl_->BindRenderbuffer(GL_RENDERBUFFER, 0);
  }

  // We don't want to allow anything more than triple buffering.
  DCHECK_LT(allocated_count_, 4U);
  std::unique_ptr<gfx::GpuMemoryBuffer> buffer(
      gpu_memory_buffer_manager_->CreateGpuMemoryBuffer(
          size_, format_, gfx::BufferUsage::SCANOUT, surface_handle_));
  if (!buffer) {
    gl_->DeleteTextures(1, &texture);
    DLOG(ERROR) << "Failed to allocate GPU memory buffer";
    return nullptr;
  }
  buffer->SetColorSpace(color_space_);

  uint32_t id =
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
  return std::make_unique<AllocatedSurface>(this, std::move(buffer), texture,
                                            id, stencil, gfx::Rect(size_));
}

BufferQueue::AllocatedSurface::AllocatedSurface(
    BufferQueue* buffer_queue,
    std::unique_ptr<gfx::GpuMemoryBuffer> buffer,
    uint32_t texture,
    uint32_t image,
    uint32_t stencil,
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
