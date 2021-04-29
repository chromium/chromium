// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_TEST_GPU_HOST_IMPL_TEST_API_H_
#define COMPONENTS_VIZ_TEST_GPU_HOST_IMPL_TEST_API_H_

#include "mojo/public/cpp/bindings/remote.h"
#include "services/viz/privileged/mojom/gl/gpu_service.mojom.h"

namespace viz {
class GpuHostImpl;

class GpuHostImplTestApi {
 public:
  explicit GpuHostImplTestApi(GpuHostImpl* gpu_host);
  ~GpuHostImplTestApi();

  // Waits until all messages to the mojo::Remote<mojom::GpuService> have been
  // processed.
  void FlushRemoteForTesting();
  void SetGpuService(mojo::Remote<mojom::GpuService> gpu_service);

 private:
  GpuHostImpl* gpu_host_;

  DISALLOW_COPY_AND_ASSIGN(GpuHostImplTestApi);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_TEST_GPU_HOST_IMPL_TEST_API_H_
