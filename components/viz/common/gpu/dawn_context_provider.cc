// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/gpu/dawn_context_provider.h"

#include <memory>
#include <vector>

#include "base/check_op.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "third_party/dawn/include/dawn/dawn_proc.h"
#include "third_party/skia/include/gpu/graphite/Context.h"
#include "third_party/skia/include/gpu/graphite/dawn/DawnBackendContext.h"
#include "third_party/skia/include/gpu/graphite/dawn/DawnUtils.h"

namespace viz {

namespace {

wgpu::BackendType GetDefaultBackendType() {
#if BUILDFLAG(IS_WIN)
  return wgpu::BackendType::D3D12;
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  return wgpu::BackendType::Vulkan;
#elif BUILDFLAG(IS_MAC)
  return wgpu::BackendType::Metal;
#else
  NOTREACHED();
  return wgpu::BackendType::Null;
#endif
}

}  // namespace

std::unique_ptr<DawnContextProvider> DawnContextProvider::Create() {
  auto context_provider = base::WrapUnique(new DawnContextProvider());
  if (!context_provider->GetDevice()) {
    return nullptr;
  }
  return context_provider;
}

DawnContextProvider::DawnContextProvider() {
  // TODO(rivr): This may return a GPU that is not the active one. Currently
  // the only known way to avoid this is platform-specific; e.g. on Mac, create
  // a Dawn device, get the actual Metal device from it, and compare against
  // MTLCreateSystemDefaultDevice().
  device_ = CreateDevice(GetDefaultBackendType());
}

DawnContextProvider::~DawnContextProvider() = default;

wgpu::Device DawnContextProvider::CreateDevice(wgpu::BackendType type) {
  instance_.DiscoverDefaultAdapters();
  DawnProcTable backend_procs = dawn::native::GetProcs();
  dawnProcSetProcs(&backend_procs);

  // If a new toggle is added here, ForceDawnTogglesForSkia() which collects
  // info for about:gpu should be updated as well.

  // Disable validation in non-DCHECK builds.
  dawn::native::DawnDeviceDescriptor descriptor;
#if !DCHECK_IS_ON()
  descriptor.forceEnabledToggles.push_back("disable_robustness");
  descriptor.forceEnabledToggles.push_back("skip_validation");
  descriptor.forceDisabledToggles.push_back("lazy_clear_resource_on_first_use");
#endif
  descriptor.requiredFeatures.push_back("dawn-internal-usages");
  descriptor.requiredFeatures.push_back("depth-clip-control");
  descriptor.requiredFeatures.push_back("depth32float-stencil8");

  std::vector<dawn::native::Adapter> adapters = instance_.GetAdapters();
  for (dawn::native::Adapter adapter : adapters) {
    wgpu::AdapterProperties properties;
    adapter.GetProperties(&properties);
    if (properties.backendType == type)
      return adapter.CreateDevice(&descriptor);
  }
  return nullptr;
}

bool DawnContextProvider::InitializeGraphiteContext(
    const skgpu::graphite::ContextOptions& options) {
  CHECK(!graphite_context_);

  if (device_) {
    skgpu::graphite::DawnBackendContext backend_context;
    backend_context.fDevice = device_;
    backend_context.fQueue = device_.GetQueue();
    graphite_context_ =
        skgpu::graphite::ContextFactory::MakeDawn(backend_context, options);
  }

  return !!graphite_context_;
}

}  // namespace viz
