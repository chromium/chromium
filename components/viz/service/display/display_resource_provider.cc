// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/display_resource_provider.h"

#include <algorithm>
#include <string>

#include "base/atomic_sequence_num.h"
#include "base/not_fatal_until.h"
#include "base/notreached.h"
#include "base/numerics/safe_math.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/viz/common/resources/resource_sizes.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/shared_image_trace_utils.h"
#include "gpu/command_buffer/service/shared_image/shared_image_format_service_utils.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/trace_util.h"

namespace viz {

namespace {

// Generates process-unique IDs to use for tracing resources.
base::AtomicSequenceNumber g_next_display_resource_provider_tracing_id;

}  // namespace

DisplayResourceProvider::DisplayResourceProvider(Mode mode)
    : mode_(mode),
      tracing_id_(g_next_display_resource_provider_tracing_id.GetNext()) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // In certain cases, SingleThreadTaskRunner::CurrentDefaultHandle isn't set
  // (Android Webview).  Don't register a dump provider in these cases.
  // TODO(crbug.com/40430067): Get this working in Android Webview.
  if (base::SingleThreadTaskRunner::HasCurrentDefault()) {
    base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
        this, "cc::ResourceProvider",
        base::SingleThreadTaskRunner::GetCurrentDefault());
  }
}

DisplayResourceProvider::~DisplayResourceProvider() {
  DCHECK(children_.empty()) << "Destroy() must be called before dtor";

  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);
}

void DisplayResourceProvider::Destroy() {
  while (!children_.empty())
    DestroyChildInternal(children_.begin(), FOR_SHUTDOWN);
}

bool DisplayResourceProvider::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (const auto& resource_entry : resources_) {
    const auto& resource = resource_entry.second;

    bool backing_memory_allocated = false;
    if (resource.transferable.is_software)
      backing_memory_allocated = !!resource.shared_bitmap;
    else
      backing_memory_allocated = !!resource.image_context;

    if (!backing_memory_allocated) {
      // Don't log unallocated resources - they have no backing memory.
      continue;
    }

    // ResourceIds are not process-unique, so log with the ResourceProvider's
    // unique id.
    std::string dump_name =
        base::StringPrintf("cc/resource_memory/provider_%d/resource_%u",
                           tracing_id_, resource_entry.first.GetUnsafeValue());
    base::trace_event::MemoryAllocatorDump* dump =
        pmd->CreateAllocatorDump(dump_name);

    // Texture resources may not come with a size, in which case don't report
    // one.
    if (!resource.transferable.size.IsEmpty()) {
      uint64_t total_bytes = resource.transferable.format.EstimatedSizeInBytes(
          resource.transferable.size);
      dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                      base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                      static_cast<uint64_t>(total_bytes));
    }

    // Resources may be shared across processes and require a shared GUID to
    // prevent double counting the memory.
    //
    // The client that owns the resource will use a higher importance, and the
    // GPU service will use a lower one.
    constexpr int kImportance =
        static_cast<int>(gpu::TracingImportance::kServiceOwner);
    if (resource.transferable.is_software) {
      pmd->CreateSharedMemoryOwnershipEdge(
          dump->guid(), resource.shared_bitmap_tracing_guid, kImportance);
    } else {
      auto guid = GetSharedImageGUIDForTracing(resource.transferable.mailbox());
      pmd->CreateSharedGlobalAllocatorDump(guid);
      pmd->AddOwnershipEdge(dump->guid(), guid, kImportance);
    }
  }

  return true;
}

base::WeakPtr<DisplayResourceProvider> DisplayResourceProvider::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

#if BUILDFLAG(IS_ANDROID)
bool DisplayResourceProvider::IsBackedBySurfaceTexture(ResourceId id) const {
  const ChildResource* resource = GetResource(id);
  return resource->transferable.is_backed_by_surface_texture;
}
#endif

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_WIN)
bool DisplayResourceProvider::DoesResourceWantPromotionHint(
    ResourceId id) const {
  const ChildResource* resource = TryGetResource(id);
  // TODO(ericrk): We should never fail TryGetResource, but we appear to
  // be doing so on Android in rare cases. Handle this gracefully until a
  // better solution can be found. https://crbug.com/811858
  return resource && resource->transferable.wants_promotion_hint;
}
#endif

