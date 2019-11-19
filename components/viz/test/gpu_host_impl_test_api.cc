// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/test/gpu_host_impl_test_api.h"

#include <algorithm>

#include "components/viz/host/gpu_host_impl.h"

namespace viz {

GpuHostImplTestApi::GpuHostImplTestApi(GpuHostImpl* gpu_host)
    : gpu_host_(gpu_host) {}

GpuHostImplTestApi::~GpuHostImplTestApi() = default;

void GpuHostImplTestApi::FlushRemoteForTesting() {
  gpu_host_->gpu_service_remote_.FlushForTesting();
}

void GpuHostImplTestApi::SetGpuService(
    mojo::Remote<mojom::GpuService> gpu_service) {
  gpu_host_->gpu_service_remote_ = std::move(gpu_service);
}

}  // namespace viz
