// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/manta/anchovy/anchovy_requests.h"

namespace manta::anchovy {
ImageDescriptionRequest::ImageDescriptionRequest(
    std::string source_id,
    std::string lang_tag,
    const std::vector<uint8_t>& bytes)
    : image_bytes(bytes), lang_tag(lang_tag), source_id(source_id) {}
}  // namespace manta::anchovy
