// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/test_resource_factory.h"

#include <unordered_map>
#include <vector>

#include "components/viz/client/client_resource_provider.h"
#include "components/viz/common/resources/resource_id.h"
#include "components/viz/service/display/display_resource_provider_skia.h"
#include "components/viz/test/fake_skia_output_surface.h"
#include "components/viz/test/test_context_provider.h"

namespace viz {
namespace {

static ResourceId CreateResourceInLayerTree(
    ClientResourceProvider* child_resource_provider,
    const gfx::Size& size,
    const TestResourceFactory::TestResourceContext& resource_context,
    SharedImageFormat format) {
  auto resource = TransferableResource::MakeGpu(
      gpu::Mailbox::Generate(), GL_TEXTURE_2D, gpu::SyncToken(), size, format,
      resource_context.is_overlay_candidate);

  if (resource_context.is_low_latency_rendering) {
    resource.is_low_latency_rendering = true;
  }

  ResourceId resource_id =
      child_resource_provider->ImportResource(resource, base::DoNothing());

  return resource_id;
}

}  // namespace

TestResourceFactory::TestResourceFactory() {
  output_surface_ = FakeSkiaOutputSurface::Create3d();
  output_surface_->BindToClient(&output_surface_client_);

  display_resource_provider_ = std::make_unique<DisplayResourceProviderSkia>();
  lock_set_for_external_use_.emplace(display_resource_provider_.get(),
                                     output_surface_.get());

  client_context_provider_ = TestContextProvider::Create();
  client_context_provider_->BindToCurrentSequence();
  client_resource_provider_ = std::make_unique<ClientResourceProvider>();
}

TestResourceFactory::~TestResourceFactory() {
  client_resource_provider_->ShutdownAndReleaseAllResources();
  client_resource_provider_ = nullptr;
  client_context_provider_ = nullptr;
  lock_set_for_external_use_.reset();
  display_resource_provider_ = nullptr;
  output_surface_ = nullptr;
}

ResourceId TestResourceFactory::CreateResource(
    const gfx::Size& size,
    const TestResourceContext& resource_context,
    SharedImageFormat format,
    SurfaceId test_surface_id) {
  ResourceId resource_id = CreateResourceInLayerTree(
      client_resource_provider_.get(), size, resource_context, format);

  const int child_id = display_resource_provider_->CreateChild(
      base::DoNothing(), test_surface_id);

  // Transfer resource to the parent.
  std::vector<ResourceId> resource_ids_to_transfer;
  resource_ids_to_transfer.push_back(resource_id);
  std::vector<TransferableResource> list;

  client_resource_provider_->PrepareSendToParent(
      resource_ids_to_transfer, &list, client_context_provider_.get());
  display_resource_provider_->ReceiveFromChild(child_id, list);

  // Delete it in the child so it won't be leaked, and will be released once
  // returned from the parent.
  client_resource_provider_->RemoveImportedResource(resource_id);

  // In DisplayResourceProvider's namespace, use the mapped resource id.
  std::unordered_map<ResourceId, ResourceId, ResourceIdHasher> resource_map =
      display_resource_provider_->GetChildToParentMap(child_id);
  return resource_map[list[0].id];
}

}  // namespace viz
