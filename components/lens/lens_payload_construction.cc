// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/containers/span.h"
#include "build/build_config.h"
#include "components/lens/lens_features.h"
#include "components/lens/lens_overlay_mime_type.h"
#include "third_party/lens_server_proto/lens_overlay_service_deps.pb.h"

#if !BUILDFLAG(IS_ANDROID)
#include "third_party/zstd/src/lib/zstd.h"  // nogncheck
#endif  // !BUILDFLAG(IS_ANDROID)

namespace lens {

// Returns the media type for the given mime type.
lens::LensOverlayRequestId::MediaType MimeTypeToMediaType(
    lens::MimeType mime_type,
    bool has_viewport_screenshot) {
  switch (mime_type) {
    case lens::MimeType::kPdf:
      return has_viewport_screenshot
                 ? lens::LensOverlayRequestId::MEDIA_TYPE_PDF_AND_IMAGE
                 : lens::LensOverlayRequestId::MEDIA_TYPE_PDF;
    case lens::MimeType::kAnnotatedPageContent:
      return has_viewport_screenshot
                 ? lens::LensOverlayRequestId::MEDIA_TYPE_WEBPAGE_AND_IMAGE
                 : lens::LensOverlayRequestId::MEDIA_TYPE_WEBPAGE;
    case lens::MimeType::kImage:
      [[fallthrough]];
    default:
      return lens::LensOverlayRequestId::MEDIA_TYPE_DEFAULT_IMAGE;
  }
}

lens::ContentData::ContentType MimeTypeToContentType(
    lens::MimeType content_type) {
  switch (content_type) {
    case lens::MimeType::kPdf:
      return lens::ContentData::CONTENT_TYPE_PDF;
    case lens::MimeType::kHtml:
      return lens::ContentData::CONTENT_TYPE_INNER_HTML;
    case lens::MimeType::kPlainText:
      return lens::ContentData::CONTENT_TYPE_INNER_TEXT;
    case lens::MimeType::kUnknown:
      return lens::ContentData::CONTENT_TYPE_UNSPECIFIED;
    case lens::MimeType::kAnnotatedPageContent:
      return lens::ContentData::CONTENT_TYPE_ANNOTATED_PAGE_CONTENT;
    case lens::MimeType::kImage:
    case lens::MimeType::kVideo:
    case lens::MimeType::kAudio:
    case lens::MimeType::kJson:
      // These content types are not supported for the page content upload flow.
      NOTREACHED() << "Unsupported option in page content upload";
  }
}

// Compresses the given bytes using Zstd and store them into `dst_bytes`.
// Returns true if the compression is successful.
bool ZstdCompressBytes(base::span<const uint8_t> src_bytes,
                       std::string* dst_bytes) {
#if BUILDFLAG(IS_ANDROID)
  // Disable compression on Android due to binary size increase.
  return false;
#else
  CHECK(dst_bytes);
  size_t uncompressed_size = src_bytes.size();
  size_t buffer_bounds = ZSTD_compressBound(uncompressed_size);

  // Resize the output buffer to the upper bound of the compressed size.
  dst_bytes->resize(buffer_bounds);

  // Do the compression.
  const size_t compressed_size = ZSTD_compress(
      dst_bytes->data(), buffer_bounds, src_bytes.data(), uncompressed_size,
      lens::features::GetZstdCompressionLevel());

  if (ZSTD_isError(compressed_size)) {
    return false;
  }

  // Resize the output vector to the actual compressed size.
  dst_bytes->resize(compressed_size);
  return true;
#endif  // BUILDFLAG(IS_ANDROID)
}

}  // namespace lens
