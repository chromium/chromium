// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/display_resource_provider_gl.h"

namespace viz {

DisplayResourceProviderGL::DisplayResourceProviderGL(
    ContextProvider* compositor_context_provider,
    SharedBitmapManager* shared_bitmap_manager,
    bool enable_shared_images)
    : DisplayResourceProvider(DisplayResourceProvider::kGpu,
                              compositor_context_provider,
                              shared_bitmap_manager,
                              enable_shared_images) {}

}  // namespace viz
