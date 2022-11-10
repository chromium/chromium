// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/gpu/metal_context_provider.h"

#import <Metal/Metal.h>

#include "base/bind.h"
#include "base/logging.h"
#include "base/mac/scoped_nsobject.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_macros.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/metal_util/device.h"
#include "third_party/skia/include/gpu/GrDirectContext.h"

namespace viz {

namespace {

struct MetalContextProviderImpl : public MetalContextProvider {
 public:
  explicit MetalContextProviderImpl(id<MTLDevice> device,
                                    const GrContextOptions& context_options) {
    device_.reset(device, base::scoped_policy::RETAIN);
    command_queue_.reset([device_ newCommandQueue]);

    gr_context_ =
        GrDirectContext::MakeMetal(device_, command_queue_, context_options);
    DCHECK(gr_context_);
  }

  MetalContextProviderImpl(const MetalContextProviderImpl&) = delete;
  MetalContextProviderImpl& operator=(const MetalContextProviderImpl&) = delete;
  ~MetalContextProviderImpl() override = default;

  void SetProgressReporter(gl::ProgressReporter* progress_reporter) override {}
  GrDirectContext* GetGrContext() override { return gr_context_.get(); }
  metal::MTLDevicePtr GetMTLDevice() override { return device_.get(); }

 private:
  base::scoped_nsprotocol<id<MTLDevice>> device_;
  base::scoped_nsprotocol<id<MTLCommandQueue>> command_queue_;
  sk_sp<GrDirectContext> gr_context_;
};

}  // namespace

// static
std::unique_ptr<MetalContextProvider> MetalContextProvider::Create(
    const GrContextOptions& context_options) {
  // First attempt to find a low power device to use.
  base::scoped_nsprotocol<id<MTLDevice>> device_to_use(
      metal::CreateDefaultDevice());
  if (!device_to_use) {
    DLOG(ERROR) << "Failed to find MTLDevice.";
    return nullptr;
  }
  return std::make_unique<MetalContextProviderImpl>(device_to_use.get(),
                                                    context_options);
}

}  // namespace viz
