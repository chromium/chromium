// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_GPU_DAWN_CONTEXT_PROVIDER_H_
#define COMPONENTS_VIZ_COMMON_GPU_DAWN_CONTEXT_PROVIDER_H_

#include <dawn_native/DawnNative.h>

#include "base/macros.h"
#include "components/viz/common/viz_dawn_context_provider_export.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "third_party/skia/include/gpu/dawn/GrDawnTypes.h"

class GrContext;

namespace viz {

class VIZ_DAWN_CONTEXT_PROVIDER_EXPORT DawnContextProvider {
 public:
  static std::unique_ptr<DawnContextProvider> Create();
  ~DawnContextProvider();

  dawn::Device GetDevice() { return device_; }
  GrContext* GetGrContext() { return gr_context_.get(); }
  bool IsValid() { return !!gr_context_; }

 private:
  DawnContextProvider();

  dawn::Device CreateDevice(dawn_native::BackendType type);

  dawn_native::Instance instance_;
  dawn::Device device_;
  sk_sp<GrContext> gr_context_;

  DISALLOW_COPY_AND_ASSIGN(DawnContextProvider);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_GPU_DAWN_CONTEXT_PROVIDER_H_