bool DisplayResourceProvider::IsOverlayCandidate(ResourceId id) const {
  const ChildResource* resource = TryGetResource(id);
  // TODO(ericrk): We should never fail TryGetResource, but we appear to
  // be doing so on Android in rare cases. Handle this gracefully until a
  // better solution can be found. https://crbug.com/811858
  return resource && resource->transferable.is_overlay_candidate;
}

SurfaceId DisplayResourceProvider::GetSurfaceId(ResourceId id) const {
  const ChildResource* resource = GetResource(id);
  return children_.contains(resource->child_id)
             ? children_.at(resource->child_id).surface_id
             : SurfaceId();
}

int DisplayResourceProvider::GetChildId(ResourceId id) const {
  const ChildResource* resource = GetResource(id);
  return resource->child_id;
}

bool DisplayResourceProvider::IsResourceSoftwareBacked(ResourceId id) const {
  return GetResource(id)->transferable.is_software;
}

const gfx::Size DisplayResourceProvider::GetResourceBackedSize(
    ResourceId id) const {
  return GetResource(id)->transferable.size;
}

SharedImageFormat DisplayResourceProvider::GetSharedImageFormat(
    ResourceId id) const {
  const ChildResource* resource = GetResource(id);
  return resource->transferable.format;
}

const gfx::ColorSpace& DisplayResourceProvider::GetColorSpace(
    ResourceId id) const {
  const ChildResource* resource = GetResource(id);
  return resource->transferable.color_space;
}

bool DisplayResourceProvider::GetNeedsDetiling(ResourceId id) const {
  const ChildResource* resource = GetResource(id);
  return resource->transferable.needs_detiling;
}

const gfx::HDRMetadata& DisplayResourceProvider::GetHDRMetadata(
    ResourceId id) const {
  const ChildResource* resource = GetResource(id);
  return resource->transferable.hdr_metadata;
}

int DisplayResourceProvider::CreateChild(ReturnCallback return_callback,
                                         const SurfaceId& surface_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  int child_id = next_child_++;
  Child& child = children_[child_id];
  child.id = child_id;
  child.return_callback = std::move(return_callback);
  child.surface_id = surface_id;

  return child_id;
}

void DisplayResourceProvider::DestroyChild(int child_id) {
  auto it = children_.find(child_id);
  CHECK(it != children_.end(), base::NotFatalUntil::M130);
  DestroyChildInternal(it, NORMAL);
}

void DisplayResourceProvider::ReceiveFromChild(
    int child_id,
    const std::vector<TransferableResource>& resources) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto child_it = children_.find(child_id);
  DCHECK(child_it != children_.end());

  Child& child_info = child_it->second;
  DCHECK(!child_info.marked_for_deletion);
  for (const TransferableResource& transferable_resource : resources) {
    auto resource_in_map_it =
        child_info.child_to_parent_map.find(transferable_resource.id);
    if (resource_in_map_it != child_info.child_to_parent_map.end()) {
      ChildResource* resource = GetResource(resource_in_map_it->second);
      resource->marked_for_deletion = false;
      resource->imported_count++;
      continue;
    }

    if (transferable_resource.is_software != IsSoftware() ||
        transferable_resource.is_empty()) {
      TRACE_EVENT0(
          "viz", "DisplayResourceProvider::ReceiveFromChild dropping invalid");
      std::vector<ReturnedResource> returned;
      returned.push_back(transferable_resource.ToReturnedResource());
      child_info.return_callback.Run(std::move(returned));
      continue;
    }

    ResourceId local_id = resource_id_generator_.GenerateNextId();

    // If using legacy shared bitmaps, verify that the format is supported.
    DCHECK(!transferable_resource.is_software ||
           transferable_resource.IsSoftwareSharedImage() ||
           (!transferable_resource.IsSoftwareSharedImage() &&
            transferable_resource.format.IsBitmapFormatSupported()));
    resources_.emplace(local_id,
                       ChildResource(child_id, transferable_resource));
    child_info.child_to_parent_map[transferable_resource.id] = local_id;
  }
}

