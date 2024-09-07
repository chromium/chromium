// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/frame_sinks/frame_sink_bundle_impl.h"

#include <cstdint>
#include <optional>
#include <utility>

#include "base/auto_reset.h"
#include "base/json/values_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/task_environment.h"
#include "base/unguessable_token.h"
#include "components/viz/common/features.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/resources/resource_id.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "components/viz/common/surfaces/frame_sink_bundle_id.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/common/surfaces/local_surface_id.h"
#include "components/viz/service/display_embedder/server_shared_bitmap_manager.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "components/viz/test/compositor_frame_helpers.h"
#include "components/viz/test/fake_external_begin_frame_source.h"
#include "components/viz/test/mock_compositor_frame_sink_client.h"
#include "components/viz/test/mock_display_client.h"
#include "components/viz/test/test_output_surface_provider.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/viz/public/mojom/compositing/compositor_frame_sink.mojom-params-data.h"
#include "services/viz/public/mojom/compositing/compositor_frame_sink.mojom.h"
#include "services/viz/public/mojom/compositing/frame_sink_bundle.mojom-forward.h"
#include "services/viz/public/mojom/compositing/frame_sink_bundle.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/khronos/GLES2/gl2.h"

namespace viz {
namespace {

using ::testing::ElementsAre;
using ::testing::UnorderedElementsAre;

constexpr FrameSinkId kRootFrame(1, 1);
constexpr FrameSinkBundleId kBundleId(2, 1);
constexpr FrameSinkId kMainFrame(2, 1);
constexpr FrameSinkId kSubFrameA(2, 2);
constexpr FrameSinkId kSubFrameB(2, 3);
constexpr FrameSinkId kSubFrameC(2, 4);

const base::UnguessableToken kSurfaceTokenA =
    base::UnguessableToken::CreateForTesting(3, 42);
const base::UnguessableToken kSurfaceTokenB =
    base::UnguessableToken::CreateForTesting(5, 42);
const base::UnguessableToken kSurfaceTokenC =
    base::UnguessableToken::CreateForTesting(7, 42);

const LocalSurfaceId kSurfaceA{2, kSurfaceTokenA};
const LocalSurfaceId kSurfaceB{3, kSurfaceTokenB};
const LocalSurfaceId kSurfaceC{4, kSurfaceTokenC};

const uint64_t kBeginFrameSourceId = 1337;

gpu::SyncToken MakeVerifiedSyncToken(int id) {
  gpu::SyncToken token;
  token.Set(gpu::CommandBufferNamespace::GPU_IO,
            gpu::CommandBufferId::FromUnsafeValue(id), 1);
  token.SetVerifyFlush();
  return token;
}

// Matches a pointer to a structure with a sink_id field against a given
// FrameSinkId.
MATCHER_P(ForSink, id, "is for " + id.ToString()) {
  return arg->sink_id == id.sink_id();
}

// Matches a ReturnedResource with an ID matching a given ResourceId.
MATCHER_P(ForResource,
          id,
          "is for resource " + base::NumberToString(id.GetUnsafeValue())) {
  return arg.id == id;
}

// Holds the four interface objects needed to create a RootCompositorFrameSink.
struct TestRootFrameSink {
  TestRootFrameSink(FrameSinkManagerImpl& manager,
                    BeginFrameSource& begin_frame_source)
      : manager_(manager) {
    auto params = mojom::RootCompositorFrameSinkParams::New();
    params->frame_sink_id = kRootFrame;
    params->widget = gpu::kNullSurfaceHandle;
    params->compositor_frame_sink =
        compositor_frame_sink.BindNewEndpointAndPassReceiver();
    params->compositor_frame_sink_client =
        compositor_frame_sink_client.BindInterfaceRemote();
    params->display_private = display_private.BindNewEndpointAndPassReceiver();
    params->display_client = display_client.BindRemote();

    manager_->RegisterFrameSinkId(kRootFrame, /*report_activation=*/true);
    manager_->RegisterBeginFrameSource(&begin_frame_source, kRootFrame);
    manager_->CreateRootCompositorFrameSink(std::move(params));
  }

