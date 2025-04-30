// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/content/browser/page_context_eligibility_api.h"

extern "C" {

namespace optimization_guide {

MetaTag::MetaTag(const std::string& name, const std::string& content)
    : name(name), content(content) {}
MetaTag::MetaTag(const MetaTag& other) = default;
MetaTag& MetaTag::MetaTag::operator=(const MetaTag& other) = default;
MetaTag::~MetaTag() = default;

FrameMetadata::FrameMetadata(const std::string& host,
                             const std::string& path,
                             std::vector<MetaTag> meta_tags)
    : host(host), path(path), meta_tags(std::move(meta_tags)) {}
FrameMetadata::FrameMetadata(const FrameMetadata& other) = default;
FrameMetadata& FrameMetadata::FrameMetadata::operator=(
    const FrameMetadata& other) = default;
FrameMetadata::~FrameMetadata() = default;
}  // namespace optimization_guide

}  // extern "C"
