// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_CLIENT_CLIENT_RESOURCE_PROVIDER_H_
#define COMPONENTS_VIZ_CLIENT_CLIENT_RESOURCE_PROVIDER_H_

#include <vector>

#include "base/threading/thread_checker.h"
#include "components/viz/client/viz_client_export.h"
#include "components/viz/common/display/renderer_settings.h"
#include "components/viz/common/resources/release_callback.h"
#include "components/viz/common/resources/resource_id.h"
#include "components/viz/common/resources/resource_settings.h"
#include "components/viz/common/resources/returned_resource.h"
#include "components/viz/common/resources/single_release_callback.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "third_party/skia/include/gpu/GrContext.h"

namespace gpu {
namespace gles2 {
class GLES2Interface;
}
namespace raster {
class RasterInterface;
}
}  // namespace gpu

namespace viz {
class ContextProvider;
class RasterContextProvider;

// This class is used to give an integer name (ResourceId) to a gpu or software
// resource (shipped as a TransferableResource), in order to use that name in
// DrawQuads and give the resource to the viz display compositor. When the
// resource is removed from the ClientResourceProvider, the
// SingleReleaseCallback will be called once the resource is no longer in use by
// the display compositor.
//
// This class is not thread-safe and can only be called from the thread it was
// created on (in practice, the impl thread).
class VIZ_CLIENT_EXPORT ClientResourceProvider {
 public:
  explicit ClientResourceProvider(bool verified_sync_tokens_required);
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

  // TODO(sergeyu): Remove after updating all callers to use the above version
  // of this method.
  void PrepareSendToParent(
      const std::vector<ResourceId>& resource_ids,
      std::vector<TransferableResource>* transferable_resources,
      ContextProvider* context_provider);

  // Receives resources from the parent, moving them from mailboxes. ResourceIds
  // passed are in the child namespace.
  // NOTE: if the sync_token is set on any TransferableResource, this will
  // wait on it.
  void ReceiveReturnsFromParent(
      std::vector<ReturnedResource> transferable_resources);

  // Receives a resource from an external client that can be used in compositor
  // frames, via the returned ResourceId.
  ResourceId ImportResource(const TransferableResource&,
                            std::unique_ptr<SingleReleaseCallback>);
  // Removes an imported resource, which will call the ReleaseCallback given
  // originally, once the resource is no longer in use by any compositor frame.
  void RemoveImportedResource(ResourceId);

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

  // Immediately runs the SingleReleaseCallback for all resources that have been
  // previously imported and removed, but not released yet. There should not be
  // any imported resources yet when this is called, as they can be removed
  // first via RemoveImportedResource(), and potentially avoid being lost.
  void ShutdownAndReleaseAllResources();

  // Verify that the ResourceId is valid and is known to this class, for debug
  // checks.
  void ValidateResource(ResourceId id) const;

  // Checks whether a resource is in use by a consumer.
  bool InUseByConsumer(ResourceId id);

  size_t num_resources_for_testing() const;

  class VIZ_CLIENT_EXPORT ScopedSkSurface {
   public:
    ScopedSkSurface(GrContext* gr_context,
                    sk_sp<SkColorSpace> color_space,
                    GLuint texture_id,
                    GLenum texture_target,
                    const gfx::Size& size,
                    ResourceFormat format,
                    bool can_use_lcd_text,
                    int msaa_sample_count);
    ~ScopedSkSurface();

    SkSurface* surface() const { return surface_.get(); }

    static SkSurfaceProps ComputeSurfaceProps(bool can_use_lcd_text);

   private:
    sk_sp<SkSurface> surface_;

    DISALLOW_COPY_AND_ASSIGN(ScopedSkSurface);
  };

 private:
  struct ImportedResource;

  void PrepareSendToParentInternal(
      const std::vector<ResourceId>& export_ids,
      std::vector<TransferableResource>* list,
      base::OnceCallback<void(std::vector<GLbyte*>* tokens)>
          verify_sync_tokens);

  THREAD_CHECKER(thread_checker_);
  const bool verified_sync_tokens_required_;

  base::flat_map<ResourceId, ImportedResource> imported_resources_;
  // The ResourceIds in ClientResourceProvider start from 1 to avoid
  // conflicts with id from DisplayResourceProvider.
  ResourceId next_id_ = 1;

  DISALLOW_COPY_AND_ASSIGN(ClientResourceProvider);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_CLIENT_CLIENT_RESOURCE_PROVIDER_H_
