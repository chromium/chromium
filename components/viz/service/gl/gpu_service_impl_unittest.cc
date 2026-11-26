// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/gl/gpu_service_impl.h"

#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/viz/common/resources/peak_gpu_memory_tracker_util.h"
#include "components/viz/service/gl/mock_gpu_service_impl.h"
#include "components/viz/service/input/peak_gpu_memory_tracker_impl.h"
#include "gpu/config/gpu_info.h"
#include "gpu/ipc/service/gpu_watchdog_thread.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/viz/public/mojom/gpu.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_utils.h"
#include "ui/gl/init/gl_factory.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "components/viz/common/features.h"
#include "components/viz/service/display_embedder/compositor_gpu_thread.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#endif

namespace viz {

namespace {

const uint64_t kPeakMemoryMB = 42u;
const uint64_t kPeakMemory = kPeakMemoryMB * 1048576u;

}  // namespace

class GpuServiceTest : public testing::Test {
 public:
  GpuServiceTest()
      : io_thread_("TestIOThread"),
        wait_(base::WaitableEvent::ResetPolicy::MANUAL,
              base::WaitableEvent::InitialState::NOT_SIGNALED) {}

  GpuServiceTest(const GpuServiceTest&) = delete;
  GpuServiceTest& operator=(const GpuServiceTest&) = delete;

  ~GpuServiceTest() override = default;

  MockGpuServiceImpl* mock_gpu_service() {
    return static_cast<MockGpuServiceImpl*>(gpu_service_.get());
  }
  GpuServiceImpl* gpu_service() { return gpu_service_.get(); }

  void DestroyService() { gpu_service_ = nullptr; }

  void BlockIOThread() {
    wait_.Reset();
    io_runner()->PostTask(FROM_HERE, wait_.GetWaitCallbackForTesting());
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
    gpu_service_ = std::make_unique<testing::NiceMock<MockGpuServiceImpl>>(
        gpu::GpuPreferences(), gpu_info, gpu::GpuFeatureInfo(), gpu::GPUInfo(),
        gpu::GpuFeatureInfo(), gfx::GpuExtraInfo(), std::move(init_params));
  }

  void TearDown() override {
    DestroyService();
    base::RunLoop runloop;
    runloop.RunUntilIdle();
    io_thread_.Stop();
  }

