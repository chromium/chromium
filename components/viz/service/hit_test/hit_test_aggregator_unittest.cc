// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/hit_test/hit_test_aggregator.h"

#include <map>
#include <memory>

#include "components/viz/common/hit_test/hit_test_region_list.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "components/viz/service/display_embedder/server_shared_bitmap_manager.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "components/viz/service/hit_test/hit_test_aggregator_delegate.h"
#include "components/viz/service/surfaces/surface_manager.h"
#include "components/viz/test/compositor_frame_helpers.h"
#include "components/viz/test/surface_id_allocator_set.h"
#include "components/viz/test/test_latest_local_surface_id_lookup_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace viz {
namespace {

constexpr uint32_t kDisplayClientId = 2;
constexpr FrameSinkId kDisplayFrameSink(kDisplayClientId, 0);

class TestHostFrameSinkManager : public HostFrameSinkManager {
 public:
  TestHostFrameSinkManager() = default;
  ~TestHostFrameSinkManager() override = default;

  void OnAggregatedHitTestRegionListUpdated(
      const FrameSinkId& frame_sink_id,
      const std::vector<AggregatedHitTestRegion>& hit_test_data) override {
    buffer_frame_sink_id_ = frame_sink_id;
    active_list_ = hit_test_data;
  }

  const std::vector<AggregatedHitTestRegion>& regions() { return active_list_; }

  const FrameSinkId& buffer_frame_sink_id() { return buffer_frame_sink_id_; }

 private:
  FrameSinkId buffer_frame_sink_id_;
  std::vector<AggregatedHitTestRegion> active_list_;

  DISALLOW_COPY_AND_ASSIGN(TestHostFrameSinkManager);
};

class TestFrameSinkManagerImpl : public FrameSinkManagerImpl {
 public:
  explicit TestFrameSinkManagerImpl(SharedBitmapManager* shared_bitmap_manager)
      : FrameSinkManagerImpl(shared_bitmap_manager) {}
  ~TestFrameSinkManagerImpl() override = default;

  void SetLocalClient(TestHostFrameSinkManager* client) {
    host_client_ = client;
  }

  void OnAggregatedHitTestRegionListUpdated(
      const FrameSinkId& frame_sink_id,
      const std::vector<AggregatedHitTestRegion>& hit_test_data) override {
    // Do not check if it's on valid thread for tests.
    if (host_client_) {
      host_client_->OnAggregatedHitTestRegionListUpdated(frame_sink_id,
                                                         hit_test_data);
    }
  }

 private:
  TestHostFrameSinkManager* host_client_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(TestFrameSinkManagerImpl);
};

}  // namespace

class TestHitTestAggregator final : public HitTestAggregator {
 public:
  TestHitTestAggregator(
      const HitTestManager* manager,
      HitTestAggregatorDelegate* delegate,
      LatestLocalSurfaceIdLookupDelegate* local_surface_id_lookup_delegate,
      const FrameSinkId& frame_sink_id)
      : HitTestAggregator(manager,
                          delegate,
                          local_surface_id_lookup_delegate,
                          frame_sink_id),
        frame_sink_id_(frame_sink_id) {}
  ~TestHitTestAggregator() = default;

  int GetRegionCount() const { return hit_test_data_size_; }
  int GetHitTestRegionListCapacity() { return hit_test_data_capacity_; }
  uint64_t GetLastSubmitHitTestRegionListIndex() const {
    return last_submit_hit_test_region_list_index_;
  }

 private:
  const FrameSinkId frame_sink_id_;
};

class HitTestAggregatorTest : public testing::Test {
 public:
  HitTestAggregatorTest() = default;
  ~HitTestAggregatorTest() override = default;

  // testing::Test:
  void SetUp() override {
    frame_sink_manager_ =
        std::make_unique<TestFrameSinkManagerImpl>(&shared_bitmap_manager_);
    host_frame_sink_manager_ = std::make_unique<TestHostFrameSinkManager>();
    local_surface_id_lookup_delegate_ =
        std::make_unique<TestLatestLocalSurfaceIdLookupDelegate>();
    frame_sink_manager_->SetLocalClient(host_frame_sink_manager_.get());
    support_ = std::make_unique<CompositorFrameSinkSupport>(
        nullptr /* client */, frame_sink_manager_.get(), kDisplayFrameSink,
        true /* is_root */, false /* needs_sync_points */);
    hit_test_aggregator_ = std::make_unique<TestHitTestAggregator>(
        hit_test_manager(), frame_sink_manager(),
        local_surface_id_lookup_delegate(), kDisplayFrameSink);
  }
  void TearDown() override {
    support_.reset();
    frame_sink_manager_.reset();
    host_frame_sink_manager_.reset();
  }

  void ExpireAllTemporaryReferencesAndGarbageCollect() {
    frame_sink_manager_->surface_manager()->ExpireOldTemporaryReferences();
    frame_sink_manager_->surface_manager()->ExpireOldTemporaryReferences();
    frame_sink_manager_->surface_manager()->GarbageCollectSurfaces();
  }

  // Creates a hit test data element with 8 children recursively to
  // the specified depth.  SurfaceIds are generated in sequential order and
  // the method returns the next unused id.
  int CreateAndSubmitHitTestRegionListWith8Children(uint32_t client_id,
                                                    int depth) {
    SurfaceId surface_id = MakeSurfaceId(client_id);
    client_id++;

    HitTestRegionList hit_test_region_list;
    hit_test_region_list.flags = HitTestRegionFlags::kHitTestMine;
    hit_test_region_list.bounds.SetRect(0, 0, 1024, 768);

    for (int i = 0; i < 8; i++) {
      HitTestRegion hit_test_region;
      hit_test_region.rect.SetRect(100, 100, 100, 100);
      SurfaceId child_surface_id = MakeSurfaceId(client_id);
      hit_test_region.frame_sink_id = child_surface_id.frame_sink_id();

      if (depth > 0) {
        hit_test_region.flags = HitTestRegionFlags::kHitTestChildSurface;
        client_id =
            CreateAndSubmitHitTestRegionListWith8Children(client_id, depth - 1);
      } else {
        hit_test_region.flags = HitTestRegionFlags::kHitTestMine;
      }
      hit_test_region_list.regions.push_back(std::move(hit_test_region));
    }

    if (surface_id.frame_sink_id() == kDisplayFrameSink) {
      support()->SubmitCompositorFrame(surface_id.local_surface_id(),
                                       MakeDefaultCompositorFrame(),
                                       std::move(hit_test_region_list));
      local_surface_id_lookup_delegate()->SetSurfaceIdMap(surface_id);
    } else {
      auto support = std::make_unique<CompositorFrameSinkSupport>(
          nullptr, frame_sink_manager(), surface_id.frame_sink_id(),
          false /* is_root */, false /* needs_sync_points */);
      support->SubmitCompositorFrame(surface_id.local_surface_id(),
                                     MakeDefaultCompositorFrame(),
                                     std::move(hit_test_region_list));
      local_surface_id_lookup_delegate()->SetSurfaceIdMap(surface_id);
    }
    return client_id;
  }

