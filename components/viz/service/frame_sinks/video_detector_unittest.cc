// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/frame_sinks/video_detector.h"

#include <algorithm>
#include <memory>
#include <set>
#include <utility>

#include "base/compiler_specific.h"
#include "base/containers/circular_deque.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "components/viz/common/features.h"
#include "components/viz/common/quads/surface_draw_quad.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "components/viz/service/display/display_resource_provider_software.h"
#include "components/viz/service/display/surface_aggregator.h"
#include "components/viz/service/display_embedder/server_shared_bitmap_manager.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "components/viz/test/compositor_frame_helpers.h"
#include "components/viz/test/fake_compositor_frame_sink_client.h"
#include "components/viz/test/surface_id_allocator_set.h"
#include "gpu/command_buffer/service/scheduler.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/viz/public/mojom/compositing/video_detector_observer.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/rect.h"

namespace viz {

namespace {

// Implementation that just records video state changes.
class TestObserver : public mojom::VideoDetectorObserver {
 public:
  TestObserver() = default;

  TestObserver(const TestObserver&) = delete;
  TestObserver& operator=(const TestObserver&) = delete;

  void Bind(mojo::PendingReceiver<mojom::VideoDetectorObserver> receiver) {
    receiver_.Bind(std::move(receiver));
  }

  bool IsEmpty() {
    receiver_.FlushForTesting();
    return states_.empty();
  }

  void Reset() {
    receiver_.FlushForTesting();
    states_.clear();
  }

  // Pops and returns the earliest-received state.
  bool PopState() {
    receiver_.FlushForTesting();
    CHECK(!states_.empty());
    uint8_t first_state = states_.front();
    states_.pop_front();
    return first_state;
  }

  // mojom::VideoDetectorObserver implementation.
  void OnVideoActivityStarted() override { states_.push_back(true); }
  void OnVideoActivityEnded() override { states_.push_back(false); }

 private:
  // States in the order they were received.
  base::circular_deque<bool> states_;

  mojo::Receiver<mojom::VideoDetectorObserver> receiver_{this};
};

}  // namespace

class VideoDetectorTest : public testing::Test {
 public:
  VideoDetectorTest()
      : surface_aggregator_(frame_sink_manager_.surface_manager(),
                            &resource_provider_,
                            false) {}

  VideoDetectorTest(const VideoDetectorTest&) = delete;
  VideoDetectorTest& operator=(const VideoDetectorTest&) = delete;

  ~VideoDetectorTest() override = default;

  void SetUp() override {
    mock_task_runner_ = base::MakeRefCounted<base::TestMockTimeTaskRunner>(
        base::Time() + base::Seconds(1), base::TimeTicks() + base::Seconds(1));

    detector_ = frame_sink_manager_.CreateVideoDetectorForTesting(
        mock_task_runner_->GetMockTickClock(), mock_task_runner_);

    mojo::PendingRemote<mojom::VideoDetectorObserver> video_detector_observer;
    observer_.Bind(video_detector_observer.InitWithNewPipeAndPassReceiver());
    detector_->AddObserver(std::move(video_detector_observer));

    root_frame_sink_ = CreateFrameSink();
    ParentLocalSurfaceIdAllocator* allocator =
        allocators_.GetAllocator(root_frame_sink_->frame_sink_id());
    allocator->GenerateId();
    root_frame_sink_->SubmitCompositorFrame(
        allocator->GetCurrentLocalSurfaceId(), MakeDefaultCompositorFrame());
  }

 protected:
  // Constants placed here for convenience.
  static constexpr int kMinFps = VideoDetector::kMinFramesPerSecond;
  static constexpr gfx::Rect kMinRect =
      gfx::Rect(VideoDetector::kMinDamageWidth,
                VideoDetector::kMinDamageHeight);
  static constexpr base::TimeDelta kMinDuration =
      VideoDetector::kMinVideoDuration;
  static constexpr base::TimeDelta kTimeout = VideoDetector::kMaxVideoTimeout;

  // Move |detector_|'s idea of the current time forward by |delta|.
  void AdvanceTime(base::TimeDelta delta) {
    mock_task_runner_->FastForwardBy(delta);
  }

  void CreateDisplayFrame() {
    surface_aggregator_.Aggregate(root_frame_sink_->last_activated_surface_id(),
                                  mock_task_runner_->NowTicks(),
                                  gfx::OVERLAY_TRANSFORM_NONE);
  }

