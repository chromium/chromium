// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"

#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

// ParentLocalSurfaceIdAllocator has 2 accessors which do not alter state:
// - GetCurrentLocalSurfaceIdAllocation().local_surface_id()
// - is_allocation_suppressed()
//
// For every operation which changes state we can test:
// - the operation completed as expected,
// - the accessors did not change, and/or
// - the accessors changed in the way we expected.

namespace viz {

class ParentLocalSurfaceIdAllocatorTest : public testing::Test {
 public:
  ParentLocalSurfaceIdAllocatorTest() = default;

  ~ParentLocalSurfaceIdAllocatorTest() override {}

  ParentLocalSurfaceIdAllocator& allocator() { return *allocator_.get(); }

  base::TimeTicks Now() { return now_src_->NowTicks(); }

  void AdvanceTime(base::TimeDelta delta) { now_src_->Advance(delta); }

  LocalSurfaceIdAllocation GenerateChildLocalSurfaceIdAllocation() {
    const LocalSurfaceId& current_local_surface_id =
        allocator_->GetCurrentLocalSurfaceIdAllocation().local_surface_id();

    return LocalSurfaceIdAllocation(
        LocalSurfaceId(current_local_surface_id.parent_sequence_number(),
                       current_local_surface_id.child_sequence_number() + 1,
                       current_local_surface_id.embed_token()),
        base::TimeTicks::Now());
  }

  // testing::Test:
  void SetUp() override {
    testing::Test::SetUp();
    now_src_ = std::make_unique<base::SimpleTestTickClock>();
    // Advance time by one millisecond to ensure all time stamps are non-null.
    AdvanceTime(base::TimeDelta::FromMilliseconds(1u));
    allocator_ =
        std::make_unique<ParentLocalSurfaceIdAllocator>(now_src_.get());
  }

  void TearDown() override {
    allocator_.reset();
    now_src_.reset();
  }

 private:
  std::unique_ptr<base::SimpleTestTickClock> now_src_;
  std::unique_ptr<ParentLocalSurfaceIdAllocator> allocator_;

  DISALLOW_COPY_AND_ASSIGN(ParentLocalSurfaceIdAllocatorTest);
};

// UpdateFromChild() on a parent allocator should accept the child's sequence
// number. But it should continue to use its own parent sequence number and
// embed_token.
TEST_F(ParentLocalSurfaceIdAllocatorTest,
       UpdateFromChildOnlyUpdatesExpectedLocalSurfaceIdComponents) {
  allocator().GenerateId();
  LocalSurfaceId preupdate_local_surface_id =
      allocator().GetCurrentLocalSurfaceIdAllocation().local_surface_id();
  LocalSurfaceIdAllocation child_local_surface_id_allocation =
      GenerateChildLocalSurfaceIdAllocation();
  const LocalSurfaceId& child_allocated_local_surface_id =
      child_local_surface_id_allocation.local_surface_id();
  EXPECT_EQ(preupdate_local_surface_id.parent_sequence_number(),
            child_allocated_local_surface_id.parent_sequence_number());
  EXPECT_NE(preupdate_local_surface_id.child_sequence_number(),
            child_allocated_local_surface_id.child_sequence_number());
  EXPECT_EQ(preupdate_local_surface_id.embed_token(),
            child_allocated_local_surface_id.embed_token());

  bool changed = allocator().UpdateFromChild(child_local_surface_id_allocation);
  EXPECT_TRUE(changed);

  const LocalSurfaceId& postupdate_local_surface_id =
      allocator().GetCurrentLocalSurfaceIdAllocation().local_surface_id();
  EXPECT_EQ(postupdate_local_surface_id.parent_sequence_number(),
            child_allocated_local_surface_id.parent_sequence_number());
  EXPECT_EQ(postupdate_local_surface_id.child_sequence_number(),
            child_allocated_local_surface_id.child_sequence_number());
  EXPECT_EQ(postupdate_local_surface_id.embed_token(),
            child_allocated_local_surface_id.embed_token());
  EXPECT_FALSE(allocator().is_allocation_suppressed());
}

// GenerateId() on a parent allocator should monotonically increment the parent
// sequence number and use the previous embed_token.
TEST_F(ParentLocalSurfaceIdAllocatorTest,
       GenerateIdOnlyUpdatesExpectedLocalSurfaceIdComponents) {
  allocator().GenerateId();
  LocalSurfaceId pregenerateid_local_surface_id =
      allocator().GetCurrentLocalSurfaceIdAllocation().local_surface_id();

  allocator().GenerateId();
  const LocalSurfaceId& returned_local_surface_id =
      allocator().GetCurrentLocalSurfaceIdAllocation().local_surface_id();

  const LocalSurfaceId& postgenerateid_local_surface_id =
      allocator().GetCurrentLocalSurfaceIdAllocation().local_surface_id();
  EXPECT_EQ(pregenerateid_local_surface_id.parent_sequence_number() + 1,
            postgenerateid_local_surface_id.parent_sequence_number());
  EXPECT_EQ(pregenerateid_local_surface_id.child_sequence_number(),
            postgenerateid_local_surface_id.child_sequence_number());
  EXPECT_EQ(pregenerateid_local_surface_id.embed_token(),
            postgenerateid_local_surface_id.embed_token());
  EXPECT_EQ(
      returned_local_surface_id,
      allocator().GetCurrentLocalSurfaceIdAllocation().local_surface_id());
  EXPECT_FALSE(allocator().is_allocation_suppressed());
}

// This test verifies that if the child-allocated LocalSurfaceId has the most
// recent parent sequence number at the time UpdateFromChild is called, then
// its allocation time is used as the latest allocation time in
// ParentLocalSurfaceIdAllocator. In the event that the child-allocated
// LocalSurfaceId does not correspond to the latest parent sequence number
// then UpdateFromChild represents a new allocation and thus the allocation time
// is updated.
TEST_F(ParentLocalSurfaceIdAllocatorTest,
       CorrectTimeStampUsedInUpdateFromChild) {
  allocator().GenerateId();
  LocalSurfaceIdAllocation child_allocated_id =
      GenerateChildLocalSurfaceIdAllocation();

  // Advance time by one millisecond.
  AdvanceTime(base::TimeDelta::FromMilliseconds(1u));

  {
    bool changed = allocator().UpdateFromChild(child_allocated_id);
    EXPECT_TRUE(changed);
    EXPECT_EQ(child_allocated_id,
              allocator().GetCurrentLocalSurfaceIdAllocation());
  }

  LocalSurfaceIdAllocation child_allocated_id2 =
      GenerateChildLocalSurfaceIdAllocation();
  allocator().GenerateId();
  {
    bool changed = allocator().UpdateFromChild(child_allocated_id2);
    EXPECT_TRUE(changed);
    EXPECT_NE(child_allocated_id2,
              allocator().GetCurrentLocalSurfaceIdAllocation());
    EXPECT_EQ(child_allocated_id2.local_surface_id().child_sequence_number(),
              allocator()
                  .GetCurrentLocalSurfaceIdAllocation()
                  .local_surface_id()
                  .child_sequence_number());
    EXPECT_EQ(
        Now(),
        allocator().GetCurrentLocalSurfaceIdAllocation().allocation_time());
  }
}

}  // namespace viz
