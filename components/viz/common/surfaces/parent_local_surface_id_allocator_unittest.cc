// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"

#include "base/time/time.h"
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
namespace {

::testing::AssertionResult ParentSequenceNumberIsSet(
    const LocalSurfaceId& local_surface_id);
::testing::AssertionResult ChildSequenceNumberIsSet(
    const LocalSurfaceId& local_surface_id);
::testing::AssertionResult NonceIsEmpty(const LocalSurfaceId& local_surface_id);

LocalSurfaceId GetFakeChildAllocatedLocalSurfaceId(
    const ParentLocalSurfaceIdAllocator& parent_allocator);
ParentLocalSurfaceIdAllocator GetChildUpdatedAllocator();

}  // namespace

// The default constructor should generate a embed_token and initialize the
// last known LocalSurfaceId.
// Allocation should not be suppressed.
TEST(ParentLocalSurfaceIdAllocatorTest,
     DefaultConstructorShouldInitializeLocalSurfaceIdAndNotBeSuppressed) {
  ParentLocalSurfaceIdAllocator default_constructed_parent_allocator;

  const LocalSurfaceId& default_local_surface_id =
      default_constructed_parent_allocator.GetCurrentLocalSurfaceId();
  EXPECT_TRUE(default_local_surface_id.is_valid());
  EXPECT_TRUE(ParentSequenceNumberIsSet(default_local_surface_id));
  EXPECT_TRUE(ChildSequenceNumberIsSet(default_local_surface_id));
  EXPECT_FALSE(NonceIsEmpty(default_local_surface_id));
  EXPECT_FALSE(default_constructed_parent_allocator.is_allocation_suppressed());
}

// The move constructor should move the last-known LocalSurfaceId.
TEST(ParentLocalSurfaceIdAllocatorTest,
     MoveConstructorShouldMoveLastKnownLocalSurfaceId) {
  ParentLocalSurfaceIdAllocator moving_parent_allocator =
      GetChildUpdatedAllocator();
  LocalSurfaceId premoved_local_surface_id =
      moving_parent_allocator.GetCurrentLocalSurfaceId();

  ParentLocalSurfaceIdAllocator moved_to_parent_allocator =
      std::move(moving_parent_allocator);

  EXPECT_EQ(premoved_local_surface_id,
            moved_to_parent_allocator.GetCurrentLocalSurfaceId());
  EXPECT_FALSE(moved_to_parent_allocator.is_allocation_suppressed());
}

// The move assignment operator should move the last-known LocalSurfaceId.
TEST(ParentLocalSurfaceIdAllocatorTest,
     MoveAssignmentOperatorShouldMoveLastKnownLocalSurfaceId) {
  ParentLocalSurfaceIdAllocator moving_parent_allocator =
      GetChildUpdatedAllocator();
  LocalSurfaceId premoved_local_surface_id =
      moving_parent_allocator.GetCurrentLocalSurfaceId();
  ParentLocalSurfaceIdAllocator moved_to_parent_allocator;
  EXPECT_NE(premoved_local_surface_id,
            moved_to_parent_allocator.GetCurrentLocalSurfaceId());

  moved_to_parent_allocator = std::move(moving_parent_allocator);

  EXPECT_EQ(premoved_local_surface_id,
            moved_to_parent_allocator.GetCurrentLocalSurfaceId());
  EXPECT_FALSE(moved_to_parent_allocator.is_allocation_suppressed());
}

// UpdateFromChild() on a parent allocator should accept the child's sequence
// number. But it should continue to use its own parent sequence number and
// embed_token.
TEST(ParentLocalSurfaceIdAllocatorTest,
     UpdateFromChildOnlyUpdatesExpectedLocalSurfaceIdComponents) {
  ParentLocalSurfaceIdAllocator child_updated_parent_allocator;
  LocalSurfaceId preupdate_local_surface_id =
      child_updated_parent_allocator.GenerateId();
  LocalSurfaceId child_allocated_local_surface_id =
      GetFakeChildAllocatedLocalSurfaceId(child_updated_parent_allocator);
  EXPECT_EQ(preupdate_local_surface_id.parent_sequence_number(),
            child_allocated_local_surface_id.parent_sequence_number());
  EXPECT_NE(preupdate_local_surface_id.child_sequence_number(),
            child_allocated_local_surface_id.child_sequence_number());
  EXPECT_EQ(preupdate_local_surface_id.embed_token(),
            child_allocated_local_surface_id.embed_token());

  bool changed = child_updated_parent_allocator.UpdateFromChild(
      child_allocated_local_surface_id, base::TimeTicks());
  EXPECT_TRUE(changed);

  const LocalSurfaceId& postupdate_local_surface_id =
      child_updated_parent_allocator.GetCurrentLocalSurfaceId();
  EXPECT_EQ(postupdate_local_surface_id.parent_sequence_number(),
            child_allocated_local_surface_id.parent_sequence_number());
  EXPECT_EQ(postupdate_local_surface_id.child_sequence_number(),
            child_allocated_local_surface_id.child_sequence_number());
  EXPECT_EQ(postupdate_local_surface_id.embed_token(),
            child_allocated_local_surface_id.embed_token());
  EXPECT_FALSE(child_updated_parent_allocator.is_allocation_suppressed());
}

