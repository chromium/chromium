// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_CLIENT_CLIENT_RESOURCE_PROVIDER_H_
#define COMPONENTS_VIZ_CLIENT_CLIENT_RESOURCE_PROVIDER_H_

#include <memory>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/thread_checker.h"
#include "components/viz/client/viz_client_export.h"
#include "components/viz/common/display/renderer_settings.h"
#include "components/viz/common/resources/release_callback.h"
#include "components/viz/common/resources/resource_id.h"
#include "components/viz/common/resources/returned_resource.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "third_party/khronos/GLES2/gl2.h"

namespace gpu {
namespace gles2 {
class GLES2Interface;
}
namespace raster {
class RasterInterface;
}
}  // namespace gpu

namespace viz {
class RasterContextProvider;

// This class is used to give an integer name (ResourceId) to a gpu or software
// resource (shipped as a TransferableResource), in order to use that name in
// DrawQuads and give the resource to the viz display compositor. When the
// resource is removed from the ClientResourceProvider, the ReleaseCallback will
// be called once the resource is no longer in use by the display compositor.
//
// This class is not thread-safe and can only be called from the thread it was
// created on (in practice, the impl thread).
class VIZ_CLIENT_EXPORT ClientResourceProvider {
 public:
  using ResourceFlushCallback = base::RepeatingCallback<void()>;

  // Upon destruction this will call `batch_release_callback_` in order to
  // signal that all accumulated resource callbacks should be ran.
  //
  // See `CreateScopedBatchResourcesRelease`
  class VIZ_CLIENT_EXPORT ScopedBatchResourcesRelease {
   public:
    ScopedBatchResourcesRelease(const ScopedBatchResourcesRelease& other) =
        delete;
    ScopedBatchResourcesRelease& operator=(
        const ScopedBatchResourcesRelease& other) = delete;
    ScopedBatchResourcesRelease(ScopedBatchResourcesRelease&& other);
    ScopedBatchResourcesRelease& operator=(
        ScopedBatchResourcesRelease&& other) = default;
    ~ScopedBatchResourcesRelease();

   protected:
    explicit ScopedBatchResourcesRelease(
        base::OnceClosure batch_release_callback);

   private:
    base::OnceClosure batch_release_callback_;
  };

  ClientResourceProvider();
  ClientResourceProvider(
      scoped_refptr<base::SequencedTaskRunner> main_task_runner,
      scoped_refptr<base::SequencedTaskRunner> impl_task_runner,
      ResourceFlushCallback resource_flush_callback);

  ClientResourceProvider(const ClientResourceProvider&) = delete;
  ClientResourceProvider& operator=(const ClientResourceProvider&) = delete;

  ~ClientResourceProvider();

  static gpu::SyncToken GenerateSyncTokenHelper(gpu::gles2::GLES2Interface* gl);
  static gpu::SyncToken GenerateSyncTokenHelper(
      gpu::raster::RasterInterface* ri);

  // Prepares resources to be transfered to the parent, moving them to
  // mailboxes and serializing meta-data into TransferableResources.
  // Resources are not removed from the ResourceProvider, but are marked as
  // "in use".
  void PrepareSendToParent(
      const std::vector<ResourceId>& resource_ids,
      std::vector<TransferableResource>* transferable_resources,
      RasterContextProvider* context_provider);

  // Receives resources from the parent, moving them from mailboxes. ResourceIds
  // passed are in the child namespace.
  // NOTE: if the sync_token is set on any TransferableResource, this will
  // wait on it.
  void ReceiveReturnsFromParent(
      std::vector<ReturnedResource> transferable_resources);

  // Receives a resource from an external client that can be used in compositor
  // frames, via the returned ResourceId. Can be provided with an optional
  // `evicted_callback`, which will be invoked once we are no longer visible and
  // have been evicted. When `evicted_callback` is called the client should
  // invoke `RemoveImportedResources` to unlock the resource. Allowing the
  // resource to be released when it is returned from the parent. When
  // `main_thread_release_callback` is provided, and
  // `features::kBatchMainThreadReleaseCallbacks` is enabled, the callback will
  // be invoked on `main_thread_task_runner_` when it has been returned.
  ResourceId ImportResource(const TransferableResource& resource,
                            ReleaseCallback impl_release_callback,
                            ReleaseCallback main_thread_release_callback = {},
                            ResourceEvictedCallback evicted_callback = {});
  // Removes an imported resource, which will call the ReleaseCallback given
  // originally, once the resource is no longer in use by any compositor frame.
  void RemoveImportedResource(ResourceId resource_id);

