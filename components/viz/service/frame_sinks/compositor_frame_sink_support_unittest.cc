// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/viz/service/frame_sinks/compositor_frame_sink_support.h"

#include <string>
#include <tuple>
#include <utility>

#include "base/functional/bind.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/with_feature_override.h"
#include "base/time/time.h"
#include "base/token.h"
#include "build/build_config.h"
#include "components/viz/common/features.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/quads/compositor_frame_transition_directive.h"
#include "components/viz/common/quads/shared_element_draw_quad.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/common/surfaces/subtree_capture_id.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "components/viz/common/surfaces/surface_info.h"
#include "components/viz/service/display_embedder/server_shared_bitmap_manager.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "components/viz/service/surfaces/surface.h"
#include "components/viz/test/begin_frame_args_test.h"
#include "components/viz/test/compositor_frame_helpers.h"
#include "components/viz/test/fake_compositor_frame_sink_client.h"
#include "components/viz/test/fake_external_begin_frame_source.h"
#include "components/viz/test/fake_surface_observer.h"
#include "components/viz/test/mock_compositor_frame_sink_client.h"
#include "components/viz/test/test_context_provider.h"
#include "components/viz/test/test_shared_image_interface_provider.h"
#include "components/viz/test/viz_test_suite.h"
#include "services/viz/privileged/mojom/compositing/frame_sink_manager.mojom.h"
#include "services/viz/public/mojom/compositing/compositor_frame_sink.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/khronos/GLES2/gl2.h"

using testing::_;
using testing::Contains;
using testing::Eq;
using testing::Invoke;
using testing::IsEmpty;
using testing::Key;
using testing::Ne;
using testing::SizeIs;
using testing::UnorderedElementsAre;

namespace viz {
namespace {

constexpr bool kIsRoot = false;

constexpr FrameSinkId kArbitraryFrameSinkId(1, 1);
constexpr FrameSinkId kAnotherArbitraryFrameSinkId(2, 2);

constexpr gfx::Size kDefaultSize(20, 20);
constexpr gfx::Rect kDefaultOutputRect(kDefaultSize);

const base::UnguessableToken kArbitraryToken =
    base::UnguessableToken::CreateForTesting(1, 2);
const base::UnguessableToken kAnotherArbitraryToken =
    base::UnguessableToken::CreateForTesting(2, 2);

const uint64_t kBeginFrameSourceId = 1337;

// Matches a SurfaceInfo for |surface_id|.
MATCHER_P(SurfaceInfoWithId, surface_id, "") {
  return arg.id() == surface_id;
}

void StubResultCallback(std::unique_ptr<CopyOutputResult> result) {}

gpu::SyncToken GenTestSyncToken(int id) {
  gpu::SyncToken token;
  token.Set(gpu::CommandBufferNamespace::GPU_IO,
            gpu::CommandBufferId::FromUnsafeValue(id), 1);
  return token;
}

bool BeginFrameArgsAreEquivalent(const BeginFrameArgs& first,
                                 const BeginFrameArgs& second) {
  return first.frame_id == second.frame_id;
}

std::string PostTestCaseNameTuple(
    const ::testing::TestParamInfo<std::tuple<bool, bool>>& info) {
  return base::StringPrintf(
      "%s_%s",
      std::get<0>(info.param) ? "BeginFrameAcks" : "CompositorFrameAcks",
      std::get<1>(info.param) ? "AckOnSurfaceActivationWhenInteractive"
                              : "DoNotAckOnSurfaceActivationWhenInteractive");
}

std::string PostTestCaseNameBool(const ::testing::TestParamInfo<bool>& info) {
  return info.param ? "AckOnSurfaceActivationWhenInteractive"
                    : "DoNotAckOnSurfaceActivationWhenInteractive";
}

}  // namespace

class MockFrameSinkManagerClient : public mojom::FrameSinkManagerClient {
 public:
  MockFrameSinkManagerClient() = default;

  MockFrameSinkManagerClient(const MockFrameSinkManagerClient&) = delete;
  MockFrameSinkManagerClient& operator=(const MockFrameSinkManagerClient&) =
      delete;

  ~MockFrameSinkManagerClient() override = default;

  // mojom::FrameSinkManagerClient:
  MOCK_METHOD1(OnFirstSurfaceActivation, void(const SurfaceInfo&));
  MOCK_METHOD3(OnFrameTokenChanged,
               void(const FrameSinkId&, uint32_t, base::TimeTicks));
  void OnAggregatedHitTestRegionListUpdated(
      const FrameSinkId& frame_sink_id,
      const std::vector<AggregatedHitTestRegion>& hit_test_data) override {}
#if BUILDFLAG(IS_ANDROID)
  void VerifyThreadIdsDoNotBelongToHost(
      const std::vector<int32_t>& thread_ids,
      VerifyThreadIdsDoNotBelongToHostCallback callback) override {}
#endif
  void OnScreenshotCaptured(
      const blink::SameDocNavigationScreenshotDestinationToken&
          destination_token,
      std::unique_ptr<CopyOutputResult> copy_output_result) override {}
};

class CompositorFrameSinkSupportTestBase : public testing::Test {
 public:
  CompositorFrameSinkSupportTestBase()
      : begin_frame_source_(0.f, false),
        local_surface_id_(3, kArbitraryToken),
        frame_sync_token_(GenTestSyncToken(4)),
        consumer_sync_token_(GenTestSyncToken(5)) {}
  ~CompositorFrameSinkSupportTestBase() override = default;

  // testing::Test
  void SetUp() override {
    manager_ = std::make_unique<FrameSinkManagerImpl>(
        FrameSinkManagerImpl::InitParams(&shared_bitmap_manager_));
    surface_observer_ =
        std::make_unique<FakeSurfaceObserver>(manager_->surface_manager());
    manager_->SetLocalClient(&frame_sink_manager_client_);
    now_src_ = std::make_unique<base::SimpleTestTickClock>();
    manager_->surface_manager()->SetTickClockForTesting(now_src_.get());
    manager_->RegisterFrameSinkId(kArbitraryFrameSinkId,
                                  true /* report_activation */);
    manager_->SetSharedImageInterfaceProviderForTest(
        &shared_image_interface_provider_);
    support_ = std::make_unique<CompositorFrameSinkSupport>(
        &fake_support_client_, manager_.get(), kArbitraryFrameSinkId, kIsRoot);
    support_->SetBeginFrameSource(&begin_frame_source_);
  }

  void TearDown() override {
    manager_->InvalidateFrameSinkId(kArbitraryFrameSinkId);
  }

  void AddResourcesToFrame(CompositorFrame* frame,
                           ResourceId* resource_ids,
                           size_t num_resource_ids) {
    for (size_t i = 0u; i < num_resource_ids; ++i) {
      TransferableResource resource;
      resource.id = resource_ids[i];
      resource.set_texture_target(GL_TEXTURE_2D);
      resource.set_sync_token(frame_sync_token_);
      frame->resource_list.push_back(resource);
    }
  }

  void SubmitCompositorFrameWithResources(ResourceId* resource_ids,
                                          size_t num_resource_ids) {
    auto frame = MakeDefaultInteractiveCompositorFrame(kBeginFrameSourceId);
    AddResourcesToFrame(&frame, resource_ids, num_resource_ids);
    support_->SubmitCompositorFrame(local_surface_id_, std::move(frame));
    EXPECT_EQ(surface_observer_->last_created_surface_id().local_surface_id(),
              local_surface_id_);
  }

  bool SubmitCompositorFrameWithCopyRequest(
      CompositorFrame frame,
      std::unique_ptr<CopyOutputRequest> request) {
    frame.render_pass_list.back()->copy_requests.push_back(std::move(request));
    const auto result = support_->MaybeSubmitCompositorFrame(
        local_surface_id_, std::move(frame), std::nullopt, 0,
        mojom::CompositorFrameSink::SubmitCompositorFrameSyncCallback());
    switch (result) {
      case SubmitResult::ACCEPTED:
        return true;
      case SubmitResult::COPY_OUTPUT_REQUESTS_NOT_ALLOWED:
        return false;
      default:
        ADD_FAILURE()
            << "Test broken; fail result not related to copy requests: "
            << CompositorFrameSinkSupport::GetSubmitResultAsString(result);
        break;
    }
    return false;
  }

  void UnrefResources(ResourceId* ids_to_unref,
                      int* counts_to_unref,
                      size_t num_ids_to_unref) {
    std::vector<ReturnedResource> unref_array;
    for (size_t i = 0; i < num_ids_to_unref; ++i) {
      ReturnedResource resource;
      resource.sync_token = consumer_sync_token_;
      resource.id = ids_to_unref[i];
      resource.count = counts_to_unref[i];
      unref_array.push_back(std::move(resource));
    }
    support_->UnrefResources(std::move(unref_array));
  }

  void CheckReturnedResourcesMatchExpected(ResourceId* expected_returned_ids,
                                           size_t expected_resources) {
    const std::vector<ReturnedResource>& actual_resources =
        fake_support_client_.returned_resources();
    ASSERT_EQ(expected_resources, actual_resources.size());
    for (size_t i = 0; i < expected_resources; ++i) {
      EXPECT_EQ(expected_returned_ids[i], actual_resources[i].id);
    }
    fake_support_client_.clear_returned_resources();
  }

  void CheckReturnedResourcesMatchExpected(ResourceId* expected_returned_ids,
                                           int* expected_returned_counts,
                                           size_t expected_resources,
                                           gpu::SyncToken expected_sync_token) {
    const std::vector<ReturnedResource>& actual_resources =
        fake_support_client_.returned_resources();
    ASSERT_EQ(expected_resources, actual_resources.size());
    for (size_t i = 0; i < expected_resources; ++i) {
      const auto& resource = actual_resources[i];
      EXPECT_EQ(expected_sync_token, resource.sync_token);
      EXPECT_EQ(expected_returned_ids[i], resource.id);
      EXPECT_EQ(expected_returned_counts[i], resource.count);
    }
    fake_support_client_.clear_returned_resources();
  }

  Surface* GetSurfaceForId(const SurfaceId& id) {
    return manager_->surface_manager()->GetSurfaceForId(id);
  }

  bool HasTemporaryReference(const SurfaceId& id) {
    return manager_->surface_manager()->HasTemporaryReference(id);
  }

  void RefCurrentFrameResources() {
    Surface* surface = GetSurfaceForId(
        SurfaceId(support_->frame_sink_id(), local_surface_id_));
    support_->RefResources(surface->GetActiveFrame().resource_list);
  }

  void ExpireAllTemporaryReferences() {
    // First call marks temporary references as old.
    manager_->surface_manager()->ExpireOldTemporaryReferences();
    // Second call removes the temporary references marked as old.
    manager_->surface_manager()->ExpireOldTemporaryReferences();
  }

  const BeginFrameArgs& GetLastUsedBeginFrameArgs(
      const CompositorFrameSinkSupport* support) const {
    return support->LastUsedBeginFrameArgs();
  }

  void SendPresentationFeedback(CompositorFrameSinkSupport* support,
                                uint32_t frame_token) {
    base::TimeTicks draw_time = base::TimeTicks::Now();

    base::TimeTicks swap_time = base::TimeTicks::Now();
    gfx::SwapTimings timings = {swap_time, swap_time};

    support->DidPresentCompositorFrame(
        frame_token, draw_time, timings,
        gfx::PresentationFeedback(base::TimeTicks::Now(),
                                  base::Milliseconds(16),
                                  /*flags=*/0));
  }

  bool HasAnimationManagerForToken(blink::ViewTransitionToken token) const {
    return manager_->transition_token_to_animation_manager_.contains(token);
  }

  void ProcessCompositorFrameTransitionDirective(
      CompositorFrameSinkSupport* support,
      const CompositorFrameTransitionDirective& directive,
      Surface* surface) {
    support->ProcessCompositorFrameTransitionDirective(directive, surface);
  }

  bool SupportHasSurfaceAnimationManager(
      CompositorFrameSinkSupport* support) const {
    return !support->view_transition_token_to_animation_manager_.empty();
  }

 protected:
  TestSharedImageInterfaceProvider shared_image_interface_provider_;
  std::unique_ptr<base::SimpleTestTickClock> now_src_;
  ServerSharedBitmapManager shared_bitmap_manager_;
  std::unique_ptr<FrameSinkManagerImpl> manager_;
  testing::NiceMock<MockFrameSinkManagerClient> frame_sink_manager_client_;
  FakeCompositorFrameSinkClient fake_support_client_;
  FakeExternalBeginFrameSource begin_frame_source_;
  std::unique_ptr<CompositorFrameSinkSupport> support_;
  LocalSurfaceId local_surface_id_;
  std::unique_ptr<FakeSurfaceObserver> surface_observer_;

  // This is the sync token submitted with the frame. It should never be
  // returned to the client.
  const gpu::SyncToken frame_sync_token_;

