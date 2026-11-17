// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MANTA_ANCHOVY_ANCHOVY_REQUESTS_H_
#define COMPONENTS_MANTA_ANCHOVY_ANCHOVY_REQUESTS_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/memory/raw_ref.h"

namespace manta::anchovy {

// Key holding the per-tag annotation lists in a batched Manta response dict.
inline constexpr char kResultsByTagKey[] = "results_by_tag";

// Server-enforced maximum number of images in a single batched Manta request.
inline constexpr size_t kMaxBatchSize = 100;

// Description type strings used in the parsed Manta response dictionary.
inline constexpr char kTypePrimaryCaption[] = "PRIMARY_CAPTION";
inline constexpr char kTypeSecondaryCaption[] = "SECONDARY_CAPTION";
inline constexpr char kTypeCaption[] = "CAPTION";
inline constexpr char kTypeLabel[] = "LABEL";
inline constexpr char kTypeOcr[] = "OCR";

// Represents one tagged image payload within a batched description request.
// `tag` must be a non-empty valid UTF-8 string unique within the batch, and
// `image_bytes` must be non-empty caller-owned storage that outlives
// synchronous protobuf serialization.
struct COMPONENT_EXPORT(MANTA) ImageDescriptionBatchEntry {
  ImageDescriptionBatchEntry(std::string tag,
                             const std::vector<uint8_t>& bytes);
  // Disallow binding `raw_ref` to temporary or moved-from byte buffers that
  // would be destroyed before synchronous request serialization.
  ImageDescriptionBatchEntry(std::string tag,
                             const std::vector<uint8_t>&& bytes) = delete;
  ImageDescriptionBatchEntry(const ImageDescriptionBatchEntry&) = delete;
  ImageDescriptionBatchEntry& operator=(const ImageDescriptionBatchEntry&) =
      delete;
  ImageDescriptionBatchEntry(ImageDescriptionBatchEntry&& other) noexcept;
  ImageDescriptionBatchEntry& operator=(
      ImageDescriptionBatchEntry&& other) noexcept;
  ~ImageDescriptionBatchEntry();

  std::string tag;
  raw_ref<const std::vector<uint8_t>> image_bytes;
};

// Groups multiple images that share a single `lang_tag` into one Manta request.
// `images` must contain at least one entry.
struct COMPONENT_EXPORT(MANTA) ImageDescriptionBatchRequest {
  explicit ImageDescriptionBatchRequest(std::string lang_tag);
  ImageDescriptionBatchRequest(const ImageDescriptionBatchRequest&) = delete;
  ImageDescriptionBatchRequest& operator=(const ImageDescriptionBatchRequest&) =
      delete;
  ImageDescriptionBatchRequest(ImageDescriptionBatchRequest&& other) noexcept;
  ImageDescriptionBatchRequest& operator=(
      ImageDescriptionBatchRequest&& other) noexcept;
  ~ImageDescriptionBatchRequest();

  std::string lang_tag;
  std::vector<ImageDescriptionBatchEntry> images;
};

}  // namespace manta::anchovy

#endif  // COMPONENTS_MANTA_ANCHOVY_ANCHOVY_REQUESTS_H_
