// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mirroring/service/mirroring_gpu_factories_factory.h"

#include <atomic>
#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "base/time/default_tick_clock.h"
#include "gpu/command_buffer/common/shared_image_capabilities.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_info.h"
#include "media/cast/cast_environment.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "services/viz/public/cpp/gpu/gpu.h"
#include "services/viz/public/mojom/gpu.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_implementation.h"

namespace mirroring {

class MirroringGpuFactoriesFactoryTest : public ::testing::Test {
 public:
  MirroringGpuFactoriesFactoryTest()
      : task_environment_(
            base::test::TaskEnvironment::MainThreadType::UI,
            base::test::TaskEnvironment::ThreadPoolExecutionMode::ASYNC),
        io_thread_("TestIO") {
    main_task_runner_ = task_environment_.GetMainThreadTaskRunner();
    video_task_runner_ =
        base::ThreadPool::CreateSingleThreadTaskRunner({base::MayBlock()});

    CHECK(io_thread_.StartWithOptions(
        base::Thread::Options(base::MessagePumpType::IO, 0)));

    cast_environment_ = base::MakeRefCounted<media::cast::CastEnvironment>(
        *base::DefaultTickClock::GetInstance(), main_task_runner_,
        main_task_runner_,   // audio
        video_task_runner_,  // video
        base::BindOnce(
            &MirroringGpuFactoriesFactoryTest::OnCastEnvironmentDestroyed,
            base::Unretained(this)));

    mojo::PendingRemote<viz::mojom::Gpu> gpu_remote;
    io_thread_.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&StubGpu::BindReceiver, base::Unretained(&stub_gpu_),
                       gpu_remote.InitWithNewPipeAndPassReceiver()));
    gpu_ = viz::Gpu::Create(std::move(gpu_remote), io_thread_.task_runner());
  }

  void TearDown() override {
    // Ensure all pending tasks on the video thread (like factory destruction)
    // are processed before we start tearing down.
    base::RunLoop video_flush_loop;
    video_task_runner_->PostTask(FROM_HERE, video_flush_loop.QuitClosure());
    video_flush_loop.Run();

    // CastEnvironment holds a LogEventDispatcher which uses DeleteSoon to
    // destroy its internal implementation on the main thread. We must ensure
    // that these tasks run before the TaskEnvironment is destroyed to avoid
    // memory leaks.
    cast_environment_.reset();
    gpu_.reset();
    base::RunLoop run_loop;
    environment_destruction_closure_ = run_loop.QuitClosure();
    run_loop.Run();

    base::RunLoop io_loop;
    io_thread_.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&StubGpu::ResetReceiver, base::Unretained(&stub_gpu_))
            .Then(io_loop.QuitClosure()));
    io_loop.Run();

    io_thread_.Stop();
  }

  void OnCastEnvironmentDestroyed() {
    if (environment_destruction_closure_) {
      std::move(environment_destruction_closure_).Run();
    }
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  base::Thread io_thread_;
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> video_task_runner_;
  scoped_refptr<media::cast::CastEnvironment> cast_environment_;
  std::unique_ptr<viz::Gpu> gpu_;
  base::OnceClosure environment_destruction_closure_;

  // Dummy receiver to keep the mojo connection alive.
  class StubGpu : public viz::mojom::Gpu {
   public:
    StubGpu() = default;
    ~StubGpu() override = default;

    void BindReceiver(mojo::PendingReceiver<viz::mojom::Gpu> receiver) {
      receiver_.Bind(std::move(receiver));
    }

    void ResetReceiver() { receiver_.reset(); }

    void EstablishGpuChannel(EstablishGpuChannelCallback callback) override {
      if (!channel_handle_valid_) {
        std::move(callback).Run(
            /*client_id=*/0, mojo::ScopedMessagePipeHandle(), gpu::GPUInfo(),
            gpu::GpuFeatureInfo(), gpu::SharedImageCapabilities());
        return;
      }
      mojo::MessagePipe pipe;
      held_channel_handle_ = std::move(pipe.handle1);
      gpu::GPUInfo info;
      info.gl_implementation_parts.gl = gl_disabled_
                                            ? gl::kGLImplementationDisabled
                                            : gl::kGLImplementationEGLGLES2;
      std::move(callback).Run(/*client_id=*/1, std::move(pipe.handle0), info,
                              gpu::GpuFeatureInfo(),
                              gpu::SharedImageCapabilities());
    }

    void CreateVideoEncodeAcceleratorProvider(
        mojo::PendingReceiver<media::mojom::VideoEncodeAcceleratorProvider>
            receiver) override {}
#if BUILDFLAG(IS_CHROMEOS)
    void CreateJpegDecodeAccelerator(
        mojo::PendingReceiver<chromeos_camera::mojom::MjpegDecodeAccelerator>
            jda_receiver) override {}
#endif

    void set_gl_disabled(bool disabled) { gl_disabled_ = disabled; }
    void set_channel_handle_valid(bool valid) { channel_handle_valid_ = valid; }

   private:
    std::atomic<bool> gl_disabled_{true};
    std::atomic<bool> channel_handle_valid_{true};
    mojo::ScopedMessagePipeHandle held_channel_handle_;
    mojo::Receiver<viz::mojom::Gpu> receiver_{this};
  };
  StubGpu stub_gpu_;
};

