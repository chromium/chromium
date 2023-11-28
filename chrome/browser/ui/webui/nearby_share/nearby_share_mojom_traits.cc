// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/nearby_share/nearby_share_mojom_traits.h"

#include "base/notreached.h"
#include "chrome/browser/nearby_sharing/attachment.h"
#include "chrome/browser/nearby_sharing/file_attachment.h"
#include "chrome/browser/nearby_sharing/text_attachment.h"
#include "mojo/public/cpp/bindings/optional_as_pointer.h"

namespace mojo {

// static
const base::UnguessableToken&
StructTraits<nearby_share::mojom::ShareTargetDataView, ShareTarget>::id(
    const ShareTarget& share_target) {
  return share_target.id;
}

// static
const std::string&
StructTraits<nearby_share::mojom::ShareTargetDataView, ShareTarget>::name(
    const ShareTarget& share_target) {
  return share_target.device_name;
}

// static
nearby_share::mojom::ShareTargetType
StructTraits<nearby_share::mojom::ShareTargetDataView, ShareTarget>::type(
    const ShareTarget& share_target) {
  return share_target.type;
}

// static
mojo::OptionalAsPointer<const GURL>
StructTraits<nearby_share::mojom::ShareTargetDataView, ShareTarget>::image_url(
    const ShareTarget& share_target) {
  return mojo::OptionalAsPointer(share_target.image_url &&
                                         share_target.image_url->is_valid()
                                     ? &share_target.image_url.value()
                                     : nullptr);
}

// static
nearby_share::mojom::PayloadPreviewPtr
StructTraits<nearby_share::mojom::ShareTargetDataView,
             ShareTarget>::payload_preview(const ShareTarget& share_target) {
  // TODO(crbug.com/1158627): Extract this which is very similar to logic in
  // NearbyPerSessionDiscoveryManager.
  nearby_share::mojom::PayloadPreviewPtr payload_preview =
      nearby_share::mojom::PayloadPreview::New();
  payload_preview->file_count = share_target.file_attachments.size();
  payload_preview->share_type = nearby_share::mojom::ShareType::kText;

  // Retrieve the attachment that we'll use for the default description.
  const Attachment* attachment = nullptr;
  if (!share_target.file_attachments.empty()) {
    attachment = &share_target.file_attachments[0];
  } else if (!share_target.text_attachments.empty()) {
    attachment = &share_target.text_attachments[0];
  } else if (!share_target.wifi_credentials_attachments.empty()) {
    attachment = &share_target.wifi_credentials_attachments[0];
  }

  // If there are no attachments, return an empty text preview.
  if (!attachment) {
    return payload_preview;
  }

  payload_preview->description = attachment->GetDescription();

  // Determine the share type.
  if (payload_preview->file_count > 1) {
    payload_preview->share_type =
        nearby_share::mojom::ShareType::kMultipleFiles;
  } else {
    payload_preview->share_type = attachment->GetShareType();
  }

  return payload_preview;
}

// static
bool mojo::StructTraits<nearby_share::mojom::ShareTargetDataView,
                        ShareTarget>::for_self_share(const ShareTarget&
                                                         share_target) {
  return share_target.for_self_share;
}

// static
bool StructTraits<nearby_share::mojom::ShareTargetDataView, ShareTarget>::Read(
    nearby_share::mojom::ShareTargetDataView data,
    ShareTarget* out) {
  return data.ReadId(&out->id) && data.ReadName(&out->device_name) &&
         data.ReadType(&out->type);
}

}  // namespace mojo
