// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_GPU_METAL_CONTEXT_PROVIDER_H_
#define COMPONENTS_VIZ_COMMON_GPU_METAL_CONTEXT_PROVIDER_H_

#include <memory>

#include "components/viz/common/viz_metal_context_provider_export.h"
#include "third_party/skia/include/gpu/graphite/ContextOptions.h"
#include "third_party/skia/include/gpu/graphite/mtl/MtlGraphiteTypes.h"

#if __OBJC__
@protocol MTLDevice;
#endif  // __OBJC__

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

  MetalContextProvider(const MetalContextProvider&) = delete;
  MetalContextProvider& operator=(const MetalContextProvider&) = delete;
  ~MetalContextProvider();

  bool InitializeGraphiteContext(
      const skgpu::graphite::ContextOptions& options);

  skgpu::graphite::Context* GetGraphiteContext();

#if __OBJC__
  id<MTLDevice> GetMTLDevice();
#endif  // __OBJC__

 private:
#if __OBJC__
  explicit MetalContextProvider(id<MTLDevice> device);
#endif  // __OBJC__

  struct ObjCStorage;
  std::unique_ptr<ObjCStorage> objc_storage_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_GPU_METAL_CONTEXT_PROVIDER_H_
