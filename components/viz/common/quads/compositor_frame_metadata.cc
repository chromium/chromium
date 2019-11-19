// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/quads/compositor_frame_metadata.h"

namespace viz {

CompositorFrameMetadata::CompositorFrameMetadata() = default;

CompositorFrameMetadata::CompositorFrameMetadata(
    CompositorFrameMetadata&& other) = default;

CompositorFrameMetadata::~CompositorFrameMetadata() = default;

CompositorFrameMetadata& CompositorFrameMetadata::operator=(
    CompositorFrameMetadata&& other) = default;

CompositorFrameMetadata CompositorFrameMetadata::Clone() const {
  CompositorFrameMetadata metadata(*this);
  return metadata;
}

CompositorFrameMetadata::CompositorFrameMetadata(
    const CompositorFrameMetadata& other) = default;

}  // namespace viz