  // Call this to indicate that the connection to the parent is lost and
  // resources previously exported will not be able to be returned. If |lose| is
  // true, the resources are also marked as lost, to indicate the state of each
  // resource can not be known, and/or they can not be reused.
  //
  // When a resource is sent to the parent (via PrepareSendToParent) it is put
  // into an exported state, preventing it from being released until the parent
  // returns the resource. Calling this drops that exported state on all
  // resources allowing immediate release of them if they are removed via
  // RemoveImportedResource().
  void ReleaseAllExportedResources(bool lose);

  // Immediately runs the ReleaseCallback for all resources that have been
  // previously imported and removed, but not released yet. There should not be
  // any imported resources yet when this is called, as they can be removed
  // first via RemoveImportedResource(), and potentially avoid being lost.
  void ShutdownAndReleaseAllResources();

  // Verify that the ResourceId is valid and is known to this class, for debug
  // checks.
  void ValidateResource(ResourceId id) const;

  // Checks whether a resource is in use by a consumer.
  bool InUseByConsumer(ResourceId id);

  void SetEvicted(bool evicted);
  void SetVisible(bool visible);

  // Controls how `RemoveImportedResource` handled callbacks. While
  // `ScopedBatchResourcesRelease` is alive, we will collect all callbacks of
  // resources being removed. Upon leaving scope, we will perform a batch
  // release of all releases. This includes a single thread-hop to the
  // Main-thread for associated callbacks.
  ScopedBatchResourcesRelease CreateScopedBatchResourcesRelease();

  size_t num_resources_for_testing() const;

 private:
  struct ImportedResource;

  void PrepareSendToParentInternal(
      const std::vector<ResourceId>& export_ids,
      std::vector<TransferableResource>* list,
      base::OnceCallback<void(std::vector<GLbyte*>* tokens)>
          verify_sync_tokens);

  // Validates the memory impact of resources that are locked once we are both
  // evicted and no longer visible. This will also notify clients of eviction
  // via any `RemoveImportedResources`. If resources have been already returned
  // by the parent (the Display Compositor's FrameSink) this can lead to them
  // being returned to the client (such as cc::LayerTreeHostImpl.)
  void HandleEviction();

  void BatchMainReleaseCallbacks(
      std::vector<base::OnceClosure> release_callbacks);

  // Runs all release callbacks accumulated in `batch_main_release_callbacks_`.
  // Main thread callbacks will be posted to that thread in a single thread hop.
  void BatchResourceRelease();
  // If `batch` is true, then this will take the `main_thread_release_callback`
  // from `imported` to be batched later. Otherwise this runs them immediately.
  // This will also run all `impl_thread_release_callback` immediately.
  void TakeOrRunResourceReleases(bool batch, ImportedResource& imported);

  THREAD_CHECKER(thread_checker_);

  base::flat_map<ResourceId, ImportedResource> imported_resources_;
  // The ResourceIds in ClientResourceProvider start from 1 to avoid
  // conflicts with id from DisplayResourceProvider.
  ResourceIdGenerator id_generator_;

  // Whether the Client has had its Surface Evicted. When `true` all
  // `imported_resources_` are no longer required by the Parent. Though we need
  // to wait until we are not `visible_` as the Client may still use them.
  bool evicted_ = false;

  // Whether the Client is visible. While ClientResourceProvider is not
  // thread-safe, it is often used in a multi-threaded Renderer. While `true`
  // all `imported_resources_` may still be used by the Client. So it is not
  // safe to release them, even if we have been `evicted_`.
  bool visible_ = false;

  const scoped_refptr<base::SequencedTaskRunner> main_task_runner_;
  const scoped_refptr<base::SequencedTaskRunner> impl_task_runner_;

  ResourceFlushCallback resource_flush_callback_;

  // When true, we are able to use our TaskRunners to post all
  // `main_thread_release_callback` to the `main_task_runner_`. Enabling us to
  // have a single thread hop, rather than each callback performing it's own
  // separate hop.
  bool threaded_release_callbacks_supported_ = false;

  // While `true` resources being released will have their callbacks stored in
  // the vector below. To be released afterwards in `BatchResourceRelease`.
  bool batch_release_callbacks_ = false;
  std::vector<base::OnceClosure> batch_main_release_callbacks_;

  base::WeakPtrFactory<ClientResourceProvider> weak_factory_{this};
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_CLIENT_CLIENT_RESOURCE_PROVIDER_H_
