// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/display_resource_provider.h"

#include "base/atomic_sequence_num.h"
#include "base/numerics/safe_math.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/trace_event.h"
#include "components/viz/common/gpu/context_provider.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "components/viz/common/resources/resource_sizes.h"
#include "components/viz/service/display/shared_bitmap_manager.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "ui/gl/trace_util.h"

using gpu::gles2::GLES2Interface;

namespace viz {

namespace {

// Generates process-unique IDs to use for tracing resources.
base::AtomicSequenceNumber g_next_display_resource_provider_tracing_id;

}  // namespace

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

DisplayResourceProvider::DisplayResourceProvider(
    Mode mode,
    ContextProvider* compositor_context_provider,
    SharedBitmapManager* shared_bitmap_manager)
    : mode_(mode),
      compositor_context_provider_(compositor_context_provider),
      shared_bitmap_manager_(shared_bitmap_manager),
      tracing_id_(g_next_display_resource_provider_tracing_id.GetNext()) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // If no ContextProvider, then we are doing software compositing and a
  // SharedBitmapManager must be given.
  DCHECK(mode_ == kGpu || shared_bitmap_manager);

  // In certain cases, ThreadTaskRunnerHandle isn't set (Android Webview).
  // Don't register a dump provider in these cases.
  // TODO(crbug.com/517156): Get this working in Android Webview.
  if (base::ThreadTaskRunnerHandle::IsSet()) {
    base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
        this, "cc::ResourceProvider", base::ThreadTaskRunnerHandle::Get());
  }
}

DisplayResourceProvider::~DisplayResourceProvider() {
  while (!children_.empty())
    DestroyChildInternal(children_.begin(), FOR_SHUTDOWN);

  GLES2Interface* gl = ContextGL();
  if (gl)
    gl->Finish();

  while (!resources_.empty())
    DeleteResourceInternal(resources_.begin(), FOR_SHUTDOWN);

  if (compositor_context_provider_) {
    // Check that all GL resources has been deleted.
    for (const auto& pair : resources_)
      DCHECK(!pair.second.is_gpu_resource_type());
  }

  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);
}

bool DisplayResourceProvider::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  for (const auto& resource_entry : resources_) {
    const auto& resource = resource_entry.second;

    bool backing_memory_allocated = false;
    if (resource.transferable.is_software)
      backing_memory_allocated = !!resource.shared_bitmap;
    else
      backing_memory_allocated = !!resource.gl_id;

    if (!backing_memory_allocated) {
      // Don't log unallocated resources - they have no backing memory.
      continue;
    }

    // ResourceIds are not process-unique, so log with the ResourceProvider's
    // unique id.
    std::string dump_name =
        base::StringPrintf("cc/resource_memory/provider_%d/resource_%d",
                           tracing_id_, resource_entry.first);
    base::trace_event::MemoryAllocatorDump* dump =
        pmd->CreateAllocatorDump(dump_name);

    // Texture resources may not come with a size, in which case don't report
    // one.
    if (!resource.transferable.size.IsEmpty()) {
      uint64_t total_bytes = ResourceSizes::UncheckedSizeInBytesAligned<size_t>(
          resource.transferable.size, resource.transferable.format);
      dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                      base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                      static_cast<uint64_t>(total_bytes));
    }

    // Resources may be shared across processes and require a shared GUID to
    // prevent double counting the memory.
    base::trace_event::MemoryAllocatorDumpGuid guid;
    base::UnguessableToken shared_memory_guid;
    if (resource.transferable.is_software) {
      shared_memory_guid = resource.shared_bitmap_tracing_guid;
    } else {
      guid = gl::GetGLTextureClientGUIDForTracing(
          compositor_context_provider_->ContextSupport()
              ->ShareGroupTracingGUID(),
          resource.gl_id);
    }

    DCHECK(!shared_memory_guid.is_empty() || !guid.empty());

    // The client that owns the resource will use a higher importance (2), and
    // the GPU service will use a lower one (0).
    const int importance = 1;
    if (!shared_memory_guid.is_empty()) {
      pmd->CreateSharedMemoryOwnershipEdge(dump->guid(), shared_memory_guid,
                                           importance);
    } else {
      pmd->CreateSharedGlobalAllocatorDump(guid);
      pmd->AddOwnershipEdge(dump->guid(), guid, importance);
    }
  }

  return true;
}

