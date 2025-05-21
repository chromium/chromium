// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/resources/transferable_resource.h"
#include "components/viz/common/resources/returned_resource.h"
#include "gpu/command_buffer/client/client_shared_image.h"

namespace viz {

// static
TransferableResource TransferableResource::MakeSoftwareSharedImage(
    const scoped_refptr<gpu::ClientSharedImage>& client_shared_image,
    const gpu::SyncToken& sync_token,
    const gfx::Size& size,
    SharedImageFormat format,
    ResourceSource source) {
  // Passed in format must be either single or multiplane and not default set.
  CHECK(format.is_single_plane() || format.is_multi_plane());
  TransferableResource r;
  r.is_software = true;
  r.memory_buffer_id_ = client_shared_image->mailbox();
  r.sync_token_ = sync_token;
  r.size = size;
  r.format = format;
  r.resource_source = source;
  return r;
}

// static
TransferableResource TransferableResource::MakeGpu(
    const gpu::Mailbox& mailbox,
    uint32_t texture_target,
    const gpu::SyncToken& sync_token,
    const gfx::Size& size,
    SharedImageFormat format,
    bool is_overlay_candidate,
    ResourceSource source) {
  // Passed in format must be either single or multiplane and not default set.
  CHECK(format.is_single_plane() || format.is_multi_plane());
  TransferableResource r;
  r.is_software = false;
  r.memory_buffer_id_ = mailbox;
  r.texture_target_ = texture_target;
  r.sync_token_ = sync_token;
  r.size = size;
  r.format = format;
  r.is_overlay_candidate = is_overlay_candidate;
  r.resource_source = source;
  return r;
}

TransferableResource TransferableResource::MakeGpu(
    const scoped_refptr<gpu::ClientSharedImage>& client_shared_image,
    uint32_t texture_target,
    const gpu::SyncToken& sync_token,
    const gfx::Size& size,
    SharedImageFormat format,
    bool is_overlay_candidate,
    ResourceSource source) {
  CHECK(client_shared_image);
  return MakeGpu(client_shared_image->mailbox(), texture_target, sync_token,
                 size, format, is_overlay_candidate, source);
}

TransferableResource TransferableResource::Make(
    const scoped_refptr<gpu::ClientSharedImage>& shared_image,
    ResourceSource source,
    const gpu::SyncToken& sync_token,
    const MetadataOverride& override) {
  CHECK(shared_image);
  TransferableResource resource;
  resource.is_software = shared_image->is_software();
  resource.memory_buffer_id_ = shared_image->mailbox();
  resource.sync_token_ = sync_token;
  resource.resource_source = source;

  resource.size = override.size.value_or(shared_image->size());
  resource.format = override.format.value_or(shared_image->format());
  // Passed in format must be either single or multiplane and not default set.
  CHECK(resource.format.is_single_plane() || resource.format.is_multi_plane());
  resource.is_overlay_candidate = override.is_overlay_candidate.value_or(
      shared_image->usage().Has(gpu::SHARED_IMAGE_USAGE_SCANOUT));
  resource.color_space =
      override.color_space.value_or(shared_image->color_space());
  resource.origin = override.origin.value_or(shared_image->surface_origin());
  SkAlphaType alpha_type =
      override.alpha_type.value_or(shared_image->alpha_type());
  // TODO(crbug.com/410591523): Set `resource.alpha_type` directly from
  // `alpha_type` under a killswitch; this will result in kOpaque_SkAlphaType
  // being passed through to the service side, whereas historically that has
  // been compressed to a "premul" bool and treated as kPremul_SkAlphaType on
  // the service side.
  resource.alpha_type =
      (alpha_type == kUnpremul_SkAlphaType) ? alpha_type : kPremul_SkAlphaType;
  resource.set_texture_target(
      override.texture_target.value_or(shared_image->GetTextureTarget()));

  return resource;
}

TransferableResource::TransferableResource() = default;
TransferableResource::~TransferableResource() = default;

TransferableResource::TransferableResource(const TransferableResource& other) =
    default;
TransferableResource& TransferableResource::operator=(
    const TransferableResource& other) = default;

ReturnedResource TransferableResource::ToReturnedResource() const {
  ReturnedResource returned;
  returned.id = id;
  returned.sync_token = sync_token_;
  returned.count = 1;
  return returned;
}

// static
std::vector<ReturnedResource> TransferableResource::ReturnResources(
    const std::vector<TransferableResource>& input) {
  std::vector<ReturnedResource> out;
  out.reserve(input.size());
  for (const auto& r : input)
    out.push_back(r.ToReturnedResource());
  return out;
}

}  // namespace viz