void DisplayResourceProvider::DeclareUsedResourcesFromChild(
    int child,
    const ResourceIdSet& resources_from_child) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto child_it = children_.find(child);
  DCHECK(child_it != children_.end());

  Child& child_info = child_it->second;
  DCHECK(!child_info.marked_for_deletion);

  std::vector<ResourceId> unused;
  for (auto& entry : child_info.child_to_parent_map) {
    ResourceId local_id = entry.second;
    bool resource_is_in_use = resources_from_child.count(entry.first) > 0;
    if (!resource_is_in_use)
      unused.push_back(local_id);
  }
  DeleteAndReturnUnusedResourcesToChild(child_it, NORMAL, unused);
}

gpu::Mailbox DisplayResourceProvider::GetMailbox(ResourceId resource_id) const {
  const ChildResource* resource = TryGetResource(resource_id);
  if (!resource)
    return gpu::Mailbox();
  return resource->transferable.mailbox();
}

const std::unordered_map<ResourceId, ResourceId, ResourceIdHasher>&
DisplayResourceProvider::GetChildToParentMap(int child) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = children_.find(child);
  CHECK(it != children_.end(), base::NotFatalUntil::M130);
  DCHECK(!it->second.marked_for_deletion);
  return it->second.child_to_parent_map;
}

bool DisplayResourceProvider::InUse(ResourceId id) const {
  const ChildResource* resource = GetResource(id);
  return resource->InUse();
}

const DisplayResourceProvider::ChildResource*
DisplayResourceProvider::GetResource(ResourceId id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(id);
  auto it = resources_.find(id);
  CHECK(it != resources_.end(), base::NotFatalUntil::M130);
  return &it->second;
}

DisplayResourceProvider::ChildResource* DisplayResourceProvider::GetResource(
    ResourceId id) {
  return const_cast<DisplayResourceProvider::ChildResource*>(
      const_cast<const DisplayResourceProvider*>(this)->GetResource(id));
}

const DisplayResourceProvider::ChildResource*
DisplayResourceProvider::TryGetResource(ResourceId id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!id)
    return nullptr;
  auto it = resources_.find(id);
  if (it == resources_.end())
    return nullptr;
  return &it->second;
}

DisplayResourceProvider::ChildResource* DisplayResourceProvider::TryGetResource(
    ResourceId id) {
  return const_cast<DisplayResourceProvider::ChildResource*>(
      const_cast<const DisplayResourceProvider*>(this)->TryGetResource(id));
}

void DisplayResourceProvider::OnResourceFencePassed(
    ResourceFence* resource_fence,
    base::flat_set<ResourceId> resources) {
  for (auto local_id : resources) {
    auto it = resources_.find(local_id);
    if (it == resources_.end() ||
        resource_fence != it->second.resource_fence.get()) {
      continue;
    }
    TryReleaseResource(local_id, &it->second);
  }
}

void DisplayResourceProvider::TryReleaseResource(ResourceId id,
                                                 ChildResource* resource) {
  if (resource->marked_for_deletion && !resource->InUse()) {
    auto child_it = children_.find(resource->child_id);
    DeleteAndReturnUnusedResourcesToChild(child_it, NORMAL, {id});
  }
}

bool DisplayResourceProvider::ResourceFenceHasPassed(
    const ChildResource* resource) const {
  return !resource->resource_fence || resource->resource_fence->HasPassed();
}

