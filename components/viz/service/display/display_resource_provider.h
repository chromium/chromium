// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_DISPLAY_RESOURCE_PROVIDER_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_DISPLAY_RESOURCE_PROVIDER_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/memory/stack_allocated.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/trace_event/memory_dump_provider.h"
#include "build/build_config.h"
#include "components/viz/common/resources/resource_id.h"
#include "components/viz/common/resources/return_callback.h"
#include "components/viz/common/resources/shared_bitmap.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "components/viz/service/display/external_use_client.h"
#include "components/viz/service/display/resource_fence.h"
#include "components/viz/service/viz_service_export.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"

namespace gfx {
class ColorSpace;
}  // namespace gfx

namespace viz {

class ScopedAllowGpuAccessForDisplayResourceProvider;

// This class provides abstractions for receiving and using resources from other
// modules/threads/processes. It abstracts away GL textures vs GpuMemoryBuffers
// vs software bitmaps behind a single ResourceId so that code in common can
// hold onto ResourceIds, as long as the code using them knows the correct type.
// It accepts as input TransferableResources which it holds internally, tracks
// state on, and exposes as a ResourceId.
//
// The resource's underlying type is accessed through locks that help to
// scope and safeguard correct usage with DCHECKs.
//
// This class is not thread-safe and can only be called from the thread it was
// created on.
class VIZ_SERVICE_EXPORT DisplayResourceProvider
    : public base::trace_event::MemoryDumpProvider {
 public:
  enum Mode {
    kGpu,
    kSoftware,
  };
  ~DisplayResourceProvider() override;

  DisplayResourceProvider(const DisplayResourceProvider&) = delete;
  DisplayResourceProvider& operator=(const DisplayResourceProvider&) = delete;

  bool IsSoftware() const { return mode_ == kSoftware; }
  size_t num_resources() const { return resources_.size(); }

  // base::trace_event::MemoryDumpProvider implementation.
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

  base::WeakPtr<DisplayResourceProvider> GetWeakPtr();

#if BUILDFLAG(IS_ANDROID)
  // Indicates if this resource is backed by an Android SurfaceTexture, and thus
  // can't really be promoted to an overlay.
  bool IsBackedBySurfaceTexture(ResourceId id) const;
#endif

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_WIN)
  // Indicates if this resource wants to receive promotion hints.
  bool DoesResourceWantPromotionHint(ResourceId id) const;
#endif

  // Returns the size in pixels of the underlying gpu mailbox/software bitmap.
  const gfx::Size GetResourceBackedSize(ResourceId id) const;

  bool IsResourceSoftwareBacked(ResourceId id) const;
  // Return the SharedImageFormat of the underlying buffer that can be used for
  // scanout.
  SharedImageFormat GetSharedImageFormat(ResourceId id) const;
  // Returns the color space of the resource.
  const gfx::ColorSpace& GetColorSpace(ResourceId id) const;
  // Returns true if the resource needs a detiling pass before scanout.
  bool GetNeedsDetiling(ResourceId id) const;

  const gfx::HDRMetadata& GetHDRMetadata(ResourceId id) const;

  // Indicates if this resource may be used for a hardware overlay plane.
  bool IsOverlayCandidate(ResourceId id) const;
  SurfaceId GetSurfaceId(ResourceId id) const;
  int GetChildId(ResourceId id) const;

  // Checks whether a resource is in use.
  bool InUse(ResourceId id) const;

  // Try removing the resources that are pending the |resource_fence|.
  void OnResourceFencePassed(ResourceFence* resource_fence,
                             base::flat_set<ResourceId> resources);

  // The following lock classes are part of the DisplayResourceProvider API and
  // are needed to read the resource contents. The user must ensure that they
  // only use GL locks on GL resources, etc, and this is enforced by assertions.

 protected:
  // Forward declared for ScopedReadLockSharedImage below.
  struct ChildResource;

 public:
  // Lock the resource to make sure the shared image is alive when accessing
  // SharedImage Mailbox.
  class VIZ_SERVICE_EXPORT ScopedReadLockSharedImage {
   public:
    ScopedReadLockSharedImage(DisplayResourceProvider* resource_provider,
                              ResourceId resource_id);
    ~ScopedReadLockSharedImage();

    ScopedReadLockSharedImage(ScopedReadLockSharedImage&& other);
    ScopedReadLockSharedImage& operator=(ScopedReadLockSharedImage&& other);

    const gpu::Mailbox& mailbox() const {
      DCHECK(resource_);
      return resource_->transferable.mailbox();
    }
    const gpu::SyncToken& sync_token() const {
      DCHECK(resource_);
      return resource_->sync_token();
    }

    // Sets the given |release_fence| onto this resource.
    // This is propagated to ReturnedResource when the resource is freed.
    void SetReleaseFence(gfx::GpuFenceHandle release_fence);

