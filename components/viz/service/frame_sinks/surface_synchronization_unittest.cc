// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/flat_set.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "components/viz/common/features.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "components/viz/service/display_embedder/server_shared_bitmap_manager.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "components/viz/service/surfaces/frame_index_constants.h"
#include "components/viz/service/surfaces/surface.h"
#include "components/viz/service/surfaces/surface_allocation_group.h"
#include "components/viz/test/begin_frame_args_test.h"
#include "components/viz/test/compositor_frame_helpers.h"
#include "components/viz/test/fake_external_begin_frame_source.h"
#include "components/viz/test/fake_surface_observer.h"
#include "components/viz/test/mock_compositor_frame_sink_client.h"
#include "components/viz/test/surface_id_allocator_set.h"
#include "components/viz/test/test_surface_id_allocator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Eq;
using testing::IsEmpty;
using testing::UnorderedElementsAre;

namespace viz {
namespace {

constexpr bool kIsRoot = true;
constexpr bool kIsChildRoot = false;
constexpr FrameSinkId kDisplayFrameSink(2, 0);
constexpr FrameSinkId kParentFrameSink(3, 0);
constexpr FrameSinkId kChildFrameSink1(65563, 0);
constexpr FrameSinkId kChildFrameSink2(65564, 0);
constexpr FrameSinkId kArbitraryFrameSink(1337, 7331);

const uint64_t kBeginFrameSourceId = 1337;

std::vector<SurfaceId> empty_surface_ids() {
  return std::vector<SurfaceId>();
}
std::vector<SurfaceRange> empty_surface_ranges() {
  return std::vector<SurfaceRange>();
}

CompositorFrame MakeCompositorFrame(
    std::vector<SurfaceId> activation_dependencies,
    std::vector<SurfaceRange> referenced_surfaces,
    std::vector<TransferableResource> resource_list,
    const FrameDeadline& deadline = FrameDeadline(),
    bool is_handling_interaction = true) {
  return CompositorFrameBuilder()
      .AddDefaultRenderPass()
      .SetActivationDependencies(std::move(activation_dependencies))
      .SetBeginFrameSourceId(kBeginFrameSourceId)
      .SetReferencedSurfaces(std::move(referenced_surfaces))
      .SetTransferableResources(std::move(resource_list))
      .SetIsHandlingInteraction(is_handling_interaction)
      .SetDeadline(deadline)
      .Build();
}

}  // namespace

class SurfaceSynchronizationTest : public testing::Test {
 public:
  SurfaceSynchronizationTest() = default;
  SurfaceSynchronizationTest(const SurfaceSynchronizationTest&) = delete;
  SurfaceSynchronizationTest& operator=(const SurfaceSynchronizationTest&) =
      delete;

  ~SurfaceSynchronizationTest() override {}

  CompositorFrameSinkSupport& display_support() {
    return *supports_[kDisplayFrameSink];
  }
  Surface* display_surface() {
    return display_support().GetLastCreatedSurfaceForTesting();
  }

  CompositorFrameSinkSupport& parent_support() {
    return *supports_[kParentFrameSink];
  }
  Surface* parent_surface() {
    return parent_support().GetLastCreatedSurfaceForTesting();
  }

  CompositorFrameSinkSupport& child_support1() {
    return *supports_[kChildFrameSink1];
  }
  Surface* child_surface1() {
    return child_support1().GetLastCreatedSurfaceForTesting();
  }

  CompositorFrameSinkSupport& child_support2() {
    return *supports_[kChildFrameSink2];
  }
  Surface* child_surface2() {
    return child_support2().GetLastCreatedSurfaceForTesting();
  }

  void CreateFrameSink(const FrameSinkId& frame_sink_id, bool is_root) {
    supports_[frame_sink_id] = std::make_unique<CompositorFrameSinkSupport>(
        &support_client_, frame_sink_manager_.get(), frame_sink_id, is_root);
  }

  void DestroyFrameSink(const FrameSinkId& frame_sink_id) {
    auto it = supports_.find(frame_sink_id);
    if (it == supports_.end())
      return;
    supports_.erase(it);
  }

  // Returns all the references where |surface_id| is the parent.
  const base::flat_set<SurfaceId>& GetReferencesFrom(
      const SurfaceId& surface_id) {
    return frame_sink_manager()
        .surface_manager()
        ->GetSurfacesReferencedByParent(surface_id);
  }

  FrameSinkManagerImpl& frame_sink_manager() { return *frame_sink_manager_; }
  SurfaceManager* surface_manager() {
    return frame_sink_manager_->surface_manager();
  }

  void ExpireAllTemporaryReferencesAndGarbageCollect() {
    surface_manager()->ExpireOldTemporaryReferences();
    surface_manager()->ExpireOldTemporaryReferences();
    surface_manager()->GarbageCollectSurfaces();
  }

  // Returns all the references where |surface_id| is the parent.
  const base::flat_set<SurfaceId>& GetChildReferences(
      const SurfaceId& surface_id) {
    return frame_sink_manager()
        .surface_manager()
        ->GetSurfacesReferencedByParent(surface_id);
  }

  // Returns true if there is a temporary reference for |surface_id|.
  bool HasTemporaryReference(const SurfaceId& surface_id) {
    return frame_sink_manager().surface_manager()->HasTemporaryReference(
        surface_id);
  }

  Surface* GetLatestInFlightSurface(const SurfaceRange& surface_range) {
    return frame_sink_manager().surface_manager()->GetLatestInFlightSurface(
        surface_range);
  }

  FakeExternalBeginFrameSource* begin_frame_source() {
    return begin_frame_source_.get();
  }

  base::TimeTicks Now() { return now_src_->NowTicks(); }

  FrameDeadline MakeDefaultDeadline() {
    return FrameDeadline(Now(), 4u, BeginFrameArgs::DefaultInterval(), false);
  }

  FrameDeadline MakeDeadline(uint32_t deadline_in_frames) {
    return FrameDeadline(Now(), deadline_in_frames,
                         BeginFrameArgs::DefaultInterval(), false);
  }

  void SendNextBeginFrame() {
    // Creep the time forward so that any BeginFrameArgs is not equal to the
    // last one otherwise we violate the BeginFrameSource contract.
    now_src_->Advance(BeginFrameArgs::DefaultInterval());
    BeginFrameArgs args = begin_frame_source_->CreateBeginFrameArgs(
        BEGINFRAME_FROM_HERE, now_src_.get());
    begin_frame_source_->TestOnBeginFrame(args);
  }

  void SendLateBeginFrame() {
    // Creep the time forward so that any BeginFrameArgs is not equal to the
    // last one otherwise we violate the BeginFrameSource contract.
    now_src_->Advance(4u * BeginFrameArgs::DefaultInterval());
    BeginFrameArgs args = begin_frame_source_->CreateBeginFrameArgs(
        BEGINFRAME_FROM_HERE, now_src_.get());
    begin_frame_source_->TestOnBeginFrame(args);
  }

  FakeSurfaceObserver& surface_observer() { return *surface_observer_; }

  // testing::Test:
  void SetUp() override {
    testing::Test::SetUp();
    frame_sink_manager_ = std::make_unique<FrameSinkManagerImpl>(
        FrameSinkManagerImpl::InitParams(&shared_bitmap_manager_));
    surface_observer_ =
        std::make_unique<FakeSurfaceObserver>(surface_manager(), false);
    begin_frame_source_ =
        std::make_unique<FakeExternalBeginFrameSource>(0.f, false);
    now_src_ = std::make_unique<base::SimpleTestTickClock>();
    surface_manager()->SetTickClockForTesting(now_src_.get());

    supports_[kDisplayFrameSink] = std::make_unique<CompositorFrameSinkSupport>(
        &support_client_, frame_sink_manager_.get(), kDisplayFrameSink,
        kIsRoot);

    supports_[kParentFrameSink] = std::make_unique<CompositorFrameSinkSupport>(
        &support_client_, frame_sink_manager_.get(), kParentFrameSink,
        kIsChildRoot);

    supports_[kChildFrameSink1] = std::make_unique<CompositorFrameSinkSupport>(
        &support_client_, frame_sink_manager_.get(), kChildFrameSink1,
        kIsChildRoot);

    supports_[kChildFrameSink2] = std::make_unique<CompositorFrameSinkSupport>(
        &support_client_, frame_sink_manager_.get(), kChildFrameSink2,
        kIsChildRoot);

    // Normally, the BeginFrameSource would be registered by the Display. We
    // register it here so that BeginFrames are received by the display support,
    // for use in the PassesOnBeginFrameAcks test. Other supports do not receive
    // BeginFrames, since the frame sink hierarchy is not set up in this test.
    frame_sink_manager().RegisterBeginFrameSource(begin_frame_source_.get(),
                                                  kDisplayFrameSink);
    frame_sink_manager().RegisterFrameSinkHierarchy(kDisplayFrameSink,
                                                    kParentFrameSink);
    frame_sink_manager().RegisterFrameSinkHierarchy(kDisplayFrameSink,
                                                    kChildFrameSink1);
    frame_sink_manager().RegisterFrameSinkHierarchy(kDisplayFrameSink,
                                                    kChildFrameSink2);
  }

  void TearDown() override {
    frame_sink_manager_->UnregisterBeginFrameSource(begin_frame_source_.get());

    begin_frame_source_->SetClient(nullptr);
    begin_frame_source_.reset();

    supports_.clear();

    surface_observer_->Reset();
    surface_observer_.reset();
    frame_sink_manager_.reset();
  }

  bool IsMarkedForDestruction(const SurfaceId& surface_id) {
    return surface_manager()->IsMarkedForDestruction(surface_id);
  }

  Surface* GetSurfaceForId(const SurfaceId& surface_id) {
    return surface_manager()->GetSurfaceForId(surface_id);
  }

  SurfaceId MakeSurfaceId(const FrameSinkId& frame_sink_id,
                          uint32_t parent_sequence_number,
                          uint32_t child_sequence_number = 1u) {
    return allocator_set_.MakeSurfaceId(frame_sink_id, parent_sequence_number,
                                        child_sequence_number);
  }

  bool allocation_groups_need_garbage_collection() {
    return surface_manager()->allocation_groups_need_garbage_collection_;
  }

 protected:
  testing::NiceMock<MockCompositorFrameSinkClient> support_client_;

 private:
  std::unique_ptr<base::SimpleTestTickClock> now_src_;
  ServerSharedBitmapManager shared_bitmap_manager_;
  std::unique_ptr<FrameSinkManagerImpl> frame_sink_manager_;
  std::unique_ptr<FakeSurfaceObserver> surface_observer_;
  std::unique_ptr<FakeExternalBeginFrameSource> begin_frame_source_;
  std::unordered_map<FrameSinkId,
                     std::unique_ptr<CompositorFrameSinkSupport>,
                     FrameSinkIdHash>
      supports_;
  SurfaceIdAllocatorSet allocator_set_;
};

// The display root surface should have a surface reference from the top-level
// root added/removed when a CompositorFrame is submitted with a new
// SurfaceId.
TEST_F(SurfaceSynchronizationTest, RootSurfaceReceivesReferences) {
  const SurfaceId display_id_first = MakeSurfaceId(kDisplayFrameSink, 1);
  const SurfaceId display_id_second = MakeSurfaceId(kDisplayFrameSink, 2);

  // Submit a CompositorFrame for the first display root surface.
  display_support().SubmitCompositorFrame(
      display_id_first.local_surface_id(),
      MakeDefaultInteractiveCompositorFrame(kBeginFrameSourceId));

  // A surface reference from the top-level root is added and there shouldn't be
  // a temporary reference.
  EXPECT_FALSE(HasTemporaryReference(display_id_first));
  EXPECT_THAT(GetChildReferences(
                  frame_sink_manager().surface_manager()->GetRootSurfaceId()),
              UnorderedElementsAre(display_id_first));

  // Submit a CompositorFrame for the second display root surface.
  display_support().SubmitCompositorFrame(
      display_id_second.local_surface_id(),
      MakeDefaultInteractiveCompositorFrame(kBeginFrameSourceId));

  // A surface reference from the top-level root to |display_id_second| should
  // be added and the reference to |display_root_first| removed.
  EXPECT_FALSE(HasTemporaryReference(display_id_second));
  EXPECT_THAT(GetChildReferences(
                  frame_sink_manager().surface_manager()->GetRootSurfaceId()),
              UnorderedElementsAre(display_id_second));

  frame_sink_manager().surface_manager()->GarbageCollectSurfaces();

  // Surface |display_id_first| is unreachable and should get deleted.
  EXPECT_EQ(nullptr, GetSurfaceForId(display_id_first));
}

// The parent Surface is blocked on |child_id1| and |child_id2|.
TEST_F(SurfaceSynchronizationTest, BlockedOnTwo) {
  const SurfaceId parent_id = MakeSurfaceId(kParentFrameSink, 1);
  const SurfaceId child_id1 = MakeSurfaceId(kChildFrameSink1, 1);
  const SurfaceId child_id2 = MakeSurfaceId(kChildFrameSink2, 1);

  parent_support().SubmitCompositorFrame(
      parent_id.local_surface_id(),
      MakeCompositorFrame({child_id1, child_id2}, empty_surface_ranges(),
                          std::vector<TransferableResource>()));

  // parent_support is blocked on |child_id1| and |child_id2|.
  EXPECT_TRUE(parent_surface()->has_deadline());
  EXPECT_FALSE(parent_surface()->HasActiveFrame());
  EXPECT_TRUE(parent_surface()->HasPendingFrame());
  EXPECT_THAT(parent_surface()->activation_dependencies(),
              UnorderedElementsAre(child_id1, child_id2));

  // Submit a CompositorFrame without any dependencies to |child_id1|.
  // parent_support should now only be blocked on |child_id2|.
  child_support1().SubmitCompositorFrame(
      child_id1.local_surface_id(),
      MakeDefaultInteractiveCompositorFrame(kBeginFrameSourceId));

  EXPECT_TRUE(parent_surface()->has_deadline());
  EXPECT_FALSE(parent_surface()->HasActiveFrame());
  EXPECT_TRUE(parent_surface()->HasPendingFrame());
  EXPECT_THAT(parent_surface()->activation_dependencies(),
              UnorderedElementsAre(child_id2));

  // Submit a CompositorFrame without any dependencies to |child_id2|.
  // parent_support should be activated.
  child_support2().SubmitCompositorFrame(
      child_id2.local_surface_id(),
      MakeDefaultInteractiveCompositorFrame(kBeginFrameSourceId));

  EXPECT_FALSE(child_surface2()->has_deadline());
  EXPECT_TRUE(parent_surface()->HasActiveFrame());
  EXPECT_FALSE(parent_surface()->HasPendingFrame());
  EXPECT_THAT(parent_surface()->activation_dependencies(), IsEmpty());
}

// The parent Surface is blocked on |child_id2| which is blocked on
// |child_id3|.
TEST_F(SurfaceSynchronizationTest, BlockedChain) {
  const SurfaceId parent_id = MakeSurfaceId(kParentFrameSink, 1);
  const SurfaceId child_id1 = MakeSurfaceId(kChildFrameSink1, 1);
  const SurfaceId child_id2 = MakeSurfaceId(kChildFrameSink2, 1);

  parent_support().SubmitCompositorFrame(
      parent_id.local_surface_id(),
      MakeCompositorFrame({child_id1}, empty_surface_ranges(),
                          std::vector<TransferableResource>()));

  // parent_support is blocked on |child_id1|.
  EXPECT_TRUE(parent_surface()->has_deadline());
  EXPECT_FALSE(parent_surface()->HasActiveFrame());
  EXPECT_TRUE(parent_surface()->HasPendingFrame());
  EXPECT_THAT(parent_surface()->activation_dependencies(),
              UnorderedElementsAre(child_id1));
  // The parent should not report damage until it activates.
  EXPECT_FALSE(surface_observer().IsSurfaceDamaged(parent_id));

  child_support1().SubmitCompositorFrame(
      child_id1.local_surface_id(),
      MakeCompositorFrame({child_id2}, empty_surface_ranges(),
                          std::vector<TransferableResource>()));

  // child_support1 should now be blocked on |child_id2|.
  EXPECT_TRUE(child_surface1()->has_deadline());
  EXPECT_FALSE(child_surface1()->HasActiveFrame());
  EXPECT_TRUE(child_surface1()->HasPendingFrame());
  EXPECT_THAT(child_surface1()->activation_dependencies(),
              UnorderedElementsAre(child_id2));
  // The parent and child should not report damage until they activate.
  EXPECT_FALSE(surface_observer().IsSurfaceDamaged(parent_id));
  EXPECT_FALSE(surface_observer().IsSurfaceDamaged(child_id1));

  // The parent should still be blocked on |child_id1| because it's pending.
  EXPECT_THAT(parent_surface()->activation_dependencies(),
              UnorderedElementsAre(child_id1));

  // Submit a CompositorFrame without any dependencies to |child_id2|.
  // parent_support should be activated.
  child_support2().SubmitCompositorFrame(
      child_id2.local_surface_id(),
      MakeCompositorFrame(empty_surface_ids(), empty_surface_ranges(),
                          std::vector<TransferableResource>()));

  EXPECT_FALSE(child_surface2()->has_deadline());

  // child_surface1 should now be active.
  EXPECT_TRUE(child_surface1()->HasActiveFrame());
  EXPECT_FALSE(child_surface1()->HasPendingFrame());
  EXPECT_THAT(child_surface1()->activation_dependencies(), IsEmpty());

  // parent_surface should now be active.
  EXPECT_TRUE(parent_surface()->HasActiveFrame());
  EXPECT_FALSE(parent_surface()->HasPendingFrame());
  EXPECT_THAT(parent_surface()->activation_dependencies(), IsEmpty());

  // All three surfaces |parent_id|, |child_id1|, and |child_id2| should
  // now report damage. This would trigger a new display frame.
  EXPECT_TRUE(surface_observer().IsSurfaceDamaged(parent_id));
  EXPECT_TRUE(surface_observer().IsSurfaceDamaged(child_id1));
  EXPECT_TRUE(surface_observer().IsSurfaceDamaged(child_id2));
}

// parent_surface and child_surface1 are blocked on |child_id2|.
TEST_F(SurfaceSynchronizationTest, TwoBlockedOnOne) {
  const SurfaceId parent_id = MakeSurfaceId(kParentFrameSink, 1);
  const SurfaceId child_id1 = MakeSurfaceId(kChildFrameSink1, 1);
  const SurfaceId child_id2 = MakeSurfaceId(kChildFrameSink2, 1);

  parent_support().SubmitCompositorFrame(
      parent_id.local_surface_id(),
      MakeCompositorFrame({child_id2}, {SurfaceRange(std::nullopt, child_id2)},
                          std::vector<TransferableResource>()));

  // parent_support is blocked on |child_id2|.
  EXPECT_TRUE(parent_surface()->has_deadline());
  EXPECT_FALSE(parent_surface()->HasActiveFrame());
  EXPECT_TRUE(parent_surface()->HasPendingFrame());
  EXPECT_THAT(parent_surface()->activation_dependencies(),
              UnorderedElementsAre(child_id2));

  // child_support1 should now be blocked on |child_id2|.
  child_support1().SubmitCompositorFrame(
      child_id1.local_surface_id(),
      MakeCompositorFrame({child_id2}, {SurfaceRange(std::nullopt, child_id2)},
                          std::vector<TransferableResource>()));

  EXPECT_TRUE(child_surface1()->has_deadline());
  EXPECT_FALSE(child_surface1()->HasActiveFrame());
  EXPECT_TRUE(child_surface1()->HasPendingFrame());
  EXPECT_THAT(child_surface1()->activation_dependencies(),
              UnorderedElementsAre(child_id2));

  // The parent should still be blocked on |child_id2|.
  EXPECT_THAT(parent_surface()->activation_dependencies(),
              UnorderedElementsAre(child_id2));

  // Submit a CompositorFrame without any dependencies to |child_id2|.
  // parent_support should be activated.
  child_support2().SubmitCompositorFrame(
      child_id2.local_surface_id(),
      MakeDefaultInteractiveCompositorFrame(kBeginFrameSourceId));

  EXPECT_FALSE(child_surface2()->has_deadline());

  // child_surface1 should now be active.
  EXPECT_TRUE(child_surface1()->HasActiveFrame());
  EXPECT_FALSE(child_surface1()->HasPendingFrame());
  EXPECT_THAT(child_surface1()->activation_dependencies(), IsEmpty());

  // parent_surface should now be active.
  EXPECT_TRUE(parent_surface()->HasActiveFrame());
  EXPECT_FALSE(parent_surface()->HasPendingFrame());
  EXPECT_THAT(parent_surface()->activation_dependencies(), IsEmpty());
}

// parent_surface is blocked on |child_id1|, and child_surface2 is blocked on
// |child_id2| until the deadline hits.
TEST_F(SurfaceSynchronizationTest, DeadlineHits) {
  const SurfaceId parent_id = MakeSurfaceId(kParentFrameSink, 1);
  const SurfaceId child_id1 = MakeSurfaceId(kChildFrameSink1, 1);
  const SurfaceId child_id2 = MakeSurfaceId(kChildFrameSink2, 1);

  parent_support().SubmitCompositorFrame(
      parent_id.local_surface_id(),
      MakeCompositorFrame({child_id1}, empty_surface_ranges(),
                          std::vector<TransferableResource>(),
                          MakeDefaultDeadline()));

  // parent_support is blocked on |child_id1|.
  EXPECT_TRUE(parent_surface()->has_deadline());
  EXPECT_FALSE(parent_surface()->HasActiveFrame());
  EXPECT_TRUE(parent_surface()->HasPendingFrame());
  EXPECT_THAT(parent_surface()->activation_dependencies(),
              UnorderedElementsAre(child_id1));

  child_support1().SubmitCompositorFrame(
      child_id1.local_surface_id(),
      MakeCompositorFrame({child_id2}, empty_surface_ranges(),
                          std::vector<TransferableResource>(),
                          MakeDefaultDeadline()));

  // child_support1 should now be blocked on |child_id2|.
  EXPECT_TRUE(child_surface1()->has_deadline());
  EXPECT_FALSE(child_surface1()->HasActiveFrame());
  EXPECT_TRUE(child_surface1()->HasPendingFrame());
  EXPECT_THAT(child_surface1()->activation_dependencies(),
              UnorderedElementsAre(child_id2));

  // The parent should still be blocked on |child_id1| because it's pending.
  EXPECT_THAT(parent_surface()->activation_dependencies(),
              UnorderedElementsAre(child_id1));

  for (int i = 0; i < 3; ++i) {
    SendNextBeginFrame();
    // There is still a looming deadline! Eeek!
    EXPECT_TRUE(parent_surface()->has_deadline());

    // parent_support is still blocked on |child_id1|.
    EXPECT_FALSE(parent_surface()->HasActiveFrame());
    EXPECT_TRUE(parent_surface()->HasPendingFrame());
    EXPECT_THAT(parent_surface()->activation_dependencies(),
                UnorderedElementsAre(child_id1));

    // child_support1 is still blocked on |child_id2|.
    EXPECT_FALSE(child_surface1()->HasActiveFrame());
    EXPECT_TRUE(child_surface1()->HasPendingFrame());
    EXPECT_THAT(child_surface1()->activation_dependencies(),
                UnorderedElementsAre(child_id2));
  }

  SendNextBeginFrame();

  // The deadline has passed.
  EXPECT_FALSE(parent_surface()->has_deadline());

  // parent_surface has been activated.
  EXPECT_TRUE(parent_surface()->HasActiveFrame());
  EXPECT_FALSE(parent_surface()->HasPendingFrame());
  EXPECT_THAT(parent_surface()->activation_dependencies(), IsEmpty());

  // child_surface1 has been activated.
  EXPECT_TRUE(child_surface1()->HasActiveFrame());
  EXPECT_FALSE(child_surface1()->HasPendingFrame());
  EXPECT_THAT(child_surface1()->activation_dependencies(), IsEmpty());
}

// This test verifies that unlimited deadline mode works and that surfaces will
// not activate until dependencies are resolved.
TEST_F(SurfaceSynchronizationTest, UnlimitedDeadline) {
  // Turn on unlimited deadline mode.
  frame_sink_manager()
      .surface_manager()
      ->SetActivationDeadlineInFramesForTesting(std::nullopt);

  const SurfaceId parent_id = MakeSurfaceId(kParentFrameSink, 1);
  const SurfaceId child_id1 = MakeSurfaceId(kChildFrameSink1, 1);

  // The deadline specified by the parent is ignored in unlimited deadline
  // mode.
  parent_support().SubmitCompositorFrame(
      parent_id.local_surface_id(),
      MakeCompositorFrame({child_id1}, empty_surface_ranges(),
                          std::vector<TransferableResource>(),
                          MakeDefaultDeadline()));

  // parent_support is blocked on |child_id1|.
  EXPECT_FALSE(parent_surface()->HasActiveFrame());
  EXPECT_TRUE(parent_surface()->HasPendingFrame());
  EXPECT_THAT(parent_surface()->activation_dependencies(),
              UnorderedElementsAre(child_id1));

  for (int i = 0; i < 4; ++i) {
    SendNextBeginFrame();
    // parent_support is still blocked on |child_id1|.
    EXPECT_FALSE(parent_surface()->HasActiveFrame());
    EXPECT_TRUE(parent_surface()->HasPendingFrame());
    EXPECT_THAT(parent_surface()->activation_dependencies(),
                UnorderedElementsAre(child_id1));
  }

  // parent_support is STILL blocked on |child_id1|.
  EXPECT_FALSE(parent_surface()->HasActiveFrame());
  EXPECT_TRUE(parent_surface()->HasPendingFrame());
  EXPECT_THAT(parent_surface()->activation_dependencies(),
              UnorderedElementsAre(child_id1));

  child_support1().SubmitCompositorFrame(
      child_id1.local_surface_id(),
      MakeDefaultInteractiveCompositorFrame(kBeginFrameSourceId));

  // parent_surface has been activated.
  EXPECT_TRUE(parent_surface()->HasActiveFrame());
  EXPECT_FALSE(parent_surface()->HasPendingFrame());
  EXPECT_THAT(parent_surface()->activation_dependencies(), IsEmpty());
}

// parent_surface is blocked on |child_id1| until a late BeginFrame arrives and
// triggers a deadline.
TEST_F(SurfaceSynchronizationTest, LateBeginFrameTriggersDeadline) {
  const SurfaceId parent_id = MakeSurfaceId(kParentFrameSink, 1);
  const SurfaceId child_id1 = MakeSurfaceId(kChildFrameSink1, 1);

  parent_support().SubmitCompositorFrame(
      parent_id.local_surface_id(),
      MakeCompositorFrame({child_id1}, empty_surface_ranges(),
                          std::vector<TransferableResource>()));

  // parent_support is blocked on |child_id1|.
  EXPECT_TRUE(parent_surface()->has_deadline());
  EXPECT_FALSE(parent_surface()->HasActiveFrame());
  EXPECT_TRUE(parent_surface()->HasPendingFrame());
  EXPECT_THAT(parent_surface()->activation_dependencies(),
              UnorderedElementsAre(child_id1));

  SendLateBeginFrame();

  // The deadline has passed.
  EXPECT_FALSE(parent_surface()->has_deadline());

  // parent_surface has been activated.
  EXPECT_TRUE(parent_surface()->HasActiveFrame());
  EXPECT_FALSE(parent_surface()->HasPendingFrame());
  EXPECT_THAT(parent_surface()->activation_dependencies(), IsEmpty());
}

// This test verifies at the Surface activates once a CompositorFrame is
// submitted that has no unresolved dependencies.
TEST_F(SurfaceSynchronizationTest, NewFrameOverridesOldDependencies) {
  const SurfaceId parent_id = MakeSurfaceId(kParentFrameSink, 1);
  const SurfaceId arbitrary_id = MakeSurfaceId(kArbitraryFrameSink, 1);

  // Submit a CompositorFrame that depends on |arbitrary_id|.
  parent_support().SubmitCompositorFrame(
      parent_id.local_surface_id(),
      MakeCompositorFrame({arbitrary_id}, empty_surface_ranges(),
                          std::vector<TransferableResource>()));

  // Verify that the CompositorFrame is blocked on |arbitrary_id|.
  EXPECT_FALSE(parent_surface()->HasActiveFrame());
  EXPECT_TRUE(parent_surface()->HasPendingFrame());
  EXPECT_THAT(parent_surface()->activation_dependencies(),
              UnorderedElementsAre(arbitrary_id));

  // Submit a CompositorFrame that has no dependencies.
  parent_support().SubmitCompositorFrame(
      parent_id.local_surface_id(),
      MakeDefaultInteractiveCompositorFrame(kBeginFrameSourceId));

  // Verify that the CompositorFrame has been activated.
  EXPECT_TRUE(parent_surface()->HasActiveFrame());
  EXPECT_FALSE(parent_surface()->HasPendingFrame());
  EXPECT_THAT(parent_surface()->activation_dependencies(), IsEmpty());
}

// Supports testing features::OnBeginFrameAcks, which changes the expectations
// of what IPCs are sent to the CompositorFrameSinkClient. When enabled
// OnBeginFrame also handles ReturnResources as well as
// DidReceiveCompositorFrameAck.
class OnBeginFrameAcksSurfaceSynchronizationTest
    : public SurfaceSynchronizationTest,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  OnBeginFrameAcksSurfaceSynchronizationTest();
  ~OnBeginFrameAcksSurfaceSynchronizationTest() override = default;

