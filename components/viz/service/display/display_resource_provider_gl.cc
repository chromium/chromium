// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/display_resource_provider_gl.h"

#include <vector>

#include "base/dcheck_is_on.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/viz/common/gpu/context_provider.h"
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
    bool enable_shared_images)
    : DisplayResourceProvider(DisplayResourceProvider::kGpu),
      compositor_context_provider_(compositor_context_provider),
      enable_shared_images_(enable_shared_images) {
  DCHECK(compositor_context_provider_);
}

DisplayResourceProviderGL::~DisplayResourceProviderGL() {
  Destroy();
  GLES2Interface* gl = ContextGL();
  if (gl)
    gl->Finish();

  while (!resources_.empty())
    DeleteResourceInternal(resources_.begin());
}

void DisplayResourceProviderGL::DeleteResourceInternal(
    ResourceMap::iterator it) {
  TRACE_EVENT0("viz", "DisplayResourceProvider::DeleteResourceInternal");
  ChildResource* resource = &it->second;

  if (resource->gl_id) {
    GLES2Interface* gl = ContextGL();
    DCHECK(gl);
    gl->DeleteTextures(1, &resource->gl_id);
  }

  resources_.erase(it);
}

GLES2Interface* DisplayResourceProviderGL::ContextGL() const {
  DCHECK(compositor_context_provider_);
  return compositor_context_provider_->ContextGL();
}

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

std::vector<ReturnedResource>
DisplayResourceProviderGL::DeleteAndReturnUnusedResourcesToChildImpl(
    Child& child_info,
    DeleteStyle style,
    const std::vector<ResourceId>& unused) {
  std::vector<ReturnedResource> to_return;
  // Reserve enough space to avoid re-allocating, so we can keep item pointers
  // for later using.
  to_return.reserve(unused.size());
  std::vector<ReturnedResource*> need_synchronization_resources;
  std::vector<GLbyte*> unverified_sync_tokens;

  GLES2Interface* gl = ContextGL();
  DCHECK(gl);
  for (ResourceId local_id : unused) {
    auto it = resources_.find(local_id);
    CHECK(it != resources_.end());
    ChildResource& resource = it->second;
    DCHECK(resource.is_gpu_resource_type());

    ResourceId child_id = resource.transferable.id;
    DCHECK(child_info.child_to_parent_map.count(child_id));

    bool is_lost = lost_context_provider_;
    auto can_delete = CanDeleteNow(child_info, resource, style);
    if (can_delete == CanDeleteNowResult::kNo) {
      // Defer this resource deletion.
      resource.marked_for_deletion = true;
      continue;
    }

    is_lost = is_lost || can_delete == CanDeleteNowResult::kYesButLoseResource;

    if (resource.gl_id && resource.filter != resource.transferable.filter) {
      DCHECK(resource.transferable.mailbox_holder.texture_target);
      DCHECK(!resource.ShouldWaitSyncToken());
      gl->BindTexture(resource.transferable.mailbox_holder.texture_target,
                      resource.gl_id);
      gl->TexParameteri(resource.transferable.mailbox_holder.texture_target,
                        GL_TEXTURE_MIN_FILTER, resource.transferable.filter);
      gl->TexParameteri(resource.transferable.mailbox_holder.texture_target,
                        GL_TEXTURE_MAG_FILTER, resource.transferable.filter);
      resource.SetLocallyUsed();
    }

    to_return.emplace_back(child_id, resource.sync_token(),
                           resource.imported_count, is_lost);
    auto& returned = to_return.back();

    if (resource.needs_sync_token()) {
      need_synchronization_resources.push_back(&returned);
    } else if (returned.sync_token.HasData() &&
               !returned.sync_token.verified_flush()) {
      unverified_sync_tokens.push_back(returned.sync_token.GetData());
    }

    child_info.child_to_parent_map.erase(child_id);
    resource.imported_count = 0;
#if defined(OS_ANDROID)
    DeletePromotionHint(it);
#endif
    DeleteResourceInternal(it);
  }

  gpu::SyncToken new_sync_token;
  if (!need_synchronization_resources.empty()) {
    gl->GenUnverifiedSyncTokenCHROMIUM(new_sync_token.GetData());
    unverified_sync_tokens.push_back(new_sync_token.GetData());
  }

  if (!unverified_sync_tokens.empty()) {
    gl->VerifySyncTokensCHROMIUM(unverified_sync_tokens.data(),
                                 unverified_sync_tokens.size());
  }

  // Set sync token after verification.
  for (ReturnedResource* returned : need_synchronization_resources)
    returned->sync_token = new_sync_token;

  return to_return;
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

DisplayResourceProviderGL::SynchronousFence::SynchronousFence(
    gpu::gles2::GLES2Interface* gl)
    : gl_(gl), has_synchronized_(true) {}

DisplayResourceProviderGL::SynchronousFence::~SynchronousFence() = default;

void DisplayResourceProviderGL::SynchronousFence::Set() {
  has_synchronized_ = false;
}

bool DisplayResourceProviderGL::SynchronousFence::HasPassed() {
  if (!has_synchronized_) {
    has_synchronized_ = true;
    Synchronize();
  }
  return true;
}

void DisplayResourceProviderGL::SynchronousFence::Synchronize() {
  TRACE_EVENT0("viz", "DisplayResourceProvider::SynchronousFence::Synchronize");
  gl_->Finish();
}

}  // namespace viz