  // This is the sync token returned by the consumer. It should always be
  // returned to the client.
  const gpu::SyncToken consumer_sync_token_;
};

class CompositorFrameSinkSupportTest
    : public testing::WithParamInterface<bool>,
      public CompositorFrameSinkSupportTestBase {
 public:
  CompositorFrameSinkSupportTest();
  ~CompositorFrameSinkSupportTest() override = default;

  bool ShouldAckOnSurfaceActivationWhenInteractive() const {
    return GetParam();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

CompositorFrameSinkSupportTest::CompositorFrameSinkSupportTest() {
  std::vector<base::test::FeatureRef> enabled_features;
  std::vector<base::test::FeatureRef> disabled_features;
  if (ShouldAckOnSurfaceActivationWhenInteractive()) {
    enabled_features.push_back(
        features::kAckOnSurfaceActivationWhenInteractive);
  } else {
    disabled_features.push_back(
        features::kAckOnSurfaceActivationWhenInteractive);
  }
  scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
}

// Supports testing features::OnBeginFrameAcks, which changes the expectations
// of what IPCs are sent to the CompositorFrameSinkClient. When enabled
// OnBeginFrame also handles ReturnResources as well as
// DidReceiveCompositorFrameAck.
class OnBeginFrameAcksCompositorFrameSinkSupportTest
    : public CompositorFrameSinkSupportTestBase,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  OnBeginFrameAcksCompositorFrameSinkSupportTest();
  ~OnBeginFrameAcksCompositorFrameSinkSupportTest() override = default;

  // testing::Test
  void SetUp() override {
    CompositorFrameSinkSupportTestBase::SetUp();
    if (BeginFrameAcksEnabled()) {
      support_->SetWantsBeginFrameAcks();
    }
  }

  // When features::OnBeginFrameAcks is enabled resources are only returned
  // after a frame has been Acked, and during the next OnBeginFrame. When this
  // is off the resources are returned immediately.
  //
  // These methods will submit the according Ack/BeginFrames when
  // features::OnBeginFrameAcks is enabled, to ensure the resource return path
  // is triggered.
  void MaybeSendCompositorFrameAck();
  void MaybeTestOnBeginFrame(uint64_t sequence_number);

  bool BeginFrameAcksEnabled() const { return std::get<0>(GetParam()); }
  bool ShouldAckOnSurfaceActivationWhenInteractive() const {
    return std::get<1>(GetParam());
  }

  int num_pending_frames(const CompositorFrameSinkSupport* support) const {
    return support->pending_frames_.size();
  }

  bool client_needs_begin_frame(
      const CompositorFrameSinkSupport* support) const {
    return support->client_needs_begin_frame_;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

OnBeginFrameAcksCompositorFrameSinkSupportTest::
    OnBeginFrameAcksCompositorFrameSinkSupportTest() {
  std::vector<base::test::FeatureRef> enabled_features;
  std::vector<base::test::FeatureRef> disabled_features;

  if (BeginFrameAcksEnabled()) {
    enabled_features.push_back(features::kOnBeginFrameAcks);
  } else {
    disabled_features.push_back(features::kOnBeginFrameAcks);
  }

  if (ShouldAckOnSurfaceActivationWhenInteractive()) {
    enabled_features.push_back(
        features::kAckOnSurfaceActivationWhenInteractive);
  } else {
    disabled_features.push_back(
        features::kAckOnSurfaceActivationWhenInteractive);
  }

  scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
}

void OnBeginFrameAcksCompositorFrameSinkSupportTest::
    MaybeSendCompositorFrameAck() {
  if (!BeginFrameAcksEnabled() ||
      ShouldAckOnSurfaceActivationWhenInteractive()) {
    return;
  }
  support_->SendCompositorFrameAck();
}

void OnBeginFrameAcksCompositorFrameSinkSupportTest::MaybeTestOnBeginFrame(
    uint64_t sequence_number) {
  if (!BeginFrameAcksEnabled()) {
    return;
  }
  BeginFrameArgs args =
      CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, sequence_number);
  begin_frame_source_.TestOnBeginFrame(args);
}

// Tests submitting a frame with resources followed by one with no resources
// with no resource provider action in between.
TEST_P(OnBeginFrameAcksCompositorFrameSinkSupportTest, ResourceLifetimeSimple) {
  ResourceId first_frame_ids[] = {ResourceId(1), ResourceId(2), ResourceId(3)};
  SubmitCompositorFrameWithResources(first_frame_ids,
                                     std::size(first_frame_ids));
  MaybeSendCompositorFrameAck();

  // All of the resources submitted in the first frame are still in use at this
  // time by virtue of being in the pending frame, so none can be returned to
  // the client yet.
  EXPECT_EQ(0u, fake_support_client_.returned_resources().size());
  fake_support_client_.clear_returned_resources();

  // The second frame references no resources of first frame and thus should
  // make all resources of first frame available to be returned.
  SubmitCompositorFrameWithResources(nullptr, 0);

  MaybeTestOnBeginFrame(1);
  ResourceId expected_returned_ids[] = {ResourceId(1), ResourceId(2),
                                        ResourceId(3)};
  int expected_returned_counts[] = {1, 1, 1};
  // Resources were never consumed so no sync token should be set.
  CheckReturnedResourcesMatchExpected(
      expected_returned_ids, expected_returned_counts,
      std::size(expected_returned_counts), gpu::SyncToken());

  ResourceId third_frame_ids[] = {ResourceId(4), ResourceId(5), ResourceId(6)};
  SubmitCompositorFrameWithResources(third_frame_ids,
                                     std::size(third_frame_ids));

  // All of the resources submitted in the third frame are still in use at this
  // time by virtue of being in the pending frame, so none can be returned to
  // the client yet.
  EXPECT_EQ(0u, fake_support_client_.returned_resources().size());
  fake_support_client_.clear_returned_resources();

  // The forth frame references no resources of third frame and thus should
  // make all resources of third frame available to be returned.
  ResourceId forth_frame_ids[] = {ResourceId(7), ResourceId(8), ResourceId(9)};
  SubmitCompositorFrameWithResources(forth_frame_ids,
                                     std::size(forth_frame_ids));

  MaybeTestOnBeginFrame(2);
  ResourceId forth_expected_returned_ids[] = {ResourceId(4), ResourceId(5),
                                              ResourceId(6)};
  int forth_expected_returned_counts[] = {1, 1, 1};
  // Resources were never consumed so no sync token should be set.
  CheckReturnedResourcesMatchExpected(
      forth_expected_returned_ids, forth_expected_returned_counts,
      std::size(forth_expected_returned_counts), gpu::SyncToken());
}

// Tests submitting a frame with resources followed by one with no resources
// with the resource provider holding everything alive.
TEST_P(OnBeginFrameAcksCompositorFrameSinkSupportTest,
       ResourceLifetimeSimpleWithProviderHoldingAlive) {
  ResourceId first_frame_ids[] = {ResourceId(1), ResourceId(2), ResourceId(3)};
  SubmitCompositorFrameWithResources(first_frame_ids,
                                     std::size(first_frame_ids));
  MaybeSendCompositorFrameAck();

  // All of the resources submitted in the first frame are still in use at this
  // time by virtue of being in the pending frame, so none can be returned to
  // the client yet.
  EXPECT_EQ(0u, fake_support_client_.returned_resources().size());
  fake_support_client_.clear_returned_resources();

  // Hold on to everything.
  RefCurrentFrameResources();

  // The second frame references no resources and thus should make all resources
  // available to be returned as soon as the resource provider releases them.
  SubmitCompositorFrameWithResources(nullptr, 0);

  EXPECT_EQ(0u, fake_support_client_.returned_resources().size());
  fake_support_client_.clear_returned_resources();

  int release_counts[] = {1, 1, 1};
  UnrefResources(first_frame_ids, release_counts, std::size(first_frame_ids));

  // None are returned to the client since DidReceiveCompositorAck is not
  // invoked.
  //
  // If it is, and we're doing begin frame acks, although we will get an ack,
  // we will also not have returned resources. They will be instead be
  // enqueued in `surface_returned_resources_` since we need a begin frame.
  EXPECT_EQ(0u, fake_support_client_.returned_resources().size());

  // Submitting an empty frame causes previous resources referenced by the
  // previous frame to be returned to client.
  SubmitCompositorFrameWithResources(nullptr, 0);
  MaybeTestOnBeginFrame(1);
  ResourceId expected_returned_ids[] = {ResourceId(1), ResourceId(2),
                                        ResourceId(3)};
  int expected_returned_counts[] = {1, 1, 1};
  CheckReturnedResourcesMatchExpected(
      expected_returned_ids, expected_returned_counts,
      std::size(expected_returned_counts), consumer_sync_token_);
}

// Tests referencing a resource, unref'ing it to zero, then using it again
// before returning it to the client.
TEST_P(OnBeginFrameAcksCompositorFrameSinkSupportTest,
       ResourceReusedBeforeReturn) {
  ResourceId first_frame_ids[] = {ResourceId(7)};
  SubmitCompositorFrameWithResources(first_frame_ids,
                                     std::size(first_frame_ids));
  MaybeSendCompositorFrameAck();

  // This removes all references to resource id 7.
  SubmitCompositorFrameWithResources(nullptr, 0);

  // This references id 7 again.
  SubmitCompositorFrameWithResources(first_frame_ids,
                                     std::size(first_frame_ids));

  // This removes it again.
  SubmitCompositorFrameWithResources(nullptr, 0);

  MaybeTestOnBeginFrame(2);
  // Now it should be returned.
  // We don't care how many entries are in the returned array for 7, so long as
  // the total returned count matches the submitted count.
  const std::vector<ReturnedResource>& returned =
      fake_support_client_.returned_resources();
  size_t return_count = 0;
  for (size_t i = 0; i < returned.size(); ++i) {
    EXPECT_EQ(ResourceId(7u), returned[i].id);
    return_count += returned[i].count;
  }
  EXPECT_EQ(2u, return_count);
}

// Tests having resources referenced multiple times, as if referenced by
// multiple providers.
TEST_P(OnBeginFrameAcksCompositorFrameSinkSupportTest,
       ResourceRefMultipleTimes) {
  ResourceId first_frame_ids[] = {ResourceId(3), ResourceId(4)};
  SubmitCompositorFrameWithResources(first_frame_ids,
                                     std::size(first_frame_ids));
  MaybeSendCompositorFrameAck();

  // Ref resources from the first frame twice.
  RefCurrentFrameResources();
  RefCurrentFrameResources();

  ResourceId second_frame_ids[] = {ResourceId(4), ResourceId(5)};
  SubmitCompositorFrameWithResources(second_frame_ids,
                                     std::size(second_frame_ids));

  // Ref resources from the second frame 3 times.
  RefCurrentFrameResources();
  RefCurrentFrameResources();
  RefCurrentFrameResources();

  // Submit a frame with no resources to remove all current frame refs from
  // submitted resources.
  SubmitCompositorFrameWithResources(nullptr, 0);

  EXPECT_EQ(0u, fake_support_client_.returned_resources().size());
  fake_support_client_.clear_returned_resources();

  // Expected current refs:
  //  3 -> 2
  //  4 -> 2 + 3 = 5
  //  5 -> 3
  {
    SCOPED_TRACE("unref all 3");
    ResourceId ids_to_unref[] = {ResourceId(3), ResourceId(4), ResourceId(5)};
    int counts[] = {1, 1, 1};
    UnrefResources(ids_to_unref, counts, std::size(ids_to_unref));

    EXPECT_EQ(0u, fake_support_client_.returned_resources().size());
    fake_support_client_.clear_returned_resources();

    UnrefResources(ids_to_unref, counts, std::size(ids_to_unref));
    SubmitCompositorFrameWithResources(nullptr, 0);

    MaybeTestOnBeginFrame(1);
    ResourceId expected_returned_ids[] = {ResourceId(3)};
    int expected_returned_counts[] = {1};
    CheckReturnedResourcesMatchExpected(
        expected_returned_ids, expected_returned_counts,
        std::size(expected_returned_counts), consumer_sync_token_);
  }

  // Expected refs remaining:
  //  4 -> 3
  //  5 -> 1
  {
    SCOPED_TRACE("unref 4 and 5");
    ResourceId ids_to_unref[] = {ResourceId(4), ResourceId(5)};
    int counts[] = {1, 1};
    UnrefResources(ids_to_unref, counts, std::size(ids_to_unref));
    SubmitCompositorFrameWithResources(nullptr, 0);

    MaybeTestOnBeginFrame(2);
    ResourceId expected_returned_ids[] = {ResourceId(5)};
    int expected_returned_counts[] = {1};
    CheckReturnedResourcesMatchExpected(
        expected_returned_ids, expected_returned_counts,
        std::size(expected_returned_counts), consumer_sync_token_);
  }

  // Now, just 2 refs remaining on resource 4. Unref both at once and make sure
  // the returned count is correct.
  {
    SCOPED_TRACE("unref only 4");
    ResourceId ids_to_unref[] = {ResourceId(4)};
    int counts[] = {2};
    UnrefResources(ids_to_unref, counts, std::size(ids_to_unref));
    SubmitCompositorFrameWithResources(nullptr, 0);

    MaybeTestOnBeginFrame(3);
    ResourceId expected_returned_ids[] = {ResourceId(4)};
    int expected_returned_counts[] = {2};
    CheckReturnedResourcesMatchExpected(
        expected_returned_ids, expected_returned_counts,
        std::size(expected_returned_counts), consumer_sync_token_);
  }
}

TEST_P(OnBeginFrameAcksCompositorFrameSinkSupportTest, ResourceLifetime) {
  support_->SetNeedsBeginFrame(true);
  ResourceId first_frame_ids[] = {ResourceId(1), ResourceId(2), ResourceId(3)};
  SubmitCompositorFrameWithResources(first_frame_ids,
                                     std::size(first_frame_ids));

  // This test relied on CompositorFrameSinkSupport::ReturnResources to not send
  // as long as there has been no DidReceiveCompositorFrameAck. Such that
  // the number of pending frames is always greater than 1.
  //
  // With features::kOnBeginFrameAcks we now return the resources during
  // OnBeginFrame, however that is throttled while we await any ack.
  //
  // Renderers, the principal Viz Client, do not submit new CompositorFrames as
  // long as there is a pending ack. So the original testing scenario here does
  // not occur.
  MaybeSendCompositorFrameAck();

  // All of the resources submitted in the first frame are still in use at this
  // time by virtue of being in the pending frame, so none can be returned to
  // the client yet.
  EXPECT_EQ(0u, fake_support_client_.returned_resources().size());
  fake_support_client_.clear_returned_resources();

  // The second frame references some of the same resources, but some different
  // ones. We expect to receive back resource 1 with a count of 1 since it was
  // only referenced by the first frame.
  ResourceId second_frame_ids[] = {ResourceId(2), ResourceId(3), ResourceId(4)};
  SubmitCompositorFrameWithResources(second_frame_ids,
                                     std::size(second_frame_ids));
  {
    SCOPED_TRACE("second frame");
    MaybeTestOnBeginFrame(1);
    ResourceId expected_returned_ids[] = {ResourceId(1)};
    int expected_returned_counts[] = {1};
    CheckReturnedResourcesMatchExpected(
        expected_returned_ids, expected_returned_counts,
        std::size(expected_returned_counts), gpu::SyncToken());
  }

  // The third frame references a disjoint set of resources, so we expect to
  // receive back all resources from the first and second frames. Resource IDs 2
  // and 3 will have counts of 2, since they were used in both frames, and
  // resource ID 4 will have a count of 1.
  ResourceId third_frame_ids[] = {ResourceId(10), ResourceId(11),
                                  ResourceId(12), ResourceId(13)};
  SubmitCompositorFrameWithResources(third_frame_ids,
                                     std::size(third_frame_ids));

  {
    SCOPED_TRACE("third frame");
    MaybeTestOnBeginFrame(2);
    ResourceId expected_returned_ids[] = {ResourceId(2), ResourceId(3),
                                          ResourceId(4)};
    int expected_returned_counts[] = {2, 2, 1};
    CheckReturnedResourcesMatchExpected(
        expected_returned_ids, expected_returned_counts,
        std::size(expected_returned_counts), gpu::SyncToken());
  }

  // Simulate a ResourceProvider taking a ref on all of the resources.
  RefCurrentFrameResources();

  ResourceId fourth_frame_ids[] = {ResourceId(12), ResourceId(13)};
  SubmitCompositorFrameWithResources(fourth_frame_ids,
                                     std::size(fourth_frame_ids));

  EXPECT_EQ(0u, fake_support_client_.returned_resources().size());

  RefCurrentFrameResources();

  // All resources are still being used by the external reference, so none can
  // be returned to the client.
  EXPECT_EQ(0u, fake_support_client_.returned_resources().size());

  // Release resources associated with the first RefCurrentFrameResources() call
  // first.
  {
    ResourceId ids_to_unref[] = {ResourceId(10), ResourceId(11), ResourceId(12),
                                 ResourceId(13)};
    int counts[] = {1, 1, 1, 1};
    // With BeginFrameAcksEnabled, we do not attempt to unref here, either.
    // However, if we have disabled begin frame acks as well as always acking
    // upon surface activation, we will not have any pending surfaces and we
    // will unref here.
    UnrefResources(ids_to_unref, counts, std::size(ids_to_unref));
  }

  // Nothing is returned to the client yet since DidReceiveCompositorFrameAck
  // is not invoked.
  {
    SCOPED_TRACE("fourth frame, first unref");
    CheckReturnedResourcesMatchExpected(nullptr, nullptr, 0,
                                        consumer_sync_token_);
  }

  {
    ResourceId ids_to_unref[] = {ResourceId(12), ResourceId(13)};
    int counts[] = {1, 1};
    UnrefResources(ids_to_unref, counts, std::size(ids_to_unref));
  }

  // Resources 12 and 13 are still in use by the current frame, so they
  // shouldn't be available to be returned.
  EXPECT_EQ(0u, fake_support_client_.returned_resources().size());

  // If we submit an empty frame, however, they should become available.
  // Resources that were previously unref'd also return at this point.
  SubmitCompositorFrameWithResources(nullptr, 0u);

  {
    SCOPED_TRACE("fourth frame, second unref");
    MaybeTestOnBeginFrame(3);
    ResourceId expected_returned_ids[] = {ResourceId(10), ResourceId(11),
                                          ResourceId(12), ResourceId(13)};
    int expected_returned_counts[] = {1, 1, 2, 2};
    CheckReturnedResourcesMatchExpected(
        expected_returned_ids, expected_returned_counts,
        std::size(expected_returned_counts), consumer_sync_token_);
  }
}

TEST_P(OnBeginFrameAcksCompositorFrameSinkSupportTest, AddDuringEviction) {
  manager_->RegisterFrameSinkId(kAnotherArbitraryFrameSinkId,
                                true /* report_activation */);
  MockCompositorFrameSinkClient mock_client;
  auto support = std::make_unique<CompositorFrameSinkSupport>(
      &mock_client, manager_.get(), kAnotherArbitraryFrameSinkId, kIsRoot);
  if (BeginFrameAcksEnabled()) {
    support->SetWantsBeginFrameAcks();
  }

  SurfaceManager* surface_manager = manager_->surface_manager();
  auto submit_compositor_frame = [&]() {
    LocalSurfaceId new_id(7, base::UnguessableToken::Create());
    support->SubmitCompositorFrame(
        new_id, MakeDefaultInteractiveCompositorFrame(kBeginFrameSourceId));
    surface_manager->GarbageCollectSurfaces();
  };

  LocalSurfaceId local_surface_id(6, kArbitraryToken);
  support->SubmitCompositorFrame(
      local_surface_id,
      MakeDefaultInteractiveCompositorFrame(kBeginFrameSourceId));

  if (BeginFrameAcksEnabled()) {
    EXPECT_CALL(mock_client, DidReceiveCompositorFrameAck(_)).Times(0);
  } else {
    EXPECT_CALL(mock_client, DidReceiveCompositorFrameAck(_))
        .WillOnce(submit_compositor_frame)
        .WillRepeatedly(testing::Return());
  }
  support->EvictSurface(local_surface_id);
  ExpireAllTemporaryReferences();
  manager_->InvalidateFrameSinkId(kAnotherArbitraryFrameSinkId);

  if (BeginFrameAcksEnabled()) {
    submit_compositor_frame();
    testing::Mock::VerifyAndClearExpectations(&mock_client);
  }
  EXPECT_EQ(1, num_pending_frames(support.get()));
}

// Verifies that only monotonically increasing LocalSurfaceIds are accepted.
TEST_P(CompositorFrameSinkSupportTest, MonotonicallyIncreasingLocalSurfaceIds) {
  manager_->RegisterFrameSinkId(kAnotherArbitraryFrameSinkId,
                                true /* report_activation */);
  MockCompositorFrameSinkClient mock_client;
  auto support = std::make_unique<CompositorFrameSinkSupport>(
      &mock_client, manager_.get(), kAnotherArbitraryFrameSinkId, kIsRoot);
  base::UnguessableToken embed_token = base::UnguessableToken::Create();
  LocalSurfaceId local_surface_id1(6, 1, embed_token);
  LocalSurfaceId local_surface_id2(6, 2, embed_token);
  LocalSurfaceId local_surface_id3(7, 2, embed_token);
  LocalSurfaceId local_surface_id4(5, 3, embed_token);
  LocalSurfaceId local_surface_id5(8, 1, embed_token);
  LocalSurfaceId local_surface_id6(9, 3, embed_token);

  // LocalSurfaceId1(6, 1)
  auto result = support->MaybeSubmitCompositorFrame(
      local_surface_id1, MakeDefaultInteractiveCompositorFrame(), std::nullopt,
      0, mojom::CompositorFrameSink::SubmitCompositorFrameSyncCallback());
  EXPECT_EQ(SubmitResult::ACCEPTED, result);

  // LocalSurfaceId(6, 2): Child-initiated synchronization.
  result = support->MaybeSubmitCompositorFrame(
      local_surface_id2, MakeDefaultInteractiveCompositorFrame(), std::nullopt,
      0, mojom::CompositorFrameSink::SubmitCompositorFrameSyncCallback());
  EXPECT_EQ(SubmitResult::ACCEPTED, result);

  // LocalSurfaceId(7, 2): Parent-initiated synchronization.
  result = support->MaybeSubmitCompositorFrame(
      local_surface_id3, MakeDefaultInteractiveCompositorFrame(), std::nullopt,
      0, mojom::CompositorFrameSink::SubmitCompositorFrameSyncCallback());
  EXPECT_EQ(SubmitResult::ACCEPTED, result);

  // LocalSurfaceId(5, 3): Submit rejected because not monotonically increasing.
  result = support->MaybeSubmitCompositorFrame(
      local_surface_id4, MakeDefaultInteractiveCompositorFrame(), std::nullopt,
      0, mojom::CompositorFrameSink::SubmitCompositorFrameSyncCallback());
  EXPECT_EQ(SubmitResult::SURFACE_ID_DECREASED, result);

  // LocalSurfaceId(8, 1): Submit rejected because not monotonically increasing.
  result = support->MaybeSubmitCompositorFrame(
      local_surface_id5, MakeDefaultInteractiveCompositorFrame(), std::nullopt,
      0, mojom::CompositorFrameSink::SubmitCompositorFrameSyncCallback());
  EXPECT_EQ(SubmitResult::SURFACE_ID_DECREASED, result);

  // LocalSurfaceId(9, 3): Parent AND child-initiated synchronization.
  result = support->MaybeSubmitCompositorFrame(
      local_surface_id6, MakeDefaultInteractiveCompositorFrame(), std::nullopt,
      0, mojom::CompositorFrameSink::SubmitCompositorFrameSyncCallback());
  EXPECT_EQ(SubmitResult::ACCEPTED, result);

  manager_->InvalidateFrameSinkId(kAnotherArbitraryFrameSinkId);
}

// Verifies that CopyOutputRequests submitted by unprivileged clients are
// rejected.
TEST_P(OnBeginFrameAcksCompositorFrameSinkSupportTest,
       ProhibitsUnprivilegedCopyRequests) {
  manager_->RegisterFrameSinkId(kAnotherArbitraryFrameSinkId,
                                true /* report_activation */);
  MockCompositorFrameSinkClient mock_client;
  auto support = std::make_unique<CompositorFrameSinkSupport>(
      &mock_client, manager_.get(), kAnotherArbitraryFrameSinkId,
      false /* not root frame sink */);

  bool did_receive_aborted_copy_result = false;
  base::RunLoop aborted_copy_run_loop;
  auto request = std::make_unique<CopyOutputRequest>(
      CopyOutputRequest::ResultFormat::RGBA,
      CopyOutputRequest::ResultDestination::kSystemMemory,
      base::BindOnce(
          [](bool* got_nothing, base::OnceClosure finished,
             std::unique_ptr<CopyOutputResult> result) {
            *got_nothing = result->IsEmpty();
            std::move(finished).Run();
          },
          &did_receive_aborted_copy_result,
          aborted_copy_run_loop.QuitClosure()));

  auto frame = MakeDefaultInteractiveCompositorFrame();
  ResourceId frame_resource_ids[] = {ResourceId(1), ResourceId(2),
                                     ResourceId(3)};
  AddResourcesToFrame(&frame, frame_resource_ids,
                      std::size(frame_resource_ids));

  EXPECT_FALSE(SubmitCompositorFrameWithCopyRequest(std::move(frame),
                                                    std::move(request)));
  aborted_copy_run_loop.Run();
  EXPECT_TRUE(did_receive_aborted_copy_result);

  MaybeTestOnBeginFrame(1);

  // All the resources in the rejected frame should have been returned.
  CheckReturnedResourcesMatchExpected(frame_resource_ids,
                                      std::size(frame_resource_ids));

  manager_->InvalidateFrameSinkId(kAnotherArbitraryFrameSinkId);
}

// Tests doing an EvictLastActivatedSurface before shutting down the factory.
TEST_P(OnBeginFrameAcksCompositorFrameSinkSupportTest,
       EvictLastActivatedSurface) {
  manager_->RegisterFrameSinkId(kAnotherArbitraryFrameSinkId,
                                true /* report_activation */);
  MockCompositorFrameSinkClient mock_client;
  auto support = std::make_unique<CompositorFrameSinkSupport>(
      &mock_client, manager_.get(), kAnotherArbitraryFrameSinkId, kIsRoot);
  if (BeginFrameAcksEnabled()) {
    support->SetWantsBeginFrameAcks();
  }
  LocalSurfaceId local_surface_id(7, kArbitraryToken);
  SurfaceId id(kAnotherArbitraryFrameSinkId, local_surface_id);

  TransferableResource resource;
  resource.id = ResourceId(1);
  resource.set_texture_target(GL_TEXTURE_2D);
  auto frame = CompositorFrameBuilder()
                   .AddDefaultRenderPass()
                   .AddTransferableResource(resource)
                   .SetBeginFrameSourceId(kBeginFrameSourceId)
                   .SetIsHandlingInteraction(true)
                   .Build();
  support->SubmitCompositorFrame(local_surface_id, std::move(frame));
  EXPECT_EQ(surface_observer_->last_created_surface_id().local_surface_id(),
            local_surface_id);
  local_surface_id_ = LocalSurfaceId();

  ResourceId returned_id = resource.ToReturnedResource().id;
  EXPECT_TRUE(GetSurfaceForId(id));
  auto expected_returned_resources = [=](std::vector<ReturnedResource> got) {
    EXPECT_EQ(1u, got.size());
    EXPECT_EQ(returned_id, got[0].id);
  };
  // If always ack is enabled, in the compositor frame ack case, we we would
  // have already received the ack so we shouldn't get one later.
  if (BeginFrameAcksEnabled()) {
    EXPECT_CALL(mock_client, DidReceiveCompositorFrameAck(_)).Times(0);
    EXPECT_CALL(mock_client, ReclaimResources(_))
        .WillOnce(expected_returned_resources);
  } else {
    EXPECT_CALL(mock_client, DidReceiveCompositorFrameAck(_))
        .WillOnce(expected_returned_resources);
  }
  support->EvictSurface(local_surface_id);
  ExpireAllTemporaryReferences();
  manager_->surface_manager()->GarbageCollectSurfaces();
  EXPECT_FALSE(GetSurfaceForId(id));
  manager_->InvalidateFrameSinkId(kAnotherArbitraryFrameSinkId);
}

// This test checks the case where a client submits a CompositorFrame for a
// SurfaceId that has been evicted. The CompositorFrame must be immediately
// evicted.
TEST_P(CompositorFrameSinkSupportTest, ResurectAndImmediatelyEvict) {
  LocalSurfaceId local_surface_id(1, kArbitraryToken);
  SurfaceId surface_id(kArbitraryFrameSinkId, local_surface_id);

  auto frame = CompositorFrameBuilder()
                   .AddDefaultRenderPass()
                   .SetIsHandlingInteraction(true)
                   .Build();
  support_->SubmitCompositorFrame(local_surface_id, std::move(frame));

  // The surface should be activated now.
  EXPECT_EQ(support_->last_activated_surface_id(), surface_id);

  // Evict the surface. Make surface CompositorFrameSinkSupport reflects this.
  manager_->EvictSurfaces({surface_id});
  EXPECT_FALSE(support_->last_activated_surface_id().is_valid());

  // We don't garbage collect the evicted surface yet because either garbage
  // collection hasn't run or something still has a reference to it.

  // Submit the late CompositorFrame which will resurrect the Surface and
  // trigger another eviction.
  frame = CompositorFrameBuilder()
              .AddDefaultRenderPass()
              .SetIsHandlingInteraction(true)
              .Build();
  support_->SubmitCompositorFrame(local_surface_id, std::move(frame));

  // The Surface should be evicted again immediately after it's submitted.
  EXPECT_FALSE(support_->last_activated_surface_id().is_valid());
}

// Verify that a temporary reference does not block surface eviction.
TEST_P(CompositorFrameSinkSupportTest, EvictSurfaceWithTemporaryReference) {
  constexpr FrameSinkId parent_frame_sink_id(1234, 5678);

  manager_->RegisterFrameSinkId(parent_frame_sink_id,
                                true /* report_activation */);

  const LocalSurfaceId local_surface_id(5, kArbitraryToken);
  const SurfaceId surface_id(support_->frame_sink_id(), local_surface_id);

  // When CompositorFrame is submitted, a temporary reference will be created.
  support_->SubmitCompositorFrame(local_surface_id,
                                  MakeDefaultInteractiveCompositorFrame());
  EXPECT_TRUE(HasTemporaryReference(surface_id));

  // Verify the temporary reference has not prevented the surface from getting
  // destroyed.
  support_->EvictSurface(local_surface_id);
  manager_->surface_manager()->GarbageCollectSurfaces();
  EXPECT_FALSE(GetSurfaceForId(surface_id));
}

// Verifies that evicting a surface destroys all older surfaces as well.
TEST_P(CompositorFrameSinkSupportTest, EvictOlderSurfaces) {
  constexpr FrameSinkId parent_frame_sink_id(1234, 5678);

  manager_->RegisterFrameSinkId(parent_frame_sink_id,
                                true /* report_activation */);

  const LocalSurfaceId local_surface_id1(5, kArbitraryToken);
  const LocalSurfaceId local_surface_id2(6, kArbitraryToken);
  const SurfaceId surface_id1(support_->frame_sink_id(), local_surface_id1);
  const SurfaceId surface_id2(support_->frame_sink_id(), local_surface_id2);

  // When CompositorFrame is submitted, a temporary reference will be created.
  support_->SubmitCompositorFrame(local_surface_id1,
                                  MakeDefaultInteractiveCompositorFrame());
  EXPECT_TRUE(HasTemporaryReference(surface_id1));

  // Evict |surface_id2|. |surface_id1| should be evicted too.
  support_->EvictSurface(local_surface_id2);
  manager_->surface_manager()->GarbageCollectSurfaces();
  EXPECT_FALSE(GetSurfaceForId(surface_id1));
}

void CopyRequestTestCallback(bool* called,
                             base::OnceClosure finished,
                             std::unique_ptr<CopyOutputResult> result) {
  *called = true;
  std::move(finished).Run();
}

TEST_P(CompositorFrameSinkSupportTest, CopyRequestOnSubtree) {
  const SurfaceId surface_id(support_->frame_sink_id(), local_surface_id_);

  constexpr SubtreeCaptureId kSubtreeId1(base::Token(0, 22u));
  constexpr SubtreeCaptureId kSubtreeId2(base::Token(0, 44u));

  {
    auto frame = CompositorFrameBuilder()
                     .AddDefaultRenderPass()
                     .AddDefaultRenderPass()
                     .SetReferencedSurfaces({SurfaceRange(surface_id)})
                     .SetIsHandlingInteraction(true)
                     .Build();
    frame.render_pass_list.front()->subtree_capture_id = kSubtreeId1;
    support_->SubmitCompositorFrame(local_surface_id_, std::move(frame));
    EXPECT_EQ(surface_observer_->last_created_surface_id().local_surface_id(),
              local_surface_id_);
  }

  // Requesting copy of output of a render pass identifiable by a valid
  // SubtreeCaptureId.
  bool called1 = false;
  base::RunLoop called1_run_loop;
  auto request = std::make_unique<CopyOutputRequest>(
      CopyOutputRequest::ResultFormat::RGBA,
      CopyOutputRequest::ResultDestination::kSystemMemory,
      base::BindOnce(&CopyRequestTestCallback, &called1,
                     called1_run_loop.QuitClosure()));
  support_->RequestCopyOfOutput(
      {local_surface_id_, kSubtreeId1, std::move(request)});
  GetSurfaceForId(surface_id)->TakeCopyOutputRequestsFromClient();
  EXPECT_FALSE(called1);

  // Requesting copy of output using a SubtreeCaptureId that has no associated
  // render pass. The callback will be called immediately.
  bool called2 = false;
  base::RunLoop called2_run_loop;
  request = std::make_unique<CopyOutputRequest>(
      CopyOutputRequest::ResultFormat::RGBA,
      CopyOutputRequest::ResultDestination::kSystemMemory,
      base::BindOnce(&CopyRequestTestCallback, &called2,
                     called2_run_loop.QuitClosure()));
  support_->RequestCopyOfOutput(
      {local_surface_id_, kSubtreeId2, std::move(request)});
  GetSurfaceForId(surface_id)->TakeCopyOutputRequestsFromClient();
  called2_run_loop.Run();
  EXPECT_FALSE(called1);
  EXPECT_TRUE(called2);

  support_->EvictSurface(local_surface_id_);
  ExpireAllTemporaryReferences();
  local_surface_id_ = LocalSurfaceId();
  manager_->surface_manager()->GarbageCollectSurfaces();
  called1_run_loop.Run();
  EXPECT_TRUE(called1);
}

TEST_P(CompositorFrameSinkSupportTest, DuplicateCopyRequest) {
  const base::UnguessableToken source_id1 = base::UnguessableToken::Create();
  const base::UnguessableToken source_id2 = base::UnguessableToken::Create();

  const SurfaceId surface_id(support_->frame_sink_id(), local_surface_id_);

  {
    auto frame = CompositorFrameBuilder()
                     .AddDefaultRenderPass()
                     .SetReferencedSurfaces({SurfaceRange(surface_id)})
                     .SetIsHandlingInteraction(true)
                     .Build();
    support_->SubmitCompositorFrame(local_surface_id_, std::move(frame));
    EXPECT_EQ(surface_observer_->last_created_surface_id().local_surface_id(),
              local_surface_id_);
  }

  bool called1 = false;
  base::RunLoop called1_run_loop;
  auto request = std::make_unique<CopyOutputRequest>(
      CopyOutputRequest::ResultFormat::RGBA,
      CopyOutputRequest::ResultDestination::kSystemMemory,
      base::BindOnce(&CopyRequestTestCallback, &called1,
                     called1_run_loop.QuitClosure()));
  request->set_source(source_id1);

  support_->RequestCopyOfOutput(
      {local_surface_id_, SubtreeCaptureId(), std::move(request)});
  GetSurfaceForId(surface_id)->TakeCopyOutputRequestsFromClient();
  EXPECT_FALSE(called1);

  bool called2 = false;
  base::RunLoop called2_run_loop;
  request = std::make_unique<CopyOutputRequest>(
      CopyOutputRequest::ResultFormat::RGBA,
      CopyOutputRequest::ResultDestination::kSystemMemory,
      base::BindOnce(&CopyRequestTestCallback, &called2,
                     called2_run_loop.QuitClosure()));
  request->set_source(source_id2);

  support_->RequestCopyOfOutput(
      {local_surface_id_, SubtreeCaptureId(), std::move(request)});
  GetSurfaceForId(surface_id)->TakeCopyOutputRequestsFromClient();
  // Callbacks have different sources so neither should be called.
  EXPECT_FALSE(called1);
  EXPECT_FALSE(called2);

  bool called3 = false;
  base::RunLoop called3_run_loop;
  request = std::make_unique<CopyOutputRequest>(
      CopyOutputRequest::ResultFormat::RGBA,
      CopyOutputRequest::ResultDestination::kSystemMemory,
      base::BindOnce(&CopyRequestTestCallback, &called3,
                     called3_run_loop.QuitClosure()));
  request->set_source(source_id1);

  support_->RequestCopyOfOutput(
      {local_surface_id_, SubtreeCaptureId(), std::move(request)});
  GetSurfaceForId(surface_id)->TakeCopyOutputRequestsFromClient();
  // Two callbacks are from source1, so the first should be called.
  called1_run_loop.Run();
  EXPECT_TRUE(called1);
  EXPECT_FALSE(called2);
  EXPECT_FALSE(called3);

  support_->EvictSurface(local_surface_id_);
  ExpireAllTemporaryReferences();
  local_surface_id_ = LocalSurfaceId();
  manager_->surface_manager()->GarbageCollectSurfaces();
  called2_run_loop.Run();
  called3_run_loop.Run();
  EXPECT_TRUE(called1);
  EXPECT_TRUE(called2);
  EXPECT_TRUE(called3);
}

// Check whether the SurfaceInfo object is created and populated correctly
// after the frame submission.
TEST_P(CompositorFrameSinkSupportTest, SurfaceInfo) {
  auto frame = CompositorFrameBuilder()
                   .AddRenderPass(gfx::Rect(5, 6), gfx::Rect())
                   .AddRenderPass(gfx::Rect(7, 8), gfx::Rect())
                   .SetDeviceScaleFactor(2.5f)
                   .SetIsHandlingInteraction(true)
                   .Build();

  support_->SubmitCompositorFrame(local_surface_id_, std::move(frame));
  SurfaceId expected_surface_id(support_->frame_sink_id(), local_surface_id_);
  EXPECT_EQ(expected_surface_id, surface_observer_->last_surface_info().id());
  EXPECT_EQ(2.5f, surface_observer_->last_surface_info().device_scale_factor());
  EXPECT_EQ(gfx::Size(7, 8),
            surface_observer_->last_surface_info().size_in_pixels());
}

// Check that if the size of a CompositorFrame doesn't match the size of the
// Surface it's being submitted to, we skip the frame.
TEST_P(OnBeginFrameAcksCompositorFrameSinkSupportTest, FrameSizeMismatch) {
  SurfaceId id(support_->frame_sink_id(), local_surface_id_);

  // Submit a frame with size (5,5).
  auto frame = CompositorFrameBuilder()
                   .AddRenderPass(gfx::Rect(5, 5), gfx::Rect())
                   .SetIsHandlingInteraction(true)
                   .Build();
  auto result = support_->MaybeSubmitCompositorFrame(
      local_surface_id_, std::move(frame), std::nullopt, 0,
      mojom::CompositorFrameSink::SubmitCompositorFrameSyncCallback());
  EXPECT_EQ(SubmitResult::ACCEPTED, result);
  EXPECT_TRUE(GetSurfaceForId(id));

  // Submit a frame with size (5,4). This frame should be rejected and the
  // surface should be destroyed.
  frame = CompositorFrameBuilder()
              .AddRenderPass(gfx::Rect(5, 4), gfx::Rect())
              .SetIsHandlingInteraction(true)
              .Build();
  ResourceId frame_resource_ids[] = {ResourceId(1), ResourceId(2),
                                     ResourceId(3)};
  AddResourcesToFrame(&frame, frame_resource_ids,
                      std::size(frame_resource_ids));

  result = support_->MaybeSubmitCompositorFrame(
      local_surface_id_, std::move(frame), std::nullopt, 0,
      mojom::CompositorFrameSink::SubmitCompositorFrameSyncCallback());

  EXPECT_EQ(SubmitResult::SIZE_MISMATCH, result);

  MaybeSendCompositorFrameAck();
  MaybeTestOnBeginFrame(1);

  // All the resources in the rejected frame should have been returned.
  CheckReturnedResourcesMatchExpected(frame_resource_ids,
                                      std::size(frame_resource_ids));
}

// Check that if the device scale factor of a CompositorFrame doesn't match the
// device scale factor of the Surface it's being submitted to, the frame is
// rejected and the surface is destroyed.
TEST_P(CompositorFrameSinkSupportTest, DeviceScaleFactorMismatch) {
  SurfaceId id(support_->frame_sink_id(), local_surface_id_);

  // Submit a frame with device scale factor of 0.5.
  auto frame = CompositorFrameBuilder()
                   .AddDefaultRenderPass()
                   .SetDeviceScaleFactor(0.5f)
                   .SetIsHandlingInteraction(true)
                   .Build();
  auto result = support_->MaybeSubmitCompositorFrame(
      local_surface_id_, std::move(frame), std::nullopt, 0,
      mojom::CompositorFrameSink::SubmitCompositorFrameSyncCallback());
  EXPECT_EQ(SubmitResult::ACCEPTED, result);
  EXPECT_TRUE(GetSurfaceForId(id));

  // Submit a frame with device scale factor of 0.4. This frame should be
  // rejected and the surface should be destroyed.
  frame = CompositorFrameBuilder()
              .AddDefaultRenderPass()
              .SetDeviceScaleFactor(0.4f)
              .SetIsHandlingInteraction(true)
              .Build();
  result = support_->MaybeSubmitCompositorFrame(
      local_surface_id_, std::move(frame), std::nullopt, 0,
      mojom::CompositorFrameSink::SubmitCompositorFrameSyncCallback());
  EXPECT_EQ(SubmitResult::SIZE_MISMATCH, result);
}

TEST_P(CompositorFrameSinkSupportTest, PassesOnBeginFrameAcks) {
  // Request BeginFrames.
  support_->SetNeedsBeginFrame(true);

  // Issue a BeginFrame.
  BeginFrameArgs args =
      CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1);
  begin_frame_source_.TestOnBeginFrame(args);

  // Check that the support and SurfaceManager forward the BeginFrameAck
  // attached to a CompositorFrame to the SurfaceObserver.
  BeginFrameAck ack(0, 1, true);
  CompositorFrame frame = CompositorFrameBuilder()
                              .AddDefaultRenderPass()
                              .SetBeginFrameAck(ack)
                              .SetIsHandlingInteraction(true)
                              .Build();
  support_->SubmitCompositorFrame(local_surface_id_, std::move(frame));
  EXPECT_EQ(ack, surface_observer_->last_ack());

  // Issue another BeginFrame.
  args = CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 2);
  begin_frame_source_.TestOnBeginFrame(args);

  // Check that the support and SurfaceManager forward a DidNotProduceFrame ack
  // to the SurfaceObserver.
  BeginFrameAck ack2(0, 2, false);
  support_->DidNotProduceFrame(ack2);
  EXPECT_EQ(ack2, surface_observer_->last_ack());

  support_->SetNeedsBeginFrame(false);
}

// Validates that if a client asked to stop receiving begin-frames, then it
// stops receiving begin-frames after receiving the presentation-feedback from
// the last submitted frame.
TEST_P(OnBeginFrameAcksCompositorFrameSinkSupportTest,
       NeedsBeginFrameResetAfterPresentationFeedback) {
  // Request BeginFrames.
  support_->SetNeedsBeginFrame(true);

  // Issue a BeginFrame. Validate that the client receives the begin-frame.
  BeginFrameArgs args =
      CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1);
  begin_frame_source_.TestOnBeginFrame(args);
  BeginFrameArgs received_args = GetLastUsedBeginFrameArgs(support_.get());
  EXPECT_TRUE(BeginFrameArgsAreEquivalent(args, received_args));
  EXPECT_EQ(received_args.type, BeginFrameArgs::NORMAL);