  ~TestRootFrameSink() { manager_->InvalidateFrameSinkId(kRootFrame); }

  const raw_ref<FrameSinkManagerImpl> manager_;
  mojo::AssociatedRemote<mojom::CompositorFrameSink> compositor_frame_sink;
  MockCompositorFrameSinkClient compositor_frame_sink_client;
  mojo::AssociatedRemote<mojom::DisplayPrivate> display_private;
  MockDisplayClient display_client;
};

// Holds objects needed to create other kinds of CompositorFrameSinks.
struct TestFrameSink {
  TestFrameSink(
      FrameSinkManagerImpl& manager,
      const FrameSinkId& id,
      const FrameSinkId& parent_id,
      const std::optional<FrameSinkBundleId>& bundle_id = std::nullopt)
      : manager_(manager), id_(id) {
    manager_->RegisterFrameSinkId(id, /*report_activation=*/true);
    if (parent_id.is_valid()) {
      manager_->RegisterFrameSinkHierarchy(parent_id, id);
    }
    manager_->CreateCompositorFrameSink(
        id, bundle_id, frame_sink.BindNewPipeAndPassReceiver(),
        client_receiver_.BindNewPipeAndPassRemote(),
        /* render_input_router_config= */ nullptr);
    manager_->GetFrameSinkForId(id)->SetNeedsBeginFrame(true);
  }

  ~TestFrameSink() {
    base::RunLoop loop;
    manager_->DestroyCompositorFrameSink(id_, loop.QuitClosure());
    loop.Run();
  }

  const raw_ref<FrameSinkManagerImpl> manager_;
  const FrameSinkId id_;
  MockCompositorFrameSinkClient mock_client_;
  mojo::Receiver<mojom::CompositorFrameSinkClient> client_receiver_{
      &mock_client_};
  mojo::Remote<mojom::CompositorFrameSink> frame_sink;
};

// Helper to receive FrameSinkBundleClient notifications and aggregate their
// contents for inspection by tests.
class TestBundleClient : public mojom::FrameSinkBundleClient {
 public:
  TestBundleClient() = default;
  ~TestBundleClient() override = default;

  void WaitForNextMessage() {
    DCHECK(!wait_loop_);
    wait_loop_.emplace();
    wait_loop_->Run();
    wait_loop_.reset();
  }

  void WaitForNextFlush(
      std::vector<mojom::BundledReturnedResourcesPtr>* acks,
      std::vector<mojom::BeginFrameInfoPtr>* begin_frames,
      std::vector<mojom::BundledReturnedResourcesPtr>* reclaimed_resources) {
    base::AutoReset acks_scope(&acks_, acks);
    base::AutoReset frames_scope(&begin_frames_, begin_frames);
    base::AutoReset resources_scope(&reclaimed_resources_, reclaimed_resources);
    WaitForNextMessage();
  }

  // mojom::FrameSinkBundleClient implementation:
  void FlushNotifications(std::vector<mojom::BundledReturnedResourcesPtr> acks,
                          std::vector<mojom::BeginFrameInfoPtr> begin_frames,
                          std::vector<mojom::BundledReturnedResourcesPtr>
                              reclaimed_resources) override {
    if (acks_) {
      *acks_ = std::move(acks);
    } else {
      EXPECT_TRUE(acks.empty()) << "Got unexpected acks";
    }

    if (begin_frames_) {
      *begin_frames_ = std::move(begin_frames);
    } else {
      EXPECT_TRUE(begin_frames.empty()) << "Got unexpected OnBeginFrames";
    }

    if (reclaimed_resources_) {
      *reclaimed_resources_ = std::move(reclaimed_resources);
    } else {
      EXPECT_TRUE(reclaimed_resources.empty())
          << "Got unexpected ReclaimResources";
    }

    NotifyOnMessage();
  }

