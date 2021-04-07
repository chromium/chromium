// Copyright 2017 The Chromium Authors. All rights reserved.
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
#include "base/containers/small_map.h"
#include "base/threading/thread_checker.h"
#include "base/trace_event/memory_dump_provider.h"
#include "build/build_config.h"
#include "components/viz/common/resources/resource_id.h"
#include "components/viz/common/resources/return_callback.h"
#include "components/viz/common/resources/shared_bitmap.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "components/viz/service/display/external_use_client.h"
#include "components/viz/service/display/resource_fence.h"
#include "components/viz/service/viz_service_export.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "third_party/khronos/GLES2/gl2.h"
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
  void DidLoseContextProvider() { lost_context_provider_ = true; }
  size_t num_resources() const { return resources_.size(); }

  // base::trace_event::MemoryDumpProvider implementation.
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

#if defined(OS_ANDROID)
  // Indicates if this resource is backed by an Android SurfaceTexture, and thus
  // can't really be promoted to an overlay.
  bool IsBackedBySurfaceTexture(ResourceId id);

  // Return the number of resources that request promotion hints.
  size_t CountPromotionHintRequestsForTesting();

  // This should be called after WaitSyncToken in GLRenderer.
  void InitializePromotionHintRequest(ResourceId id);
#endif

  // Indicates if this resource wants to receive promotion hints.
  bool DoesResourceWantPromotionHint(ResourceId id) const;

  // Return true if and only if any resource wants a promotion hint.
  bool DoAnyResourcesWantPromotionHints() const;

  bool IsResourceSoftwareBacked(ResourceId id);
  // Return the format of the underlying buffer that can be used for scanout.
  gfx::BufferFormat GetBufferFormat(ResourceId id);
  ResourceFormat GetResourceFormat(ResourceId id);
  const gfx::ColorSpace& GetColorSpace(ResourceId id);
  // Indicates if this resource may be used for a hardware overlay plane.
  bool IsOverlayCandidate(ResourceId id);

  // Checks whether a resource is in use.
  bool InUse(ResourceId id);

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
      return resource_->transferable.mailbox_holder.mailbox;
    }
    const gpu::SyncToken& sync_token() const {
      DCHECK(resource_);
      return resource_->sync_token();
    }

   private:
    void Reset();

    DisplayResourceProvider* resource_provider_ = nullptr;
    ResourceId resource_id_ = kInvalidResourceId;
    ChildResource* resource_ = nullptr;
  };

  // All resources that are returned to children while an instance of this
  // class exists will be stored and returned when the instance is destroyed.
  class VIZ_SERVICE_EXPORT ScopedBatchReturnResources {
   public:
    explicit ScopedBatchReturnResources(
        DisplayResourceProvider* resource_provider,
        bool allow_access_to_gpu_thread = false);
    ~ScopedBatchReturnResources();

   private:
    DisplayResourceProvider* const resource_provider_;
    const bool was_access_to_gpu_thread_allowed_;
  };

  // Sets the current read fence. If a resource is locked for read
  // and has read fences enabled, the resource will not allow writes
  // until this fence has passed.
  void SetReadLockFence(ResourceFence* fence) {
    current_read_lock_fence_ = fence;
  }

  // Creates accounting for a child. Returns a child ID.
  int CreateChild(ReturnCallback return_callback);

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
  gpu::Mailbox GetMailbox(ResourceId resource_id);

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

  enum SynchronizationState {
    // The LOCALLY_USED state is the state each resource defaults to when
    // constructed or modified or read. This state indicates that the
    // resource has not been properly synchronized and it would be an error
    // to return this resource to a client.
    LOCALLY_USED,

    // The NEEDS_WAIT state is the state that indicates a resource has been
    // modified but it also has an associated sync token assigned to it.
    // The sync token has not been waited on with the local context. When
    // a sync token arrives from a client, it is automatically initialized as
    // NEEDS_WAIT as well since we still need to wait on it before the resource
    // is synchronized on the current context. It is an error to use the
    // resource locally for reading or writing if the resource is in this state.
    NEEDS_WAIT,

    // The SYNCHRONIZED state indicates that the resource has been properly
    // synchronized locally. This can either synchronized externally (such
    // as the case of software rasterized bitmaps), or synchronized
    // internally using a sync token that has been waited upon. In the
    // former case where the resource was synchronized externally, a
    // corresponding sync token will not exist. In the latter case which was
    // synchronized from the NEEDS_WAIT state, a corresponding sync token will
    // exist which is associated with the resource. This sync token is still
    // valid and still associated with the resource and can be passed as an
    // external resource for others to wait on.
    SYNCHRONIZED,
  };

  struct Child {
    Child();
    Child(Child&& other);
    Child& operator=(Child&& other);
    ~Child();

    std::unordered_map<ResourceId, ResourceId, ResourceIdHasher>
        child_to_parent_map;
    ReturnCallback return_callback;
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
    bool needs_sync_token() const {
      return is_gpu_resource_type() && synchronization_state_ == LOCALLY_USED;
    }
    const gpu::SyncToken& sync_token() const { return sync_token_; }
    SynchronizationState synchronization_state() const {
      return synchronization_state_;
    }
    // If true the gpu resource needs its SyncToken waited on in order to be
    // synchronized for use.
    bool ShouldWaitSyncToken() const {
      return synchronization_state_ == NEEDS_WAIT;
    }

    bool InUse() const {
      return lock_for_read_count > 0 || locked_for_external_use ||
             lock_for_overlay_count > 0;
    }

    void SetLocallyUsed();
    void SetSynchronized();
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

    // Texture id for texture-backed resources, when the mailbox is mapped into
    // a client GL id in this process.
    GLuint gl_id = 0;
    // The current min/mag filter for GL texture-backed resources. The original
    // filter as given from the client is saved in |transferable|.
    GLenum filter;
    // A pointer to the shared memory structure for software-backed resources,
    // when it is mapped into memory in this process.
    std::unique_ptr<SharedBitmap> shared_bitmap;
    // A GUID for reporting the |shared_bitmap| to memory tracing. The GUID is
    // known by other components in the system as well to give the same id for
    // this shared memory bitmap everywhere. This is empty until the resource is
    // mapped for use in the display compositor.
    base::UnguessableToken shared_bitmap_tracing_guid;

    // A fence used for accessing a gpu resource for reading, that ensures any
    // writing done to the resource has been completed. This is implemented and
    // used to implement transferring ownership of the resource from the client
    // to the service, and in the GL drawing code before reading from the
    // texture.
    scoped_refptr<ResourceFence> read_lock_fence;

    // SkiaRenderer specific details about this resource. Added to ChildResource
    // to avoid map lookups further down the pipeline.
    std::unique_ptr<ExternalUseClient::ImageContext> image_context;

   private:
    // Tracks if a sync token needs to be waited on before using the resource.
    SynchronizationState synchronization_state_;
    // A SyncToken associated with a texture-backed or GpuMemoryBuffer-backed
    // resource. It is given from a child to the service, and waited on in order
    // to use the resource, and this is tracked by the |synchronization_state_|.
    gpu::SyncToken sync_token_;
  };

  using ChildMap = std::unordered_map<int, Child>;
  using ResourceMap =
      std::unordered_map<ResourceId, ChildResource, ResourceIdHasher>;

  explicit DisplayResourceProvider(Mode mode);

  ChildResource* GetResource(ResourceId id);

  // TODO(ericrk): TryGetResource is part of a temporary workaround for cases
  // where resources which should be available are missing. This version may
  // return nullptr if a resource is not found. https://crbug.com/811858
  ChildResource* TryGetResource(ResourceId id);

  void TryReleaseResource(ResourceId id, ChildResource* resource);
  // Binds the given GL resource to a texture target for sampling using the
  // specified filter for both minification and magnification. Returns the
  // texture target used. The resource must be locked for reading.
  bool ReadLockFenceHasPassed(const ChildResource* resource);
