// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/gl/gpu_service_impl.h"

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "gpu/config/gpu_info.h"
#include "gpu/ipc/service/display_context.h"
#include "gpu/ipc/service/gpu_watchdog_thread.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/viz/public/mojom/gpu.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/init/gl_factory.h"

namespace viz {
namespace {

class MockDisplayContext : public gpu::DisplayContext {
 public:
  MockDisplayContext() = default;
  ~MockDisplayContext() override = default;

  // gpu::DisplayContext implementation.
  MOCK_METHOD0(MarkContextLost, void());
};

}  // namespace

class GpuServiceTest : public testing::Test {
 public:
  GpuServiceTest()
      : io_thread_("TestIOThread"),
        wait_(base::WaitableEvent::ResetPolicy::MANUAL,
              base::WaitableEvent::InitialState::NOT_SIGNALED) {}
  ~GpuServiceTest() override {}

  GpuServiceImpl* gpu_service() { return gpu_service_.get(); }

  void DestroyService() { gpu_service_ = nullptr; }

  void BlockIOThread() {
    wait_.Reset();
    io_runner()->PostTask(FROM_HERE, base::BindOnce(&base::WaitableEvent::Wait,
                                                    base::Unretained(&wait_)));
  }

  void UnblockIOThread() {
    DCHECK(!wait_.IsSignaled());
    wait_.Signal();
  }

  scoped_refptr<base::SingleThreadTaskRunner> io_runner() {
    return io_thread_.task_runner();
  }

  // testing::Test
  void SetUp() override {
    ASSERT_TRUE(io_thread_.Start());
    gpu::GPUInfo gpu_info;
    gpu_info.in_process_gpu = false;
    gpu_service_ = std::make_unique<GpuServiceImpl>(
        gpu_info, /*watchdog_thread=*/nullptr, io_thread_.task_runner(),
        gpu::GpuFeatureInfo(), gpu::GpuPreferences(), gpu::GPUInfo(),
        gpu::GpuFeatureInfo(), gpu::GpuExtraInfo(),
        /*vulkan_implementation=*/nullptr,
        /*exit_callback=*/base::DoNothing());
  }

  void TearDown() override {
    DestroyService();
    base::RunLoop runloop;
    runloop.RunUntilIdle();
    io_thread_.Stop();
  }

 private:
  base::Thread io_thread_;
  std::unique_ptr<GpuServiceImpl> gpu_service_;
  base::WaitableEvent wait_;

  DISALLOW_COPY_AND_ASSIGN(GpuServiceTest);
};

// Tests that GpuServiceImpl can be destroyed before Bind() succeeds on the IO
// thread.
TEST_F(GpuServiceTest, ServiceDestroyedBeforeBind) {
  // Block the IO thread to make sure that the GpuServiceImpl is destroyed
  // before the binding happens on the IO thread.
  mojo::Remote<mojom::GpuService> gpu_service_remote;
  BlockIOThread();
  gpu_service()->Bind(gpu_service_remote.BindNewPipeAndPassReceiver());
  UnblockIOThread();
  DestroyService();
}

// Tests that GpuServiceImpl can be destroyed after Bind() succeeds on the IO
// thread.
TEST_F(GpuServiceTest, ServiceDestroyedAfterBind) {
  mojo::Remote<mojom::GpuService> gpu_service_remote;
  gpu_service()->Bind(gpu_service_remote.BindNewPipeAndPassReceiver());
  base::WaitableEvent wait(base::WaitableEvent::ResetPolicy::MANUAL,
                           base::WaitableEvent::InitialState::NOT_SIGNALED);
  io_runner()->PostTask(FROM_HERE, base::BindOnce(&base::WaitableEvent::Signal,
                                                  base::Unretained(&wait)));
  wait.Wait();
  DestroyService();
}

TEST_F(GpuServiceTest, LoseAllContexts) {
  mojo::Remote<mojom::GpuService> gpu_service_remote;
  gpu_service()->Bind(gpu_service_remote.BindNewPipeAndPassReceiver());

  // Use a disconnected mojo remote for GpuHost, we don't need to receive any
  // messages.
  mojo::PendingRemote<mojom::GpuHost> gpu_host_proxy;
  ignore_result(gpu_host_proxy.InitWithNewPipeAndPassReceiver());
  gpu_service()->InitializeWithHost(
      std::move(gpu_host_proxy), gpu::GpuProcessActivityFlags(),
      gl::init::CreateOffscreenGLSurface(gfx::Size()),
      /*sync_point_manager=*/nullptr, /*shared_image_manager=*/nullptr,
      /*shutdown_event=*/nullptr);
  gpu_service_remote.FlushForTesting();

  MockDisplayContext display_context;
  gpu_service()->RegisterDisplayContext(&display_context);

  // Verify that |display_context| is told to lose it's context.
  EXPECT_CALL(display_context, MarkContextLost());
  gpu_service()->LoseAllContexts();
  testing::Mock::VerifyAndClearExpectations(&display_context);

  gpu_service()->MaybeExitOnContextLost();
  EXPECT_TRUE(gpu_service()->IsExiting());

  // Verify that if GPU process is already exiting then |display_context| won't
  // be told to lose it's context.
  EXPECT_CALL(display_context, MarkContextLost()).Times(0);
  gpu_service()->LoseAllContexts();
  testing::Mock::VerifyAndClearExpectations(&display_context);

  gpu_service()->UnregisterDisplayContext(&display_context);
}

}  // namespace viz
