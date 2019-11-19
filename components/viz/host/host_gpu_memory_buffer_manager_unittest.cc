// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/host/host_gpu_memory_buffer_manager.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/run_loop.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "gpu/ipc/common/gpu_memory_buffer_support.h"
#include "gpu/ipc/host/gpu_memory_buffer_support.h"
#include "services/viz/privileged/mojom/gl/gpu_service.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/client_native_pixmap_factory.h"

#if defined(USE_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

#if defined(OS_ANDROID)
#include "base/android/android_hardware_buffer_compat.h"
#endif

namespace viz {

namespace {

class TestGpuService : public mojom::GpuService {
 public:
  TestGpuService() = default;
  ~TestGpuService() override = default;

  mojom::GpuService* GetGpuService(base::OnceClosure connection_error_handler) {
    DCHECK(!connection_error_handler_);
    connection_error_handler_ = std::move(connection_error_handler);
    return this;
  }

  void SimulateConnectionError() {
    if (connection_error_handler_)
      std::move(connection_error_handler_).Run();
  }

  int GetAllocationRequestsCount() const { return allocation_requests_.size(); }

  bool IsAllocationRequestAt(size_t index,
                             gfx::GpuMemoryBufferId id,
                             int client_id) const {
    DCHECK_LT(index, allocation_requests_.size());
    const auto& req = allocation_requests_[index];
    return req.id == id && req.client_id == client_id;
  }

  int GetDestructionRequestsCount() const {
    return destruction_requests_.size();
  }

  bool IsDestructionRequestAt(size_t index,
                              gfx::GpuMemoryBufferId id,
                              int client_id) const {
    DCHECK_LT(index, destruction_requests_.size());
    const auto& req = destruction_requests_[index];
    return req.id == id && req.client_id == client_id;
  }

  void SatisfyAllocationRequestAt(size_t index) {
    DCHECK_LT(index, allocation_requests_.size());
    auto& req = allocation_requests_[index];

    gfx::GpuMemoryBufferHandle handle;
    handle.id = req.id;
    handle.type = gfx::SHARED_MEMORY_BUFFER;
    constexpr size_t kBufferSizeBytes = 100;
    handle.region = base::UnsafeSharedMemoryRegion::Create(kBufferSizeBytes);

    DCHECK(req.callback);
    std::move(req.callback).Run(std::move(handle));
  }

  // mojom::GpuService:
  void EstablishGpuChannel(int32_t client_id,
                           uint64_t client_tracing_id,
                           bool is_gpu_host,
                           bool cache_shaders_on_disk,
                           EstablishGpuChannelCallback callback) override {}

  void CloseChannel(int32_t client_id) override {}
#if defined(OS_CHROMEOS)
  void CreateArcVideoDecodeAccelerator(
      mojo::PendingReceiver<arc::mojom::VideoDecodeAccelerator> vda_receiver)
      override {}

  void CreateArcVideoEncodeAccelerator(
      mojo::PendingReceiver<arc::mojom::VideoEncodeAccelerator> vea_receiver)
      override {}

  void CreateArcVideoProtectedBufferAllocator(
      mojo::PendingReceiver<arc::mojom::VideoProtectedBufferAllocator>
          pba_receiver) override {}

  void CreateArcProtectedBufferManager(
      mojo::PendingReceiver<arc::mojom::ProtectedBufferManager> pbm_receiver)
      override {}

  void CreateJpegDecodeAccelerator(
      mojo::PendingReceiver<chromeos_camera::mojom::MjpegDecodeAccelerator>
          jda_receiver) override {}

  void CreateJpegEncodeAccelerator(
      mojo::PendingReceiver<chromeos_camera::mojom::JpegEncodeAccelerator>
          jea_receiver) override {}
#endif  // defined(OS_CHROMEOS)

  void CreateVideoEncodeAcceleratorProvider(
      mojo::PendingReceiver<media::mojom::VideoEncodeAcceleratorProvider>
          receiver) override {}

  void CreateGpuMemoryBuffer(gfx::GpuMemoryBufferId id,
                             const gfx::Size& size,
                             gfx::BufferFormat format,
                             gfx::BufferUsage usage,
                             int client_id,
                             gpu::SurfaceHandle surface_handle,
                             CreateGpuMemoryBufferCallback callback) override {
    allocation_requests_.push_back({id, client_id, std::move(callback)});
  }

