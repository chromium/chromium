// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/resources/transferable_resource.h"
#include "components/viz/common/resources/returned_resource.h"

namespace viz {

// static
TransferableResource TransferableResource::MakeSoftware(
    const SharedBitmapId& id,
    const gfx::Size& size,
    SharedImageFormat format) {
  TransferableResource r;
  r.is_software = true;
  r.mailbox_holder.mailbox = id;
  r.size = size;
  r.format = format;
  return r;
}

// static
TransferableResource TransferableResource::MakeGpu(
    const gpu::Mailbox& mailbox,
    uint32_t texture_target,
    const gpu::SyncToken& sync_token,
    const gfx::Size& size,
    SharedImageFormat format,
    bool is_overlay_candidate) {
  TransferableResource r;
  r.is_software = false;
  r.mailbox_holder.mailbox = mailbox;
  r.mailbox_holder.texture_target = texture_target;
  r.mailbox_holder.sync_token = sync_token;
  r.size = size;
  r.format = format;
  r.is_overlay_candidate = is_overlay_candidate;
  return r;
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
