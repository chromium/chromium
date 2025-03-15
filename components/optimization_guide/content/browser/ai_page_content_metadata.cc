// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/content/browser/ai_page_content_metadata.h"

namespace optimization_guide {

FrameMetadata::FrameMetadata() = default;
FrameMetadata::~FrameMetadata() = default;
FrameMetadata::FrameMetadata(FrameMetadata&& other) = default;
FrameMetadata& FrameMetadata::operator=(FrameMetadata&& other) = default;

AIPageContentMetadata::AIPageContentMetadata() = default;
AIPageContentMetadata::~AIPageContentMetadata() = default;
AIPageContentMetadata::AIPageContentMetadata(AIPageContentMetadata&& other) =
    default;
AIPageContentMetadata& AIPageContentMetadata::operator=(
    AIPageContentMetadata&& other) = default;

}  // namespace optimization_guide