  SurfaceId MakeSurfaceId(uint32_t frame_sink_id_client_id) {
    return allocator_set_.MakeSurfaceId(FrameSinkId(frame_sink_id_client_id, 0),
                                        1);
  }

 protected:
  TestHitTestAggregator* hit_test_aggregator() {
    return hit_test_aggregator_.get();
  }

  const std::vector<AggregatedHitTestRegion>& host_regions() {
    return host_frame_sink_manager_->regions();
  }

  const FrameSinkId& host_buffer_frame_sink_id() {
    return host_frame_sink_manager_->buffer_frame_sink_id();
  }

  const HitTestManager* hit_test_manager() const {
    return frame_sink_manager_->hit_test_manager();
  }

  CompositorFrameSinkSupport* support() const { return support_.get(); }

  FrameSinkManagerImpl* frame_sink_manager() const {
    return frame_sink_manager_.get();
  }

  SurfaceManager* surface_manager() const {
    return frame_sink_manager_->surface_manager();
  }

  TestLatestLocalSurfaceIdLookupDelegate* local_surface_id_lookup_delegate()
      const {
    return local_surface_id_lookup_delegate_.get();
  }

 private:
  ServerSharedBitmapManager shared_bitmap_manager_;
  std::unique_ptr<TestHitTestAggregator> hit_test_aggregator_;
  std::unique_ptr<TestFrameSinkManagerImpl> frame_sink_manager_;
  std::unique_ptr<TestHostFrameSinkManager> host_frame_sink_manager_;
  std::unique_ptr<TestLatestLocalSurfaceIdLookupDelegate>
      local_surface_id_lookup_delegate_;
  std::unique_ptr<CompositorFrameSinkSupport> support_;
  SurfaceIdAllocatorSet allocator_set_;

  DISALLOW_COPY_AND_ASSIGN(HitTestAggregatorTest);
};

// TODO(gklassen): Add tests for 3D use cases as suggested by and with
// input from rjkroege.

// One surface.
//
//  +----------+
//  |          |
//  |          |
//  |          |
//  +----------+
//
TEST_F(HitTestAggregatorTest, OneSurface) {
  TestHitTestAggregator* aggregator = hit_test_aggregator();
  EXPECT_EQ(aggregator->GetRegionCount(), 0);

  SurfaceId display_surface_id = MakeSurfaceId(kDisplayClientId);

  HitTestRegionList hit_test_region_list;
  hit_test_region_list.flags = HitTestRegionFlags::kHitTestMine;
  hit_test_region_list.bounds.SetRect(0, 0, 1024, 768);

  support()->SubmitCompositorFrame(display_surface_id.local_surface_id(),
                                   MakeDefaultCompositorFrame(),
                                   std::move(hit_test_region_list));
  local_surface_id_lookup_delegate()->SetSurfaceIdMap(display_surface_id);
  aggregator->Aggregate(display_surface_id);

  // Expect 1 entry routing all events to the one surface (display root).
  EXPECT_EQ(aggregator->GetRegionCount(), 1);

  EXPECT_EQ(host_buffer_frame_sink_id(), kDisplayFrameSink);

  AggregatedHitTestRegion region = host_regions()[0];
  EXPECT_EQ(region.flags, HitTestRegionFlags::kHitTestMine);
  EXPECT_EQ(region.frame_sink_id, display_surface_id.frame_sink_id());
  EXPECT_EQ(region.rect, gfx::Rect(0, 0, 1024, 768));
  EXPECT_EQ(region.child_count, 0);
}

// One opaque embedder with two regions.
//
//  +e-------------+
//  | +r1-+ +r2--+ |
//  | |   | |    | |
//  | |   | |    | |
//  | +---+ +----+ |
//  +--------------+
//
TEST_F(HitTestAggregatorTest, OneEmbedderTwoRegions) {
  TestHitTestAggregator* aggregator = hit_test_aggregator();
  EXPECT_EQ(aggregator->GetRegionCount(), 0);

  SurfaceId e_surface_id = MakeSurfaceId(kDisplayClientId);

  HitTestRegionList e_hit_test_region_list;
  e_hit_test_region_list.flags = HitTestRegionFlags::kHitTestMine;
  e_hit_test_region_list.bounds.SetRect(0, 0, 1024, 768);

  HitTestRegion e_hit_test_region_r1;
  e_hit_test_region_r1.frame_sink_id = e_surface_id.frame_sink_id();
  e_hit_test_region_r1.flags = HitTestRegionFlags::kHitTestMine;
  e_hit_test_region_r1.rect.SetRect(100, 100, 200, 400);

  HitTestRegion e_hit_test_region_r2;
  e_hit_test_region_r2.frame_sink_id = e_surface_id.frame_sink_id();
  e_hit_test_region_r2.flags = HitTestRegionFlags::kHitTestMine;
  e_hit_test_region_r2.rect.SetRect(400, 100, 300, 400);

  e_hit_test_region_list.regions.push_back(std::move(e_hit_test_region_r1));
  e_hit_test_region_list.regions.push_back(std::move(e_hit_test_region_r2));

  support()->SubmitCompositorFrame(e_surface_id.local_surface_id(),
                                   MakeDefaultCompositorFrame(),
                                   std::move(e_hit_test_region_list));
  local_surface_id_lookup_delegate()->SetSurfaceIdMap(e_surface_id);

  aggregator->Aggregate(e_surface_id);

  EXPECT_EQ(aggregator->GetRegionCount(), 3);

  EXPECT_EQ(host_buffer_frame_sink_id(), kDisplayFrameSink);

  AggregatedHitTestRegion region = host_regions()[0];
  EXPECT_EQ(region.flags, HitTestRegionFlags::kHitTestMine);
  EXPECT_EQ(region.frame_sink_id, e_surface_id.frame_sink_id());
  EXPECT_EQ(region.rect, gfx::Rect(0, 0, 1024, 768));
  EXPECT_EQ(region.child_count, 2);

  region = host_regions()[1];
  EXPECT_EQ(region.flags, HitTestRegionFlags::kHitTestMine);
  EXPECT_EQ(region.rect, gfx::Rect(100, 100, 200, 400));
  EXPECT_EQ(region.child_count, 0);

  region = host_regions()[2];
  EXPECT_EQ(region.flags, HitTestRegionFlags::kHitTestMine);
  EXPECT_EQ(region.rect, gfx::Rect(400, 100, 300, 400));
  EXPECT_EQ(region.child_count, 0);
}

// One embedder with two children.
//
//  +e-------------+
//  | +c1-+ +c2--+ |
//  | |   | |    | |
//  | |   | |    | |
//  | +---+ +----+ |
//  +--------------+
//

