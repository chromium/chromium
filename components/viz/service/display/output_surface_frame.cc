// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/output_surface_frame.h"

namespace viz {

OutputSurfaceFrame::OutputSurfaceFrame() = default;

OutputSurfaceFrame::OutputSurfaceFrame(OutputSurfaceFrame&& other) = default;

OutputSurfaceFrame::~OutputSurfaceFrame() = default;

OutputSurfaceFrame& OutputSurfaceFrame::operator=(OutputSurfaceFrame&& other) =
    default;

}  // namespace viz
