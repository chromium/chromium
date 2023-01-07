// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_DISPLAY_RESOURCE_PROVIDER_NULL_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_DISPLAY_RESOURCE_PROVIDER_NULL_H_

#include <vector>

#include "components/viz/service/display/display_resource_provider.h"
#include "components/viz/service/viz_service_export.h"

namespace viz {

// DisplayResourceProvider implementation used with NullRenderer.
class VIZ_SERVICE_EXPORT DisplayResourceProviderNull
    : public DisplayResourceProvider {
 public:
  DisplayResourceProviderNull();
  ~DisplayResourceProviderNull() override;

 private:
  // DisplayResourceProvider overrides:
  std::vector<ReturnedResource> DeleteAndReturnUnusedResourcesToChildImpl(
      Child& child_info,
      DeleteStyle style,
      const std::vector<ResourceId>& unused) override;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_DISPLAY_RESOURCE_PROVIDER_NULL_H_