TEST_F(HitTestAggregatorTest, OneEmbedderTwoChildren) {
  TestHitTestAggregator* aggregator = hit_test_aggregator();
  EXPECT_EQ(aggregator->GetRegionCount(), 0);

  SurfaceId e_surface_id = MakeSurfaceId(kDisplayClientId);
  SurfaceId c1_surface_id = MakeSurfaceId(kDisplayClientId + 1);
  SurfaceId c2_surface_id = MakeSurfaceId(kDisplayClientId + 2);

  HitTestRegionList e_hit_test_region_list;
  e_hit_test_region_list.flags = HitTestRegionFlags::kHitTestMine;
  e_hit_test_region_list.bounds.SetRect(0, 0, 1024, 768);

  HitTestRegion e_hit_test_region_c1;
  e_hit_test_region_c1.flags = HitTestRegionFlags::kHitTestChildSurface;
  e_hit_test_region_c1.frame_sink_id = c1_surface_id.frame_sink_id();
  e_hit_test_region_c1.rect.SetRect(100, 100, 200, 300);

  HitTestRegion e_hit_test_region_c2;
  e_hit_test_region_c2.flags = HitTestRegionFlags::kHitTestChildSurface;
  e_hit_test_region_c2.frame_sink_id = c2_surface_id.frame_sink_id();
  e_hit_test_region_c2.rect.SetRect(400, 100, 400, 300);

  e_hit_test_region_list.regions.push_back(std::move(e_hit_test_region_c1));
  e_hit_test_region_list.regions.push_back(std::move(e_hit_test_region_c2));

  HitTestRegionList c1_hit_test_region_list;

  HitTestRegionList c2_hit_test_region_list;

  // Submit in unexpected order.

  auto support2 = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, frame_sink_manager(), c1_surface_id.frame_sink_id(),
      false /* is_root */, false /* needs_sync_points */);
  support2->SubmitCompositorFrame(c1_surface_id.local_surface_id(),
                                  MakeDefaultCompositorFrame(),
                                  std::move(c1_hit_test_region_list));
  local_surface_id_lookup_delegate()->SetSurfaceIdMap(c1_surface_id);
  support()->SubmitCompositorFrame(e_surface_id.local_surface_id(),
                                   MakeDefaultCompositorFrame(),
                                   std::move(e_hit_test_region_list));
  local_surface_id_lookup_delegate()->SetSurfaceIdMap(e_surface_id);
  auto support3 = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, frame_sink_manager(), c2_surface_id.frame_sink_id(),
      false /* is_root */, false /* needs_sync_points */);
  support3->SubmitCompositorFrame(c2_surface_id.local_surface_id(),
                                  MakeDefaultCompositorFrame(),
                                  std::move(c2_hit_test_region_list));
  local_surface_id_lookup_delegate()->SetSurfaceIdMap(c2_surface_id);

  aggregator->Aggregate(e_surface_id);

  EXPECT_EQ(aggregator->GetRegionCount(), 3);

  EXPECT_EQ(host_buffer_frame_sink_id(), kDisplayFrameSink);

  AggregatedHitTestRegion region = host_regions()[0];
  EXPECT_EQ(region.flags, HitTestRegionFlags::kHitTestMine);
  EXPECT_EQ(region.frame_sink_id, e_surface_id.frame_sink_id());
  EXPECT_EQ(region.rect, gfx::Rect(0, 0, 1024, 768));
  EXPECT_EQ(region.child_count, 2);

  region = host_regions()[1];
  EXPECT_EQ(region.flags, HitTestRegionFlags::kHitTestChildSurface);
  EXPECT_EQ(region.frame_sink_id, c1_surface_id.frame_sink_id());
  EXPECT_EQ(region.rect, gfx::Rect(100, 100, 200, 300));
  EXPECT_EQ(region.child_count, 0);

  region = host_regions()[2];
  EXPECT_EQ(region.flags, HitTestRegionFlags::kHitTestChildSurface);
  EXPECT_EQ(region.frame_sink_id, c2_surface_id.frame_sink_id());
  EXPECT_EQ(region.rect, gfx::Rect(400, 100, 400, 300));
  EXPECT_EQ(region.child_count, 0);
}

// Occluded child frame (OOPIF).
//
//  +e-----------+
//  | +c--+      |
//  | | +div-+   |
//  | | |    |   |
//  | | +----+   |
//  | +---+      |
//  +------------+
//

TEST_F(HitTestAggregatorTest, OccludedChildFrame) {
  TestHitTestAggregator* aggregator = hit_test_aggregator();
  EXPECT_EQ(aggregator->GetRegionCount(), 0);

  SurfaceId e_surface_id = MakeSurfaceId(kDisplayClientId);
  SurfaceId c_surface_id = MakeSurfaceId(kDisplayClientId + 1);

  HitTestRegionList e_hit_test_region_list;
  e_hit_test_region_list.flags = HitTestRegionFlags::kHitTestMine;
  e_hit_test_region_list.bounds.SetRect(0, 0, 1024, 768);

  HitTestRegion e_hit_test_region_div;
  e_hit_test_region_div.flags = HitTestRegionFlags::kHitTestMine;
  e_hit_test_region_div.frame_sink_id = e_surface_id.frame_sink_id();
  e_hit_test_region_div.rect.SetRect(200, 200, 300, 200);

  HitTestRegion e_hit_test_region_c;
  e_hit_test_region_c.flags = HitTestRegionFlags::kHitTestChildSurface;
  e_hit_test_region_c.frame_sink_id = c_surface_id.frame_sink_id();
  e_hit_test_region_c.rect.SetRect(100, 100, 200, 500);

  e_hit_test_region_list.regions.push_back(std::move(e_hit_test_region_div));
  e_hit_test_region_list.regions.push_back(std::move(e_hit_test_region_c));

  HitTestRegionList c_hit_test_region_list;
  c_hit_test_region_list.flags = HitTestRegionFlags::kHitTestMine;
  c_hit_test_region_list.bounds.SetRect(0, 0, 200, 500);

  // Submit in unexpected order.
  auto support2 = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, frame_sink_manager(), c_surface_id.frame_sink_id(),
      false /* is_root */, false /* needs_sync_points */);
  support2->SubmitCompositorFrame(c_surface_id.local_surface_id(),
                                  MakeDefaultCompositorFrame(),
                                  std::move(c_hit_test_region_list));
  local_surface_id_lookup_delegate()->SetSurfaceIdMap(c_surface_id);
  support()->SubmitCompositorFrame(e_surface_id.local_surface_id(),
                                   MakeDefaultCompositorFrame(),
                                   std::move(e_hit_test_region_list));
  local_surface_id_lookup_delegate()->SetSurfaceIdMap(e_surface_id);

  aggregator->Aggregate(e_surface_id);

  EXPECT_EQ(aggregator->GetRegionCount(), 3);

  EXPECT_EQ(host_buffer_frame_sink_id(), kDisplayFrameSink);

  AggregatedHitTestRegion region = host_regions()[0];
  EXPECT_EQ(region.flags, HitTestRegionFlags::kHitTestMine);
  EXPECT_EQ(region.frame_sink_id, e_surface_id.frame_sink_id());
  EXPECT_EQ(region.rect, gfx::Rect(0, 0, 1024, 768));
  EXPECT_EQ(region.child_count, 2);

  region = host_regions()[1];
  EXPECT_EQ(region.flags, HitTestRegionFlags::kHitTestMine);
  EXPECT_EQ(region.frame_sink_id, e_surface_id.frame_sink_id());
  EXPECT_EQ(region.rect, gfx::Rect(200, 200, 300, 200));
  EXPECT_EQ(region.child_count, 0);

  region = host_regions()[2];
  EXPECT_EQ(HitTestRegionFlags::kHitTestChildSurface |
                HitTestRegionFlags::kHitTestMine,
            region.flags);
  EXPECT_EQ(region.frame_sink_id, c_surface_id.frame_sink_id());
  EXPECT_EQ(region.rect, gfx::Rect(100, 100, 200, 500));
  EXPECT_EQ(region.child_count, 0);
}