    // Returns true iff this resource has a read lock fence set.
    bool HasReadLockFence() const;

   protected:
    ChildResource* resource() { return resource_; }

   private:
    void Reset();

    // RAW_PTR_EXCLUSION: Performance reasons (based on analysis of MotionMark).
    RAW_PTR_EXCLUSION DisplayResourceProvider* resource_provider_ = nullptr;
    ResourceId resource_id_ = kInvalidResourceId;
    // RAW_PTR_EXCLUSION: Performance reasons (based on analysis of MotionMark).
    RAW_PTR_EXCLUSION ChildResource* resource_ = nullptr;
  };

  // All resources that are returned to children while an instance of this
  // class exists will be stored and returned when the instance is destroyed.
  class VIZ_SERVICE_EXPORT ScopedBatchReturnResources {
    STACK_ALLOCATED();

   public:
    explicit ScopedBatchReturnResources(
        DisplayResourceProvider* resource_provider,
        bool allow_access_to_gpu_thread = false);
    ~ScopedBatchReturnResources();

   private:
    DisplayResourceProvider* const resource_provider_ = nullptr;
    const bool was_access_to_gpu_thread_allowed_;
  };

  // Creates accounting for a child. Returns a child ID. surface_id is used to
  // associate resources to the surface they belong to. This is used for
  // overlays on webview where overlays are updated outside of normal draw (i.e
  // DrawAndSwap isn't called).
  int CreateChild(ReturnCallback return_callback, const SurfaceId& surface_id);

  // Destroys accounting for the child, deleting all accounted resources.
  void DestroyChild(int child);

  // Gets the child->parent resource ID map.
  const std::unordered_map<ResourceId, ResourceId, ResourceIdHasher>&
  GetChildToParentMap(int child) const;

  // Receives resources from a child, moving them from mailboxes. ResourceIds
  // passed are in the child namespace, and will be translated to the parent
  // namespace, added to the child->parent map.
  // This adds the resources to the working set in the ResourceProvider without
  // declaring which resources are in use. Use DeclareUsedResourcesFromChild
  // after calling this method to do that. All calls to ReceiveFromChild should
  // be followed by a DeclareUsedResourcesFromChild.
  // NOTE: if the sync_token is set on any TransferableResource, this will
  // wait on it.
  void ReceiveFromChild(
      int child,
      const std::vector<TransferableResource>& transferable_resources);

  // Once a set of resources have been received, they may or may not be used.
  // This declares what set of resources are currently in use from the child,
  // releasing any other resources back to the child.
  void DeclareUsedResourcesFromChild(int child,
                                     const ResourceIdSet& resources_from_child);

  // Returns the mailbox corresponding to a resource id.
  gpu::Mailbox GetMailbox(ResourceId resource_id) const;

  // Sets if the GPU thread is available (it always is for Chrome, but for
  // WebView it happens only when Android calls us on RenderThread.
  void SetAllowAccessToGPUThread(bool allow);

 protected:
  friend class ScopedAllowGpuAccessForDisplayResourceProvider;

  enum class CanDeleteNowResult { kYes, kYesButLoseResource, kNo };

  enum DeleteStyle {
    NORMAL,
    FOR_SHUTDOWN,
  };

  struct Child {
    Child();
    Child(Child&& other);
    Child& operator=(Child&& other);
    ~Child();

    int id;
    std::unordered_map<ResourceId, ResourceId, ResourceIdHasher>
        child_to_parent_map;
    ReturnCallback return_callback;
    SurfaceId surface_id;
    bool marked_for_deletion = false;
  };

  // The data structure used to track state of Gpu and Software-based
  // resources and the service, for resources transferred
  // between the two. This is an implementation detail of the resource tracking
  // for client and service libraries and should not be used directly from
  // external client code.
  struct ChildResource {
    ChildResource(int child_id, const TransferableResource& transferable);
    ChildResource(ChildResource&& other);
    ~ChildResource();

    bool is_gpu_resource_type() const { return !transferable.is_software; }
    const gpu::SyncToken& sync_token() const { return sync_token_; }

    bool InUse() const {
      return lock_for_read_count > 0 || locked_for_external_use ||
             lock_for_overlay_count > 0;
    }

    void UpdateSyncToken(const gpu::SyncToken& sync_token);

    // This is the id of the client the resource comes from.
    const int child_id;
    // Data received from the client that describes the resource fully.
    const TransferableResource transferable;

    // The number of times the resource has been received from a client. It must
    // have this many number of references returned back to the client in order
    // for it to know it is no longer in use in the service. This is used to
    // avoid races where a resource is in flight to the service while also being
    // returned to the client. It starts with an initial count of 1, for the
    // first time the resource is received.
    int imported_count = 1;

