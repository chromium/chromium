// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_GL_INFO_COLLECTION_GPU_SERVICE_IMPL_H_
#define COMPONENTS_VIZ_SERVICE_GL_INFO_COLLECTION_GPU_SERVICE_IMPL_H_

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "components/viz/service/viz_service_export.h"
#include "gpu/config/device_perf_info.h"
#include "gpu/config/gpu_info.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/viz/privileged/mojom/gl/info_collection_gpu_service.mojom.h"

namespace viz {
// This runs in the info-collection GPU process, and communicates with the gpu
// host (which is the window server) over the mojom APIs. This is responsible
// for setting up the connection to clients

class VIZ_SERVICE_EXPORT InfoCollectionGpuServiceImpl
    : public mojom::InfoCollectionGpuService {
 public:
  InfoCollectionGpuServiceImpl(
      scoped_refptr<base::SingleThreadTaskRunner> main_runner,
      scoped_refptr<base::SingleThreadTaskRunner> io_runner,
      const gpu::DevicePerfInfo& device_perf_info,
      const gpu::GPUInfo::GPUDevice& gpu_device,
      mojo::PendingReceiver<mojom::InfoCollectionGpuService> pending_receiver);

  InfoCollectionGpuServiceImpl(const InfoCollectionGpuServiceImpl&) = delete;
  InfoCollectionGpuServiceImpl& operator=(const InfoCollectionGpuServiceImpl&) =
      delete;

  ~InfoCollectionGpuServiceImpl() override;

  void GetGpuSupportedDirectXVersionAndDevicePerfInfo(
      GetGpuSupportedDirectXVersionAndDevicePerfInfoCallback callback) override;

  void GetGpuSupportedVulkanVersionInfo(
      GetGpuSupportedVulkanVersionInfoCallback callback) override;

 private:
  void BindOnIO(
      mojo::PendingReceiver<mojom::InfoCollectionGpuService> pending_receiver);

  void GetGpuSupportedDirectXVersionAndDevicePerfInfoOnMain(
      GetGpuSupportedDirectXVersionAndDevicePerfInfoCallback callback);

  void GetGpuSupportedVulkanVersionInfoOnMain(
      GetGpuSupportedVulkanVersionInfoCallback callback);

  scoped_refptr<base::SingleThreadTaskRunner> main_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> io_runner_;

  // Information related to device perf category, only collected on the second
  // unsandboxed GPU process.
  const gpu::DevicePerfInfo device_perf_info_;

  // The GPU ids and the driver version that was passed down from the browser
  // process
  const gpu::GPUInfo::GPUDevice gpu_device_;

  // Should only be accessed on the IO thread after creation.
  mojo::Receiver<mojom::InfoCollectionGpuService> receiver_{this};
};
}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_GL_INFO_COLLECTION_GPU_SERVICE_IMPL_H_