  void DestroyGpuMemoryBuffer(gfx::GpuMemoryBufferId id,
                              int client_id,
                              const gpu::SyncToken& sync_token) override {
    destruction_requests_.push_back({id, client_id});
  }

  void GetVideoMemoryUsageStats(
      GetVideoMemoryUsageStatsCallback callback) override {}

  void StartPeakMemoryMonitor(uint32_t sequence_num) override {}

  void GetPeakMemoryUsage(uint32_t sequence_num,
                          GetPeakMemoryUsageCallback callback) override {}

#if defined(OS_WIN)
  void RequestCompleteGpuInfo(
      RequestCompleteGpuInfoCallback callback) override {}

  void GetGpuSupportedRuntimeVersion(
      GetGpuSupportedRuntimeVersionCallback callback) override {}
#endif

  void RequestHDRStatus(RequestHDRStatusCallback callback) override {}

  void LoadedShader(int32_t client_id,
                    const std::string& key,
                    const std::string& data) override {}

  void WakeUpGpu() override {}

  void GpuSwitched(gl::GpuPreference active_gpu_heuristic) override {}

  void DestroyAllChannels() override {}

  void OnBackgroundCleanup() override {}

  void OnBackgrounded() override {}

  void OnForegrounded() override {}

#if defined(OS_MACOSX)
  void BeginCATransaction() override {}

  void CommitCATransaction(CommitCATransactionCallback callback) override {}
#endif

  void Crash() override {}

  void Hang() override {}

  void ThrowJavaException() override {}

  void Stop(StopCallback callback) override {}

 private:
  base::OnceClosure connection_error_handler_;

  struct AllocationRequest {
    gfx::GpuMemoryBufferId id;
    int client_id;
    CreateGpuMemoryBufferCallback callback;
  };
  std::vector<AllocationRequest> allocation_requests_;

  struct DestructionRequest {
    const gfx::GpuMemoryBufferId id;
    const int client_id;
  };
  std::vector<DestructionRequest> destruction_requests_;

  DISALLOW_COPY_AND_ASSIGN(TestGpuService);
};

}  // namespace

class HostGpuMemoryBufferManagerTest : public ::testing::Test {
 public:
  HostGpuMemoryBufferManagerTest() = default;
  ~HostGpuMemoryBufferManagerTest() override = default;

  void SetUp() override {
    gpu_service_ = std::make_unique<TestGpuService>();
    auto gpu_service_provider = base::BindRepeating(
        &TestGpuService::GetGpuService, base::Unretained(gpu_service_.get()));
    auto gpu_memory_buffer_support =
        std::make_unique<gpu::GpuMemoryBufferSupport>();
    gpu_memory_buffer_manager_ = std::make_unique<HostGpuMemoryBufferManager>(
        std::move(gpu_service_provider), 1,
        std::move(gpu_memory_buffer_support),
        base::ThreadTaskRunnerHandle::Get());
  }

  // Not all platforms support native configurations (currently only Windows,
  // Mac and some Ozone platforms). Abort the test in those platforms.
  bool IsNativePixmapConfigSupported() {
    bool native_pixmap_supported = false;
#if defined(USE_OZONE)
    native_pixmap_supported =
        ui::OzonePlatform::GetInstance()->IsNativePixmapConfigSupported(
            gfx::BufferFormat::RGBA_8888, gfx::BufferUsage::GPU_READ);
#elif defined(OS_ANDROID)
    native_pixmap_supported =
        base::AndroidHardwareBufferCompat::IsSupportAvailable();
#elif defined(OS_MACOSX) || defined(OS_WIN)
    native_pixmap_supported = true;
#endif

    if (native_pixmap_supported)
      return true;

    gpu::GpuMemoryBufferSupport support;
    DCHECK(gpu::GetNativeGpuMemoryBufferConfigurations(&support).empty());
    return false;
  }

