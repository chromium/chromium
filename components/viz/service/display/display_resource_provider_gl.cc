// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/display_resource_provider_gl.h"

#include "base/dcheck_is_on.h"
#include "build/build_config.h"
#include "gpu/command_buffer/client/gles2_interface.h"

using gpu::gles2::GLES2Interface;

namespace viz {
namespace {

class ScopedSetActiveTexture {
 public:
  ScopedSetActiveTexture(GLES2Interface* gl, GLenum unit)
      : gl_(gl), unit_(unit) {
#if DCHECK_IS_ON()
    GLint active_unit = 0;
    gl->GetIntegerv(GL_ACTIVE_TEXTURE, &active_unit);
    DCHECK_EQ(GL_TEXTURE0, active_unit);
#endif

    if (unit_ != GL_TEXTURE0)
      gl_->ActiveTexture(unit_);
  }

  ~ScopedSetActiveTexture() {
    // Active unit being GL_TEXTURE0 is effectively the ground state.
    if (unit_ != GL_TEXTURE0)
      gl_->ActiveTexture(GL_TEXTURE0);
  }

 private:
  GLES2Interface* gl_;
  GLenum unit_;
};

}  // namespace

DisplayResourceProviderGL::DisplayResourceProviderGL(
    ContextProvider* compositor_context_provider,
    SharedBitmapManager* shared_bitmap_manager,
    bool enable_shared_images)
    : DisplayResourceProvider(DisplayResourceProvider::kGpu,
                              compositor_context_provider,
                              shared_bitmap_manager,
                              enable_shared_images) {}

GLenum DisplayResourceProviderGL::GetResourceTextureTarget(ResourceId id) {
  return GetResource(id)->transferable.mailbox_holder.texture_target;
}

void DisplayResourceProviderGL::WaitSyncToken(ResourceId id) {
  ChildResource* resource = TryGetResource(id);
  // TODO(ericrk): We should never fail TryGetResource, but we appear to
  // be doing so on Android in rare cases. Handle this gracefully until a
  // better solution can be found. https://crbug.com/811858
  if (!resource)
    return;
  WaitSyncTokenInternal(resource);

#if defined(OS_ANDROID)
  // Now that the resource is synced, we may send it a promotion hint.
  InitializePromotionHintRequest(id);
#endif
}

GLenum DisplayResourceProviderGL::BindForSampling(ResourceId resource_id,
                                                  GLenum unit,
                                                  GLenum filter) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  GLES2Interface* gl = ContextGL();
  auto it = resources_.find(resource_id);
  // TODO(ericrk): We should never fail to find resource_id, but we appear to
  // be doing so on Android in rare cases. Handle this gracefully until a
  // better solution can be found. https://crbug.com/811858
  if (it == resources_.end())
    return GL_TEXTURE_2D;

  ChildResource* resource = &it->second;
  DCHECK(resource->lock_for_read_count);

  ScopedSetActiveTexture scoped_active_tex(gl, unit);
  GLenum target = resource->transferable.mailbox_holder.texture_target;
  gl->BindTexture(target, resource->gl_id);

  // Texture parameters can be modified by concurrent reads so reset them
  // before binding the texture. See https://crbug.com/1092080.
  gl->TexParameteri(target, GL_TEXTURE_MIN_FILTER, filter);
  gl->TexParameteri(target, GL_TEXTURE_MAG_FILTER, filter);
  resource->filter = filter;

  return target;
}

void DisplayResourceProviderGL::WaitSyncTokenInternal(ChildResource* resource) {
  DCHECK(resource);
  if (!resource->ShouldWaitSyncToken())
    return;
  GLES2Interface* gl = ContextGL();
  DCHECK(gl);
  // In the case of context lost, this sync token may be empty (see comment in
  // the UpdateSyncToken() function). The WaitSyncTokenCHROMIUM() function
  // handles empty sync tokens properly so just wait anyways and update the
  // state the synchronized.
  gl->WaitSyncTokenCHROMIUM(resource->sync_token().GetConstData());
  resource->SetSynchronized();
}

DisplayResourceProviderGL::ScopedReadLockGL::ScopedReadLockGL(
    DisplayResourceProviderGL* resource_provider,
    ResourceId resource_id)
    : resource_provider_(resource_provider), resource_id_(resource_id) {
  const ChildResource* resource =
      resource_provider->LockForRead(resource_id, false /* overlay_only */);
  // TODO(ericrk): We should never fail LockForRead, but we appear to be
  // doing so on Android in rare cases. Handle this gracefully until a better
  // solution can be found. https://crbug.com/811858
  if (!resource)
    return;

  texture_id_ = resource->gl_id;
  target_ = resource->transferable.mailbox_holder.texture_target;
  size_ = resource->transferable.size;
  color_space_ = resource->transferable.color_space;
}

DisplayResourceProviderGL::ScopedReadLockGL::~ScopedReadLockGL() {
  resource_provider_->UnlockForRead(resource_id_, false /* overlay_only */);
}

DisplayResourceProviderGL::ScopedSamplerGL::ScopedSamplerGL(
    DisplayResourceProviderGL* resource_provider,
    ResourceId resource_id,
    GLenum filter)
    : resource_lock_(resource_provider, resource_id),
      unit_(GL_TEXTURE0),
      target_(resource_provider->BindForSampling(resource_id, unit_, filter)) {}

DisplayResourceProviderGL::ScopedSamplerGL::ScopedSamplerGL(
    DisplayResourceProviderGL* resource_provider,
    ResourceId resource_id,
    GLenum unit,
    GLenum filter)
    : resource_lock_(resource_provider, resource_id),
      unit_(unit),
      target_(resource_provider->BindForSampling(resource_id, unit_, filter)) {}

DisplayResourceProviderGL::ScopedSamplerGL::~ScopedSamplerGL() = default;

DisplayResourceProviderGL::ScopedOverlayLockGL::ScopedOverlayLockGL(
    DisplayResourceProviderGL* resource_provider,
    ResourceId resource_id)
    : resource_provider_(resource_provider), resource_id_(resource_id) {
  const ChildResource* resource =
      resource_provider->LockForRead(resource_id, true /* overlay_only */);
  if (!resource)
    return;

  texture_id_ = resource->gl_id;
}

DisplayResourceProviderGL::ScopedOverlayLockGL::~ScopedOverlayLockGL() {
  resource_provider_->UnlockForRead(resource_id_, true /* overlay_only */);
}

}  // namespace viz