  void OnBeginFramePausedChanged(uint32_t, bool) override {}
  void OnCompositorFrameTransitionDirectiveProcessed(
      uint32_t sink_id,
      uint32_t sequence_id) override {}

 private:
  void NotifyOnMessage() {
    if (wait_loop_) {
      wait_loop_->Quit();
    }
  }

  std::optional<base::RunLoop> wait_loop_;
  raw_ptr<std::vector<mojom::BundledReturnedResourcesPtr>> acks_;
  raw_ptr<std::vector<mojom::BeginFrameInfoPtr>> begin_frames_;
  raw_ptr<std::vector<mojom::BundledReturnedResourcesPtr>> reclaimed_resources_;
};

class FrameSinkBundleImplTest : public testing::Test {
 public:
  FrameSinkBundleImplTest() {
    manager_.surface_manager()->SetTickClockForTesting(&test_clock_);
    manager_.CreateFrameSinkBundle(kBundleId,
                                   bundle_.BindNewPipeAndPassReceiver(),
                                   client_receiver_.BindNewPipeAndPassRemote());
  }

  ~FrameSinkBundleImplTest() override {
    manager_.UnregisterBeginFrameSource(&begin_frame_source_);
  }

  void IssueOnBeginFrame() {
    begin_frame_source_.TestOnBeginFrame(
        begin_frame_source_.CreateBeginFrameArgs(BEGINFRAME_FROM_HERE));
  }

  mojom::BundledFrameSubmissionPtr CreateFrameSubmission(
      const FrameSinkId& frame_sink_id,
      const LocalSurfaceId& surface_id,
      std::vector<ResourceId> resource_ids = {}) {
    auto frame = MakeDefaultCompositorFrame(kBeginFrameSourceId);
    for (const auto& id : resource_ids) {
      TransferableResource resource;
      resource.id = id;
      resource.set_texture_target(GL_TEXTURE_2D);
      resource.set_sync_token(frame_sync_token_);
      frame.resource_list.push_back(resource);
    }

    auto data = mojom::BundledCompositorFrame::New(surface_id, std::move(frame),
                                                   std::nullopt, 0);
    return mojom::BundledFrameSubmission::New(
        frame_sink_id.sink_id(),
        mojom::BundledFrameSubmissionData::NewFrame(std::move(data)));
  }

  mojom::BundledFrameSubmissionPtr CreateDidNotSubmitFrame(
      const FrameSinkId& frame_sink_id) {
    return mojom::BundledFrameSubmission::New(
        frame_sink_id.sink_id(),
        mojom::BundledFrameSubmissionData::NewDidNotProduceFrame(
            BeginFrameAck::CreateManualAckWithDamage()));
  }

  FrameSinkManagerImpl& manager() { return manager_; }
  TestBundleClient& test_client() { return test_client_; }
  mojo::Remote<mojom::FrameSinkBundle>& bundle() { return bundle_; }
  FakeExternalBeginFrameSource& begin_frame_source() {
    return begin_frame_source_;
  }

 private:
  const gpu::SyncToken frame_sync_token_{MakeVerifiedSyncToken(42)};

  base::SimpleTestTickClock test_clock_;
  DebugRendererSettings debug_settings_;
  ServerSharedBitmapManager shared_bitmap_manager_;
  TestOutputSurfaceProvider output_surface_provider_;
  FrameSinkManagerImpl manager_{
      FrameSinkManagerImpl::InitParams(&shared_bitmap_manager_,
                                       &output_surface_provider_)};
  FakeExternalBeginFrameSource begin_frame_source_{0.0f, false};

  TestBundleClient test_client_;
  mojo::Receiver<mojom::FrameSinkBundleClient> client_receiver_{&test_client_};
  mojo::Remote<mojom::FrameSinkBundle> bundle_;