DisplayResourceProvider::CanDeleteNowResult
DisplayResourceProvider::CanDeleteNow(const Child& child_info,
                                      const ChildResource& resource,
                                      DeleteStyle style) const {
  if (resource.InUse()) {
    // We can't postpone the deletion, so we'll have to lose it.
    if (style == FOR_SHUTDOWN)
      return CanDeleteNowResult::kYesButLoseResource;

    // Defer this resource deletion.
    return CanDeleteNowResult::kNo;
  } else if (!ResourceFenceHasPassed(&resource)) {
    // TODO(dcastagna): see if it's possible to use this logic for
    // the branch above too, where the resource is locked or still exported.
    // We can't postpone the deletion, so we'll have to lose it.
    if (style == FOR_SHUTDOWN || child_info.marked_for_deletion)
      return CanDeleteNowResult::kYesButLoseResource;

    // Defer this resource deletion.
    return CanDeleteNowResult::kNo;
  }
  return CanDeleteNowResult::kYes;
}

void DisplayResourceProvider::DeleteAndReturnUnusedResourcesToChild(
    ChildMap::iterator child_it,
    DeleteStyle style,
    const std::vector<ResourceId>& unused) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(child_it != children_.end());
  Child& child_info = child_it->second;

  // No work is done in this case.
  if (unused.empty() && !child_info.marked_for_deletion)
    return;

  // Store unused resources while batching is enabled.
  if (batch_return_resources_lock_count_ > 0) {
    int child_id = child_it->first;
    auto& child_resources = batched_returning_resources_[child_id];
    child_resources.reserve(child_resources.size() + unused.size());
    child_resources.insert(child_resources.end(), unused.begin(), unused.end());
    return;
  }

  std::vector<ReturnedResource> to_return =
      DeleteAndReturnUnusedResourcesToChildImpl(child_info, style, unused);

  if (!to_return.empty())
    child_info.return_callback.Run(std::move(to_return));

  if (child_info.marked_for_deletion &&
      child_info.child_to_parent_map.empty()) {
    children_.erase(child_it);
  }
}

void DisplayResourceProvider::DestroyChildInternal(ChildMap::iterator it,
                                                   DeleteStyle style) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  Child& child = it->second;
  DCHECK(style == FOR_SHUTDOWN || !child.marked_for_deletion);

  std::vector<ResourceId> resources_for_child;
  for (auto& entry : child.child_to_parent_map) {
    resources_for_child.push_back(entry.second);
  }

  child.marked_for_deletion = true;

  DeleteAndReturnUnusedResourcesToChild(it, style, resources_for_child);
}

void DisplayResourceProvider::TryFlushBatchedResources() {
  if (batch_return_resources_lock_count_ == 0 && can_access_gpu_thread_) {
    for (auto& child_resources_kv : batched_returning_resources_) {
      auto child_it = children_.find(child_resources_kv.first);

      // Remove duplicates from child's unused resources.  Duplicates are
      // possible when batching is enabled because resources are saved in
      // |batched_returning_resources_| for removal, and not removed from the
      // child's |child_to_parent_map|, so the same set of resources can be
      // saved again using DeclareUsedResourcesForChild() or DestroyChild().
      auto& unused_resources = child_resources_kv.second;
      std::sort(unused_resources.begin(), unused_resources.end());
      auto last = std::unique(unused_resources.begin(), unused_resources.end());
      unused_resources.erase(last, unused_resources.end());

      DeleteAndReturnUnusedResourcesToChild(child_it, NORMAL, unused_resources);
    }
    batched_returning_resources_.clear();
  }
}

void DisplayResourceProvider::SetBatchReturnResources(bool batch) {
  if (batch) {
    DCHECK_GE(batch_return_resources_lock_count_, 0);
    batch_return_resources_lock_count_++;
  } else {
    DCHECK_GT(batch_return_resources_lock_count_, 0);
    batch_return_resources_lock_count_--;
    if (batch_return_resources_lock_count_ == 0) {
      TryFlushBatchedResources();
    }
  }
}

void DisplayResourceProvider::SetAllowAccessToGPUThread(bool allow) {
  can_access_gpu_thread_ = allow;
  if (allow) {
    TryFlushBatchedResources();
  }
}

