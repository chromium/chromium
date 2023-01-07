// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/surfaces/child_local_surface_id_allocator.h"

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
::testing::AssertionResult EmbedTokenIsValid(
    const LocalSurfaceId& local_surface_id);

}  // namespace

class ChildLocalSurfaceIdAllocatorTest : public testing::Test {
 public:
  ChildLocalSurfaceIdAllocatorTest() = default;

  ChildLocalSurfaceIdAllocatorTest(const ChildLocalSurfaceIdAllocatorTest&) =
      delete;
  ChildLocalSurfaceIdAllocatorTest& operator=(
      const ChildLocalSurfaceIdAllocatorTest&) = delete;

  ~ChildLocalSurfaceIdAllocatorTest() override {}

  ChildLocalSurfaceIdAllocator& allocator() { return *allocator_.get(); }

  ParentLocalSurfaceIdAllocator& parent_allocator1() {
    return *parent_allocator1_.get();
  }

  ParentLocalSurfaceIdAllocator& parent_allocator2() {
    return *parent_allocator2_.get();
  }

  // testing::Test:
  void SetUp() override {
    testing::Test::SetUp();
    allocator_ = std::make_unique<ChildLocalSurfaceIdAllocator>();
    parent_allocator1_ = std::make_unique<ParentLocalSurfaceIdAllocator>();
    parent_allocator2_ = std::make_unique<ParentLocalSurfaceIdAllocator>();
  }

  void TearDown() override {
    parent_allocator2_.reset();
    parent_allocator1_.reset();
    allocator_.reset();
  }

 private:
  std::unique_ptr<ChildLocalSurfaceIdAllocator> allocator_;
  std::unique_ptr<ParentLocalSurfaceIdAllocator> parent_allocator1_;
  std::unique_ptr<ParentLocalSurfaceIdAllocator> parent_allocator2_;
};

// The default constructor should initialize its last-known LocalSurfaceId (and
// all of its components) to an invalid state.
TEST_F(ChildLocalSurfaceIdAllocatorTest,
       DefaultConstructorShouldNotSetLocalSurfaceIdComponents) {
  const LocalSurfaceId& default_local_surface_id =
      allocator().GetCurrentLocalSurfaceId();
  EXPECT_FALSE(default_local_surface_id.is_valid());
  EXPECT_TRUE(ParentSequenceNumberIsNotSet(default_local_surface_id));
  EXPECT_TRUE(ChildSequenceNumberIsSet(default_local_surface_id));
  EXPECT_FALSE(EmbedTokenIsValid(default_local_surface_id));
}

// UpdateFromParent() on a child allocator should accept the parent's sequence
// number and embed_token. But it should continue to use its own child sequence
// number.
TEST_F(ChildLocalSurfaceIdAllocatorTest,
       UpdateFromParentOnlyUpdatesExpectedLocalSurfaceIdComponents) {
  LocalSurfaceId preupdate_local_surface_id =
      allocator().GetCurrentLocalSurfaceId();

  parent_allocator1().GenerateId();
  LocalSurfaceId parent_allocated_local_surface_id =
      parent_allocator1().GetCurrentLocalSurfaceId();
  EXPECT_NE(preupdate_local_surface_id.parent_sequence_number(),
            parent_allocated_local_surface_id.parent_sequence_number());
  EXPECT_NE(preupdate_local_surface_id.embed_token(),
            parent_allocated_local_surface_id.embed_token());

  bool changed = allocator().UpdateFromParent(
      parent_allocator1().GetCurrentLocalSurfaceId());
  EXPECT_TRUE(changed);

  const LocalSurfaceId& postupdate_local_surface_id =
      allocator().GetCurrentLocalSurfaceId();
  EXPECT_EQ(postupdate_local_surface_id.parent_sequence_number(),
            parent_allocated_local_surface_id.parent_sequence_number());
  EXPECT_EQ(postupdate_local_surface_id.embed_token(),
            parent_allocated_local_surface_id.embed_token());
}