  TestRootFrameSink root_sink_{manager_, begin_frame_source_};
  TestFrameSink main_frame_{manager_, kMainFrame, kRootFrame};
};

TEST_F(FrameSinkBundleImplTest, DestroyOnDisconnect) {
  EXPECT_NE(nullptr, manager().GetFrameSinkBundle(kBundleId));

  bundle().reset();
  base::RunLoop loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
                                                           loop.QuitClosure());
  loop.Run();

  EXPECT_EQ(nullptr, manager().GetFrameSinkBundle(kBundleId));
}

TEST_F(FrameSinkBundleImplTest, OnBeginFrame) {
  // By default the bundle does not observe the BeginFrameSource. The only
  // observer is the (non-bundled) main-frame sink.
  EXPECT_EQ(1u, begin_frame_source().num_observers());

  TestFrameSink frame_a(manager(), kSubFrameA, kMainFrame, kBundleId);
  TestFrameSink frame_b(manager(), kSubFrameB, kMainFrame, kBundleId);
  TestFrameSink frame_c(manager(), kSubFrameC, kMainFrame, kBundleId);

  // The bundle should observe the BeginFrameSource on behalf of all its sinks,
  // so the only observers should now be the main-frame sink and the bundle.
  EXPECT_EQ(2u, begin_frame_source().num_observers());

  // OnBeginFrame() should elicit a single batch of notifications to the bundle
  // client, with a notification for each frame in the bundle.
  std::vector<mojom::BeginFrameInfoPtr> begin_frames;
  IssueOnBeginFrame();
  test_client().WaitForNextFlush(nullptr, &begin_frames, nullptr);
  EXPECT_THAT(begin_frames,
              UnorderedElementsAre(ForSink(kSubFrameA), ForSink(kSubFrameB),
                                   ForSink(kSubFrameC)));

  // Also verify that if a sink does not want OnBeginFrame, we don't include one
  // for it in subsequent batches.
  manager().GetFrameSinkForId(kSubFrameB)->SetNeedsBeginFrame(false);
  IssueOnBeginFrame();
  test_client().WaitForNextFlush(nullptr, &begin_frames, nullptr);
  EXPECT_THAT(begin_frames,
              UnorderedElementsAre(ForSink(kSubFrameA), ForSink(kSubFrameC)));

  // Finally, if all sinks unsubscribe from BeginFrame notifications, the bundle
  // should stop observing the BeginFrameSource.
  EXPECT_EQ(2u, begin_frame_source().num_observers());
  manager().GetFrameSinkForId(kSubFrameA)->SetNeedsBeginFrame(false);
  EXPECT_EQ(2u, begin_frame_source().num_observers());
  manager().GetFrameSinkForId(kSubFrameC)->SetNeedsBeginFrame(false);
  EXPECT_EQ(1u, begin_frame_source().num_observers());
}

class OnBeginFrameAcksFrameSinkBundleImplTest
    : public FrameSinkBundleImplTest,
      public testing::WithParamInterface<bool> {
 public:
  OnBeginFrameAcksFrameSinkBundleImplTest();
  ~OnBeginFrameAcksFrameSinkBundleImplTest() override = default;

  // This will IssueOnBeginFrame if BeginFrameAcksEnabled is true. Since we no
  // longer send the ack separately, we need the OnBeginFrame both to be the
  // ack, and so there are messages to flush.
  void MaybeIssueOnBeginFrame();

  bool BeginFrameAcksEnabled() const { return GetParam(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

OnBeginFrameAcksFrameSinkBundleImplTest::
    OnBeginFrameAcksFrameSinkBundleImplTest() {
  if (BeginFrameAcksEnabled()) {
    scoped_feature_list_.InitAndEnableFeature(features::kOnBeginFrameAcks);
  } else {
    scoped_feature_list_.InitAndDisableFeature(features::kOnBeginFrameAcks);
  }
}

void OnBeginFrameAcksFrameSinkBundleImplTest::MaybeIssueOnBeginFrame() {
  if (!BeginFrameAcksEnabled()) {
    return;
  }
  IssueOnBeginFrame();
}

TEST_P(OnBeginFrameAcksFrameSinkBundleImplTest, SubmitAndAck) {
  TestFrameSink frame_a(manager(), kSubFrameA, kMainFrame, kBundleId);
  TestFrameSink frame_b(manager(), kSubFrameB, kMainFrame, kBundleId);
  TestFrameSink frame_c(manager(), kSubFrameC, kMainFrame, kBundleId);

  // Verify that submitting a batch of frames results in the client eventually
  // receiving a corresponding batch of acks.
  std::vector<mojom::BundledFrameSubmissionPtr> submissions;
  submissions.push_back(CreateFrameSubmission(kSubFrameA, kSurfaceA));
  submissions.push_back(CreateFrameSubmission(kSubFrameB, kSurfaceB));
  submissions.push_back(CreateFrameSubmission(kSubFrameC, kSurfaceC));
  bundle()->Submit(std::move(submissions));

  std::vector<mojom::BundledReturnedResourcesPtr> acks;
  std::vector<mojom::BeginFrameInfoPtr> begin_frames;
  MaybeIssueOnBeginFrame();
  test_client().WaitForNextFlush(&acks, &begin_frames, nullptr);
  if (BeginFrameAcksEnabled()) {
    EXPECT_TRUE(acks.empty());
    EXPECT_THAT(begin_frames,
                UnorderedElementsAre(ForSink(kSubFrameA), ForSink(kSubFrameB),
                                     ForSink(kSubFrameC)));
  } else {
    EXPECT_THAT(acks, ElementsAre(ForSink(kSubFrameA), ForSink(kSubFrameB),
                                  ForSink(kSubFrameC)));
    EXPECT_TRUE(begin_frames.empty());
  }
}

TEST_P(OnBeginFrameAcksFrameSinkBundleImplTest, NoAckIfDidNotProduceFrame) {
  TestFrameSink frame_a(manager(), kSubFrameA, kMainFrame, kBundleId);
  TestFrameSink frame_b(manager(), kSubFrameB, kMainFrame, kBundleId);
  TestFrameSink frame_c(manager(), kSubFrameC, kMainFrame, kBundleId);

  // Verify batched DidNotProduceFrame requests do not elicit a batched ack.
  std::vector<mojom::BundledFrameSubmissionPtr> submissions;
  submissions.push_back(CreateFrameSubmission(kSubFrameA, kSurfaceA));
  submissions.push_back(CreateDidNotSubmitFrame(kSubFrameB));
  submissions.push_back(CreateFrameSubmission(kSubFrameC, kSurfaceC));
  bundle()->Submit(std::move(submissions));

  std::vector<mojom::BundledReturnedResourcesPtr> acks;
  std::vector<mojom::BeginFrameInfoPtr> begin_frames;
  MaybeIssueOnBeginFrame();
  test_client().WaitForNextFlush(&acks, &begin_frames, nullptr);
  if (BeginFrameAcksEnabled()) {
    EXPECT_TRUE(acks.empty());
    EXPECT_THAT(begin_frames,
                UnorderedElementsAre(ForSink(kSubFrameA), ForSink(kSubFrameB),
                                     ForSink(kSubFrameC)));
  } else {
    EXPECT_THAT(acks, ElementsAre(ForSink(kSubFrameA), ForSink(kSubFrameC)));
    EXPECT_TRUE(begin_frames.empty());
  }
}

TEST_P(OnBeginFrameAcksFrameSinkBundleImplTest, ReclaimResourcesOnAck) {
  TestFrameSink frame_a(manager(), kSubFrameA, kMainFrame, kBundleId);
  TestFrameSink frame_b(manager(), kSubFrameB, kMainFrame, kBundleId);
  TestFrameSink frame_c(manager(), kSubFrameC, kMainFrame, kBundleId);

  if (BeginFrameAcksEnabled()) {
    frame_a.frame_sink->SetWantsBeginFrameAcks();
    frame_b.frame_sink->SetWantsBeginFrameAcks();
    frame_c.frame_sink->SetWantsBeginFrameAcks();
  }

  // First submit frames through all the sinks, to each surface.
  std::vector<mojom::BundledFrameSubmissionPtr> submissions;
  submissions.push_back(CreateFrameSubmission(kSubFrameA, kSurfaceA));
  submissions.push_back(CreateFrameSubmission(kSubFrameB, kSurfaceB));
  submissions.push_back(CreateFrameSubmission(kSubFrameC, kSurfaceC));
  bundle()->Submit(std::move(submissions));

  std::vector<mojom::BundledReturnedResourcesPtr> acks;
  std::vector<mojom::BeginFrameInfoPtr> begin_frames;
  MaybeIssueOnBeginFrame();
  test_client().WaitForNextFlush(&acks, &begin_frames, nullptr);
  if (BeginFrameAcksEnabled()) {
    EXPECT_TRUE(acks.empty());
    EXPECT_THAT(begin_frames,
                UnorderedElementsAre(ForSink(kSubFrameA), ForSink(kSubFrameB),
                                     ForSink(kSubFrameC)));
  } else {
    EXPECT_THAT(acks, ElementsAre(ForSink(kSubFrameA), ForSink(kSubFrameB),
                                  ForSink(kSubFrameC)));
    EXPECT_TRUE(begin_frames.empty());
  }

  // Now frame C will submit with resources to a dead surface and be rejected
  // immediately. This should result in an ack which immediately returns the
  // attached resource.
  ResourceId resource(1337);
  manager().GetFrameSinkForId(kSubFrameC)->EvictSurface(kSurfaceC);
  submissions.clear();
  submissions.push_back(
      CreateFrameSubmission(kSubFrameC, kSurfaceC, {resource}));
  bundle()->Submit(std::move(submissions));

  acks.clear();
  begin_frames.clear();
  MaybeIssueOnBeginFrame();
  test_client().WaitForNextFlush(&acks, &begin_frames, nullptr);
  if (BeginFrameAcksEnabled()) {
    // Without the OnBeginFrame there is no message waiting to flush. While
    // `bundle()->Submit` executes during this RunLoop, the resources won't have
    // been acked by the time we issue the OnBeginFrame.
    EXPECT_THAT(begin_frames,
                UnorderedElementsAre(ForSink(kSubFrameA), ForSink(kSubFrameB),
                                     ForSink(kSubFrameC)));

    // Resources are returned as a part of future OnBeginFrames, after the
    // frame sink has internally Acked the frame. The `Submit` above will have
    // enqueued the resources to return.
    begin_frames.clear();
    IssueOnBeginFrame();
    test_client().WaitForNextFlush(nullptr, &begin_frames, nullptr);
    EXPECT_THAT(begin_frames,
                UnorderedElementsAre(ForSink(kSubFrameA), ForSink(kSubFrameB),
                                     ForSink(kSubFrameC)));
    EXPECT_EQ(kSubFrameC.sink_id(), begin_frames[2]->sink_id);
    EXPECT_THAT(begin_frames[2]->resources, ElementsAre(ForResource(resource)));
  } else {
    EXPECT_THAT(acks, ElementsAre(ForSink(kSubFrameC)));

    EXPECT_EQ(kSubFrameC.sink_id(), acks[0]->sink_id);
    EXPECT_THAT(acks[0]->resources, ElementsAre(ForResource(resource)));
  }
}

INSTANTIATE_TEST_SUITE_P(,
                         OnBeginFrameAcksFrameSinkBundleImplTest,
                         testing::Bool(),
                         [](auto& info) {
                           return info.param ? "BeginFrameAcks"
                                             : "CompositoFrameAcks";
                         });
}  // namespace
}  // namespace viz
