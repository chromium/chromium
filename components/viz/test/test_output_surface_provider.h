// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_TEST_TEST_OUTPUT_SURFACE_PROVIDER_H_
#define COMPONENTS_VIZ_TEST_TEST_OUTPUT_SURFACE_PROVIDER_H_

#include <memory>

#include "components/viz/service/display_embedder/output_surface_provider.h"

namespace viz {

// Test implementation that creates a FakeOutputSurface.
class TestOutputSurfaceProvider : public OutputSurfaceProvider {
 public:
  TestOutputSurfaceProvider();
  ~TestOutputSurfaceProvider() override;

  // OutputSurfaceProvider implementation.
  std::unique_ptr<OutputSurface> CreateOutputSurface(
      gpu::SurfaceHandle surface_handle,
      bool gpu_compositing,
      mojom::DisplayClient* display_client,
      const RendererSettings& renderer_settings) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestOutputSurfaceProvider);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_TEST_TEST_OUTPUT_SURFACE_PROVIDER_H_
