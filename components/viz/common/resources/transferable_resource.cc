// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/resources/transferable_resource.h"

#include "base/feature_list.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "base/values.h"
#include "components/viz/common/features.h"
#include "components/viz/common/resources/returned_resource.h"
#include "gpu/command_buffer/client/client_shared_image.h"

namespace viz {

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
  resource.format = shared_image->format();
  resource.set_texture_target(shared_image->GetTextureTarget());
  resource.size = shared_image->size();

  // Passed in format must be either single or multiplane and not default set.
  CHECK(resource.format.is_single_plane() || resource.format.is_multi_plane());
  resource.is_overlay_candidate = override.is_overlay_candidate.value_or(
      shared_image->usage().Has(gpu::SHARED_IMAGE_USAGE_SCANOUT));
  resource.color_space =
      override.color_space.value_or(shared_image->color_space());
  resource.origin = override.origin.value_or(shared_image->surface_origin());
  SkAlphaType alpha_type =
      override.alpha_type.value_or(shared_image->alpha_type());
  resource.alpha_type = alpha_type;

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

void TransferableResource::AsValueInto(
    base::trace_event::TracedValue* value) const {
  // Skip |id| because it's different between client and viz.
  value->SetBoolean("is_software", GetIsSoftware());
  value->SetString("memory_buffer_id", memory_buffer_id_.ToDebugString());
  value->SetString("sync_token", sync_token_.ToDebugString());
  value->SetInteger("texture_target", texture_target());
  value->SetString("size", GetSize().ToString());
  value->SetString("format", GetFormat().ToString());
  value->SetString("color_space", GetColorSpace().ToString());
  value->SetString("hdr_metadata", hdr_metadata.ToString());
  value->SetBoolean("is_overlay_candidate", GetIsOverlayCandidate());
  value->SetBoolean("is_low_latency_rendering", is_low_latency_rendering);
  value->SetInteger("synchronization_type",
                    static_cast<int>(synchronization_type));
#if BUILDFLAG(IS_ANDROID)
  if (ycbcr_info) {
    value->BeginDictionary("ycbcr_info");
    ycbcr_info->AsValueInto(value);
    value->EndDictionary();
  }
  value->SetBoolean("is_backed_by_surface_view", is_backed_by_surface_view);
#endif
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_WIN)
  value->SetBoolean("wants_promotion_hint", wants_promotion_hint);
#endif
  value->SetBoolean("needs_detiling", needs_detiling);
  value->SetInteger("origin", static_cast<int>(GetOrigin()));
  value->SetInteger("alpha_type", static_cast<int>(GetAlphaType()));
  value->SetInteger("resource_source", static_cast<int>(resource_source));
}

}  // namespace viz
