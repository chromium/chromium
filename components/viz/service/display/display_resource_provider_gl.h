// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_DISPLAY_RESOURCE_PROVIDER_GL_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_DISPLAY_RESOURCE_PROVIDER_GL_H_

#include <vector>

#include "components/viz/service/display/display_resource_provider.h"
#include "components/viz/service/viz_service_export.h"

namespace gpu {
namespace gles2 {
class GLES2Interface;
}  // namespace gles2
}  // namespace gpu

namespace viz {

class ContextProvider;

// DisplayResourceProvider implementation used with GLRenderer.
class VIZ_SERVICE_EXPORT DisplayResourceProviderGL
    : public DisplayResourceProvider {
 public:
  // Android with GLRenderer doesn't support overlays with shared images
  // enabled. For everything else |enable_shared_images| is true.
  DisplayResourceProviderGL(ContextProvider* compositor_context_provider,
                            bool enable_shared_images = true);
  ~DisplayResourceProviderGL() override;

  GLenum GetResourceTextureTarget(ResourceId id);
  void WaitSyncToken(ResourceId id);

  // The following lock classes are part of the DisplayResourceProvider API and
  // are needed to read the resource contents. The user must ensure that they
  // only use GL locks on GL resources, etc, and this is enforced by assertions.
  class VIZ_SERVICE_EXPORT ScopedReadLockGL {
   public:
    ScopedReadLockGL(DisplayResourceProviderGL* resource_provider,
                     ResourceId resource_id);
    ~ScopedReadLockGL();

    ScopedReadLockGL(const ScopedReadLockGL&) = delete;
    ScopedReadLockGL& operator=(const ScopedReadLockGL&) = delete;

    GLuint texture_id() const { return texture_id_; }
    GLenum target() const { return target_; }
    const gfx::Size& size() const { return size_; }
    const gfx::ColorSpace& color_space() const { return color_space_; }

   private:
    DisplayResourceProviderGL* const resource_provider_;
    const ResourceId resource_id_;

    GLuint texture_id_ = 0;
    GLenum target_ = GL_TEXTURE_2D;
    gfx::Size size_;
    gfx::ColorSpace color_space_;
  };

  class VIZ_SERVICE_EXPORT ScopedSamplerGL {
   public:
    ScopedSamplerGL(DisplayResourceProviderGL* resource_provider,
                    ResourceId resource_id,
                    GLenum filter);
    ScopedSamplerGL(DisplayResourceProviderGL* resource_provider,
                    ResourceId resource_id,
                    GLenum unit,
                    GLenum filter);
    ~ScopedSamplerGL();

    ScopedSamplerGL(const ScopedSamplerGL&) = delete;
    ScopedSamplerGL& operator=(const ScopedSamplerGL&) = delete;

    GLuint texture_id() const { return resource_lock_.texture_id(); }
    GLenum target() const { return target_; }
    const gfx::ColorSpace& color_space() const {
      return resource_lock_.color_space();
    }

   private:
    const ScopedReadLockGL resource_lock_;
    const GLenum unit_;
    const GLenum target_;
  };

  class VIZ_SERVICE_EXPORT ScopedOverlayLockGL {
   public:
    ScopedOverlayLockGL(DisplayResourceProviderGL* resource_provider,
                        ResourceId resource_id);
    ~ScopedOverlayLockGL();

    ScopedOverlayLockGL(const ScopedOverlayLockGL&) = delete;
    ScopedOverlayLockGL& operator=(const ScopedOverlayLockGL&) = delete;

    GLuint texture_id() const { return texture_id_; }

   private:
    DisplayResourceProviderGL* const resource_provider_;
    const ResourceId resource_id_;
    GLuint texture_id_ = 0;
  };

  class VIZ_SERVICE_EXPORT SynchronousFence : public ResourceFence {
   public:
    explicit SynchronousFence(gpu::gles2::GLES2Interface* gl);

    SynchronousFence(const SynchronousFence&) = delete;
    SynchronousFence& operator=(const SynchronousFence&) = delete;

    // ResourceFence implementation.
    void Set() override;
    bool HasPassed() override;

    // Returns true if fence has been set but not yet synchornized.
    bool has_synchronized() const { return has_synchronized_; }

   private:
    ~SynchronousFence() override;

    void Synchronize();

    gpu::gles2::GLES2Interface* gl_;
    bool has_synchronized_;
  };

 private:
  const ChildResource* LockForRead(ResourceId id, bool overlay_only);
  void UnlockForRead(ResourceId id, bool overlay_only);

  // DisplayResourceProvider overrides:
  std::vector<ReturnedResource> DeleteAndReturnUnusedResourcesToChildImpl(
      Child& child_info,
      DeleteStyle style,
      const std::vector<ResourceId>& unused) override;

  gpu::gles2::GLES2Interface* ContextGL() const;
  void DeleteResourceInternal(ResourceMap::iterator it);
  GLenum BindForSampling(ResourceId resource_id, GLenum unit, GLenum filter);
  void WaitSyncTokenInternal(ChildResource* resource);

  ContextProvider* const compositor_context_provider_;
  const bool enable_shared_images_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_DISPLAY_RESOURCE_PROVIDER_GL_H_
