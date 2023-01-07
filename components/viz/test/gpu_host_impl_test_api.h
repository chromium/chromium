// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_TEST_GPU_HOST_IMPL_TEST_API_H_
#define COMPONENTS_VIZ_TEST_GPU_HOST_IMPL_TEST_API_H_

#include "base/memory/raw_ptr.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/viz/privileged/mojom/gl/gpu_service.mojom.h"

namespace viz {
class GpuHostImpl;

class GpuHostImplTestApi {
 public:
  explicit GpuHostImplTestApi(GpuHostImpl* gpu_host);

  GpuHostImplTestApi(const GpuHostImplTestApi&) = delete;
  GpuHostImplTestApi& operator=(const GpuHostImplTestApi&) = delete;

  ~GpuHostImplTestApi();

  // Waits until all messages to the mojo::Remote<mojom::GpuService> have been
  // processed.
  void FlushRemoteForTesting();
  void SetGpuService(mojo::Remote<mojom::GpuService> gpu_service);

 private:
  raw_ptr<GpuHostImpl, DanglingUntriaged> gpu_host_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_TEST_GPU_HOST_IMPL_TEST_API_H_
