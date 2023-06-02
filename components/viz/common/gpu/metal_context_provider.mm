// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/gpu/metal_context_provider.h"

#import <Metal/Metal.h>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/mac/scoped_nsobject.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_macros.h"
#include "base/synchronization/waitable_event.h"
#include "base/time/time.h"
#include "components/metal_util/device.h"
#include "third_party/skia/include/gpu/graphite/Context.h"
#include "third_party/skia/include/gpu/graphite/mtl/MtlBackendContext.h"
#include "third_party/skia/include/gpu/graphite/mtl/MtlGraphiteUtils.h"

namespace viz {

namespace {

struct MetalContextProviderImpl : public MetalContextProvider {
 public:
  explicit MetalContextProviderImpl(
      base::scoped_nsprotocol<id<MTLDevice>> device)
      : device_(std::move(device)) {
    CHECK(device_);
  }

  MetalContextProviderImpl(const MetalContextProviderImpl&) = delete;
  MetalContextProviderImpl& operator=(const MetalContextProviderImpl&) = delete;
  ~MetalContextProviderImpl() override = default;

  bool InitializeGraphiteContext(
      const skgpu::graphite::ContextOptions& options) override {
    CHECK(!graphite_context_);
    CHECK(device_);
    skgpu::graphite::MtlBackendContext backend_context = {};
    backend_context.fDevice.retain(device_);
    backend_context.fQueue.reset([device_ newCommandQueue]);
    graphite_context_ =
        skgpu::graphite::ContextFactory::MakeMetal(backend_context, options);
    if (!graphite_context_) {
      DLOG(ERROR) << "Failed to create Graphite Context for Metal";
      return false;
    }
    return true;
  }

  skgpu::graphite::Context* GetGraphiteContext() override {
    return graphite_context_.get();
  }

  metal::MTLDevicePtr GetMTLDevice() override { return device_.get(); }

 private:
  base::scoped_nsprotocol<id<MTLDevice>> device_;
  std::unique_ptr<skgpu::graphite::Context> graphite_context_;
};

}  // namespace

// static
std::unique_ptr<MetalContextProvider> MetalContextProvider::Create() {
  // First attempt to find a low power device to use.
  base::scoped_nsprotocol<id<MTLDevice>> device(metal::CreateDefaultDevice());
  if (!device) {
    DLOG(ERROR) << "Failed to find MTLDevice.";
    return nullptr;
  }
  return std::make_unique<MetalContextProviderImpl>(std::move(device));
}

}  // namespace viz