#if defined(OS_ANDROID)
void DisplayResourceProvider::SendPromotionHints(
    const OverlayCandidateList::PromotionHintInfoMap& promotion_hints) {
  GLES2Interface* gl = ContextGL();
  if (!gl)
    return;

  for (const auto& id : wants_promotion_hints_set_) {
    auto it = resources_.find(id);
    if (it == resources_.end())
      continue;

    if (it->second.marked_for_deletion)
      continue;

    const ChildResource* resource = LockForRead(id);
    // TODO(ericrk): We should never fail LockForRead, but we appear to be
    // doing so on Android in rare cases. Handle this gracefully until a better
    // solution can be found. https://crbug.com/811858
    if (!resource)
      return;

    DCHECK(resource->transferable.wants_promotion_hint);

    // Insist that this is backed by a GPU texture.
    if (resource->is_gpu_resource_type()) {
      DCHECK(resource->gl_id);
      auto iter = promotion_hints.find(id);
      bool promotable = iter != promotion_hints.end();
      gl->OverlayPromotionHintCHROMIUM(resource->gl_id, promotable,
                                       promotable ? iter->second.x() : 0,
                                       promotable ? iter->second.y() : 0,
                                       promotable ? iter->second.width() : 0,
                                       promotable ? iter->second.height() : 0);
    }
    UnlockForRead(id);
  }
}

bool DisplayResourceProvider::IsBackedBySurfaceTexture(ResourceId id) {
  ChildResource* resource = GetResource(id);
  return resource->transferable.is_backed_by_surface_texture;
}

bool DisplayResourceProvider::WantsPromotionHintForTesting(ResourceId id) {
  return wants_promotion_hints_set_.count(id) > 0;
}

size_t DisplayResourceProvider::CountPromotionHintRequestsForTesting() {
  return wants_promotion_hints_set_.size();
}
#endif

bool DisplayResourceProvider::IsOverlayCandidate(ResourceId id) {
  ChildResource* resource = TryGetResource(id);
  // TODO(ericrk): We should never fail TryGetResource, but we appear to
  // be doing so on Android in rare cases. Handle this gracefully until a
  // better solution can be found. https://crbug.com/811858
  return resource && resource->transferable.is_overlay_candidate;
}

bool DisplayResourceProvider::IsResourceSoftwareBacked(ResourceId id) {
  return GetResource(id)->transferable.is_software;
}

GLenum DisplayResourceProvider::GetResourceTextureTarget(ResourceId id) {
  return GetResource(id)->transferable.mailbox_holder.texture_target;
}

gfx::BufferFormat DisplayResourceProvider::GetBufferFormat(ResourceId id) {
  ChildResource* resource = GetResource(id);
  return BufferFormat(resource->transferable.format);
}

void DisplayResourceProvider::WaitSyncToken(ResourceId id) {
  ChildResource* resource = TryGetResource(id);
  // TODO(ericrk): We should never fail TryGetResource, but we appear to
  // be doing so on Android in rare cases. Handle this gracefully until a
  // better solution can be found. https://crbug.com/811858
  if (!resource)
    return;
  WaitSyncTokenInternal(resource);
#if defined(OS_ANDROID)
  // Now that the resource is synced, we may send it a promotion hint.  We could
  // sync all |wants_promotion_hint| resources elsewhere, and send 'no' to all
  // resources that weren't used.  However, there's no real advantage.
  if (resource->transferable.wants_promotion_hint)
    wants_promotion_hints_set_.insert(id);
#endif  // OS_ANDROID
}

int DisplayResourceProvider::CreateChild(const ReturnCallback& return_callback,
                                         bool needs_sync_tokens) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  Child child_info;
  child_info.return_callback = return_callback;
  child_info.needs_sync_tokens = needs_sync_tokens;
  int child = next_child_++;
  children_[child] = child_info;
  return child;
}

void DisplayResourceProvider::DestroyChild(int child_id) {
  auto it = children_.find(child_id);
  DCHECK(it != children_.end());
  DestroyChildInternal(it, NORMAL);
}

