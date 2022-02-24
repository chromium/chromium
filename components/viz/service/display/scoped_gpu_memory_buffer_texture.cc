// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/scoped_gpu_memory_buffer_texture.h"

#include "base/check.h"
#include "components/viz/common/gpu/context_provider.h"
#include "components/viz/common/resources/resource_format.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"

namespace viz {

ScopedGpuMemoryBufferTexture::ScopedGpuMemoryBufferTexture(
    ContextProvider* context_provider,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space)
    : context_provider_(context_provider),
      size_(size),
      color_space_(color_space) {
  DCHECK(context_provider_);

  const auto& caps = context_provider->ContextCapabilities();
  // This capability is needed to use TexStorage2DImageCHROMIUM, and should be
  // known to be enabled before using an object of this type.
  DCHECK(caps.texture_storage_image);

  gpu::gles2::GLES2Interface* gl = context_provider_->ContextGL();
  gl->GenTextures(1, &gl_id_);

  gfx::BufferUsage usage = gfx::BufferUsage::SCANOUT;
  ResourceFormat format = RGBA_8888;
  gfx::BufferFormat buffer_format = BufferFormat(format);

  target_ = gpu::GetBufferTextureTarget(usage, buffer_format, caps);

  gl->BindTexture(target_, gl_id_);
  gl->TexParameteri(target_, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  gl->TexParameteri(target_, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  gl->TexParameteri(target_, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  gl->TexParameteri(target_, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  gl->TexStorage2DImageCHROMIUM(
      target_, TextureStorageFormat(format, caps.angle_rgbx_internal_format),
      GL_SCANOUT_CHROMIUM, size_.width(), size_.height());
  if (color_space_.IsValid()) {
    gl->SetColorSpaceMetadataCHROMIUM(gl_id_, color_space_.AsGLColorSpace());
  }
  gl->BindTexture(target_, 0);
}

ScopedGpuMemoryBufferTexture::ScopedGpuMemoryBufferTexture() = default;

ScopedGpuMemoryBufferTexture::~ScopedGpuMemoryBufferTexture() {
  Free();
}

ScopedGpuMemoryBufferTexture::ScopedGpuMemoryBufferTexture(
    ScopedGpuMemoryBufferTexture&& other)
    : context_provider_(other.context_provider_),
      gl_id_(other.gl_id_),
      target_(other.target_),
      size_(other.size_),
      color_space_(other.color_space_) {
  other.gl_id_ = 0;
}

ScopedGpuMemoryBufferTexture& ScopedGpuMemoryBufferTexture::operator=(
    ScopedGpuMemoryBufferTexture&& other) {
  DCHECK(!context_provider_ || !other.context_provider_ ||
         context_provider_ == other.context_provider_);
  if (this != &other) {
    Free();
    context_provider_ = other.context_provider_;
    gl_id_ = other.gl_id_;
    target_ = other.target_;
    size_ = other.size_;
    color_space_ = other.color_space_;

    other.gl_id_ = 0;
  }
  return *this;
}

void ScopedGpuMemoryBufferTexture::Free() {
  if (!gl_id_)
    return;
  gpu::gles2::GLES2Interface* gl = context_provider_->ContextGL();
  gl->DeleteTextures(1, &gl_id_);
  gl_id_ = 0;
}

}  // namespace viz