  // Client submits a compositor frame in response.
  BeginFrameAck ack(args, true);
  CompositorFrame frame = CompositorFrameBuilder()
                              .AddDefaultRenderPass()
                              .SetBeginFrameAck(ack)
                              .SetIsHandlingInteraction(true)
                              .Build();
  auto token = frame.metadata.frame_token;
  support_->SubmitCompositorFrame(local_surface_id_, std::move(frame));

  // Client stops asking for begin-frames.
  support_->SetNeedsBeginFrame(false);

  // Issue a new BeginFrame. This time, the client should not receive it since
  // it has stopped asking for begin-frames. However, this is only true if we
  // haven't forced a begin frame for another reason (such as sending an ack).
  args = CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 1, 2);
  begin_frame_source_.TestOnBeginFrame(args);
  received_args = GetLastUsedBeginFrameArgs(support_.get());
  EXPECT_FALSE(BeginFrameArgsAreEquivalent(args, received_args));

  // The ACK from the last submitted frame arrives. If BeginFrameAcks is
  // enabled this results in the client immediately receiving a MISSED
  // begin-frame.
  support_->SendCompositorFrameAck();

  if (BeginFrameAcksEnabled()) {
    received_args = GetLastUsedBeginFrameArgs(support_.get());
    EXPECT_TRUE(BeginFrameArgsAreEquivalent(args, received_args));
    EXPECT_EQ(received_args.type, BeginFrameArgs::MISSED);

    // Issue a new BeginFrame. This time, the client should not receive it since
    // it has stopped asking for begin-frames.
    args = CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 2, 3);
    begin_frame_source_.TestOnBeginFrame(args);
    received_args = GetLastUsedBeginFrameArgs(support_.get());
    EXPECT_FALSE(BeginFrameArgsAreEquivalent(args, received_args));
  }

  // The presentation-feedback from the last submitted frame arrives. This
  // results in the client immediately receiving a MISSED begin-frame.
  SendPresentationFeedback(support_.get(), token);
  received_args = GetLastUsedBeginFrameArgs(support_.get());
  EXPECT_TRUE(BeginFrameArgsAreEquivalent(args, received_args));
  EXPECT_EQ(received_args.type, BeginFrameArgs::MISSED);

  // Issue another begin-frame. This time, the client should not receive it
  // anymore since it has stopped asking for begin-frames, and it has already
  // received the last presentation-feedback.
  args = CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 3, 4);
  begin_frame_source_.TestOnBeginFrame(args);
  received_args = GetLastUsedBeginFrameArgs(support_.get());
  EXPECT_FALSE(BeginFrameArgsAreEquivalent(args, received_args));
}