TEST_F(MirroringGpuFactoriesFactoryTest, DestroysOnVideoThread) {
  auto factory = MirroringGpuFactoriesFactory::Create(
      cast_environment_, *gpu_, base::DoNothing(), base::DoNothing());
  ASSERT_TRUE(factory.has_value());

  // Resetting on the main thread should trigger a task on the video thread.
  factory.reset();

  base::RunLoop run_loop;
  video_task_runner_->PostTask(FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
}

TEST_F(MirroringGpuFactoriesFactoryTest, TriggersContextLostCallback) {
  base::RunLoop run_loop;
  bool lost_called = false;

  auto factory = MirroringGpuFactoriesFactory::Create(
      cast_environment_, *gpu_,
      base::BindOnce([](bool* called) { *called = true; }, &lost_called),
      base::DoNothing());
  ASSERT_TRUE(factory.has_value());

  video_task_runner_->PostTask(FROM_HERE,
                               base::BindOnce(
                                   [](MirroringGpuFactoriesFactory* factory,
                                      base::OnceClosure quit_closure) {
                                     factory->OnContextLost();
                                     std::move(quit_closure).Run();
                                   },
                                   factory->get(), run_loop.QuitClosure()));
  run_loop.Run();

  EXPECT_TRUE(lost_called);

  // Teardown is deferred until the owner drops its reference to the factory,
  // which destroys the factory on the video thread.
  factory.reset();

  base::RunLoop cleanup_loop;
  video_task_runner_->PostTask(FROM_HERE, cleanup_loop.QuitClosure());
  cleanup_loop.Run();
}

TEST_F(MirroringGpuFactoriesFactoryTest,
       MaintainsValidInstanceAcrossContextLoss) {
  base::RunLoop run_loop;
  bool lost_called = false;

  auto factory = MirroringGpuFactoriesFactory::Create(
      cast_environment_, *gpu_,
      base::BindOnce([](bool* called) { *called = true; }, &lost_called),
      base::DoNothing());
  ASSERT_TRUE(factory.has_value());

  media::GpuVideoAcceleratorFactories& factories = (*factory)->GetInstance();

  video_task_runner_->PostTask(FROM_HERE,
                               base::BindOnce(
                                   [](MirroringGpuFactoriesFactory* factory,
                                      base::OnceClosure quit_closure) {
                                     factory->OnContextLost();
                                     std::move(quit_closure).Run();
                                   },
                                   factory->get(), run_loop.QuitClosure()));
  run_loop.Run();

  EXPECT_TRUE(lost_called);

  // The factories reference must remain valid and callable after context loss.
  EXPECT_TRUE(factories.IsGpuVideoEncodeAcceleratorEnabled());
  EXPECT_EQ(factories.GetTaskRunner(), video_task_runner_);

  factory.reset();

  base::RunLoop cleanup_loop;
  video_task_runner_->PostTask(FROM_HERE, cleanup_loop.QuitClosure());
  cleanup_loop.Run();
}

TEST_F(MirroringGpuFactoriesFactoryTest, HandlesBindFailureGracefully) {
  base::RunLoop run_loop;
  bool lost_called = false;

  stub_gpu_.set_gl_disabled(true);

  auto factory = MirroringGpuFactoriesFactory::Create(
      cast_environment_, *gpu_,
      base::BindOnce(
          [](bool* called, base::OnceClosure quit) {
            *called = true;
            std::move(quit).Run();
          },
          &lost_called, run_loop.QuitClosure()),
      base::DoNothing());
  ASSERT_TRUE(factory.has_value());

  media::GpuVideoAcceleratorFactories& factories = (*factory)->GetInstance();

  // Wait for the video thread to attempt binding and trigger context lost
  // callback.
  run_loop.Run();

  EXPECT_TRUE(lost_called);

  // The factories reference must remain valid and safe to query even after bind
  // failure.
  EXPECT_TRUE(factories.IsGpuVideoEncodeAcceleratorEnabled());
  EXPECT_EQ(factories.GetTaskRunner(), video_task_runner_);

  factory.reset();

  base::RunLoop cleanup_loop;
  video_task_runner_->PostTask(FROM_HERE, cleanup_loop.QuitClosure());
  cleanup_loop.Run();
}

TEST_F(MirroringGpuFactoriesFactoryTest,
       HandlesGpuChannelEstablishmentFailure) {
  stub_gpu_.set_channel_handle_valid(false);

  auto factory = MirroringGpuFactoriesFactory::Create(
      cast_environment_, *gpu_, base::DoNothing(), base::DoNothing());
  EXPECT_FALSE(factory.has_value());
}

}  // namespace mirroring
