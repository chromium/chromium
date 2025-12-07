// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LENS_LENS_PAYLOAD_CONSTRUCTION_H_
#define COMPONENTS_LENS_LENS_PAYLOAD_CONSTRUCTION_H_

#include <string>

#include "base/containers/span.h"
#include "components/lens/lens_overlay_mime_type.h"
#include "third_party/lens_server_proto/lens_overlay_service_deps.pb.h"

namespace lens {

lens::LensOverlayRequestId::MediaType MimeTypeToMediaType(
    lens::MimeType mime_type,
    bool has_viewport_screenshot);

lens::ContentData::ContentType MimeTypeToContentType(
    lens::MimeType content_type);

// Compresses the given bytes using Zstd and store them into `dst_bytes`.
// Returns true if the compression is successful.
bool ZstdCompressBytes(base::span<const uint8_t> src_bytes,
                       std::string* dst_bytes);

}  // namespace lens

#endif  // COMPONENTS_LENS_LENS_PAYLOAD_CONSTRUCTION_H_