// Validates that if the client wants AutoNeedsBeginFrame, an unsolicited frame
// starts subsequent BeginFrames, as if SetNeedsBeginFrame(true) is called.
TEST_P(OnBeginFrameAcksCompositorFrameSinkSupportTest,
       AutoNeedsBeginFrameOnUnsolicitedFrame) {
  support_->SetAutoNeedsBeginFrame();

  EXPECT_FALSE(client_needs_begin_frame(support_.get()));

  BeginFrameArgs args =
      CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 2, 1);
  begin_frame_source_.TestOnBeginFrame(args);

  EXPECT_EQ(fake_support_client_.begin_frame_count(), 0);

  CompositorFrame unsolicited_frame =
      MakeDefaultInteractiveCompositorFrame(BeginFrameArgs::kManualSourceId);
  support_->SubmitCompositorFrame(local_surface_id_,
                                  std::move(unsolicited_frame));

  EXPECT_TRUE(client_needs_begin_frame(support_.get()));

  // BeginFrame is not sent synchronously while processing unsolicited frame.
  EXPECT_EQ(fake_support_client_.begin_frame_count(), 0);

  args = CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 2, 2);
  begin_frame_source_.TestOnBeginFrame(args);

  EXPECT_EQ(fake_support_client_.begin_frame_count(), 1);
}