void DisplayResourceProvider::ReceiveFromChild(
    int child_id,
    const std::vector<TransferableResource>& resources) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // TODO(crbug.com/855785): Fishing for misuse of DisplayResourceProvider
  // causing crashes.
  CHECK(child_id);
  auto child_it = children_.find(child_id);
  // TODO(crbug.com/855785): Fishing for misuse of DisplayResourceProvider
  // causing crashes.
  CHECK(child_it != children_.end());
  Child& child_info = child_it->second;
  DCHECK(!child_info.marked_for_deletion);
  for (auto it = resources.begin(); it != resources.end(); ++it) {
    auto resource_in_map_it = child_info.child_to_parent_map.find(it->id);
    if (resource_in_map_it != child_info.child_to_parent_map.end()) {
      ChildResource* resource = GetResource(resource_in_map_it->second);
      resource->marked_for_deletion = false;
      resource->imported_count++;
      continue;
    }

    if (it->is_software != IsSoftware() ||
        it->mailbox_holder.mailbox.IsZero()) {
      TRACE_EVENT0(
          "viz", "DisplayResourceProvider::ReceiveFromChild dropping invalid");
      std::vector<ReturnedResource> to_return;
      to_return.push_back(it->ToReturnedResource());
      child_info.return_callback.Run(to_return);
      continue;
    }

    ResourceId local_id = next_id_++;
    if (it->is_software) {
      DCHECK(IsBitmapFormatSupported(it->format));
      InsertResource(local_id, ChildResource(child_id, *it));
    } else {
      InsertResource(local_id, ChildResource(child_id, *it));
    }
    child_info.child_to_parent_map[it->id] = local_id;
  }
}

void DisplayResourceProvider::DeclareUsedResourcesFromChild(
    int child,
    const ResourceIdSet& resources_from_child) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // TODO(crbug.com/855785): Fishing for misuse of DisplayResourceProvider
  // causing crashes.
  CHECK(child);
  auto child_it = children_.find(child);
  // TODO(crbug.com/855785): Fishing for misuse of DisplayResourceProvider
  // causing crashes.
  CHECK(child_it != children_.end());
  Child& child_info = child_it->second;
  DCHECK(!child_info.marked_for_deletion);

  std::vector<ResourceId> unused;
  for (auto it = child_info.child_to_parent_map.begin();
       it != child_info.child_to_parent_map.end(); ++it) {
    ResourceId local_id = it->second;
    bool resource_is_in_use = resources_from_child.count(it->first) > 0;
    if (!resource_is_in_use)
      unused.push_back(local_id);
  }
  DeleteAndReturnUnusedResourcesToChild(child_it, NORMAL, unused);
}

const std::unordered_map<ResourceId, ResourceId>&
DisplayResourceProvider::GetChildToParentMap(int child) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto it = children_.find(child);
  DCHECK(it != children_.end());
  DCHECK(!it->second.marked_for_deletion);
  return it->second.child_to_parent_map;
}

bool DisplayResourceProvider::InUse(ResourceId id) {
  ChildResource* resource = GetResource(id);
  return resource->lock_for_read_count > 0 || resource->locked_for_external_use;
}

DisplayResourceProvider::ChildResource* DisplayResourceProvider::InsertResource(
    ResourceId id,
    ChildResource resource) {
  auto result =
      resources_.insert(ResourceMap::value_type(id, std::move(resource)));
  DCHECK(result.second);
  return &result.first->second;
}

DisplayResourceProvider::ChildResource* DisplayResourceProvider::GetResource(
    ResourceId id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(id);
  auto it = resources_.find(id);
  DCHECK(it != resources_.end());
  return &it->second;
}

DisplayResourceProvider::ChildResource* DisplayResourceProvider::TryGetResource(
    ResourceId id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!id)
    return nullptr;
  auto it = resources_.find(id);
  if (it == resources_.end())
    return nullptr;
  return &it->second;
}

void DisplayResourceProvider::PopulateSkBitmapWithResource(
    SkBitmap* sk_bitmap,
    const ChildResource* resource) {
  DCHECK(IsBitmapFormatSupported(resource->transferable.format));
  SkImageInfo info =
      SkImageInfo::MakeN32Premul(resource->transferable.size.width(),
                                 resource->transferable.size.height());
  bool pixels_installed = sk_bitmap->installPixels(
      info, resource->shared_bitmap->pixels(), info.minRowBytes());
  DCHECK(pixels_installed);
}