// Foreground child frame (OOPIF).
// Same as the previous test except the child is foreground.
//
//  +e-----------+
//  | +c--+      |
//  | |   |div-+ |
//  | |   |    | |
//  | |   |----+ |
//  | +---+      |
//  +------------+
//

TEST_F(HitTestAggregatorTest, ForegroundChildFrame) {
  TestHitTestAggregator* aggregator = hit_test_aggregator();
  EXPECT_EQ(aggregator->GetRegionCount(), 0);

  SurfaceId e_surface_id = MakeSurfaceId(kDisplayClientId);
  SurfaceId c_surface_id = MakeSurfaceId(kDisplayClientId + 1);

  HitTestRegionList e_hit_test_region_list;
  e_hit_test_region_list.flags = HitTestRegionFlags::kHitTestMine;
  e_hit_test_region_list.bounds.SetRect(0, 0, 1024, 768);

  HitTestRegion e_hit_test_region_div;
  e_hit_test_region_div.flags = HitTestRegionFlags::kHitTestMine;
  e_hit_test_region_div.frame_sink_id = e_surface_id.frame_sink_id();
  e_hit_test_region_div.rect.SetRect(200, 200, 300, 200);

  HitTestRegion e_hit_test_region_c;
  e_hit_test_region_c.flags = HitTestRegionFlags::kHitTestChildSurface;
  e_hit_test_region_c.frame_sink_id = c_surface_id.frame_sink_id();
  e_hit_test_region_c.rect.SetRect(100, 100, 200, 500);

  e_hit_test_region_list.regions.push_back(std::move(e_hit_test_region_c));
  e_hit_test_region_list.regions.push_back(std::move(e_hit_test_region_div));

  HitTestRegionList c_hit_test_region_list;
  c_hit_test_region_list.flags = HitTestRegionFlags::kHitTestMine;
  c_hit_test_region_list.bounds.SetRect(0, 0, 200, 500);

  // Submit in unexpected order.

  auto support2 = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, frame_sink_manager(), c_surface_id.frame_sink_id(),
      false /* is_root */, false /* needs_sync_points */);
  support2->SubmitCompositorFrame(c_surface_id.local_surface_id(),
                                  MakeDefaultCompositorFrame(),
                                  std::move(c_hit_test_region_list));
  local_surface_id_lookup_delegate()->SetSurfaceIdMap(c_surface_id);
  support()->SubmitCompositorFrame(e_surface_id.local_surface_id(),
                                   MakeDefaultCompositorFrame(),
                                   std::move(e_hit_test_region_list));
  local_surface_id_lookup_delegate()->SetSurfaceIdMap(e_surface_id);

  aggregator->Aggregate(e_surface_id);

  EXPECT_EQ(aggregator->GetRegionCount(), 3);

  EXPECT_EQ(host_buffer_frame_sink_id(), kDisplayFrameSink);

  AggregatedHitTestRegion region = host_regions()[0];
  EXPECT_EQ(region.flags, HitTestRegionFlags::kHitTestMine);
  EXPECT_EQ(region.frame_sink_id, e_surface_id.frame_sink_id());
  EXPECT_EQ(region.rect, gfx::Rect(0, 0, 1024, 768));
  EXPECT_EQ(region.child_count, 2);

  region = host_regions()[1];
  EXPECT_EQ(HitTestRegionFlags::kHitTestChildSurface |
                HitTestRegionFlags::kHitTestMine,
            region.flags);
  EXPECT_EQ(region.frame_sink_id, c_surface_id.frame_sink_id());
  EXPECT_EQ(region.rect, gfx::Rect(100, 100, 200, 500));
  EXPECT_EQ(region.child_count, 0);

  region = host_regions()[2];
  EXPECT_EQ(region.flags, HitTestRegionFlags::kHitTestMine);
  EXPECT_EQ(region.frame_sink_id, e_surface_id.frame_sink_id());
  EXPECT_EQ(region.rect, gfx::Rect(200, 200, 300, 200));
  EXPECT_EQ(region.child_count, 0);
}

// One embedder with a clipped child with a tab and transparent background.
//
//  +e-------------+
//  |   +c---------|     Point   maps to
//  | 1 |+a--+     |     -----   -------
//  |   || 2 |  3  |       1        e
//  |   |+b--------|       2        a
//  |   ||         |       3        e (transparent area in c)
//  |   ||   4     |       4        b
//  +--------------+
//