#if defined(OS_ANDROID)
  void DeletePromotionHint(ResourceMap::iterator it);
#endif

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
                                  DeleteStyle style);

  // Destroys DisplayResourceProvider, must be called before destructor because
  // it might call virtual functions from inside.
  void Destroy();
  void DestroyChildInternal(ChildMap::iterator it, DeleteStyle style);

  void SetBatchReturnResources(bool aggregate);
  void TryFlushBatchedResources();

  THREAD_CHECKER(thread_checker_);
  const Mode mode_;

  ResourceMap resources_;
  ChildMap children_;

  base::flat_map<int, std::vector<ResourceId>> batched_returning_resources_;
  scoped_refptr<ResourceFence> current_read_lock_fence_;
  // Keep track of whether deleted resources should be batched up or returned
  // immediately.
  int batch_return_resources_lock_count_ = 0;
  // Set to true when the ContextProvider becomes lost, to inform that resources
  // modified by this class are now in an indeterminate state.
  bool lost_context_provider_ = false;
  // The ResourceIds in DisplayResourceProvider start from 2 to avoid
  // conflicts with id from ClientResourceProvider.
  ResourceIdGenerator resource_id_generator_{2u};
  // Used as child id when creating a child.
  int next_child_ = 1;
  // A process-unique ID used for disambiguating memory dumps from different
  // resource providers.
  int tracing_id_;

#if defined(OS_ANDROID)
  // Set of ResourceIds that would like to be notified about promotion hints.
  ResourceIdSet wants_promotion_hints_set_;
#endif

  // Indicates that gpu thread is available and calls like
  // ReleaseImageContexts() are expected to finish in finite time. It's always
  // true for Chrome, but on WebView we need to have access to RenderThread.
  bool can_access_gpu_thread_ = true;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_DISPLAY_RESOURCE_PROVIDER_H_
