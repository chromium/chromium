// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/gpu/dawn_context_provider.h"

#include <dawn/dawn_proc.h>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "build/build_config.h"

namespace viz {

namespace {

dawn_native::BackendType GetDefaultBackendType() {
#if defined(OS_WIN)
  return dawn_native::BackendType::D3D12;
#elif defined(OS_LINUX)
  return dawn_native::BackendType::Vulkan;
#else
  NOTREACHED();
  return dawn_native::BackendType::Null;
#endif
}

}  // namespace

std::unique_ptr<DawnContextProvider> DawnContextProvider::Create() {
  auto context_provider = base::WrapUnique(new DawnContextProvider());
  if (!context_provider->IsValid())
    return nullptr;
  return context_provider;
}

DawnContextProvider::DawnContextProvider() {
  // TODO(sgilhuly): This may return a GPU that is not the active one. Currently
  // the only known way to avoid this is platform-specific; e.g. on Mac, create
  // a Dawn device, get the actual Metal device from it, and compare against
  // MTLCreateSystemDefaultDevice().
  device_ = CreateDevice(GetDefaultBackendType());
  if (device_)
    gr_context_ = GrContext::MakeDawn(device_);
}

DawnContextProvider::~DawnContextProvider() = default;

dawn::Device DawnContextProvider::CreateDevice(dawn_native::BackendType type) {
  instance_.DiscoverDefaultAdapters();
  DawnProcTable backend_procs = dawn_native::GetProcs();
  dawnProcSetProcs(&backend_procs);

  std::vector<dawn_native::Adapter> adapters = instance_.GetAdapters();
  for (dawn_native::Adapter adapter : adapters) {
    if (adapter.GetBackendType() == type)
      return adapter.CreateDevice();
  }
  return nullptr;
}

}  // namespace viz