TEST_F(HitTestAggregatorTest, ClippedChildWithTabAndTransparentBackground) {
  TestHitTestAggregator* aggregator = hit_test_aggregator();
  EXPECT_EQ(aggregator->GetRegionCount(), 0);

  SurfaceId e_surface_id = MakeSurfaceId(kDisplayClientId);
  SurfaceId c_surface_id = MakeSurfaceId(kDisplayClientId + 1);
  SurfaceId a_surface_id = MakeSurfaceId(kDisplayClientId + 2);
  SurfaceId b_surface_id = MakeSurfaceId(kDisplayClientId + 3);

  HitTestRegionList e_hit_test_region_list;
  e_hit_test_region_list.flags = HitTestRegionFlags::kHitTestMine;
  e_hit_test_region_list.bounds.SetRect(0, 0, 1024, 768);

  HitTestRegion e_hit_test_region_c;
  e_hit_test_region_c.flags = HitTestRegionFlags::kHitTestChildSurface;
  e_hit_test_region_c.frame_sink_id = c_surface_id.frame_sink_id();
  e_hit_test_region_c.rect.SetRect(300, 100, 1600, 800);
  e_hit_test_region_c.transform.Translate(200, 100);

  e_hit_test_region_list.regions.push_back(std::move(e_hit_test_region_c));

  HitTestRegionList c_hit_test_region_list;
  c_hit_test_region_list.flags = HitTestRegionFlags::kHitTestIgnore;
  c_hit_test_region_list.bounds.SetRect(0, 0, 1600, 800);

  HitTestRegion c_hit_test_region_a;
  c_hit_test_region_a.flags = HitTestRegionFlags::kHitTestChildSurface;
  c_hit_test_region_a.frame_sink_id = a_surface_id.frame_sink_id();
  c_hit_test_region_a.rect.SetRect(0, 0, 200, 100);

  HitTestRegion c_hit_test_region_b;
  c_hit_test_region_b.flags = HitTestRegionFlags::kHitTestChildSurface;
  c_hit_test_region_b.frame_sink_id = b_surface_id.frame_sink_id();
  c_hit_test_region_b.rect.SetRect(0, 100, 800, 600);

  c_hit_test_region_list.regions.push_back(std::move(c_hit_test_region_a));
  c_hit_test_region_list.regions.push_back(std::move(c_hit_test_region_b));

  HitTestRegionList a_hit_test_region_list;
  a_hit_test_region_list.flags = HitTestRegionFlags::kHitTestMine;
  a_hit_test_region_list.bounds.SetRect(0, 0, 200, 100);

  HitTestRegionList b_hit_test_region_list;
  b_hit_test_region_list.flags = HitTestRegionFlags::kHitTestMine;
  b_hit_test_region_list.bounds.SetRect(0, 100, 800, 600);

  // Submit in unexpected order.

  auto support2 = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, frame_sink_manager(), c_surface_id.frame_sink_id(),
      false /* is_root */, false /* needs_sync_points */);
  support2->SubmitCompositorFrame(c_surface_id.local_surface_id(),
                                  MakeDefaultCompositorFrame(),
                                  std::move(c_hit_test_region_list));
  local_surface_id_lookup_delegate()->SetSurfaceIdMap(c_surface_id);
  auto support3 = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, frame_sink_manager(), a_surface_id.frame_sink_id(),
      false /* is_root */, false /* needs_sync_points */);
  support3->SubmitCompositorFrame(a_surface_id.local_surface_id(),
                                  MakeDefaultCompositorFrame(),
                                  std::move(a_hit_test_region_list));
  local_surface_id_lookup_delegate()->SetSurfaceIdMap(a_surface_id);
  auto support4 = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, frame_sink_manager(), b_surface_id.frame_sink_id(),
      false /* is_root */, false /* needs_sync_points */);
  support4->SubmitCompositorFrame(b_surface_id.local_surface_id(),
                                  MakeDefaultCompositorFrame(),
                                  std::move(b_hit_test_region_list));
  local_surface_id_lookup_delegate()->SetSurfaceIdMap(b_surface_id);
  support()->SubmitCompositorFrame(e_surface_id.local_surface_id(),
                                   MakeDefaultCompositorFrame(),
                                   std::move(e_hit_test_region_list));
  local_surface_id_lookup_delegate()->SetSurfaceIdMap(e_surface_id);

  aggregator->Aggregate(e_surface_id);

  EXPECT_EQ(aggregator->GetRegionCount(), 4);

  EXPECT_EQ(host_buffer_frame_sink_id(), kDisplayFrameSink);

  AggregatedHitTestRegion region = host_regions()[0];
  EXPECT_EQ(region.flags, HitTestRegionFlags::kHitTestMine);
  EXPECT_EQ(region.frame_sink_id, e_surface_id.frame_sink_id());
  EXPECT_EQ(region.rect, gfx::Rect(0, 0, 1024, 768));
  EXPECT_EQ(region.child_count, 3);

  region = host_regions()[1];
  EXPECT_EQ(HitTestRegionFlags::kHitTestChildSurface |
                HitTestRegionFlags::kHitTestIgnore,
            region.flags);
  EXPECT_EQ(region.frame_sink_id, c_surface_id.frame_sink_id());
  EXPECT_EQ(region.rect, gfx::Rect(300, 100, 1600, 800));
  EXPECT_EQ(region.child_count, 2);

  gfx::Point point(300, 300);
  gfx::Transform transform(region.transform());
  transform.TransformPointReverse(&point);
  EXPECT_TRUE(point == gfx::Point(100, 200));

  region = host_regions()[2];
  EXPECT_EQ(HitTestRegionFlags::kHitTestChildSurface |
                HitTestRegionFlags::kHitTestMine,
            region.flags);
  EXPECT_EQ(region.frame_sink_id, a_surface_id.frame_sink_id());
  EXPECT_EQ(region.rect, gfx::Rect(0, 0, 200, 100));
  EXPECT_EQ(region.child_count, 0);

  region = host_regions()[3];
  EXPECT_EQ(HitTestRegionFlags::kHitTestChildSurface |
                HitTestRegionFlags::kHitTestMine,
            region.flags);
  EXPECT_EQ(region.frame_sink_id, b_surface_id.frame_sink_id());
  EXPECT_EQ(region.rect, gfx::Rect(0, 100, 800, 600));
  EXPECT_EQ(region.child_count, 0);
}

// Three children deep.
//
//  +e------------+
//  | +c1-------+ |
//  | | +c2---+ | |
//  | | | +c3-| | |
//  | | | |   | | |
//  | | | +---| | |
//  | | +-----+ | |
//  | +---------+ |
//  +-------------+
//