  std::optional<base::Process::Priority> priority_;

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

// Tests that the visibility callback gets called when visibility changes.
TEST_F(GpuServiceTest, VisibilityCallbackCalled) {
  mojo::Remote<mojom::GpuService> gpu_service_remote;
  gpu_service()->Bind(gpu_service_remote.BindNewPipeAndPassReceiver());

  mojo::PendingRemote<mojom::GpuHost> gpu_host_proxy;
  std::ignore = gpu_host_proxy.InitWithNewPipeAndPassReceiver();
  gpu_service()->InitializeWithHost(
      std::move(gpu_host_proxy), gpu::GpuProcessShmCount(),
      gl::init::CreateOffscreenGLSurface(gl::GetDefaultDisplay(), gfx::Size()),
      mojom::GpuServiceCreationParams::New());
  gpu_service_remote.FlushForTesting();

  gpu_service()->SetPriorityChangedCallback(base::BindRepeating(
      [](GpuServiceTest* test, base::Process::Priority priority) {
        test->priority_ = priority;
      },
      base::Unretained(this)));
  EXPECT_FALSE(priority_.has_value());

  gpu_service_remote->OnForegrounded();
  gpu_service_remote.FlushForTesting();

  EXPECT_TRUE(priority_.has_value());
  EXPECT_EQ(*priority_, base::Process::Priority::kUserBlocking);

  gpu_service_remote->OnBackgrounded();
  gpu_service_remote.FlushForTesting();

  EXPECT_TRUE(priority_.has_value());
  EXPECT_EQ(*priority_, base::Process::Priority::kBestEffort);
}

// Tests that when a PeakGpuMemoryTracker is destroyed, GpuService properly
// updates the histograms.
TEST_F(GpuServiceTest, PeakGpuMemoryCallback) {
  mojo::Remote<mojom::GpuService> gpu_service_remote;
  gpu_service()->Bind(gpu_service_remote.BindNewPipeAndPassReceiver());

  mojo::PendingRemote<mojom::GpuHost> gpu_host_proxy;
  std::ignore = gpu_host_proxy.InitWithNewPipeAndPassReceiver();
  gpu_service()->InitializeWithHost(
      std::move(gpu_host_proxy), gpu::GpuProcessShmCount(),
      gl::init::CreateOffscreenGLSurface(gl::GetDefaultDisplay(), gfx::Size()),
      mojom::GpuServiceCreationParams::New());
  gpu_service_remote.FlushForTesting();

  ON_CALL(*mock_gpu_service(), GetPeakMemoryUsageOnMainThread)
      .WillByDefault(
          [](uint32_t sequence_num,
             GpuServiceImpl::GetPeakMemoryUsageCallback callback) {
            ASSERT_EQ(GetPeakMemoryUsageRequestLocation(sequence_num),
                      SequenceLocation::kGpuProcess);
            base::flat_map<gpu::GpuPeakMemoryAllocationSource, uint64_t>
                allocation_per_source;
            allocation_per_source[gpu::GpuPeakMemoryAllocationSource::UNKNOWN] =
                kPeakMemory;
            std::move(callback).Run(kPeakMemory, allocation_per_source);
          });

  base::HistogramTester histogram;
  auto tracker = std::make_unique<PeakGpuMemoryTrackerImpl>(
      PeakGpuMemoryTracker::Usage::PAGE_LOAD, gpu_service());

  // No report in response to creation.
  histogram.ExpectTotalCount("Memory.GPU.PeakMemoryUsage2.PageLoad", 0);
  histogram.ExpectTotalCount(
      "Memory.GPU.PeakMemoryAllocationSource.PageLoad.Unknown", 0);

  // Deleting the tracker should start a request for peak Gpu memory usage,
  // with the callback being a posted task.
  tracker.reset();
  gpu_service_remote.FlushForTesting();

  histogram.ExpectUniqueSample("Memory.GPU.PeakMemoryUsage2.PageLoad",
                               kPeakMemoryMB, 1);
  histogram.ExpectUniqueSample(
      "Memory.GPU.PeakMemoryAllocationSource2.PageLoad.Unknown", kPeakMemoryMB,
      1);
}

// Dawn info collection is expensive, so GpuServiceImpl should only collect it
// once per mode and return cached results for subsequent requests.
TEST_F(GpuServiceTest, DawnInfoCollectedOncePerMode) {
  mojo::Remote<mojom::GpuService> gpu_service_remote;
  gpu_service()->Bind(gpu_service_remote.BindNewPipeAndPassReceiver());

  constexpr char kInfoCollectionTime[] = "GPU.Dawn.InfoCollectionTimeMS";
  base::HistogramTester histogram;

  auto get_dawn_info = [&](bool collect_metrics) {
    base::test::TestFuture<const std::vector<std::string>&> future;
    gpu_service_remote->GetDawnInfo(collect_metrics, future.GetCallback());
    return future.Take();
  };

  // The first request for the info list collects it.
  std::vector<std::string> first_info_list =
      get_dawn_info(/*collect_metrics=*/false);
  histogram.ExpectTotalCount(kInfoCollectionTime, 1);

  // Subsequent requests return the same list without collecting it again.
  EXPECT_EQ(first_info_list, get_dawn_info(/*collect_metrics=*/false));
  histogram.ExpectTotalCount(kInfoCollectionTime, 1);

  // Metrics collection is tracked separately from the info list, and only
  // happens once. It never populates the info list.
  EXPECT_TRUE(get_dawn_info(/*collect_metrics=*/true).empty());
  histogram.ExpectTotalCount(kInfoCollectionTime, 2);
  EXPECT_TRUE(get_dawn_info(/*collect_metrics=*/true).empty());
  histogram.ExpectTotalCount(kInfoCollectionTime, 2);

  // Collecting metrics does not affect the cached info list.
  EXPECT_EQ(first_info_list, get_dawn_info(/*collect_metrics=*/false));
  histogram.ExpectTotalCount(kInfoCollectionTime, 2);
}

#if BUILDFLAG(IS_ANDROID)
class GpuServicePrewarmTest : public GpuServiceTest {
 public:
  struct PrewarmSnapshot {
    bool has_shared_context = false;
    bool has_real_gl_context = false;
    bool has_gr_context = false;
  };

  void SetUp() override {
    GpuServiceTest::SetUp();
    DestroyService();
  }