// UpdateFromParent() on a child allocator should accept the parent's
// LocalSurfaceId if only the embed_token changed.
TEST_F(ChildLocalSurfaceIdAllocatorTest, UpdateFromParentEmbedTokenChanged) {
  parent_allocator1().GenerateId();
  EXPECT_TRUE(parent_allocator1().GetCurrentLocalSurfaceId().is_valid());
  EXPECT_TRUE(allocator().UpdateFromParent(
      parent_allocator1().GetCurrentLocalSurfaceId()));
  parent_allocator2().GenerateId();
  EXPECT_LE(
      parent_allocator2().GetCurrentLocalSurfaceId().parent_sequence_number(),
      parent_allocator1().GetCurrentLocalSurfaceId().parent_sequence_number());
  EXPECT_NE(parent_allocator2().GetCurrentLocalSurfaceId().embed_token(),
            parent_allocator1().GetCurrentLocalSurfaceId().embed_token());

  EXPECT_TRUE(allocator().UpdateFromParent(
      parent_allocator2().GetCurrentLocalSurfaceId()));
}

// GenerateId() on a child allocator should monotonically increment the child
// sequence number.
TEST_F(ChildLocalSurfaceIdAllocatorTest,
       GenerateIdOnlyUpdatesExpectedLocalSurfaceIdComponents) {
  parent_allocator1().GenerateId();
  allocator().UpdateFromParent(parent_allocator1().GetCurrentLocalSurfaceId());
  LocalSurfaceId pregenerateid_local_surface_id =
      allocator().GetCurrentLocalSurfaceId();

  allocator().GenerateId();
  const LocalSurfaceId& returned_local_surface_id =
      allocator().GetCurrentLocalSurfaceId();

  const LocalSurfaceId& postgenerateid_local_surface_id =
      allocator().GetCurrentLocalSurfaceId();
  EXPECT_EQ(pregenerateid_local_surface_id.parent_sequence_number(),
            postgenerateid_local_surface_id.parent_sequence_number());
  EXPECT_EQ(pregenerateid_local_surface_id.child_sequence_number() + 1,
            postgenerateid_local_surface_id.child_sequence_number());
  EXPECT_EQ(pregenerateid_local_surface_id.embed_token(),
            postgenerateid_local_surface_id.embed_token());
  EXPECT_EQ(returned_local_surface_id, allocator().GetCurrentLocalSurfaceId());
}

TEST_F(ChildLocalSurfaceIdAllocatorTest, EmbedTokenChangeResetsChildId) {
  parent_allocator1().GenerateId();
  EXPECT_TRUE(parent_allocator1().GetCurrentLocalSurfaceId().is_valid());
  EXPECT_TRUE(allocator().UpdateFromParent(
      parent_allocator1().GetCurrentLocalSurfaceId()));
  allocator().GenerateId();

  EXPECT_NE(1u, allocator().GetCurrentLocalSurfaceId().child_sequence_number());

  parent_allocator2().GenerateId();

  EXPECT_TRUE(allocator().UpdateFromParent(
      parent_allocator2().GetCurrentLocalSurfaceId()));

  EXPECT_EQ(1u, allocator().GetCurrentLocalSurfaceId().child_sequence_number());
}

namespace {

::testing::AssertionResult ParentSequenceNumberIsNotSet(
    const LocalSurfaceId& local_surface_id) {
  if (local_surface_id.parent_sequence_number() == kInvalidParentSequenceNumber)
    return ::testing::AssertionSuccess();

  return ::testing::AssertionFailure() << "parent_sequence_number() is set.";
}

::testing::AssertionResult ChildSequenceNumberIsSet(
    const LocalSurfaceId& local_surface_id) {
  if (local_surface_id.child_sequence_number() != kInvalidChildSequenceNumber)
    return ::testing::AssertionSuccess();

  return ::testing::AssertionFailure() << "child_sequence_number() is not set.";
}

::testing::AssertionResult EmbedTokenIsValid(
    const LocalSurfaceId& local_surface_id) {
  if (!local_surface_id.embed_token().is_empty())
    return ::testing::AssertionSuccess();

  return ::testing::AssertionFailure() << "embed_token() is not valid.";
}

}  // namespace
}  // namespace viz
