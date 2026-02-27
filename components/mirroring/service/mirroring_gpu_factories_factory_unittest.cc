// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mirroring/service/mirroring_gpu_factories_factory.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "base/time/default_tick_clock.h"
#include "media/cast/cast_environment.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/viz/public/cpp/gpu/gpu.h"
#include "services/viz/public/mojom/gpu.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mirroring {

class MirroringGpuFactoriesFactoryTest : public ::testing::Test {
 public:
  MirroringGpuFactoriesFactoryTest()
      : task_environment_(
            base::test::TaskEnvironment::MainThreadType::UI,
            base::test::TaskEnvironment::ThreadPoolExecutionMode::ASYNC) {
    main_task_runner_ = task_environment_.GetMainThreadTaskRunner();
    video_task_runner_ =
        base::ThreadPool::CreateSingleThreadTaskRunner({base::MayBlock()});

    cast_environment_ = base::MakeRefCounted<media::cast::CastEnvironment>(
        *base::DefaultTickClock::GetInstance(), main_task_runner_,
        main_task_runner_,   // audio
        video_task_runner_,  // video
        base::BindOnce(
            &MirroringGpuFactoriesFactoryTest::OnCastEnvironmentDestroyed,
            base::Unretained(this)));

    mojo::PendingRemote<viz::mojom::Gpu> gpu_remote;
    // Bind the receiver to avoid disconnects during the test.
    gpu_receiver_.Bind(gpu_remote.InitWithNewPipeAndPassReceiver());
    gpu_ = viz::Gpu::Create(std::move(gpu_remote), main_task_runner_);
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
  }

  void OnCastEnvironmentDestroyed() {
    if (environment_destruction_closure_) {
      std::move(environment_destruction_closure_).Run();
    }
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> video_task_runner_;
  scoped_refptr<media::cast::CastEnvironment> cast_environment_;
  std::unique_ptr<viz::Gpu> gpu_;
  base::OnceClosure environment_destruction_closure_;

 private:
  // Dummy receiver to keep the mojo connection alive.
  class StubGpu : public viz::mojom::Gpu {
   public:
    StubGpu() = default;
    ~StubGpu() override = default;

    void EstablishGpuChannel(EstablishGpuChannelCallback callback) override {}
    void CreateVideoEncodeAcceleratorProvider(
        mojo::PendingReceiver<media::mojom::VideoEncodeAcceleratorProvider>
            receiver) override {}
#if BUILDFLAG(IS_CHROMEOS)
    void CreateJpegDecodeAccelerator(
        mojo::PendingReceiver<chromeos_camera::mojom::MjpegDecodeAccelerator>
            jda_receiver) override {}
#endif
  };
  StubGpu stub_gpu_;
  mojo::Receiver<viz::mojom::Gpu> gpu_receiver_{&stub_gpu_};
};

TEST_F(MirroringGpuFactoriesFactoryTest, DestroysOnVideoThread) {
  auto factory = MirroringGpuFactoriesFactory::Create(
      cast_environment_, *gpu_, base::DoNothing(), base::DoNothing());

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

  video_task_runner_->PostTask(FROM_HERE,
                               base::BindOnce(
                                   [](MirroringGpuFactoriesFactory* factory,
                                      base::OnceClosure quit_closure) {
                                     factory->OnContextLost();
                                     std::move(quit_closure).Run();
                                   },
                                   factory.get(), run_loop.QuitClosure()));
  run_loop.Run();

  EXPECT_TRUE(lost_called);

  // The factory should have been reset in OnContextLost if it triggers
  // its own destruction (which OpenscreenSessionHost does). In this test,
  // we just manually reset it.
  factory.reset();

  base::RunLoop cleanup_loop;
  video_task_runner_->PostTask(FROM_HERE, cleanup_loop.QuitClosure());
  cleanup_loop.Run();
}

}  // namespace mirroring
