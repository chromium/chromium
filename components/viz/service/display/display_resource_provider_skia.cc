// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/display_resource_provider_skia.h"

namespace viz {

DisplayResourceProviderSkia::DisplayResourceProviderSkia(
    SharedBitmapManager* shared_bitmap_manager)
    : DisplayResourceProvider(DisplayResourceProvider::kGpu,
                              /*compositor_context_provider=*/nullptr,
                              shared_bitmap_manager,
                              /*enable_shared_images=*/true) {}

}  // namespace viz