    // The number of active users of a resource in the display compositor. While
    // a resource is in use, it will not be returned back to the client even if
    // the ResourceId is deleted.
    int lock_for_read_count = 0;
    // When true, the resource is currently being used externally. This is a
    // parallel counter to |lock_for_read_count| which can only go to 1.
    bool locked_for_external_use = false;
    // The number of active users using this resource as overlay content.
    int lock_for_overlay_count = 0;

    // When the resource should be deleted until it is actually reaped.
    bool marked_for_deletion = false;

    // A pointer to the shared memory structure for software-backed resources,
    // when it is mapped into memory in this process.
    std::unique_ptr<SharedBitmap> shared_bitmap;
    // A GUID for reporting the |shared_bitmap| to memory tracing. The GUID is
    // known by other components in the system as well to give the same id for
    // this shared memory bitmap everywhere. This is empty until the resource is
    // mapped for use in the display compositor.
    base::UnguessableToken shared_bitmap_tracing_guid;

    // A fence used for returning resources after the display compositor has
    // completed accessing the resources it received from a client. This can
    // either be a read lock or a release fence. If the |transferable| has
    // synchronization type set as kGpuCommandsCompleted, the resource can be
    // returned after ResourceFence::HasPassed is true. If the |transferable|
    // has the synchronization type set as kReleaseFence, the resource can be
    // returned after the fence has a release fence set.
    scoped_refptr<ResourceFence> resource_fence;

    // SkiaRenderer specific details about this resource. Added to ChildResource
    // to avoid map lookups further down the pipeline.
    std::unique_ptr<ExternalUseClient::ImageContext> image_context;

    // A release fence to propagate to ReturnedResource so clients may
    // use it.
    gfx::GpuFenceHandle release_fence;

   private:
    // A SyncToken associated with a texture-backed or GpuMemoryBuffer-backed
    // resource. It is given from a child to the service, and waited on in order
    // to use the resource.
    gpu::SyncToken sync_token_;
  };

  using ChildMap = std::unordered_map<int, Child>;
  using ResourceMap =
      std::unordered_map<ResourceId, ChildResource, ResourceIdHasher>;

  explicit DisplayResourceProvider(Mode mode);

  const ChildResource* GetResource(ResourceId id) const;
  ChildResource* GetResource(ResourceId id);

  // TODO(ericrk): TryGetResource is part of a temporary workaround for cases
  // where resources which should be available are missing. This version may
  // return nullptr if a resource is not found. https://crbug.com/811858
  const ChildResource* TryGetResource(ResourceId id) const;
  ChildResource* TryGetResource(ResourceId id);

  void TryReleaseResource(ResourceId id, ChildResource* resource);
  bool ResourceFenceHasPassed(const ChildResource* resource) const;

  void DeleteAndReturnUnusedResourcesToChild(
      ChildMap::iterator child_it,
      DeleteStyle style,
      const std::vector<ResourceId>& unused);
  virtual std::vector<ReturnedResource>
  DeleteAndReturnUnusedResourcesToChildImpl(
      Child& child_info,
      DeleteStyle style,
      const std::vector<ResourceId>& unused) = 0;
  CanDeleteNowResult CanDeleteNow(const Child& child_info,
                                  const ChildResource& resource,
                                  DeleteStyle style) const;

  // Destroys DisplayResourceProvider, must be called before destructor because
  // it might call virtual functions from inside.
  void Destroy();
  void DestroyChildInternal(ChildMap::iterator it, DeleteStyle style);

  void SetBatchReturnResources(bool aggregate);
  void TryFlushBatchedResources();

  SEQUENCE_CHECKER(sequence_checker_);
  const Mode mode_;

  ResourceMap resources_;
  ChildMap children_;

  base::flat_map<int, std::vector<ResourceId>> batched_returning_resources_;
  // Keep track of whether deleted resources should be batched up or returned
  // immediately.
  int batch_return_resources_lock_count_ = 0;
  // The ResourceIds in DisplayResourceProvider start from 2 to avoid
  // conflicts with id from ClientResourceProvider.
  ResourceIdGenerator resource_id_generator_{2u};
  // Used as child id when creating a child.
  int next_child_ = 1;
  // A process-unique ID used for disambiguating memory dumps from different
  // resource providers.
  int tracing_id_;

  // Indicates that gpu thread is available and calls like
  // ReleaseImageContexts() are expected to finish in finite time. It's always
  // true for Chrome, but on WebView we need to have access to RenderThread.
  bool can_access_gpu_thread_ = true;

  // OnResourceFencePassed() may be called by resource_fence that lives past
  // destruction of this class.
  base::WeakPtrFactory<DisplayResourceProvider> weak_factory_{this};
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_DISPLAY_RESOURCE_PROVIDER_H_