TEST_F(HitTestAggregatorTest, ThreeChildrenDeep) {
  TestHitTestAggregator* aggregator = hit_test_aggregator();
  EXPECT_EQ(aggregator->GetRegionCount(), 0);

  SurfaceId e_surface_id = MakeSurfaceId(kDisplayClientId);
  SurfaceId c1_surface_id = MakeSurfaceId(kDisplayClientId + 1);
  SurfaceId c2_surface_id = MakeSurfaceId(kDisplayClientId + 2);
  SurfaceId c3_surface_id = MakeSurfaceId(kDisplayClientId + 3);

  HitTestRegionList e_hit_test_region_list;
  e_hit_test_region_list.flags = HitTestRegionFlags::kHitTestMine;
  e_hit_test_region_list.bounds.SetRect(0, 0, 1024, 768);

  HitTestRegion e_hit_test_region_c1;
  e_hit_test_region_c1.flags = HitTestRegionFlags::kHitTestChildSurface;
  e_hit_test_region_c1.frame_sink_id = c1_surface_id.frame_sink_id();
  e_hit_test_region_c1.rect.SetRect(100, 100, 700, 700);

  e_hit_test_region_list.regions.push_back(std::move(e_hit_test_region_c1));

  HitTestRegionList c1_hit_test_region_list;
  c1_hit_test_region_list.flags = HitTestRegionFlags::kHitTestMine;
  c1_hit_test_region_list.bounds.SetRect(0, 0, 600, 600);

  HitTestRegion c1_hit_test_region_c2;
  c1_hit_test_region_c2.flags = HitTestRegionFlags::kHitTestChildSurface;
  c1_hit_test_region_c2.frame_sink_id = c2_surface_id.frame_sink_id();
  c1_hit_test_region_c2.rect.SetRect(100, 100, 500, 500);

  c1_hit_test_region_list.regions.push_back(std::move(c1_hit_test_region_c2));

  HitTestRegionList c2_hit_test_region_list;
  c2_hit_test_region_list.flags = HitTestRegionFlags::kHitTestMine;
  c2_hit_test_region_list.bounds.SetRect(0, 0, 400, 400);

  HitTestRegion c2_hit_test_region_c3;
  c2_hit_test_region_c3.flags = HitTestRegionFlags::kHitTestChildSurface;
  c2_hit_test_region_c3.frame_sink_id = c3_surface_id.frame_sink_id();
  c2_hit_test_region_c3.rect.SetRect(100, 100, 300, 300);

  c2_hit_test_region_list.regions.push_back(std::move(c2_hit_test_region_c3));

  HitTestRegionList c3_hit_test_region_list;
  c3_hit_test_region_list.flags = HitTestRegionFlags::kHitTestMine;
  c3_hit_test_region_list.bounds.SetRect(0, 0, 200, 200);

  // Submit in unexpected order.

  auto support2 = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, frame_sink_manager(), c1_surface_id.frame_sink_id(),
      false /* is_root */, false /* needs_sync_points */);
  support2->SubmitCompositorFrame(c1_surface_id.local_surface_id(),
                                  MakeDefaultCompositorFrame(),
                                  std::move(c1_hit_test_region_list));
  local_surface_id_lookup_delegate()->SetSurfaceIdMap(c1_surface_id);
  auto support3 = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, frame_sink_manager(), c3_surface_id.frame_sink_id(),
      false /* is_root */, false /* needs_sync_points */);
  support3->SubmitCompositorFrame(c3_surface_id.local_surface_id(),
                                  MakeDefaultCompositorFrame(),
                                  std::move(c3_hit_test_region_list));
  local_surface_id_lookup_delegate()->SetSurfaceIdMap(c3_surface_id);
  support()->SubmitCompositorFrame(e_surface_id.local_surface_id(),
                                   MakeDefaultCompositorFrame(),
                                   std::move(e_hit_test_region_list));
  local_surface_id_lookup_delegate()->SetSurfaceIdMap(e_surface_id);
  auto support4 = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, frame_sink_manager(), c2_surface_id.frame_sink_id(),
      false /* is_root */, false /* needs_sync_points */);
  support4->SubmitCompositorFrame(c2_surface_id.local_surface_id(),
                                  MakeDefaultCompositorFrame(),
                                  std::move(c2_hit_test_region_list));
  local_surface_id_lookup_delegate()->SetSurfaceIdMap(c2_surface_id);

  aggregator->Aggregate(e_surface_id);

  EXPECT_EQ(aggregator->GetRegionCount(), 4);

  EXPECT_EQ(host_buffer_frame_sink_id(), kDisplayFrameSink);

  AggregatedHitTestRegion region = host_regions()[0];
  EXPECT_EQ(region.flags, HitTestRegionFlags::kHitTestMine);
  EXPECT_EQ(region.frame_sink_id, e_surface_id.frame_sink_id());
  EXPECT_EQ(region.rect, gfx::Rect(0, 0, 1024, 768));
  EXPECT_EQ(region.child_count, 3);

  region = host_regions()[1];
  EXPECT_EQ(HitTestRegionFlags::kHitTestChildSurface |
                HitTestRegionFlags::kHitTestMine,
            region.flags);
  EXPECT_EQ(region.frame_sink_id, c1_surface_id.frame_sink_id());
  EXPECT_EQ(region.rect, gfx::Rect(100, 100, 700, 700));
  EXPECT_EQ(region.child_count, 2);

  region = host_regions()[2];
  EXPECT_EQ(HitTestRegionFlags::kHitTestChildSurface |
                HitTestRegionFlags::kHitTestMine,
            region.flags);
  EXPECT_EQ(region.frame_sink_id, c2_surface_id.frame_sink_id());
  EXPECT_EQ(region.rect, gfx::Rect(100, 100, 500, 500));
  EXPECT_EQ(region.child_count, 1);

  region = host_regions()[3];
  EXPECT_EQ(HitTestRegionFlags::kHitTestChildSurface |
                HitTestRegionFlags::kHitTestMine,
            region.flags);
  EXPECT_EQ(region.frame_sink_id, c3_surface_id.frame_sink_id());
  EXPECT_EQ(region.rect, gfx::Rect(100, 100, 300, 300));
  EXPECT_EQ(region.child_count, 0);
}

// Missing / late child.
//
//  +e-----------+
//  | +c--+      |
//  | |   |div-+ |
//  | |   |    | |
//  | |   |----+ |
//  | +---+      |
//  +------------+
//

TEST_F(HitTestAggregatorTest, MissingChildFrame) {
  TestHitTestAggregator* aggregator = hit_test_aggregator();
  EXPECT_EQ(aggregator->GetRegionCount(), 0);

  SurfaceId e_surface_id = MakeSurfaceId(kDisplayClientId);
  SurfaceId c_surface_id = MakeSurfaceId(kDisplayClientId + 1);

  HitTestRegionList e_hit_test_region_list;
  e_hit_test_region_list.flags = HitTestRegionFlags::kHitTestMine;
  e_hit_test_region_list.bounds.SetRect(0, 0, 1024, 768);

  HitTestRegion e_hit_test_region_div;
  e_hit_test_region_div.flags = HitTestRegionFlags::kHitTestMine;
  e_hit_test_region_div.frame_sink_id = e_surface_id.frame_sink_id();
  e_hit_test_region_div.rect.SetRect(200, 200, 300, 200);

  HitTestRegion e_hit_test_region_c;
  e_hit_test_region_c.flags = HitTestRegionFlags::kHitTestChildSurface;
  e_hit_test_region_c.frame_sink_id = c_surface_id.frame_sink_id();
  e_hit_test_region_c.rect.SetRect(100, 100, 200, 500);

  e_hit_test_region_list.regions.push_back(std::move(e_hit_test_region_c));
  e_hit_test_region_list.regions.push_back(std::move(e_hit_test_region_div));

  HitTestRegionList c_hit_test_region_list;
  c_hit_test_region_list.flags = HitTestRegionFlags::kHitTestMine;
  c_hit_test_region_list.bounds.SetRect(0, 0, 200, 500);

  // Submit in unexpected order.

  support()->SubmitCompositorFrame(e_surface_id.local_surface_id(),
                                   MakeDefaultCompositorFrame(),
                                   std::move(e_hit_test_region_list));
  local_surface_id_lookup_delegate()->SetSurfaceIdMap(e_surface_id);

  aggregator->Aggregate(e_surface_id);

  EXPECT_EQ(aggregator->GetRegionCount(), 3);

  EXPECT_EQ(host_buffer_frame_sink_id(), kDisplayFrameSink);

  AggregatedHitTestRegion region = host_regions()[0];
  EXPECT_EQ(region.flags, HitTestRegionFlags::kHitTestMine);
  EXPECT_EQ(region.frame_sink_id, e_surface_id.frame_sink_id());
  EXPECT_EQ(region.rect, gfx::Rect(0, 0, 1024, 768));
  EXPECT_EQ(region.child_count, 2);

  // |c_hit_test_region_list| was not submitted on time, so we should do
  // async targeting with |e_hit_test_region_c|.
  region = host_regions()[1];
  EXPECT_EQ(region.flags, HitTestRegionFlags::kHitTestChildSurface |
                              HitTestRegionFlags::kHitTestAsk |
                              HitTestRegionFlags::kHitTestNotActive);
  EXPECT_EQ(region.frame_sink_id, c_surface_id.frame_sink_id());
  EXPECT_EQ(region.rect, gfx::Rect(100, 100, 200, 500));
  EXPECT_EQ(region.child_count, 0);

  region = host_regions()[2];
  EXPECT_EQ(region.flags, HitTestRegionFlags::kHitTestMine);
  EXPECT_EQ(region.frame_sink_id, e_surface_id.frame_sink_id());
  EXPECT_EQ(region.rect, gfx::Rect(200, 200, 300, 200));
  EXPECT_EQ(region.child_count, 0);
}

