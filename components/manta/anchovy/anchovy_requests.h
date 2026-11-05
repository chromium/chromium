// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MANTA_ANCHOVY_ANCHOVY_REQUESTS_H_
#define COMPONENTS_MANTA_ANCHOVY_ANCHOVY_REQUESTS_H_

#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/memory/raw_ref.h"

namespace manta::anchovy {

// Description type strings used in the parsed Manta response dictionary.
inline constexpr char kTypePrimaryCaption[] = "PRIMARY_CAPTION";
inline constexpr char kTypeSecondaryCaption[] = "SECONDARY_CAPTION";
inline constexpr char kTypeCaption[] = "CAPTION";
inline constexpr char kTypeLabel[] = "LABEL";
inline constexpr char kTypeOcr[] = "OCR";

struct COMPONENT_EXPORT(MANTA) ImageDescriptionRequest {
  ImageDescriptionRequest(std::string source_id,
                          std::string lang_tag,
                          const std::vector<uint8_t>& bytes);

  ImageDescriptionRequest(ImageDescriptionRequest&& other) = default;

  ImageDescriptionRequest(const ImageDescriptionRequest&) = delete;
  ImageDescriptionRequest& operator=(const ImageDescriptionRequest&) = delete;

  const raw_ref<const std::vector<uint8_t>> image_bytes;
  const std::string lang_tag;
  const std::string source_id;
};
}  // namespace manta::anchovy

#endif  // COMPONENTS_MANTA_ANCHOVY_ANCHOVY_REQUESTS_H_