TEST_P(CompositorFrameSinkSupportTest, FrameIndexCarriedOverToNewSurface) {
  LocalSurfaceId local_surface_id1(1, kArbitraryToken);
  LocalSurfaceId local_surface_id2(2, kArbitraryToken);
  SurfaceId id1(support_->frame_sink_id(), local_surface_id1);
  SurfaceId id2(support_->frame_sink_id(), local_surface_id2);

  // Submit a frame to |id1| and record the frame index.
  support_->SubmitCompositorFrame(local_surface_id1,
                                  MakeDefaultInteractiveCompositorFrame());
  Surface* surface1 = GetSurfaceForId(id1);
  uint64_t frame_index = surface1->GetActiveFrameIndex();

  // Submit a frame to |id2| and verify that the new frame index is one more
  // than what we had before.
  support_->SubmitCompositorFrame(local_surface_id2,
                                  MakeDefaultInteractiveCompositorFrame());
  Surface* surface2 = GetSurfaceForId(id2);
  EXPECT_EQ(frame_index + 1, surface2->GetActiveFrameIndex());
}

// Verifies that CopyOutputRequests made at frame sink level are sent to the
// surface that takes them first. In this test this surface is not the last
// activated surface.
TEST_P(CompositorFrameSinkSupportTest,
       OldSurfaceTakesCopyOutputRequestsFromClient) {
  LocalSurfaceId local_surface_id1(1, kArbitraryToken);
  LocalSurfaceId local_surface_id2(2, kArbitraryToken);
  SurfaceId id1(support_->frame_sink_id(), local_surface_id1);
  SurfaceId id2(support_->frame_sink_id(), local_surface_id2);

  // Create the first surface.
  support_->SubmitCompositorFrame(local_surface_id1,
                                  MakeDefaultInteractiveCompositorFrame());

  // Create the second surface.
  support_->SubmitCompositorFrame(local_surface_id2,
                                  MakeDefaultInteractiveCompositorFrame());

  // Send a CopyOutputRequest.
  auto request = std::make_unique<CopyOutputRequest>(
      CopyOutputRequest::ResultFormat::RGBA,
      CopyOutputRequest::ResultDestination::kSystemMemory,
      base::BindOnce(StubResultCallback));
  support_->RequestCopyOfOutput(
      {local_surface_id1, SubtreeCaptureId(), std::move(request)});

  // First surface takes CopyOutputRequests from its client. Now only the first
  // surface should report having CopyOutputRequests.
  GetSurfaceForId(id1)->TakeCopyOutputRequestsFromClient();
  EXPECT_TRUE(GetSurfaceForId(id1)->HasCopyOutputRequests());
  EXPECT_FALSE(GetSurfaceForId(id2)->HasCopyOutputRequests());

  // Calling TakeCopyOutputRequestsFromClient() on the second surface should
  // have no effect.
  GetSurfaceForId(id2)->TakeCopyOutputRequestsFromClient();
  EXPECT_TRUE(GetSurfaceForId(id1)->HasCopyOutputRequests());
  EXPECT_FALSE(GetSurfaceForId(id2)->HasCopyOutputRequests());

  // Also try TakeCopyOutputRequests, to see if its output is consistent with
  // HasCopyOutputRequests.
  Surface::CopyRequestsMap requests_map;
  GetSurfaceForId(id2)->TakeCopyOutputRequests(&requests_map);
  EXPECT_TRUE(requests_map.empty());
  GetSurfaceForId(id1)->TakeCopyOutputRequests(&requests_map);
  EXPECT_FALSE(requests_map.empty());
}

TEST_P(CompositorFrameSinkSupportTest,
       OldSurfaceDoesNotTakeCopyOutputRequestsFromNewLocalId) {
  LocalSurfaceId local_surface_id1(1, kArbitraryToken);
  LocalSurfaceId local_surface_id2(2, kArbitraryToken);
  SurfaceId id1(support_->frame_sink_id(), local_surface_id1);
  SurfaceId id2(support_->frame_sink_id(), local_surface_id2);

  // Create the first surface.
  support_->SubmitCompositorFrame(local_surface_id1,
                                  MakeDefaultInteractiveCompositorFrame());

  // Create the second surface.
  support_->SubmitCompositorFrame(local_surface_id2,
                                  MakeDefaultInteractiveCompositorFrame());

  // Send a CopyOutputRequest.
  auto request = std::make_unique<CopyOutputRequest>(
      CopyOutputRequest::ResultFormat::RGBA,
      CopyOutputRequest::ResultDestination::kSystemMemory,
      base::BindOnce(StubResultCallback));
  support_->RequestCopyOfOutput(
      {local_surface_id2, SubtreeCaptureId(), std::move(request)});

  // The first surface doesn't have copy output requests, because it can't
  // satisfy the request that the client has.
  GetSurfaceForId(id1)->TakeCopyOutputRequestsFromClient();
  EXPECT_FALSE(GetSurfaceForId(id1)->HasCopyOutputRequests());
  EXPECT_FALSE(GetSurfaceForId(id2)->HasCopyOutputRequests());

  // Second surface should succeed at taking the requests.
  GetSurfaceForId(id2)->TakeCopyOutputRequestsFromClient();
  EXPECT_FALSE(GetSurfaceForId(id1)->HasCopyOutputRequests());
  EXPECT_TRUE(GetSurfaceForId(id2)->HasCopyOutputRequests());

  // Do TakeCopyOutputRequests, to see if its output is consistent with
  // HasCopyOutputRequests.
  Surface::CopyRequestsMap requests_map;
  GetSurfaceForId(id1)->TakeCopyOutputRequests(&requests_map);
  EXPECT_TRUE(requests_map.empty());
  GetSurfaceForId(id2)->TakeCopyOutputRequests(&requests_map);
  EXPECT_FALSE(requests_map.empty());
}

// Verifies that CopyOutputRequests made at frame sink level are sent to the
// surface that takes them first. In this test this surface is the last
// activated surface.
TEST_P(CompositorFrameSinkSupportTest,
       LastSurfaceTakesCopyOutputRequestsFromClient) {
  LocalSurfaceId local_surface_id1(1, kArbitraryToken);
  LocalSurfaceId local_surface_id2(2, kArbitraryToken);
  SurfaceId id1(support_->frame_sink_id(), local_surface_id1);
  SurfaceId id2(support_->frame_sink_id(), local_surface_id2);

  // Create the first surface.
  support_->SubmitCompositorFrame(local_surface_id1,
                                  MakeDefaultInteractiveCompositorFrame());

  // Send a CopyOutputRequest. Note that the second surface doesn't even exist
  // yet.
  auto request = std::make_unique<CopyOutputRequest>(
      CopyOutputRequest::ResultFormat::RGBA,
      CopyOutputRequest::ResultDestination::kSystemMemory,
      base::BindOnce(StubResultCallback));
  support_->RequestCopyOfOutput(
      {local_surface_id1, SubtreeCaptureId(), std::move(request)});

  // Create the second surface.
  support_->SubmitCompositorFrame(local_surface_id2,
                                  MakeDefaultInteractiveCompositorFrame());

  // Second surface takes CopyOutputRequests from its client. Now only the
  // second surface should report having CopyOutputRequests.
  GetSurfaceForId(id2)->TakeCopyOutputRequestsFromClient();
  EXPECT_FALSE(GetSurfaceForId(id1)->HasCopyOutputRequests());
  EXPECT_TRUE(GetSurfaceForId(id2)->HasCopyOutputRequests());

  // Calling TakeCopyOutputRequestsFromClient() on the first surface should have
  // no effect.
  GetSurfaceForId(id1)->TakeCopyOutputRequestsFromClient();
  EXPECT_FALSE(GetSurfaceForId(id1)->HasCopyOutputRequests());
  EXPECT_TRUE(GetSurfaceForId(id2)->HasCopyOutputRequests());

  // Also try TakeCopyOutputRequests, to see if its output is consistent with
  // HasCopyOutputRequests.
  Surface::CopyRequestsMap requests_map;
  GetSurfaceForId(id1)->TakeCopyOutputRequests(&requests_map);
  EXPECT_TRUE(requests_map.empty());
  GetSurfaceForId(id2)->TakeCopyOutputRequests(&requests_map);
  EXPECT_FALSE(requests_map.empty());
}