DisplayResourceProvider::ScopedReadLockSharedImage::ScopedReadLockSharedImage(
    DisplayResourceProvider* resource_provider,
    ResourceId resource_id)
    : resource_provider_(resource_provider),
      resource_id_(resource_id),
      resource_(resource_provider_->GetResource(resource_id_)) {
  DCHECK(resource_);
  DCHECK(resource_->is_gpu_resource_type());
  resource_->lock_for_overlay_count++;
}

DisplayResourceProvider::ScopedReadLockSharedImage::ScopedReadLockSharedImage(
    ScopedReadLockSharedImage&& other) {
  *this = std::move(other);
}

DisplayResourceProvider::ScopedReadLockSharedImage::
    ~ScopedReadLockSharedImage() {
  Reset();
}

DisplayResourceProvider::ScopedReadLockSharedImage&
DisplayResourceProvider::ScopedReadLockSharedImage::operator=(
    ScopedReadLockSharedImage&& other) {
  Reset();
  resource_provider_ = other.resource_provider_;
  resource_id_ = other.resource_id_;
  resource_ = other.resource_;
  other.resource_provider_ = nullptr;
  other.resource_id_ = kInvalidResourceId;
  other.resource_ = nullptr;
  return *this;
}

void DisplayResourceProvider::ScopedReadLockSharedImage::SetReleaseFence(
    gfx::GpuFenceHandle release_fence) {
  DCHECK(resource_);
  resource_->release_fence = std::move(release_fence);
}

bool DisplayResourceProvider::ScopedReadLockSharedImage::HasReadLockFence()
    const {
  DCHECK(resource_);
  return resource_->transferable.synchronization_type ==
         TransferableResource::SynchronizationType::kGpuCommandsCompleted;
}

void DisplayResourceProvider::ScopedReadLockSharedImage::Reset() {
  if (!resource_provider_)
    return;
  DCHECK(resource_->lock_for_overlay_count);
  resource_->lock_for_overlay_count--;
  resource_provider_->TryReleaseResource(resource_id_, resource_);
  resource_provider_ = nullptr;
  resource_id_ = kInvalidResourceId;
}

DisplayResourceProvider::ScopedBatchReturnResources::ScopedBatchReturnResources(
    DisplayResourceProvider* resource_provider,
    bool allow_access_to_gpu_thread)
    : resource_provider_(resource_provider),
      was_access_to_gpu_thread_allowed_(
          resource_provider_->can_access_gpu_thread_) {
  resource_provider_->SetBatchReturnResources(true);
  if (allow_access_to_gpu_thread)
    resource_provider_->SetAllowAccessToGPUThread(true);
}

DisplayResourceProvider::ScopedBatchReturnResources::
    ~ScopedBatchReturnResources() {
  resource_provider_->SetBatchReturnResources(false);
  resource_provider_->SetAllowAccessToGPUThread(
      was_access_to_gpu_thread_allowed_);
}

DisplayResourceProvider::Child::Child() = default;
DisplayResourceProvider::Child::Child(Child&& other) = default;
DisplayResourceProvider::Child& DisplayResourceProvider::Child::operator=(
    Child&& other) = default;
DisplayResourceProvider::Child::~Child() = default;

DisplayResourceProvider::ChildResource::ChildResource(
    int child_id,
    const TransferableResource& transferable)
    : child_id(child_id), transferable(transferable) {
  if (is_gpu_resource_type())
    UpdateSyncToken(transferable.sync_token());
}

DisplayResourceProvider::ChildResource::ChildResource(ChildResource&& other) =
    default;
DisplayResourceProvider::ChildResource::~ChildResource() = default;

void DisplayResourceProvider::ChildResource::UpdateSyncToken(
    const gpu::SyncToken& sync_token) {
  DCHECK(is_gpu_resource_type());
  sync_token_ = sync_token;
}

}  // namespace viz
