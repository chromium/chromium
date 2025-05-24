// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_TEST_RESOURCE_FACTORY_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_TEST_RESOURCE_FACTORY_H_

#include <memory>

#include "cc/test/fake_output_surface_client.h"
#include "components/viz/common/resources/resource_id.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "components/viz/service/display/display_resource_provider_skia.h"

namespace gfx {
class Size;
}  // namespace gfx

namespace viz {
class SkiaOutputSurface;
class ClientResourceProvider;
class TestContextProvider;

class TestResourceFactory {
 public:
  TestResourceFactory();

  TestResourceFactory(const TestResourceFactory&) = delete;
  TestResourceFactory& operator=(const TestResourceFactory&) = delete;

  ~TestResourceFactory();

  struct TestResourceContext {
    bool is_overlay_candidate = false;
    bool is_low_latency_rendering = false;
  };

  ResourceId CreateResource(const gfx::Size& size,
                            const TestResourceContext& resource_context,
                            SharedImageFormat format,
                            SurfaceId test_surface_id);

  DisplayResourceProvider* resource_provider() {
    return display_resource_provider_.get();
  }

 private:
  std::unique_ptr<SkiaOutputSurface> output_surface_;
  cc::FakeOutputSurfaceClient output_surface_client_;
  std::unique_ptr<DisplayResourceProviderSkia> display_resource_provider_;
  std::optional<DisplayResourceProviderSkia::LockSetForExternalUse>
      lock_set_for_external_use_;
  scoped_refptr<TestContextProvider> client_context_provider_;
  std::unique_ptr<ClientResourceProvider> client_resource_provider_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_TEST_RESOURCE_FACTORY_H_
