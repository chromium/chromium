// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/resources/transferable_resource.h"
#include "components/viz/common/resources/returned_resource.h"
#include "gpu/command_buffer/client/client_shared_image.h"

namespace viz {

// static
TransferableResource TransferableResource::MakeSoftware(
    const SharedBitmapId& id,
    const gpu::SyncToken& sync_token,
    const gfx::Size& size,
    SharedImageFormat format,
    ResourceSource source) {
  TransferableResource r;
  r.is_software = true;
  r.mailbox_holder.mailbox = id;
  r.mailbox_holder.sync_token = sync_token;
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
  TransferableResource r;
  r.is_software = false;
  r.mailbox_holder.mailbox = mailbox;
  r.mailbox_holder.texture_target = texture_target;
  r.mailbox_holder.sync_token = sync_token;
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

TransferableResource::TransferableResource() = default;
TransferableResource::~TransferableResource() = default;

TransferableResource::TransferableResource(const TransferableResource& other) =
    default;
TransferableResource& TransferableResource::operator=(
    const TransferableResource& other) = default;

ReturnedResource TransferableResource::ToReturnedResource() const {
  ReturnedResource returned;
  returned.id = id;
  returned.sync_token = mailbox_holder.sync_token;
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
