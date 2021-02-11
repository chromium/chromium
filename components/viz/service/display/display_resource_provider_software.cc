// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/display_resource_provider_software.h"

namespace viz {

DisplayResourceProviderSoftware::DisplayResourceProviderSoftware(
    SharedBitmapManager* shared_bitmap_manager)
    : DisplayResourceProvider(DisplayResourceProvider::kSoftware,
                              /*compositor_context_provider=*/nullptr,
                              shared_bitmap_manager,
                              /*enable_shared_images=*/true) {}

}  // namespace viz
