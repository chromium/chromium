// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/quads/compositor_frame.h"

#include "base/containers/adapters.h"

namespace viz {

CompositorFrame::CompositorFrame() = default;

CompositorFrame::CompositorFrame(CompositorFrame&& other) = default;

CompositorFrame::~CompositorFrame() = default;

CompositorFrame& CompositorFrame::operator=(CompositorFrame&& other) = default;

bool CompositorFrame::HasCopyOutputRequests() const {
  // Iterate the RenderPasses back-to-front, because CopyOutputRequests tend to
  // be made on the later passes.
  for (const auto& pass : base::Reversed(render_pass_list)) {
    if (!pass->copy_requests.empty()) {
      return true;
    }
  }
  return false;
}

}  // namespace viz