// Exceed limits to ensure that bounds and resize work.
//
// A tree of embedders each with 8 children and 4 levels deep = 4096 regions.
// This will exceed initial allocation and force a resize.
//
//  +e--------------------------------------------------------+
//  | +c1----------++c2----------++c3----------++c4----------+|
//  | | +c1--------|| +c1--------|| +c1--------|| +c1--------||
//  | | | +c1-++c2-|| | +c1-++c2-|| | +c1-++c2-|| | +c1-++c2-||
//  | | | |   ||   || | |   ||   || | |   ||   || | |   ||   ||
//  | | | +---++---|| | +---++---|| | +---++---|| | +---++---||
//  | +------------++------------++------------++------------+|
//  | +c5----------++c6----------++c7----------++c8----------+|
//  | | +c1--------|| +c1--------|| +c1--------|| +c1--------||
//  | | | +c1-++c2-|| | +c1-++c2-|| | +c1-++c2-|| | +c1-++c2-||
//  | | | |   ||   || | |   ||   || | |   ||   || | |   ||   ||
//  | | | +---++---|| | +---++---|| | +---++---|| | +---++---||
//  | +------------++------------++------------++------------+|
//  +---------------------------------------------------------+
//

TEST_F(HitTestAggregatorTest, ExceedLimits) {
  TestHitTestAggregator* aggregator = hit_test_aggregator();
  EXPECT_EQ(aggregator->GetRegionCount(), 0);

  EXPECT_LT(aggregator->GetHitTestRegionListCapacity(), 4096);

  SurfaceId display_surface_id = MakeSurfaceId(kDisplayClientId);

  CreateAndSubmitHitTestRegionListWith8Children(kDisplayClientId, 3);

  aggregator->Aggregate(display_surface_id);

  // Expect 4680 regions:
  //  8 children 4 levels deep 8*8*8*8 is  4096
  //  1 region for each embedder/surface +  584
  //  1 root                             +    1
  //                                      -----
  //                                       4681.
  EXPECT_GE(aggregator->GetHitTestRegionListCapacity(), 4681);

  EXPECT_EQ(host_buffer_frame_sink_id(), kDisplayFrameSink);
  EXPECT_EQ(host_regions().size(), 4681u);
}

TEST_F(HitTestAggregatorTest, DiscardedSurfaces) {
  TestHitTestAggregator* aggregator = hit_test_aggregator();

  SurfaceId e_surface_id = MakeSurfaceId(kDisplayClientId);
  SurfaceId c_surface_id = MakeSurfaceId(kDisplayClientId + 1);

  HitTestRegionList e_hit_test_region_list;
  e_hit_test_region_list.flags = HitTestRegionFlags::kHitTestMine;
  e_hit_test_region_list.bounds.SetRect(0, 0, 1024, 768);

  HitTestRegion e_hit_test_region_div;
  e_hit_test_region_div.flags = HitTestRegionFlags::kHitTestMine;
  e_hit_test_region_div.frame_sink_id = e_surface_id.frame_sink_id();
  e_hit_test_region_div.rect.SetRect(200, 200, 300, 200);

  HitTestRegion e_hit_test_region_c;
  e_hit_test_region_c.flags = HitTestRegionFlags::kHitTestChildSurface;
  e_hit_test_region_c.frame_sink_id = c_surface_id.frame_sink_id();
  e_hit_test_region_c.rect.SetRect(100, 100, 200, 500);

  e_hit_test_region_list.regions.push_back(std::move(e_hit_test_region_c));
  e_hit_test_region_list.regions.push_back(std::move(e_hit_test_region_div));

  HitTestRegionList c_hit_test_region_list;
  c_hit_test_region_list.flags = HitTestRegionFlags::kHitTestMine;
  c_hit_test_region_list.bounds.SetRect(0, 0, 200, 500);

  EXPECT_FALSE(hit_test_manager()->GetActiveHitTestRegionList(
      local_surface_id_lookup_delegate(), e_surface_id.frame_sink_id()));
  EXPECT_FALSE(hit_test_manager()->GetActiveHitTestRegionList(
      local_surface_id_lookup_delegate(), c_surface_id.frame_sink_id()));

  // Submit in unexpected order.

  auto support2 = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, frame_sink_manager(), c_surface_id.frame_sink_id(),
      false /* is_root */, false /* needs_sync_points */);
  support2->SubmitCompositorFrame(c_surface_id.local_surface_id(),
                                  MakeDefaultCompositorFrame(),
                                  std::move(c_hit_test_region_list));
  local_surface_id_lookup_delegate()->SetSurfaceIdMap(c_surface_id);
  support()->SubmitCompositorFrame(e_surface_id.local_surface_id(),
                                   MakeDefaultCompositorFrame(),
                                   std::move(e_hit_test_region_list));
  local_surface_id_lookup_delegate()->SetSurfaceIdMap(e_surface_id);

  aggregator->Aggregate(e_surface_id);

  EXPECT_TRUE(hit_test_manager()->GetActiveHitTestRegionList(
      local_surface_id_lookup_delegate(), e_surface_id.frame_sink_id()));
  EXPECT_TRUE(hit_test_manager()->GetActiveHitTestRegionList(
      local_surface_id_lookup_delegate(), c_surface_id.frame_sink_id()));

  // Discard Surface and ensure active count goes down.
  support2->EvictSurface(c_surface_id.local_surface_id());
  ExpireAllTemporaryReferencesAndGarbageCollect();
  EXPECT_TRUE(hit_test_manager()->GetActiveHitTestRegionList(
      local_surface_id_lookup_delegate(), e_surface_id.frame_sink_id()));
  EXPECT_FALSE(hit_test_manager()->GetActiveHitTestRegionList(
      local_surface_id_lookup_delegate(), c_surface_id.frame_sink_id()));

  support()->EvictSurface(e_surface_id.local_surface_id());
  ExpireAllTemporaryReferencesAndGarbageCollect();
  EXPECT_FALSE(hit_test_manager()->GetActiveHitTestRegionList(
      local_surface_id_lookup_delegate(), e_surface_id.frame_sink_id()));
  EXPECT_FALSE(hit_test_manager()->GetActiveHitTestRegionList(
      local_surface_id_lookup_delegate(), c_surface_id.frame_sink_id()));
}

// Region c1 is transparent and on top of e, c2 is a child of e, d1 is a
// child of c1.
//
//  +e/c1----------+
//  |              |     Point   maps to
//  |   +c2-+      |     -----   -------
//  |   | 1 |      |       1        c2
//  |   +d1--------|
//  |   |          |
//  |   |          |
//  +--------------+
//