// Verifies that OnFrameTokenUpdate is issued after OnFirstSurfaceActivation.
TEST_P(CompositorFrameSinkSupportTest,
       OnFrameTokenUpdateAfterFirstSurfaceActivation) {
  LocalSurfaceId local_surface_id(1, kArbitraryToken);
  auto frame = CompositorFrameBuilder()
                   .AddDefaultRenderPass()
                   .SetSendFrameTokenToEmbedder(true)
                   .SetIsHandlingInteraction(true)
                   .Build();
  uint32_t frame_token = frame.metadata.frame_token;
  ASSERT_NE(frame_token, 0u);

  testing::InSequence sequence;
  EXPECT_CALL(frame_sink_manager_client_, OnFirstSurfaceActivation(_));
  EXPECT_CALL(frame_sink_manager_client_,
              OnFrameTokenChanged(_, frame_token, _));
  support_->SubmitCompositorFrame(local_surface_id, std::move(frame));
}

// Test that `PendingCopyOutputRequest` with `capture_exact_surface_id` set to
// true can only be taken by the `Surface` with the exact same `SurfaceId`
// requested.
TEST_P(CompositorFrameSinkSupportTest,
       OnlyExactSurfaceCanTakeExactOutputRequest) {
  LocalSurfaceId local_surface_id1(1, kArbitraryToken);
  LocalSurfaceId local_surface_id2(2, kArbitraryToken);
  SurfaceId id1(support_->frame_sink_id(), local_surface_id1);
  SurfaceId id2(support_->frame_sink_id(), local_surface_id2);

  // Create Surface1.
  support_->SubmitCompositorFrame(local_surface_id1,
                                  MakeDefaultInteractiveCompositorFrame());

  // Create Surface2.
  support_->SubmitCompositorFrame(local_surface_id2,
                                  MakeDefaultInteractiveCompositorFrame());

  // Send a non-exact CopyOutputRequest. It can be picked up by either Surface1
  // or Surface2.
  support_->RequestCopyOfOutput(
      {local_surface_id1, SubtreeCaptureId(),
       std::make_unique<CopyOutputRequest>(
           CopyOutputRequest::ResultFormat::RGBA,
           CopyOutputRequest::ResultDestination::kSystemMemory,
           base::BindOnce(StubResultCallback))});
  EXPECT_TRUE(surface_observer_->IsSurfaceDamaged(id1));

  // Send an exact CopyOutputRequest for Surface1. It can only be picked up by
  // Surface1.
  support_->RequestCopyOfOutput(
      {local_surface_id1, SubtreeCaptureId(),
       std::make_unique<CopyOutputRequest>(
           CopyOutputRequest::ResultFormat::RGBA,
           CopyOutputRequest::ResultDestination::kSystemMemory,
           base::BindOnce(StubResultCallback)),
       /*capture_exact_id=*/true});
  EXPECT_TRUE(surface_observer_->IsSurfaceDamaged(id2));

  // Surface2 picks up the non-exact CopyOutputRequest.
  GetSurfaceForId(id2)->TakeCopyOutputRequestsFromClient();
  EXPECT_FALSE(GetSurfaceForId(id1)->HasCopyOutputRequests());
  EXPECT_TRUE(GetSurfaceForId(id2)->HasCopyOutputRequests());

  // Surface1 picks up the exact CopyOutputRequest for Surface1.
  GetSurfaceForId(id1)->TakeCopyOutputRequestsFromClient();
  EXPECT_TRUE(GetSurfaceForId(id1)->HasCopyOutputRequests());
  EXPECT_TRUE(GetSurfaceForId(id2)->HasCopyOutputRequests());
}

// Verify that FrameToken is sent to the client if and only if the frame is
// active.
TEST_P(CompositorFrameSinkSupportTest, OnFrameTokenUpdate) {
  LocalSurfaceId child_local_surface_id(1, kAnotherArbitraryToken);
  SurfaceId child_id(kAnotherArbitraryFrameSinkId, child_local_surface_id);

  auto frame = CompositorFrameBuilder()
                   .AddDefaultRenderPass()
                   .SetSendFrameTokenToEmbedder(true)
                   .SetActivationDependencies({child_id})
                   .SetIsHandlingInteraction(true)
                   .Build();
  uint32_t frame_token = frame.metadata.frame_token;
  ASSERT_NE(frame_token, 0u);

  LocalSurfaceId local_surface_id(1, kArbitraryToken);
  support_->SubmitCompositorFrame(local_surface_id, std::move(frame));

  Surface* surface = support_->GetLastCreatedSurfaceForTesting();
  EXPECT_TRUE(surface->has_deadline());
  EXPECT_FALSE(surface->HasActiveFrame());
  EXPECT_TRUE(surface->HasPendingFrame());

  // Since the frame is not activated, |frame_token| is not sent to the client.
  EXPECT_CALL(frame_sink_manager_client_, OnFrameTokenChanged(_, _, _))
      .Times(0);
  testing::Mock::VerifyAndClearExpectations(&frame_sink_manager_client_);

  // Since the frame is now activated, |frame_token| is sent to the client.
  EXPECT_CALL(frame_sink_manager_client_,
              OnFrameTokenChanged(_, frame_token, _));
  surface->ActivatePendingFrameForDeadline();
}

// This test verifies that it is not possible to reuse the same embed token in
// two different frame sinks.
TEST_P(CompositorFrameSinkSupportTest,
       DisallowEmbedTokenReuseAcrossFrameSinks) {
  auto result = support_->MaybeSubmitCompositorFrame(
      local_surface_id_, MakeDefaultInteractiveCompositorFrame(), std::nullopt,
      0, mojom::CompositorFrameSink::SubmitCompositorFrameSyncCallback());
  EXPECT_EQ(SubmitResult::ACCEPTED, result);

  // Create another sink and reuse the same embed token to submit a frame. The
  // frame should be rejected.
  MockCompositorFrameSinkClient mock_client;
  auto support = std::make_unique<CompositorFrameSinkSupport>(
      &mock_client, manager_.get(), kAnotherArbitraryFrameSinkId,
      false /* not root frame sink */);
  LocalSurfaceId local_surface_id(31232, local_surface_id_.embed_token());
  result = support->MaybeSubmitCompositorFrame(
      local_surface_id, MakeDefaultInteractiveCompositorFrame(), std::nullopt,
      0, mojom::CompositorFrameSink::SubmitCompositorFrameSyncCallback());
  EXPECT_EQ(SubmitResult::SURFACE_OWNED_BY_ANOTHER_CLIENT, result);
}

// This test verifies that the parent sequence number of the submitted
// CompositorFrames can decrease as long as the embed token changes as well.
TEST_P(CompositorFrameSinkSupportTest, SubmitAfterReparenting) {
  LocalSurfaceId local_surface_id1(2, base::UnguessableToken::Create());
  LocalSurfaceId local_surface_id2(1, base::UnguessableToken::Create());

  ASSERT_NE(local_surface_id1.embed_token(), local_surface_id2.embed_token());

  CompositorFrame frame = CompositorFrameBuilder()
                              .AddDefaultRenderPass()
                              .SetIsHandlingInteraction(true)
                              .Build();
  SubmitResult result = support_->MaybeSubmitCompositorFrame(
      local_surface_id1, std::move(frame), std::nullopt, 0,
      mojom::CompositorFrameSink::SubmitCompositorFrameSyncCallback());
  EXPECT_EQ(SubmitResult::ACCEPTED, result);

  frame = CompositorFrameBuilder()
              .AddDefaultRenderPass()
              .SetIsHandlingInteraction(true)
              .Build();
  result = support_->MaybeSubmitCompositorFrame(
      local_surface_id2, std::move(frame), std::nullopt, 0,
      mojom::CompositorFrameSink::SubmitCompositorFrameSyncCallback());

  // Even though |local_surface_id2| has a smaller parent sequence number than
  // |local_surface_id1|, the submit should still succeed because it has a
  // different embed token.
  EXPECT_EQ(SubmitResult::ACCEPTED, result);
}

// This test verifies that surfaces created with a new embed token are not
// compared against the evicted parent sequence number of the previous embed
// token.
TEST_P(CompositorFrameSinkSupportTest, EvictThenReparent) {
  LocalSurfaceId local_surface_id1(2, base::UnguessableToken::Create());
  LocalSurfaceId local_surface_id2(1, base::UnguessableToken::Create());

  ASSERT_NE(local_surface_id1.embed_token(), local_surface_id2.embed_token());

  support_->EvictSurface(local_surface_id1);
  CompositorFrame frame = CompositorFrameBuilder()
                              .AddDefaultRenderPass()
                              .SetIsHandlingInteraction(true)
                              .Build();
  support_->SubmitCompositorFrame(local_surface_id2, std::move(frame));
  manager_->surface_manager()->GarbageCollectSurfaces();

  // Even though |local_surface_id2| has a smaller parent sequence number than
  // |local_surface_id1|, it should not be evicted because it has a different
  // embed token.
  EXPECT_TRUE(
      GetSurfaceForId(SurfaceId(support_->frame_sink_id(), local_surface_id2)));
}

// Verifies that invalid hit test region does not get submitted.
TEST_P(CompositorFrameSinkSupportTest, HitTestRegionValidation) {
  constexpr FrameSinkId frame_sink_id(1234, 5678);
  manager_->RegisterFrameSinkId(frame_sink_id, true /* report_activation */);
  auto support = std::make_unique<CompositorFrameSinkSupport>(
      &fake_support_client_, manager_.get(), frame_sink_id, kIsRoot);
  LocalSurfaceId local_surface_id(6, 1, base::UnguessableToken::Create());

  HitTestRegionList hit_test_region_list;

  // kHitTestAsk not set, async_hit_test_reasons not set.
  HitTestRegion hit_test_region_1;
  hit_test_region_1.frame_sink_id = frame_sink_id;
  hit_test_region_1.flags = HitTestRegionFlags::kHitTestMine;
  hit_test_region_1.rect.SetRect(100, 100, 200, 400);

  hit_test_region_list.regions.push_back(std::move(hit_test_region_1));

  EXPECT_EQ(manager_->hit_test_manager()->submit_hit_test_region_list_index(),
            0u);
  support->MaybeSubmitCompositorFrame(
      local_surface_id, MakeDefaultInteractiveCompositorFrame(),
      hit_test_region_list, 0,
      mojom::CompositorFrameSink::SubmitCompositorFrameSyncCallback());
  // hit_test_region_1 is valid. Submitted region count increases.
  EXPECT_EQ(manager_->hit_test_manager()->submit_hit_test_region_list_index(),
            1u);
  hit_test_region_list.regions.clear();

  // kHitTestAsk set, async_hit_test_reasons not set.
  HitTestRegion hit_test_region_2;
  hit_test_region_2.frame_sink_id = frame_sink_id;
  hit_test_region_2.flags = HitTestRegionFlags::kHitTestAsk;
  hit_test_region_2.rect.SetRect(400, 100, 300, 400);

  hit_test_region_list.regions.push_back(std::move(hit_test_region_2));
  EXPECT_EQ(manager_->hit_test_manager()->submit_hit_test_region_list_index(),
            1u);
  support->MaybeSubmitCompositorFrame(
      local_surface_id, MakeDefaultInteractiveCompositorFrame(),
      hit_test_region_list, 0,
      mojom::CompositorFrameSink::SubmitCompositorFrameSyncCallback());
  // hit_test_region_2 is invalid. Submitted region count does not change.
  EXPECT_EQ(manager_->hit_test_manager()->submit_hit_test_region_list_index(),
            1u);

  // kHitTestAsk not set, async_hit_test_reasons set.
  HitTestRegion hit_test_region_3;
  hit_test_region_3.frame_sink_id = frame_sink_id;
  hit_test_region_3.async_hit_test_reasons =
      AsyncHitTestReasons::kOverlappedRegion;
  hit_test_region_3.rect.SetRect(400, 100, 300, 400);

  hit_test_region_list.regions.clear();
  hit_test_region_list.regions.push_back(std::move(hit_test_region_3));
  EXPECT_EQ(manager_->hit_test_manager()->submit_hit_test_region_list_index(),
            1u);
  support->MaybeSubmitCompositorFrame(
      local_surface_id, MakeDefaultInteractiveCompositorFrame(),
      hit_test_region_list, 0,
      mojom::CompositorFrameSink::SubmitCompositorFrameSyncCallback());
  // hit_test_region_3 is invalid. Submitted region count does not change.
  EXPECT_EQ(manager_->hit_test_manager()->submit_hit_test_region_list_index(),
            1u);

  // kHitTestAsk set, async_hit_test_reasons set.
  HitTestRegion hit_test_region_4;
  hit_test_region_4.frame_sink_id = frame_sink_id;
  hit_test_region_4.flags = HitTestRegionFlags::kHitTestAsk;
  hit_test_region_4.async_hit_test_reasons =
      AsyncHitTestReasons::kOverlappedRegion;
  hit_test_region_4.rect.SetRect(400, 100, 300, 400);

  hit_test_region_list.regions.clear();
  hit_test_region_list.regions.push_back(std::move(hit_test_region_4));
  EXPECT_EQ(manager_->hit_test_manager()->submit_hit_test_region_list_index(),
            1u);
  support->MaybeSubmitCompositorFrame(
      local_surface_id, MakeDefaultInteractiveCompositorFrame(),
      hit_test_region_list, 0,
      mojom::CompositorFrameSink::SubmitCompositorFrameSyncCallback());
  // hit_test_region_4 is valid. Submitted region count increases.
  EXPECT_EQ(manager_->hit_test_manager()->submit_hit_test_region_list_index(),
            2u);
}

