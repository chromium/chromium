// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/gl/gpu_service_impl.h"

#include <memory>
#include <tuple>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "gpu/config/gpu_info.h"
#include "gpu/ipc/service/gpu_watchdog_thread.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/viz/public/mojom/gpu.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_utils.h"
#include "ui/gl/init/gl_factory.h"

namespace viz {

class GpuServiceTest : public testing::Test {
 public:
  GpuServiceTest()
      : io_thread_("TestIOThread"),
        wait_(base::WaitableEvent::ResetPolicy::MANUAL,
              base::WaitableEvent::InitialState::NOT_SIGNALED) {}

  GpuServiceTest(const GpuServiceTest&) = delete;
  GpuServiceTest& operator=(const GpuServiceTest&) = delete;

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
    GpuServiceImpl::InitParams init_params;
    init_params.io_runner = io_thread_.task_runner();
    init_params.exit_callback = base::DoNothing();
    gpu_service_ = std::make_unique<GpuServiceImpl>(
        gpu::GpuPreferences(), gpu_info, gpu::GpuFeatureInfo(), gpu::GPUInfo(),
        gpu::GpuFeatureInfo(), gfx::GpuExtraInfo(), std::move(init_params));
  }

  void TearDown() override {
    DestroyService();
    base::RunLoop runloop;
    runloop.RunUntilIdle();
    io_thread_.Stop();
  }

  std::optional<bool> visible_;

 private:
  base::Thread io_thread_;
  std::unique_ptr<GpuServiceImpl> gpu_service_;
  base::WaitableEvent wait_;
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
  std::ignore = gpu_host_proxy.InitWithNewPipeAndPassReceiver();
  gpu_service()->InitializeWithHost(
      std::move(gpu_host_proxy), gpu::GpuProcessShmCount(),
      gl::init::CreateOffscreenGLSurface(gl::GetDefaultDisplay(), gfx::Size()),
      /*sync_point_manager=*/nullptr, /*shared_image_manager=*/nullptr,
      /*shutdown_event=*/nullptr);
  gpu_service_remote.FlushForTesting();

  gpu_service()->MaybeExitOnContextLost(
      /*synthetic_loss=*/false, gpu::error::ContextLostReason::kUnknown);
  EXPECT_TRUE(gpu_service()->IsExiting());
}

// Tests that the visibility callback gets called when visibility changes.
TEST_F(GpuServiceTest, VisibilityCallbackCalled) {
  mojo::Remote<mojom::GpuService> gpu_service_remote;
  gpu_service()->Bind(gpu_service_remote.BindNewPipeAndPassReceiver());

  mojo::PendingRemote<mojom::GpuHost> gpu_host_proxy;
  std::ignore = gpu_host_proxy.InitWithNewPipeAndPassReceiver();
  gpu_service()->InitializeWithHost(
      std::move(gpu_host_proxy), gpu::GpuProcessShmCount(),
      gl::init::CreateOffscreenGLSurface(gl::GetDefaultDisplay(), gfx::Size()),
      /*sync_point_manager=*/nullptr, /*shared_image_manager=*/nullptr,
      /*shutdown_event=*/nullptr);
  gpu_service_remote.FlushForTesting();

  gpu_service()->SetVisibilityChangedCallback(base::BindRepeating(
      [](GpuServiceTest* test, bool visible) { test->visible_ = visible; },
      base::Unretained(this)));
  EXPECT_FALSE(visible_.has_value());

  gpu_service_remote->OnForegrounded();
  gpu_service_remote.FlushForTesting();

  EXPECT_TRUE(visible_.has_value());
  EXPECT_TRUE(*visible_);

  gpu_service_remote->OnBackgrounded();
  gpu_service_remote.FlushForTesting();

  EXPECT_TRUE(visible_.has_value());
  EXPECT_FALSE(*visible_);
}

}  // namespace viz
