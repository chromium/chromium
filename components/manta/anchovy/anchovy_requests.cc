// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/manta/anchovy/anchovy_requests.h"

#include <utility>

namespace manta::anchovy {

ImageDescriptionBatchEntry::ImageDescriptionBatchEntry(
    std::string tag,
    const std::vector<uint8_t>& bytes)
    : tag(std::move(tag)), image_bytes(bytes) {}

ImageDescriptionBatchEntry::ImageDescriptionBatchEntry(
    ImageDescriptionBatchEntry&& other) noexcept = default;

ImageDescriptionBatchEntry& ImageDescriptionBatchEntry::operator=(
    ImageDescriptionBatchEntry&& other) noexcept = default;

ImageDescriptionBatchEntry::~ImageDescriptionBatchEntry() = default;

ImageDescriptionBatchRequest::ImageDescriptionBatchRequest(std::string lang_tag)
    : lang_tag(std::move(lang_tag)) {}

ImageDescriptionBatchRequest::ImageDescriptionBatchRequest(
    ImageDescriptionBatchRequest&& other) noexcept = default;

ImageDescriptionBatchRequest& ImageDescriptionBatchRequest::operator=(
    ImageDescriptionBatchRequest&& other) noexcept = default;

ImageDescriptionBatchRequest::~ImageDescriptionBatchRequest() = default;

}  // namespace manta::anchovy
