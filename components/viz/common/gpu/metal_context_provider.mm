// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/gpu/metal_context_provider.h"

#include <Foundation/Foundation.h>
#import <Metal/Metal.h>

#include <memory>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/synchronization/waitable_event.h"
#include "base/time/time.h"
#include "components/metal_util/device.h"
#include "gpu/command_buffer/service/graphite_shared_context.h"
#include "gpu/config/gpu_finch_features.h"
#include "third_party/skia/include/gpu/graphite/Context.h"
#include "third_party/skia/include/gpu/graphite/mtl/MtlBackendContext.h"
#include "third_party/skia/include/gpu/graphite/mtl/MtlGraphiteUtils.h"

namespace viz {

struct MetalContextProvider::ObjCStorage {
  id<MTLDevice> __strong device;
  std::unique_ptr<skgpu::graphite::Context> graphite_context;
  std::unique_ptr<gpu::GraphiteSharedContext> graphite_shared_context;
};

MetalContextProvider::MetalContextProvider(id<MTLDevice> device)
    : objc_storage_(std::make_unique<ObjCStorage>()) {
  objc_storage_->device = device;
  CHECK(objc_storage_->device);
}

MetalContextProvider::~MetalContextProvider() = default;

// static
std::unique_ptr<MetalContextProvider> MetalContextProvider::Create() {
  // First attempt to find a low power device to use.
  id<MTLDevice> device = metal::GetDefaultDevice();
  if (!device) {
    DLOG(ERROR) << "Failed to find MTLDevice.";
    return nullptr;
  }
  return base::WrapUnique(new MetalContextProvider(std::move(device)));
}

bool MetalContextProvider::InitializeGraphiteContext(
    const skgpu::graphite::ContextOptions& options) {
  CHECK(!objc_storage_->graphite_context);
  CHECK(objc_storage_->device);

  skgpu::graphite::MtlBackendContext backend_context = {};
  // ARC note: MtlBackendContext contains two owning smart pointers of CFTypeRef
  // so give them owning references.
  backend_context.fDevice.reset(CFBridgingRetain(objc_storage_->device));
  backend_context.fQueue.reset(
      CFBridgingRetain([objc_storage_->device newCommandQueue]));

  std::unique_ptr<skgpu::graphite::Context> graphite_context =
      skgpu::graphite::ContextFactory::MakeMetal(backend_context, options);
  if (!graphite_context) {
    DLOG(ERROR) << "Failed to create Graphite Context for Metal";
    return false;
  }

  if (features::IsGraphiteContextThreadSafe()) {
    objc_storage_->graphite_shared_context =
        std::make_unique<gpu::GraphiteSharedContext>(
            std::move(graphite_context), /*is_thread_safe=*/true);

    // TODO(crbug.com/407874799): Return false for now. The feature is
    // incomplete. Do not enable kGraphiteContextIsThreadSafe.
    return false;
  } else {
    objc_storage_->graphite_context = std::move(graphite_context);
    return true;
  }
}

skgpu::graphite::Context* MetalContextProvider::GetGraphiteContext() {
  return objc_storage_->graphite_context.get();
}

gpu::GraphiteSharedContext* MetalContextProvider::GetGraphiteSharedContext()
    const {
  return objc_storage_->graphite_shared_context.get();
}

int32_t MetalContextProvider::GetMaxTextureSize() const {
#if BUILDFLAG(IS_IOS)
  if (id<MTLDevice> device = GetMTLDevice()) {
    if ([device supportsFamily:MTLGPUFamilyApple3]) {
      return 16384;
    }
  }
  return 8192;
#elif BUILDFLAG(IS_MAC)
  return 16384;
#else
  NOTREACHED();
#endif
}

id<MTLDevice> MetalContextProvider::GetMTLDevice() const {
  return objc_storage_->device;
}

}  // namespace viz
