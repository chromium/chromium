// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_GPU_DAWN_CONTEXT_PROVIDER_H_
#define COMPONENTS_VIZ_COMMON_GPU_DAWN_CONTEXT_PROVIDER_H_

#include <memory>

#include "components/viz/common/viz_dawn_context_provider_export.h"
#include "third_party/dawn/include/dawn/native/DawnNative.h"
#include "third_party/skia/include/gpu/GrDirectContext.h"
#include "third_party/skia/include/gpu/dawn/GrDawnTypes.h"

namespace viz {

class VIZ_DAWN_CONTEXT_PROVIDER_EXPORT DawnContextProvider {
 public:
  static std::unique_ptr<DawnContextProvider> Create();

  DawnContextProvider(const DawnContextProvider&) = delete;
  DawnContextProvider& operator=(const DawnContextProvider&) = delete;

  ~DawnContextProvider();

  wgpu::Device GetDevice() { return device_; }
  wgpu::Instance GetInstance() { return instance_.Get(); }
  GrDirectContext* GetGrContext() { return gr_context_.get(); }
  bool IsValid() { return !!gr_context_; }

 private:
  DawnContextProvider();

  wgpu::Device CreateDevice(wgpu::BackendType type);

  dawn::native::Instance instance_;
  wgpu::Device device_;
  sk_sp<GrDirectContext> gr_context_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_GPU_DAWN_CONTEXT_PROVIDER_H_
