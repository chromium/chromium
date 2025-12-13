// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/test/stub_gpu_service.h"

#include "components/persistent_cache/pending_backend.h"

namespace viz {

StubGpuService::StubGpuService() = default;
StubGpuService::~StubGpuService() = default;

void StubGpuService::EstablishGpuChannel(int32_t client_id,
                                         uint64_t client_tracing_id,
                                         bool is_gpu_host,
                                         bool enable_extra_handles_validation,
                                         EstablishGpuChannelCallback callback) {
}

void StubGpuService::SetChannelClientPid(int32_t client_id,
                                         base::ProcessId client_pid) {}

void StubGpuService::SetChannelDiskCacheHandle(
    int32_t client_id,
    const gpu::GpuDiskCacheHandle& handle) {}

void StubGpuService::SetChannelPersistentCachePendingBackend(
    int32_t client_id,
    const gpu::GpuDiskCacheHandle& handle,
    persistent_cache::PendingBackend pending_backend) {}

void StubGpuService::OnDiskCacheHandleDestoyed(
    const gpu::GpuDiskCacheHandle& handle) {}

void StubGpuService::CloseChannel(int32_t client_id) {}

void StubGpuService::StartPeakMemoryMonitor(uint32_t sequence_num) {}

void StubGpuService::GetPeakMemoryUsage(uint32_t sequence_num,
                                        GetPeakMemoryUsageCallback callback) {}

#if BUILDFLAG(IS_CHROMEOS)
void StubGpuService::CreateJpegDecodeAccelerator(
    mojo::PendingReceiver<chromeos_camera::mojom::MjpegDecodeAccelerator>
        jda_receiver) {}
void StubGpuService::CreateJpegEncodeAccelerator(
    mojo::PendingReceiver<chromeos_camera::mojom::JpegEncodeAccelerator>
        jea_receiver) {}
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_WIN)
void StubGpuService::RegisterDCOMPSurfaceHandle(
    mojo::PlatformHandle surface_handle,
    RegisterDCOMPSurfaceHandleCallback callback) {}
void StubGpuService::UnregisterDCOMPSurfaceHandle(
    const base::UnguessableToken& token) {}
#endif

void StubGpuService::CreateVideoEncodeAcceleratorProvider(
    mojo::PendingReceiver<media::mojom::VideoEncodeAcceleratorProvider>
        receiver) {}

void StubGpuService::BindWebNNContextProvider(
    mojo::PendingReceiver<webnn::mojom::WebNNContextProvider> receiver,
    int32_t client_id) {}

void StubGpuService::GetVideoMemoryUsageStats(
    GetVideoMemoryUsageStatsCallback callback) {}

#if BUILDFLAG(IS_WIN)
void StubGpuService::RequestDXGIInfo(RequestDXGIInfoCallback callback) {}
#endif

void StubGpuService::LoadedBlob(const gpu::GpuDiskCacheHandle& handle,
                                const std::string& key,
                                const std::string& data) {}

void StubGpuService::WakeUpGpu() {}

void StubGpuService::GpuSwitched() {}

void StubGpuService::DisplayAdded() {}

void StubGpuService::DisplayRemoved() {}

void StubGpuService::DisplayMetricsChanged() {}

void StubGpuService::DestroyAllChannels() {}

void StubGpuService::OnBackgroundCleanup() {}

void StubGpuService::OnBackgrounded() {}

void StubGpuService::OnForegrounded() {}

#if !BUILDFLAG(IS_ANDROID)
void StubGpuService::OnMemoryPressure(base::MemoryPressureLevel level) {}
#endif

#if BUILDFLAG(IS_APPLE)
void StubGpuService::BeginCATransaction() {}
void StubGpuService::CommitCATransaction(CommitCATransactionCallback callback) {
}
#endif

#if BUILDFLAG(CLANG_PROFILING_INSIDE_SANDBOX)
void StubGpuService::WriteClangProfilingProfile(
    WriteClangProfilingProfileCallback callback) {}
#endif

void StubGpuService::GetDawnInfo(bool collect_metrics,
                                 GetDawnInfoCallback callback) {}

void StubGpuService::Crash() {}

void StubGpuService::Hang() {}

void StubGpuService::ThrowJavaException() {}

}  // namespace viz