void DisplayResourceProvider::DeleteResourceInternal(ResourceMap::iterator it,
                                                     DeleteStyle style) {
  TRACE_EVENT0("viz", "DosplayResourceProvider::DeleteResourceInternal");
  ChildResource* resource = &it->second;

  if (resource->gl_id) {
    GLES2Interface* gl = ContextGL();
    DCHECK(gl);
    gl->DeleteTextures(1, &resource->gl_id);
  }

  resources_.erase(it);
}

void DisplayResourceProvider::WaitSyncTokenInternal(ChildResource* resource) {
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

GLES2Interface* DisplayResourceProvider::ContextGL() const {
  ContextProvider* context_provider = compositor_context_provider_;
  return context_provider ? context_provider->ContextGL() : nullptr;
}

const DisplayResourceProvider::ChildResource*
DisplayResourceProvider::LockForRead(ResourceId id) {
  // TODO(ericrk): We should never fail TryGetResource, but we appear to be
  // doing so on Android in rare cases. Handle this gracefully until a better
  // solution can be found. https://crbug.com/811858
  ChildResource* resource = TryGetResource(id);
  if (!resource)
    return nullptr;

  // Mailbox sync_tokens must be processed by a call to WaitSyncToken() prior to
  // calling LockForRead().
  DCHECK_NE(NEEDS_WAIT, resource->synchronization_state());

  if (resource->is_gpu_resource_type() && !resource->gl_id) {
    GLES2Interface* gl = ContextGL();
    DCHECK(gl);
    resource->gl_id = gl->CreateAndConsumeTextureCHROMIUM(
        resource->transferable.mailbox_holder.mailbox.name);
    resource->SetLocallyUsed();
  }

  if (!resource->shared_bitmap && !resource->is_gpu_resource_type()) {
    const SharedBitmapId& shared_bitmap_id =
        resource->transferable.mailbox_holder.mailbox;
    std::unique_ptr<SharedBitmap> bitmap =
        shared_bitmap_manager_->GetSharedBitmapFromId(
            resource->transferable.size, resource->transferable.format,
            shared_bitmap_id);
    if (bitmap) {
      resource->shared_bitmap = std::move(bitmap);
      resource->shared_bitmap_tracing_guid =
          shared_bitmap_manager_->GetSharedBitmapTracingGUIDFromId(
              shared_bitmap_id);
    }
  }

  resource->lock_for_read_count++;
  if (resource->transferable.read_lock_fences_enabled) {
    if (current_read_lock_fence_.get())
      current_read_lock_fence_->Set();
    resource->read_lock_fence = current_read_lock_fence_;
  }

  return resource;
}

void DisplayResourceProvider::UnlockForRead(ResourceId id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto it = resources_.find(id);
  // TODO(ericrk): We should never fail to find id, but we appear to be
  // doing so on Android in rare cases. Handle this gracefully until a better
  // solution can be found. https://crbug.com/811858
  if (it == resources_.end())
    return;

  ChildResource* resource = &it->second;
  DCHECK_GT(resource->lock_for_read_count, 0);
  resource->lock_for_read_count--;
  TryReleaseResource(it);
}

ResourceMetadata DisplayResourceProvider::LockForExternalUse(ResourceId id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto it = resources_.find(id);
  DCHECK(it != resources_.end());

  ChildResource* resource = &it->second;
  ResourceMetadata metadata;
  // Make sure there is no outstanding LockForExternalUse without calling
  // UnlockForExternalUse.
  DCHECK(!resource->locked_for_external_use);
  // TODO(penghuang): support software resource.
  DCHECK(resource->is_gpu_resource_type());

  metadata.mailbox = resource->transferable.mailbox_holder.mailbox;
  metadata.backend_format = GrBackendFormat::MakeGL(
      TextureStorageFormat(resource->transferable.format),
      resource->transferable.mailbox_holder.texture_target);
  metadata.size = resource->transferable.size;
  metadata.mip_mapped = GrMipMapped::kNo;
  metadata.origin = kTopLeft_GrSurfaceOrigin;
  metadata.color_type = ResourceFormatToClosestSkColorType(
      !IsSoftware(), resource->transferable.format);
  metadata.alpha_type = kPremul_SkAlphaType;
  metadata.color_space = nullptr;
  metadata.sync_token = resource->sync_token();

  resource->locked_for_external_use = true;
  return metadata;
}

void DisplayResourceProvider::UnlockForExternalUse(
    ResourceId id,
    const gpu::SyncToken& sync_token) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto it = resources_.find(id);
  DCHECK(it != resources_.end());
  DCHECK(sync_token.verified_flush());

  ChildResource* resource = &it->second;
  DCHECK(resource->locked_for_external_use);
  // TODO(penghuang): support software resource.
  DCHECK(resource->is_gpu_resource_type());

  // Update the resource sync token to |sync_token|. When the next frame is
  // being composited, the DeclareUsedResourcesFromChild() will be called with
  // resources belong to every child for the next frame. If the resource is not
  // used by the next frame, the resource will be returned to a child which
  // owns it with the |sync_token|. The child is responsible for issuing a
  // WaitSyncToken GL command with the |sync_token| before reusing it.
  resource->UpdateSyncToken(sync_token);
  resource->locked_for_external_use = false;

  TryReleaseResource(it);
}

