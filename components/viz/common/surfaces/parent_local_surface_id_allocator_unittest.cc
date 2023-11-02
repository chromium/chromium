// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"

// ParentLocalSurfaceIdAllocator has 2 accessors which do not alter state:
// - GetCurrentLocalSurfaceId()
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

  ParentLocalSurfaceIdAllocatorTest(const ParentLocalSurfaceIdAllocatorTest&) =
      delete;
  ParentLocalSurfaceIdAllocatorTest& operator=(
      const ParentLocalSurfaceIdAllocatorTest&) = delete;

  ~ParentLocalSurfaceIdAllocatorTest() override = default;

  ParentLocalSurfaceIdAllocator& allocator() { return *allocator_.get(); }

  LocalSurfaceId GenerateChildLocalSurfaceId() {
    const LocalSurfaceId& current_local_surface_id =
        allocator_->GetCurrentLocalSurfaceId();

    return LocalSurfaceId(current_local_surface_id.parent_sequence_number(),
                          current_local_surface_id.child_sequence_number() + 1,
                          current_local_surface_id.embed_token());
  }

  // testing::Test:
  void SetUp() override {
    testing::Test::SetUp();
    allocator_ = std::make_unique<ParentLocalSurfaceIdAllocator>();
  }

  void TearDown() override {
    allocator_.reset();
  }

 private:
  std::unique_ptr<ParentLocalSurfaceIdAllocator> allocator_;
};

// UpdateFromChild() on a parent allocator should accept the child's sequence
// number. But it should continue to use its own parent sequence number and
// embed_token.
TEST_F(ParentLocalSurfaceIdAllocatorTest,
       UpdateFromChildOnlyUpdatesExpectedLocalSurfaceIdComponents) {
  allocator().GenerateId();
  LocalSurfaceId preupdate_local_surface_id =
      allocator().GetCurrentLocalSurfaceId();
  LocalSurfaceId child_local_surface_id = GenerateChildLocalSurfaceId();
  const LocalSurfaceId& child_allocated_local_surface_id =
      child_local_surface_id;
  EXPECT_EQ(preupdate_local_surface_id.parent_sequence_number(),
            child_allocated_local_surface_id.parent_sequence_number());
  EXPECT_NE(preupdate_local_surface_id.child_sequence_number(),
            child_allocated_local_surface_id.child_sequence_number());
  EXPECT_EQ(preupdate_local_surface_id.embed_token(),
            child_allocated_local_surface_id.embed_token());

  bool changed = allocator().UpdateFromChild(child_local_surface_id);
  EXPECT_TRUE(changed);

  const LocalSurfaceId& postupdate_local_surface_id =
      allocator().GetCurrentLocalSurfaceId();
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
      allocator().GetCurrentLocalSurfaceId();

  allocator().GenerateId();
  const LocalSurfaceId& returned_local_surface_id =
      allocator().GetCurrentLocalSurfaceId();

  const LocalSurfaceId& postgenerateid_local_surface_id =
      allocator().GetCurrentLocalSurfaceId();
  EXPECT_EQ(pregenerateid_local_surface_id.parent_sequence_number() + 1,
            postgenerateid_local_surface_id.parent_sequence_number());
  EXPECT_EQ(pregenerateid_local_surface_id.child_sequence_number(),
            postgenerateid_local_surface_id.child_sequence_number());
  EXPECT_EQ(pregenerateid_local_surface_id.embed_token(),
            postgenerateid_local_surface_id.embed_token());
  EXPECT_EQ(returned_local_surface_id, allocator().GetCurrentLocalSurfaceId());
  EXPECT_FALSE(allocator().is_allocation_suppressed());
}

}  // namespace viz
