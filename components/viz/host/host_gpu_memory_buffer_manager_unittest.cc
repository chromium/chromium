// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/host/host_gpu_memory_buffer_manager.h"

#include <utility>

#include "base/run_loop.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "gpu/ipc/common/gpu_memory_buffer_support.h"
#include "gpu/ipc/host/gpu_memory_buffer_support.h"
#include "services/viz/privileged/interfaces/gl/gpu_service.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/client_native_pixmap_factory.h"

namespace viz {

namespace {

class TestGpuService : public mojom::GpuService {
 public:
  TestGpuService() {}
  ~TestGpuService() override {}

  HostGpuMemoryBufferManager::GpuServiceProvider CreateProvider() {
    return base::BindRepeating(
        [](mojom::GpuService* gpu_service,
           base::OnceClosure connection_error_handler) { return gpu_service; },
        base::Unretained(this));
  }

  bool HasAllocationRequest(gfx::GpuMemoryBufferId id, int client_id) const {
    for (const auto& req : allocation_requests_) {
      if (req.id == id && req.client_id == client_id)
        return true;
    }
    return false;
  }

  bool HasDestructionRequest(gfx::GpuMemoryBufferId id, int client_id) const {
    for (const auto& req : destruction_requests_) {
      if (req.id == id && req.client_id == client_id)
        return true;
    }
    return false;
  }

  void SatisfyAllocationRequest(gfx::GpuMemoryBufferId id, int client_id) {
    for (auto& req : allocation_requests_) {
      if (req.id == id && req.client_id == client_id) {
        gfx::GpuMemoryBufferHandle handle;
        handle.id = id;
        handle.type = gfx::SHARED_MEMORY_BUFFER;
        DCHECK(req.callback);
        std::move(req.callback).Run(std::move(handle));
        return;
      }
    }
    NOTREACHED();
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
      arc::mojom::VideoDecodeAcceleratorRequest vda_request) override {}

  void CreateArcVideoEncodeAccelerator(
      arc::mojom::VideoEncodeAcceleratorRequest vea_request) override {}

  void CreateArcVideoProtectedBufferAllocator(
      arc::mojom::VideoProtectedBufferAllocatorRequest pba_request) override {}

  void CreateArcProtectedBufferManager(
      arc::mojom::ProtectedBufferManagerRequest pbm_request) override {}
#endif  // defined(OS_CHROMEOS)
  void CreateJpegDecodeAccelerator(
      media::mojom::JpegDecodeAcceleratorRequest jda_request) override {}

  void CreateJpegEncodeAccelerator(
      media::mojom::JpegEncodeAcceleratorRequest jea_request) override {}

  void CreateVideoEncodeAcceleratorProvider(
      media::mojom::VideoEncodeAcceleratorProviderRequest request) override {}

  void CreateGpuMemoryBuffer(gfx::GpuMemoryBufferId id,
                             const gfx::Size& size,
                             gfx::BufferFormat format,
                             gfx::BufferUsage usage,
                             int client_id,
                             gpu::SurfaceHandle surface_handle,
                             CreateGpuMemoryBufferCallback callback) override {
    allocation_requests_.push_back(
        {id, size, format, usage, client_id, std::move(callback)});
  }

  void DestroyGpuMemoryBuffer(gfx::GpuMemoryBufferId id,
                              int client_id,
                              const gpu::SyncToken& sync_token) override {
    destruction_requests_.push_back({id, client_id});
  }

  void GetVideoMemoryUsageStats(
      GetVideoMemoryUsageStatsCallback callback) override {}

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

  void GpuSwitched() override {}

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
  struct AllocationRequest {
    const gfx::GpuMemoryBufferId id;
    const gfx::Size size;
    const gfx::BufferFormat format;
    const gfx::BufferUsage usage;
    const int client_id;
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

// It is necessary to install a custom pixmap factory which claims to support
// all native configurations, so that code that deals with this can be tested
// correctly.
class FakeClientNativePixmapFactory : public gfx::ClientNativePixmapFactory {
 public:
  explicit FakeClientNativePixmapFactory(bool allow_native_buffers)
      : allow_native_buffers_(allow_native_buffers) {}
  ~FakeClientNativePixmapFactory() override {}

  // gfx::ClientNativePixmapFactory:
  bool IsConfigurationSupported(gfx::BufferFormat format,
                                gfx::BufferUsage usage) const override {
    return allow_native_buffers_;
  }
  std::unique_ptr<gfx::ClientNativePixmap> ImportFromHandle(
      const gfx::NativePixmapHandle& handle,
      const gfx::Size& size,
      gfx::BufferUsage usage) override {
    NOTREACHED();
    return nullptr;
  }

 private:
  bool allow_native_buffers_ = false;

  DISALLOW_COPY_AND_ASSIGN(FakeClientNativePixmapFactory);
};

}  // namespace

class HostGpuMemoryBufferManagerTest : public ::testing::Test {
 public:
  HostGpuMemoryBufferManagerTest() = default;
  ~HostGpuMemoryBufferManagerTest() override = default;

