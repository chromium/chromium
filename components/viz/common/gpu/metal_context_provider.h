// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_GPU_METAL_CONTEXT_PROVIDER_H_
#define COMPONENTS_VIZ_COMMON_GPU_METAL_CONTEXT_PROVIDER_H_

#include <memory>
#include "components/metal_util/types.h"
#include "components/viz/common/viz_metal_context_provider_export.h"

class GrContext;

namespace gl {
class ProgressReporter;
}  // namespace gl

namespace viz {

// The MetalContextProvider provides a Metal-backed GrContext.
class VIZ_METAL_CONTEXT_PROVIDER_EXPORT MetalContextProvider {
 public:
  // Create and return a MetalContextProvider if possible. May return nullptr
  // if no Metal devices exist.
  static std::unique_ptr<MetalContextProvider> Create();
  virtual ~MetalContextProvider() {}

  virtual GrContext* GetGrContext() = 0;
  virtual metal::MTLDevicePtr GetMTLDevice() = 0;

  // Set the progress reported used to prevent watchdog timeouts during longer
  // sequences of Metal API calls. It is guaranteed that no further calls to
  // |progress_reporter| will be made after |this| is destroyed.
  virtual void SetProgressReporter(gl::ProgressReporter* progress_reporter) = 0;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_GPU_METAL_CONTEXT_PROVIDER_H_
