// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_COMPOSITOR_FRAME_FUZZER_FUZZER_SOFTWARE_OUTPUT_SURFACE_PROVIDER_H_
#define COMPONENTS_VIZ_SERVICE_COMPOSITOR_FRAME_FUZZER_FUZZER_SOFTWARE_OUTPUT_SURFACE_PROVIDER_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/optional.h"
#include "components/viz/service/display_embedder/output_surface_provider.h"

namespace viz {

// OutputSurfaceProvider implementation that provides SoftwareOutputSurface
// (no-op by default, with an option to dump pixmap to a PNG for debugging).
class FuzzerSoftwareOutputSurfaceProvider : public OutputSurfaceProvider {
 public:
  explicit FuzzerSoftwareOutputSurfaceProvider(
      base::Optional<base::FilePath> png_dir_path);
  ~FuzzerSoftwareOutputSurfaceProvider() override;

  // OutputSurfaceProvider implementation.
  std::unique_ptr<OutputSurface> CreateOutputSurface(
      gpu::SurfaceHandle surface_handle,
      bool gpu_compositing,
      mojom::DisplayClient* display_client,
      const RendererSettings& renderer_settings) override;

 private:
  base::Optional<base::FilePath> png_dir_path_;

  DISALLOW_COPY_AND_ASSIGN(FuzzerSoftwareOutputSurfaceProvider);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_COMPOSITOR_FRAME_FUZZER_FUZZER_SOFTWARE_OUTPUT_SURFACE_PROVIDER_H_
