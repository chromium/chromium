// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/display_resource_provider_gl.h"

#include "base/dcheck_is_on.h"
#include "build/build_config.h"
#include "gpu/GLES2/gl2extchromium.h"
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

const DisplayResourceProvider::ChildResource*
DisplayResourceProviderGL::LockForRead(ResourceId id, bool overlay_only) {
  // TODO(ericrk): We should never fail TryGetResource, but we appear to be
  // doing so on Android in rare cases. Handle this gracefully until a better
  // solution can be found. https://crbug.com/811858
  ChildResource* resource = TryGetResource(id);
  if (!resource)
    return nullptr;

  // Mailbox sync_tokens must be processed by a call to WaitSyncToken() prior to
  // calling LockForRead().
  DCHECK_NE(NEEDS_WAIT, resource->synchronization_state());
  DCHECK(resource->is_gpu_resource_type());

  const gpu::Mailbox& mailbox = resource->transferable.mailbox_holder.mailbox;
  GLES2Interface* gl = ContextGL();
  DCHECK(gl);
  if (!resource->gl_id) {
    if (mailbox.IsSharedImage() && enable_shared_images_) {
      resource->gl_id =
          gl->CreateAndTexStorage2DSharedImageCHROMIUM(mailbox.name);
    } else {
      resource->gl_id = gl->CreateAndConsumeTextureCHROMIUM(
          resource->transferable.mailbox_holder.mailbox.name);
    }
    resource->SetLocallyUsed();
  }
  if (mailbox.IsSharedImage() && enable_shared_images_) {
    if (overlay_only) {
      if (resource->lock_for_overlay_count == 0) {
        // If |lock_for_read_count| > 0, then BeginSharedImageAccess has
        // already been called with READ, so don't re-lock with OVERLAY.
        if (resource->lock_for_read_count == 0) {
          gl->BeginSharedImageAccessDirectCHROMIUM(
              resource->gl_id, GL_SHARED_IMAGE_ACCESS_MODE_OVERLAY_CHROMIUM);
        }
      }
    } else {
      if (resource->lock_for_read_count == 0) {
        // If |lock_for_overlay_count| > 0, then we have already begun access
        // for OVERLAY. End this access and "upgrade" it to READ.
        // See https://crbug.com/1113925 for how this can go wrong.
        if (resource->lock_for_overlay_count > 0)
          gl->EndSharedImageAccessDirectCHROMIUM(resource->gl_id);
        gl->BeginSharedImageAccessDirectCHROMIUM(
            resource->gl_id, GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM);
      }
    }
  }

  if (overlay_only)
    resource->lock_for_overlay_count++;
  else
    resource->lock_for_read_count++;
  if (resource->transferable.read_lock_fences_enabled) {
    if (current_read_lock_fence_.get())
      current_read_lock_fence_->Set();
    resource->read_lock_fence = current_read_lock_fence_;
  }

  return resource;
}

void DisplayResourceProviderGL::UnlockForRead(ResourceId id,
                                              bool overlay_only) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  ChildResource* resource = TryGetResource(id);
  // TODO(ericrk): We should never fail to find id, but we appear to be
  // doing so on Android in rare cases. Handle this gracefully until a better
  // solution can be found. https://crbug.com/811858
  if (!resource)
    return;

  DCHECK(resource->is_gpu_resource_type());
  if (resource->transferable.mailbox_holder.mailbox.IsSharedImage() &&
      enable_shared_images_) {
    // If this is the last READ or OVERLAY access, then end access.
    if (resource->lock_for_read_count + resource->lock_for_overlay_count == 1) {
      DCHECK(resource->gl_id);
      GLES2Interface* gl = ContextGL();
      DCHECK(gl);
      gl->EndSharedImageAccessDirectCHROMIUM(resource->gl_id);
    }
  }
  if (overlay_only) {
    DCHECK_GT(resource->lock_for_overlay_count, 0);
    resource->lock_for_overlay_count--;
  } else {
    DCHECK_GT(resource->lock_for_read_count, 0);
    resource->lock_for_read_count--;
  }
  TryReleaseResource(id, resource);
}

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