  void EmbedClient(CompositorFrameSinkSupport* frame_sink) {
    embedded_clients_.insert(frame_sink);
    SubmitRootFrame();
  }

  void SubmitRootFrame() {
    CompositorFrame frame = MakeDefaultCompositorFrame();
    CompositorRenderPass* render_pass = frame.render_pass_list.back().get();
    SharedQuadState* shared_quad_state =
        render_pass->CreateAndAppendSharedQuadState();
    for (CompositorFrameSinkSupport* frame_sink : embedded_clients_) {
      SurfaceDrawQuad* quad =
          render_pass->CreateAndAppendDrawQuad<SurfaceDrawQuad>();
      quad->SetNew(
          shared_quad_state, gfx::Rect(0, 0, 10, 10), gfx::Rect(0, 0, 5, 5),
          SurfaceRange(std::nullopt, frame_sink->last_activated_surface_id()),
          SkColors::kMagenta, /*stretch_content_to_fill_bounds=*/false);
    }
    root_frame_sink_->SubmitCompositorFrame(
        root_frame_sink_->last_activated_local_surface_id(), std::move(frame));
  }

  void SendUpdate(CompositorFrameSinkSupport* frame_sink,
                  const gfx::Rect& damage,
                  bool may_contain_video,
                  bool use_per_quad_damage) {
    LocalSurfaceId local_surface_id =
        frame_sink->last_activated_local_surface_id();
    if (!local_surface_id.is_valid()) {
      ParentLocalSurfaceIdAllocator* allocator =
          allocators_.GetAllocator(frame_sink->frame_sink_id());
      allocator->GenerateId();
      local_surface_id = allocator->GetCurrentLocalSurfaceId();
    }
    frame_sink->SubmitCompositorFrame(
        local_surface_id,
        use_per_quad_damage
            ? MakeDamagedCompositorFrameWithPerQuadDamage(damage,
                                                          may_contain_video)
            : MakeDamagedCompositorFrame(damage, may_contain_video));
  }

  // Report updates to |client| of area |damage| at a rate of
  // |updates_per_second| over |duration|. The first update will be sent
  // immediately and time will have advanced by |duration| upon returning.
  void SendUpdates(CompositorFrameSinkSupport* frame_sink,
                   const gfx::Rect& damage,
                   bool may_contain_video,
                   bool use_per_quad_damage,
                   int updates_per_second,
                   base::TimeDelta duration) {
    const base::TimeDelta time_between_updates =
        base::Seconds(1.0 / updates_per_second);
    for (base::TimeDelta d; d < duration; d += time_between_updates) {
      SendUpdate(frame_sink, damage, may_contain_video, use_per_quad_damage);
      CreateDisplayFrame();
      AdvanceTime(std::min(time_between_updates, duration - d));
    }
  }

  std::unique_ptr<CompositorFrameSinkSupport> CreateFrameSink() {
    constexpr bool is_root = false;
    static uint32_t client_id = 1;
    FrameSinkId frame_sink_id(client_id++, 0);
    frame_sink_manager_.RegisterFrameSinkId(frame_sink_id,
                                            true /* report_activation */);
    auto frame_sink = std::make_unique<CompositorFrameSinkSupport>(
        &frame_sink_client_, &frame_sink_manager_, frame_sink_id, is_root);
    SendUpdate(frame_sink.get(), gfx::Rect(), /*may_contain_video*/ false,
               /*use_per_quad_damage*/ false);
    return frame_sink;
  }

  TestObserver observer_;
  scoped_refptr<base::TestMockTimeTaskRunner> mock_task_runner_;

 protected:
  CompositorFrame MakeDamagedCompositorFrame(const gfx::Rect& damage,
                                             bool may_contain_video) {
    constexpr gfx::Rect kFrameSinkRect(10000, 10000);
    auto frame =
        CompositorFrameBuilder().AddRenderPass(kFrameSinkRect, damage).Build();
    frame.metadata.may_contain_video = may_contain_video;

    return frame;
  }

