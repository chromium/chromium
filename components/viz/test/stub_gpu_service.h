// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_TEST_STUB_GPU_SERVICE_H_
#define COMPONENTS_VIZ_TEST_STUB_GPU_SERVICE_H_

#include <string>

#include "base/clang_profiling_buildflags.h"
#include "build/build_config.h"
#include "media/media_buildflags.h"
#include "services/viz/privileged/mojom/gl/gpu_service.mojom.h"

namespace viz {

// Test implementation of mojom::GpuService that provides empty
// implementations for all methods. Tests can inherit from this and override
// methods that they care about.
class StubGpuService : public mojom::GpuService {
 public:
  StubGpuService();
  ~StubGpuService() override;

  // mojom::GpuService:
  void EstablishGpuChannel(int32_t client_id,
                           uint64_t client_tracing_id,
                           bool is_gpu_host,
                           bool enable_extra_handles_validation,
                           EstablishGpuChannelCallback callback) override;
  void SetChannelClientPid(int32_t client_id,
                           base::ProcessId client_pid) override;
  void SetChannelDiskCacheHandle(
      int32_t client_id,
      const gpu::GpuDiskCacheHandle& handle) override;
  void SetChannelPersistentCachePendingBackend(
      int32_t client_id,
      const gpu::GpuDiskCacheHandle& handle,
      persistent_cache::PendingBackend pending_backend) override;
  void OnDiskCacheHandleDestoyed(
      const gpu::GpuDiskCacheHandle& handle) override;
  void CloseChannel(int32_t client_id) override;
  void StartPeakMemoryMonitor(uint32_t sequence_num) override;
  void GetPeakMemoryUsage(uint32_t sequence_num,
                          GetPeakMemoryUsageCallback callback) override;
#if BUILDFLAG(IS_CHROMEOS)
  void CreateJpegDecodeAccelerator(
      mojo::PendingReceiver<chromeos_camera::mojom::MjpegDecodeAccelerator>
          jda_receiver) override;
  void CreateJpegEncodeAccelerator(
      mojo::PendingReceiver<chromeos_camera::mojom::JpegEncodeAccelerator>
          jea_receiver) override;
#endif  // BUILDFLAG(IS_CHROMEOS)
#if BUILDFLAG(IS_WIN)
  void RegisterDCOMPSurfaceHandle(
      mojo::PlatformHandle surface_handle,
      RegisterDCOMPSurfaceHandleCallback callback) override;
  void UnregisterDCOMPSurfaceHandle(
      const base::UnguessableToken& token) override;
#endif
  void CreateVideoEncodeAcceleratorProvider(
      mojo::PendingReceiver<media::mojom::VideoEncodeAcceleratorProvider>
          receiver) override;
  void BindWebNNContextProvider(
      mojo::PendingReceiver<webnn::mojom::WebNNContextProvider> receiver,
      int32_t client_id) override;
  void GetVideoMemoryUsageStats(
      GetVideoMemoryUsageStatsCallback callback) override;
#if BUILDFLAG(IS_WIN)
  void RequestDXGIInfo(RequestDXGIInfoCallback callback) override;
#endif
  void LoadedBlob(const gpu::GpuDiskCacheHandle& handle,
                  const std::string& key,
                  const std::string& data) override;
  void WakeUpGpu() override;
  void GpuSwitched() override;
  void DisplayAdded() override;
  void DisplayRemoved() override;
  void DisplayMetricsChanged() override;
  void DestroyAllChannels() override;
  void OnBackgroundCleanup() override;
  void OnBackgrounded() override;
  void OnForegrounded() override;
#if !BUILDFLAG(IS_ANDROID)
  void OnMemoryPressure(base::MemoryPressureLevel level) override;
#endif
#if BUILDFLAG(IS_APPLE)
  void BeginCATransaction() override;
  void CommitCATransaction(CommitCATransactionCallback callback) override;
#endif
#if BUILDFLAG(CLANG_PROFILING_INSIDE_SANDBOX)
  void WriteClangProfilingProfile(
      WriteClangProfilingProfileCallback callback) override;
#endif
  void GetDawnInfo(bool collect_metrics, GetDawnInfoCallback callback) override;
  void Crash() override;
  void Hang() override;
  void ThrowJavaException() override;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_TEST_STUB_GPU_SERVICE_H_