  std::unique_ptr<gfx::GpuMemoryBuffer> AllocateGpuMemoryBufferSync() {
    base::Thread diff_thread("TestThread");
    diff_thread.Start();
    std::unique_ptr<gfx::GpuMemoryBuffer> buffer;
    base::RunLoop run_loop;
    diff_thread.task_runner()->PostTask(
        FROM_HERE, base::BindOnce(
                       [](HostGpuMemoryBufferManager* manager,
                          std::unique_ptr<gfx::GpuMemoryBuffer>* out_buffer,
                          base::OnceClosure callback) {
                         *out_buffer = manager->CreateGpuMemoryBuffer(
                             gfx::Size(64, 64), gfx::BufferFormat::YVU_420,
                             gfx::BufferUsage::GPU_READ,
                             gpu::kNullSurfaceHandle);
                         std::move(callback).Run();
                       },
                       gpu_memory_buffer_manager_.get(), &buffer,
                       run_loop.QuitClosure()));
    run_loop.Run();
    return buffer;
  }

  TestGpuService* gpu_service() const { return gpu_service_.get(); }

  HostGpuMemoryBufferManager* gpu_memory_buffer_manager() const {
    return gpu_memory_buffer_manager_.get();
  }

 private:
  std::unique_ptr<TestGpuService> gpu_service_;
  std::unique_ptr<HostGpuMemoryBufferManager> gpu_memory_buffer_manager_;

