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
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_macros.h"
#include "base/synchronization/waitable_event.h"
#include "base/time/time.h"
#include "components/metal_util/device.h"
#include "third_party/skia/include/gpu/graphite/Context.h"
#include "third_party/skia/include/gpu/graphite/mtl/MtlBackendContext.h"
#include "third_party/skia/include/gpu/graphite/mtl/MtlGraphiteUtils.h"

namespace viz {

struct MetalContextProvider::ObjCStorage {
  id<MTLDevice> __strong device;
  std::unique_ptr<skgpu::graphite::Context> graphite_context;
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
  objc_storage_->graphite_context =
      skgpu::graphite::ContextFactory::MakeMetal(backend_context, options);
  if (!objc_storage_->graphite_context) {
    DLOG(ERROR) << "Failed to create Graphite Context for Metal";
    return false;
  }

  return true;
}

skgpu::graphite::Context* MetalContextProvider::GetGraphiteContext() {
  return objc_storage_->graphite_context.get();
}

id<MTLDevice> MetalContextProvider::GetMTLDevice() {
  return objc_storage_->device;
}

}  // namespace viz