  bool BeginFrameAcksEnabled() const { return std::get<0>(GetParam()); }
  bool AutoNeedsBeginFrame() const { return std::get<1>(GetParam()); }

  // SurfaceSynchronizationTest:
  void SetUp() override;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

OnBeginFrameAcksSurfaceSynchronizationTest::
    OnBeginFrameAcksSurfaceSynchronizationTest() {
  if (BeginFrameAcksEnabled()) {
    scoped_feature_list_.InitAndEnableFeature(features::kOnBeginFrameAcks);
  } else {
    scoped_feature_list_.InitAndDisableFeature(features::kOnBeginFrameAcks);
  }
}

void OnBeginFrameAcksSurfaceSynchronizationTest::SetUp() {
  SurfaceSynchronizationTest::SetUp();
  if (BeginFrameAcksEnabled()) {
    parent_support().SetWantsBeginFrameAcks();
    child_support1().SetWantsBeginFrameAcks();
    child_support2().SetWantsBeginFrameAcks();
  }
  if (AutoNeedsBeginFrame()) {
    parent_support().SetAutoNeedsBeginFrame();
    child_support1().SetAutoNeedsBeginFrame();
    child_support2().SetAutoNeedsBeginFrame();
  }
}

// This test verifies that a pending CompositorFrame does not affect surface
// references. A new surface from a child will continue to exist as a temporary
// reference until the parent's frame activates.
TEST_P(OnBeginFrameAcksSurfaceSynchronizationTest,
       OnlyActiveFramesAffectSurfaceReferences) {
  const SurfaceId parent_id = MakeSurfaceId(kParentFrameSink, 1);
  const SurfaceId child_id1 = MakeSurfaceId(kChildFrameSink1, 1);
  const SurfaceId child_id2 = MakeSurfaceId(kChildFrameSink2, 1);

  // child_support1 submits a CompositorFrame without any dependencies.
  // DidReceiveCompositorFrameAck should call on immediate activation.
  EXPECT_CALL(support_client_, DidReceiveCompositorFrameAck(_))
      .Times(BeginFrameAcksEnabled() ? 0 : 1);
  child_support1().SubmitCompositorFrame(
      child_id1.local_surface_id(),
      MakeDefaultInteractiveCompositorFrame(kBeginFrameSourceId));
  testing::Mock::VerifyAndClearExpectations(&support_client_);

  // Verify that the child surface is not blocked.
  EXPECT_TRUE(child_surface1()->HasActiveFrame());
  EXPECT_FALSE(child_surface1()->HasPendingFrame());
  EXPECT_THAT(child_surface1()->activation_dependencies(), IsEmpty());

  // Verify that there's a temporary reference for |child_id1|.
  EXPECT_TRUE(HasTemporaryReference(child_id1));

  // parent_support submits a CompositorFrame that depends on |child_id1|
  // (which is already active) and |child_id2|. Thus, the parent should not
  // activate immediately. DidReceiveCompositorFrameAck should not be called
  // immediately because the parent CompositorFrame is also blocked on
  // |child_id2|.
  EXPECT_CALL(support_client_, DidReceiveCompositorFrameAck(_)).Times(0);
  parent_support().SubmitCompositorFrame(
      parent_id.local_surface_id(),
      MakeCompositorFrame({child_id2}, {SurfaceRange(child_id1)},
                          std::vector<TransferableResource>()));
  EXPECT_FALSE(parent_surface()->HasActiveFrame());
  EXPECT_TRUE(parent_surface()->HasPendingFrame());
  EXPECT_THAT(parent_surface()->activation_dependencies(),
              UnorderedElementsAre(child_id2));
  EXPECT_THAT(GetChildReferences(parent_id), IsEmpty());
  testing::Mock::VerifyAndClearExpectations(&support_client_);

  // Verify that there's a temporary reference for |child_id1| that still
  // exists.
  EXPECT_TRUE(HasTemporaryReference(child_id1));

  // child_support2 submits a CompositorFrame without any dependencies.
  // Both the child and the parent should immediately ACK CompositorFrames
  // on activation.
  EXPECT_CALL(support_client_, DidReceiveCompositorFrameAck(_))
      .Times(BeginFrameAcksEnabled() ? 0 : 2);
  child_support2().SubmitCompositorFrame(
      child_id2.local_surface_id(),
      MakeDefaultInteractiveCompositorFrame(kBeginFrameSourceId));
  testing::Mock::VerifyAndClearExpectations(&support_client_);

  // Verify that the child surface is not blocked.
  EXPECT_TRUE(child_surface1()->HasActiveFrame());
  EXPECT_FALSE(child_surface1()->HasPendingFrame());
  EXPECT_THAT(child_surface1()->activation_dependencies(), IsEmpty());

  // Verify that the parent surface's CompositorFrame has activated and that
  // the temporary reference has been replaced by a permanent one.
  EXPECT_TRUE(parent_surface()->HasActiveFrame());
  EXPECT_FALSE(parent_surface()->HasPendingFrame());
  EXPECT_THAT(parent_surface()->activation_dependencies(), IsEmpty());
  EXPECT_FALSE(HasTemporaryReference(child_id1));
  EXPECT_THAT(GetChildReferences(parent_id), UnorderedElementsAre(child_id1));
}

// This test verifies that we do not double count returned resources when a
// CompositorFrame starts out as pending, then becomes active, and then is
// replaced with another active CompositorFrame.
TEST_P(OnBeginFrameAcksSurfaceSynchronizationTest, ResourcesOnlyReturnedOnce) {
  const SurfaceId parent_id = MakeSurfaceId(kParentFrameSink, 1);
  const SurfaceId child_id = MakeSurfaceId(kChildFrameSink1, 1);

  // The parent submits a CompositorFrame that depends on |child_id| before
  // the child submits a CompositorFrame. The CompositorFrame also has
  // resources in its resource list.
  TransferableResource resource;
  resource.id = ResourceId(1337);
  resource.format = SinglePlaneFormat::kALPHA_8;
  resource.size = gfx::Size(1234, 5678);
  std::vector<TransferableResource> resource_list = {resource};
  parent_support().SubmitCompositorFrame(
      parent_id.local_surface_id(),
      MakeCompositorFrame({child_id}, empty_surface_ranges(), resource_list));

  // Verify that the CompositorFrame is blocked on |child_id|.
  EXPECT_FALSE(parent_surface()->HasActiveFrame());
  EXPECT_TRUE(parent_surface()->HasPendingFrame());
  EXPECT_THAT(parent_surface()->activation_dependencies(),
              UnorderedElementsAre(child_id));

  child_support1().SubmitCompositorFrame(
      child_id.local_surface_id(),
      MakeCompositorFrame(empty_surface_ids(), empty_surface_ranges(),
                          std::vector<TransferableResource>()));

  // Verify that the child CompositorFrame activates immediately.
  EXPECT_TRUE(child_surface1()->HasActiveFrame());
  EXPECT_FALSE(child_surface1()->HasPendingFrame());
  EXPECT_THAT(child_surface1()->activation_dependencies(), IsEmpty());

  // Verify that the parent has activated.
  EXPECT_TRUE(parent_surface()->HasActiveFrame());
  EXPECT_FALSE(parent_surface()->HasPendingFrame());
  EXPECT_THAT(parent_surface()->activation_dependencies(), IsEmpty());

  if (BeginFrameAcksEnabled()) {
    EXPECT_CALL(support_client_, DidReceiveCompositorFrameAck(_)).Times(0);
    EXPECT_CALL(support_client_, OnBeginFrame(_, _, _, _))
        .WillRepeatedly([=](const BeginFrameArgs& args,
                            const FrameTimingDetailsMap& timing_details,
                            bool frame_ack, std::vector<ReturnedResource> got) {
          EXPECT_EQ(0u, got.size());
        });
    SendNextBeginFrame();
  } else {
    std::vector<ReturnedResource> returned_resources;
    ResourceId id = resource.ToReturnedResource().id;
    EXPECT_CALL(support_client_, DidReceiveCompositorFrameAck(_))
        .WillOnce([=](std::vector<ReturnedResource> got) {
          EXPECT_EQ(1u, got.size());
          EXPECT_EQ(id, got[0].id);
        });
  }

  // The parent submits a CompositorFrame without any dependencies. That
  // frame should activate immediately, replacing the earlier frame. The
  // resource from the earlier frame should be returned to the client.
  parent_support().SubmitCompositorFrame(
      parent_id.local_surface_id(),
      MakeCompositorFrame({empty_surface_ids()}, {empty_surface_ranges()},
                          std::vector<TransferableResource>()));
  EXPECT_TRUE(parent_surface()->HasActiveFrame());
  EXPECT_FALSE(parent_surface()->HasPendingFrame());
  EXPECT_THAT(parent_surface()->activation_dependencies(), IsEmpty());

  if (BeginFrameAcksEnabled()) {
    if (AutoNeedsBeginFrame()) {
      // In this case, both the parent and the child have been set to needing
      // BeginFrame events because of the previous unsolicited frames.
      // Therefore, explicitly set the child to not needing BeginFrame events,
      // so that the expectation below for the client to receive exactly 1
      // OnBeginFrame call won't break.
      child_support1().SetNeedsBeginFrame(false);
    }

    ResourceId id = resource.ToReturnedResource().id;
    EXPECT_CALL(support_client_, OnBeginFrame(_, _, _, _))
        .WillOnce([=](const BeginFrameArgs& args,
                      const FrameTimingDetailsMap& timing_details,
                      bool frame_ack, std::vector<ReturnedResource> got) {
          EXPECT_EQ(1u, got.size());
          EXPECT_EQ(id, got[0].id);
        });
    SendNextBeginFrame();
  }
}

// This test verifies that if a surface has both a pending and active
// CompositorFrame and the pending CompositorFrame activates, replacing
// the existing active CompositorFrame, then the surface reference hierarchy
// will be updated allowing garbage collection of surfaces that are no longer
// referenced.
TEST_P(OnBeginFrameAcksSurfaceSynchronizationTest,
       DropStaleReferencesAfterActivation) {
  const SurfaceId parent_id = MakeSurfaceId(kParentFrameSink, 1);
  const SurfaceId child_id1 = MakeSurfaceId(kChildFrameSink1, 1);
  const SurfaceId child_id2 = MakeSurfaceId(kChildFrameSink2, 1);

  // The parent submits a CompositorFrame that depends on |child_id1| before
  // the child submits a CompositorFrame.
  EXPECT_CALL(support_client_, DidReceiveCompositorFrameAck(_)).Times(0);
  parent_support().SubmitCompositorFrame(
      parent_id.local_surface_id(),
      MakeCompositorFrame({child_id1}, empty_surface_ranges(),
                          std::vector<TransferableResource>()));

  // Verify that the CompositorFrame is blocked on |child_id|.
  EXPECT_FALSE(parent_surface()->HasActiveFrame());
  EXPECT_TRUE(parent_surface()->HasPendingFrame());
  EXPECT_THAT(parent_surface()->activation_dependencies(),
              UnorderedElementsAre(child_id1));
  testing::Mock::VerifyAndClearExpectations(&support_client_);

  // Verify that no references are added while the CompositorFrame is
  // pending.
  EXPECT_THAT(GetChildReferences(parent_id), IsEmpty());

  // DidReceiveCompositorFrameAck should get called twice: once for the child
  // and once for the now active parent CompositorFrame.
  EXPECT_CALL(support_client_, DidReceiveCompositorFrameAck(_))
      .Times(BeginFrameAcksEnabled() ? 0 : 2);
  child_support1().SubmitCompositorFrame(
      child_id1.local_surface_id(),
      MakeDefaultInteractiveCompositorFrame(kBeginFrameSourceId));
  testing::Mock::VerifyAndClearExpectations(&support_client_);

  // Verify that the child CompositorFrame activates immediately.
  EXPECT_TRUE(child_surface1()->HasActiveFrame());
  EXPECT_FALSE(child_surface1()->HasPendingFrame());
  EXPECT_THAT(child_surface1()->activation_dependencies(), IsEmpty());

  // Verify that the parent Surface has activated.
  EXPECT_TRUE(parent_surface()->HasActiveFrame());
  EXPECT_FALSE(parent_surface()->HasPendingFrame());
  EXPECT_THAT(parent_surface()->activation_dependencies(), IsEmpty());

  // Submit a new parent CompositorFrame to add a reference.
  parent_support().SubmitCompositorFrame(
      parent_id.local_surface_id(),
      MakeCompositorFrame(empty_surface_ids(), {SurfaceRange(child_id1)},
                          std::vector<TransferableResource>()));

  // Verify that the parent Surface has activated.
  EXPECT_TRUE(parent_surface()->HasActiveFrame());
  EXPECT_FALSE(parent_surface()->HasPendingFrame());
  EXPECT_THAT(parent_surface()->activation_dependencies(), IsEmpty());

  // Verify that there is no temporary reference for the child and that
  // the reference from the parent to the child still exists.
  EXPECT_FALSE(HasTemporaryReference(child_id1));
  EXPECT_THAT(GetChildReferences(parent_id), UnorderedElementsAre(child_id1));

  // The parent submits another CompositorFrame that depends on |child_id2|.
  // Submitting a pending CompositorFrame will not trigger a
  // CompositorFrameAck.
  EXPECT_CALL(support_client_, DidReceiveCompositorFrameAck(_)).Times(0);
  parent_support().SubmitCompositorFrame(
      parent_id.local_surface_id(),
      MakeCompositorFrame({child_id2}, empty_surface_ranges(),
                          std::vector<TransferableResource>()));
  testing::Mock::VerifyAndClearExpectations(&support_client_);

  // The parent surface should now have both a pending and activate
  // CompositorFrame. Verify that the set of child references from
  // |parent_id| are only from the active CompositorFrame.
  EXPECT_TRUE(parent_surface()->HasActiveFrame());
  EXPECT_TRUE(parent_surface()->HasPendingFrame());
  EXPECT_THAT(parent_surface()->activation_dependencies(),
              UnorderedElementsAre(child_id2));
  EXPECT_THAT(GetChildReferences(parent_id), UnorderedElementsAre(child_id1));

  child_support2().SubmitCompositorFrame(
      child_id2.local_surface_id(),
      MakeDefaultInteractiveCompositorFrame(kBeginFrameSourceId));

  // Verify that the parent Surface has activated and no longer has a
  // pending CompositorFrame. Also verify that |child_id1| is no longer a
  // child reference of |parent_id|.
  EXPECT_TRUE(parent_surface()->HasActiveFrame());
  EXPECT_FALSE(parent_surface()->HasPendingFrame());
  EXPECT_THAT(parent_surface()->activation_dependencies(), IsEmpty());
  // The parent will not immediately refer to the child until it submits a new
  // CompositorFrame with the reference.
  EXPECT_THAT(GetChildReferences(parent_id), IsEmpty());
}

// Verifies that LatencyInfo does not get too large after multiple frame
// submissions.
TEST_F(SurfaceSynchronizationTest, LimitLatencyInfo) {
  const SurfaceId parent_id = MakeSurfaceId(kParentFrameSink, 1);
  const ui::LatencyComponentType latency_type1 =
      ui::INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT;
  const ui::LatencyComponentType latency_type2 =
      ui::LATENCY_COMPONENT_TYPE_LAST;

  // Submit a frame with latency info
  ui::LatencyInfo info;
  info.AddLatencyNumber(latency_type1);

  CompositorFrameBuilder builder;
  builder.AddDefaultRenderPass();
  for (int i = 0; i < 60; ++i)
    builder.AddLatencyInfo(info);
  CompositorFrame frame = builder.Build();

  parent_support().SubmitCompositorFrame(parent_id.local_surface_id(),
                                         std::move(frame));

  // Verify that the surface has an active frame and no pending frame.
  Surface* surface = GetSurfaceForId(parent_id);
  ASSERT_NE(nullptr, surface);
  EXPECT_TRUE(surface->HasActiveFrame());
  EXPECT_FALSE(surface->HasPendingFrame());

  // Submit another frame with some other latency info.
  ui::LatencyInfo info2;
  info2.AddLatencyNumber(latency_type2);

  builder.AddDefaultRenderPass();
  for (int i = 0; i < 60; ++i)
    builder.AddLatencyInfo(info);
  CompositorFrame frame2 = builder.Build();

  parent_support().SubmitCompositorFrame(parent_id.local_surface_id(),
                                         std::move(frame2));

  // Verify that the surface has an active frame and no pending frames.
  EXPECT_TRUE(surface->HasActiveFrame());
  EXPECT_FALSE(surface->HasPendingFrame());

  // Verify that the surface has no latency info objects because it grew
  // too large.
  std::vector<ui::LatencyInfo> info_list;
  surface->TakeActiveLatencyInfo(&info_list);
  EXPECT_EQ(0u, info_list.size());
}

// Checks whether SurfaceAllocationGroup properly aggregates LatencyInfo of
// multiple surfaces. In this variation of the test, there are no pending
// frames.
TEST_F(SurfaceSynchronizationTest,
       LatencyInfoAggregation_NoUnresolvedDependencies) {
  const SurfaceId parent_id1 = MakeSurfaceId(kParentFrameSink, 1);
  const SurfaceId parent_id2 = MakeSurfaceId(kParentFrameSink, 2);
  const ui::LatencyComponentType latency_type1 =
      ui::INPUT_EVENT_LATENCY_RENDERER_SWAP_COMPONENT;
  const ui::LatencyComponentType latency_type2 =
      ui::LATENCY_COMPONENT_TYPE_LAST;

  // Submit a frame with latency info
  ui::LatencyInfo info;
  info.AddLatencyNumber(latency_type1);

  CompositorFrame frame = CompositorFrameBuilder()
                              .AddDefaultRenderPass()
                              .AddLatencyInfo(info)
                              .SetIsHandlingInteraction(true)
                              .Build();

  parent_support().SubmitCompositorFrame(parent_id1.local_surface_id(),
                                         std::move(frame));

  // Verify that the old surface has an active frame and no pending frame.
  Surface* old_surface = GetSurfaceForId(parent_id1);
  ASSERT_NE(nullptr, old_surface);
  EXPECT_TRUE(old_surface->HasActiveFrame());
  EXPECT_FALSE(old_surface->HasPendingFrame());

  // Submit another frame with some other latency info and a different
  // LocalSurfaceId.
  ui::LatencyInfo info2;
  info2.AddLatencyNumber(latency_type2);

  CompositorFrame frame2 = CompositorFrameBuilder()
                               .AddDefaultRenderPass()
                               .AddLatencyInfo(info2)
                               .SetIsHandlingInteraction(true)
                               .Build();

  parent_support().SubmitCompositorFrame(parent_id2.local_surface_id(),
                                         std::move(frame2));

  // Verify that the new surface has an active frame and no pending frames.
  Surface* surface = GetSurfaceForId(parent_id2);
  ASSERT_NE(nullptr, surface);
  EXPECT_TRUE(surface->HasActiveFrame());
  EXPECT_FALSE(surface->HasPendingFrame());

  // Verify that the new surface has both latency info elements.
  std::vector<ui::LatencyInfo> info_list;
  surface->allocation_group()->TakeAggregatedLatencyInfoUpTo(surface,
                                                             &info_list);
  EXPECT_EQ(2u, info_list.size());

  ui::LatencyInfo aggregated_latency_info = info_list[0];
  aggregated_latency_info.AddNewLatencyFrom(info_list[1]);

  // Two components are the original ones, and the third one is
  // DISPLAY_COMPOSITOR_RECEIVED_FRAME_COMPONENT, logged on compositor frame
  // submit.
  EXPECT_EQ(3u, aggregated_latency_info.latency_components().size());

  EXPECT_TRUE(aggregated_latency_info.FindLatency(latency_type1, nullptr));
  EXPECT_TRUE(aggregated_latency_info.FindLatency(latency_type2, nullptr));
  EXPECT_TRUE(aggregated_latency_info.FindLatency(
      ui::DISPLAY_COMPOSITOR_RECEIVED_FRAME_COMPONENT, nullptr));
}

// Checks whether SurfaceAllocationGroup properly aggregates LatencyInfo of
// multiple surfaces. In this variation of the test, the older surface has both
// pending and active frames and we verify that the LatencyInfo of both pending
// and active frame are present in the aggregated LatencyInfo.
TEST_F(SurfaceSynchronizationTest,
       LatencyInfoAggregation_OldSurfaceHasPendingAndActiveFrame) {
  const SurfaceId parent_id1 = MakeSurfaceId(kParentFrameSink, 1);
  const SurfaceId parent_id2 = MakeSurfaceId(kParentFrameSink, 2);
  const SurfaceId child_id = MakeSurfaceId(kChildFrameSink1, 1);

  const ui::LatencyComponentType latency_type1 =
      ui::INPUT_EVENT_LATENCY_RENDERER_SWAP_COMPONENT;
  const ui::LatencyComponentType latency_type2 =
      ui::LATENCY_COMPONENT_TYPE_LAST;

  // Submit a frame with no unresolved dependency.
  ui::LatencyInfo info;
  info.AddLatencyNumber(latency_type1);

  CompositorFrame frame =
      MakeDefaultInteractiveCompositorFrame(kBeginFrameSourceId);
  frame.metadata.latency_info.push_back(info);

  parent_support().SubmitCompositorFrame(parent_id1.local_surface_id(),
                                         std::move(frame));

  // Submit a frame with unresolved dependencies.
  ui::LatencyInfo info2;
  info2.AddLatencyNumber(latency_type2);

  CompositorFrame frame2 = MakeCompositorFrame(
      {child_id}, empty_surface_ranges(), std::vector<TransferableResource>());
  frame2.metadata.latency_info.push_back(info2);

  parent_support().SubmitCompositorFrame(parent_id1.local_surface_id(),
                                         std::move(frame2));

  // Verify that the old surface has both an active and a pending frame.
  Surface* old_surface = GetSurfaceForId(parent_id1);
  ASSERT_NE(nullptr, old_surface);
  EXPECT_TRUE(old_surface->HasActiveFrame());
  EXPECT_TRUE(old_surface->HasPendingFrame());

  // Submit a frame with a new local surface id.
  parent_support().SubmitCompositorFrame(
      parent_id2.local_surface_id(),
      MakeDefaultInteractiveCompositorFrame(kBeginFrameSourceId));

  // Verify that the new surface has an active frame only.
  Surface* surface = GetSurfaceForId(parent_id2);
  ASSERT_NE(nullptr, surface);
  EXPECT_TRUE(surface->HasActiveFrame());
  EXPECT_FALSE(surface->HasPendingFrame());

  // Verify that the aggregated LatencyInfo has LatencyInfo from both active and
  // pending frame of the old surface.
  std::vector<ui::LatencyInfo> info_list;
  surface->allocation_group()->TakeAggregatedLatencyInfoUpTo(surface,
                                                             &info_list);
  EXPECT_EQ(2u, info_list.size());

  ui::LatencyInfo aggregated_latency_info = info_list[0];
  aggregated_latency_info.AddNewLatencyFrom(info_list[1]);

  // Two components are the original ones, and the third one is
  // DISPLAY_COMPOSITOR_RECEIVED_FRAME_COMPONENT, logged on compositor frame
  // submit.
  EXPECT_EQ(3u, aggregated_latency_info.latency_components().size());

  EXPECT_TRUE(aggregated_latency_info.FindLatency(latency_type1, nullptr));
  EXPECT_TRUE(aggregated_latency_info.FindLatency(latency_type2, nullptr));
  EXPECT_TRUE(aggregated_latency_info.FindLatency(
      ui::DISPLAY_COMPOSITOR_RECEIVED_FRAME_COMPONENT, nullptr));
}

// Checks whether SurfaceAllocationGroup properly aggregates LatencyInfo of
// multiple surfaces. In this variation of the test, the newer surface has a
// pending frame that becomes active after the dependency is resolved and we
// make sure the LatencyInfo of the activated frame is included in the
// aggregated LatencyInfo.
TEST_F(SurfaceSynchronizationTest,
       LatencyInfoAggregation_NewSurfaceHasPendingFrame) {
  const SurfaceId parent_id1 = MakeSurfaceId(kParentFrameSink, 1);
  const SurfaceId parent_id2 = MakeSurfaceId(kParentFrameSink, 2);
  const SurfaceId child_id = MakeSurfaceId(kChildFrameSink1, 1);

  const ui::LatencyComponentType latency_type1 =
      ui::INPUT_EVENT_LATENCY_RENDERER_SWAP_COMPONENT;
  const ui::LatencyComponentType latency_type2 =
      ui::LATENCY_COMPONENT_TYPE_LAST;

  // Submit a frame with no unresolved dependencies.
  ui::LatencyInfo info;
  info.AddLatencyNumber(latency_type1);

  CompositorFrame frame =
      MakeDefaultInteractiveCompositorFrame(kBeginFrameSourceId);
  frame.metadata.latency_info.push_back(info);

  parent_support().SubmitCompositorFrame(parent_id1.local_surface_id(),
                                         std::move(frame));

  // Verify that the old surface has an active frame only.
  Surface* old_surface = GetSurfaceForId(parent_id1);
  ASSERT_NE(nullptr, old_surface);
  EXPECT_TRUE(old_surface->HasActiveFrame());
  EXPECT_FALSE(old_surface->HasPendingFrame());

  // Submit a frame with a new local surface id and with unresolved
  // dependencies.
  ui::LatencyInfo info2;
  info2.AddLatencyNumber(latency_type2);

  CompositorFrame frame2 = MakeCompositorFrame(
      {child_id}, empty_surface_ranges(), std::vector<TransferableResource>());
  frame2.metadata.latency_info.push_back(info2);

  parent_support().SubmitCompositorFrame(parent_id2.local_surface_id(),
                                         std::move(frame2));

  // Verify that the new surface has a pending frame and no active frame.
  Surface* surface = GetSurfaceForId(parent_id2);
  ASSERT_NE(nullptr, surface);
  EXPECT_TRUE(surface->HasPendingFrame());
  EXPECT_FALSE(surface->HasActiveFrame());

  // Resolve the dependencies. The frame in parent's surface must become active.
  child_support1().SubmitCompositorFrame(
      child_id.local_surface_id(),
      MakeDefaultInteractiveCompositorFrame(kBeginFrameSourceId));
  EXPECT_FALSE(surface->HasPendingFrame());
  EXPECT_TRUE(surface->HasActiveFrame());

  // Both latency info elements must exist in the aggregated LatencyInfo.
  std::vector<ui::LatencyInfo> info_list;
  surface->allocation_group()->TakeAggregatedLatencyInfoUpTo(surface,
                                                             &info_list);
  EXPECT_EQ(2u, info_list.size());

  ui::LatencyInfo aggregated_latency_info = info_list[0];
  aggregated_latency_info.AddNewLatencyFrom(info_list[1]);

  // Two components are the original ones, and the third one is
  // DISPLAY_COMPOSITOR_RECEIVED_FRAME_COMPONENT, logged on compositor frame
  // submit.
  EXPECT_EQ(3u, aggregated_latency_info.latency_components().size());

  EXPECT_TRUE(aggregated_latency_info.FindLatency(latency_type1, nullptr));
  EXPECT_TRUE(aggregated_latency_info.FindLatency(latency_type2, nullptr));
  EXPECT_TRUE(aggregated_latency_info.FindLatency(
      ui::DISPLAY_COMPOSITOR_RECEIVED_FRAME_COMPONENT, nullptr));
}

// Checks whether SurfaceAllocationGroup properly aggregates LatencyInfo of
// multiple surfaces. In this variation of the test, multiple older surfaces
// with pending frames exist during aggregation of an activated frame on a newer
// surface.
TEST_F(SurfaceSynchronizationTest,
       LatencyInfoAggregation_MultipleOldSurfacesWithPendingFrames) {
  const SurfaceId parent_id1 = MakeSurfaceId(kParentFrameSink, 1);
  const SurfaceId parent_id2 = MakeSurfaceId(kParentFrameSink, 2);
  const SurfaceId parent_id3 = MakeSurfaceId(kParentFrameSink, 3);
  const SurfaceId child_id1 = MakeSurfaceId(kChildFrameSink1, 1);
  const SurfaceId child_id2 = MakeSurfaceId(kChildFrameSink1, 2);

  const ui::LatencyComponentType latency_type1 =
      ui::INPUT_EVENT_LATENCY_RENDERER_SWAP_COMPONENT;
  const ui::LatencyComponentType latency_type2 =
      ui::LATENCY_COMPONENT_TYPE_LAST;

  // Submit a frame with unresolved dependencies to parent_id1.
  ui::LatencyInfo info1;
  info1.AddLatencyNumber(latency_type1);

  CompositorFrame frame1 = MakeCompositorFrame(
      {child_id1}, empty_surface_ranges(), std::vector<TransferableResource>());
  frame1.metadata.latency_info.push_back(info1);

  parent_support().SubmitCompositorFrame(parent_id1.local_surface_id(),
                                         std::move(frame1));

  // Submit a frame with unresolved dependencies to parent_id2.
  ui::LatencyInfo info2;
  info2.AddLatencyNumber(latency_type2);

  CompositorFrame frame2 = MakeCompositorFrame(
      {child_id2}, empty_surface_ranges(), std::vector<TransferableResource>());
  frame2.metadata.latency_info.push_back(info2);

  parent_support().SubmitCompositorFrame(parent_id2.local_surface_id(),
                                         std::move(frame2));

  // Verify that the both old surfaces have pending frames.
  Surface* old_surface1 = GetSurfaceForId(parent_id1);
  Surface* old_surface2 = GetSurfaceForId(parent_id2);
  ASSERT_NE(nullptr, old_surface1);
  ASSERT_NE(nullptr, old_surface2);
  EXPECT_FALSE(old_surface1->HasActiveFrame());
  EXPECT_FALSE(old_surface2->HasActiveFrame());
  EXPECT_TRUE(old_surface1->HasPendingFrame());
  EXPECT_TRUE(old_surface2->HasPendingFrame());

  // Submit a frame with no dependencies to the new surface parent_id3.
  parent_support().SubmitCompositorFrame(
      parent_id3.local_surface_id(),
      MakeDefaultInteractiveCompositorFrame(kBeginFrameSourceId));

  // Verify that the new surface has an active frame only.
  Surface* surface = GetSurfaceForId(parent_id3);
  ASSERT_NE(nullptr, surface);
  EXPECT_TRUE(surface->HasActiveFrame());
  EXPECT_FALSE(surface->HasPendingFrame());

  // Verify that the aggregated LatencyInfo has LatencyInfo from both old
  // surfaces.
  std::vector<ui::LatencyInfo> info_list;
  surface->allocation_group()->TakeAggregatedLatencyInfoUpTo(surface,
                                                             &info_list);
  EXPECT_EQ(2u, info_list.size());

  ui::LatencyInfo aggregated_latency_info = info_list[0];
  aggregated_latency_info.AddNewLatencyFrom(info_list[1]);

  // Two components are the original ones, and the third one is
  // DISPLAY_COMPOSITOR_RECEIVED_FRAME_COMPONENT, logged on compositor frame
  // submit.
  EXPECT_EQ(3u, aggregated_latency_info.latency_components().size());

  EXPECT_TRUE(aggregated_latency_info.FindLatency(latency_type1, nullptr));
  EXPECT_TRUE(aggregated_latency_info.FindLatency(latency_type2, nullptr));
  EXPECT_TRUE(aggregated_latency_info.FindLatency(
      ui::DISPLAY_COMPOSITOR_RECEIVED_FRAME_COMPONENT, nullptr));
}

// This test verifies that when a new surface is created, the LatencyInfo of the
// previous surface does not get carried over into the new surface.
TEST_F(SurfaceSynchronizationTest, LatencyInfoNotCarriedOver) {
  const SurfaceId parent_id1 = MakeSurfaceId(kParentFrameSink, 1);
  const SurfaceId parent_id2 = MakeSurfaceId(kParentFrameSink, 2);
  const ui::LatencyComponentType latency_type1 =
      ui::INPUT_EVENT_LATENCY_RENDERER_SWAP_COMPONENT;
  const ui::LatencyComponentType latency_type2 =
      ui::LATENCY_COMPONENT_TYPE_LAST;

  // Submit a frame with latency info
  ui::LatencyInfo info;
  info.AddLatencyNumber(latency_type1);

  CompositorFrame frame = CompositorFrameBuilder()
                              .AddDefaultRenderPass()
                              .AddLatencyInfo(info)
                              .SetIsHandlingInteraction(true)
                              .Build();

  parent_support().SubmitCompositorFrame(parent_id1.local_surface_id(),
                                         std::move(frame));

  // Verify that the old surface has an active frame and no pending frame.
  Surface* old_surface = GetSurfaceForId(parent_id1);
  ASSERT_NE(nullptr, old_surface);
  EXPECT_TRUE(old_surface->HasActiveFrame());
  EXPECT_FALSE(old_surface->HasPendingFrame());

  // Submit another frame with some other latency info and a different
  // LocalSurfaceId.
  ui::LatencyInfo info2;
  info2.AddLatencyNumber(latency_type2);

  CompositorFrame frame2 = CompositorFrameBuilder()
                               .AddDefaultRenderPass()
                               .AddLatencyInfo(info2)
                               .SetIsHandlingInteraction(true)
                               .Build();

  parent_support().SubmitCompositorFrame(parent_id2.local_surface_id(),
                                         std::move(frame2));

  // Verify that the new surface has an active frame and no pending frames.
  Surface* surface = GetSurfaceForId(parent_id2);
  ASSERT_NE(nullptr, surface);
  EXPECT_TRUE(surface->HasActiveFrame());
  EXPECT_FALSE(surface->HasPendingFrame());

  // Verify that the old surface still has its LatencyInfo.
  std::vector<ui::LatencyInfo> info_list;
  old_surface->TakeActiveLatencyInfo(&info_list);
  EXPECT_EQ(1u, info_list.size());
  EXPECT_TRUE(info_list[0].FindLatency(latency_type1, nullptr));
  EXPECT_FALSE(info_list[0].FindLatency(latency_type2, nullptr));
  EXPECT_TRUE(info_list[0].FindLatency(
      ui::DISPLAY_COMPOSITOR_RECEIVED_FRAME_COMPONENT, nullptr));

  // Take the aggregated LatencyInfo. Since the LatencyInfo of the old surface
  // is previously taken, it should not show up here.
  info_list.clear();
  surface->allocation_group()->TakeAggregatedLatencyInfoUpTo(surface,
                                                             &info_list);
  EXPECT_EQ(1u, info_list.size());
  EXPECT_EQ(2u, info_list[0].latency_components().size());
  EXPECT_FALSE(info_list[0].FindLatency(latency_type1, nullptr));
  EXPECT_TRUE(info_list[0].FindLatency(latency_type2, nullptr));
  EXPECT_TRUE(info_list[0].FindLatency(
      ui::DISPLAY_COMPOSITOR_RECEIVED_FRAME_COMPONENT, nullptr));
}

// Checks that resources and ack are sent together if possible.
TEST_P(OnBeginFrameAcksSurfaceSynchronizationTest, ReturnResourcesWithAck) {
  const SurfaceId parent_id = MakeSurfaceId(kParentFrameSink, 1);
  TransferableResource resource;
  resource.id = ResourceId(1234);
  parent_support().SubmitCompositorFrame(
      parent_id.local_surface_id(),
      MakeCompositorFrame(empty_surface_ids(), empty_surface_ranges(),
                          {resource}));
  ResourceId id = resource.ToReturnedResource().id;
  EXPECT_CALL(support_client_, ReclaimResources(_)).Times(0);
  if (BeginFrameAcksEnabled()) {
    EXPECT_CALL(support_client_, DidReceiveCompositorFrameAck(_)).Times(0);
  } else {
    EXPECT_CALL(support_client_, DidReceiveCompositorFrameAck(_))
        .WillOnce([=](std::vector<ReturnedResource> got) {
          EXPECT_EQ(1u, got.size());
          EXPECT_EQ(id, got[0].id);
        });
  }
  parent_support().SubmitCompositorFrame(
      parent_id.local_surface_id(),
      MakeDefaultInteractiveCompositorFrame(kBeginFrameSourceId));
  if (BeginFrameAcksEnabled()) {
    EXPECT_CALL(support_client_, OnBeginFrame(_, _, _, _))
        .WillOnce([=](const BeginFrameArgs& args,
                      const FrameTimingDetailsMap& timing_details,
                      bool frame_ack, std::vector<ReturnedResource> got) {
          EXPECT_EQ(1u, got.size());
          EXPECT_EQ(id, got[0].id);
        });
    SendNextBeginFrame();
  }
}

// Verifies that arrival of a new CompositorFrame doesn't change the fact that a
// surface is marked for destruction.
TEST_P(OnBeginFrameAcksSurfaceSynchronizationTest, SubmitToDestroyedSurface) {
  const SurfaceId parent_id = MakeSurfaceId(kParentFrameSink, 1);
  const SurfaceId child_id = MakeSurfaceId(kChildFrameSink1, 3);

  // Create the child surface by submitting a frame to it.
  EXPECT_EQ(nullptr, GetSurfaceForId(child_id));
  TransferableResource resource;
  resource.id = ResourceId(1234);
  child_support1().SubmitCompositorFrame(
      child_id.local_surface_id(),
      MakeCompositorFrame(empty_surface_ids(), empty_surface_ranges(),
                          {resource}));
  // Verify that the child surface is created.
  Surface* surface = GetSurfaceForId(child_id);
  EXPECT_NE(nullptr, surface);

  // Add a reference from the parent to the child.
  parent_support().SubmitCompositorFrame(
      parent_id.local_surface_id(),
      MakeCompositorFrame({child_id}, {SurfaceRange(child_id)},
                          std::vector<TransferableResource>()));

  // Attempt to destroy the child surface. The surface must still exist since
  // the parent needs it but it will be marked as destroyed.
  child_support1().EvictSurface(child_id.local_surface_id());
  surface = GetSurfaceForId(child_id);
  EXPECT_NE(nullptr, surface);
  EXPECT_TRUE(IsMarkedForDestruction(child_id));

  // Child submits another frame to the same local surface id that is marked
  // destroyed. The frame is immediately rejected.
  {
    EXPECT_CALL(support_client_, ReclaimResources(_)).Times(0);
    EXPECT_CALL(support_client_, DidReceiveCompositorFrameAck(_))
        .Times(BeginFrameAcksEnabled() ? 0 : 1);
    surface_observer().Reset();
    child_support1().SubmitCompositorFrame(
        child_id.local_surface_id(),
        MakeDefaultInteractiveCompositorFrame(kBeginFrameSourceId));
    testing::Mock::VerifyAndClearExpectations(&support_client_);
  }

  // The parent stops referencing the child surface. This allows the child
  // surface to be garbage collected.
  parent_support().SubmitCompositorFrame(
      parent_id.local_surface_id(),
      MakeDefaultInteractiveCompositorFrame(kBeginFrameSourceId));

  {
    ResourceId id = resource.ToReturnedResource().id;
    EXPECT_CALL(support_client_, ReclaimResources(_))
        .WillOnce([=](std::vector<ReturnedResource> got) {
          EXPECT_EQ(1u, got.size());
          EXPECT_EQ(id, got[0].id);
        });
    frame_sink_manager().surface_manager()->GarbageCollectSurfaces();
    testing::Mock::VerifyAndClearExpectations(&support_client_);
  }

  // We shouldn't observe an OnFirstSurfaceActivation because we reject the
  // CompositorFrame to the evicted surface.
  EXPECT_EQ(SurfaceId(), surface_observer().last_created_surface_id());
}

// Verifies that if a LocalSurfaceId belonged to a surface that doesn't
// exist anymore, it can not be recreated.
TEST_F(SurfaceSynchronizationTest, LocalSurfaceIdIsNotReusable) {
  const SurfaceId parent_id = MakeSurfaceId(kParentFrameSink, 1);
  const SurfaceId child_id = MakeSurfaceId(kChildFrameSink1, 3);

  // Submit the first frame. Creates the surface.
  child_support1().SubmitCompositorFrame(
      child_id.local_surface_id(),
      MakeDefaultInteractiveCompositorFrame(kBeginFrameSourceId));
  EXPECT_NE(nullptr, GetSurfaceForId(child_id));

  // Add a reference from parent.
  parent_support().SubmitCompositorFrame(
      parent_id.local_surface_id(),
      MakeCompositorFrame({child_id}, {SurfaceRange(child_id)},
                          std::vector<TransferableResource>()));

  // Remove the reference from parant. This allows us to destroy the surface.
  parent_support().SubmitCompositorFrame(
      parent_id.local_surface_id(),
      MakeDefaultInteractiveCompositorFrame(kBeginFrameSourceId));

  // Destroy the surface.
  child_support1().EvictSurface(child_id.local_surface_id());
  frame_sink_manager().surface_manager()->GarbageCollectSurfaces();

  EXPECT_EQ(nullptr, GetSurfaceForId(child_id));

  // Submit another frame with the same local surface id. The surface should not
  // be recreated.
  child_support1().SubmitCompositorFrame(
      child_id.local_surface_id(),
      MakeDefaultInteractiveCompositorFrame(kBeginFrameSourceId));
  EXPECT_EQ(nullptr, GetSurfaceForId(child_id));
}

// This test verifies that a crash does not occur if garbage collection is
// triggered during surface dependency resolution. This test triggers garbage
// collection during surface resolution, by causing an activation to remove
// a surface subtree from the root. Both the old subtree and the new
// activated subtree refer to the same dependency. The old subtree was activated
// by deadline, and the new subtree was activated by a dependency finally
// resolving.
TEST_F(SurfaceSynchronizationTest, DependencyTrackingGarbageCollection) {
  const SurfaceId display_id = MakeSurfaceId(kDisplayFrameSink, 1);
  const SurfaceId parent_id1 = MakeSurfaceId(kParentFrameSink, 1);
  const SurfaceId parent_id2 = MakeSurfaceId(kParentFrameSink, 2);
  const SurfaceId child_id = MakeSurfaceId(kChildFrameSink1, 1);

  parent_support().SubmitCompositorFrame(
      parent_id1.local_surface_id(),
      MakeCompositorFrame({child_id}, empty_surface_ranges(),
                          std::vector<TransferableResource>(),
                          MakeDefaultDeadline()));
  display_support().SubmitCompositorFrame(
      display_id.local_surface_id(),
      MakeCompositorFrame({parent_id1}, empty_surface_ranges(),
                          std::vector<TransferableResource>(),
                          MakeDefaultDeadline()));

  EXPECT_TRUE(parent_surface()->has_deadline());

  // Advance BeginFrames to trigger a deadline.
  for (int i = 0; i < 3; ++i) {
    SendNextBeginFrame();
    EXPECT_TRUE(display_surface()->has_deadline());
    EXPECT_TRUE(parent_surface()->has_deadline());
  }
  SendNextBeginFrame();

  EXPECT_TRUE(display_surface()->HasActiveFrame());
  EXPECT_FALSE(display_surface()->HasPendingFrame());
  EXPECT_TRUE(parent_surface()->HasActiveFrame());
  EXPECT_FALSE(parent_surface()->HasPendingFrame());

  parent_support().SubmitCompositorFrame(
      parent_id2.local_surface_id(),
      MakeCompositorFrame({child_id}, empty_surface_ranges(),
                          std::vector<TransferableResource>(),
                          MakeDefaultDeadline()));
  display_support().SubmitCompositorFrame(
      display_id.local_surface_id(),
      MakeCompositorFrame({parent_id2}, empty_surface_ranges(),
                          std::vector<TransferableResource>(),
                          MakeDefaultDeadline()));

  // The display surface now has two CompositorFrames. One that is pending,
  // indirectly blocked on child_id and one that is active, also indirectly
  // referring to child_id, but activated due to the deadline above.
  EXPECT_TRUE(display_surface()->HasActiveFrame());
  EXPECT_TRUE(display_surface()->HasPendingFrame());

  // Submitting a CompositorFrame will trigger garbage collection of the
  // |parent_id1| subtree. This should not crash.
  child_support1().SubmitCompositorFrame(
      child_id.local_surface_id(),
      MakeDefaultInteractiveCompositorFrame(kBeginFrameSourceId));
}

// This test verifies that a crash does not occur if garbage collection is
// triggered when a deadline forces frame activation. This test triggers garbage
// collection during deadline activation by causing the activation of a display
// frame to replace a previously activated display frame that was referring to
// a now-unreachable surface subtree. That subtree gets garbage collected during
// deadline activation. SurfaceDependencyTracker is also tracking a surface
// from that subtree due to an unresolved dependency. This test verifies that
// this dependency resolution does not crash.
TEST_F(SurfaceSynchronizationTest, GarbageCollectionOnDeadline) {
  const SurfaceId display_id = MakeSurfaceId(kDisplayFrameSink, 1);
  const SurfaceId parent_id1 = MakeSurfaceId(kParentFrameSink, 1);
  const SurfaceId parent_id2 = MakeSurfaceId(kParentFrameSink, 2);
  const SurfaceId child_id = MakeSurfaceId(kChildFrameSink1, 1);

  // |parent_id1| is blocked on |child_id|.
  parent_support().SubmitCompositorFrame(
      parent_id1.local_surface_id(),
      MakeCompositorFrame({child_id}, empty_surface_ranges(),
                          std::vector<TransferableResource>(),
                          MakeDefaultDeadline()));

  display_support().SubmitCompositorFrame(
      display_id.local_surface_id(),
      MakeCompositorFrame({parent_id1}, {SurfaceRange(parent_id1)},
                          std::vector<TransferableResource>(),
                          MakeDefaultDeadline()));

  EXPECT_TRUE(display_surface()->has_deadline());
  EXPECT_TRUE(parent_surface()->has_deadline());
  EXPECT_TRUE(display_surface()->HasPendingFrame());
  EXPECT_FALSE(display_surface()->HasActiveFrame());

  // Advance BeginFrames to trigger a deadline. This activates the
  // CompositorFrame submitted above.
  for (int i = 0; i < 3; ++i) {
    SendNextBeginFrame();
    EXPECT_TRUE(display_surface()->has_deadline());
    EXPECT_TRUE(parent_surface()->has_deadline());
  }
  SendNextBeginFrame();
  EXPECT_FALSE(display_surface()->has_deadline());
  EXPECT_FALSE(parent_surface()->has_deadline());
  EXPECT_FALSE(display_surface()->HasPendingFrame());
  EXPECT_TRUE(display_surface()->HasActiveFrame());

  // By submitting a display CompositorFrame, and replacing the parent's
  // CompositorFrame with another surface ID, parent_id1 becomes unreachable
  // and a candidate for garbage collection.
  display_support().SubmitCompositorFrame(
      display_id.local_surface_id(),
      MakeCompositorFrame({parent_id2}, empty_surface_ranges(),
                          std::vector<TransferableResource>(),
                          MakeDefaultDeadline()));
  EXPECT_TRUE(display_surface()->has_deadline());

  // Now |parent_id1| is only kept alive by the active |display_id| frame.
  parent_support().SubmitCompositorFrame(
      parent_id2.local_surface_id(),
      MakeCompositorFrame({child_id}, empty_surface_ranges(),
                          std::vector<TransferableResource>(),
                          MakeDefaultDeadline()));
  EXPECT_TRUE(display_surface()->has_deadline());
  EXPECT_TRUE(parent_surface()->has_deadline());

  // SurfaceDependencyTracker should now be tracking |display_id|, |parent_id1|
  // and |parent_id2|. By activating the pending |display_id| frame by deadline,
  // |parent_id1| becomes unreachable and is garbage collected while
  // SurfaceDependencyTracker is in the process of activating surfaces. This
  // should not cause a crash or use-after-free.
  for (int i = 0; i < 3; ++i) {
    SendNextBeginFrame();
    EXPECT_TRUE(display_surface()->has_deadline());
  }
  SendNextBeginFrame();
  EXPECT_FALSE(display_surface()->has_deadline());
}

// This test verifies that a CompositorFrame will only blocked on embedded
// surfaces but not on other retained surface IDs in the CompositorFrame.
TEST_F(SurfaceSynchronizationTest, OnlyBlockOnEmbeddedSurfaces) {
  const SurfaceId display_id = MakeSurfaceId(kDisplayFrameSink, 1);
  const SurfaceId parent_id1 = MakeSurfaceId(kParentFrameSink, 1);
  const SurfaceId parent_id2 = MakeSurfaceId(kParentFrameSink, 2);

  // Submitting a CompositorFrame with |parent_id2| so that the display
  // CompositorFrame can hold a reference to it.
  parent_support().SubmitCompositorFrame(
      parent_id1.local_surface_id(),
      MakeDefaultInteractiveCompositorFrame(kBeginFrameSourceId));

  display_support().SubmitCompositorFrame(
      display_id.local_surface_id(),
      MakeCompositorFrame({parent_id2}, {SurfaceRange(parent_id1)},
                          std::vector<TransferableResource>()));

  EXPECT_TRUE(display_surface()->HasPendingFrame());
  EXPECT_FALSE(display_surface()->HasActiveFrame());
  EXPECT_TRUE(display_surface()->has_deadline());

  // Verify that the display CompositorFrame will only block on |parent_id2|
  // but not |parent_id1|.
  EXPECT_THAT(display_surface()->activation_dependencies(),
              UnorderedElementsAre(parent_id2));
  // Verify that the display surface holds no references while its
  // CompositorFrame is pending.
  EXPECT_THAT(GetChildReferences(display_id), IsEmpty());

  // Submitting a CompositorFrame with |parent_id2| should unblock the
  // display CompositorFrame.
  parent_support().SubmitCompositorFrame(
      parent_id2.local_surface_id(),
      MakeDefaultInteractiveCompositorFrame(kBeginFrameSourceId));

  EXPECT_FALSE(display_surface()->has_deadline());
  EXPECT_FALSE(display_surface()->HasPendingFrame());
  EXPECT_TRUE(display_surface()->HasActiveFrame());
  EXPECT_THAT(display_surface()->activation_dependencies(), IsEmpty());
}

// This test verifies that a late arriving CompositorFrame activates
// immediately and does not trigger a new deadline.
TEST_F(SurfaceSynchronizationTest, LateArrivingDependency) {
  const SurfaceId display_id = MakeSurfaceId(kDisplayFrameSink, 1);
  const SurfaceId parent_id1 = MakeSurfaceId(kParentFrameSink, 1);
  const SurfaceId child_id1 = MakeSurfaceId(kChildFrameSink1, 1);

  display_support().SubmitCompositorFrame(
      display_id.local_surface_id(),
      MakeCompositorFrame(
          {parent_id1}, {SurfaceRange(std::nullopt, parent_id1)},
          std::vector<TransferableResource>(), MakeDefaultDeadline()));

  EXPECT_TRUE(display_surface()->HasPendingFrame());
  EXPECT_FALSE(display_surface()->HasActiveFrame());
  EXPECT_TRUE(display_surface()->has_deadline());

  // Advance BeginFrames to trigger a deadline. This activates the
  // CompositorFrame submitted above.
  for (int i = 0; i < 3; ++i) {
    SendNextBeginFrame();
    EXPECT_TRUE(display_surface()->has_deadline());
  }
  SendNextBeginFrame();
  EXPECT_FALSE(display_surface()->has_deadline());
  EXPECT_FALSE(display_surface()->HasPendingFrame());
  EXPECT_TRUE(display_surface()->HasActiveFrame());

  // A late arriving CompositorFrame should activate immediately without
  // scheduling a deadline and without waiting for dependencies to resolve.
  parent_support().SubmitCompositorFrame(
      parent_id1.local_surface_id(),
      MakeCompositorFrame({child_id1}, {SurfaceRange(std::nullopt, child_id1)},
                          std::vector<TransferableResource>(),
                          MakeDefaultDeadline()));
  EXPECT_FALSE(parent_surface()->has_deadline());
  EXPECT_FALSE(parent_surface()->HasPendingFrame());
  EXPECT_TRUE(parent_surface()->HasActiveFrame());
}

// This test verifies that a late arriving CompositorFrame activates
// immediately along with its subtree and does not trigger a new deadline.
TEST_F(SurfaceSynchronizationTest, MultiLevelLateArrivingDependency) {
  const SurfaceId display_id = MakeSurfaceId(kDisplayFrameSink, 1);
  const SurfaceId parent_id = MakeSurfaceId(kParentFrameSink, 1);
  const SurfaceId child_id = MakeSurfaceId(kChildFrameSink1, 1);
  const SurfaceId arbitrary_id = MakeSurfaceId(kArbitraryFrameSink, 1);

  display_support().SubmitCompositorFrame(
      display_id.local_surface_id(),
      MakeCompositorFrame({parent_id}, {SurfaceRange(std::nullopt, parent_id)},
                          std::vector<TransferableResource>(),
                          MakeDefaultDeadline()));
  EXPECT_TRUE(display_surface()->HasPendingFrame());
  EXPECT_FALSE(display_surface()->HasActiveFrame());
  EXPECT_TRUE(display_surface()->has_deadline());

  // Issue some BeginFrames to trigger the deadline and activate the display's
  // surface. |parent_id| is now late. Advance BeginFrames to trigger a
  // deadline.
  for (int i = 0; i < 4; ++i) {
    EXPECT_TRUE(display_surface()->has_deadline());
    SendNextBeginFrame();
  }
  EXPECT_FALSE(display_surface()->HasPendingFrame());
  EXPECT_TRUE(display_surface()->HasActiveFrame());
  EXPECT_FALSE(display_surface()->has_deadline());

  // The child surface is not currently causally linked to the display's
  // surface and so it gets a separate deadline.
  child_support1().SubmitCompositorFrame(
      child_id.local_surface_id(),
      MakeCompositorFrame(
          {arbitrary_id}, {SurfaceRange(std::nullopt, arbitrary_id)},
          std::vector<TransferableResource>(), MakeDefaultDeadline()));
  EXPECT_TRUE(child_surface1()->HasPendingFrame());
  EXPECT_FALSE(child_surface1()->HasActiveFrame());
  EXPECT_TRUE(child_surface1()->has_deadline());

  // Submitting a CompositorFrame to the parent surface creates a dependency
  // chain from the display to the parent to the child, allowing them all to
  // assume the same deadline. Both the parent and the child are determined to
  // be late and activate immediately.
  parent_support().SubmitCompositorFrame(
      parent_id.local_surface_id(),
      MakeCompositorFrame({child_id}, {SurfaceRange(std::nullopt, child_id)},
                          std::vector<TransferableResource>(),
                          MakeDefaultDeadline()));
  EXPECT_FALSE(parent_surface()->HasPendingFrame());
  EXPECT_TRUE(parent_surface()->HasActiveFrame());
  EXPECT_FALSE(parent_surface()->has_deadline());

  EXPECT_FALSE(child_surface1()->HasPendingFrame());
  EXPECT_TRUE(child_surface1()->HasActiveFrame());
  EXPECT_FALSE(child_surface1()->has_deadline());
}

// This test verifies that CompositorFrames submitted to a surface referenced
// by a parent CompositorFrame as a fallback will be ACK'ed immediately.
TEST_P(OnBeginFrameAcksSurfaceSynchronizationTest, FallbackSurfacesClosed) {
  const SurfaceId parent_id1 = MakeSurfaceId(kParentFrameSink, 1);
  // This is the fallback child surface that the parent holds a reference to.
  const SurfaceId child_id1 = MakeSurfaceId(kChildFrameSink1, 1);
  // This is the primary child surface that the parent wants to block on.
  const SurfaceId child_id2 = MakeSurfaceId(kChildFrameSink1, 2);
  const SurfaceId arbitrary_id = MakeSurfaceId(kChildFrameSink2, 3);

  SendNextBeginFrame();

  // child_support1 submits a CompositorFrame with unresolved dependencies.
  // DidReceiveCompositorFrameAck should not be called because the frame hasn't
  // activated yet.
  EXPECT_CALL(support_client_, DidReceiveCompositorFrameAck(_)).Times(0);
  child_support1().SubmitCompositorFrame(
      child_id1.local_surface_id(),
      MakeCompositorFrame({arbitrary_id},
                          {SurfaceRange(std::nullopt, arbitrary_id)}, {},
                          MakeDefaultDeadline()));
  EXPECT_TRUE(child_surface1()->has_deadline());
  EXPECT_TRUE(child_surface1()->HasPendingFrame());
  EXPECT_FALSE(child_surface1()->HasActiveFrame());
  testing::Mock::VerifyAndClearExpectations(&support_client_);

  // The parent is blocked on |child_id2| and references |child_id1|.
  // |child_id1| should immediately activate and the ack must be sent.
  EXPECT_CALL(support_client_, DidReceiveCompositorFrameAck(_))
      .Times(BeginFrameAcksEnabled() ? 0 : 1);
  parent_support().SubmitCompositorFrame(
      parent_id1.local_surface_id(),
      MakeCompositorFrame({child_id2}, {SurfaceRange(child_id1, child_id2)},
                          std::vector<TransferableResource>(),
                          MakeDefaultDeadline()));
  EXPECT_TRUE(parent_surface()->has_deadline());
  EXPECT_TRUE(parent_surface()->HasPendingFrame());
  EXPECT_FALSE(parent_surface()->HasActiveFrame());
  EXPECT_FALSE(child_surface1()->HasPendingFrame());
  EXPECT_TRUE(child_surface1()->HasActiveFrame());
  testing::Mock::VerifyAndClearExpectations(&support_client_);

  // Any further CompositorFrames sent to |child_id1| will also activate
  // immediately so that the child can submit another frame and catch up with
  // the parent.
  //
  // When using AutoNeedsBeginFrame and OnBeginFrameAcks, the OnBeginFrame
  // associated with this feature will include the ACK, rather that a later
  // separate call.
  if (AutoNeedsBeginFrame() && BeginFrameAcksEnabled()) {
    EXPECT_CALL(support_client_, OnBeginFrame(_, _, false, _)).Times(1);
    EXPECT_CALL(support_client_, OnBeginFrame(_, _, true, _)).Times(1);
  }
  SendNextBeginFrame();
  EXPECT_CALL(support_client_, DidReceiveCompositorFrameAck(_))
      .Times((AutoNeedsBeginFrame() && BeginFrameAcksEnabled()) ? 0 : 1);
  child_support1().SubmitCompositorFrame(
      child_id1.local_surface_id(),
      MakeCompositorFrame({arbitrary_id},
                          {SurfaceRange(std::nullopt, arbitrary_id)}, {},
                          MakeDefaultDeadline()));
  EXPECT_FALSE(child_surface1()->HasPendingFrame());
  EXPECT_TRUE(child_surface1()->HasActiveFrame());
  testing::Mock::VerifyAndClearExpectations(&support_client_);
}

// This test verifies that two surface subtrees have independent deadlines.
TEST_F(SurfaceSynchronizationTest, IndependentDeadlines) {
  const SurfaceId parent_id1 = MakeSurfaceId(kParentFrameSink, 1);
  const SurfaceId child_id1 = MakeSurfaceId(kChildFrameSink1, 1);
  const SurfaceId child_id2 = MakeSurfaceId(kChildFrameSink2, 1);
  const SurfaceId arbitrary_id = MakeSurfaceId(kArbitraryFrameSink, 1);

  child_support1().SubmitCompositorFrame(
      child_id1.local_surface_id(),
      MakeDefaultInteractiveCompositorFrame(kBeginFrameSourceId));
  EXPECT_FALSE(child_surface1()->HasPendingFrame());
  EXPECT_TRUE(child_surface1()->HasActiveFrame());

  child_support2().SubmitCompositorFrame(
      child_id2.local_surface_id(),
      MakeDefaultInteractiveCompositorFrame(kBeginFrameSourceId));
  EXPECT_FALSE(child_surface2()->HasPendingFrame());
  EXPECT_TRUE(child_surface2()->HasActiveFrame());

  parent_support().SubmitCompositorFrame(
      parent_id1.local_surface_id(),
      MakeCompositorFrame({child_id1, child_id2}, empty_surface_ranges(),
                          std::vector<TransferableResource>(),
                          MakeDefaultDeadline()));

  EXPECT_FALSE(parent_surface()->HasPendingFrame());
  EXPECT_TRUE(parent_surface()->HasActiveFrame());
  EXPECT_FALSE(parent_surface()->has_deadline());

  // Submit another CompositorFrame to |child_id1| that blocks on
  // |arbitrary_id|.
  child_support1().SubmitCompositorFrame(
      child_id1.local_surface_id(),
      MakeCompositorFrame(
          {arbitrary_id}, empty_surface_ranges(),
          std::vector<TransferableResource>(),
          FrameDeadline(Now(), 3, BeginFrameArgs::DefaultInterval(), false)));
  EXPECT_TRUE(child_surface1()->HasPendingFrame());
  EXPECT_TRUE(child_surface1()->HasActiveFrame());
  EXPECT_TRUE(child_surface1()->has_deadline());

  // Advance to the next BeginFrame. |child_id1|'s pending Frame should activate
  // after 2 frames.
  SendNextBeginFrame();

  // Submit another CompositorFrame to |child_id2| that blocks on
  // |arbitrary_id|.
  child_support2().SubmitCompositorFrame(
      child_id2.local_surface_id(),
      MakeCompositorFrame({arbitrary_id}, empty_surface_ranges(),
                          std::vector<TransferableResource>(),
                          MakeDefaultDeadline()));
  EXPECT_TRUE(child_surface2()->HasPendingFrame());
  EXPECT_TRUE(child_surface2()->HasActiveFrame());
  EXPECT_TRUE(child_surface2()->has_deadline());

  // If we issue another two BeginFrames both children should remain blocked.
  SendNextBeginFrame();
  EXPECT_TRUE(child_surface1()->has_deadline());
  EXPECT_TRUE(child_surface2()->has_deadline());

  // Issuing another BeginFrame should activate the frame in |child_id1| but not
  // |child_id2|. This verifies that |child_id1| and |child_id2| have different
  // deadlines.
  SendNextBeginFrame();

  EXPECT_TRUE(child_surface2()->has_deadline());

  EXPECT_FALSE(child_surface1()->has_deadline());
  EXPECT_FALSE(child_surface1()->HasPendingFrame());
  EXPECT_TRUE(child_surface1()->HasActiveFrame());

  SendNextBeginFrame();

  EXPECT_TRUE(child_surface2()->has_deadline());
  EXPECT_TRUE(child_surface2()->HasPendingFrame());
  EXPECT_TRUE(child_surface2()->HasActiveFrame());

  // Issuing another BeginFrame should activate the frame in |child_id2|.
  SendNextBeginFrame();

  EXPECT_FALSE(child_surface2()->has_deadline());
  EXPECT_FALSE(child_surface2()->HasPendingFrame());
  EXPECT_TRUE(child_surface2()->HasActiveFrame());
}

// This test verifies that a child inherits its deadline from its dependent
// parent (embedder) if the deadline is shorter than child's deadline.
TEST_F(SurfaceSynchronizationTest, InheritShorterDeadline) {
  const SurfaceId parent_id1 = MakeSurfaceId(kParentFrameSink, 1);
  const SurfaceId child_id1 = MakeSurfaceId(kChildFrameSink1, 1);
  const SurfaceId arbitrary_id = MakeSurfaceId(kArbitraryFrameSink, 1);

  // Using the default lower bound deadline results in the deadline of 2 frames
  // effectively being ignored because the default lower bound is 4 frames.
  parent_support().SubmitCompositorFrame(
      parent_id1.local_surface_id(),
      MakeCompositorFrame(
          {child_id1}, {SurfaceRange(std::nullopt, child_id1)},
          std::vector<TransferableResource>(),
          FrameDeadline(Now(), 2, BeginFrameArgs::DefaultInterval(), true)));

  EXPECT_TRUE(parent_surface()->HasPendingFrame());
  EXPECT_FALSE(parent_surface()->HasActiveFrame());
  EXPECT_TRUE(parent_surface()->has_deadline());

  // Advance to the next BeginFrame. The parent surface will activate in 3
  // frames.
  SendNextBeginFrame();

  child_support1().SubmitCompositorFrame(
      child_id1.local_surface_id(),
      MakeCompositorFrame({arbitrary_id}, empty_surface_ranges(),
                          std::vector<TransferableResource>(),
                          MakeDefaultDeadline()));
  EXPECT_TRUE(child_surface1()->HasPendingFrame());
  EXPECT_FALSE(child_surface1()->HasActiveFrame());
  EXPECT_TRUE(child_surface1()->has_deadline());

  // If we issue another three BeginFrames then both the parent and the child
  // should activate, verifying that the child's deadline is inherited from the
  // parent.
  for (int i = 0; i < 3; ++i) {
    EXPECT_TRUE(parent_surface()->has_deadline());
    EXPECT_TRUE(child_surface1()->has_deadline());
    SendNextBeginFrame();
  }

  // Verify that both the parent and child have activated.
  EXPECT_FALSE(parent_surface()->HasPendingFrame());
  EXPECT_TRUE(parent_surface()->HasActiveFrame());
  EXPECT_FALSE(parent_surface()->has_deadline());

  EXPECT_FALSE(child_surface1()->HasPendingFrame());
  EXPECT_TRUE(child_surface1()->HasActiveFrame());
  EXPECT_FALSE(child_surface1()->has_deadline());
}

// This test verifies that in case of A embedding B embedding C, if the deadline
// of A is longer than the deadline of B, B's deadline is not extended.
TEST_F(SurfaceSynchronizationTest, ChildDeadlineNotExtendedByInheritance) {
  const SurfaceId parent_id = MakeSurfaceId(kParentFrameSink, 1);
  const SurfaceId child_id1 = MakeSurfaceId(kChildFrameSink1, 1);
  const SurfaceId child_id2 = MakeSurfaceId(kChildFrameSink2, 1);

  // Parent blocks on Child1 with a deadline of 10.
  parent_support().SubmitCompositorFrame(
      parent_id.local_surface_id(),
      MakeCompositorFrame({child_id1}, empty_surface_ranges(),
                          std::vector<TransferableResource>(),
                          MakeDeadline(10)));

  // Child1 blocks on Child2 with a deadline of 2.
  child_support1().SubmitCompositorFrame(
      child_id1.local_surface_id(),
      MakeCompositorFrame({child_id2}, {SurfaceRange(std::nullopt, child_id2)},
                          std::vector<TransferableResource>(),
                          MakeDeadline(2)));

  // Both Parent and Child1 should be blocked because Child2 doesn't exist.
  EXPECT_FALSE(parent_surface()->HasActiveFrame());
  EXPECT_TRUE(parent_surface()->HasPendingFrame());
  EXPECT_FALSE(child_surface1()->HasActiveFrame());
  EXPECT_TRUE(child_surface1()->HasPendingFrame());

  // Send one BeginFrame. Both Parent and Child1 should be still blocked because
  // Child2 still doesn't exist and Child1's deadline is 2.
  SendNextBeginFrame();
  EXPECT_FALSE(parent_surface()->HasActiveFrame());
  EXPECT_TRUE(parent_surface()->HasPendingFrame());
  EXPECT_FALSE(child_surface1()->HasActiveFrame());
  EXPECT_TRUE(child_surface1()->HasPendingFrame());

  // Send one more BeginFrame. Child1 should activate by deadline, and parent
  // will consequenctly activates. This wouldn't happen if Child1's deadline was
  // extended to match Parent's deadline.
  SendNextBeginFrame();
  EXPECT_TRUE(parent_surface()->HasActiveFrame());
  EXPECT_FALSE(parent_surface()->HasPendingFrame());
  EXPECT_TRUE(child_surface1()->HasActiveFrame());
  EXPECT_FALSE(child_surface1()->HasPendingFrame());
}

// This test verifies that all surfaces within a dependency chain will
// ultimately inherit the parent's shorter deadline even if the grandchild is
// available before the child.
TEST_F(SurfaceSynchronizationTest, MultiLevelDeadlineInheritance) {
  const SurfaceId display_id = MakeSurfaceId(kDisplayFrameSink, 1);
  const SurfaceId parent_id = MakeSurfaceId(kParentFrameSink, 1);
  const SurfaceId child_id = MakeSurfaceId(kChildFrameSink1, 1);
  const SurfaceId arbitrary_id = MakeSurfaceId(kArbitraryFrameSink, 1);

  display_support().SubmitCompositorFrame(
      display_id.local_surface_id(),
      MakeCompositorFrame({parent_id}, {SurfaceRange(std::nullopt, parent_id)},
                          std::vector<TransferableResource>(),
                          MakeDefaultDeadline()));
  EXPECT_TRUE(display_surface()->HasPendingFrame());
  EXPECT_FALSE(display_surface()->HasActiveFrame());
  EXPECT_TRUE(display_surface()->has_deadline());

  // Issue a BeginFrame to move closer to the display's deadline.
  SendNextBeginFrame();

  // The child surface is not currently causally linked to the display's
  // surface and so it gets a separate deadline.
  child_support1().SubmitCompositorFrame(
      child_id.local_surface_id(),
      MakeCompositorFrame(
          {arbitrary_id}, {SurfaceRange(std::nullopt, arbitrary_id)},
          std::vector<TransferableResource>(), MakeDefaultDeadline()));
  EXPECT_TRUE(child_surface1()->HasPendingFrame());
  EXPECT_FALSE(child_surface1()->HasActiveFrame());
  EXPECT_TRUE(child_surface1()->has_deadline());

  // Submitting a CompositorFrame to the parent frame creates a dependency
  // chain from the display to the parent to the child, allowing them all to
  // assume the same deadline.
  parent_support().SubmitCompositorFrame(
      parent_id.local_surface_id(),
      MakeCompositorFrame({child_id}, {SurfaceRange(std::nullopt, child_id)},
                          std::vector<TransferableResource>(),
                          MakeDefaultDeadline()));
  EXPECT_TRUE(parent_surface()->HasPendingFrame());
  EXPECT_FALSE(parent_surface()->HasActiveFrame());
  EXPECT_TRUE(parent_surface()->has_deadline());

  // Advancing the time by three BeginFrames should activate all the surfaces.
  for (int i = 0; i < 3; ++i) {
    EXPECT_TRUE(display_surface()->has_deadline());
    EXPECT_TRUE(parent_surface()->has_deadline());
    EXPECT_TRUE(child_surface1()->has_deadline());
    SendNextBeginFrame();
  }

  // Verify that all the CompositorFrames have activated.
  EXPECT_FALSE(display_surface()->HasPendingFrame());
  EXPECT_TRUE(display_surface()->HasActiveFrame());
  EXPECT_FALSE(display_surface()->has_deadline());

  EXPECT_FALSE(parent_surface()->HasPendingFrame());
  EXPECT_TRUE(parent_surface()->HasActiveFrame());
  EXPECT_FALSE(parent_surface()->has_deadline());

  EXPECT_FALSE(child_surface1()->HasPendingFrame());
  EXPECT_TRUE(child_surface1()->HasActiveFrame());
  EXPECT_FALSE(child_surface1()->has_deadline());
}

// This test verifies that no crash occurs if a CompositorFrame activates AFTER
// its FrameSink has been destroyed.
TEST_F(SurfaceSynchronizationTest, FrameActivationAfterFrameSinkDestruction) {
  const SurfaceId display_id = MakeSurfaceId(kDisplayFrameSink, 1);
  const SurfaceId parent_id = MakeSurfaceId(kParentFrameSink, 1);
  const SurfaceId child_id = MakeSurfaceId(kChildFrameSink1, 1);

  parent_support().SubmitCompositorFrame(
      parent_id.local_surface_id(),
      MakeDefaultInteractiveCompositorFrame(kBeginFrameSourceId));

  EXPECT_FALSE(parent_surface()->has_deadline());
  EXPECT_TRUE(parent_surface()->HasActiveFrame());
  EXPECT_FALSE(parent_surface()->HasPendingFrame());

  // Submit a CompositorFrame that refers to to |parent_id|.
  display_support().SubmitCompositorFrame(
      display_id.local_surface_id(),
      MakeCompositorFrame(empty_surface_ids(), {SurfaceRange(parent_id)},
                          std::vector<TransferableResource>()));

  EXPECT_FALSE(display_surface()->has_deadline());
  EXPECT_FALSE(display_surface()->HasPendingFrame());
  EXPECT_TRUE(display_surface()->HasActiveFrame());
  EXPECT_THAT(GetChildReferences(display_id), UnorderedElementsAre(parent_id));

  // Submit a new CompositorFrame to the parent CompositorFrameSink. It should
  // now have a pending and active CompositorFrame.
  parent_support().SubmitCompositorFrame(
      parent_id.local_surface_id(),
      MakeCompositorFrame({child_id}, empty_surface_ranges(),
                          std::vector<TransferableResource>()));

  EXPECT_TRUE(parent_surface()->has_deadline());
  EXPECT_TRUE(parent_surface()->HasActiveFrame());
  EXPECT_TRUE(parent_surface()->HasPendingFrame());
  surface_observer().Reset();

  // Destroy the parent CompositorFrameSink. The parent_surface will be kept
  // alive by the display.
  DestroyFrameSink(kParentFrameSink);

  // The parent surface stays alive through the display.
  Surface* parent_surface = GetSurfaceForId(parent_id);
  EXPECT_NE(nullptr, parent_surface);

  // Submitting a new CompositorFrame to the display should free the parent.
  display_support().SubmitCompositorFrame(
      display_id.local_surface_id(),
      MakeDefaultInteractiveCompositorFrame(kBeginFrameSourceId));

  frame_sink_manager().surface_manager()->GarbageCollectSurfaces();

  parent_surface = GetSurfaceForId(parent_id);
  EXPECT_EQ(nullptr, parent_surface);
}

TEST_F(SurfaceSynchronizationTest, PreviousFrameSurfaceId) {
  const SurfaceId parent_id1 = MakeSurfaceId(kParentFrameSink, 1);
  const SurfaceId parent_id2 = MakeSurfaceId(kParentFrameSink, 2);
  const SurfaceId child_id = MakeSurfaceId(kChildFrameSink1, 1);

  // Submit a frame with no dependencies to |parent_id1|.
  parent_support().SubmitCompositorFrame(
      parent_id1.local_surface_id(),
      MakeCompositorFrame(empty_surface_ids(), empty_surface_ranges(),
                          std::vector<TransferableResource>()));

  // Submit a frame with unresolved dependencies to |parent_id2|. The frame
  // should become pending and previous_frame_surface_id() should return
  // |parent_id1|.
  parent_support().SubmitCompositorFrame(
      parent_id2.local_surface_id(),
      MakeCompositorFrame({child_id}, empty_surface_ranges(),
                          std::vector<TransferableResource>()));
  Surface* parent_surface2 =
      frame_sink_manager().surface_manager()->GetSurfaceForId(parent_id2);
  EXPECT_FALSE(parent_surface2->HasActiveFrame());
  EXPECT_TRUE(parent_surface2->HasPendingFrame());

  // Activate the pending frame in |parent_id2|. previous_frame_surface_id()
  // should still return |parent_id1|.
  parent_surface2->ActivatePendingFrameForDeadline();
  EXPECT_TRUE(parent_surface2->HasActiveFrame());
  EXPECT_FALSE(parent_surface2->HasPendingFrame());
  EXPECT_EQ(parent_id1, parent_surface2->previous_frame_surface_id());
}

TEST_F(SurfaceSynchronizationTest, FrameIndexWithPendingFrames) {
  const SurfaceId parent_id = MakeSurfaceId(kParentFrameSink, 1);
  const SurfaceId child_id1 = MakeSurfaceId(kChildFrameSink1, 1);
  constexpr int n_iterations = 7;

  // Submit a frame with no dependencies that will activate immediately. Record
  // the initial frame index.
  parent_support().SubmitCompositorFrame(
      parent_id.local_surface_id(),
      MakeCompositorFrame(empty_surface_ids(), empty_surface_ranges(),
                          std::vector<TransferableResource>()));
  Surface* parent_surface =
      frame_sink_manager().surface_manager()->GetSurfaceForId(parent_id);
  uint64_t initial_frame_index = parent_surface->GetActiveFrameIndex();

  // Submit frames with unresolved dependencies. GetActiveFrameIndex should
  // return the same value as before.
  for (int i = 0; i < n_iterations; i++) {
    parent_support().SubmitCompositorFrame(
        parent_id.local_surface_id(),
        MakeCompositorFrame({child_id1}, empty_surface_ranges(),
                            std::vector<TransferableResource>()));
    EXPECT_EQ(initial_frame_index, parent_surface->GetActiveFrameIndex());
  }

  // Activate the pending frame. GetActiveFrameIndex should return the frame
  // index of the newly activated frame.
  parent_surface->ActivatePendingFrameForDeadline();
  EXPECT_EQ(initial_frame_index + n_iterations,
            parent_surface->GetActiveFrameIndex());
}

// This test verifies that a new surface with a pending CompositorFrame gets
// a temporary reference immediately, as opposed to when the surface activates.
TEST_F(SurfaceSynchronizationTest, PendingSurfaceKeptAlive) {
  const SurfaceId parent_id = MakeSurfaceId(kParentFrameSink, 1);
  const SurfaceId child_id1 = MakeSurfaceId(kChildFrameSink1, 1);

  // |parent_id| depends on |child_id1|. It shouldn't activate.
  parent_support().SubmitCompositorFrame(
      parent_id.local_surface_id(),
      MakeCompositorFrame({child_id1}, empty_surface_ranges(),
                          std::vector<TransferableResource>()));
  EXPECT_FALSE(parent_surface()->HasActiveFrame());
  EXPECT_TRUE(parent_surface()->HasPendingFrame());
  EXPECT_TRUE(HasTemporaryReference(parent_id));
}

// Tests getting the correct active frame index.
TEST_F(SurfaceSynchronizationTest, ActiveFrameIndex) {
  const SurfaceId parent_id = MakeSurfaceId(kParentFrameSink, 1);
  const SurfaceId child_id1 = MakeSurfaceId(kChildFrameSink1, 1);
  const SurfaceId child_id2 = MakeSurfaceId(kChildFrameSink2, 1);

  parent_support().SubmitCompositorFrame(
      parent_id.local_surface_id(),
      MakeCompositorFrame({child_id1, child_id2}, empty_surface_ranges(),
                          std::vector<TransferableResource>()));

  // parent_support is blocked on |child_id1| and |child_id2|.
  EXPECT_FALSE(parent_surface()->HasActiveFrame());
  EXPECT_EQ(0u, parent_surface()->GetActiveFrameIndex());

  child_support1().SubmitCompositorFrame(
      child_id1.local_surface_id(),
      MakeDefaultInteractiveCompositorFrame(kBeginFrameSourceId));
  child_support2().SubmitCompositorFrame(
      child_id2.local_surface_id(),
      MakeDefaultInteractiveCompositorFrame(kBeginFrameSourceId));
  EXPECT_TRUE(parent_surface()->HasActiveFrame());
  uint64_t expected_index = kFrameIndexStart;
  EXPECT_EQ(expected_index, parent_surface()->GetActiveFrameIndex());
}

// This test verifies that SurfaceManager::GetLatestInFlightSurface returns
// the latest child surface not yet set as a fallback by the parent.
// Alternatively, it returns the fallback surface specified, if no tempoary
// references to child surfaces are available. This mechanism is used by surface
// synchronization to present the freshest surfaces available at aggregation
// time.
TEST_F(SurfaceSynchronizationTest, LatestInFlightSurface) {
  const SurfaceId parent_id = MakeSurfaceId(kParentFrameSink, 1);
  const SurfaceId child_id1 = MakeSurfaceId(kChildFrameSink1, 1);
  const SurfaceId child_id2 = MakeSurfaceId(kChildFrameSink1, 2);
  const SurfaceId child_id3 = MakeSurfaceId(kChildFrameSink1, 2, 2);
  const SurfaceId child_id4 = MakeSurfaceId(kChildFrameSink1, 2, 3);

  child_support1().SubmitCompositorFrame(
      child_id1.local_surface_id(),
      MakeDefaultInteractiveCompositorFrame(kBeginFrameSourceId));

  parent_support().SubmitCompositorFrame(
      parent_id.local_surface_id(),
      MakeCompositorFrame({child_id1}, empty_surface_ranges(),
                          std::vector<TransferableResource>()));

  // Verify that the child CompositorFrame activates immediately.
  EXPECT_TRUE(child_surface1()->HasActiveFrame());
  EXPECT_FALSE(child_surface1()->HasPendingFrame());
  EXPECT_THAT(child_surface1()->activation_dependencies(), IsEmpty());

  // Verify that the parent Surface has activated.
  EXPECT_TRUE(parent_surface()->HasActiveFrame());
  EXPECT_FALSE(parent_surface()->HasPendingFrame());
  EXPECT_THAT(parent_surface()->activation_dependencies(), IsEmpty());

  // Verify that there is a temporary reference for the child and there is
  // no reference from the parent to the child yet.
  EXPECT_TRUE(HasTemporaryReference(child_id1));
  EXPECT_THAT(GetChildReferences(parent_id), IsEmpty());
  EXPECT_EQ(GetSurfaceForId(child_id1),
            GetLatestInFlightSurface(SurfaceRange(child_id1, child_id2)));

  parent_support().SubmitCompositorFrame(
      parent_id.local_surface_id(),
      MakeCompositorFrame(empty_surface_ids(), {SurfaceRange(child_id1)},
                          std::vector<TransferableResource>()));

  // Verify that the parent Surface has activated.
  EXPECT_TRUE(parent_surface()->HasActiveFrame());
  EXPECT_FALSE(parent_surface()->HasPendingFrame());
  EXPECT_THAT(parent_surface()->activation_dependencies(), IsEmpty());

  // Verify that there is no temporary reference for the child and there is
  // a reference from the parent to the child.
  EXPECT_FALSE(HasTemporaryReference(child_id1));
  EXPECT_THAT(GetChildReferences(parent_id), UnorderedElementsAre(child_id1));
  EXPECT_EQ(GetSurfaceForId(child_id1),
            GetLatestInFlightSurface(SurfaceRange(child_id1, child_id2)));

  // Submit a child CompositorFrame to a new SurfaceId and verify that
  // GetLatestInFlightSurface returns the right surface.
  child_support1().SubmitCompositorFrame(
      child_id2.local_surface_id(),
      MakeDefaultInteractiveCompositorFrame(kBeginFrameSourceId));

  // Verify that there is a temporary reference for child_id2 and there is
  // a reference from the parent to child_id1.
  EXPECT_TRUE(HasTemporaryReference(child_id2));
  EXPECT_THAT(GetChildReferences(parent_id), UnorderedElementsAre(child_id1));

  EXPECT_EQ(GetSurfaceForId(child_id2),
            GetLatestInFlightSurface(SurfaceRange(child_id1, child_id3)));

  // If the primary surface is old, then we shouldn't return an in-flight
  // surface that is newer than the primary.
  EXPECT_EQ(GetSurfaceForId(child_id1),
            GetLatestInFlightSurface(SurfaceRange(child_id1)));

  // Submit a child CompositorFrame to a new SurfaceId and verify that
  // GetLatestInFlightSurface returns the right surface.
  child_support1().SubmitCompositorFrame(
      child_id3.local_surface_id(),
      MakeDefaultInteractiveCompositorFrame(kBeginFrameSourceId));

  // Verify that there is a temporary reference for child_id3.
  EXPECT_TRUE(HasTemporaryReference(child_id3));

  EXPECT_EQ(GetSurfaceForId(child_id3),
            GetLatestInFlightSurface(SurfaceRange(child_id1, child_id4)));

  parent_support().SubmitCompositorFrame(
      parent_id.local_surface_id(),
      MakeCompositorFrame({child_id3}, {SurfaceRange(std::nullopt, child_id3)},
                          std::vector<TransferableResource>()));

  EXPECT_THAT(GetChildReferences(parent_id), UnorderedElementsAre(child_id3));

  // If the primary surface is active, we return it.
  EXPECT_EQ(GetSurfaceForId(child_id3),
            GetLatestInFlightSurface(SurfaceRange(child_id1, child_id3)));
}

// This test verifies that GetLatestInFlightSurface will return nullptr when the
// start of the range is newer than its end, even if a surface matching the end
// exists.
TEST_F(SurfaceSynchronizationTest,
       LatestInFlightSurfaceWithInvalidSurfaceRange) {
  const SurfaceId parent_id = MakeSurfaceId(kParentFrameSink, 1);
  const SurfaceId child_id1 = MakeSurfaceId(kChildFrameSink1, 1);
  const SurfaceId child_id2 = MakeSurfaceId(kChildFrameSink1, 2);

  child_support1().SubmitCompositorFrame(
      child_id1.local_surface_id(),
      MakeDefaultInteractiveCompositorFrame(kBeginFrameSourceId));

  parent_support().SubmitCompositorFrame(
      parent_id.local_surface_id(),
      MakeCompositorFrame({child_id1}, empty_surface_ranges(),
                          std::vector<TransferableResource>()));

  // Verify that the parent and child CompositorFrames are active.
  EXPECT_TRUE(child_surface1()->HasActiveFrame());
  EXPECT_FALSE(child_surface1()->HasPendingFrame());
  EXPECT_THAT(child_surface1()->activation_dependencies(), IsEmpty());

  EXPECT_TRUE(parent_surface()->HasActiveFrame());
  EXPECT_FALSE(parent_surface()->HasPendingFrame());
  EXPECT_THAT(parent_surface()->activation_dependencies(), IsEmpty());

  const SurfaceId bogus_child_id = MakeSurfaceId(kChildFrameSink1, 10);

  // The end exists but don't return it because the start is newer than the end.
  EXPECT_EQ(nullptr,
            GetLatestInFlightSurface(SurfaceRange(bogus_child_id, child_id1)));

  // In this case, the end doesn't exist either. Still return nullptr.
  EXPECT_EQ(nullptr,
            GetLatestInFlightSurface(SurfaceRange(bogus_child_id, child_id2)));
}

// This test verifies that GetLatestInFlightSurface will return the primary or
// nullptr if fallback is not specified.
// TODO(akaba): this would change after https://crbug.com/861769
TEST_F(SurfaceSynchronizationTest, LatestInFlightSurfaceWithoutFallback) {
  const SurfaceId parent_id = MakeSurfaceId(kParentFrameSink, 1);
  const SurfaceId child_id1 = MakeSurfaceId(kChildFrameSink1, 1);
  const SurfaceId child_id2 = MakeSurfaceId(kChildFrameSink1, 2);

  child_support1().SubmitCompositorFrame(
      child_id1.local_surface_id(),
      MakeDefaultInteractiveCompositorFrame(kBeginFrameSourceId));
  // Verify that |child_id1| is active.
  EXPECT_TRUE(child_surface1()->HasActiveFrame());
  EXPECT_FALSE(child_surface1()->HasPendingFrame());
  EXPECT_THAT(child_surface1()->activation_dependencies(), IsEmpty());

  parent_support().SubmitCompositorFrame(
      parent_id.local_surface_id(),
      MakeCompositorFrame({child_id2}, {SurfaceRange(child_id1, child_id2)},
                          std::vector<TransferableResource>()));

  // Verify that the |parent_id| is not active yet.
  EXPECT_FALSE(parent_surface()->HasActiveFrame());
  EXPECT_TRUE(parent_surface()->HasPendingFrame());
  EXPECT_THAT(parent_surface()->activation_dependencies(),
              UnorderedElementsAre(child_id2));

  // Verify that |child_id1| is the latest active surface.
  EXPECT_EQ(GetSurfaceForId(child_id1),
            GetLatestInFlightSurface(SurfaceRange(child_id1, child_id2)));

  // Fallback is not specified and |child_id1| is the latest.
  EXPECT_EQ(GetSurfaceForId(child_id1),
            GetLatestInFlightSurface(SurfaceRange(std::nullopt, child_id2)));

  // Activate |child_id2|
  child_support1().SubmitCompositorFrame(
      child_id2.local_surface_id(),
      MakeDefaultInteractiveCompositorFrame(kBeginFrameSourceId));
  // Verify that child2 is active.
  EXPECT_TRUE(child_surface1()->HasActiveFrame());
  EXPECT_FALSE(child_surface1()->HasPendingFrame());
  EXPECT_THAT(child_surface1()->activation_dependencies(), IsEmpty());

  // Verify that |child_id2| is the latest active surface.
  EXPECT_EQ(GetSurfaceForId(child_id2),
            GetLatestInFlightSurface(SurfaceRange(child_id1, child_id2)));

  // Fallback is not specified but primary exists so we return it.
  EXPECT_EQ(GetSurfaceForId(child_id2),
            GetLatestInFlightSurface(SurfaceRange(std::nullopt, child_id2)));
}

// This test verifies that GetLatestInFlightSurface will not return null if the
// fallback is garbage collected, but instead returns the latest surface older
// than primary if that exists.
TEST_F(SurfaceSynchronizationTest, LatestInFlightSurfaceWithGarbageFallback) {
  const SurfaceId parent_id = MakeSurfaceId(kParentFrameSink, 1);
  const SurfaceId child_id1 = MakeSurfaceId(kChildFrameSink1, 1);
  const SurfaceId child_id2 = MakeSurfaceId(kChildFrameSink1, 2);
  const SurfaceId child_id3 = MakeSurfaceId(kChildFrameSink1, 3);
  const SurfaceId child_id4 = MakeSurfaceId(kChildFrameSink1, 4);

  // Activate |child_id1|.
  child_support1().SubmitCompositorFrame(
      child_id1.local_surface_id(),
      MakeDefaultInteractiveCompositorFrame(kBeginFrameSourceId));

  // Verify that |child_id1| CompositorFrames is active.
  EXPECT_TRUE(child_surface1()->HasActiveFrame());
  EXPECT_FALSE(child_surface1()->HasPendingFrame());
  EXPECT_THAT(child_surface1()->activation_dependencies(), IsEmpty());
  EXPECT_TRUE(HasTemporaryReference(child_id1));

  parent_support().SubmitCompositorFrame(
      parent_id.local_surface_id(),
      MakeCompositorFrame(empty_surface_ids(), {SurfaceRange(child_id1)},
                          std::vector<TransferableResource>()));

  // Verify that parent is referencing |child_id1|.
  EXPECT_TRUE(parent_surface()->HasActiveFrame());
  EXPECT_FALSE(parent_surface()->HasPendingFrame());
  EXPECT_THAT(parent_surface()->activation_dependencies(), IsEmpty());
  EXPECT_THAT(parent_surface()->active_referenced_surfaces(),
              UnorderedElementsAre(child_id1));
  EXPECT_FALSE(HasTemporaryReference(child_id1));

  // Activate |child_id2|.
  child_support1().SubmitCompositorFrame(
      child_id2.local_surface_id(),
      MakeDefaultInteractiveCompositorFrame(kBeginFrameSourceId));

  // Verify that |child_id2| CompositorFrames is active and it has a temporary
  // reference.
  EXPECT_TRUE(child_surface1()->HasActiveFrame());
  EXPECT_FALSE(child_surface1()->HasPendingFrame());
  EXPECT_THAT(child_surface1()->activation_dependencies(), IsEmpty());
  EXPECT_TRUE(HasTemporaryReference(child_id2));

  // Activate |child_id3|.
  child_support1().SubmitCompositorFrame(
      child_id3.local_surface_id(),
      MakeDefaultInteractiveCompositorFrame(kBeginFrameSourceId));

  // Verify that |child_id3| CompositorFrames is active.
  EXPECT_TRUE(child_surface1()->HasActiveFrame());
  EXPECT_FALSE(child_surface1()->HasPendingFrame());
  EXPECT_THAT(child_surface1()->activation_dependencies(), IsEmpty());
  EXPECT_TRUE(HasTemporaryReference(child_id3));

  parent_support().SubmitCompositorFrame(
      parent_id.local_surface_id(),
      MakeCompositorFrame(empty_surface_ids(), {SurfaceRange(child_id2)},
                          std::vector<TransferableResource>()));

  // Verify that parent is referencing |child_id2| which lost its temporary
  // reference, but |child_id3| still has a temporary reference.
  EXPECT_TRUE(parent_surface()->HasActiveFrame());
  EXPECT_FALSE(parent_surface()->HasPendingFrame());
  EXPECT_THAT(parent_surface()->activation_dependencies(), IsEmpty());
  EXPECT_THAT(parent_surface()->active_referenced_surfaces(),
              UnorderedElementsAre(child_id2));
  EXPECT_FALSE(HasTemporaryReference(child_id2));
  EXPECT_TRUE(HasTemporaryReference(child_id3));

  // Garbage collect |child_id1|.
  frame_sink_manager().surface_manager()->GarbageCollectSurfaces();

  // Make sure |child_id1| is garbage collected.
  EXPECT_EQ(frame_sink_manager().surface_manager()->GetSurfaceForId(child_id1),
            nullptr);

  EXPECT_EQ(GetSurfaceForId(child_id3),
            GetLatestInFlightSurface(SurfaceRange(child_id1, child_id4)));
}

// This test verifies that in the case of different frame sinks
// GetLatestInFlightSurface will return the latest surface in the primary's
// FrameSinkId or the latest in the fallback's FrameSinkId if no surface exists
// in the primary's.
TEST_F(SurfaceSynchronizationTest, LatestInFlightSurfaceDifferentFrameSinkIds) {
  const SurfaceId parent_id = MakeSurfaceId(kParentFrameSink, 1);
  const SurfaceId child_id1 = MakeSurfaceId(kChildFrameSink1, 1);
  const SurfaceId child_id2 = MakeSurfaceId(kChildFrameSink1, 2);
  const SurfaceId child_id3 = MakeSurfaceId(kChildFrameSink2, 1);
  const SurfaceId child_id4 = MakeSurfaceId(kChildFrameSink2, 2);

  // Activate |child_id1|.
  child_support1().SubmitCompositorFrame(
      child_id1.local_surface_id(),
      MakeDefaultInteractiveCompositorFrame(kBeginFrameSourceId));

  // Activate |child_id2|.
  child_support1().SubmitCompositorFrame(
      child_id2.local_surface_id(),
      MakeDefaultInteractiveCompositorFrame(kBeginFrameSourceId));

  parent_support().SubmitCompositorFrame(
      parent_id.local_surface_id(),
      MakeCompositorFrame({child_id4}, {SurfaceRange(child_id1, child_id4)},
                          std::vector<TransferableResource>()));

  // Primary's frame sink id empty and |child_id2| is the latest in fallback's
  // frame sink.
  EXPECT_EQ(GetSurfaceForId(child_id2),
            GetLatestInFlightSurface(SurfaceRange(child_id1, child_id4)));

  // Activate |child_id3| which is in different frame sink.
  child_support2().SubmitCompositorFrame(
      child_id3.local_surface_id(),
      MakeDefaultInteractiveCompositorFrame(kBeginFrameSourceId));

  // |child_id3| is the latest in primary's frame sink.
  EXPECT_EQ(GetSurfaceForId(child_id3),
            GetLatestInFlightSurface(SurfaceRange(child_id1, child_id4)));
}

// This test verifies that GetLatestInFlightSurface will return the
// primary surface ID if it is in the temporary reference queue.
TEST_F(SurfaceSynchronizationTest, LatestInFlightSurfaceReturnPrimary) {
  const SurfaceId parent_id = MakeSurfaceId(kParentFrameSink, 1);
  const SurfaceId child_id1 = MakeSurfaceId(kChildFrameSink1, 1);
  const SurfaceId child_id2 = MakeSurfaceId(kChildFrameSink1, 2);
  const SurfaceId child_id3 = MakeSurfaceId(kChildFrameSink1, 3);

  child_support1().SubmitCompositorFrame(
      child_id1.local_surface_id(),
      MakeDefaultInteractiveCompositorFrame(kBeginFrameSourceId));

  // Create a reference from |parent_id| to |child_id|.
  parent_support().SubmitCompositorFrame(
      parent_id.local_surface_id(),
      MakeCompositorFrame(empty_surface_ids(), {SurfaceRange(child_id1)},
                          std::vector<TransferableResource>()));

  child_support1().SubmitCompositorFrame(
      child_id2.local_surface_id(),
      MakeDefaultInteractiveCompositorFrame(kBeginFrameSourceId));

  EXPECT_EQ(GetSurfaceForId(child_id2),
            GetLatestInFlightSurface(SurfaceRange(child_id1, child_id3)));

  child_support1().SubmitCompositorFrame(
      child_id3.local_surface_id(),
      MakeDefaultInteractiveCompositorFrame(kBeginFrameSourceId));

  // GetLatestInFlightSurface will return the primary surface ID if it's
  // available.
  EXPECT_EQ(GetSurfaceForId(child_id3),
            GetLatestInFlightSurface(SurfaceRange(child_id1, child_id3)));
}

// This test verifies that GetLatestInFlightSurface can use persistent
// references to compute the latest surface.
TEST_F(SurfaceSynchronizationTest,
       LatestInFlightSurfaceUsesPersistentReferences) {
  const SurfaceId parent_id = MakeSurfaceId(kParentFrameSink, 1);
  const SurfaceId child_id1 = MakeSurfaceId(kChildFrameSink1, 1);
  const SurfaceId child_id2 = MakeSurfaceId(kChildFrameSink1, 2);
  const SurfaceId child_id3 = MakeSurfaceId(kChildFrameSink1, 3);

  // Activate |child_id1|.
  child_support1().SubmitCompositorFrame(
      child_id1.local_surface_id(),
      MakeDefaultInteractiveCompositorFrame(kBeginFrameSourceId));

  // |child_id1| now should have a temporary reference.
  EXPECT_TRUE(HasTemporaryReference(child_id1));
  EXPECT_TRUE(surface_manager()
                  ->GetSurfacesThatReferenceChildForTesting(child_id1)
                  .empty());

  // Activate |child_id2|.
  child_support1().SubmitCompositorFrame(
      child_id2.local_surface_id(),
      MakeDefaultInteractiveCompositorFrame(kBeginFrameSourceId));

  // |child_id2| now should have a temporary reference.
  EXPECT_TRUE(HasTemporaryReference(child_id2));
  EXPECT_TRUE(surface_manager()
                  ->GetSurfacesThatReferenceChildForTesting(child_id2)
                  .empty());

  // Create a reference from |parent_id| to |child_id2|.
  parent_support().SubmitCompositorFrame(
      parent_id.local_surface_id(),
      MakeCompositorFrame(empty_surface_ids(), {SurfaceRange(child_id2)},
                          std::vector<TransferableResource>()));

  // |child_id1| have no references and can be garbage collected.
  EXPECT_FALSE(HasTemporaryReference(child_id1));
  EXPECT_TRUE(surface_manager()
                  ->GetSurfacesThatReferenceChildForTesting(child_id1)
                  .empty());

  // |child_id2| has a persistent references now.
  EXPECT_FALSE(HasTemporaryReference(child_id2));
  EXPECT_FALSE(surface_manager()
                   ->GetSurfacesThatReferenceChildForTesting(child_id2)
                   .empty());

  // Verify that GetLatestInFlightSurface returns |child_id2|.
  EXPECT_EQ(GetSurfaceForId(child_id2),
            GetLatestInFlightSurface(SurfaceRange(child_id1, child_id3)));
}

// This test verifies that GetLatestInFlightSurface will skip a surface if
// its nonce is different.
TEST_F(SurfaceSynchronizationTest, LatestInFlightSurfaceSkipDifferentNonce) {
  const SurfaceId parent_id = MakeSurfaceId(kParentFrameSink, 1);
  const base::UnguessableToken nonce1 = base::UnguessableToken::Create();
  const base::UnguessableToken nonce2 = base::UnguessableToken::Create();
  const base::UnguessableToken nonce3 = base::UnguessableToken::Create();
  const SurfaceId child_id1 =
      SurfaceId(kChildFrameSink1, LocalSurfaceId(1, nonce1));
  const SurfaceId child_id2 =
      SurfaceId(kChildFrameSink1, LocalSurfaceId(2, nonce1));
  const SurfaceId child_id3 =
      SurfaceId(kChildFrameSink1, LocalSurfaceId(3, nonce2));
  const SurfaceId child_id4 =
      SurfaceId(kChildFrameSink1, LocalSurfaceId(4, nonce2));
  const SurfaceId child_id5 =
      SurfaceId(kChildFrameSink1, LocalSurfaceId(5, nonce3));

  child_support1().SubmitCompositorFrame(
      child_id1.local_surface_id(),
      MakeDefaultInteractiveCompositorFrame(kBeginFrameSourceId));

  // Create a reference from |parent_id| to |child_id|.
  parent_support().SubmitCompositorFrame(
      parent_id.local_surface_id(),
      MakeCompositorFrame(empty_surface_ids(), {SurfaceRange(child_id1)},
                          std::vector<TransferableResource>()));

  child_support1().SubmitCompositorFrame(
      child_id2.local_surface_id(),
      MakeDefaultInteractiveCompositorFrame(kBeginFrameSourceId));

  EXPECT_EQ(GetSurfaceForId(child_id2),
            GetLatestInFlightSurface(SurfaceRange(child_id1, child_id4)));

  child_support1().SubmitCompositorFrame(
      child_id3.local_surface_id(),
      MakeDefaultInteractiveCompositorFrame(kBeginFrameSourceId));

  // GetLatestInFlightSurface will return child_id3 because the nonce
  // matches that of child_id4.
  EXPECT_EQ(GetSurfaceForId(child_id3),
            GetLatestInFlightSurface(SurfaceRange(child_id1, child_id4)));

  // GetLatestInFlightSurface will return child_id2 because the nonce
  // doesn't match |child_id1| or |child_id5|.
  EXPECT_EQ(GetSurfaceForId(child_id2),
            GetLatestInFlightSurface(SurfaceRange(child_id1, child_id5)));
}

// This test verifies that if a child submits a LocalSurfaceId newer that the
// parent's dependency, then the parent will drop its dependency and activate
// if possible. In this version of the test, parent sequence number of the
// activated surface is larger than that in the dependency, while the child
// sequence number is smaller.
TEST_F(SurfaceSynchronizationTest, DropDependenciesThatWillNeverArrive1) {
  const SurfaceId parent_id = MakeSurfaceId(kParentFrameSink, 1);
  const SurfaceId child_id11 = MakeSurfaceId(kChildFrameSink1, 1, 2);
  const SurfaceId child_id12 = MakeSurfaceId(kChildFrameSink1, 2, 1);
  const SurfaceId child_id21 = MakeSurfaceId(kChildFrameSink2, 1);

  // |parent_id| depends on { child_id11, child_id21 }. It shouldn't activate.
  parent_support().SubmitCompositorFrame(
      parent_id.local_surface_id(),
      MakeCompositorFrame({child_id11, child_id21}, empty_surface_ranges(),
                          std::vector<TransferableResource>()));
  EXPECT_FALSE(parent_surface()->HasActiveFrame());
  EXPECT_TRUE(parent_surface()->HasPendingFrame());

  // The first child submits a new CompositorFrame to |child_id12|. |parent_id|
  // no longer depends on |child_id11| because it cannot expect it to arrive.
  // However, the parent is still blocked on |child_id21|.
  child_support1().SubmitCompositorFrame(
      child_id12.local_surface_id(),
      MakeCompositorFrame(empty_surface_ids(), empty_surface_ranges(),
                          std::vector<TransferableResource>()));
  EXPECT_FALSE(parent_surface()->HasActiveFrame());
  EXPECT_TRUE(parent_surface()->HasPendingFrame());
  EXPECT_THAT(parent_surface()->activation_dependencies(),
              UnorderedElementsAre(child_id21));

  // Finally, the second child submits a frame to the remaining dependency and
  // the parent activates.
  child_support2().SubmitCompositorFrame(
      child_id21.local_surface_id(),
      MakeCompositorFrame(empty_surface_ids(), empty_surface_ranges(),
                          std::vector<TransferableResource>()));
  EXPECT_TRUE(parent_surface()->HasActiveFrame());
  EXPECT_FALSE(parent_surface()->HasPendingFrame());
  EXPECT_THAT(parent_surface()->activation_dependencies(), IsEmpty());
}

// This test verifies that if a child submits a LocalSurfaceId newer that the
// parent's dependency, then the parent will drop its dependency and activate
// if possible. In this version of the test, parent sequence number of the
// activated surface is smaller than that in the dependency, while the child
// sequence number is larger.
TEST_F(SurfaceSynchronizationTest, DropDependenciesThatWillNeverArrive2) {
  const SurfaceId parent_id = MakeSurfaceId(kParentFrameSink, 1);
  const SurfaceId child_id11 = MakeSurfaceId(kChildFrameSink1, 2, 1);
  const SurfaceId child_id12 = MakeSurfaceId(kChildFrameSink1, 1, 2);
  const SurfaceId child_id21 = MakeSurfaceId(kChildFrameSink2, 1);

  // |parent_id| depends on { child_id11, child_id21 }. It shouldn't activate.
  parent_support().SubmitCompositorFrame(
      parent_id.local_surface_id(),
      MakeCompositorFrame({child_id11, child_id21}, empty_surface_ranges(),
                          std::vector<TransferableResource>()));
  EXPECT_FALSE(parent_surface()->HasActiveFrame());
  EXPECT_TRUE(parent_surface()->HasPendingFrame());

  // The first child submits a new CompositorFrame to |child_id12|. |parent_id|
  // no longer depends on |child_id11| because it cannot expect it to arrive.
  // However, the parent is still blocked on |child_id21|.
  child_support1().SubmitCompositorFrame(
      child_id12.local_surface_id(),
      MakeCompositorFrame(empty_surface_ids(), empty_surface_ranges(),
                          std::vector<TransferableResource>()));
  EXPECT_FALSE(parent_surface()->HasActiveFrame());
  EXPECT_TRUE(parent_surface()->HasPendingFrame());
  EXPECT_THAT(parent_surface()->activation_dependencies(),
              UnorderedElementsAre(child_id21));

  // Finally, the second child submits a frame to the remaining dependency and
  // the parent activates.
  child_support2().SubmitCompositorFrame(
      child_id21.local_surface_id(),
      MakeCompositorFrame(empty_surface_ids(), empty_surface_ranges(),
                          std::vector<TransferableResource>()));
  EXPECT_TRUE(parent_surface()->HasActiveFrame());
  EXPECT_FALSE(parent_surface()->HasPendingFrame());
  EXPECT_THAT(parent_surface()->activation_dependencies(), IsEmpty());
}

// This test verifies that a surface will continue to observe a child surface
// until its dependency arrives.
TEST_F(SurfaceSynchronizationTest, ObserveDependenciesUntilArrival) {
  const SurfaceId parent_id = MakeSurfaceId(kParentFrameSink, 1);
  const SurfaceId child_id21 = MakeSurfaceId(kChildFrameSink1, 2, 1);
  const SurfaceId child_id22 = MakeSurfaceId(kChildFrameSink1, 2, 2);

  // |parent_id| depends on |child_id22|. It shouldn't activate.
  parent_support().SubmitCompositorFrame(
      parent_id.local_surface_id(),
      MakeCompositorFrame({child_id22}, empty_surface_ranges(),
                          std::vector<TransferableResource>()));
  EXPECT_FALSE(parent_surface()->HasActiveFrame());
  EXPECT_TRUE(parent_surface()->HasPendingFrame());

  // The child submits to |child_id21|. The parent should not activate.
  child_support1().SubmitCompositorFrame(
      child_id21.local_surface_id(),
      MakeCompositorFrame(empty_surface_ids(), empty_surface_ranges(),
                          std::vector<TransferableResource>()));
  EXPECT_FALSE(parent_surface()->HasActiveFrame());
  EXPECT_TRUE(parent_surface()->HasPendingFrame());
  EXPECT_THAT(parent_surface()->activation_dependencies(),
              UnorderedElementsAre(child_id22));

  // The child submits to |child_id22|. The parent should activate.
  child_support1().SubmitCompositorFrame(
      child_id22.local_surface_id(),
      MakeCompositorFrame(empty_surface_ids(), empty_surface_ranges(),
                          std::vector<TransferableResource>()));
  EXPECT_TRUE(parent_surface()->HasActiveFrame());
  EXPECT_FALSE(parent_surface()->HasPendingFrame());
  EXPECT_THAT(parent_surface()->activation_dependencies(), IsEmpty());
}

// This test verifies that we don't mark the previous active surface for
// destruction until the new surface activates.
TEST_F(SurfaceSynchronizationTest,
       MarkPreviousSurfaceForDestructionAfterActivation) {
  const SurfaceId parent_id1 = MakeSurfaceId(kParentFrameSink, 1);
  const SurfaceId parent_id2 = MakeSurfaceId(kParentFrameSink, 2);
  const SurfaceId arbitrary_id = MakeSurfaceId(kArbitraryFrameSink, 1);

  // Submit a CompositorFrame that has no dependencies.
  parent_support().SubmitCompositorFrame(
      parent_id1.local_surface_id(),
      MakeDefaultInteractiveCompositorFrame(kBeginFrameSourceId));

  // Verify that the CompositorFrame has been activated.
  Surface* parent_surface1 = GetSurfaceForId(parent_id1);
  EXPECT_TRUE(parent_surface1->HasActiveFrame());
  EXPECT_FALSE(parent_surface1->HasPendingFrame());
  EXPECT_THAT(parent_surface1->activation_dependencies(), IsEmpty());
  EXPECT_FALSE(IsMarkedForDestruction(parent_id1));

  parent_support().SubmitCompositorFrame(
      parent_id2.local_surface_id(),
      MakeCompositorFrame({arbitrary_id}, empty_surface_ranges(),
                          std::vector<TransferableResource>(),
                          MakeDefaultDeadline()));

  // Verify that the CompositorFrame to the new surface has not been activated.
  Surface* parent_surface2 = GetSurfaceForId(parent_id2);
  EXPECT_FALSE(parent_surface2->HasActiveFrame());
  EXPECT_TRUE(parent_surface2->HasPendingFrame());
  EXPECT_THAT(parent_surface2->activation_dependencies(),
              UnorderedElementsAre(arbitrary_id));
  EXPECT_FALSE(IsMarkedForDestruction(parent_id1));
  EXPECT_FALSE(IsMarkedForDestruction(parent_id2));

  // Advance BeginFrames to trigger a deadline.
  for (int i = 0; i < 3; ++i) {
    SendNextBeginFrame();
    EXPECT_TRUE(parent_surface2->has_deadline());
  }
  SendNextBeginFrame();
  EXPECT_FALSE(parent_surface2->has_deadline());

  // Verify that the CompositorFrame has been activated.
  EXPECT_TRUE(parent_surface2->HasActiveFrame());
  EXPECT_FALSE(parent_surface2->HasPendingFrame());
  EXPECT_THAT(parent_surface2->activation_dependencies(), IsEmpty());

  // Verify that the old surface is now marked for destruction.
  EXPECT_TRUE(IsMarkedForDestruction(parent_id1));
  EXPECT_FALSE(IsMarkedForDestruction(parent_id2));
}

// This test verifies that CompositorFrameSinkSupport does not refer to
// a valid but non-existant |last_activated_surface_id_|.
TEST_F(SurfaceSynchronizationTest, SetPreviousFrameSurfaceDoesntCrash) {
  const SurfaceId parent_id = MakeSurfaceId(kParentFrameSink, 1);
  const SurfaceId parent_id2 = MakeSurfaceId(kParentFrameSink, 2);
  const SurfaceId child_id1 = MakeSurfaceId(kChildFrameSink1, 1);

  // The parent CompositorFrame is not blocked on anything and so it should
  // immediately activate.
  EXPECT_FALSE(parent_support().last_activated_surface_id().is_valid());
  parent_support().SubmitCompositorFrame(
      parent_id.local_surface_id(),
      MakeDefaultInteractiveCompositorFrame(kBeginFrameSourceId));

  // Verify that the parent CompositorFrame has activated.
  EXPECT_FALSE(parent_surface()->has_deadline());
  EXPECT_TRUE(parent_surface()->HasActiveFrame());
  EXPECT_FALSE(parent_surface()->HasPendingFrame());
  EXPECT_THAT(parent_surface()->activation_dependencies(), IsEmpty());
  EXPECT_TRUE(parent_support().last_activated_surface_id().is_valid());

  // Submit another CompositorFrame to |parent_id|, but this time block it
  // on |child_id1|.
  parent_support().SubmitCompositorFrame(
      parent_id.local_surface_id(),
      MakeCompositorFrame({child_id1}, empty_surface_ranges(),
                          std::vector<TransferableResource>(),
                          MakeDefaultDeadline()));

  // Verify that the surface has both a pending and activate CompositorFrame.
  EXPECT_TRUE(parent_surface()->has_deadline());
  EXPECT_TRUE(parent_surface()->HasActiveFrame());
  EXPECT_TRUE(parent_surface()->HasPendingFrame());
  EXPECT_THAT(parent_surface()->activation_dependencies(),
              UnorderedElementsAre(child_id1));

  // Evict the activated surface in the parent_support.
  EXPECT_TRUE(parent_support().last_activated_surface_id().is_valid());
  parent_support().EvictSurface(
      parent_support().last_activated_surface_id().local_surface_id());
  EXPECT_FALSE(parent_support().last_activated_surface_id().is_valid());

  // The CompositorFrame in the evicted |parent_id| activates here because it
  // was blocked on |child_id1|.
  child_support1().SubmitCompositorFrame(
      child_id1.local_surface_id(),
      MakeDefaultInteractiveCompositorFrame(kBeginFrameSourceId));

  // parent_support will be informed of the activation of a CompositorFrame
  // associated with |parent_id|, but we clear |last_active_surface_id_| because
  // it was evicted before.
  EXPECT_FALSE(parent_support().last_activated_surface_id().is_valid());

  // Perform a garbage collection. |parent_id| should no longer exist.
  EXPECT_NE(nullptr, GetSurfaceForId(parent_id));
  ExpireAllTemporaryReferencesAndGarbageCollect();
  EXPECT_EQ(nullptr, GetSurfaceForId(parent_id));

  // This should not crash as the previous surface was cleared in
  // CompositorFrameSinkSupport.
  parent_support().SubmitCompositorFrame(
      parent_id2.local_surface_id(),
      MakeDefaultInteractiveCompositorFrame(kBeginFrameSourceId));
}

// This test verifies that when a surface activates that has the same
// FrameSinkId of the primary but its embed token doesn't match, we don't
// update the references of the parent.
TEST_F(SurfaceSynchronizationTest,
       SurfaceReferenceTracking_PrimaryEmbedTokenDoesntMatch) {
  const SurfaceId parent_id = MakeSurfaceId(kParentFrameSink, 1);
  const SurfaceId child_id1 = MakeSurfaceId(kChildFrameSink1, 1);
  const SurfaceId child_id2(
      kChildFrameSink2, LocalSurfaceId(2, base::UnguessableToken::Create()));
  const SurfaceId child_id3 = MakeSurfaceId(kChildFrameSink2, 5);

  // The parent embeds (child_id1, child_id3).
  parent_support().SubmitCompositorFrame(
      parent_id.local_surface_id(),
      MakeCompositorFrame(
          empty_surface_ids(), {SurfaceRange(child_id1, child_id3)},
          std::vector<TransferableResource>(), MakeDefaultDeadline()));

  // Verify that no references exist.
  EXPECT_THAT(GetReferencesFrom(parent_id), empty_surface_ids());

  // Activate |child_id2|.
  child_support2().SubmitCompositorFrame(
      child_id2.local_surface_id(),
      MakeDefaultInteractiveCompositorFrame(kBeginFrameSourceId));

  // Since |child_id2| has a different embed token than both primary and
  // fallback, it should not be used as a reference even if it has the same
  // FrameSinkId as the primary.
  EXPECT_THAT(GetReferencesFrom(parent_id), empty_surface_ids());

  // Activate |child_id3|.
  child_support2().SubmitCompositorFrame(
      child_id3.local_surface_id(),
      MakeDefaultInteractiveCompositorFrame(kBeginFrameSourceId));

  // Verify that a reference is acquired.
  EXPECT_THAT(GetReferencesFrom(parent_id), UnorderedElementsAre(child_id3));
}

// This test verifies that a parent referencing a SurfaceRange get updated
// whenever a child surface activates inside this range. This should also update
// the SurfaceReferences tree.
TEST_F(SurfaceSynchronizationTest,
       SurfaceReferenceTracking_NewerSurfaceUpdatesReferences) {
  const SurfaceId parent_id = MakeSurfaceId(kParentFrameSink, 1);
  const SurfaceId child_id1 = MakeSurfaceId(kChildFrameSink1, 1);
  const SurfaceId child_id2 = MakeSurfaceId(kChildFrameSink1, 2);
  const SurfaceId child_id3 = MakeSurfaceId(kChildFrameSink1, 3);
  const SurfaceId child_id4 = MakeSurfaceId(kChildFrameSink2, 1);

  parent_support().SubmitCompositorFrame(
      parent_id.local_surface_id(),
      MakeCompositorFrame(
          empty_surface_ids(), {SurfaceRange(child_id1, child_id4)},
          std::vector<TransferableResource>(), MakeDefaultDeadline()));

  // Verify that no references exist.
  EXPECT_THAT(GetReferencesFrom(parent_id), empty_surface_ids());

  // Activate |child_id1|.
  child_support1().SubmitCompositorFrame(
      child_id1.local_surface_id(),
      MakeDefaultInteractiveCompositorFrame(kBeginFrameSourceId));

  // Verify that a reference is acquired.
  EXPECT_THAT(GetReferencesFrom(parent_id), UnorderedElementsAre(child_id1));

  // Activate |child_id2|.
  child_support1().SubmitCompositorFrame(
      child_id2.local_surface_id(),
      MakeDefaultInteractiveCompositorFrame(kBeginFrameSourceId));

  // Verify that the reference is updated.
  EXPECT_THAT(GetReferencesFrom(parent_id), UnorderedElementsAre(child_id2));

  // Activate |child_id4| in a different frame sink.
  child_support2().SubmitCompositorFrame(
      child_id4.local_surface_id(),
      MakeDefaultInteractiveCompositorFrame(kBeginFrameSourceId));

  // Verify that the reference is updated.
  EXPECT_THAT(GetReferencesFrom(parent_id), UnorderedElementsAre(child_id4));

  // Activate |child_id3|.
  child_support1().SubmitCompositorFrame(
      child_id3.local_surface_id(),
      MakeDefaultInteractiveCompositorFrame(kBeginFrameSourceId));

  // Verify that the reference will not get updated since |child_id3| is in the
  // fallback's FrameSinkId.
  EXPECT_THAT(GetReferencesFrom(parent_id), UnorderedElementsAre(child_id4));
}

// This test verifies that once a frame sink become invalidated, it should
// immediately unblock all pending frames depending on that sink.
TEST_F(SurfaceSynchronizationTest,
       InvalidatedFrameSinkShouldNotBlockActivation) {
  const SurfaceId parent_id = MakeSurfaceId(kParentFrameSink, 1);
  const SurfaceId child_id1 = MakeSurfaceId(kChildFrameSink1, 1);

  // The parent submitted a frame that refers to a future child surface.
  EXPECT_FALSE(parent_support().last_activated_surface_id().is_valid());
  parent_support().SubmitCompositorFrame(
      parent_id.local_surface_id(), MakeCompositorFrame({child_id1}, {}, {}));

  // The frame is pending because the child frame hasn't be submitted yet.
  EXPECT_FALSE(parent_surface()->HasActiveFrame());
  EXPECT_TRUE(parent_surface()->HasPendingFrame());
  EXPECT_FALSE(parent_support().last_activated_surface_id().is_valid());

  // Killing the child sink should unblock the frame because it is known
  // the dependency can never fulfill.
  frame_sink_manager().InvalidateFrameSinkId(kChildFrameSink1);
  EXPECT_TRUE(parent_surface()->HasActiveFrame());
  EXPECT_FALSE(parent_surface()->HasPendingFrame());
  EXPECT_EQ(parent_id, parent_support().last_activated_surface_id());
}

TEST_F(SurfaceSynchronizationTest, EvictSurface) {
  const SurfaceId child_id1 = MakeSurfaceId(kChildFrameSink1, 1, 1);
  // Child-initiated synchronization event:
  const SurfaceId child_id2 = MakeSurfaceId(kChildFrameSink1, 1, 2);
  // Parent-initiated synchronizaton event:
  const SurfaceId child_id3 = MakeSurfaceId(kChildFrameSink1, 2, 2);

  // Submit a CompositorFrame to |child_id1|.
  child_support1().SubmitCompositorFrame(
      child_id1.local_surface_id(),
      MakeDefaultInteractiveCompositorFrame(kBeginFrameSourceId));

  // Evict |child_id1|. It should get marked for destruction immediately.
  child_support1().EvictSurface(child_id1.local_surface_id());
  EXPECT_TRUE(IsMarkedForDestruction(child_id1));

  // Submit a CompositorFrame to |child_id2|. This CompositorFrame should be
  // immediately rejected because |child_id2| has the same parent sequence
  // number as |child_id1|.
  child_support1().SubmitCompositorFrame(
      child_id2.local_surface_id(),
      MakeDefaultInteractiveCompositorFrame(kBeginFrameSourceId));
  EXPECT_EQ(nullptr, GetSurfaceForId(child_id2));

  // Submit a CompositorFrame to |child_id3|. It should not be accepted and not
  // marked for destruction.
  child_support1().SubmitCompositorFrame(
      child_id3.local_surface_id(),
      MakeDefaultInteractiveCompositorFrame(kBeginFrameSourceId));
  ASSERT_NE(nullptr, GetSurfaceForId(child_id3));
  EXPECT_FALSE(IsMarkedForDestruction(child_id3));
}

// Tests that in cases where a pending-deletion surface (surface A) is
// activated during anothother surface (surface B)'s deletion, we don't attempt
// to delete surface A twice.
TEST_F(SurfaceSynchronizationTest, SurfaceActivationDuringDeletion) {
  const SurfaceId child_id1 = MakeSurfaceId(kChildFrameSink1, 1, 1);
  // Child-initiated synchronization event:
  const SurfaceId child_id2 = MakeSurfaceId(kChildFrameSink1, 1, 2);
  const SurfaceId arbitrary_id = MakeSurfaceId(kArbitraryFrameSink, 1);

  // Submit a CompositorFrame to |child_id1|.
  child_support1().SubmitCompositorFrame(
      child_id1.local_surface_id(),
      MakeDefaultInteractiveCompositorFrame(kBeginFrameSourceId));

  // Child 1 should not yet be active.
  Surface* child_surface1 = GetSurfaceForId(child_id1);
  ASSERT_NE(nullptr, child_surface1);
  EXPECT_FALSE(child_surface1->HasPendingFrame());
  EXPECT_TRUE(child_surface1->HasActiveFrame());

  // Submit a CompositorFrame to |child_id2|.
  child_support1().SubmitCompositorFrame(
      child_id2.local_surface_id(),
      MakeCompositorFrame({arbitrary_id}, empty_surface_ranges(),
                          std::vector<TransferableResource>()));

  // Child 2 should not yet be active.
  Surface* child_surface2 = GetSurfaceForId(child_id2);
  ASSERT_NE(nullptr, child_surface2);
  EXPECT_TRUE(child_surface2->HasPendingFrame());
  EXPECT_FALSE(child_surface2->HasActiveFrame());

  // Evict |child_id2|. both surfaces should be marked for deletion.
  child_support1().EvictSurface(child_id1.local_surface_id());
  EXPECT_TRUE(IsMarkedForDestruction(child_id1));
  EXPECT_TRUE(IsMarkedForDestruction(child_id2));

  // Garbage collect to delete the above surfaces. This will activate
  // |child_id2|, which will cause it to attempt re-deletion.
  ExpireAllTemporaryReferencesAndGarbageCollect();

  // Neither should still be marked for deletion.
  EXPECT_FALSE(IsMarkedForDestruction(child_id1));
  EXPECT_FALSE(IsMarkedForDestruction(child_id2));
}

// This test verifies that if a surface is created with new embed token, a new
// allocation group is created, and once that surface is destroyed, the
// allocation group is destroyed as well.
TEST_F(SurfaceSynchronizationTest,
       AllocationGroupCreationInitiatedBySubmitter) {
  const SurfaceId surface_id = MakeSurfaceId(kChildFrameSink1, 1, 1);

  // The allocation group should not exist yet.
  EXPECT_FALSE(surface_manager()->GetAllocationGroupForSurfaceId(surface_id));

  // Submit a CompositorFrame to |child_id1|.
  child_support1().SubmitCompositorFrame(
      surface_id.local_surface_id(),
      MakeDefaultInteractiveCompositorFrame(kBeginFrameSourceId));

  // The allocation group should now exist.
  EXPECT_TRUE(surface_manager()->GetAllocationGroupForSurfaceId(surface_id));

  // Mark the surface for destruction. The allocation group should continue to
  // exist because the surface won't be actually destroyed until garbage
  // collection time.
  child_support1().EvictSurface(surface_id.local_surface_id());
  EXPECT_TRUE(surface_manager()->GetSurfaceForId(surface_id));
  EXPECT_TRUE(surface_manager()->GetAllocationGroupForSurfaceId(surface_id));

  // Now garbage-collect. Both allocation group and the surface itself will be
  // destroyed.
  surface_manager()->GarbageCollectSurfaces();
  EXPECT_FALSE(surface_manager()->GetSurfaceForId(surface_id));
  EXPECT_FALSE(surface_manager()->GetAllocationGroupForSurfaceId(surface_id));
}

// This test verifies that if a surface references another surface that has an
// embed token that was never seen before, an allocation group will be created
// for the embedded surface.
TEST_F(SurfaceSynchronizationTest, AllocationGroupCreationInitiatedByEmbedder) {
  const SurfaceId parent_id = MakeSurfaceId(kParentFrameSink, 1, 1);
  const SurfaceId child_id = MakeSurfaceId(kChildFrameSink1, 1, 1);

  // The allocation group should not exist yet.
  EXPECT_FALSE(surface_manager()->GetAllocationGroupForSurfaceId(child_id));

  // Now submit a CompositorFrame that references |child_id|. An allocation
  // group will be created for it.
  CompositorFrame frame =
      MakeCompositorFrame({}, {SurfaceRange(std::nullopt, child_id)}, {});
  parent_support().SubmitCompositorFrame(parent_id.local_surface_id(),
                                         std::move(frame));
  EXPECT_TRUE(surface_manager()->GetAllocationGroupForSurfaceId(child_id));

  // Now the parent unembeds the child surface. The allocation group for child
  // surface should be marked for destruction. However, it won't get actually
  // destroyed until garbage collection time.
  parent_support().SubmitCompositorFrame(
      parent_id.local_surface_id(),
      MakeDefaultInteractiveCompositorFrame(kBeginFrameSourceId));
  ASSERT_TRUE(surface_manager()->GetAllocationGroupForSurfaceId(child_id));
  EXPECT_TRUE(surface_manager()
                  ->GetAllocationGroupForSurfaceId(child_id)
                  ->IsReadyToDestroy());
  EXPECT_TRUE(allocation_groups_need_garbage_collection());

  // Now start garbage-collection. Note that no surface has been deleted
  // recently, but the allocation group is ready to destroy and must be
  // garbage-collected.
  surface_manager()->GarbageCollectSurfaces();
  EXPECT_FALSE(surface_manager()->GetAllocationGroupForSurfaceId(child_id));
  EXPECT_FALSE(allocation_groups_need_garbage_collection());
}

// This test verifies that if the parent embeds a SurfaceRange that has
// different embed tokens at start and end, then initially both allocation
// groups are created, but once the primary becomes available, the allocation
// group for the fallback gets destroyed.
TEST_F(SurfaceSynchronizationTest,
       FallbackAllocationGroupDestroyedAfterPrimaryAvailable) {
  const SurfaceId parent_id = MakeSurfaceId(kParentFrameSink, 1, 1);
  const SurfaceId child_id1 = MakeSurfaceId(kChildFrameSink1, 1, 1);
  const SurfaceId child_id2 = MakeSurfaceId(kChildFrameSink2, 1, 1);

  // The allocation group should not exist yet.
  EXPECT_FALSE(surface_manager()->GetAllocationGroupForSurfaceId(child_id1));
  EXPECT_FALSE(surface_manager()->GetAllocationGroupForSurfaceId(child_id2));

  // Now submit a CompositorFrame that references |child_id2| as primary and
  // |child_id1| as the fallback. An allocation group will be created for both
  // of them.
  CompositorFrame frame =
      MakeCompositorFrame({}, {SurfaceRange(child_id1, child_id2)}, {});
  parent_support().SubmitCompositorFrame(parent_id.local_surface_id(),
                                         std::move(frame));
  EXPECT_TRUE(surface_manager()->GetAllocationGroupForSurfaceId(child_id1));
  EXPECT_TRUE(surface_manager()->GetAllocationGroupForSurfaceId(child_id2));

  // Make |child_id2| available. The allocation group for |child_id1| should be
  // marked for destruction.
  EXPECT_FALSE(allocation_groups_need_garbage_collection());
  child_support2().SubmitCompositorFrame(
      child_id2.local_surface_id(),
      MakeDefaultInteractiveCompositorFrame(kBeginFrameSourceId));
  ASSERT_TRUE(surface_manager()->GetAllocationGroupForSurfaceId(child_id1));
  EXPECT_TRUE(surface_manager()
                  ->GetAllocationGroupForSurfaceId(child_id1)
                  ->IsReadyToDestroy());
  ASSERT_TRUE(surface_manager()->GetAllocationGroupForSurfaceId(child_id2));
  EXPECT_FALSE(surface_manager()
                   ->GetAllocationGroupForSurfaceId(child_id2)
                   ->IsReadyToDestroy());
  EXPECT_TRUE(allocation_groups_need_garbage_collection());

  // Initiate garbage-collection. The allocation group for |child_id1| should be
  // destroyed but the one for |child_id2| should stay alive.
  surface_manager()->GarbageCollectSurfaces();
  EXPECT_FALSE(surface_manager()->GetAllocationGroupForSurfaceId(child_id1));
  EXPECT_TRUE(surface_manager()->GetAllocationGroupForSurfaceId(child_id2));
  EXPECT_FALSE(allocation_groups_need_garbage_collection());
}

// This test verifies that if the parent embeds a SurfaceRange that has
// different embed tokens for primary and fallback and a surface already exists
// in the primary's allocation group, we don't create an allocation group for
// the fallback at all.
TEST_F(SurfaceSynchronizationTest,
       FallbackAllocationGroupNotCreatedIfPrimaryAvailable) {
  const SurfaceId parent_id = MakeSurfaceId(kParentFrameSink, 1, 1);
  const SurfaceId child_id1 = MakeSurfaceId(kChildFrameSink1, 1, 1);
  const SurfaceId child_id2 = MakeSurfaceId(kChildFrameSink2, 1, 1);

  // The allocation group should not exist yet.
  EXPECT_FALSE(surface_manager()->GetAllocationGroupForSurfaceId(child_id1));
  EXPECT_FALSE(surface_manager()->GetAllocationGroupForSurfaceId(child_id2));

  // Make |child_id2| available. An allocation group should be created for it.
  child_support2().SubmitCompositorFrame(
      child_id2.local_surface_id(),
      MakeDefaultInteractiveCompositorFrame(kBeginFrameSourceId));
  EXPECT_FALSE(surface_manager()->GetAllocationGroupForSurfaceId(child_id1));
  EXPECT_TRUE(surface_manager()->GetAllocationGroupForSurfaceId(child_id2));

  // Now submit a CompositorFrame that references |child_id2| as primary and
  // |child_id1| as the fallback. An allocation group should not be created for
  // the fallback because if the primary is available, we don't need the
  // fallback.
  CompositorFrame frame =
      MakeCompositorFrame({}, {SurfaceRange(child_id1, child_id2)}, {});
  parent_support().SubmitCompositorFrame(parent_id.local_surface_id(),
                                         std::move(frame));
  EXPECT_FALSE(surface_manager()->GetAllocationGroupForSurfaceId(child_id1));
  EXPECT_TRUE(surface_manager()->GetAllocationGroupForSurfaceId(child_id2));
}

// Verifies that the value of last active surface is correct after embed token
// changes. https://crbug.com/967012
TEST_F(SurfaceSynchronizationTest,
       CheckLastActiveSurfaceAfterEmbedTokenChange) {
  const SurfaceId parent_id1 = MakeSurfaceId(kParentFrameSink, 2);
  const SurfaceId parent_id2(
      kParentFrameSink, LocalSurfaceId(1, base::UnguessableToken::Create()));
  parent_support().SubmitCompositorFrame(
      parent_id1.local_surface_id(),
      MakeDefaultInteractiveCompositorFrame(kBeginFrameSourceId));
  Surface* parent_surface1 = GetSurfaceForId(parent_id1);
  EXPECT_TRUE(parent_surface1->HasActiveFrame());
  EXPECT_FALSE(parent_surface1->HasPendingFrame());
  parent_support().SubmitCompositorFrame(
      parent_id2.local_surface_id(),
      MakeDefaultInteractiveCompositorFrame(kBeginFrameSourceId));
  Surface* parent_surface2 = GetSurfaceForId(parent_id2);
  EXPECT_TRUE(parent_surface2->HasActiveFrame());
  EXPECT_FALSE(parent_surface2->HasPendingFrame());

  // Even though |parent_id1| has a larger sequence number, the value of
  // |last_activated_surface_id| should be |parent_id2|.
  EXPECT_EQ(parent_id2, parent_support().last_activated_surface_id());
}

// Regression test for https://crbug.com/1000868. Verify that the output of
// GetLatestInFlightSurface is correct when there is a conflict between the last
// surface in the group and the queried SurfaceRange.
TEST_F(SurfaceSynchronizationTest, LatestInFlightSurfaceConflict) {
  const SurfaceId id1 = MakeSurfaceId(kParentFrameSink, 1, 1);
  const SurfaceId id2 = MakeSurfaceId(kParentFrameSink, 2, 2);
  const SurfaceId id3 = MakeSurfaceId(kParentFrameSink, 1, 3);

  parent_support().SubmitCompositorFrame(
      id1.local_surface_id(),
      MakeDefaultInteractiveCompositorFrame(kBeginFrameSourceId));
  parent_support().SubmitCompositorFrame(
      id2.local_surface_id(),
      MakeDefaultInteractiveCompositorFrame(kBeginFrameSourceId));
  EXPECT_EQ(GetSurfaceForId(id1),
            GetLatestInFlightSurface(SurfaceRange(std::nullopt, id3)));
}

// Check that if two different SurfaceIds with the same embed token are
// embedded, viz doesn't crash. https://crbug.com/1001143
TEST_F(SurfaceSynchronizationTest,
       DuplicateAllocationGroupInActivationDependencies) {
  const SurfaceId parent_id = MakeSurfaceId(kParentFrameSink, 1);
  const SurfaceId child1_id1 = MakeSurfaceId(kChildFrameSink1, 1);
  const SurfaceId child1_id2 = MakeSurfaceId(kChildFrameSink1, 2);
  const SurfaceId child2_id1 = MakeSurfaceId(kChildFrameSink2, 1);

  // Submit a CompositorFrame to |child1_id1| embedding |child2_id1|.
  CompositorFrame child1_frame =
      CompositorFrameBuilder()
          .AddDefaultRenderPass()
          .SetActivationDependencies({child2_id1})
          .SetReferencedSurfaces({SurfaceRange(std::nullopt, child2_id1)})
          .SetIsHandlingInteraction(true)
          .Build();
  child_support1().SubmitCompositorFrame(child1_id1.local_surface_id(),
                                         std::move(child1_frame));

  // Submit a CompositorFrame to |parent_id| embedding both |child1_id1| and
  // |child1_id2|.
  CompositorFrame parent_frame =
      CompositorFrameBuilder()
          .AddDefaultRenderPass()
          .SetActivationDependencies({child1_id1, child1_id2})
          .SetReferencedSurfaces({SurfaceRange(std::nullopt, child1_id1),
                                  SurfaceRange(std::nullopt, child1_id2)})
          .SetIsHandlingInteraction(true)
          .Build();
  // This shouldn't crash.
  parent_support().SubmitCompositorFrame(parent_id.local_surface_id(),
                                         std::move(parent_frame));

  // When multiple dependencies have the same embed token, only the first one
  // should be taken into account.
  EXPECT_EQ(1u, parent_surface()->activation_dependencies().size());
  EXPECT_EQ(child1_id1, *parent_surface()->activation_dependencies().begin());
}

class SurfaceSynchronizationTestMayAlwaysAckOnActivation
    : public SurfaceSynchronizationTest,
      public testing::WithParamInterface<bool> {
 public:
  SurfaceSynchronizationTestMayAlwaysAckOnActivation();
  ~SurfaceSynchronizationTestMayAlwaysAckOnActivation() override = default;

  bool ShouldAckOnSurfaceActivationWhenInteractive() const {
    return GetParam();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

SurfaceSynchronizationTestMayAlwaysAckOnActivation::
    SurfaceSynchronizationTestMayAlwaysAckOnActivation() {
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

// Tests that when a new CompositorFrame for an Embedded Surface arrives, and is
// not immediately ACKed, that when a CompositorFrame from its Embedder arrives
// with new ActivationDependencies, that the UnACKed frame receives and ACK so
// that that client can begin frame production to satistfy the new dependencies.
// (https://crbug.com/1203804)
TEST_P(SurfaceSynchronizationTestMayAlwaysAckOnActivation,
       UnAckedSurfaceArrivesBeforeNewActivationDependencies) {
  TestSurfaceIdAllocator parent_id(kParentFrameSink);
  TestSurfaceIdAllocator child_id(kChildFrameSink1);

  parent_support().SubmitCompositorFrame(
      parent_id.local_surface_id(),
      MakeCompositorFrame({child_id}, {SurfaceRange(std::nullopt, child_id)},
                          std::vector<TransferableResource>()));
  // |parent_support| is blocked on |child_id|.
  EXPECT_TRUE(parent_surface()->has_deadline());
  EXPECT_FALSE(parent_surface()->HasActiveFrame());
  EXPECT_TRUE(parent_surface()->HasPendingFrame());
  EXPECT_THAT(parent_surface()->activation_dependencies(),
              UnorderedElementsAre(child_id));

  child_support1().SubmitCompositorFrame(
      child_id.local_surface_id(),
      MakeCompositorFrame(empty_surface_ids(), empty_surface_ranges(),
                          std::vector<TransferableResource>()));
  // |child_surface| should now be active.
  EXPECT_TRUE(child_surface1()->HasActiveFrame());
  EXPECT_FALSE(child_surface1()->HasPendingFrame());
  EXPECT_THAT(child_surface1()->activation_dependencies(), IsEmpty());
  EXPECT_FALSE(child_surface1()->HasUnackedActiveFrame());

  // |parent_surface| should now be active.
  EXPECT_TRUE(parent_surface()->HasActiveFrame());
  EXPECT_FALSE(parent_surface()->HasPendingFrame());
  EXPECT_THAT(parent_surface()->activation_dependencies(), IsEmpty());

  // We start tracking that surfaces will damage the display. This will lead to
  // frames not being immediately ACKed.
  surface_observer().set_damage_display(true);
  EXPECT_CALL(support_client_, DidReceiveCompositorFrameAck(_)).Times(0);
  // Submit second frame at the same LocalSurfaceId.
  child_support1().SubmitCompositorFrame(
      child_id.local_surface_id(),
      MakeCompositorFrame(empty_surface_ids(), empty_surface_ranges(),
                          std::vector<TransferableResource>()));
  testing::Mock::VerifyAndClearExpectations(&support_client_);
  // |child_surface| should still have an active frame, which will be the newly
  // submitted frame (it activates immediately since it has no/ dependencies).
  EXPECT_TRUE(child_surface1()->HasActiveFrame());
  EXPECT_FALSE(child_surface1()->HasPendingFrame());
  EXPECT_THAT(child_surface1()->activation_dependencies(), IsEmpty());
  // There should also be an un-acked frame, which was just submitted.
  EXPECT_TRUE(child_surface1()->HasUnackedActiveFrame());

  // Submit new |parent_surface|, with ActivationDependencies that are newer
  // than the currently unACKed |child_surface|.
  parent_id.Increment();
  child_id.Increment();

  parent_support().SubmitCompositorFrame(
      parent_id.local_surface_id(),
      MakeCompositorFrame({child_id}, {SurfaceRange(std::nullopt, child_id)},
                          std::vector<TransferableResource>()));
  // |parent_support| is blocked on |child_id2| the previous parent_surface
  // should still be active.
  EXPECT_TRUE(parent_surface()->has_deadline());
  EXPECT_FALSE(parent_surface()->HasActiveFrame());
  EXPECT_TRUE(parent_surface()->HasPendingFrame());
  EXPECT_THAT(parent_surface()->activation_dependencies(),
              UnorderedElementsAre(child_id));
  EXPECT_FALSE(child_surface1()->HasUnackedActiveFrame());

  // Submitting a new child frame for the newer dependencies should activate the
  // parent frame.
  child_support1().SubmitCompositorFrame(
      child_id.local_surface_id(),
      MakeCompositorFrame(empty_surface_ids(), empty_surface_ranges(),
                          std::vector<TransferableResource>()));
  EXPECT_TRUE(child_surface1()->HasActiveFrame());
  EXPECT_FALSE(child_surface1()->HasPendingFrame());
  EXPECT_THAT(child_surface1()->activation_dependencies(), IsEmpty());
  EXPECT_TRUE(parent_surface()->HasActiveFrame());
  EXPECT_FALSE(parent_surface()->HasPendingFrame());
}

// Tests that when we are handling interactions, that we only activate the
// non-interactive clients.
TEST_P(SurfaceSynchronizationTestMayAlwaysAckOnActivation,
       OnlyEarlyAckNonInteractiveSurface) {
  TestSurfaceIdAllocator parent_id(kParentFrameSink);
  TestSurfaceIdAllocator child_id(kChildFrameSink1);

  // We start tracking that surfaces will damage the display. This will lead to
  // frames not being immediately ACKed.
  surface_observer().set_damage_display(true);

  // The interactive Surface should never have immediate Ack
  EXPECT_CALL(support_client_, DidReceiveCompositorFrameAck(_)).Times(0);
  parent_support().SubmitCompositorFrame(
      parent_id.local_surface_id(),
      MakeCompositorFrame({child_id}, {SurfaceRange(std::nullopt, child_id)},
                          std::vector<TransferableResource>(), FrameDeadline(),
                          /*is_handling_interaction=*/true));
  testing::Mock::VerifyAndClearExpectations(&support_client_);

  // The non-interactive Surface can be immediately Acked
  if (ShouldAckOnSurfaceActivationWhenInteractive()) {
    EXPECT_CALL(support_client_, DidReceiveCompositorFrameAck(_)).Times(1);
  } else {
    EXPECT_CALL(support_client_, DidReceiveCompositorFrameAck(_)).Times(0);
  }
  child_support1().SubmitCompositorFrame(
      child_id.local_surface_id(),
      MakeCompositorFrame(empty_surface_ids(), empty_surface_ranges(),
                          std::vector<TransferableResource>(), FrameDeadline(),
                          /*is_handling_interaction=*/false));
  testing::Mock::VerifyAndClearExpectations(&support_client_);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    SurfaceSynchronizationTestMayAlwaysAckOnActivation,
    testing::Bool(),
    [](const ::testing::TestParamInfo<bool>& info) -> std::string {
      return info.param ? "AckOnSurfaceActivationWhenInteractive"
                        : "DoNotAckOnSurfaceActivationWhenInteractive";
    });

class SurfaceSynchronizationTestDrawImmediatelyWithActivationAck
    : public SurfaceSynchronizationTest {
 public:
  SurfaceSynchronizationTestDrawImmediatelyWithActivationAck() {
    scoped_feature_list_.InitWithFeatures(
        {features::kAckOnSurfaceActivationWhenInteractive,
         features::kDrawImmediatelyWhenInteractive},
        {});
  }
  ~SurfaceSynchronizationTestDrawImmediatelyWithActivationAck() override =
      default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that a frame activation causes acks when submitted for a frame prior to
// an interactive frame or within the cooldown frames, post-interaction.
TEST_F(SurfaceSynchronizationTestDrawImmediatelyWithActivationAck,
       AckOnSurfaceActivationDuringInteractionCooldown) {
  const int interactive_frame_number = 10;
  TestSurfaceIdAllocator parent_id(kParentFrameSink);
  TestSurfaceIdAllocator child_id(kChildFrameSink1);

  // Start by creating a frame with an interaction at frame 10 (interactive is
  // the default for frames created by `MakeCompositorFrame` as defined above).
  auto frame1 =
      MakeCompositorFrame({child_id}, {SurfaceRange(std::nullopt, child_id)},
                          std::vector<TransferableResource>());
  frame1.metadata.begin_frame_ack.frame_id.sequence_number =
      interactive_frame_number;

  parent_support().SubmitCompositorFrame(parent_id.local_surface_id(),
                                         std::move(frame1));

  // |parent_support| should not block on |child_id| since we're drawing
  // immediately.
  EXPECT_FALSE(parent_surface()->has_deadline());
  EXPECT_TRUE(parent_surface()->HasActiveFrame());
  EXPECT_FALSE(parent_surface()->HasPendingFrame());
  EXPECT_THAT(parent_surface()->activation_dependencies(), IsEmpty());

  // Without this, we early ack due to no damage.
  surface_observer().set_damage_display(true);

  // A child frame earlier than our last interactive frame should ack
  // immediately.
  auto frame2 = MakeCompositorFrame(empty_surface_ids(), empty_surface_ranges(),
                                    std::vector<TransferableResource>());
  frame2.metadata.begin_frame_ack.frame_id.sequence_number =
      interactive_frame_number - 1;
  frame2.metadata.is_handling_interaction = false;
  child_support1().SubmitCompositorFrame(child_id.local_surface_id(),
                                         std::move(frame2));

  // |child_surface| should now be active and should have acked.
  EXPECT_TRUE(child_surface1()->HasActiveFrame());
  EXPECT_FALSE(child_surface1()->HasPendingFrame());
  EXPECT_THAT(child_surface1()->activation_dependencies(), IsEmpty());
  EXPECT_FALSE(child_surface1()->HasUnackedActiveFrame());

  // A child frame at our interactive frame should ack immediately.
  auto frame3 = MakeCompositorFrame(empty_surface_ids(), empty_surface_ranges(),
                                    std::vector<TransferableResource>());
  frame3.metadata.begin_frame_ack.frame_id.sequence_number =
      interactive_frame_number;
  frame3.metadata.is_handling_interaction = false;
  child_support1().SubmitCompositorFrame(child_id.local_surface_id(),
                                         std::move(frame3));

  // |child_surface| should now be active and should have acked.
  EXPECT_TRUE(child_surface1()->HasActiveFrame());
  EXPECT_FALSE(child_surface1()->HasPendingFrame());
  EXPECT_THAT(child_surface1()->activation_dependencies(), IsEmpty());
  EXPECT_FALSE(child_surface1()->HasUnackedActiveFrame());

  // A child frame after our interactive frame (but within the cooldown frames)
  // should ack immediately.
  auto frame4 = MakeCompositorFrame(empty_surface_ids(), empty_surface_ranges(),
                                    std::vector<TransferableResource>());
  frame4.metadata.begin_frame_ack.frame_id.sequence_number =
      interactive_frame_number + 1;
  frame4.metadata.is_handling_interaction = false;
  child_support1().SubmitCompositorFrame(child_id.local_surface_id(),
                                         std::move(frame4));

  // |child_surface| should now be active and should have acked.
  EXPECT_TRUE(child_surface1()->HasActiveFrame());
  EXPECT_FALSE(child_surface1()->HasPendingFrame());
  EXPECT_THAT(child_surface1()->activation_dependencies(), IsEmpty());
  EXPECT_FALSE(child_surface1()->HasUnackedActiveFrame());

  // A child frame after our interactive frame and after the cooldown frames
  // should not ack immediately.
  auto frame5 = MakeCompositorFrame(empty_surface_ids(), empty_surface_ranges(),
                                    std::vector<TransferableResource>());
  std::optional<uint64_t> cooldown_frames =
      features::NumCooldownFramesForAckOnSurfaceActivationDuringInteraction();
  EXPECT_TRUE(cooldown_frames);
  frame5.metadata.begin_frame_ack.frame_id.sequence_number =
      interactive_frame_number + *cooldown_frames + 1;
  frame5.metadata.is_handling_interaction = false;
  child_support1().SubmitCompositorFrame(child_id.local_surface_id(),
                                         std::move(frame5));

  // |child_surface| should now be active but it should not have acked since it
  // falls outside the number of cooldown frames.
  EXPECT_TRUE(child_surface1()->HasActiveFrame());
  EXPECT_FALSE(child_surface1()->HasPendingFrame());
  EXPECT_THAT(child_surface1()->activation_dependencies(), IsEmpty());
  EXPECT_TRUE(child_surface1()->HasUnackedActiveFrame());
}

// Tests that when a CompositorFrame for an Embedded Surface arrives after its
// Embedder has submitted new ActivationDependencies, that it is immediately
// ACKed, even if normally it would not be due to damage. This way we don't have
// an Embedder blocked on an unACKed frame. (https://crbug.com/1203804)
TEST_P(OnBeginFrameAcksSurfaceSynchronizationTest,
       UnAckedOldActivationDependencyArrivesAfterNewDependencies) {
  TestSurfaceIdAllocator parent_id(kParentFrameSink);
  TestSurfaceIdAllocator child_id(kChildFrameSink1);

  parent_support().SubmitCompositorFrame(
      parent_id.local_surface_id(),
      MakeCompositorFrame({child_id}, {SurfaceRange(std::nullopt, child_id)},
                          std::vector<TransferableResource>()));
  // |parent_support| is blocked on |child_id|.
  EXPECT_TRUE(parent_surface()->has_deadline());
  EXPECT_FALSE(parent_surface()->HasActiveFrame());
  EXPECT_TRUE(parent_surface()->HasPendingFrame());
  EXPECT_THAT(parent_surface()->activation_dependencies(),
              UnorderedElementsAre(child_id));

  child_support1().SubmitCompositorFrame(
      child_id.local_surface_id(),
      MakeCompositorFrame(empty_surface_ids(), empty_surface_ranges(),
                          std::vector<TransferableResource>()));
  // |child_surface| should now be active.
  EXPECT_TRUE(child_surface1()->HasActiveFrame());
  EXPECT_FALSE(child_surface1()->HasPendingFrame());
  EXPECT_THAT(child_surface1()->activation_dependencies(), IsEmpty());
  EXPECT_FALSE(child_surface1()->HasUnackedActiveFrame());

  // |parent_surface| should now be active.
  EXPECT_TRUE(parent_surface()->HasActiveFrame());
  EXPECT_FALSE(parent_surface()->HasPendingFrame());
  EXPECT_THAT(parent_surface()->activation_dependencies(), IsEmpty());

  // Submit new |parent_surface|, with ActivationDependencies that are newer
  // than the currently |child_surface|.
  parent_id.Increment();
  LocalSurfaceId old_child_id = child_id.local_surface_id();
  child_id.Increment();

  EXPECT_CALL(support_client_, DidReceiveCompositorFrameAck(_)).Times(0);
  parent_support().SubmitCompositorFrame(
      parent_id.local_surface_id(),
      MakeCompositorFrame({child_id}, {SurfaceRange(std::nullopt, child_id)},
                          std::vector<TransferableResource>()));
  testing::Mock::VerifyAndClearExpectations(&support_client_);
  // |parent_support| is blocked on |child_id2| the previous |parent_surface|
  // should still be active.
  EXPECT_TRUE(parent_surface()->has_deadline());
  EXPECT_FALSE(parent_surface()->HasActiveFrame());
  EXPECT_TRUE(parent_surface()->HasPendingFrame());
  EXPECT_THAT(parent_surface()->activation_dependencies(),
              UnorderedElementsAre(child_id));
  // There should not be an unACKed |child_surface|.
  EXPECT_FALSE(child_surface1()->HasUnackedActiveFrame());

  // We start tracking that surfaces will damage the display. This will lead to
  // frames not being immediately ACKed.
  surface_observer().set_damage_display(true);
  // Submitting a CompositorFrame to the old SurfaceId, which is no longer the
  // dependency, should lead to an immediate ACK.
  EXPECT_CALL(support_client_, DidReceiveCompositorFrameAck(_))
      .Times(BeginFrameAcksEnabled() ? 0 : 1);
  child_support1().SubmitCompositorFrame(
      old_child_id,
      MakeCompositorFrame(empty_surface_ids(), empty_surface_ranges(),
                          std::vector<TransferableResource>()));
  testing::Mock::VerifyAndClearExpectations(&support_client_);
  // |child_surface| should still have an active frame.
  EXPECT_TRUE(child_surface1()->HasActiveFrame());
  EXPECT_FALSE(child_surface1()->HasPendingFrame());
  EXPECT_THAT(child_surface1()->activation_dependencies(), IsEmpty());
  // Since we are blocking our embedder, we should have been ACKed to allow for
  // frame production to begin on the new dependency.
  EXPECT_FALSE(child_surface1()->HasUnackedActiveFrame());
}

// The first boolean parameter is whether BeginFrameAcks is enabled;
// the second is whether AutoNeedsBeginFrame is enabled.
INSTANTIATE_TEST_SUITE_P(,
                         OnBeginFrameAcksSurfaceSynchronizationTest,
                         testing::Combine(testing::Bool(), testing::Bool()),
                         [](auto& info) {
                           std::string name = std::get<0>(info.param)
                                                  ? "BeginFrameAcks"
                                                  : "CompositoFrameAcks";
                           if (std::get<1>(info.param)) {
                             name += "_AutoNeedsBeginFrame";
                           }
                           return name;
                         });

}  // namespace viz