void DisplayResourceProvider::TryReleaseResource(ResourceMap::iterator it) {
  ResourceId id = it->first;
  ChildResource* resource = &it->second;
  if (resource->marked_for_deletion && !resource->lock_for_read_count &&
      !resource->locked_for_external_use) {
    auto child_it = children_.find(resource->child_id);
    DeleteAndReturnUnusedResourcesToChild(child_it, NORMAL, {id});
  }
}

GLenum DisplayResourceProvider::BindForSampling(ResourceId resource_id,
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
  if (filter != resource->filter) {
    gl->TexParameteri(target, GL_TEXTURE_MIN_FILTER, filter);
    gl->TexParameteri(target, GL_TEXTURE_MAG_FILTER, filter);
    resource->filter = filter;
  }

  return target;
}

bool DisplayResourceProvider::ReadLockFenceHasPassed(
    const ChildResource* resource) {
  return !resource->read_lock_fence || resource->read_lock_fence->HasPassed();
}

#if defined(OS_ANDROID)
void DisplayResourceProvider::DeletePromotionHint(ResourceMap::iterator it,
                                                  DeleteStyle style) {
  ChildResource* resource = &it->second;
  // If this resource was interested in promotion hints, then remove it from
  // the set of resources that we'll notify.
  if (resource->transferable.wants_promotion_hint)
    wants_promotion_hints_set_.erase(it->first);
}
#endif