  std::unique_ptr<gfx::GpuMemoryBuffer> AllocateGpuMemoryBufferSync(
      HostGpuMemoryBufferManager* manager) {
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
                       manager, &buffer, run_loop.QuitClosure()));
    run_loop.Run();
    return buffer;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(HostGpuMemoryBufferManagerTest);
};

std::unique_ptr<gpu::GpuMemoryBufferSupport> MakeGpuMemoryBufferSupport(
    bool allow_native_buffers) {
#if defined(OS_LINUX)
  return std::make_unique<gpu::GpuMemoryBufferSupport>(
      std::make_unique<FakeClientNativePixmapFactory>(allow_native_buffers));
#else
  return std::make_unique<gpu::GpuMemoryBufferSupport>();
#endif
}

// Tests that allocation requests from a client that goes away before allocation
// completes are cleaned up correctly.
TEST_F(HostGpuMemoryBufferManagerTest, AllocationRequestsForDestroyedClient) {
#if !defined(USE_OZONE) && !defined(OS_MACOSX) && !defined(OS_WIN)
  // Not all platforms support native configurations (currently only ozone and
  // mac support it). Abort the test in those platforms.
  gpu::GpuMemoryBufferSupport support;
  DCHECK(gpu::GetNativeGpuMemoryBufferConfigurations(&support).empty());
  return;
#else
  // Note: HostGpuMemoryBufferManager normally operates on a mojom::GpuService
  // implementation over mojo. Which means the communication from SGMBManager to
  // GpuService is asynchronous. In this test, the mojom::GpuService is not
  // bound to a mojo pipe, which means those calls are all synchronous.
  TestGpuService gpu_service;
  auto gpu_memory_buffer_support = MakeGpuMemoryBufferSupport(true);
  HostGpuMemoryBufferManager manager(gpu_service.CreateProvider(), 1,
                                     std::move(gpu_memory_buffer_support),
                                     base::ThreadTaskRunnerHandle::Get());

  const auto buffer_id = static_cast<gfx::GpuMemoryBufferId>(1);
  const int client_id = 2;
  const gfx::Size size(10, 20);
  const gfx::BufferFormat format = gfx::BufferFormat::RGBA_8888;
  const gfx::BufferUsage usage = gfx::BufferUsage::GPU_READ;
  manager.AllocateGpuMemoryBuffer(buffer_id, client_id, size, format, usage,
                                  gpu::kNullSurfaceHandle, base::DoNothing());
  EXPECT_TRUE(gpu_service.HasAllocationRequest(buffer_id, client_id));
  EXPECT_FALSE(gpu_service.HasDestructionRequest(buffer_id, client_id));

  // Destroy the client. Since no memory has been allocated yet, there will be
  // no request for freeing memory.
  manager.DestroyAllGpuMemoryBufferForClient(client_id);
  EXPECT_TRUE(gpu_service.HasAllocationRequest(buffer_id, client_id));
  EXPECT_FALSE(gpu_service.HasDestructionRequest(buffer_id, client_id));

  // When the host receives the allocated memory for the destroyed client, it
  // should request the allocated memory to be freed.
  gpu_service.SatisfyAllocationRequest(buffer_id, client_id);
  EXPECT_TRUE(gpu_service.HasDestructionRequest(buffer_id, client_id));
#endif
}

TEST_F(HostGpuMemoryBufferManagerTest, RequestsFromUntrustedClientsValidated) {
  TestGpuService gpu_service;
  auto gpu_memory_buffer_support = MakeGpuMemoryBufferSupport(false);
  HostGpuMemoryBufferManager manager(gpu_service.CreateProvider(), 1,
                                     std::move(gpu_memory_buffer_support),
                                     base::ThreadTaskRunnerHandle::Get());
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
    manager.AllocateGpuMemoryBuffer(
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
    EXPECT_FALSE(gpu_service.HasAllocationRequest(buffer_id, client_id));
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
  TestGpuService gpu_service;
  auto gpu_memory_buffer_support = MakeGpuMemoryBufferSupport(false);
  HostGpuMemoryBufferManager manager(gpu_service.CreateProvider(), 1,
                                     std::move(gpu_memory_buffer_support),
                                     base::ThreadTaskRunnerHandle::Get());
  auto buffer = AllocateGpuMemoryBufferSync(&manager);
  EXPECT_TRUE(buffer);
  buffer.reset();
}

TEST_F(HostGpuMemoryBufferManagerTest,
       GpuMemoryBufferDestroyedOnDifferentThread) {
  TestGpuService gpu_service;
  auto gpu_memory_buffer_support = MakeGpuMemoryBufferSupport(false);
  HostGpuMemoryBufferManager manager(gpu_service.CreateProvider(), 1,
                                     std::move(gpu_memory_buffer_support),
                                     base::ThreadTaskRunnerHandle::Get());
  auto buffer = AllocateGpuMemoryBufferSync(&manager);
  EXPECT_TRUE(buffer);
  // Destroy the buffer in a different thread.
  base::Thread diff_thread("DestroyThread");
  ASSERT_TRUE(diff_thread.Start());
  diff_thread.task_runner()->DeleteSoon(FROM_HERE, std::move(buffer));
  diff_thread.Stop();
}

}  // namespace viz