// GenerateId() on a parent allocator should monotonically increment the parent
// sequence number and use the previous embed_token.
TEST(ParentLocalSurfaceIdAllocatorTest,
     GenerateIdOnlyUpdatesExpectedLocalSurfaceIdComponents) {
  ParentLocalSurfaceIdAllocator generating_parent_allocator =
      GetChildUpdatedAllocator();
  LocalSurfaceId pregenerateid_local_surface_id =
      generating_parent_allocator.GetCurrentLocalSurfaceId();

  const LocalSurfaceId& returned_local_surface_id =
      generating_parent_allocator.GenerateId();

  const LocalSurfaceId& postgenerateid_local_surface_id =
      generating_parent_allocator.GetCurrentLocalSurfaceId();
  EXPECT_EQ(pregenerateid_local_surface_id.parent_sequence_number() + 1,
            postgenerateid_local_surface_id.parent_sequence_number());
  EXPECT_EQ(pregenerateid_local_surface_id.child_sequence_number(),
            postgenerateid_local_surface_id.child_sequence_number());
  EXPECT_EQ(pregenerateid_local_surface_id.embed_token(),
            postgenerateid_local_surface_id.embed_token());
  EXPECT_EQ(returned_local_surface_id,
            generating_parent_allocator.GetCurrentLocalSurfaceId());
  EXPECT_FALSE(generating_parent_allocator.is_allocation_suppressed());
}

// This test verifies that calling reset with a LocalSurfaceId updates the
// GetCurrentLocalSurfaceId and affects GenerateId.
TEST(ParentLocalSurfaceIdAllocatorTest, ResetUpdatesComponents) {
  ParentLocalSurfaceIdAllocator default_constructed_parent_allocator;

  LocalSurfaceId default_local_surface_id =
      default_constructed_parent_allocator.GetCurrentLocalSurfaceId();
  EXPECT_TRUE(default_local_surface_id.is_valid());
  EXPECT_TRUE(ParentSequenceNumberIsSet(default_local_surface_id));
  EXPECT_TRUE(ChildSequenceNumberIsSet(default_local_surface_id));
  EXPECT_FALSE(NonceIsEmpty(default_local_surface_id));

  LocalSurfaceId new_local_surface_id(
      2u, 2u, base::UnguessableToken::Deserialize(0, 1u));

  default_constructed_parent_allocator.Reset(new_local_surface_id);
  EXPECT_EQ(new_local_surface_id,
            default_constructed_parent_allocator.GetCurrentLocalSurfaceId());

  LocalSurfaceId generated_id =
      default_constructed_parent_allocator.GenerateId();

  EXPECT_EQ(generated_id.embed_token(), new_local_surface_id.embed_token());
  EXPECT_EQ(generated_id.child_sequence_number(),
            new_local_surface_id.child_sequence_number());
  EXPECT_EQ(generated_id.parent_sequence_number(),
            new_local_surface_id.child_sequence_number() + 1);
}

namespace {

::testing::AssertionResult ParentSequenceNumberIsSet(
    const LocalSurfaceId& local_surface_id) {
  if (local_surface_id.parent_sequence_number() != kInvalidParentSequenceNumber)
    return ::testing::AssertionSuccess();

  return ::testing::AssertionFailure() << "parent_sequence_number() is not set";
}

::testing::AssertionResult ChildSequenceNumberIsSet(
    const LocalSurfaceId& local_surface_id) {
  if (local_surface_id.child_sequence_number() != kInvalidChildSequenceNumber)
    return ::testing::AssertionSuccess();

  return ::testing::AssertionFailure() << "child_sequence_number() is not set";
}

::testing::AssertionResult NonceIsEmpty(
    const LocalSurfaceId& local_surface_id) {
  if (local_surface_id.embed_token().is_empty())
    return ::testing::AssertionSuccess();

  return ::testing::AssertionFailure() << "embed_token() is not empty";
}

LocalSurfaceId GetFakeChildAllocatedLocalSurfaceId(
    const ParentLocalSurfaceIdAllocator& parent_allocator) {
  const LocalSurfaceId& current_local_surface_id =
      parent_allocator.GetCurrentLocalSurfaceId();

  return LocalSurfaceId(current_local_surface_id.parent_sequence_number(),
                        current_local_surface_id.child_sequence_number() + 1,
                        current_local_surface_id.embed_token());
}

ParentLocalSurfaceIdAllocator GetChildUpdatedAllocator() {
  ParentLocalSurfaceIdAllocator child_updated_parent_allocator;
  LocalSurfaceId child_allocated_local_surface_id =
      GetFakeChildAllocatedLocalSurfaceId(child_updated_parent_allocator);
  child_updated_parent_allocator.UpdateFromChild(
      child_allocated_local_surface_id, base::TimeTicks());
  return child_updated_parent_allocator;
}

}  // namespace
}  // namespace viz