  DISALLOW_COPY_AND_ASSIGN(HostGpuMemoryBufferManagerTest);
};

// Tests that allocation requests from a client that goes away before allocation
// completes are cleaned up correctly.
TEST_F(HostGpuMemoryBufferManagerTest, AllocationRequestsForDestroyedClient) {
  if (!IsNativePixmapConfigSupported())
    return;

  // Note: HostGpuMemoryBufferManager normally operates on a mojom::GpuService
  // implementation over mojo. Which means the communication from HGMBManager to
  // GpuService is asynchronous. In this test, the mojom::GpuService is not
  // bound to a mojo pipe, which means those calls are all synchronous.

  const auto buffer_id = static_cast<gfx::GpuMemoryBufferId>(1);
  const int client_id = 2;
  const gfx::Size size(10, 20);
  const gfx::BufferFormat format = gfx::BufferFormat::RGBA_8888;
  const gfx::BufferUsage usage = gfx::BufferUsage::GPU_READ;
  gpu_memory_buffer_manager()->AllocateGpuMemoryBuffer(
      buffer_id, client_id, size, format, usage, gpu::kNullSurfaceHandle,
      base::DoNothing());
  EXPECT_EQ(1, gpu_service()->GetAllocationRequestsCount());
  EXPECT_TRUE(gpu_service()->IsAllocationRequestAt(0, buffer_id, client_id));
  EXPECT_EQ(0, gpu_service()->GetDestructionRequestsCount());

  // Destroy the client. Since no memory has been allocated yet, there will be
  // no request for freeing memory.
  gpu_memory_buffer_manager()->DestroyAllGpuMemoryBufferForClient(client_id);
  EXPECT_EQ(1, gpu_service()->GetAllocationRequestsCount());
  EXPECT_EQ(0, gpu_service()->GetDestructionRequestsCount());

  // When the host receives the allocated memory for the destroyed client, it
  // should request the allocated memory to be freed.
  gpu_service()->SatisfyAllocationRequestAt(0);
  EXPECT_EQ(1, gpu_service()->GetAllocationRequestsCount());
  EXPECT_EQ(1, gpu_service()->GetDestructionRequestsCount());
  EXPECT_TRUE(gpu_service()->IsDestructionRequestAt(0, buffer_id, client_id));
}

TEST_F(HostGpuMemoryBufferManagerTest, RequestsFromUntrustedClientsValidated) {
  const auto buffer_id = static_cast<gfx::GpuMemoryBufferId>(1);
  const int client_id = 2;
  // SCANOUT cannot be used if native gpu memory buffer is not supported.
  struct {
    gfx::BufferUsage usage;
    gfx::BufferFormat format;
    gfx::Size size;
    bool expect_null_handle;
  } configs[] = {
      {gfx::BufferUsage::SCANOUT, gfx::BufferFormat::YVU_420, {10, 20}, true},
      {gfx::BufferUsage::GPU_READ, gfx::BufferFormat::YVU_420, {64, 64}, false},
  };
  for (const auto& config : configs) {
    gfx::GpuMemoryBufferHandle allocated_handle;
    base::RunLoop runloop;
    gpu_memory_buffer_manager()->AllocateGpuMemoryBuffer(
        buffer_id, client_id, config.size, config.format, config.usage,
        gpu::kNullSurfaceHandle,
        base::BindOnce(
            [](gfx::GpuMemoryBufferHandle* allocated_handle,
               base::OnceClosure callback, gfx::GpuMemoryBufferHandle handle) {
              *allocated_handle = std::move(handle);
              std::move(callback).Run();
            },
            &allocated_handle, runloop.QuitClosure()));
    // Since native gpu memory buffers are not supported, the mojom.GpuService
    // should not receive any allocation requests.
    EXPECT_EQ(0, gpu_service()->GetAllocationRequestsCount());
    runloop.Run();
    if (config.expect_null_handle) {
      EXPECT_TRUE(allocated_handle.is_null());
    } else {
      EXPECT_FALSE(allocated_handle.is_null());
      EXPECT_EQ(gfx::GpuMemoryBufferType::SHARED_MEMORY_BUFFER,
                allocated_handle.type);
    }
  }
}

TEST_F(HostGpuMemoryBufferManagerTest, GpuMemoryBufferDestroyed) {
  auto buffer = AllocateGpuMemoryBufferSync();
  EXPECT_TRUE(buffer);
  buffer.reset();
}

TEST_F(HostGpuMemoryBufferManagerTest,
       GpuMemoryBufferDestroyedOnDifferentThread) {
  auto buffer = AllocateGpuMemoryBufferSync();
  EXPECT_TRUE(buffer);
  // Destroy the buffer in a different thread.
  base::Thread diff_thread("DestroyThread");
  ASSERT_TRUE(diff_thread.Start());
  diff_thread.task_runner()->DeleteSoon(FROM_HERE, std::move(buffer));
  diff_thread.Stop();
}

// Tests that if an allocated buffer is received after the gpu service issuing
// it has died, HGMBManager retries the allocation request properly.
TEST_F(HostGpuMemoryBufferManagerTest, AllocationRequestFromDeadGpuService) {
  if (!IsNativePixmapConfigSupported())
    return;

  // Request allocation. No allocation should happen yet.
  gfx::GpuMemoryBufferHandle allocated_handle;
  const auto buffer_id = static_cast<gfx::GpuMemoryBufferId>(1);
  const int client_id = 2;
  const gfx::Size size(10, 20);
  const gfx::BufferFormat format = gfx::BufferFormat::RGBA_8888;
  const gfx::BufferUsage usage = gfx::BufferUsage::GPU_READ;
  gpu_memory_buffer_manager()->AllocateGpuMemoryBuffer(
      buffer_id, client_id, size, format, usage, gpu::kNullSurfaceHandle,
      base::BindOnce(
          [](gfx::GpuMemoryBufferHandle* allocated_handle,
             gfx::GpuMemoryBufferHandle handle) {
            *allocated_handle = std::move(handle);
          },
          &allocated_handle));
  EXPECT_EQ(1, gpu_service()->GetAllocationRequestsCount());
  EXPECT_TRUE(gpu_service()->IsAllocationRequestAt(0, buffer_id, client_id));
  EXPECT_TRUE(allocated_handle.is_null());

  // Simulate a connection error from gpu. HGMBManager should retry allocation
  // request.
  gpu_service()->SimulateConnectionError();
  EXPECT_EQ(2, gpu_service()->GetAllocationRequestsCount());
  EXPECT_TRUE(gpu_service()->IsAllocationRequestAt(1, buffer_id, client_id));
  EXPECT_TRUE(allocated_handle.is_null());

  // Send an allocated buffer corresponding to the first request on the old gpu.
  // This should not result in a buffer handle.
  gpu_service()->SatisfyAllocationRequestAt(0);
  EXPECT_EQ(2, gpu_service()->GetAllocationRequestsCount());
  EXPECT_TRUE(allocated_handle.is_null());

  // Send an allocated buffer corresponding to the retried request on the new
  // gpu. This should result in a buffer handle.
  gpu_service()->SatisfyAllocationRequestAt(1);
  EXPECT_EQ(2, gpu_service()->GetAllocationRequestsCount());
  EXPECT_FALSE(allocated_handle.is_null());
}

}  // namespace viz