TEST_F(HitTestAggregatorTest, TransparentOverlayRegions) {
  TestHitTestAggregator* aggregator = hit_test_aggregator();
  EXPECT_EQ(aggregator->GetRegionCount(), 0);

  SurfaceId e_surface_id = MakeSurfaceId(kDisplayClientId);
  SurfaceId c1_surface_id = MakeSurfaceId(kDisplayClientId + 1);
  SurfaceId c2_surface_id = MakeSurfaceId(kDisplayClientId + 2);
  SurfaceId d1_surface_id = MakeSurfaceId(kDisplayClientId + 3);

  HitTestRegionList e_hit_test_region_list;
  e_hit_test_region_list.flags = HitTestRegionFlags::kHitTestMine;
  e_hit_test_region_list.bounds.SetRect(0, 0, 1024, 768);

  HitTestRegion e_hit_test_region_c1;
  e_hit_test_region_c1.flags = HitTestRegionFlags::kHitTestChildSurface;
  e_hit_test_region_c1.frame_sink_id = c1_surface_id.frame_sink_id();
  e_hit_test_region_c1.rect.SetRect(0, 0, 1024, 768);

  HitTestRegion e_hit_test_region_c2;
  e_hit_test_region_c2.flags = HitTestRegionFlags::kHitTestChildSurface;
  e_hit_test_region_c2.frame_sink_id = c2_surface_id.frame_sink_id();
  e_hit_test_region_c2.rect.SetRect(0, 0, 200, 100);
  e_hit_test_region_c2.transform.Translate(200, 100);

  e_hit_test_region_list.regions.push_back(std::move(e_hit_test_region_c1));
  e_hit_test_region_list.regions.push_back(std::move(e_hit_test_region_c2));

  HitTestRegionList c1_hit_test_region_list;
  c1_hit_test_region_list.flags = HitTestRegionFlags::kHitTestIgnore;
  c1_hit_test_region_list.bounds.SetRect(0, 0, 1024, 768);

  HitTestRegion c1_hit_test_region_d1;
  c1_hit_test_region_d1.flags = HitTestRegionFlags::kHitTestChildSurface;
  c1_hit_test_region_d1.frame_sink_id = d1_surface_id.frame_sink_id();
  c1_hit_test_region_d1.rect.SetRect(0, 100, 800, 600);
  c1_hit_test_region_d1.transform.Translate(200, 100);

  c1_hit_test_region_list.regions.push_back(std::move(c1_hit_test_region_d1));

  HitTestRegionList d1_hit_test_region_list;
  d1_hit_test_region_list.flags = HitTestRegionFlags::kHitTestMine;
  d1_hit_test_region_list.bounds.SetRect(0, 100, 800, 600);

  HitTestRegionList c2_hit_test_region_list;
  c2_hit_test_region_list.flags = HitTestRegionFlags::kHitTestMine;
  c2_hit_test_region_list.bounds.SetRect(0, 0, 200, 100);

  // Submit in unexpected order.

  auto support2 = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, frame_sink_manager(), c1_surface_id.frame_sink_id(),
      false /* is_root */, false /* needs_sync_points */);
  support2->SubmitCompositorFrame(c1_surface_id.local_surface_id(),
                                  MakeDefaultCompositorFrame(),
                                  std::move(c1_hit_test_region_list));
  local_surface_id_lookup_delegate()->SetSurfaceIdMap(c1_surface_id);
  auto support3 = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, frame_sink_manager(), c2_surface_id.frame_sink_id(),
      false /* is_root */, false /* needs_sync_points */);
  support3->SubmitCompositorFrame(c2_surface_id.local_surface_id(),
                                  MakeDefaultCompositorFrame(),
                                  std::move(c2_hit_test_region_list));
  local_surface_id_lookup_delegate()->SetSurfaceIdMap(c2_surface_id);
  auto support4 = std::make_unique<CompositorFrameSinkSupport>(
      nullptr, frame_sink_manager(), d1_surface_id.frame_sink_id(),
      false /* is_root */, false /* needs_sync_points */);
  support4->SubmitCompositorFrame(d1_surface_id.local_surface_id(),
                                  MakeDefaultCompositorFrame(),
                                  std::move(d1_hit_test_region_list));
  local_surface_id_lookup_delegate()->SetSurfaceIdMap(d1_surface_id);
  support()->SubmitCompositorFrame(e_surface_id.local_surface_id(),
                                   MakeDefaultCompositorFrame(),
                                   std::move(e_hit_test_region_list));
  local_surface_id_lookup_delegate()->SetSurfaceIdMap(e_surface_id);

  aggregator->Aggregate(e_surface_id);

  EXPECT_EQ(aggregator->GetRegionCount(), 4);

  EXPECT_EQ(host_buffer_frame_sink_id(), kDisplayFrameSink);

  AggregatedHitTestRegion region = host_regions()[0];
  EXPECT_EQ(region.flags, HitTestRegionFlags::kHitTestMine);
  EXPECT_EQ(region.frame_sink_id, e_surface_id.frame_sink_id());
  EXPECT_EQ(region.rect, gfx::Rect(0, 0, 1024, 768));
  EXPECT_EQ(region.child_count, 3);

  region = host_regions()[1];
  EXPECT_EQ(HitTestRegionFlags::kHitTestChildSurface |
                HitTestRegionFlags::kHitTestIgnore,
            region.flags);
  EXPECT_EQ(region.frame_sink_id, c1_surface_id.frame_sink_id());
  EXPECT_EQ(region.rect, gfx::Rect(0, 0, 1024, 768));
  EXPECT_EQ(region.child_count, 1);

  region = host_regions()[2];
  EXPECT_EQ(HitTestRegionFlags::kHitTestChildSurface |
                HitTestRegionFlags::kHitTestMine,
            region.flags);
  EXPECT_EQ(region.frame_sink_id, d1_surface_id.frame_sink_id());
  EXPECT_EQ(region.rect, gfx::Rect(0, 100, 800, 600));
  EXPECT_EQ(region.child_count, 0);

  region = host_regions()[3];
  EXPECT_EQ(HitTestRegionFlags::kHitTestChildSurface |
                HitTestRegionFlags::kHitTestMine,
            region.flags);
  EXPECT_EQ(region.frame_sink_id, c2_surface_id.frame_sink_id());
  EXPECT_EQ(region.rect, gfx::Rect(0, 0, 200, 100));
  EXPECT_EQ(region.child_count, 0);
}

TEST_F(HitTestAggregatorTest, HitTestDataNotUpdated) {
  TestHitTestAggregator* aggregator = hit_test_aggregator();
  EXPECT_EQ(aggregator->GetRegionCount(), 0);

  SurfaceId surface_id = MakeSurfaceId(kDisplayClientId);
  HitTestRegionList hit_test_region_list;
  hit_test_region_list.flags = HitTestRegionFlags::kHitTestMine;
  hit_test_region_list.bounds.SetRect(0, 0, 1024, 768);
  HitTestRegionList hit_test_region_list_copy = hit_test_region_list;

  support()->SubmitCompositorFrame(surface_id.local_surface_id(),
                                   MakeDefaultCompositorFrame(),
                                   std::move(hit_test_region_list));
  aggregator->Aggregate(surface_id);
  uint64_t last_index = aggregator->GetLastSubmitHitTestRegionListIndex();

  // We did not update the hit-test data. Expect the index from Aggregator /
  // Manager to remain unchanged.
  support()->SubmitCompositorFrame(surface_id.local_surface_id(),
                                   MakeDefaultCompositorFrame(), base::nullopt);
  aggregator->Aggregate(surface_id);
  EXPECT_EQ(last_index, aggregator->GetLastSubmitHitTestRegionListIndex());

  // We updated hit-test data. Expect the index to have changed.
  support()->SubmitCompositorFrame(surface_id.local_surface_id(),
                                   MakeDefaultCompositorFrame(),
                                   std::move(hit_test_region_list));
  aggregator->Aggregate(surface_id);
  EXPECT_NE(last_index, aggregator->GetLastSubmitHitTestRegionListIndex());
}

}  // namespace viz
