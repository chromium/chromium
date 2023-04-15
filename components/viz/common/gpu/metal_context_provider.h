// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_GPU_METAL_CONTEXT_PROVIDER_H_
#define COMPONENTS_VIZ_COMMON_GPU_METAL_CONTEXT_PROVIDER_H_

#include <memory>
#include "components/metal_util/types.h"
#include "components/viz/common/viz_metal_context_provider_export.h"
#include "third_party/skia/include/gpu/graphite/ContextOptions.h"
#include "third_party/skia/include/gpu/graphite/mtl/MtlGraphiteTypes.h"

namespace gl {
class ProgressReporter;
}  // namespace gl

namespace skgpu::graphite {
class Context;
}  // namespace skgpu::graphite

namespace viz {

// The MetalContextProvider provides a Metal-backed GrContext.
class VIZ_METAL_CONTEXT_PROVIDER_EXPORT MetalContextProvider {
 public:
  // Create and return a MetalContextProvider if possible. May return nullptr
  // if no Metal devices exist.
  static std::unique_ptr<MetalContextProvider> Create();

  virtual ~MetalContextProvider() = default;

  virtual bool InitializeGraphiteContext(
      const skgpu::graphite::ContextOptions& options) = 0;

  virtual skgpu::graphite::Context* GetGraphiteContext() = 0;
  virtual metal::MTLDevicePtr GetMTLDevice() = 0;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_GPU_METAL_CONTEXT_PROVIDER_H_