// Verifies that an unresponsive client has OnBeginFrame() messages throttled
// and then stopped until it becomes responsive again.
TEST_P(CompositorFrameSinkSupportTest, ThrottleUnresponsiveClient) {
  FakeExternalBeginFrameSource begin_frame_source(0.f, false);

  MockCompositorFrameSinkClient mock_client;
  auto support = std::make_unique<CompositorFrameSinkSupport>(
      &mock_client, manager_.get(), kAnotherArbitraryFrameSinkId,
      /*is_root=*/true);
  support->SetBeginFrameSource(&begin_frame_source);
  support->SetNeedsBeginFrame(true);

  constexpr base::TimeDelta interval = BeginFrameArgs::DefaultInterval();
  base::TimeTicks frametime;
  uint64_t sequence_number = 1;
  int sent_frames = 0;
  BeginFrameArgs args;

  // Issue ten OnBeginFrame() messages with no response. They should all be
  // received by the client.
  for (; sent_frames < BeginFrameTracker::kLimitThrottle; ++sent_frames) {
    frametime += interval;

    args = CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0,
                                          sequence_number++, frametime);
    EXPECT_CALL(mock_client, OnBeginFrame(args, _, _, _));
    begin_frame_source.TestOnBeginFrame(args);
    testing::Mock::VerifyAndClearExpectations(&mock_client);
  }

  for (; sent_frames < BeginFrameTracker::kLimitStop; ++sent_frames) {
    base::TimeTicks unthrottle_time = frametime + base::Seconds(1);

    // The client should now be throttled for the next second and won't receive
    // OnBeginFrames().
    frametime += interval;
    args = CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0,
                                          sequence_number++, frametime);
    EXPECT_CALL(mock_client, OnBeginFrame(args, _, _, _)).Times(0);
    begin_frame_source.TestOnBeginFrame(args);
    testing::Mock::VerifyAndClearExpectations(&mock_client);

    frametime = unthrottle_time - base::Microseconds(1);
    args = CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0,
                                          sequence_number++, frametime);
    EXPECT_CALL(mock_client, OnBeginFrame(args, _, _, _)).Times(0);
    begin_frame_source.TestOnBeginFrame(args);
    testing::Mock::VerifyAndClearExpectations(&mock_client);

    // After one second OnBeginFrame() the client should receive OnBeginFrame().
    frametime = unthrottle_time;
    args = CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0,
                                          sequence_number++, frametime);
    EXPECT_CALL(mock_client, OnBeginFrame(args, _, _, _));
    begin_frame_source.TestOnBeginFrame(args);
    testing::Mock::VerifyAndClearExpectations(&mock_client);
  }

  BeginFrameArgs last_sent_args = args;

  // The client should no longer receive OnBeginFrame() until it becomes
  // responsive again.
  frametime += base::Minutes(1);
  args = CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0,
                                        sequence_number++, frametime);
  EXPECT_CALL(mock_client, OnBeginFrame(args, _, _, _)).Times(0);
  begin_frame_source.TestOnBeginFrame(args);
  testing::Mock::VerifyAndClearExpectations(&mock_client);

  // The client becomes responsive again. The next OnBeginFrame() message should
  // be delivered.
  support->DidNotProduceFrame(BeginFrameAck(last_sent_args, false));

  frametime += interval;
  args = CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0,
                                        sequence_number++, frametime);
  EXPECT_CALL(mock_client, OnBeginFrame(args, _, _, _));
  begin_frame_source.TestOnBeginFrame(args);
  testing::Mock::VerifyAndClearExpectations(&mock_client);

  support->SetNeedsBeginFrame(false);
}

// Verifies that when CompositorFrameSinkSupport has its
// |begin_frame_interval_| set, any BeginFrame would be sent only after this
// interval has passed from the time when the last BeginFrame was sent.
TEST_P(CompositorFrameSinkSupportTest, BeginFrameInterval) {
  FakeExternalBeginFrameSource begin_frame_source(0.f, false);

  testing::NiceMock<MockCompositorFrameSinkClient> mock_client;
  auto support = std::make_unique<CompositorFrameSinkSupport>(
      &mock_client, manager_.get(), kAnotherArbitraryFrameSinkId,
      /*is_root=*/true);
  SurfaceId id(kAnotherArbitraryFrameSinkId, local_surface_id_);
  support->SetBeginFrameSource(&begin_frame_source);
  support->SetNeedsBeginFrame(true);
  support->SetLastKnownVsync(BeginFrameArgs::DefaultInterval());

  // Check that non perfect cadence throttle does not apply
  int non_perfect_cadence_fps = BeginFrameArgs::DefaultInterval().ToHz() / 2.5;
  base::TimeDelta non_perfect_throttled_interval =
      base::Seconds(1) / non_perfect_cadence_fps;
  bool did_throttle = support->ThrottleBeginFrame(
      non_perfect_throttled_interval, /*perfect_cadence*/ true);
  EXPECT_FALSE(did_throttle);

  // We only throttle multiples of the refresh rate.
  constexpr int fps = BeginFrameArgs::DefaultInterval().ToHz() / 2;
  constexpr base::TimeDelta throttled_interval = base::Seconds(1) / fps;

  // When no last known vsync exists, perfect cadence cannot be computed, just
  // apply the throttle.
  support->SetLastKnownVsync(base::TimeDelta());
  did_throttle =
      support->ThrottleBeginFrame(throttled_interval, /*perfect_cadence*/ true);
  EXPECT_TRUE(did_throttle);

  support->SetLastKnownVsync(BeginFrameArgs::DefaultInterval());
  did_throttle =
      support->ThrottleBeginFrame(throttled_interval, /*perfect_cadence*/ true);
  EXPECT_TRUE(did_throttle);

  constexpr base::TimeDelta interval = BeginFrameArgs::DefaultInterval();
  const int num_expected_skipped_frames =
      (base::ClampRound<int>(interval.ToHz()) / fps) - 1;
  base::TimeTicks frame_time;
  int sequence_number = 1;
  int sent_frames = 0;
  BeginFrameArgs args;
  int frames_throttled_since_last = 0;
  const base::TimeTicks end_time = frame_time + base::Seconds(2);

  while (frame_time < end_time) {
    args = CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0,
                                          sequence_number++, frame_time);

    BeginFrameArgs expected_args(args);
    expected_args.interval = throttled_interval;
    expected_args.deadline =
        frame_time + throttled_interval -
        BeginFrameArgs::DefaultEstimatedDisplayDrawTime(interval);
    expected_args.frames_throttled_since_last = frames_throttled_since_last;
    bool sent_frame = false;
    ON_CALL(mock_client, OnBeginFrame(_, _, _, _))
        .WillByDefault([&](const BeginFrameArgs& actual_args,
                           const FrameTimingDetailsMap&, bool frame_ack,
                           std::vector<ReturnedResource>) {
          EXPECT_THAT(actual_args, Eq(expected_args));
          support->SubmitCompositorFrame(
              local_surface_id_, MakeDefaultInteractiveCompositorFrame());
          GetSurfaceForId(id)->MarkAsDrawn();
          sent_frame = true;
          // Ack the first submitted frame, as if activation completed.
          // Subsequent frames are not sharing any resources, so the unref
          // process will Ack the frame before activation. If we're always
          // sending ack on activation, this is redundant.
          if (!sent_frames && !ShouldAckOnSurfaceActivationWhenInteractive()) {
            support->SendCompositorFrameAck();
          }
          ++sent_frames;
          if (!frame_time.is_null()) {
            EXPECT_THAT(frames_throttled_since_last,
                        Eq(num_expected_skipped_frames));
          }
          frames_throttled_since_last = 0;
        });

    begin_frame_source.TestOnBeginFrame(args);
    testing::Mock::VerifyAndClearExpectations(&mock_client);

    if (!sent_frame) {
      ++frames_throttled_since_last;
    }
    frame_time += interval;
  }
  // In total fps x 2 seconds + 1 frame at time 0.
  EXPECT_EQ(sent_frames, 2 * fps + 1);
  EXPECT_TRUE(begin_frame_source.AllFramesDidFinish());
  support->SetNeedsBeginFrame(false);
}

TEST_P(CompositorFrameSinkSupportTest, HandlesSmallErrorInBeginFrameTimes) {
  FakeExternalBeginFrameSource begin_frame_source(0.f, false);

  testing::NiceMock<MockCompositorFrameSinkClient> mock_client;
  auto support = std::make_unique<CompositorFrameSinkSupport>(
      &mock_client, manager_.get(), kAnotherArbitraryFrameSinkId,
      /*is_root=*/true);
  SurfaceId id(kAnotherArbitraryFrameSinkId, local_surface_id_);
  support->SetBeginFrameSource(&begin_frame_source);
  support->SetNeedsBeginFrame(true);
  constexpr base::TimeDelta kNativeInterval = BeginFrameArgs::DefaultInterval();
  constexpr base::TimeDelta kThrottledInterval = kNativeInterval * 2;
  support->ThrottleBeginFrame(kThrottledInterval);
  constexpr base::TimeDelta kEpsilon = base::Microseconds(2);

  base::TimeTicks frame_time;
  int sequence_number = 1;

  auto submit_compositor_frame = [&]() {
    support->SubmitCompositorFrame(local_surface_id_,
                                   MakeDefaultInteractiveCompositorFrame());
    GetSurfaceForId(id)->MarkAsDrawn();
  };

  // T: 0 (Should always draw)
  EXPECT_CALL(mock_client, OnBeginFrame(_, _, _, _))
      .WillOnce(submit_compositor_frame);
  begin_frame_source.TestOnBeginFrame(CreateBeginFrameArgsForTesting(
      BEGINFRAME_FROM_HERE, 0, sequence_number++, frame_time));
  testing::Mock::VerifyAndClearExpectations(&mock_client);
  if (!ShouldAckOnSurfaceActivationWhenInteractive()) {
    support->SendCompositorFrameAck();
  }

  // T: 1 native interval
  frame_time += kNativeInterval;
  EXPECT_CALL(mock_client, OnBeginFrame(_, _, _, _)).Times(0);
  begin_frame_source.TestOnBeginFrame(CreateBeginFrameArgsForTesting(
      BEGINFRAME_FROM_HERE, 0, sequence_number++, frame_time));
  testing::Mock::VerifyAndClearExpectations(&mock_client);

  // T: 2 native intervals - epsilon
  frame_time += (kNativeInterval - kEpsilon);
  EXPECT_CALL(mock_client, OnBeginFrame(_, _, _, _))
      .WillOnce(submit_compositor_frame);
  begin_frame_source.TestOnBeginFrame(CreateBeginFrameArgsForTesting(
      BEGINFRAME_FROM_HERE, 0, sequence_number++, frame_time));
  testing::Mock::VerifyAndClearExpectations(&mock_client);

  // T: 3 native intervals
  frame_time += kNativeInterval;
  EXPECT_CALL(mock_client, OnBeginFrame(_, _, _, _)).Times(0);
  begin_frame_source.TestOnBeginFrame(CreateBeginFrameArgsForTesting(
      BEGINFRAME_FROM_HERE, 0, sequence_number++, frame_time));
  testing::Mock::VerifyAndClearExpectations(&mock_client);

  // T: 4 native intervals + epsilon
  frame_time += kNativeInterval + 2 * kEpsilon;
  EXPECT_CALL(mock_client, OnBeginFrame(_, _, _, _))
      .WillOnce(submit_compositor_frame);
  begin_frame_source.TestOnBeginFrame(CreateBeginFrameArgsForTesting(
      BEGINFRAME_FROM_HERE, 0, sequence_number++, frame_time));
  testing::Mock::VerifyAndClearExpectations(&mock_client);

  EXPECT_TRUE(begin_frame_source.AllFramesDidFinish());
  support->SetNeedsBeginFrame(false);
}

TEST_P(CompositorFrameSinkSupportTest,
       UsesThrottledIntervalInPresentationFeedback) {
  static constexpr base::TimeDelta kThrottledFrameInterval = base::Hertz(5);
  // Request BeginFrames.
  support_->SetNeedsBeginFrame(true);
  support_->ThrottleBeginFrame(kThrottledFrameInterval);
  ASSERT_THAT(BeginFrameArgs::DefaultInterval(), Ne(kThrottledFrameInterval));

  base::TimeTicks frame_time = base::TimeTicks::Now();

  // Issue a BeginFrame.
  BeginFrameArgs args =
      CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 1, frame_time);
  begin_frame_source_.TestOnBeginFrame(args);

  // Client submits a compositor frame in response.
  BeginFrameAck ack(args, true);
  CompositorFrame frame = CompositorFrameBuilder()
                              .AddDefaultRenderPass()
                              .SetBeginFrameAck(ack)
                              .SetIsHandlingInteraction(true)
                              .Build();
  auto token = frame.metadata.frame_token;
  support_->SubmitCompositorFrame(local_surface_id_, std::move(frame));
  if (!ShouldAckOnSurfaceActivationWhenInteractive()) {
    support_->SendCompositorFrameAck();
  }

  // The presentation-feedback from the last submitted frame arrives.
  SendPresentationFeedback(support_.get(), token);

  // Issue a new BeginFrame. The frame timing details with submitted
  // presentation feedback should be received with the throttled interval.
  frame_time += kThrottledFrameInterval;
  args = CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 1, 2, frame_time);
  begin_frame_source_.TestOnBeginFrame(args);

  ASSERT_THAT(fake_support_client_.all_frame_timing_details(), SizeIs(1));
  ASSERT_THAT(fake_support_client_.all_frame_timing_details(),
              Contains(Key(token)));
  EXPECT_THAT(fake_support_client_.all_frame_timing_details()
                  .at(token)
                  .presentation_feedback.interval,
              Eq(kThrottledFrameInterval));
}

TEST_P(CompositorFrameSinkSupportTest, ForceFullFrameToActivateSurface) {
  FakeExternalBeginFrameSource begin_frame_source(0.f, false);
  testing::NiceMock<MockCompositorFrameSinkClient> mock_client;
  auto support = std::make_unique<CompositorFrameSinkSupport>(
      &mock_client, manager_.get(), kAnotherArbitraryFrameSinkId,
      /*is_root=*/true);
  SurfaceId id(kAnotherArbitraryFrameSinkId, local_surface_id_);
  support->SetBeginFrameSource(&begin_frame_source);
  support->SetNeedsBeginFrame(true);
  const base::TimeTicks frame_time;
  const int64_t sequence_number = 1;

  // ComposterFrameSink hasn't had a surface activate yet.
  EXPECT_FALSE(support->last_activated_surface_id().is_valid());

  BeginFrameArgs args = CreateBeginFrameArgsForTesting(
      BEGINFRAME_FROM_HERE, 0, sequence_number, frame_time);
  EXPECT_FALSE(args.animate_only);
  BeginFrameArgs args_animate_only = args;
  args_animate_only.animate_only = true;
  // Verify |animate_only| is toggled back to false before sending to client.
  EXPECT_CALL(mock_client,
              OnBeginFrame(testing::Field(&BeginFrameArgs::animate_only,
                                          testing::IsFalse()),
                           _, _, _));
  begin_frame_source.TestOnBeginFrame(args_animate_only);
}

