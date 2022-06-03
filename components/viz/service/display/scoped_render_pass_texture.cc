// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/scoped_render_pass_texture.h"

#include <algorithm>

#include "base/bits.h"
#include "base/check.h"
#include "components/viz/common/gpu/context_provider.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gles2_interface.h"

namespace viz {

ScopedRenderPassTexture::ScopedRenderPassTexture() = default;

ScopedRenderPassTexture::ScopedRenderPassTexture(
    ContextProvider* context_provider,
    const gfx::Size& size,
    ResourceFormat format,
    const gfx::ColorSpace& color_space,
    bool mipmap)
    : context_provider_(context_provider),
      size_(size),
      mipmap_(mipmap),
      color_space_(color_space) {
  DCHECK(context_provider_);
  gpu::gles2::GLES2Interface* gl = context_provider_->ContextGL();
  const gpu::Capabilities& caps = context_provider_->ContextCapabilities();
  gl->GenTextures(1, &gl_id_);

  gl->BindTexture(GL_TEXTURE_2D, gl_id_);
  gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  // This texture will be bound as a framebuffer, so optimize for that.
  if (caps.texture_usage) {
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_USAGE_ANGLE,
                      GL_FRAMEBUFFER_ATTACHMENT_ANGLE);
  }

  if (caps.texture_storage) {
    GLint levels = 1;
    if (caps.texture_npot && mipmap_)
      levels += base::bits::Log2Floor(std::max(size_.width(), size_.height()));

    gl->TexStorage2DEXT(GL_TEXTURE_2D, levels, TextureStorageFormat(format),
                        size_.width(), size_.height());
  } else {
    DCHECK(GLSupportsFormat(format));
    gl->TexImage2D(GL_TEXTURE_2D, 0, GLInternalFormat(format), size_.width(),
                   size_.height(), 0, GLDataFormat(format), GLDataType(format),
                   nullptr);
  }
}

ScopedRenderPassTexture::~ScopedRenderPassTexture() {
  Free();
}

ScopedRenderPassTexture::ScopedRenderPassTexture(
    ScopedRenderPassTexture&& other) {
  context_provider_ = other.context_provider_;
  size_ = other.size_;
  mipmap_ = other.mipmap_;
  color_space_ = other.color_space_;
  gl_id_ = other.gl_id_;
  mipmap_state_ = other.mipmap_state_;

  // When being moved, other will no longer hold this gl_id_.
  other.gl_id_ = 0;
}

ScopedRenderPassTexture& ScopedRenderPassTexture::operator=(
    ScopedRenderPassTexture&& other) {
  if (this != &other) {
    Free();
    context_provider_ = other.context_provider_;
    size_ = other.size_;
    mipmap_ = other.mipmap_;
    color_space_ = other.color_space_;
    gl_id_ = other.gl_id_;
    mipmap_state_ = other.mipmap_state_;

    // When being moved, other will no longer hold this gl_id_.
    other.gl_id_ = 0;
  }
  return *this;
}

void ScopedRenderPassTexture::Free() {
  if (!gl_id_)
    return;
  gpu::gles2::GLES2Interface* gl = context_provider_->ContextGL();
  gl->DeleteTextures(1, &gl_id_);
  gl_id_ = 0;
}

void ScopedRenderPassTexture::BindForSampling() {
  gpu::gles2::GLES2Interface* gl = context_provider_->ContextGL();
  gl->BindTexture(GL_TEXTURE_2D, gl_id_);
  switch (mipmap_state_) {
    case INVALID:
      break;
    case GENERATE:
      // TODO(crbug.com/803286): npot texture always return false on ubuntu
      // desktop. The npot texture check is probably failing on desktop GL.
      DCHECK(context_provider_->ContextCapabilities().texture_npot);
      gl->GenerateMipmap(GL_TEXTURE_2D);
      mipmap_state_ = VALID;
      FALLTHROUGH;
    case VALID:
      gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                        GL_LINEAR_MIPMAP_LINEAR);
      break;
  }
}

}  // namespace viz