  PrewarmSnapshot RunPrewarmCase(bool enable_prewarm,
                                 bool skip_disabled_gl,
                                 gpu::GrContextType gr_context_type) {
    base::test::ScopedFeatureList feature_list;
    std::vector<base::test::FeatureRef> enabled;
    std::vector<base::test::FeatureRef> disabled;
    (enable_prewarm ? enabled : disabled)
        .push_back(features::kAndroidEarlyGpuChannelAndCompositorPrewarm);
    (skip_disabled_gl ? enabled : disabled)
        .push_back(features::kSkipDisabledGLContextOnDrDC);
    feature_list.InitWithFeatures(enabled, disabled);

    gpu::GpuPreferences gpu_preferences;
    gpu_preferences.gr_context_type = gr_context_type;
    gpu_preferences.use_passthrough_cmd_decoder =
        gl::UsePassthroughCommandDecoder(
            base::CommandLine::ForCurrentProcess());
    gpu::GPUInfo gpu_info;
    gpu_info.in_process_gpu = false;
    gpu::GpuFeatureInfo gpu_feature_info;
    gpu_feature_info.status_values
        [gpu::GPU_FEATURE_TYPE_DIRECT_RENDERING_DISPLAY_COMPOSITOR] =
        gpu::kGpuFeatureStatusEnabled;

    GpuServiceImpl::InitParams init_params;
    init_params.io_runner = io_runner();
    auto service = std::make_unique<testing::NiceMock<MockGpuServiceImpl>>(
        gpu_preferences, gpu_info, gpu_feature_info, gpu::GPUInfo(),
        gpu::GpuFeatureInfo(), gfx::GpuExtraInfo(), std::move(init_params));

    mojo::PendingRemote<mojom::GpuHost> gpu_host_proxy;
    std::ignore = gpu_host_proxy.InitWithNewPipeAndPassReceiver();
    service->InitializeWithHost(std::move(gpu_host_proxy),
                                gpu::GpuProcessShmCount(),
                                gl::init::CreateOffscreenGLSurface(
                                    gl::GetDefaultDisplay(), gfx::Size()),
                                mojom::GpuServiceCreationParams::New());

    PrewarmSnapshot snapshot;
    if (!service->compositor_gpu_thread()) {
      ADD_FAILURE() << "Expected DrDC CompositorGpuThread to be created";
      return snapshot;
    }

    base::WaitableEvent flushed(
        base::WaitableEvent::ResetPolicy::MANUAL,
        base::WaitableEvent::InitialState::NOT_SIGNALED);
    service->compositor_gpu_thread()->task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](PrewarmSnapshot* out_snapshot, base::WaitableEvent* done) {
              gpu::SharedContextState* state =
                  gpu::SharedContextState::GetForCurrentThread();
              out_snapshot->has_shared_context = (state != nullptr);
              out_snapshot->has_real_gl_context =
                  (state != nullptr && state->real_context() != nullptr);
              out_snapshot->has_gr_context =
                  (state != nullptr && state->gr_context() != nullptr);
              done->Signal();
            },
            &snapshot, &flushed));
    flushed.Wait();

    // Stop CompositorGpuThread and drain pending IO and main-thread tasks
    // before tearing down ScopedFeatureList.
    service.reset();
    base::RunLoop flush_loop;
    io_runner()->PostTaskAndReply(FROM_HERE, base::DoNothing(),
                                  flush_loop.QuitClosure());
    flush_loop.Run();
    return snapshot;
  }
};

// When the feature is disabled, InitializeWithHost leaves CompositorGpuThread's
// SharedContextState uninitialized until first compositor demand.
TEST_F(GpuServicePrewarmTest, DisabledByDefault) {
  PrewarmSnapshot snapshot =
      RunPrewarmCase(/*enable_prewarm=*/false, /*skip_disabled_gl=*/true,
                     gpu::GrContextType::kGL);
  EXPECT_FALSE(snapshot.has_shared_context);
}

// When GrContextType is kNone, prewarm is skipped because there is no Skia
// context to initialize and CleanUp() would call MakeCurrent() on a null
// GLContext under kSkipDisabledGLContextOnDrDC.
TEST_F(GpuServicePrewarmTest, SkipsGrContextTypeNone) {
  PrewarmSnapshot snapshot =
      RunPrewarmCase(/*enable_prewarm=*/true, /*skip_disabled_gl=*/true,
                     gpu::GrContextType::kNone);
  EXPECT_FALSE(snapshot.has_shared_context);
}

// On non-GL backends, prewarm is skipped when kSkipDisabledGLContextOnDrDC is
// disabled so startup does not allocate an unused DrDC GLContext.
TEST_F(GpuServicePrewarmTest, SkipsNonGLWithoutSkipDisabledGLContext) {
  PrewarmSnapshot snapshot =
      RunPrewarmCase(/*enable_prewarm=*/true, /*skip_disabled_gl=*/false,
                     gpu::GrContextType::kVulkan);
  EXPECT_FALSE(snapshot.has_shared_context);
}

// On an active GL backend, SharedContextState, GLContext, and Ganesh GrContext
// are initialized asynchronously on CompositorGpuThread during
// InitializeWithHost and torn down cleanly in CleanUp().
TEST_F(GpuServicePrewarmTest, PrewarmsGLSharedContext) {
  PrewarmSnapshot snapshot =
      RunPrewarmCase(/*enable_prewarm=*/true, /*skip_disabled_gl=*/true,
                     gpu::GrContextType::kGL);
  EXPECT_TRUE(snapshot.has_shared_context);
  EXPECT_TRUE(snapshot.has_real_gl_context);
  EXPECT_TRUE(snapshot.has_gr_context);
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace viz