TEST_P(CompositorFrameSinkSupportTest,
       ReleaseTransitionDirectiveClearsFrameSinkManagerEntry) {
  auto result = support_->MaybeSubmitCompositorFrame(
      local_surface_id_, MakeDefaultInteractiveCompositorFrame(), std::nullopt,
      0, mojom::CompositorFrameSink::SubmitCompositorFrameSyncCallback());
  EXPECT_EQ(SubmitResult::ACCEPTED, result);

  blink::ViewTransitionToken transition_token;
  const bool maybe_cross_frame_sink = true;
  Surface* surface = support_->GetLastCreatedSurfaceForTesting();
  ASSERT_TRUE(surface);

  auto test_context_provider = TestContextProvider::CreateRaster();
  gpu::TestSharedImageInterface* sii =
      test_context_provider->SharedImageInterface();
  ReservedResourceIdTracker id_tracker;

  std::unique_ptr<SurfaceAnimationManager> animation_manager =
      SurfaceAnimationManager::CreateWithSave(
          CompositorFrameTransitionDirective::CreateSave(
              transition_token, maybe_cross_frame_sink,
              /*sequence_id=*/1, {}, {}),
          surface, &shared_bitmap_manager_, sii, &id_tracker,
          base::DoNothing());
  ASSERT_TRUE(animation_manager);

  EXPECT_FALSE(HasAnimationManagerForToken(transition_token));
  manager_->CacheSurfaceAnimationManager(transition_token,
                                         std::move(animation_manager));
  EXPECT_TRUE(HasAnimationManagerForToken(transition_token));

  auto release_directive = CompositorFrameTransitionDirective::CreateRelease(
      transition_token, maybe_cross_frame_sink, /*sequence_id=*/2);
  ProcessCompositorFrameTransitionDirective(support_.get(), release_directive,
                                            surface);
  EXPECT_FALSE(HasAnimationManagerForToken(transition_token));
  EXPECT_FALSE(SupportHasSurfaceAnimationManager(support_.get()));
}

TEST_P(CompositorFrameSinkSupportTest, ViewTransitionBlitRequestTextureQuad) {
  gfx::Rect rect(0, 0, 100, 100);
  gfx::Transform transform;

  // Create a root render pass that includes a VT quad.
  auto root_render_pass = CompositorRenderPass::Create();
  CompositorRenderPassId root_id{1};
  root_render_pass->SetNew(root_id, rect, rect, transform);
  SharedQuadState* shared_quad_state =
      root_render_pass->CreateAndAppendSharedQuadState();
  blink::ViewTransitionToken transition_token;
  ViewTransitionElementResourceId resource_id(transition_token, 1);

  auto* vt_quad =
      root_render_pass->CreateAndAppendDrawQuad<SharedElementDrawQuad>();
  vt_quad->SetNew(shared_quad_state, rect, rect, resource_id);

  // Also create an orphaned render pass that will be be referenced by the draw
  // quad.
  auto orphan_render_pass = CompositorRenderPass::Create();
  CompositorRenderPassId orphan_id{2};
  orphan_render_pass->SetNew(orphan_id, rect, rect, transform);
  shared_quad_state = orphan_render_pass->CreateAndAppendSharedQuadState();
  auto* solid_quad =
      orphan_render_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  solid_quad->SetNew(shared_quad_state, rect, rect, SkColors::kBlue, false);

  // Create a frame.
  CompositorRenderPassList render_passes;
  render_passes.push_back(std::move(orphan_render_pass));
  render_passes.push_back(std::move(root_render_pass));
  CompositorFrame frame = MakeCompositorFrame(std::move(render_passes));
  frame.metadata.has_shared_element_resources = true;

  // The shared element references the orphan id.
  CompositorFrameTransitionDirective::SharedElement shared_element;
  shared_element.render_pass_id = orphan_id;
  shared_element.view_transition_element_resource_id = resource_id;

  frame.metadata.transition_directives.push_back(
      CompositorFrameTransitionDirective::CreateSave(
          transition_token,
          /*maybe_cross_frame_sink=*/false,
          /*sequence_id=*/1, {shared_element}, {}));

  // Submit the frame.
  auto result = support_->MaybeSubmitCompositorFrame(
      local_surface_id_, std::move(frame), std::nullopt, 0,
      mojom::CompositorFrameSink::SubmitCompositorFrameSyncCallback());
  EXPECT_EQ(SubmitResult::ACCEPTED, result);

  Surface* surface = support_->GetLastCreatedSurfaceForTesting();
  ASSERT_TRUE(surface);

  const auto& new_frame = surface->GetActiveFrame();
  ASSERT_EQ(new_frame.render_pass_list.size(), 2u);
  ASSERT_EQ(new_frame.render_pass_list[1]->quad_list.size(), 1u);
  auto* quad = *new_frame.render_pass_list[1]->quad_list.begin();
  EXPECT_EQ(quad->material, TextureDrawQuad::kMaterial);
}

TEST_P(CompositorFrameSinkSupportTest,
       GetRequestRegionProperties_NoSurfaceWithActiveFrame) {
  const auto props =
      support_->GetRequestRegionProperties(VideoCaptureSubTarget());
  EXPECT_EQ(std::nullopt, props);
}

TEST_P(CompositorFrameSinkSupportTest,
       GetRequestRegionProperties_SurfaceWithNoCaptureIdentifier) {
  ResourceId first_frame_ids[] = {ResourceId(1), ResourceId(2), ResourceId(3),
                                  ResourceId(4), ResourceId(5)};
  SubmitCompositorFrameWithResources(first_frame_ids,
                                     std::size(first_frame_ids));
  const auto props_with_frame =
      support_->GetRequestRegionProperties(VideoCaptureSubTarget());
  EXPECT_EQ((kDefaultOutputRect), props_with_frame->render_pass_subrect);
  EXPECT_EQ(kDefaultSize, props_with_frame->root_render_pass_size);
  EXPECT_TRUE(props_with_frame->transform_to_root.IsIdentity());
}

TEST_P(CompositorFrameSinkSupportTest,
       GetRequestRegionProperties_RenderPassWithSubtreeSize) {
  constexpr SubtreeCaptureId kSubtreeId(base::Token(0, 22u));
  const SurfaceId surface_id(support_->frame_sink_id(), local_surface_id_);

  auto frame = CompositorFrameBuilder()
                   .AddDefaultRenderPass()
                   .AddDefaultRenderPass()
                   .SetReferencedSurfaces({SurfaceRange(surface_id)})
                   .SetIsHandlingInteraction(true)
                   .Build();
  frame.render_pass_list.front()->subtree_capture_id = kSubtreeId;
  frame.render_pass_list.front()->subtree_size = gfx::Size{13, 17};
  support_->SubmitCompositorFrame(local_surface_id_, std::move(frame));
  // The subtree size should be cropped by the size of the surface (20x20).
  const auto props_with_subtree =
      support_->GetRequestRegionProperties(kSubtreeId);
  EXPECT_EQ((gfx::Rect{0, 0, 13, 17}), props_with_subtree->render_pass_subrect);
  EXPECT_EQ(kDefaultSize, props_with_subtree->root_render_pass_size);
  EXPECT_TRUE(props_with_subtree->transform_to_root.IsIdentity());
}

TEST_P(CompositorFrameSinkSupportTest,
       GetRequestRegionProperties_RenderPassWithNoSubtreeSize) {
  constexpr SubtreeCaptureId kSubtreeId(base::Token(0, 7u));
  const SurfaceId surface_id(support_->frame_sink_id(), local_surface_id_);

  auto frame = CompositorFrameBuilder()
                   .AddDefaultRenderPass()
                   .AddDefaultRenderPass()
                   .SetReferencedSurfaces({SurfaceRange(surface_id)})
                   .SetIsHandlingInteraction(true)
                   .Build();
  frame.render_pass_list.front()->subtree_capture_id = kSubtreeId;
  frame.render_pass_list.front()->output_rect = gfx::Rect{0, 0, 15, 14};
  frame.metadata.capture_bounds =
      RegionCaptureBounds{{{kSubtreeId.subtree_id(), gfx::Rect{5, 6, 15, 14}}}};
  const auto transform = gfx::Transform::MakeTranslation(5.0f, 6.0f);
  frame.render_pass_list.front()->transform_to_root_target = transform;

  // Mark the surface as damaged to update the capture bounds.
  support_->OnSurfaceAggregatedDamage(
      /*surface*/ nullptr, local_surface_id_, frame, kDefaultOutputRect,
      base::TimeTicks::Now());

  support_->SubmitCompositorFrame(local_surface_id_, std::move(frame));
  const auto region_properties =
      support_->GetRequestRegionProperties(kSubtreeId);

  ASSERT_TRUE(region_properties);
  EXPECT_EQ((gfx::Rect{0, 0, 15, 14}), region_properties->render_pass_subrect);
  EXPECT_EQ(kDefaultSize, region_properties->root_render_pass_size);
  EXPECT_EQ(transform, region_properties->transform_to_root);
}

TEST_P(
    CompositorFrameSinkSupportTest,
    GetRequestRegionProperties_RenderPassWithNoSubtreeSizeShouldClipToViewport) {
  constexpr SubtreeCaptureId kSubtreeId(base::Token(0, 7u));
  const SurfaceId surface_id(support_->frame_sink_id(), local_surface_id_);

  auto frame = CompositorFrameBuilder()
                   .AddDefaultRenderPass()
                   .AddDefaultRenderPass()
                   .SetReferencedSurfaces({SurfaceRange(surface_id)})
                   .SetIsHandlingInteraction(true)
                   .Build();
  frame.render_pass_list.front()->subtree_capture_id = kSubtreeId;
  frame.render_pass_list.front()->output_rect = gfx::Rect{0, 0, 15, 14};
  // Same as the output rect to avoid cropping.
  frame.metadata.capture_bounds = RegionCaptureBounds{
      {{kSubtreeId.subtree_id(), gfx::Rect{12, 10, 15, 14}}}};
  const auto transform = gfx::Transform::MakeTranslation(12.0f, 10.0f);
  frame.render_pass_list.front()->transform_to_root_target = transform;

  // Mark the surface as damaged to update the capture bounds.
  support_->OnSurfaceAggregatedDamage(
      /*surface*/ nullptr, local_surface_id_, frame, kDefaultOutputRect,
      base::TimeTicks::Now());

  support_->SubmitCompositorFrame(local_surface_id_, std::move(frame));
  const auto region_properties =
      support_->GetRequestRegionProperties(kSubtreeId);
  ASSERT_TRUE(region_properties);

  // The render pass is partially offscreen and needs to be intersected with
  // the viewport.
  EXPECT_EQ((gfx::Rect{0, 0, 8, 10}), region_properties->render_pass_subrect);
  EXPECT_EQ(kDefaultSize, region_properties->root_render_pass_size);
  EXPECT_EQ(transform, region_properties->transform_to_root);
}

TEST_P(CompositorFrameSinkSupportTest,
       GetRequestRegionProperties_RenderPassWithCaptureBounds) {
  const SurfaceId surface_id(support_->frame_sink_id(), local_surface_id_);
  const auto crop_id = RegionCaptureCropId::CreateRandom();

  auto frame_with_crop_id =
      CompositorFrameBuilder()
          .AddDefaultRenderPass()
          .AddDefaultRenderPass()
          .SetReferencedSurfaces({SurfaceRange(surface_id)})
          .SetIsHandlingInteraction(true)
          .Build();
  frame_with_crop_id.render_pass_list.front()->output_rect = kDefaultOutputRect;
  support_->SubmitCompositorFrame(local_surface_id_,
                                  std::move(frame_with_crop_id));

  // No capture bounds are set, so we shouldn't capture anything.
  const auto props_without_capture_bounds =
      support_->GetRequestRegionProperties(crop_id);
  EXPECT_FALSE(props_without_capture_bounds);

  // After setting capture bounds, we should be able to crop to it.
  auto frame_with_crop_id_and_bounds =
      CompositorFrameBuilder()
          .AddDefaultRenderPass()
          .AddDefaultRenderPass()
          .SetReferencedSurfaces({SurfaceRange(surface_id)})
          .SetIsHandlingInteraction(true)
          .Build();
  frame_with_crop_id_and_bounds.render_pass_list.back()->output_rect =
      kDefaultOutputRect;
  frame_with_crop_id_and_bounds.metadata.capture_bounds =
      RegionCaptureBounds{{{crop_id, gfx::Rect{0, 0, 13, 13}}}};

  // Mark the surface as damaged to update the capture bounds.
  support_->OnSurfaceAggregatedDamage(
      /*surface*/ nullptr, local_surface_id_, frame_with_crop_id_and_bounds,
      kDefaultOutputRect, base::TimeTicks::Now());

  const auto region_properties = support_->GetRequestRegionProperties(crop_id);
  ASSERT_TRUE(region_properties);
  EXPECT_EQ((gfx::Rect{0, 0, 13, 13}), region_properties->render_pass_subrect);
  EXPECT_EQ(kDefaultSize, region_properties->root_render_pass_size);
  EXPECT_TRUE(region_properties->transform_to_root.IsIdentity());
}

INSTANTIATE_TEST_SUITE_P(,
                         CompositorFrameSinkSupportTest,
                         testing::Bool(),
                         &PostTestCaseNameBool);

INSTANTIATE_TEST_SUITE_P(,
                         OnBeginFrameAcksCompositorFrameSinkSupportTest,
                         testing::Combine(testing::Bool(), testing::Bool()),
                         &PostTestCaseNameTuple);
}  // namespace viz