void DisplayResourceProvider::DeleteAndReturnUnusedResourcesToChild(
    ChildMap::iterator child_it,
    DeleteStyle style,
    const std::vector<ResourceId>& unused) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(child_it != children_.end());
  Child* child_info = &child_it->second;

  // No work is done in this case.
  if (unused.empty() && !child_info->marked_for_deletion)
    return;

  // Store unused resources while batching is enabled.
  if (batch_return_resources_lock_count_ > 0) {
    int child_id = child_it->first;
    // Ensure that we have an entry in |batched_returning_resources_| for child
    // even if |unused| is empty, in case child is marked for deletion.
    // Note: emplace() does not overwrite an entry if already present.
    batched_returning_resources_.emplace(child_id, std::vector<ResourceId>());
    auto& child_resources = batched_returning_resources_[child_id];
    child_resources.reserve(child_resources.size() + unused.size());
    child_resources.insert(child_resources.end(), unused.begin(), unused.end());
    return;
  }

  std::vector<ReturnedResource> to_return;
  to_return.reserve(unused.size());
  std::vector<ReturnedResource*> need_synchronization_resources;
  std::vector<GLbyte*> unverified_sync_tokens;
  std::vector<size_t> to_return_indices_unverified;

  GLES2Interface* gl = ContextGL();

  for (ResourceId local_id : unused) {
    auto it = resources_.find(local_id);
    CHECK(it != resources_.end());

    resource_sk_image_.erase(local_id);

    ChildResource& resource = it->second;

    ResourceId child_id = resource.transferable.id;
    DCHECK(child_info->child_to_parent_map.count(child_id));

    bool is_lost = (resource.is_gpu_resource_type() && lost_context_provider_);
    if (resource.lock_for_read_count > 0 || resource.locked_for_external_use) {
      if (style != FOR_SHUTDOWN) {
        // Defer this resource deletion.
        resource.marked_for_deletion = true;
        continue;
      }
      // We can't postpone the deletion, so we'll have to lose it.
      is_lost = true;
    } else if (!ReadLockFenceHasPassed(&resource)) {
      // TODO(dcastagna): see if it's possible to use this logic for
      // the branch above too, where the resource is locked or still exported.
      if (style != FOR_SHUTDOWN && !child_info->marked_for_deletion) {
        // Defer this resource deletion.
        resource.marked_for_deletion = true;
        continue;
      }
      // We can't postpone the deletion, so we'll have to lose it.
      is_lost = true;
    }

    if (resource.is_gpu_resource_type() &&
        resource.filter != resource.transferable.filter) {
      DCHECK(resource.transferable.mailbox_holder.texture_target);
      DCHECK(resource.gl_id);
      DCHECK(gl);
      gl->BindTexture(resource.transferable.mailbox_holder.texture_target,
                      resource.gl_id);
      gl->TexParameteri(resource.transferable.mailbox_holder.texture_target,
                        GL_TEXTURE_MIN_FILTER, resource.transferable.filter);
      gl->TexParameteri(resource.transferable.mailbox_holder.texture_target,
                        GL_TEXTURE_MAG_FILTER, resource.transferable.filter);
      resource.SetLocallyUsed();
    }

    ReturnedResource returned;
    returned.id = child_id;
    returned.sync_token = resource.sync_token();
    returned.count = resource.imported_count;
    returned.lost = is_lost;
    to_return.push_back(returned);

    if (resource.is_gpu_resource_type() && child_info->needs_sync_tokens) {
      if (resource.needs_sync_token()) {
        need_synchronization_resources.push_back(&to_return.back());
      } else if (returned.sync_token.HasData() &&
                 !returned.sync_token.verified_flush()) {
        // Before returning any sync tokens, they must be verified. Store an
        // index into |to_return| instead of a pointer as vectors may realloc
        // and move their data.
        to_return_indices_unverified.push_back(to_return.size() - 1);
      }
    }

    child_info->child_to_parent_map.erase(child_id);
    resource.imported_count = 0;
#if defined(OS_ANDROID)
    DeletePromotionHint(it, style);
#endif
    DeleteResourceInternal(it, style);
  }

  for (size_t i : to_return_indices_unverified)
    unverified_sync_tokens.push_back(to_return[i].sync_token.GetData());

  gpu::SyncToken new_sync_token;
  if (!need_synchronization_resources.empty()) {
    DCHECK(child_info->needs_sync_tokens);
    DCHECK(gl);
    gl->GenUnverifiedSyncTokenCHROMIUM(new_sync_token.GetData());
    unverified_sync_tokens.push_back(new_sync_token.GetData());
  }

  if (!unverified_sync_tokens.empty()) {
    DCHECK(child_info->needs_sync_tokens);
    DCHECK(gl);
    gl->VerifySyncTokensCHROMIUM(unverified_sync_tokens.data(),
                                 unverified_sync_tokens.size());
  }

  // Set sync token after verification.
  for (ReturnedResource* returned : need_synchronization_resources)
    returned->sync_token = new_sync_token;

  if (!to_return.empty())
    child_info->return_callback.Run(to_return);

  if (child_info->marked_for_deletion &&
      child_info->child_to_parent_map.empty()) {
    children_.erase(child_it);
  }
}

void DisplayResourceProvider::DestroyChildInternal(ChildMap::iterator it,
                                                   DeleteStyle style) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  Child& child = it->second;
  DCHECK(style == FOR_SHUTDOWN || !child.marked_for_deletion);

  std::vector<ResourceId> resources_for_child;

  for (auto child_it = child.child_to_parent_map.begin();
       child_it != child.child_to_parent_map.end(); ++child_it) {
    ResourceId id = child_it->second;
    resources_for_child.push_back(id);
  }

  child.marked_for_deletion = true;

  DeleteAndReturnUnusedResourcesToChild(it, style, resources_for_child);
}