  CompositorFrame MakeDamagedCompositorFrameWithPerQuadDamage(
      const gfx::Rect& damage,
      bool may_contain_video) {
    constexpr gfx::Rect kFrameSinkRect(10000, 10000);
    auto frame =
        CompositorFrameBuilder()
            .AddRenderPass(RenderPassBuilder(kFrameSinkRect)
                               .AddTextureQuad(kFrameSinkRect, ResourceId(1234))
                               .SetQuadDamageRect(kFrameSinkRect))
            .PopulateResources()
            .Build();
    frame.metadata.may_contain_video = may_contain_video;

    return frame;
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  ServerSharedBitmapManager shared_bitmap_manager_;
  gpu::SharedImageManager shared_image_manager_;
  gpu::SyncPointManager sync_point_manager_;
  gpu::Scheduler gpu_scheduler_{&sync_point_manager_};
  FrameSinkManagerImpl frame_sink_manager_{
      FrameSinkManagerImpl::InitParams(&shared_bitmap_manager_)};
  DisplayResourceProviderSoftware resource_provider_{
      &shared_bitmap_manager_, &shared_image_manager_, &sync_point_manager_,
      &gpu_scheduler_};
  FakeCompositorFrameSinkClient frame_sink_client_;
  SurfaceIdAllocatorSet allocators_;
  SurfaceAggregator surface_aggregator_;
  std::unique_ptr<CompositorFrameSinkSupport> root_frame_sink_;
  std::set<raw_ptr<CompositorFrameSinkSupport, SetExperimental>>
      embedded_clients_;
  raw_ptr<VideoDetector> detector_;
};


constexpr gfx::Rect VideoDetectorTest::kMinRect;
constexpr base::TimeDelta VideoDetectorTest::kMinDuration;
constexpr base::TimeDelta VideoDetectorTest::kTimeout;

// Verify that VideoDetector does not report clients with small damage rects.
TEST_F(VideoDetectorTest, DontReportWhenDamageTooSmall) {
  std::unique_ptr<CompositorFrameSinkSupport> frame_sink = CreateFrameSink();
  EmbedClient(frame_sink.get());

  {
    // Send damages with a smaller width than |kMinRect|. Make sure video
    // activity isn't detected.
    gfx::Rect rect = kMinRect;
    rect.Inset(gfx::Insets::TLBR(0, 0, 0, 1));
    SendUpdates(frame_sink.get(), rect, /*may_contain_video=*/true,
                /*use_per_quad_damage=*/false, 2 * kMinFps, 2 * kMinDuration);
    EXPECT_TRUE(observer_.IsEmpty());
  }

  {
    // Send damages with a smaller height than |kMinRect|. Make sure video
    // activity isn't detected.
    gfx::Rect rect = kMinRect;
    rect.Inset(gfx::Insets::TLBR(0, 0, 0, 1));
    SendUpdates(frame_sink.get(), rect, /*may_contain_video=*/true,
                /*use_per_quad_damage=*/false, 2 * kMinFps, 2 * kMinDuration);
    EXPECT_TRUE(observer_.IsEmpty());
  }
}

// Verify that VideoDetector does not report clients with a low frame rate.
TEST_F(VideoDetectorTest, DontReportWhenFramerateTooLow) {
  std::unique_ptr<CompositorFrameSinkSupport> frame_sink = CreateFrameSink();
  EmbedClient(frame_sink.get());
  SendUpdates(frame_sink.get(), kMinRect, /*may_contain_video=*/true,
              /*use_per_quad_damage=*/false, kMinFps - 5, 2 * kMinDuration);
  EXPECT_TRUE(observer_.IsEmpty());
}

// Verify that VideoDetector does not report clients until they have played for
// the minimum necessary duration.
TEST_F(VideoDetectorTest, DontReportWhenNotPlayingLongEnough) {
  std::unique_ptr<CompositorFrameSinkSupport> frame_sink = CreateFrameSink();
  EmbedClient(frame_sink.get());
  SendUpdates(frame_sink.get(), kMinRect, /*may_contain_video=*/true,
              /*use_per_quad_damage=*/false, 2 * kMinFps, 0.5 * kMinDuration);
  EXPECT_TRUE(observer_.IsEmpty());

  SendUpdates(frame_sink.get(), kMinRect, /*may_contain_video=*/true,
              /*use_per_quad_damage=*/false, 2 * kMinFps, 0.6 * kMinDuration);
  EXPECT_TRUE(observer_.PopState());
  EXPECT_TRUE(observer_.IsEmpty());
}

// Verify that VideoDetector does not report clients that are not visible
// on screen.
TEST_F(VideoDetectorTest, DontReportWhenClientHidden) {
  std::unique_ptr<CompositorFrameSinkSupport> frame_sink = CreateFrameSink();

  SendUpdates(frame_sink.get(), kMinRect, /*may_contain_video=*/true,
              /*use_per_quad_damage=*/false, kMinFps + 5, 2 * kMinDuration);
  EXPECT_TRUE(observer_.IsEmpty());

  // Make the client visible.
  observer_.Reset();
  AdvanceTime(kTimeout);
  EmbedClient(frame_sink.get());
  SendUpdates(frame_sink.get(), kMinRect, /*may_contain_video=*/true,
              /*use_per_quad_damage=*/false, kMinFps + 5, 2 * kMinDuration);
  EXPECT_TRUE(observer_.PopState());
  EXPECT_TRUE(observer_.IsEmpty());
}

TEST_F(VideoDetectorTest, DoesNotReportNonVideoFrames) {
  const base::TimeDelta kDuration = kMinDuration + base::Milliseconds(100);
  std::unique_ptr<CompositorFrameSinkSupport> frame_sink = CreateFrameSink();
  EmbedClient(frame_sink.get());
  SendUpdates(frame_sink.get(), kMinRect, /*may_contain_video=*/false,
              /*use_per_quad_damage=*/false, kMinFps + 5, kDuration);
  EXPECT_TRUE(observer_.IsEmpty());
}

// Turn video activity on and off. Make sure the observers are notified
// properly.
TEST_F(VideoDetectorTest, ReportStartAndStop) {
  const base::TimeDelta kDuration = kMinDuration + base::Milliseconds(100);
  std::unique_ptr<CompositorFrameSinkSupport> frame_sink = CreateFrameSink();
  EmbedClient(frame_sink.get());
  SendUpdates(frame_sink.get(), kMinRect, /*may_contain_video=*/true,
              /*use_per_quad_damage=*/false, kMinFps + 5, kDuration);
  EXPECT_TRUE(observer_.PopState());
  EXPECT_TRUE(observer_.IsEmpty());

  AdvanceTime(kTimeout);
  EXPECT_FALSE(observer_.PopState());
  EXPECT_TRUE(observer_.IsEmpty());

  // Start playing again.
  SendUpdates(frame_sink.get(), kMinRect, /*may_contain_video=*/true,
              /*use_per_quad_damage=*/false, kMinFps + 5, kDuration);
  EXPECT_TRUE(observer_.PopState());
  EXPECT_TRUE(observer_.IsEmpty());

  AdvanceTime(kTimeout);
  EXPECT_FALSE(observer_.PopState());
  EXPECT_TRUE(observer_.IsEmpty());
}

// If there are multiple clients playing video, make sure that observers only
// receive a single notification.
TEST_F(VideoDetectorTest, ReportOnceForMultipleClients) {
  std::unique_ptr<CompositorFrameSinkSupport> frame_sink1 = CreateFrameSink();
  std::unique_ptr<CompositorFrameSinkSupport> frame_sink2 = CreateFrameSink();
  EmbedClient(frame_sink1.get());
  EmbedClient(frame_sink2.get());

  // Even if there's video playing in both clients, the observer should only
  // receive a single notification.
  constexpr int fps = 2 * kMinFps;
  constexpr base::TimeDelta time_between_updates = base::Seconds(1.0 / fps);
  for (base::TimeDelta d; d < 2 * kMinDuration; d += time_between_updates) {
    SendUpdate(frame_sink1.get(), kMinRect, /*may_contain_video=*/true,
               /*use_per_quad_damage=*/false);
    SendUpdate(frame_sink2.get(), kMinRect, /*may_contain_video=*/true,
               /*use_per_quad_damage=*/false);
    AdvanceTime(time_between_updates);
    CreateDisplayFrame();
  }
  EXPECT_TRUE(observer_.PopState());
  EXPECT_TRUE(observer_.IsEmpty());
}

TEST_F(VideoDetectorTest, ReportBasedOnPerQuadDamage) {
  const base::TimeDelta kDuration = kMinDuration + base::Milliseconds(100);
  std::unique_ptr<CompositorFrameSinkSupport> frame_sink = CreateFrameSink();
  EmbedClient(frame_sink.get());
  SendUpdates(frame_sink.get(), kMinRect, /*may_contain_video=*/true,
              /*use_per_quad_damage=*/true, kMinFps + 5, kDuration);
  EXPECT_TRUE(observer_.PopState());
  EXPECT_TRUE(observer_.IsEmpty());

  AdvanceTime(kTimeout);
  EXPECT_FALSE(observer_.PopState());
  EXPECT_TRUE(observer_.IsEmpty());
}

}  // namespace viz
