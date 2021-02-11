// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_DISPLAY_RESOURCE_PROVIDER_GL_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_DISPLAY_RESOURCE_PROVIDER_GL_H_

#include "components/viz/service/display/display_resource_provider.h"
#include "components/viz/service/viz_service_export.h"

namespace viz {

// DisplayResourceProvider implementation used with GLRenderer.
class VIZ_SERVICE_EXPORT DisplayResourceProviderGL
    : public DisplayResourceProvider {
 public:
  DisplayResourceProviderGL(ContextProvider* compositor_context_provider,
                            SharedBitmapManager* shared_bitmap_manager,
                            bool enable_shared_images = true);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_DISPLAY_RESOURCE_PROVIDER_GL_H_