void DisplayResourceProvider::SetBatchReturnResources(bool batch) {
  if (batch) {
    DCHECK_GE(batch_return_resources_lock_count_, 0);
    batch_return_resources_lock_count_++;
  } else {
    DCHECK_GT(batch_return_resources_lock_count_, 0);
    batch_return_resources_lock_count_--;
    if (batch_return_resources_lock_count_ == 0) {
      for (auto& child_resources_kv : batched_returning_resources_) {
        auto child_it = children_.find(child_resources_kv.first);

        // Remove duplicates from child's unused resources.  Duplicates are
        // possible when batching is enabled because resources are saved in
        // |batched_returning_resources_| for removal, and not removed from the
        // child's |child_to_parent_map|, so the same set of resources can be
        // saved again using DeclareUsedResourcesForChild() or DestroyChild().
        auto& unused_resources = child_resources_kv.second;
        std::sort(unused_resources.begin(), unused_resources.end());
        auto last =
            std::unique(unused_resources.begin(), unused_resources.end());
        unused_resources.erase(last, unused_resources.end());

        DeleteAndReturnUnusedResourcesToChild(child_it, NORMAL,
                                              unused_resources);
      }
      batched_returning_resources_.clear();
    }
  }
}

DisplayResourceProvider::ScopedReadLockGL::ScopedReadLockGL(
    DisplayResourceProvider* resource_provider,
    ResourceId resource_id)
    : resource_provider_(resource_provider), resource_id_(resource_id) {
  const ChildResource* resource = resource_provider->LockForRead(resource_id);
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

DisplayResourceProvider::ScopedReadLockGL::~ScopedReadLockGL() {
  resource_provider_->UnlockForRead(resource_id_);
}

DisplayResourceProvider::ScopedSamplerGL::ScopedSamplerGL(
    DisplayResourceProvider* resource_provider,
    ResourceId resource_id,
    GLenum filter)
    : resource_lock_(resource_provider, resource_id),
      unit_(GL_TEXTURE0),
      target_(resource_provider->BindForSampling(resource_id, unit_, filter)) {}

DisplayResourceProvider::ScopedSamplerGL::ScopedSamplerGL(
    DisplayResourceProvider* resource_provider,
    ResourceId resource_id,
    GLenum unit,
    GLenum filter)
    : resource_lock_(resource_provider, resource_id),
      unit_(unit),
      target_(resource_provider->BindForSampling(resource_id, unit_, filter)) {}

DisplayResourceProvider::ScopedSamplerGL::~ScopedSamplerGL() = default;

DisplayResourceProvider::ScopedReadLockSkImage::ScopedReadLockSkImage(
    DisplayResourceProvider* resource_provider,
    ResourceId resource_id)
    : resource_provider_(resource_provider), resource_id_(resource_id) {
  const ChildResource* resource = resource_provider->LockForRead(resource_id);
  DCHECK(resource);

  // Use cached SkImage if possible.
  auto it = resource_provider_->resource_sk_image_.find(resource_id);
  if (it != resource_provider_->resource_sk_image_.end()) {
    sk_image_ = it->second;
    return;
  }

  if (resource->is_gpu_resource_type()) {
    DCHECK(resource->gl_id);
    GrGLTextureInfo texture_info;
    texture_info.fID = resource->gl_id;
    texture_info.fTarget = resource->transferable.mailbox_holder.texture_target;
    texture_info.fFormat = TextureStorageFormat(resource->transferable.format);
    GrBackendTexture backend_texture(resource->transferable.size.width(),
                                     resource->transferable.size.height(),
                                     GrMipMapped::kNo, texture_info);
    sk_image_ = SkImage::MakeFromTexture(
        resource_provider->compositor_context_provider_->GrContext(),
        backend_texture, kTopLeft_GrSurfaceOrigin,
        ResourceFormatToClosestSkColorType(!resource_provider->IsSoftware(),
                                           resource->transferable.format),
        kPremul_SkAlphaType, nullptr);
    resource_provider_->resource_sk_image_[resource_id] = sk_image_;
    return;
  }

  if (!resource->shared_bitmap) {
    // If a CompositorFrameSink is destroyed, it destroys all SharedBitmapIds
    // that it registered. In this case, a CompositorFrame can be drawn with
    // SharedBitmapIds that are not known in the viz service. As well, a
    // misbehaved client can use SharedBitampIds that it did not report to
    // the service. Then the |shared_bitmap| will be null, and this read lock
    // will not be valid. Software-compositing users of this read lock must
    // check for valid() to deal with this scenario.
    sk_image_ = nullptr;
    return;
  }

  SkBitmap sk_bitmap;
  resource_provider->PopulateSkBitmapWithResource(&sk_bitmap, resource);
  sk_bitmap.setImmutable();
  sk_image_ = SkImage::MakeFromBitmap(sk_bitmap);
  resource_provider_->resource_sk_image_[resource_id] = sk_image_;
}

DisplayResourceProvider::ScopedReadLockSkImage::~ScopedReadLockSkImage() {
  resource_provider_->UnlockForRead(resource_id_);
}

DisplayResourceProvider::LockSetForExternalUse::LockSetForExternalUse(
    DisplayResourceProvider* resource_provider)
    : resource_provider_(resource_provider) {}

DisplayResourceProvider::LockSetForExternalUse::~LockSetForExternalUse() {
  DCHECK(resources_.empty());
}

ResourceMetadata DisplayResourceProvider::LockSetForExternalUse::LockResource(
    ResourceId id) {
  DCHECK(!base::ContainsValue(resources_, id));
  resources_.push_back(id);
  return resource_provider_->LockForExternalUse(id);
}

void DisplayResourceProvider::LockSetForExternalUse::UnlockResources(
    const gpu::SyncToken& sync_token) {
  for (const auto& id : resources_)
    resource_provider_->UnlockForExternalUse(id, sync_token);
  resources_.clear();
}

DisplayResourceProvider::SynchronousFence::SynchronousFence(
    gpu::gles2::GLES2Interface* gl)
    : gl_(gl), has_synchronized_(true) {}

DisplayResourceProvider::SynchronousFence::~SynchronousFence() = default;

void DisplayResourceProvider::SynchronousFence::Set() {
  has_synchronized_ = false;
}

bool DisplayResourceProvider::SynchronousFence::HasPassed() {
  if (!has_synchronized_) {
    has_synchronized_ = true;
    Synchronize();
  }
  return true;
}

void DisplayResourceProvider::SynchronousFence::Wait() {
  HasPassed();
}

void DisplayResourceProvider::SynchronousFence::Synchronize() {
  TRACE_EVENT0("viz", "DisplayResourceProvider::SynchronousFence::Synchronize");
  gl_->Finish();
}

DisplayResourceProvider::ScopedBatchReturnResources::ScopedBatchReturnResources(
    DisplayResourceProvider* resource_provider)
    : resource_provider_(resource_provider) {
  resource_provider_->SetBatchReturnResources(true);
}

DisplayResourceProvider::ScopedBatchReturnResources::
    ~ScopedBatchReturnResources() {
  resource_provider_->SetBatchReturnResources(false);
}

DisplayResourceProvider::Child::Child() = default;
DisplayResourceProvider::Child::Child(const Child& other) = default;
DisplayResourceProvider::Child::~Child() = default;

DisplayResourceProvider::ChildResource::ChildResource(
    int child_id,
    const TransferableResource& transferable)
    : child_id(child_id),
      transferable(transferable),
      filter(transferable.filter) {
  if (is_gpu_resource_type())
    UpdateSyncToken(transferable.mailbox_holder.sync_token);
  else
    SetSynchronized();
}

DisplayResourceProvider::ChildResource::ChildResource(ChildResource&& other) =
    default;
DisplayResourceProvider::ChildResource::~ChildResource() = default;

void DisplayResourceProvider::ChildResource::SetLocallyUsed() {
  synchronization_state_ = LOCALLY_USED;
  sync_token_.Clear();
}

void DisplayResourceProvider::ChildResource::SetSynchronized() {
  synchronization_state_ = SYNCHRONIZED;
}

void DisplayResourceProvider::ChildResource::UpdateSyncToken(
    const gpu::SyncToken& sync_token) {
  DCHECK(is_gpu_resource_type());
  // An empty sync token may be used if commands are guaranteed to have run on
  // the gpu process or in case of context loss.
  sync_token_ = sync_token;
  synchronization_state_ = sync_token.HasData() ? NEEDS_WAIT : SYNCHRONIZED;
}

}  // namespace viz
