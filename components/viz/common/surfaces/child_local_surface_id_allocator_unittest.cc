// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/surfaces/child_local_surface_id_allocator.h"

#include "base/time/time.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "testing/gtest/include/gtest/gtest.h"

// ChildLocalSurfaceIdAllocator has 1 accessor which does not alter state:
// - GetCurrentLocalSurfaceId()
//
// For every operation which changes state we can test:
// - the operation completed as expected,
// - the accessors did not change, and/or
// - the accessors changed in the way we expected.

namespace viz {
namespace {

::testing::AssertionResult ParentSequenceNumberIsNotSet(
    const LocalSurfaceId& local_surface_id);
::testing::AssertionResult ChildSequenceNumberIsSet(
    const LocalSurfaceId& local_surface_id);
::testing::AssertionResult NonceIsEmpty(const LocalSurfaceId& local_surface_id);

LocalSurfaceId GetFakeParentAllocatedLocalSurfaceId();
ChildLocalSurfaceIdAllocator GetParentUpdatedAllocator();

}  // namespace

// The default constructor should initialize its last-known LocalSurfaceId (and
// all of its components) to an invalid state.
TEST(ChildLocalSurfaceIdAllocatorTest,
     DefaultConstructorShouldNotSetLocalSurfaceIdComponents) {
  ChildLocalSurfaceIdAllocator default_constructed_child_allocator;

  const LocalSurfaceId& default_local_surface_id =
      default_constructed_child_allocator.GetCurrentLocalSurfaceId();
  EXPECT_FALSE(default_local_surface_id.is_valid());
  EXPECT_TRUE(ParentSequenceNumberIsNotSet(default_local_surface_id));
  EXPECT_TRUE(ChildSequenceNumberIsSet(default_local_surface_id));
  EXPECT_TRUE(NonceIsEmpty(default_local_surface_id));
}

// The move constructor should move the last-known LocalSurfaceId.
TEST(ChildLocalSurfaceIdAllocatorTest,
     MoveConstructorShouldMoveLastKnownLocalSurfaceId) {
  ChildLocalSurfaceIdAllocator moving_child_allocator =
      GetParentUpdatedAllocator();
  LocalSurfaceId premoved_local_surface_id =
      moving_child_allocator.GetCurrentLocalSurfaceId();

  ChildLocalSurfaceIdAllocator moved_to_child_allocator =
      std::move(moving_child_allocator);

  EXPECT_EQ(premoved_local_surface_id,
            moved_to_child_allocator.GetCurrentLocalSurfaceId());
}

// The move assignment operator should move the last-known LocalSurfaceId.
TEST(ChildLocalSurfaceIdAllocatorTest,
     MoveAssignmentOperatorShouldMoveLastKnownLocalSurfaceId) {
  ChildLocalSurfaceIdAllocator moving_child_allocator =
      GetParentUpdatedAllocator();
  LocalSurfaceId premoved_local_surface_id =
      moving_child_allocator.GetCurrentLocalSurfaceId();
  ChildLocalSurfaceIdAllocator moved_to_child_allocator;
  EXPECT_NE(premoved_local_surface_id,
            moved_to_child_allocator.GetCurrentLocalSurfaceId());

  moved_to_child_allocator = std::move(moving_child_allocator);

  EXPECT_EQ(premoved_local_surface_id,
            moved_to_child_allocator.GetCurrentLocalSurfaceId());
}

// UpdateFromParent() on a child allocator should accept the parent's sequence
// number and embed_token. But it should continue to use its own child sequence
// number.
TEST(ChildLocalSurfaceIdAllocatorTest,
     UpdateFromParentOnlyUpdatesExpectedLocalSurfaceIdComponents) {
  ChildLocalSurfaceIdAllocator parent_updated_child_allocator;
  LocalSurfaceId preupdate_local_surface_id =
      parent_updated_child_allocator.GetCurrentLocalSurfaceId();
  LocalSurfaceId parent_allocated_local_surface_id =
      GetFakeParentAllocatedLocalSurfaceId();
  EXPECT_NE(preupdate_local_surface_id.parent_sequence_number(),
            parent_allocated_local_surface_id.parent_sequence_number());
  EXPECT_NE(preupdate_local_surface_id.child_sequence_number(),
            parent_allocated_local_surface_id.child_sequence_number());
  EXPECT_NE(preupdate_local_surface_id.embed_token(),
            parent_allocated_local_surface_id.embed_token());

  bool changed = parent_updated_child_allocator.UpdateFromParent(
      parent_allocated_local_surface_id, base::TimeTicks());
  EXPECT_TRUE(changed);

  const LocalSurfaceId& postupdate_local_surface_id =
      parent_updated_child_allocator.GetCurrentLocalSurfaceId();
  EXPECT_EQ(postupdate_local_surface_id.parent_sequence_number(),
            parent_allocated_local_surface_id.parent_sequence_number());
  EXPECT_NE(postupdate_local_surface_id.child_sequence_number(),
            parent_allocated_local_surface_id.child_sequence_number());
  EXPECT_EQ(postupdate_local_surface_id.embed_token(),
            parent_allocated_local_surface_id.embed_token());
}

// UpdateFromParent() on a child allocator should accept the parent's
// LocalSurfaceId if only the embed_token changed.
TEST(ChildLocalSurfaceIdAllocatorTest, UpdateFromParentEmbedTokenChanged) {
  ParentLocalSurfaceIdAllocator parent_allocator;
  ParentLocalSurfaceIdAllocator parent_allocator2;
  ChildLocalSurfaceIdAllocator child_allocator;

  EXPECT_TRUE(parent_allocator.GenerateId().is_valid());
  EXPECT_TRUE(child_allocator.UpdateFromParent(
      parent_allocator.GetCurrentLocalSurfaceId(),
      parent_allocator.allocation_time()));
  EXPECT_LE(
      parent_allocator2.GetCurrentLocalSurfaceId().parent_sequence_number(),
      parent_allocator.GetCurrentLocalSurfaceId().parent_sequence_number());
  EXPECT_NE(parent_allocator2.GetCurrentLocalSurfaceId().embed_token(),
            parent_allocator.GetCurrentLocalSurfaceId().embed_token());

  EXPECT_TRUE(child_allocator.UpdateFromParent(
      parent_allocator2.GetCurrentLocalSurfaceId(),
      parent_allocator2.allocation_time()));
}

// GenerateId() on a child allocator should monotonically increment the child
// sequence number.
TEST(ChildLocalSurfaceIdAllocatorTest,
     GenerateIdOnlyUpdatesExpectedLocalSurfaceIdComponents) {
  ChildLocalSurfaceIdAllocator generating_child_allocator =
      GetParentUpdatedAllocator();
  LocalSurfaceId pregenerateid_local_surface_id =
      generating_child_allocator.GetCurrentLocalSurfaceId();

  const LocalSurfaceId& returned_local_surface_id =
      generating_child_allocator.GenerateId();

  const LocalSurfaceId& postgenerateid_local_surface_id =
      generating_child_allocator.GetCurrentLocalSurfaceId();
  EXPECT_EQ(pregenerateid_local_surface_id.parent_sequence_number(),
            postgenerateid_local_surface_id.parent_sequence_number());
  EXPECT_EQ(pregenerateid_local_surface_id.child_sequence_number() + 1,
            postgenerateid_local_surface_id.child_sequence_number());
  EXPECT_EQ(pregenerateid_local_surface_id.embed_token(),
            postgenerateid_local_surface_id.embed_token());
  EXPECT_EQ(returned_local_surface_id,
            generating_child_allocator.GetCurrentLocalSurfaceId());
}

namespace {

::testing::AssertionResult ParentSequenceNumberIsNotSet(
    const LocalSurfaceId& local_surface_id) {
  if (local_surface_id.parent_sequence_number() == kInvalidParentSequenceNumber)
    return ::testing::AssertionSuccess();

  return ::testing::AssertionFailure() << "parent_sequence_number() is set";
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

LocalSurfaceId GetFakeParentAllocatedLocalSurfaceId() {
  constexpr uint32_t kParentSequenceNumber = 3;
  constexpr uint32_t kChildSequenceNumber = 2;
  const base::UnguessableToken embed_token = base::UnguessableToken::Create();

  return LocalSurfaceId(kParentSequenceNumber, kChildSequenceNumber,
                        embed_token);
}

ChildLocalSurfaceIdAllocator GetParentUpdatedAllocator() {
  ChildLocalSurfaceIdAllocator parent_updated_child_allocator;
  LocalSurfaceId parent_allocated_local_surface_id =
      GetFakeParentAllocatedLocalSurfaceId();
  parent_updated_child_allocator.UpdateFromParent(
      parent_allocated_local_surface_id, base::TimeTicks());
  return parent_updated_child_allocator;
}

}  // namespace
}  // namespace viz
